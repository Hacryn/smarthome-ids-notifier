#pragma once

#include <stdint.h>

// Sez. 6.6 - intervallo minimo tra due invii Telegram consecutivi.
constexpr uint32_t kMinSendIntervalMs = 1100;

// Non bloccante: il chiamante interroga tryConsume() ad ogni ciclo e invia
// solo quando ritorna true, invece di attendere con un delay().
class RateLimiter {
 public:
  bool tryConsume(uint32_t nowMillis);

 private:
  bool hasSentBefore_ = false;
  uint32_t lastSendMillis_ = 0;
};
