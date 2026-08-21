#pragma once

#include <map>
#include <string>

#include "NotificationRecord.h"

// Sec. 7.1/7.2 - real LittleFS I/O for notif_<chat_id>.jsonl. Not testable
// via the host-side harness (depends on real LittleFS).

// Reconstructs the current state (sec. 7.2) by reading the whole file in
// streaming, one row at a time. A missing file is equivalent to "nothing
// pending" (the common case, sec. 7.2), not an error.
std::map<std::string, NotificationRecord> loadNotificationState(int64_t chatId);

bool appendNotificationRecord(int64_t chatId, const NotificationRecord& rec);
