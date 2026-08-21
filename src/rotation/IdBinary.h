#pragma once

#include <stdint.h>

#include <array>

// Sez. 9.3.1 - conversione dell'id esadecimale (32 caratteri, sez. 5.2) in
// 16 byte binari, per contenere l'occupazione RAM della raccolta di id
// eliminabili durante la rotazione. Ritorna false se hexId non e' lungo
// esattamente 32 caratteri o contiene caratteri non esadecimali.
bool hexIdToBinary(const char* hexId, std::array<uint8_t, 16>& out);
