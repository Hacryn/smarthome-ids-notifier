#include <LittleFS.h>

#include "src/EventId.h"
#include "src/EventLogStorage.h"
#include "src/EventTiming.h"
#include "src/EventTypes.h"
#include "src/PinDebounce.h"
#include "src/PinMonitor.h"
#include "src/TimeAnchor.h"
#include "src/TimeAnchorStorage.h"

// Sez. 3.3 - un debouncer per voce di EVENT_TYPES (le voci senza pin restano inutilizzate).
PinDebouncer g_debouncers[EVENT_TYPES_COUNT];

// Sez. 5.4 - stato temporale in RAM.
uint32_t g_lastEpochAnchor = 0;
uint32_t g_lastWrittenTs = 0;

void handleRawTransition(const PinTransition& t) {
  g_debouncers[t.eventTypeIndex].onTransition(t.level, t.millisAtIsr);
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
}
