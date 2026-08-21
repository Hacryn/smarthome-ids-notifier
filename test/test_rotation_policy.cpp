#include <cassert>
#include <cstdio>

#include "../src/rotation/FsErrorCounter.h"
#include "../src/rotation/RotationPolicy.h"

static void test_retention_cutoff() {
  uint32_t nowEpoch = 1755500000;
  uint32_t cutoff = retentionCutoff(nowEpoch, 52);
  assert(cutoff == nowEpoch - 52UL * kSecondsPerWeek);
}

static void test_rotation_due_threshold() {
  uint32_t last = 1000;
  assert(!isRotationDue(last, last + kRotationIntervalSec - 1));
  assert(isRotationDue(last, last + kRotationIntervalSec));
}

static void test_space_status_thresholds() {
  assert(evaluateSpaceUsage(0, 1000) == SpaceStatus::NORMAL);
  assert(evaluateSpaceUsage(799, 1000) == SpaceStatus::NORMAL);
  assert(evaluateSpaceUsage(800, 1000) == SpaceStatus::ROTATE_EARLY);
  assert(evaluateSpaceUsage(949, 1000) == SpaceStatus::ROTATE_EARLY);
  assert(evaluateSpaceUsage(950, 1000) == SpaceStatus::DEGRADED);
  assert(evaluateSpaceUsage(1000, 1000) == SpaceStatus::DEGRADED);
}

static void test_space_status_zero_total_is_normal() {
  assert(evaluateSpaceUsage(0, 0) == SpaceStatus::NORMAL);
}

static void test_fs_error_counter_first_failure_flag() {
  FsErrorCounter counter;
  assert(counter.recordFailure());   // primo errore
  assert(!counter.recordFailure());  // secondo, non piu' "primo"
  assert(counter.count() == 2);
}

int main() {
  test_retention_cutoff();
  test_rotation_due_threshold();
  test_space_status_thresholds();
  test_space_status_zero_total_is_normal();
  test_fs_error_counter_first_failure_flag();

  printf("test_rotation_policy: tutti i test superati\n");
  return 0;
}
