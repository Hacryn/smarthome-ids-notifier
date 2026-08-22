#pragma once

// Sec. 12.2/13 - pure state-selection logic for the status LED (plan
// point 5), separated from the hardware pin writes (StatusLed.h/.cpp) so
// the priority rule is host-testable.
enum class StatusLedState {
  OK,                        // green - WiFi connected, NTP synced, no alarm/power-loss open
  NETWORK_OR_TIME,           // yellow - WiFi disconnected/backoff, or NTP not synced
  DEGRADED,                  // purple - LittleFS degraded mode (>95% full)
  ALARM,                     // red, blinking - alarm/power-loss open, WiFi/NTP ok
  ALARM_AND_NETWORK_OR_TIME, // alternating red/yellow - alarm/power-loss open AND WiFi/NTP issue
};

// Priority when multiple conditions hold at once: ALARM_AND_NETWORK_OR_TIME
// > ALARM > DEGRADED > NETWORK_OR_TIME > OK. The combined state outranks
// both states it replaces because it's strictly more informative than
// either alone - an alarm during a network/time outage must be
// distinguishable at a glance from an alarm with connectivity intact.
StatusLedState decideStatusLedState(bool wifiConnected, bool ntpSynced, bool alarmOrPowerLossOpen,
                                     bool filesystemDegraded);
