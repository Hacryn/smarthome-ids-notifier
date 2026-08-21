#include "IdBinary.h"

#include <string.h>

namespace {
bool hexNibble(char c, uint8_t& out) {
  if (c >= '0' && c <= '9') {
    out = static_cast<uint8_t>(c - '0');
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    out = static_cast<uint8_t>(c - 'a' + 10);
    return true;
  }
  if (c >= 'A' && c <= 'F') {
    out = static_cast<uint8_t>(c - 'A' + 10);
    return true;
  }
  return false;
}
}  // namespace

bool hexIdToBinary(const char* hexId, std::array<uint8_t, 16>& out) {
  if (hexId == nullptr || strlen(hexId) != 32) return false;

  for (int i = 0; i < 16; i++) {
    uint8_t hi, lo;
    if (!hexNibble(hexId[i * 2], hi) || !hexNibble(hexId[i * 2 + 1], lo)) return false;
    out[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return true;
}
