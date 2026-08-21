#pragma once

#include <stdint.h>

// Sez. 3.3 punto 4 - datazione retroattiva di una transizione rilevata
// mentre il loop era bloccato in I/O di rete:
// ts_evento = epoch_corrente - (millis_ora - millis_ISR) / 1000
uint32_t computeRetroactiveTimestamp(uint32_t epochNow, uint32_t millisNow, uint32_t millisAtIsr);
