#include "StatusLedPolicy.h"

StatusLedState decideStatusLedState(bool wifiConnected, bool ntpSynced, bool alarmOrPowerLossOpen,
                                     bool filesystemDegraded) {
  if (alarmOrPowerLossOpen) return StatusLedState::ALARM;
  if (filesystemDegraded) return StatusLedState::DEGRADED;
  if (!wifiConnected || !ntpSynced) return StatusLedState::NETWORK_OR_TIME;
  return StatusLedState::OK;
}
