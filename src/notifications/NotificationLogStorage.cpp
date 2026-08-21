#include "NotificationLogStorage.h"

#include <LittleFS.h>

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

bool appendNotificationRecord(int64_t chatId, const NotificationRecord& rec) {
  std::string path = notificationLogPath(chatId);
  File f = LittleFS.open(path.c_str(), "a");
  if (!f) return false;

  std::string line = serializeNotificationRecord(rec) + "\n";
  size_t written = f.print(line.c_str());
  f.close();

  return written == line.size();
}
