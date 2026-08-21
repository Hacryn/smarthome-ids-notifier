#include "NetworkIssueTracker.h"

NetworkIssueEvent NetworkIssueTracker::update(bool reachable, uint32_t nowMillis,
                                               uint32_t epochNow, uint32_t thresholdSec) {
  if (reachable) {
    if (confirmed_) {
      NetworkIssueEvent ev;
      ev.kind = NetworkIssueEvent::Kind::ENDED;
      ev.ts = epochNow;
      ev.downDurationSec = epochNow - unreachableSinceEpoch_;
      unreachable_ = false;
      confirmed_ = false;
      return ev;
    }
    // Sec. 3.4.1 - a blip that recovers before the threshold generates no event.
    unreachable_ = false;
    return {};
  }

  if (!unreachable_) {
    unreachable_ = true;
    unreachableSinceMillis_ = nowMillis;
    unreachableSinceEpoch_ = epochNow;
    return {};
  }

  if (!confirmed_ && (nowMillis - unreachableSinceMillis_) >= thresholdSec * 1000UL) {
    confirmed_ = true;
    NetworkIssueEvent ev;
    ev.kind = NetworkIssueEvent::Kind::STARTED;
    ev.ts = unreachableSinceEpoch_;  // sec. 3.2.3 - exact instant the outage began
    return ev;
  }

  return {};
}
