#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../events/EventTypes.h"
#include "../users/UserList.h"

// Sec. 6/7 - notification engine. Not testable via the host-side harness
// (depends on real LittleFS/Telegram); the pure logic governing it
// (NotificationPresentation, RetryTimer, NotificationFolder) is tested
// separately.

// Sec. 6.1 - normal flow: enqueues a send for every eligible user
// (whitelist + the added_ts filter from sec. 4.6). Non-blocking: the
// actual send happens on subsequent loop cycles, at the rate limiter's
// pace (sec. 6.6) via tickNotificationEngine. Call this right after
// successfully writing the row to log.jsonl.
void notifyEvent(const std::vector<AuthorizedUser>& users, const char* id, EventType type,
                  EventStatus status, uint32_t eventTs, bool eventApprox);

// Sec. 6.2/6.3/6.4/6.7 - recovery scan: call on boot, on connectivity
// restoration after NETWORK_ISSUE, and (automatically, via
// tickNotificationEngine) on retry-timer expiry. A request received while
// a scan is already in progress is discarded (sec. 6.3.1): the in-progress
// scan re-reads the complete state anyway.
void runRecoveryScan(const std::vector<AuthorizedUser>& users, uint32_t nowMillis,
                      uint32_t nowEpoch);

// Call on every loop cycle: drains the send queue at the rate limiter's
// pace and triggers the recovery scan on retry-timer expiry or after a
// success in the normal flow (sec. 6.3).
void tickNotificationEngine(const std::vector<AuthorizedUser>& users, uint32_t nowMillis,
                             uint32_t nowEpoch);

// Direct send (not queued, not tracked in sec. 7) to every admin - system
// messages such as sec. 9.4 alerts, not event notifications. Also reused
// by OpenEventsManager for sec. 6.5.
void notifyAdmins(const std::vector<AuthorizedUser>& users, const std::string& text);

// Sec. 12.2 - state exposed in /status.
bool isRetryTimerActive();
std::string lastSystemError();  // empty if none (sec. 6.5)
