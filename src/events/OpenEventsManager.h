#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../users/UserList.h"
#include "OpenEventsTracker.h"

// Sec. 8 - management of events left open after an unexpected reboot.
// Not testable via the host-side harness (depends on real LittleFS/
// Telegram); the detection logic (OpenEventsTracker) is tested separately.

// Reads the whole log.jsonl and returns the still-open events.
std::vector<OpenEvent> detectOpenEvents();

// Looks up the most recent open event of a type in the persisted log.
bool findOpenEventOfType(EventType type, OpenEvent& out);

// Sec. 8.1 - closes an open event: checks it's still open (already-closed
// or nonexistent id -> false, protection against a double-click/duplicate
// command), writes the END row and triggers the normal notification flow
// (sec. 6.1).
bool closeOpenEvent(const std::vector<AuthorizedUser>& users, const std::string& id,
                     uint32_t closeTs, uint32_t& lastWrittenTs);

// Sec. 8 - sends the open-events summary (plus any PENDING notifications
// close to being abandoned, sec. 7.2) to every authorized user: with
// inline "close" buttons for admins, without for standard users. Sends
// nothing to a user who has nothing to see.
void sendOpenEventsSummary(const std::vector<AuthorizedUser>& users);
