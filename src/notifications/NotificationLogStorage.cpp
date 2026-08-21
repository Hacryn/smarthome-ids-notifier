#include "NotificationLogStorage.h"

#include <LittleFS.h>

#include "../rotation/FsErrorCounter.h"
#include "NotificationFolder.h"

std::map<std::string, NotificationRecord> loadNotificationState(int64_t chatId) {
  std::vector<NotificationRecord> rows;

  std::string path = notificationLogPath(chatId);
  File f = LittleFS.open(path.c_str(), "r");
  if (f) {
    while (f.available()) {
      String line = f.readStringUntil('\n');
      if (line.length() == 0) continue;

      NotificationRecord rec{};
      if (parseNotificationRecord(std::string(line.c_str()), rec)) {
        rows.push_back(rec);
      }
    }
    f.close();
  }

  return foldNotificationRecords(rows);
}

namespace {
bool writeNotificationLine(const std::string& path, const std::string& line) {
  File f = LittleFS.open(path.c_str(), "a");
  if (!f) return false;

  size_t written = f.print(line.c_str());
  f.close();
  return written == line.size();
}
}  // namespace

bool appendNotificationRecord(int64_t chatId, const NotificationRecord& rec) {
  std::string path = notificationLogPath(chatId);
  std::string line = serializeNotificationRecord(rec) + "\n";

  if (writeNotificationLine(path, line)) return true;
  if (writeNotificationLine(path, line)) return true;  // sec. 9.4 - a single retry

  fsErrorCounter().recordFailure();
  return false;
}
