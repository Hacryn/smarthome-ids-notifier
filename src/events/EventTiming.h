#pragma once

#include <stdint.h>

// Sec. 3.3 point 4 - retroactive dating of a transition detected while the
// loop was blocked in network I/O:
// event_ts = current_epoch - (millis_now - millis_ISR) / 1000
uint32_t computeRetroactiveTimestamp(uint32_t epochNow, uint32_t millisNow, uint32_t millisAtIsr);
