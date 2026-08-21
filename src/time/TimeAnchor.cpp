#include "TimeAnchor.h"

uint32_t estimateTimestamp(uint32_t lastEpoch, uint32_t millisSinceBoot) {
  return lastEpoch + millisSinceBoot / 1000;
}

ClampedTimestamp applyMonotonicClamp(uint32_t candidateTs, uint32_t lastWrittenTs) {
  if (candidateTs < lastWrittenTs) {
    return {lastWrittenTs, true};
  }
  return {candidateTs, false};
}

bool shouldPersistAnchor(uint32_t msSinceLastPersist) {
  return msSinceLastPersist >= ANCHOR_PERSIST_INTERVAL_MS;
}
