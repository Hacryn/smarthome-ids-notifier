#pragma once

#include <stdint.h>

#include "../telegram/SendOutcomeClassifier.h"

// Sec. 3.4.1 - Telegram API reachability deduced from real send outcomes (no
// dedicated probe). TRANSIENT_NETWORK = the request never reached Telegram;
// any other outcome (even an application error like 429/403) proves the
// link works.
class TelegramReachabilityTracker {
 public:
  void recordOutcome(SendOutcomeCategory category);

  // Defaults to true (no send observed yet - must never trigger
  // NETWORK_ISSUE on its own before a send has been attempted). Becomes
  // false after kConsecutiveFailureThreshold consecutive TRANSIENT_NETWORK
  // outcomes; any other outcome resets the counter.
  bool reachable() const;

 private:
  static constexpr uint32_t kConsecutiveFailureThreshold = 3;  // matches the existing throttling retry cap
  uint32_t consecutiveFailures_ = 0;
};
