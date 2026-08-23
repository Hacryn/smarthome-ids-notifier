#pragma once

#include <stdint.h>

// Sec. 3.4.3 - signal shape for a remote arm/disarm command: idle LOW,
// pulse HIGH for kAlarmCommandPulseMs, back to LOW.
constexpr uint32_t kAlarmCommandPulseMs = 500;

// Non-blocking pulse state machine, same family as RetryTimer.
class AlarmPulseTimer {
 public:
  void start(uint32_t nowMillis);
  bool isActive() const;
  // True once pulseMs has elapsed since start() - caller releases the pin
  // and calls release() in the same tick.
  bool shouldRelease(uint32_t nowMillis, uint32_t pulseMs) const;
  void release();

 private:
  bool active_ = false;
  uint32_t startMillis_ = 0;
};
