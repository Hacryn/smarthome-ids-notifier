#include <cassert>
#include <cstdio>

#include "../src/time/ClockPolicy.h"

static void test_epoch_near_zero_not_plausible() {
  assert(!isEpochPlausible(0));
  assert(!isEpochPlausible(3600));  // a few hours past the epoch, clock never synced
}

static void test_epoch_at_and_above_threshold_plausible() {
  assert(isEpochPlausible(kPlausibleEpochThreshold));
  assert(isEpochPlausible(1755500000));  // August 2026, this project's development date
}

static void test_epoch_just_below_threshold_not_plausible() {
  assert(!isEpochPlausible(kPlausibleEpochThreshold - 1));
}

int main() {
  test_epoch_near_zero_not_plausible();
  test_epoch_at_and_above_threshold_plausible();
  test_epoch_just_below_threshold_not_plausible();

  printf("test_clock_policy: all tests passed\n");
  return 0;
}
