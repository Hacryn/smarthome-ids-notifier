#include "NotificationPresentation.h"

RecoveryPresentation decideRecoveryPresentation(uint32_t nowEpoch, uint32_t eventTs,
                                                 bool eventApprox, uint32_t gracePeriodSec) {
  if (eventApprox) return {true, true};

  bool withinGrace = (nowEpoch - eventTs) <= gracePeriodSec;
  return {!withinGrace, false};
}

bool shouldAggregate(size_t pendingCount, uint32_t threshold) { return pendingCount > threshold; }

bool exceedsMaxRetries(uint32_t attemptCountAfterFailure, uint32_t maxRetries) {
  return attemptCountAfterFailure > maxRetries;
}

bool isNearAbandonment(uint32_t n, uint32_t maxRetries) {
  if (maxRetries < kNearAbandonmentMargin) return n >= maxRetries;
  return n >= (maxRetries - kNearAbandonmentMargin);
}
