#pragma once

#include <stdint.h>

// Sec. 3.4.1 - pure state machine for NETWORK_ISSUE detection. Receives a
// generic reachability signal (currently: WiFi status only; sec. 3.4.1
// also requires Telegram API reachability, which will be combined here
// once the client is introduced - phase 6).
struct NetworkIssueEvent {
  enum class Kind { NONE, STARTED, ENDED } kind = Kind::NONE;
  uint32_t ts = 0;              // detection instant (START) or restoration instant (END)
  uint32_t downDurationSec = 0;  // set only for ENDED
};

class NetworkIssueTracker {
 public:
  // To be called periodically. epochNow is the current epoch estimate,
  // used to date the START (at the moment unreachability began, not when
  // the threshold is crossed) and the END.
  NetworkIssueEvent update(bool reachable, uint32_t nowMillis, uint32_t epochNow,
                            uint32_t thresholdSec);

 private:
  bool unreachable_ = false;
  bool confirmed_ = false;  // threshold already crossed, START already emitted
  uint32_t unreachableSinceMillis_ = 0;
  uint32_t unreachableSinceEpoch_ = 0;
};
