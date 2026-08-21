#include "PinDebounce.h"

void PinDebouncer::onTransition(uint8_t level, uint32_t millisAtIsr) {
  pending_ = true;
  level_ = level;
  millisAtIsr_ = millisAtIsr;
}

bool PinDebouncer::poll(uint32_t nowMillis, uint32_t debounceMs, DebouncedTransition& out) {
  if (!pending_) return false;
  if (nowMillis - millisAtIsr_ < debounceMs) return false;

  out.level = level_;
  out.millisAtIsr = millisAtIsr_;
  pending_ = false;
  return true;
}

EventStatus resolvePinEventStatus(uint8_t level, bool activeLow) {
  bool isActive = activeLow ? (level == 0) : (level == 1);
  return isActive ? EventStatus::START : EventStatus::END;
}
