#pragma once

#include <stdint.h>

#include <vector>

#include "EventLog.h"

struct OpenEvent {
  char id[33];
  EventType type;
  uint32_t startTs;
  bool approx;
};

// Sec. 8 - given a stream of rows (in file order), returns the duration
// events still missing an END row (open ones). INSTANT rows (e.g. REBOOT)
// never open/close anything.
std::vector<OpenEvent> findOpenEvents(const std::vector<EventRecord>& rows);

// Returns the most recent open event of a given type. Picking the most
// recent one also handles historical logs created before the ID fix.
bool findMostRecentOpenEventForType(const std::vector<EventRecord>& rows, EventType type,
                                    OpenEvent& out);
