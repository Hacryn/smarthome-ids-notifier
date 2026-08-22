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

static void test_alarm_alone_when_network_and_time_are_fine() {
  assert(decideStatusLedState(true, true, true, false) == StatusLedState::ALARM);
}

static void test_alarm_and_network_or_time_when_wifi_down() {
  assert(decideStatusLedState(false, true, true, false) == StatusLedState::ALARM_AND_NETWORK_OR_TIME);
}

static void test_alarm_and_network_or_time_when_ntp_not_synced() {
  assert(decideStatusLedState(true, false, true, false) == StatusLedState::ALARM_AND_NETWORK_OR_TIME);
}

static void test_alarm_and_network_or_time_overrides_degraded() {
  assert(decideStatusLedState(false, false, true, true) == StatusLedState::ALARM_AND_NETWORK_OR_TIME);
}

int main() {
  test_ok_when_everything_healthy();
  test_network_or_time_when_wifi_disconnected();
  test_network_or_time_when_ntp_not_synced();
  test_degraded_overrides_network_or_time();
  test_alarm_alone_when_network_and_time_are_fine();
  test_alarm_and_network_or_time_when_wifi_down();
  test_alarm_and_network_or_time_when_ntp_not_synced();
  test_alarm_and_network_or_time_overrides_degraded();

  printf("test_status_led_policy: all tests passed\n");
  return 0;
}
