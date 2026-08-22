#include <cassert>
#include <cstdio>

#include "../src/network/TelegramReachability.h"

static void test_default_reachable_with_no_sends_observed() {
  TelegramReachabilityTracker tracker;
  assert(tracker.reachable());
}

static void test_becomes_unreachable_after_threshold_consecutive_failures() {
  TelegramReachabilityTracker tracker;
  tracker.recordOutcome(SendOutcomeCategory::TRANSIENT_NETWORK);
  assert(tracker.reachable());
  tracker.recordOutcome(SendOutcomeCategory::TRANSIENT_NETWORK);
  assert(tracker.reachable());
  tracker.recordOutcome(SendOutcomeCategory::TRANSIENT_NETWORK);
  assert(!tracker.reachable());
}

static void test_non_network_outcome_resets_the_counter() {
  TelegramReachabilityTracker tracker;
  tracker.recordOutcome(SendOutcomeCategory::TRANSIENT_NETWORK);
  tracker.recordOutcome(SendOutcomeCategory::TRANSIENT_NETWORK);
  tracker.recordOutcome(SendOutcomeCategory::THROTTLING);  // proves the link works
  tracker.recordOutcome(SendOutcomeCategory::TRANSIENT_NETWORK);
  tracker.recordOutcome(SendOutcomeCategory::TRANSIENT_NETWORK);
  assert(tracker.reachable());  // only 2 consecutive since the reset
}

static void test_success_recovers_reachability() {
  TelegramReachabilityTracker tracker;
  tracker.recordOutcome(SendOutcomeCategory::TRANSIENT_NETWORK);
  tracker.recordOutcome(SendOutcomeCategory::TRANSIENT_NETWORK);
  tracker.recordOutcome(SendOutcomeCategory::TRANSIENT_NETWORK);
  assert(!tracker.reachable());

  tracker.recordOutcome(SendOutcomeCategory::SUCCESS);
  assert(tracker.reachable());
}

int main() {
  test_default_reachable_with_no_sends_observed();
  test_becomes_unreachable_after_threshold_consecutive_failures();
  test_non_network_outcome_resets_the_counter();
  test_success_recovers_reachability();

  printf("test_telegram_reachability: all tests passed\n");
  return 0;
}
