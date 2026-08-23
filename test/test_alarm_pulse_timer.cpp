#include <cassert>
#include <cstdio>
#include <cstring>

#include "../src/panelcontrol/AlarmCommandTypes.h"
#include "../src/panelcontrol/AlarmPulseTimer.h"

static void test_inactive_by_default() {
  AlarmPulseTimer timer;
  assert(!timer.isActive());
  assert(!timer.shouldRelease(0, kAlarmCommandPulseMs));
}

static void test_active_after_start() {
  AlarmPulseTimer timer;
  timer.start(1000);
  assert(timer.isActive());
}

static void test_should_release_after_pulse_duration_elapses() {
  AlarmPulseTimer timer;
  timer.start(1000);
  assert(!timer.shouldRelease(1499, 500));
  assert(timer.shouldRelease(1500, 500));
  assert(timer.shouldRelease(2000, 500));
}

static void test_release_deactivates() {
  AlarmPulseTimer timer;
  timer.start(1000);
  timer.release();
  assert(!timer.isActive());
  assert(!timer.shouldRelease(2000, 500));
}

static void test_find_alarm_command_config() {
  const AlarmCommandConfig* cfg = findAlarmCommandConfig("GENERALE", /*arm=*/true);
  assert(cfg != nullptr);
  assert(cfg->zone == AlarmZone::GENERAL);
  assert(cfg->action == AlarmAction::ARM);
  assert(cfg->loggedType == EventType::ARM_GENERAL);

  const AlarmCommandConfig* disarmGarage = findAlarmCommandConfig("GARAGE", /*arm=*/false);
  assert(disarmGarage != nullptr);
  assert(disarmGarage->zone == AlarmZone::GARAGE);
  assert(disarmGarage->action == AlarmAction::DISARM);
  assert(disarmGarage->loggedType == EventType::DISARM_GARAGE);

  assert(findAlarmCommandConfig("PIANO", /*arm=*/true) == nullptr);
  assert(findAlarmCommandConfig("generale", /*arm=*/true) == nullptr);  // case-sensitive
}

static void test_all_six_pins_distinct() {
  for (size_t i = 0; i < ALARM_COMMANDS_COUNT; i++) {
    for (size_t j = i + 1; j < ALARM_COMMANDS_COUNT; j++) {
      assert(ALARM_COMMANDS[i].pin != ALARM_COMMANDS[j].pin);
    }
  }
}

int main() {
  test_inactive_by_default();
  test_active_after_start();
  test_should_release_after_pulse_duration_elapses();
  test_release_deactivates();
  test_find_alarm_command_config();
  test_all_six_pins_distinct();

  printf("test_alarm_pulse_timer: all tests passed\n");
  return 0;
}
