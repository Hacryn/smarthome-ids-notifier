#include "RotationPolicy.h"

uint32_t retentionCutoff(uint32_t nowEpoch, uint32_t retentionWeeks) {
  return nowEpoch - retentionWeeks * kSecondsPerWeek;
}

bool isRotationDue(uint32_t lastRotationEpoch, uint32_t nowEpoch) {
  return (nowEpoch - lastRotationEpoch) >= kRotationIntervalSec;
}

SpaceStatus evaluateSpaceUsage(uint64_t usedBytes, uint64_t totalBytes) {
  if (totalBytes == 0) return SpaceStatus::NORMAL;

  uint64_t usedPercent = (usedBytes * 100) / totalBytes;
  if (usedPercent >= 95) return SpaceStatus::DEGRADED;
  if (usedPercent >= 80) return SpaceStatus::ROTATE_EARLY;
  return SpaceStatus::NORMAL;
}
