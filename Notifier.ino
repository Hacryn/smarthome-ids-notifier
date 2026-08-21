#include <LittleFS.h>

#include "secrets.h"
#include "src/events/EventId.h"
#include "src/events/EventLogStorage.h"
#include "src/events/EventTiming.h"
#include "src/events/EventTypes.h"
#include "src/network/NetworkIssueTracker.h"
#include "src/network/WifiManager.h"
#include "src/pins/PinDebounce.h"
#include "src/pins/PinMonitor.h"
#include "src/telegram/RateLimiter.h"
#include "src/telegram/TelegramClient.h"
#include "src/time/TimeAnchor.h"
#include "src/time/TimeAnchorStorage.h"

// Sez. 3.4.1 - soglia di durata minima per generare NETWORK_ISSUE (non ancora
// configurabile da comando: arrivera' con /setnetthreshold in una fase successiva).
constexpr uint32_t kNetworkIssueThresholdSec = 120;

// Sez. 3.3 - un debouncer per voce di EVENT_TYPES (le voci senza pin restano inutilizzate).
PinDebouncer g_debouncers[EVENT_TYPES_COUNT];

// Sez. 5.4 - stato temporale in RAM.
uint32_t g_lastEpochAnchor = 0;
uint32_t g_lastWrittenTs = 0;

NetworkIssueTracker g_networkIssueTracker;

// Fase 6 - smoke test: un solo invio alla prima connessione WiFi, per
// verificare manualmente client/classificazione (nessuna coda ancora: arriva
// in fase 8). RateLimiter usato qui a scopo dimostrativo (un solo invio non
// lo esercita davvero).
RateLimiter g_telegramRateLimiter;
bool g_bootMessageSent = false;

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
  }

  if (!g_bootMessageSent && reachable && g_telegramRateLimiter.tryConsume(nowMillis)) {
    SendOutcomeCategory result = sendTelegramMessage(ONBOARDING_CHAT_ID, "Notifier online (test fase 6)");
    Serial.print("Test invio Telegram, categoria esito=");
    Serial.println(static_cast<int>(result));
    g_bootMessageSent = true;
  }
}
