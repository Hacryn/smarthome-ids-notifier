#pragma once

#include <stdint.h>

// Sec. 6.6 - minimum interval between two consecutive Telegram sends.
constexpr uint32_t kMinSendIntervalMs = 1100;

// Non-blocking: the caller polls tryConsume() every cycle and sends only
// when it returns true, instead of waiting with a delay().
class RateLimiter {
 public:
  bool tryConsume(uint32_t nowMillis);

 private:
  bool hasSentBefore_ = false;
  uint32_t lastSendMillis_ = 0;
};
