#pragma once

#include <stdint.h>

// Sec. 5.4.1 - NVS persistence of the time anchor ("last_epoch" key). Thin
// wrapper over Preferences: no logic of its own, not testable via the
// host-side harness (depends on the ESP32's NVS hardware).
uint32_t loadLastEpoch();
void saveLastEpoch(uint32_t epoch);
