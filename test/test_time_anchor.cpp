#include <cassert>
#include <cstdio>

#include "../src/TimeAnchor.h"

static void test_estimate_timestamp() {
  // last_epoch salvato a 1755500000, 90 secondi trascorsi dal boot.
  uint32_t ts = estimateTimestamp(1755500000, 90000);
  assert(ts == 1755500090);
}

static void test_estimate_timestamp_zero_millis() {
  uint32_t ts = estimateTimestamp(1755500000, 0);
  assert(ts == 1755500000);
}

static void test_clamp_not_needed_when_increasing() {
  ClampedTimestamp result = applyMonotonicClamp(1755500100, 1755500000);
  assert(result.ts == 1755500100);
  assert(!result.wasClamped);
}

static void test_clamp_applied_when_going_backwards() {
  // Correzione NTP all'indietro: il candidato e' anteriore all'ultima riga scritta.
  ClampedTimestamp result = applyMonotonicClamp(1755499000, 1755500000);
  assert(result.ts == 1755500000);
  assert(result.wasClamped);
}

static void test_clamp_equal_is_not_clamped() {
  ClampedTimestamp result = applyMonotonicClamp(1755500000, 1755500000);
  assert(result.ts == 1755500000);
  assert(!result.wasClamped);
}

static void test_should_persist_anchor_threshold() {
  assert(!shouldPersistAnchor(ANCHOR_PERSIST_INTERVAL_MS - 1));
  assert(shouldPersistAnchor(ANCHOR_PERSIST_INTERVAL_MS));
  assert(shouldPersistAnchor(ANCHOR_PERSIST_INTERVAL_MS + 1));
}

int main() {
  test_estimate_timestamp();
  test_estimate_timestamp_zero_millis();
  test_clamp_not_needed_when_increasing();
  test_clamp_applied_when_going_backwards();
  test_clamp_equal_is_not_clamped();
  test_should_persist_anchor_threshold();

  printf("test_time_anchor: tutti i test superati\n");
  return 0;
}
