#pragma once

#include <stddef.h>
#include <stdint.h>

// Sez. 3.3 - una transizione grezza catturata dalla ISR (prima del debounce).
struct PinTransition {
  uint8_t eventTypeIndex;  // indice in EVENT_TYPES, non il numero di pin GPIO
  uint8_t level;           // 1 = HIGH, 0 = LOW
  uint32_t millisAtIsr;
};

// Registra un attachInterrupt(CHANGE) IRAM_ATTR per ogni voce di EVENT_TYPES
// con pin >= 0 ed enabled == true. Da chiamare una sola volta in setup().
void initPinMonitor();

// Svuota la coda ISR in una singola passata (profondita' 32, sez. 3.3 punto 2),
// invocando onTransition per ciascun elemento. Ritorna il numero di elementi.
size_t drainPinTransitions(void (*onTransition)(const PinTransition&));

// Contatore di overflow della coda ISR, esposto in /status in una fase successiva.
uint32_t pinQueueOverflowCount();
