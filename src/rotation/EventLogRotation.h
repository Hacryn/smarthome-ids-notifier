#pragma once

#include <stdint.h>

// Sez. 9.3 - riscrittura filtrata di log.jsonl (LittleFS non supporta la
// cancellazione selettiva di righe). Non testabile via harness host-side
// (dipende da LittleFS reale); l'algoritmo di raccolta id (DeletableIdCollector)
// e' testato separatamente.

// Ritorna il numero di cicli passata1+passata2+commit eseguiti (0 se non
// c'era nulla da eliminare), o -1 se una scrittura/rinomina e' fallita.
int rotateEventLog(uint32_t cutoff);

// Sez. 9.3.2 - rimuove un eventuale file temporaneo residuo di una
// rotazione interrotta da un blackout. Da chiamare in setup().
void cleanupStaleEventLogRotation();
