#pragma once

#include <stdint.h>

// Sec. 9.1 - default validity period; the effective value lives in
// GlobalConfig (sec. 11.1, /setretention command), this stays only the
// default GlobalConfig is initialized with.
constexpr uint32_t kDefaultRetentionWeeks = 52;
constexpr uint32_t kSecondsPerWeek = 7UL * 24UL * 3600UL;

// Sec. 9.1 - rows whose reference ts is earlier than the cutoff are deletable.
uint32_t retentionCutoff(uint32_t nowEpoch, uint32_t retentionWeeks);

// Sec. 9.2 - weekly cadence, lightweight comparison with no file scan.
constexpr uint32_t kRotationIntervalSec = kSecondsPerWeek;
bool isRotationDue(uint32_t lastRotationEpoch, uint32_t nowEpoch);

// Sec. 9.4 - filesystem usage thresholds.
enum class SpaceStatus {
  NORMAL,       // < 80%
  ROTATE_EARLY,  // >= 80%: early rotation + admin alert
  DEGRADED,      // >= 95%: non-essential writes suspended
};
SpaceStatus evaluateSpaceUsage(uint64_t usedBytes, uint64_t totalBytes);
