#pragma once

#include <stdint.h>

// Sez. 9.1 - periodo di validita' di default; il valore effettivo vive in
// GlobalConfig (sez. 11.1, comando /setretention), questo resta solo il
// default con cui GlobalConfig viene inizializzato.
constexpr uint32_t kDefaultRetentionWeeks = 52;
constexpr uint32_t kSecondsPerWeek = 7UL * 24UL * 3600UL;

// Sez. 9.1 - righe con ts di riferimento precedente al cutoff sono eliminabili.
uint32_t retentionCutoff(uint32_t nowEpoch, uint32_t retentionWeeks);

// Sez. 9.2 - cadenza settimanale, confronto leggero senza scansione file.
constexpr uint32_t kRotationIntervalSec = kSecondsPerWeek;
bool isRotationDue(uint32_t lastRotationEpoch, uint32_t nowEpoch);

// Sez. 9.4 - soglie di occupazione del filesystem.
enum class SpaceStatus {
  NORMAL,       // < 80%
  ROTATE_EARLY,  // >= 80%: rotazione anticipata + segnalazione admin
  DEGRADED,      // >= 95%: scritture non essenziali sospese
};
SpaceStatus evaluateSpaceUsage(uint64_t usedBytes, uint64_t totalBytes);
