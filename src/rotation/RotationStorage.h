#pragma once

#include <stdint.h>

// Sec. 9.2/9.3.2 - NVS persistence of the last rotation's timestamp.
// Updated only AFTER the atomic rename has succeeded (sec. 9.3.2): a
// blackout in the intermediate window at most causes a rotation already
// done to be repeated, an idempotent operation. Not testable via the
// host-side harness (depends on real NVS).
uint32_t loadLastRotationEpoch();
void saveLastRotationEpoch(uint32_t epoch);
