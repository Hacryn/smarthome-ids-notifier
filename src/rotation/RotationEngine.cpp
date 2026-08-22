#include "RotationEngine.h"

#include <LittleFS.h>

#include <string>

#include "../diagnostics/SerialLog.h"
#include "../notifications/NotificationEngine.h"
#include "../notifications/NotificationRecord.h"
#include "EventLogRotation.h"
#include "NotificationLogRotation.h"
#include "RotationStorage.h"

namespace {

SpaceStatus g_lastSpaceStatus = SpaceStatus::NORMAL;

int severityOf(SpaceStatus s) {
  switch (s) {
    case SpaceStatus::NORMAL:
      return 0;
    case SpaceStatus::ROTATE_EARLY:
      return 1;
    case SpaceStatus::DEGRADED:
      return 2;
  }
  return 0;
}

SpaceStatus readSpaceStatus() {
  return evaluateSpaceUsage(LittleFS.usedBytes(), LittleFS.totalBytes());
}

void rotateEverything(const std::vector<AuthorizedUser>& users, uint32_t nowEpoch,
                       uint32_t retentionWeeks) {
  uint32_t cutoff = retentionCutoff(nowEpoch, retentionWeeks);
  logInfo("Rotation starting: cutoff=%lu", static_cast<unsigned long>(cutoff));

  // Sec. 9.3.2 - the NVS timestamp advances only if the event log rotation
  // didn't fail; a failure on individual notification files doesn't block
  // the cycle (they'll be retried at the next rotation).
  int cycles = rotateEventLog(cutoff);
  if (cycles >= 0) {
    saveLastRotationEpoch(nowEpoch);
  }

  for (const auto& user : users) {
    rotateNotificationLog(user.chatId, cutoff);
  }

  logInfo("Rotation finished: event log cycles=%d", cycles);
}

}  // namespace

bool isFilesystemDegraded() { return g_lastSpaceStatus == SpaceStatus::DEGRADED; }

void performMaintenanceIfDue(const std::vector<AuthorizedUser>& users, uint32_t nowEpoch,
                              uint32_t retentionWeeks) {
  SpaceStatus newStatus = readSpaceStatus();
  bool worsened = severityOf(newStatus) > severityOf(g_lastSpaceStatus);
  g_lastSpaceStatus = newStatus;

  if (worsened) {
    if (newStatus == SpaceStatus::ROTATE_EARLY) {
      notifyAdmins(users, "Spazio LittleFS oltre l'80%: rotazione anticipata in corso.");
    } else {
      notifyAdmins(users, "Spazio LittleFS oltre il 95%: modalita' degradata attiva.");
    }
    rotateEverything(users, nowEpoch, retentionWeeks);
    return;
  }

  if (isRotationDue(loadLastRotationEpoch(), nowEpoch)) {
    rotateEverything(users, nowEpoch, retentionWeeks);
  }
}

void cleanupStaleRotationFiles(const std::vector<AuthorizedUser>& users) {
  cleanupStaleEventLogRotation();

  for (const auto& user : users) {
    std::string tmp = notificationLogPath(user.chatId) + ".tmp";
    if (LittleFS.exists(tmp.c_str())) LittleFS.remove(tmp.c_str());
  }
}
