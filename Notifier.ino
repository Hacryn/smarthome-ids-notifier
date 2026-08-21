#include <LittleFS.h>

#include "secrets.h"
#include "src/config/GlobalConfigStorage.h"
#include "src/events/EventId.h"
#include "src/events/EventLogStorage.h"
#include "src/events/EventTiming.h"
#include "src/events/EventTypes.h"
#include "src/events/OpenEventsManager.h"
#include "src/network/NetworkIssueTracker.h"
#include "src/network/WifiManager.h"
#include "src/notifications/NotificationEngine.h"
#include "src/pins/PinDebounce.h"
#include "src/pins/PinMonitor.h"
#include "src/rotation/FsErrorCounter.h"
#include "src/rotation/RotationEngine.h"
#include "src/telegram/CallbackData.h"
#include "src/telegram/CommandRouter.h"
#include "src/telegram/TelegramClient.h"
#include "src/time/TimeAnchor.h"
#include "src/time/TimeAnchorStorage.h"
#include "src/users/UserList.h"
#include "src/users/UserStorage.h"

// Sez. 9 - manutenzione periodica (spazio + rotazione dovuta), non ad ogni
// ciclo di loop.
constexpr uint32_t kMaintenanceIntervalMs = 10UL * 60UL * 1000UL;

// Sez. 3.3 - un debouncer per voce di EVENT_TYPES (le voci senza pin restano inutilizzate).
PinDebouncer g_debouncers[EVENT_TYPES_COUNT];

// Sez. 5.4 - stato temporale in RAM.
uint32_t g_lastEpochAnchor = 0;
uint32_t g_lastWrittenTs = 0;

NetworkIssueTracker g_networkIssueTracker;

// Sez. 4 - whitelist utenti autorizzati.
std::vector<AuthorizedUser> g_users;

// Sez. 6.2/8 - scansione di recupero e riepilogo eventi aperti, una tantum
// alla prima connessione WiFi (boot).
bool g_bootTasksDone = false;

// Sez. 9.4 - true se LittleFS.begin() e' stato riformattato al boot perche'
// non montabile: da segnalare agli admin appena la connettivita' e' pronta.
bool g_needsFsFormatAlert = false;

uint32_t g_lastMaintenanceMillis = 0;

void handleRawTransition(const PinTransition& t) {
  g_debouncers[t.eventTypeIndex].onTransition(t.level, t.millisAtIsr);
}

// Sez. 9.4 - il primo errore di scrittura filesystem va sempre segnalato.
void alertOnFirstFsError() {
  if (fsErrorCounter().count() == 1) {
    notifyAdmins(g_users, "Primo errore di scrittura filesystem rilevato.");
  }
}

// Sez. 8.1 punto 1 - l'autorizzazione va rivalutata al click, mai data per
// acquisita dal fatto che il bottone fosse visibile.
void onTelegramCallback(const IncomingCallback& cb) {
  if (!isAdmin(g_users, cb.fromChatId)) return;

  std::string id;
  if (!parseCloseEventCallbackData(cb.data, id)) return;

  bool closed = closeOpenEvent(g_users, id, estimateTimestamp(g_lastEpochAnchor, millis()),
                                g_lastWrittenTs);
  answerCallback(cb.queryId, closed ? "Evento chiuso" : "Evento gia' chiuso o non trovato");
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
  } else {
    alertOnFirstFsError();
  }
}

void setup() {
  Serial.begin(115200);

  // Sez. 9.4 - un solo tentativo di format, esclusivamente se il filesystem
  // risulta non montabile (mai come reazione a un errore su un FS montato
  // correttamente).
  if (!LittleFS.begin(false)) {
    Serial.println("LittleFS non montabile, tento un format...");
    if (LittleFS.begin(true)) {
      g_needsFsFormatAlert = true;
    } else {
      Serial.println("LittleFS irrecuperabile.");
    }
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

  // Sez. 9.3.2 - pulizia dei file temporanei residui di una rotazione
  // interrotta da un blackout.
  cleanupStaleRotationFiles(g_users);

  // Sez. 11.1 - configurazioni globali (default se non ancora presenti in NVS).
  initGlobalConfigStore();

  initPinMonitor();
  initWifi(WIFI_SSID, WIFI_PASSWORD);
  initTelegramClient(TELEGRAM_BOT_TOKEN);
  initCommandRouter(g_users, g_lastEpochAnchor, g_lastWrittenTs);
  setTelegramUpdateHandlers(onTelegramCallback, handleIncomingCommand);
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
    } else {
      alertOnFirstFsError();
    }
  }

  tickWifi(nowMillis);

  // TODO (fase 6): la definizione di sez. 3.4.1 richiede anche la
  // raggiungibilita' delle API Telegram, non solo lo stato WiFi. Per ora
  // NETWORK_ISSUE si basa unicamente sullo stato della connessione WiFi.
  bool reachable = isWifiConnected();
  NetworkIssueEvent netEv = g_networkIssueTracker.update(
      reachable, nowMillis, epochNow, globalConfig().networkIssueThresholdSec);
  if (netEv.kind == NetworkIssueEvent::Kind::STARTED) {
    logNetworkIssueEvent(EventStatus::START, netEv.ts);
  } else if (netEv.kind == NetworkIssueEvent::Kind::ENDED) {
    logNetworkIssueEvent(EventStatus::END, netEv.ts);
    // Sez. 6.2 - scansione di recupero al ripristino della connettivita'.
    runRecoveryScan(g_users, nowMillis, epochNow);
  }

  if (!g_bootTasksDone && reachable) {
    // Sez. 6.2 - scansione di recupero al boot, una tantum.
    runRecoveryScan(g_users, nowMillis, epochNow);
    // Sez. 8 - riepilogo degli eventi rimasti aperti da un riavvio precedente.
    sendOpenEventsSummary(g_users);
    if (g_needsFsFormatAlert) {
      notifyAdmins(g_users, "LittleFS riformattato al boot (non montabile): storico perso.");
      g_needsFsFormatAlert = false;
    }
    // Sez. 9 - verifica dello spazio ed eventuale rotazione, anche al boot.
    performMaintenanceIfDue(g_users, epochNow, globalConfig().retentionWeeks);
    g_bootTasksDone = true;
  }

  if (nowMillis - g_lastMaintenanceMillis >= kMaintenanceIntervalMs) {
    // Sez. 9.2/9.4 - verifica periodica di spazio e cadenza di rotazione.
    performMaintenanceIfDue(g_users, epochNow, globalConfig().retentionWeeks);
    g_lastMaintenanceMillis = nowMillis;
  }

  tickNotificationEngine(g_users, nowMillis, epochNow);
  tickTelegramUpdates();
}
