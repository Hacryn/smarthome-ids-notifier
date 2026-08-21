#pragma once

#include <stdint.h>

#include <array>

// Sec. 9.3.1 - conversion of the hex id (32 characters, sec. 5.2) into 16
// binary bytes, to keep the RAM footprint of the deletable-id collection
// small during rotation. Returns false if hexId isn't exactly 32
// characters long or contains non-hex characters.
bool hexIdToBinary(const char* hexId, std::array<uint8_t, 16>& out);
