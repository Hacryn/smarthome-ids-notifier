#pragma once

#include <stdint.h>

#include <string>

#include "UserConfig.h"

// Sec. 10.2/10.3 - formats an epoch per the user's timezone (with automatic
// DST handling) and date format. The TZ environment variable is global to
// the process: it's set immediately before the conversion and restored to
// UTC right after (sec. 10.3), since this is the only function in the
// firmware that touches it. With approx true, the result is prefixed with
// "~" (sec. 5.4.2). Not testable via the host-side harness (tzset()/
// strftime() depend on the target's libc).
std::string formatTimestampForUser(uint32_t epoch, const UserConfig& cfg, bool approx);
