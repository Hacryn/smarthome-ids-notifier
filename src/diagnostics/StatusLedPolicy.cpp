#include "StatusLedPolicy.h"

StatusLedState decideStatusLedState(bool wifiConnected, bool ntpSynced, bool alarmOrPowerLossOpen,
                                     bool filesystemDegraded) {
  bool networkOrTimeIssue = !wifiConnected || !ntpSynced;
  if (alarmOrPowerLossOpen && networkOrTimeIssue) return StatusLedState::ALARM_AND_NETWORK_OR_TIME;
  if (alarmOrPowerLossOpen) return StatusLedState::ALARM;
  if (filesystemDegraded) return StatusLedState::DEGRADED;
  if (networkOrTimeIssue) return StatusLedState::NETWORK_OR_TIME;
  return StatusLedState::OK;
}
