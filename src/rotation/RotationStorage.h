#pragma once

#include <stdint.h>

// Sez. 9.2/9.3.2 - persistenza NVS del timestamp dell'ultima rotazione
// eseguita. Aggiornato solo DOPO che il rename atomico e' andato a buon
// fine (sez. 9.3.2): un blackout nella finestra intermedia provoca al piu'
// la ripetizione di una rotazione gia' fatta, operazione idempotente.
// Non testabile via harness host-side (dipende da NVS reale).
uint32_t loadLastRotationEpoch();
void saveLastRotationEpoch(uint32_t epoch);
