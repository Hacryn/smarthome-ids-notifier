#pragma once

#include <stdint.h>

#include <string>

#include "../events/EventTypes.h"

// Sec. 7.2 - which notification of the event is being tracked.
enum class NotifyStatus : uint8_t {
  NOTIFIED_INSTANT = 0,
  NOTIFIED_START = 1,
  NOTIFIED_END = 2,
};

enum class NotifyState : uint8_t {
  PENDING = 0,
  RESOLVED = 1,
  ABANDONED = 2,
};

struct NotificationRecord {
  char id[33];  // same id as the event in log.jsonl (sec. 5.2)
  NotifyStatus status;
  uint32_t ts;
  NotifyState state;
  uint32_t n = 0;  // relevant only for PENDING/ABANDONED (sec. 7.2)
};

NotifyStatus eventStatusToNotifyStatus(EventStatus status);
EventStatus notifyStatusToEventStatus(NotifyStatus status);

// Sec. 4.2 - the chat_id's minus sign must be replaced with a text prefix
// when composing a filename. Pure formatting, no dependency on the FS.
std::string notificationLogPath(int64_t chatId);

// Grouping key (id, status) for reconstructing pending state (sec. 7.2 -
// "the last row wins" for each pair).
std::string notificationKey(const char* id, NotifyStatus status);

// Sec. 7.2 - schema: {"id","status","ts","state","n"?}. "n" omitted on RESOLVED.
std::string serializeNotificationRecord(const NotificationRecord& rec);
bool parseNotificationRecord(const std::string& line, NotificationRecord& out);
