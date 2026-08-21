#pragma once

#include <map>
#include <string>
#include <vector>

#include "NotificationRecord.h"

// Sec. 7.2 - applies the file's rows in order: the last row for each
// (id, status) pair is the valid one.
std::map<std::string, NotificationRecord> foldNotificationRecords(
    const std::vector<NotificationRecord>& rows);

// Subset of entries still PENDING (the ones to recover, sec. 6.2).
std::vector<NotificationRecord> pendingFrom(const std::map<std::string, NotificationRecord>& latest);
