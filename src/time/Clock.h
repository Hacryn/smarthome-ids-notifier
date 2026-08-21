#pragma once

#include <stdint.h>

// Sec. 5.4/10.2/13 - the single system clock for the whole firmware: NTP
// when available, NVS anchor as a fallback before the first sync (or if
// time becomes implausible again). Not testable via the host-side harness
// (depends on real configTime()/time()); the plausibility threshold
// (ClockPolicy) is pure and tested separately.

// Loads the persisted anchor (sec. 5.4.1). Call once in setup(), before
// any use of currentEpoch().
void initClock();

// Sec. 13 - starts/repeats NTP synchronization in UTC (conversion to local
// time happens only at display time, sec. 5.3/10.3, never here). Call on
// every successful WiFi connection, including the first one and after
// every reconnection.
void beginNtpSync();

// Call on every loop cycle: detects the first successful sync and
// persists the NVS anchor (right after the sync, then every 10 minutes
// while time stays valid, sec. 5.4.1).
void tickClock(uint32_t nowMillis);

// Sec. 5.4.2 - true if the current time comes from NTP (not from the
// estimated anchor). Rows written while this is false must always be
// marked approximate.
bool isTimeSynced();

// Sec. 3.3/5.4 - the current epoch, to be used everywhere in the firmware
// instead of manual millis()/anchor calculations.
uint32_t currentEpoch();
