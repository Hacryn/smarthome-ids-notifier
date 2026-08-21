#include "EventTiming.h"

uint32_t computeRetroactiveTimestamp(uint32_t epochNow, uint32_t millisNow, uint32_t millisAtIsr) {
  uint32_t elapsedMs = millisNow - millisAtIsr;
  return epochNow - elapsedMs / 1000;
}
