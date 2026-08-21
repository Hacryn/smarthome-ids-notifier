#include "NotificationFolder.h"

std::map<std::string, NotificationRecord> foldNotificationRecords(
    const std::vector<NotificationRecord>& rows) {
  std::map<std::string, NotificationRecord> latest;
  for (const auto& row : rows) {
    latest[notificationKey(row.id, row.status)] = row;
  }
  return latest;
}

std::vector<NotificationRecord> pendingFrom(const std::map<std::string, NotificationRecord>& latest) {
  std::vector<NotificationRecord> pending;
  for (const auto& [key, rec] : latest) {
    (void)key;
    if (rec.state == NotifyState::PENDING) pending.push_back(rec);
  }
  return pending;
}
