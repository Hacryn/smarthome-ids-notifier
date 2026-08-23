#include "AlarmCommandOutput.h"

#include <Arduino.h>

#include "AlarmPulseTimer.h"

namespace {
AlarmPulseTimer g_pulseTimer;
int g_activePin = -1;
}  // namespace

void initAlarmCommandOutputs() {
  for (size_t i = 0; i < ALARM_COMMANDS_COUNT; i++) {
    pinMode(ALARM_COMMANDS[i].pin, OUTPUT);
    digitalWrite(ALARM_COMMANDS[i].pin, LOW);
  }
}

bool triggerAlarmCommand(const AlarmCommandConfig& cfg) {
  if (g_pulseTimer.isActive()) return false;

  digitalWrite(cfg.pin, HIGH);
  g_activePin = cfg.pin;
  g_pulseTimer.start(millis());
  return true;
}

void tickAlarmCommandOutput(uint32_t nowMillis) {
  if (!g_pulseTimer.shouldRelease(nowMillis, kAlarmCommandPulseMs)) return;

  digitalWrite(g_activePin, LOW);
  g_activePin = -1;
  g_pulseTimer.release();
}

bool isAlarmCommandPulseInProgress() { return g_pulseTimer.isActive(); }
