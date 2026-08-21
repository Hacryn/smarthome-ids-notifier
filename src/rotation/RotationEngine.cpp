#include "RotationEngine.h"

#include <LittleFS.h>

#include <string>

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

  // Sez. 9.3.2 - il timestamp NVS avanza solo se la rotazione del log
  // eventi non e' fallita; un fallimento sui singoli file di notifica non
  // blocca il ciclo (verranno ritentati alla rotazione successiva).
  int cycles = rotateEventLog(cutoff);
  if (cycles >= 0) {
    saveLastRotationEpoch(nowEpoch);
  }

  for (const auto& user : users) {
    rotateNotificationLog(user.chatId, cutoff);
  }
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
