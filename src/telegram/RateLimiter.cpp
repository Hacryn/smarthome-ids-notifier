#include "RateLimiter.h"

bool RateLimiter::tryConsume(uint32_t nowMillis) {
  if (hasSentBefore_ && (nowMillis - lastSendMillis_) < kMinSendIntervalMs) {
    return false;
  }
  hasSentBefore_ = true;
  lastSendMillis_ = nowMillis;
  return true;
}
