#pragma once

#include <stdint.h>

// Sec. 3.4.2 - capped exponential backoff. attemptNumber is 1-based (the
// number of the attempt that just failed, for which the wait before the
// next one is computed). 0 is treated as 1.
uint32_t backoffDelayMs(uint32_t attemptNumber);

// true every 10 consecutive failed attempts: a full WiFi.disconnect() +
// WiFi.begin() cycle should be attempted instead of a plain reconnect().
bool shouldForceFullReconnect(uint32_t attemptNumber);
