#pragma once

// Sez. 5.2 - genera un id evento a 32 caratteri esadecimali (16 byte)
// usando il generatore hardware casuale dell'ESP32. Non testabile via
// harness host-side (dipende da esp_random()).
void generateEventId(char out[33]);
