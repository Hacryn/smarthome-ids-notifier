#pragma once

#include <stdint.h>

#include "StatusLedPolicy.h"

// Hardware-bound (drives the Nano ESP32's discrete LED_RED/LED_GREEN/
// LED_BLUE pins - this board has no addressable RGB LED, so no
// neopixelWrite()/library dependency is needed). Not testable via the
// host-side harness; the state-selection priority is tested separately
// in StatusLedPolicy.

// Sets pin modes and turns the LED off. Call once in setup().
void initStatusLed();

// Call on every loop cycle - non-blocking (blink timing derived from
// nowMillis, never delay()).
void tickStatusLed(uint32_t nowMillis, StatusLedState state);
