#include <cassert>
#include <cstdio>

#include "../src/notifications/NotificationPresentation.h"
#include "../src/notifications/RetryTimer.h"

static void test_recovery_within_grace_period_not_marked_recovered() {
  RecoveryPresentation p = decideRecoveryPresentation(/*nowEpoch=*/1000, /*eventTs=*/900,
                                                        /*eventApprox=*/false, /*grace=*/300);
  assert(!p.isRecovered);
  assert(!p.isApprox);
}

static void test_recovery_beyond_grace_period_marked_recovered() {
  RecoveryPresentation p = decideRecoveryPresentation(1700, 1000, false, 300);  // 700s > 300s
  assert(p.isRecovered);
  assert(!p.isApprox);
}

static void test_recovery_at_exact_grace_boundary_not_recovered() {
  RecoveryPresentation p = decideRecoveryPresentation(1300, 1000, false, 300);  // exactly 300s
  assert(!p.isRecovered);
}

static void test_approx_timestamp_always_recovered_regardless_of_gap() {
  RecoveryPresentation p = decideRecoveryPresentation(1010, 1000, /*eventApprox=*/true, 300);
  assert(p.isRecovered);
  assert(p.isApprox);
}

static void test_should_aggregate_threshold() {
  assert(!shouldAggregate(3, 3));
  assert(shouldAggregate(4, 3));
  assert(!shouldAggregate(0, 3));
}

static void test_exceeds_max_retries() {
  assert(!exceedsMaxRetries(24, 24));
  assert(exceedsMaxRetries(25, 24));
}

static void test_is_near_abandonment() {
  assert(!isNearAbandonment(20, 24));  // 4 attempts left, above the margin
  assert(isNearAbandonment(21, 24));   // 3 attempts left
  assert(isNearAbandonment(24, 24));
  assert(isNearAbandonment(2, 2));     // maxRetries below the margin: threshold = maxRetries itself
}

static void test_retry_timer_starts_on_first_failure() {
  RetryTimer t;
  assert(!t.isDue(0));
  t.onTransientFailure(1000, 60000);
  assert(!t.isDue(1000));
  assert(t.isDue(61000));
}

static void test_retry_timer_resets_on_repeated_failure() {
  RetryTimer t;
  t.onTransientFailure(1000, 60000);
  t.onTransientFailure(50000, 60000);  // new failure before expiry: reset
  assert(!t.isDue(61000));             // would have expired against the first, not the second
  assert(t.isDue(110000));
}

static void test_normal_flow_success_triggers_scan_only_if_active() {
  RetryTimer t;
  assert(!t.onNormalFlowSuccess());  // timer never armed -> no scan

  t.onTransientFailure(1000, 60000);
  assert(t.onNormalFlowSuccess());  // timer active -> triggers an early scan
}

static void test_scan_in_progress_blocks_new_triggers() {
  RetryTimer t;
  t.onTransientFailure(1000, 60000);
  t.beginScan();

  assert(!t.onNormalFlowSuccess());  // sec. 6.3.1 - no success can trigger during a scan
  assert(!t.isDue(999999));          // no new scan while one is in progress
}

static void test_end_scan_cancels_timer_when_all_resolved() {
  RetryTimer t;
  t.onTransientFailure(1000, 60000);
  t.beginScan();
  t.endScan(/*allResolvedOrAbandoned=*/true, 5000, 60000);

  assert(!t.isActive());
  assert(!t.scanInProgress());
  assert(!t.isDue(999999));
}

static void test_end_scan_restarts_timer_when_still_pending() {
  RetryTimer t;
  t.onTransientFailure(1000, 60000);
  t.beginScan();
  t.endScan(/*allResolvedOrAbandoned=*/false, 5000, 60000);

  assert(t.isActive());
  assert(!t.scanInProgress());
  assert(!t.isDue(64999));
  assert(t.isDue(65000));  // 5000 + 60000
}

int main() {
  test_recovery_within_grace_period_not_marked_recovered();
  test_recovery_beyond_grace_period_marked_recovered();
  test_recovery_at_exact_grace_boundary_not_recovered();
  test_approx_timestamp_always_recovered_regardless_of_gap();
  test_should_aggregate_threshold();
  test_exceeds_max_retries();
  test_is_near_abandonment();
  test_retry_timer_starts_on_first_failure();
  test_retry_timer_resets_on_repeated_failure();
  test_normal_flow_success_triggers_scan_only_if_active();
  test_scan_in_progress_blocks_new_triggers();
  test_end_scan_cancels_timer_when_all_resolved();
  test_end_scan_restarts_timer_when_still_pending();

  printf("test_notification_policy: all tests passed\n");
  return 0;
}
