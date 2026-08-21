#pragma once

#include <stdint.h>

#include "../events/EventTypes.h"

// Sec. 2.2 - recommended debounce threshold.
constexpr uint32_t kPinDebounceMs = 300;

struct DebouncedTransition {
  uint8_t level;  // 1 = HIGH, 0 = LOW
  uint32_t millisAtIsr;
};

// Sec. 3.3 point 3 - debounce against the millis() captured by the ISR,
// not against the processing instant: a transition is confirmed only if
// it isn't followed by another transition on the same pin within the threshold.
class PinDebouncer {
 public:
  // Called for every transition drained from the ISR queue (sec. 3.3 point 1).
  void onTransition(uint8_t level, uint32_t millisAtIsr);

  // Called periodically by the loop with the current instant. Returns true
  // and fills 'out' if a confirmed transition is available.
  bool poll(uint32_t nowMillis, uint32_t debounceMs, DebouncedTransition& out);

 private:
  bool pending_ = false;
  uint8_t level_ = 0;
  uint32_t millisAtIsr_ = 0;
};

// Sec. 3.2.1 - interprets the pin level per the configured polarity.
EventStatus resolvePinEventStatus(uint8_t level, bool activeLow);
