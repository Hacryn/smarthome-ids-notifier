#include "NotificationLogRotation.h"

#include <LittleFS.h>

#include <string>

#include "../notifications/NotificationLogStorage.h"
#include "../notifications/NotificationRecord.h"
#include "FsErrorCounter.h"

bool rotateNotificationLog(int64_t chatId, uint32_t cutoff) {
  auto state = loadNotificationState(chatId);  // full fold (sec. 7.2)

  std::string path = notificationLogPath(chatId);
  File src = LittleFS.open(path.c_str(), "r");
  if (!src) return true;  // nothing to rotate

  std::string tmpPath = path + ".tmp";
  File tmp = LittleFS.open(tmpPath.c_str(), "w");
  if (!tmp) {
    src.close();
    return false;
  }

  bool ok = true;
  while (src.available()) {
    String line = src.readStringUntil('\n');
    if (line.length() == 0) continue;

    NotificationRecord rec{};
    if (!parseNotificationRecord(std::string(line.c_str()), rec)) continue;

    std::string key = notificationKey(rec.id, rec.status);
    auto it = state.find(key);
    bool eligible = it != state.end() && it->second.state != NotifyState::PENDING &&
                    it->second.ts < cutoff;
    if (eligible) continue;

    std::string outLine = serializeNotificationRecord(rec) + "\n";
    size_t written = tmp.print(outLine.c_str());
    if (written != outLine.size()) {
      ok = false;
      break;
    }
  }
  src.close();
  tmp.close();

  if (!ok || !LittleFS.rename(tmpPath.c_str(), path.c_str())) {
    fsErrorCounter().recordFailure();
    return false;
  }
  return true;
}
