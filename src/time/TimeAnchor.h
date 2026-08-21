#pragma once

#include <stdint.h>

// Sec. 5.4.1 - the anchor must be persisted every 10 minutes, as well as
// right after every successful NTP sync (the latter is decided by the caller).
constexpr uint32_t ANCHOR_PERSIST_INTERVAL_MS = 10UL * 60UL * 1000UL;

// Sec. 5.4.1 - reconstruction of the working time before NTP synchronization:
// estimated_ts = last_epoch + millis() / 1000
uint32_t estimateTimestamp(uint32_t lastEpoch, uint32_t millisSinceBoot);

struct ClampedTimestamp {
  uint32_t ts;
  bool wasClamped;
};

// Sec. 5.4.3 - guarantees monotonicity of the append-only log:
// written_ts = max(calculated_ts, last_written_ts)
// wasClamped indicates whether the candidate was corrected (must be marked "a":1 if so).
ClampedTimestamp applyMonotonicClamp(uint32_t candidateTs, uint32_t lastWrittenTs);

// Sec. 5.4.1 - true if at least ANCHOR_PERSIST_INTERVAL_MS has elapsed
// since the anchor was last persisted.
bool shouldPersistAnchor(uint32_t msSinceLastPersist);
