#pragma once

#include <stdint.h>

// Sez. 5.4.1 - persistenza dell'ancora oraria in NVS (chiave "last_epoch").
// Wrapper sottile su Preferences: nessuna logica propria, non testabile
// via harness host-side (dipende dall'hardware NVS dell'ESP32).
uint32_t loadLastEpoch();
void saveLastEpoch(uint32_t epoch);
