#pragma once

#include <stdint.h>

// Sec. 13 - threshold above which an epoch is considered plausible, i.e.
// genuinely obtained from NTP and not the 1/1/1970 (or nearby) of a system
// clock that's never been synced. 1700000000 = 2023-11-14, comfortably in
// the past relative to "now" for any realistic use of this firmware, but
// well past the initial unsynced epoch.
constexpr uint32_t kPlausibleEpochThreshold = 1700000000UL;

bool isEpochPlausible(uint32_t epoch);
