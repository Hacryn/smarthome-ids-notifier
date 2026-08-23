#pragma once

#include <stddef.h>
#include <string.h>

#include "../events/EventTypes.h"

// Sec. 3.4.3 - output-side analogue of EVENT_TYPES: maps a /setalarm
// argument to the Ax pin driven to issue that command and the EventType
// used to log/notify it.
enum class AlarmZone : uint8_t { GENERAL, INTERNAL, GARAGE };
enum class AlarmAction : uint8_t { ARM, DISARM };

struct AlarmCommandConfig {
  AlarmZone zone;
  AlarmAction action;
  const char* zoneToken;  // "GENERALE"/"INTERNO"/"GARAGE" - the /setalarm argument, verbatim
  int pin;                // Ax pin driven to issue this command (Arduino "By Arduino pin" numbering)
  EventType loggedType;   // EVENT_TYPES entry used for the log row + notification
};

// Pin values are the raw Arduino-numbering integers for A0/A1/A2/A5/A6/A7
// (17/18/19/22/23/24 on this board's variant) rather than the Ax core
// macros, so this table stays free of an Arduino.h dependency and
// host-testable - same convention EventTypes.h already uses for its own
// pin column (plain ints, not the Dx macros).
inline const AlarmCommandConfig ALARM_COMMANDS[] = {
    {AlarmZone::GENERAL, AlarmAction::ARM, "GENERALE", 17, EventType::ARM_GENERAL},      // A0
    {AlarmZone::GENERAL, AlarmAction::DISARM, "GENERALE", 22, EventType::DISARM_GENERAL},  // A5
    {AlarmZone::INTERNAL, AlarmAction::ARM, "INTERNO", 18, EventType::ARM_INTERNAL},     // A1
    {AlarmZone::INTERNAL, AlarmAction::DISARM, "INTERNO", 23, EventType::DISARM_INTERNAL},  // A6
    {AlarmZone::GARAGE, AlarmAction::ARM, "GARAGE", 19, EventType::ARM_GARAGE},          // A2
    {AlarmZone::GARAGE, AlarmAction::DISARM, "GARAGE", 24, EventType::DISARM_GARAGE},    // A7
};
constexpr size_t ALARM_COMMANDS_COUNT = sizeof(ALARM_COMMANDS) / sizeof(ALARM_COMMANDS[0]);

// Case-sensitive match on zoneToken, as already the project's convention
// for command arguments (e.g. /notify's type names).
inline const AlarmCommandConfig* findAlarmCommandConfig(const char* zoneToken, bool arm) {
  AlarmAction action = arm ? AlarmAction::ARM : AlarmAction::DISARM;
  for (size_t i = 0; i < ALARM_COMMANDS_COUNT; i++) {
    if (ALARM_COMMANDS[i].action == action && strcmp(ALARM_COMMANDS[i].zoneToken, zoneToken) == 0) {
      return &ALARM_COMMANDS[i];
    }
  }
  return nullptr;
}
