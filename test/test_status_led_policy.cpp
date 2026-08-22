#include <cassert>
#include <cstdio>

#include "../src/diagnostics/StatusLedPolicy.h"

static void test_ok_when_everything_healthy() {
  assert(decideStatusLedState(true, true, false, false) == StatusLedState::OK);
}

static void test_network_or_time_when_wifi_disconnected() {
  assert(decideStatusLedState(false, true, false, false) == StatusLedState::NETWORK_OR_TIME);
}

static void test_network_or_time_when_ntp_not_synced() {
  assert(decideStatusLedState(true, false, false, false) == StatusLedState::NETWORK_OR_TIME);
}

static void test_degraded_overrides_network_or_time() {
  assert(decideStatusLedState(false, false, false, true) == StatusLedState::DEGRADED);
}

static void test_alarm_overrides_everything() {
  assert(decideStatusLedState(false, false, true, true) == StatusLedState::ALARM);
  assert(decideStatusLedState(true, true, true, false) == StatusLedState::ALARM);
}

int main() {
  test_ok_when_everything_healthy();
  test_network_or_time_when_wifi_disconnected();
  test_network_or_time_when_ntp_not_synced();
  test_degraded_overrides_network_or_time();
  test_alarm_overrides_everything();

  printf("test_status_led_policy: all tests passed\n");
  return 0;
}
