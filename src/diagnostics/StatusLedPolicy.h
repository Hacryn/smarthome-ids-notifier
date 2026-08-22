#pragma once

// Sec. 12.2/13 - pure state-selection logic for the status LED (plan
// point 5), separated from the hardware pin writes (StatusLed.h/.cpp) so
// the priority rule is host-testable.
enum class StatusLedState {
  OK,               // green - WiFi connected, NTP synced, no alarm/power-loss open
  NETWORK_OR_TIME,  // yellow - WiFi disconnected/backoff, or NTP not synced
  DEGRADED,         // purple - LittleFS degraded mode (>95% full)
  ALARM,            // red, blinking - at least one ALARM_* or POWER_LOSS event open
};

// Priority when multiple conditions hold at once: ALARM > DEGRADED >
// NETWORK_OR_TIME > OK.
StatusLedState decideStatusLedState(bool wifiConnected, bool ntpSynced, bool alarmOrPowerLossOpen,
                                     bool filesystemDegraded);
