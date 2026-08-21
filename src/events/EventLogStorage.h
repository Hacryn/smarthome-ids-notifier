#pragma once

#include <stdint.h>

#include "EventLog.h"

inline const char* kEventLogPath = "/log.jsonl";
inline const char* kEventLogRotationTmpPath = "/log.jsonl.tmp";

// Sec. 5.1 - append-only. Retries once on failure (open failure or
// insufficient bytes written); if it fails again on the second attempt,
// increments the shared counter from sec. 9.4 (fsErrorCounter()) and
// returns false. The event's Telegram notification is not blocked by this
// failure regardless (the caller decides independently).
bool appendEventRecord(const EventRecord& rec);

// Sec. 5.4.3 - reads the file's last row by walking back from the end,
// without a full scan. Returns 0 if the file doesn't exist, is empty, or
// the last row isn't a valid record.
uint32_t readLastWrittenTimestamp();

// Sec. 6.4/6.7 - looks up the (id, status) row in the event log, to recover
// type/approx during a recovery scan (sec. 7.2 doesn't duplicate these
// fields in the notification log). Linear streaming scan; acceptable
// because it's only used during recoveries, which are rare.
bool findEventRecordById(const char* id, EventStatus status, EventRecord& out);
