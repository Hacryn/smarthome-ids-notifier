#include "NotificationRecord.h"

#include <ArduinoJson.h>
#include <string.h>

NotifyStatus eventStatusToNotifyStatus(EventStatus status) {
  switch (status) {
    case EventStatus::INSTANT:
      return NotifyStatus::NOTIFIED_INSTANT;
    case EventStatus::START:
      return NotifyStatus::NOTIFIED_START;
    case EventStatus::END:
    default:
      return NotifyStatus::NOTIFIED_END;
  }
}

EventStatus notifyStatusToEventStatus(NotifyStatus status) {
  switch (status) {
    case NotifyStatus::NOTIFIED_INSTANT:
      return EventStatus::INSTANT;
    case NotifyStatus::NOTIFIED_START:
      return EventStatus::START;
    case NotifyStatus::NOTIFIED_END:
    default:
      return EventStatus::END;
  }
}

std::string notificationLogPath(int64_t chatId) {
  std::string path = "/notif_";
  if (chatId < 0) {
    path += "g";
    path += std::to_string(-chatId);
  } else {
    path += std::to_string(chatId);
  }
  path += ".jsonl";
  return path;
}

std::string notificationKey(const char* id, NotifyStatus status) {
  std::string key(id);
  key += ':';
  key += std::to_string(static_cast<int>(status));
  return key;
}

std::string serializeNotificationRecord(const NotificationRecord& rec) {
  JsonDocument doc;
  doc["id"] = rec.id;
  doc["status"] = static_cast<uint8_t>(rec.status);
  doc["ts"] = rec.ts;
  doc["state"] = static_cast<uint8_t>(rec.state);
  if (rec.state != NotifyState::RESOLVED) doc["n"] = rec.n;

  std::string out;
  serializeJson(doc, out);
  return out;
}

bool parseNotificationRecord(const std::string& line, NotificationRecord& out) {
  JsonDocument doc;
  if (deserializeJson(doc, line)) return false;

  JsonVariantConst id = doc["id"];
  JsonVariantConst status = doc["status"];
  JsonVariantConst ts = doc["ts"];
  JsonVariantConst state = doc["state"];
  if (id.isNull() || status.isNull() || ts.isNull() || state.isNull()) return false;

  const char* idStr = id.as<const char*>();
  if (idStr == nullptr || strlen(idStr) != 32) return false;

  strncpy(out.id, idStr, sizeof(out.id) - 1);
  out.id[32] = '\0';
  out.status = static_cast<NotifyStatus>(status.as<uint8_t>());
  out.ts = ts.as<uint32_t>();
  out.state = static_cast<NotifyState>(state.as<uint8_t>());
  out.n = doc["n"].as<uint32_t>();  // absent -> 0

  return true;
}
