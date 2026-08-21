#pragma once

// Sec. 5.2 - generates a 32-hex-character (16-byte) event id using the
// ESP32's hardware random generator. Not testable via the host-side
// harness (depends on esp_random()).
void generateEventId(char out[33]);
