#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr uint32_t kGracePeriodSec = 60;     // sec. 6.4 - default 1 minute
constexpr uint32_t kAggregateThreshold = 3;  // sec. 6.7 - default
constexpr uint32_t kMaxRetries = 24;         // sec. 6.5 - default

struct RecoveryPresentation {
  bool isRecovered;  // explicit recovery prefix
  bool isApprox;     // "~" marking on the timestamp
};

// Sec. 6.4 - decides whether a pending notification should be presented as
// "recovered". An event with an approximate timestamp (sec. 5.4) is always
// treated as recovered/approximate, regardless of the computed time gap.
RecoveryPresentation decideRecoveryPresentation(uint32_t nowEpoch, uint32_t eventTs,
                                                 bool eventApprox, uint32_t gracePeriodSec);

// Sec. 6.7 - above the threshold, a user's pending notifications should be
// grouped into a single message instead of sent individually.
bool shouldAggregate(size_t pendingCount, uint32_t threshold);

// Sec. 6.5 - true if the attempt count, after incrementing for the current
// failure, exceeds the limit: the notification should be abandoned.
bool exceedsMaxRetries(uint32_t attemptCountAfterFailure, uint32_t maxRetries);

// Sec. 7.2/8 - "a PENDING record approaching max_retries is flagged in the
// periodic summary together with open events": the design doesn't specify
// a numeric threshold, so we fix one explicitly here (the last 3 available
// attempts) as a documented choice, not an implicit one.
constexpr uint32_t kNearAbandonmentMargin = 3;
bool isNearAbandonment(uint32_t n, uint32_t maxRetries);
