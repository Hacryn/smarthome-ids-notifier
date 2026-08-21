#include "Clock.h"

#include <Arduino.h>
#include <time.h>

#include "ClockPolicy.h"
#include "TimeAnchor.h"
#include "TimeAnchorStorage.h"

namespace {
uint32_t g_anchorEpoch = 0;
bool g_synced = false;
uint32_t g_lastAnchorPersistMillis = 0;
}  // namespace

void initClock() { g_anchorEpoch = loadLastEpoch(); }

void beginNtpSync() {
  // Sez. 10.2/13 - offset zero: l'orologio di sistema resta sempre in UTC,
  // coerentemente con sez. 5.3 (i timestamp si convertono in ora locale
  // solo in visualizzazione, mai qui).
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

void tickClock(uint32_t nowMillis) {
  if (!g_synced) {
    uint32_t nowRaw = static_cast<uint32_t>(time(nullptr));
    if (!isEpochPlausible(nowRaw)) return;

    g_synced = true;
    g_anchorEpoch = nowRaw;
    saveLastEpoch(g_anchorEpoch);  // sez. 5.4.1 - subito dopo il sync riuscito
    g_lastAnchorPersistMillis = nowMillis;
    return;
  }

  if (shouldPersistAnchor(nowMillis - g_lastAnchorPersistMillis)) {
    g_anchorEpoch = currentEpoch();
    saveLastEpoch(g_anchorEpoch);  // sez. 5.4.1 - ogni 10 minuti mentre l'orario e' valido
    g_lastAnchorPersistMillis = nowMillis;
  }
}

bool isTimeSynced() { return g_synced; }

uint32_t currentEpoch() {
  if (g_synced) {
    uint32_t nowRaw = static_cast<uint32_t>(time(nullptr));
    if (isEpochPlausible(nowRaw)) return nowRaw;
    g_synced = false;  // l'orario di sistema e' tornato implausibile (difensivo, raro)
  }
  return estimateTimestamp(g_anchorEpoch, millis());
}
