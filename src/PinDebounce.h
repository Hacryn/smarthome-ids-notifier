#pragma once

#include <stdint.h>

#include "EventTypes.h"

// Sez. 2.2 - soglia di anti-rimbalzo consigliata.
constexpr uint32_t kPinDebounceMs = 300;

struct DebouncedTransition {
  uint8_t level;  // 1 = HIGH, 0 = LOW
  uint32_t millisAtIsr;
};

// Sez. 3.3 punto 3 - debounce sui millis() catturati dalla ISR, non
// sull'istante di elaborazione: una transizione e' confermata solo se
// non seguita da un'altra transizione sullo stesso pin entro la soglia.
class PinDebouncer {
 public:
  // Chiamata per ogni transizione svuotata dalla coda ISR (sez. 3.3 punto 1).
  void onTransition(uint8_t level, uint32_t millisAtIsr);

  // Chiamata periodicamente dal loop con l'istante corrente. Ritorna true e
  // valorizza 'out' se una transizione confermata e' disponibile.
  bool poll(uint32_t nowMillis, uint32_t debounceMs, DebouncedTransition& out);

 private:
  bool pending_ = false;
  uint8_t level_ = 0;
  uint32_t millisAtIsr_ = 0;
};

// Sez. 3.2.1 - interpreta il livello del pin secondo la polarita' configurata.
EventStatus resolvePinEventStatus(uint8_t level, bool activeLow);
