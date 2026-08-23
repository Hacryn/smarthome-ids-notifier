#include "AlarmPulseTimer.h"

void AlarmPulseTimer::start(uint32_t nowMillis) {
  active_ = true;
  startMillis_ = nowMillis;
}

bool AlarmPulseTimer::isActive() const { return active_; }

bool AlarmPulseTimer::shouldRelease(uint32_t nowMillis, uint32_t pulseMs) const {
  if (!active_) return false;
  return static_cast<uint32_t>(nowMillis - startMillis_) >= pulseMs;
}

void AlarmPulseTimer::release() { active_ = false; }
