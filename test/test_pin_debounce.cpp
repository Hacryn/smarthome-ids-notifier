#include <cassert>
#include <cstdio>

#include "../src/EventTiming.h"
#include "../src/PinDebounce.h"

static void test_poll_before_threshold_returns_false() {
  PinDebouncer d;
  d.onTransition(1, 1000);
  DebouncedTransition out;
  assert(!d.poll(1200, 300, out));  // 200ms trascorsi, sotto soglia 300ms
}

static void test_poll_after_threshold_confirms() {
  PinDebouncer d;
  d.onTransition(1, 1000);
  DebouncedTransition out;
  assert(d.poll(1300, 300, out));
  assert(out.level == 1);
  assert(out.millisAtIsr == 1000);
}

static void test_poll_confirms_only_once() {
  PinDebouncer d;
  d.onTransition(1, 1000);
  DebouncedTransition out;
  assert(d.poll(1300, 300, out));
  assert(!d.poll(1400, 300, out));  // nessuna transizione pendente
}

static void test_bounce_resets_window_and_final_level_wins() {
  PinDebouncer d;
  d.onTransition(1, 1000);  // rimbalzo iniziale
  d.onTransition(0, 1100);  // rimbalzo entro 300ms: sostituisce il pendente
  DebouncedTransition out;
  assert(!d.poll(1200, 300, out));  // solo 100ms dall'ultimo rimbalzo

  assert(d.poll(1400, 300, out));  // 300ms dall'ultimo rimbalzo (a 1100)
  assert(out.level == 0);          // vince l'ultimo livello, non il primo
  assert(out.millisAtIsr == 1100);
}

static void test_resolve_pin_event_status_active_low() {
  assert(resolvePinEventStatus(0, /*activeLow=*/true) == EventStatus::START);
  assert(resolvePinEventStatus(1, /*activeLow=*/true) == EventStatus::END);
}

static void test_resolve_pin_event_status_active_high() {
  assert(resolvePinEventStatus(1, /*activeLow=*/false) == EventStatus::START);
  assert(resolvePinEventStatus(0, /*activeLow=*/false) == EventStatus::END);
}

static void test_compute_retroactive_timestamp() {
  // Bloccati 10s in un timeout di rete: l'evento va datato all'istante ISR, non ora.
  uint32_t ts = computeRetroactiveTimestamp(/*epochNow=*/1000, /*millisNow=*/15000,
                                             /*millisAtIsr=*/5000);
  assert(ts == 990);
}

static void test_compute_retroactive_timestamp_no_delay() {
  uint32_t ts = computeRetroactiveTimestamp(1000, 5000, 5000);
  assert(ts == 1000);
}

int main() {
  test_poll_before_threshold_returns_false();
  test_poll_after_threshold_confirms();
  test_poll_confirms_only_once();
  test_bounce_resets_window_and_final_level_wins();
  test_resolve_pin_event_status_active_low();
  test_resolve_pin_event_status_active_high();
  test_compute_retroactive_timestamp();
  test_compute_retroactive_timestamp_no_delay();

  printf("test_pin_debounce: tutti i test superati\n");
  return 0;
}
