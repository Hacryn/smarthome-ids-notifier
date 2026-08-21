#include "EventId.h"

#include <esp_system.h>
#include <stdint.h>
#include <stdio.h>

void generateEventId(char out[33]) {
  uint8_t bytes[16];
  for (int word = 0; word < 4; word++) {
    uint32_t r = esp_random();
    bytes[word * 4 + 0] = (r >> 24) & 0xFF;
    bytes[word * 4 + 1] = (r >> 16) & 0xFF;
    bytes[word * 4 + 2] = (r >> 8) & 0xFF;
    bytes[word * 4 + 3] = r & 0xFF;
  }
  for (int i = 0; i < 16; i++) {
    snprintf(out + i * 2, 3, "%02x", bytes[i]);
  }
}
