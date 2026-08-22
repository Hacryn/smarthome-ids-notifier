#include "TelegramReachability.h"

void TelegramReachabilityTracker::recordOutcome(SendOutcomeCategory category) {
  if (category == SendOutcomeCategory::TRANSIENT_NETWORK) {
    consecutiveFailures_++;
  } else {
    consecutiveFailures_ = 0;
  }
}

bool TelegramReachabilityTracker::reachable() const {
  return consecutiveFailures_ < kConsecutiveFailureThreshold;
}
