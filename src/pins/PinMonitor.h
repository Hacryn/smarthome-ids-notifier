#pragma once

#include <stddef.h>
#include <stdint.h>

// Sec. 3.3 - a raw transition captured by the ISR (before debounce).
struct PinTransition {
  uint8_t eventTypeIndex;  // index in EVENT_TYPES, not the GPIO pin number
  uint8_t level;           // 1 = HIGH, 0 = LOW
  uint32_t millisAtIsr;
};

// Registers an IRAM_ATTR attachInterrupt(CHANGE) for every EVENT_TYPES
// entry with pin >= 0 and enabled == true. Call once in setup().
void initPinMonitor();

// Drains the ISR queue in a single pass (depth 32, sec. 3.3 point 2),
// invoking onTransition for each element. Returns the number of elements.
size_t drainPinTransitions(void (*onTransition)(const PinTransition&));

// Contatore di overflow della coda ISR, esposto in /status (sez. 12.2).
uint32_t pinQueueOverflowCount();
