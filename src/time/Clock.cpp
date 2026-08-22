#include "Clock.h"

#include <Arduino.h>
#include <time.h>

#include "../config/GlobalConfigStorage.h"
#include "../diagnostics/SerialLog.h"
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
    logInfo("NTP synced: epoch=%lu", static_cast<unsigned long>(g_anchorEpoch));
    saveLastEpoch(g_anchorEpoch);  // sec. 5.4.1 - right after a successful sync
    g_lastAnchorPersistMillis = nowMillis;
    return;
  }

  uint32_t intervalMs = globalConfig().anchorPersistIntervalMinutes * 60000UL;
  if (shouldPersistAnchor(nowMillis - g_lastAnchorPersistMillis, intervalMs)) {
    g_anchorEpoch = currentEpoch();
    saveLastEpoch(g_anchorEpoch);  // sec. 5.4.1 - while time stays valid, per the configured interval
    g_lastAnchorPersistMillis = nowMillis;
  }
}

bool isTimeSynced() { return g_synced; }

uint32_t currentEpoch() {
  if (g_synced) {
    uint32_t nowRaw = static_cast<uint32_t>(time(nullptr));
    if (isEpochPlausible(nowRaw)) return nowRaw;
    g_synced = false;  // system time became implausible again (defensive, rare)
    logWarn("NTP time became implausible again, falling back to NVS anchor");
  }
  return estimateTimestamp(g_anchorEpoch, millis());
}
