#include <LittleFS.h>

#include "secrets.h"
#include "src/events/EventId.h"
#include "src/events/EventLogStorage.h"
#include "src/events/EventTiming.h"
#include "src/events/EventTypes.h"
#include "src/network/NetworkIssueTracker.h"
#include "src/network/WifiManager.h"
#include "src/notifications/NotificationEngine.h"
#include "src/pins/PinDebounce.h"
#include "src/pins/PinMonitor.h"
#include "src/telegram/TelegramClient.h"
#include "src/time/TimeAnchor.h"
#include "src/time/TimeAnchorStorage.h"
#include "src/users/UserList.h"
#include "src/users/UserStorage.h"

// Sez. 3.4.1 - soglia di durata minima per generare NETWORK_ISSUE (non ancora
// configurabile da comando: arrivera' con /setnetthreshold in una fase successiva).
constexpr uint32_t kNetworkIssueThresholdSec = 120;

// Sez. 3.3 - un debouncer per voce di EVENT_TYPES (le voci senza pin restano inutilizzate).
PinDebouncer g_debouncers[EVENT_TYPES_COUNT];

// Sez. 5.4 - stato temporale in RAM.
uint32_t g_lastEpochAnchor = 0;
uint32_t g_lastWrittenTs = 0;

NetworkIssueTracker g_networkIssueTracker;

// Sez. 4 - whitelist utenti autorizzati.
std::vector<AuthorizedUser> g_users;

// Sez. 6.2 - scansione di recupero una tantum alla prima connessione WiFi (boot).
bool g_bootRecoveryScanDone = false;

void handleRawTransition(const PinTransition& t) {
  g_debouncers[t.eventTypeIndex].onTransition(t.level, t.millisAtIsr);
}

void logNetworkIssueEvent(EventStatus status, uint32_t rawTs) {
  ClampedTimestamp clamped = applyMonotonicClamp(rawTs, g_lastWrittenTs);

  EventRecord rec{};
  generateEventId(rec.id);
  rec.type = EventType::NETWORK_ISSUE;
  rec.status = status;
  rec.ts = clamped.ts;
  rec.approx = true;  // sempre vero finche' non esiste una fonte NTP (fase successiva)

  if (appendEventRecord(rec)) {
    g_lastWrittenTs = clamped.ts;

    const EventTypeConfig* cfg = findEventTypeConfig(EventType::NETWORK_ISSUE);
    if (cfg && shouldNotifyForStatus(cfg->notify_policy, rec.status)) {
      notifyEvent(g_users, rec.id, rec.type, rec.status, rec.ts, rec.approx);
    }
  }
}

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS.begin() fallito");
  }

  // NOTA: nessuna sincronizzazione NTP in questa fase (arriva in una fase
  // successiva). Fino ad allora l'unica fonte di tempo e' l'ancora NVS
  // (sez. 5.4.1); ogni riga scritta e' quindi sempre marcata approssimata.
  g_lastEpochAnchor = loadLastEpoch();
  g_lastWrittenTs = readLastWrittenTimestamp();

  // Sez. 4.5 - onboarding: se users.json e' vuoto/assente, il chat_id di
  // secrets.h diventa automaticamente il primo admin.
  if (!loadUsers(g_users)) {
    Serial.println("Lettura users.json fallita");
  }
  if (ensureOnboardingAdmin(g_users, ONBOARDING_CHAT_ID, estimateTimestamp(g_lastEpochAnchor, millis()))) {
    if (!saveUsers(g_users)) {
      Serial.println("Scrittura users.json fallita");
    }
  }

  initPinMonitor();
  initWifi(WIFI_SSID, WIFI_PASSWORD);
  initTelegramClient(TELEGRAM_BOT_TOKEN);
}

void loop() {
  drainPinTransitions(handleRawTransition);

  uint32_t nowMillis = millis();
  uint32_t epochNow = estimateTimestamp(g_lastEpochAnchor, nowMillis);

  for (size_t i = 0; i < EVENT_TYPES_COUNT; i++) {
    const EventTypeConfig& cfg = EVENT_TYPES[i];
    if (!cfg.enabled || cfg.pin < 0) continue;

    DebouncedTransition transition;
    if (!g_debouncers[i].poll(nowMillis, kPinDebounceMs, transition)) continue;

    EventRecord rec{};
    generateEventId(rec.id);
    rec.type = cfg.type;
    rec.status = resolvePinEventStatus(transition.level, cfg.active_low);

    uint32_t rawTs = computeRetroactiveTimestamp(epochNow, nowMillis, transition.millisAtIsr);
    ClampedTimestamp clamped = applyMonotonicClamp(rawTs, g_lastWrittenTs);
    rec.ts = clamped.ts;
    rec.approx = true;  // sempre vero finche' non esiste una fonte NTP (fase successiva)

    if (appendEventRecord(rec)) {
      g_lastWrittenTs = clamped.ts;

      if (shouldNotifyForStatus(cfg.notify_policy, rec.status)) {
        notifyEvent(g_users, rec.id, rec.type, rec.status, rec.ts, rec.approx);
      }
    }
  }

  tickWifi(nowMillis);

  // TODO (fase 6): la definizione di sez. 3.4.1 richiede anche la
  // raggiungibilita' delle API Telegram, non solo lo stato WiFi. Per ora
  // NETWORK_ISSUE si basa unicamente sullo stato della connessione WiFi.
  bool reachable = isWifiConnected();
  NetworkIssueEvent netEv =
      g_networkIssueTracker.update(reachable, nowMillis, epochNow, kNetworkIssueThresholdSec);
  if (netEv.kind == NetworkIssueEvent::Kind::STARTED) {
    logNetworkIssueEvent(EventStatus::START, netEv.ts);
  } else if (netEv.kind == NetworkIssueEvent::Kind::ENDED) {
    logNetworkIssueEvent(EventStatus::END, netEv.ts);
    // Sez. 6.2 - scansione di recupero al ripristino della connettivita'.
    runRecoveryScan(g_users, nowMillis, epochNow);
  }

  if (!g_bootRecoveryScanDone && reachable) {
    // Sez. 6.2 - scansione di recupero al boot, una tantum.
    runRecoveryScan(g_users, nowMillis, epochNow);
    g_bootRecoveryScanDone = true;
  }

  tickNotificationEngine(g_users, nowMillis, epochNow);
}
