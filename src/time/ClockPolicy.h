#pragma once

#include <stdint.h>

// Sez. 13 - soglia oltre la quale un epoch e' considerato plausibile, cioe'
// realmente ottenuto da NTP e non l'1/1/1970 (o vicino) di un orologio di
// sistema mai sincronizzato. 1700000000 = 2023-11-14, comodamente nel
// passato rispetto a "adesso" per qualunque uso realistico di questo
// firmware, ma ben oltre l'epoch iniziale non sincronizzato.
constexpr uint32_t kPlausibleEpochThreshold = 1700000000UL;

bool isEpochPlausible(uint32_t epoch);
