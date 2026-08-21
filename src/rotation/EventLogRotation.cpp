#include "EventLogRotation.h"

#include <LittleFS.h>

#include <string>

#include "../events/EventLogStorage.h"
#include "FsErrorCounter.h"
#include "RotationCollector.h"

int rotateEventLog(uint32_t cutoff) {
  int cycles = 0;
  bool capped = true;

  while (capped) {
    DeletableIdCollector collector(cutoff);

    // Pass 1 (sec. 9.3.1) - streaming collection, no file held in memory.
    File src = LittleFS.open(kEventLogPath, "r");
    if (!src) return cycles;  // no log to rotate
    while (src.available()) {
      String line = src.readStringUntil('\n');
      if (line.length() == 0) continue;

      EventRecord rec{};
      if (parseEventRecord(std::string(line.c_str()), rec)) collector.observe(rec);
    }
    src.close();

    if (collector.ids().empty()) break;  // nothing to delete, done

    // Pass 2 - filtered rewrite to a temporary file.
    File srcAgain = LittleFS.open(kEventLogPath, "r");
    File tmp = LittleFS.open(kEventLogRotationTmpPath, "w");
    if (!srcAgain || !tmp) return -1;

    bool writeOk = true;
    while (srcAgain.available()) {
      String line = srcAgain.readStringUntil('\n');
      if (line.length() == 0) continue;

      EventRecord rec{};
      if (!parseEventRecord(std::string(line.c_str()), rec)) continue;  // corrupted row, discarded
      if (collector.contains(rec.id)) continue;  // deletable

      std::string outLine = serializeEventRecord(rec) + "\n";
      size_t written = tmp.print(outLine.c_str());
      if (written != outLine.size()) {
        writeOk = false;
        break;
      }
    }
    srcAgain.close();
    tmp.close();

    // Sec. 9.3.2 - atomic rename first, NVS timestamp updated after (by the caller).
    if (!writeOk || !LittleFS.rename(kEventLogRotationTmpPath, kEventLogPath)) {
      fsErrorCounter().recordFailure();
      return -1;
    }

    cycles++;
    capped = collector.capReached();  // sec. 9.3.1 - repeats only if the cap was reached
  }

  return cycles;
}

void cleanupStaleEventLogRotation() {
  if (LittleFS.exists(kEventLogRotationTmpPath)) {
    LittleFS.remove(kEventLogRotationTmpPath);
  }
}
