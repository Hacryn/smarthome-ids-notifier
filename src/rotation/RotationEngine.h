#pragma once

#include <stdint.h>

#include <vector>

#include "../users/UserList.h"
#include "RotationPolicy.h"

// Sec. 9 - rotation orchestration and space monitoring. Not testable via
// the host-side harness (depends on real LittleFS); the threshold logic
// (RotationPolicy) is tested separately.

// Sec. 9.4 - true if the last check found usage >= 95% (non-essential
// writes, e.g. PENDING rows, are suspended elsewhere).
bool isFilesystemDegraded();

// Sec. 9 - periodic entry point: runs rotation if due per cadence (sec.
// 9.2) or if space just crossed a threshold (sec. 9.4, early rotation),
// and sends admin alerts on threshold changes. The caller decides how
// often to invoke it; no need to call it on every loop cycle (no file
// scan to decide whether rotation is due, sec. 9.2).
void performMaintenanceIfDue(const std::vector<AuthorizedUser>& users, uint32_t nowEpoch,
                              uint32_t retentionWeeks = kDefaultRetentionWeeks);

// Sec. 9.3.2 - removes leftover temp files from a rotation interrupted by
// a blackout. Call once in setup().
void cleanupStaleRotationFiles(const std::vector<AuthorizedUser>& users);
