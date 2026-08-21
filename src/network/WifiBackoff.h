#pragma once

#include <stdint.h>

// Sez. 3.4.2 - backoff esponenziale con tetto. attemptNumber e' 1-based
// (il numero del tentativo appena fallito, per cui si calcola l'attesa
// prima del successivo). 0 e' trattato come 1.
uint32_t backoffDelayMs(uint32_t attemptNumber);

// true ogni 10 tentativi consecutivi falliti: va tentato un ciclo completo
// WiFi.disconnect() + WiFi.begin() invece di una semplice reconnect().
bool shouldForceFullReconnect(uint32_t attemptNumber);
