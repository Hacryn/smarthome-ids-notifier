#include "RetryTimer.h"

void RetryTimer::onTransientFailure(uint32_t nowMillis, uint32_t intervalMs) {
  active_ = true;
  dueAtMillis_ = nowMillis + intervalMs;
}

bool RetryTimer::onNormalFlowSuccess() {
  if (scanInProgress_) return false;  // sez. 6.3.1
  return active_;
}

bool RetryTimer::isDue(uint32_t nowMillis) const {
  if (!active_ || scanInProgress_) return false;
  return static_cast<int32_t>(nowMillis - dueAtMillis_) >= 0;
}

void RetryTimer::beginScan() { scanInProgress_ = true; }

void RetryTimer::endScan(bool allResolvedOrAbandoned, uint32_t nowMillis, uint32_t intervalMs) {
  scanInProgress_ = false;
  if (allResolvedOrAbandoned) {
    active_ = false;
  } else {
    active_ = true;
    dueAtMillis_ = nowMillis + intervalMs;
  }
}
