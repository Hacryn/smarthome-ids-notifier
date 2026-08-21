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
  // Sec. 10.2/13 - zero offset: the system clock always stays in UTC,
  // consistent with sec. 5.3 (timestamps are converted to local time only
  // at display time, never here).
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

void tickClock(uint32_t nowMillis) {
  if (!g_synced) {
    uint32_t nowRaw = static_cast<uint32_t>(time(nullptr));
    if (!isEpochPlausible(nowRaw)) return;

    g_synced = true;
    g_anchorEpoch = nowRaw;
    saveLastEpoch(g_anchorEpoch);  // sec. 5.4.1 - right after a successful sync
    g_lastAnchorPersistMillis = nowMillis;
    return;
  }

  if (shouldPersistAnchor(nowMillis - g_lastAnchorPersistMillis)) {
    g_anchorEpoch = currentEpoch();
    saveLastEpoch(g_anchorEpoch);  // sec. 5.4.1 - every 10 minutes while time stays valid
    g_lastAnchorPersistMillis = nowMillis;
  }
}

bool isTimeSynced() { return g_synced; }

uint32_t currentEpoch() {
  if (g_synced) {
    uint32_t nowRaw = static_cast<uint32_t>(time(nullptr));
    if (isEpochPlausible(nowRaw)) return nowRaw;
    g_synced = false;  // system time became implausible again (defensive, rare)
  }
  return estimateTimestamp(g_anchorEpoch, millis());
}
