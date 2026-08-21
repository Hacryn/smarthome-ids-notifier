#pragma once

#include <stdint.h>

// Sez. 5.4.1 - l'ancora va persistita ogni 10 minuti, oltre che subito dopo
// ogni sincronizzazione NTP riuscita (quest'ultima e' decisa dal chiamante).
constexpr uint32_t ANCHOR_PERSIST_INTERVAL_MS = 10UL * 60UL * 1000UL;

// Sez. 5.4.1 - ricostruzione dell'orario di lavoro prima della sincronizzazione NTP:
// ts_stimato = last_epoch + millis() / 1000
uint32_t estimateTimestamp(uint32_t lastEpoch, uint32_t millisSinceBoot);

struct ClampedTimestamp {
  uint32_t ts;
  bool wasClamped;
};

// Sez. 5.4.3 - garantisce la monotonicita' del log append-only:
// ts_scritto = max(ts_calcolato, last_written_ts)
// wasClamped indica se il candidato e' stato corretto (va marcato "a":1 in tal caso).
ClampedTimestamp applyMonotonicClamp(uint32_t candidateTs, uint32_t lastWrittenTs);

// Sez. 5.4.1 - true se sono trascorsi almeno ANCHOR_PERSIST_INTERVAL_MS
// dall'ultima persistenza dell'ancora.
bool shouldPersistAnchor(uint32_t msSinceLastPersist);
