#include "WifiBackoff.h"

#include <stddef.h>

uint32_t backoffDelayMs(uint32_t attemptNumber) {
  static const uint32_t kStepsSec[] = {5, 10, 20, 40, 80, 160};
  constexpr uint32_t kCapSec = 300;
  constexpr size_t kStepCount = sizeof(kStepsSec) / sizeof(kStepsSec[0]);

  if (attemptNumber == 0) attemptNumber = 1;
  if (attemptNumber <= kStepCount) return kStepsSec[attemptNumber - 1] * 1000UL;
  return kCapSec * 1000UL;
}

bool shouldForceFullReconnect(uint32_t attemptNumber) {
  return attemptNumber > 0 && attemptNumber % 10 == 0;
}
