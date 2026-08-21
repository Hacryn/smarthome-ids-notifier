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

    // Passata 1 (sez. 9.3.1) - raccolta in streaming, nessun file in memoria.
    File src = LittleFS.open(kEventLogPath, "r");
    if (!src) return cycles;  // nessun log da ruotare
    while (src.available()) {
      String line = src.readStringUntil('\n');
      if (line.length() == 0) continue;

      EventRecord rec{};
      if (parseEventRecord(std::string(line.c_str()), rec)) collector.observe(rec);
    }
    src.close();

    if (collector.ids().empty()) break;  // nulla da eliminare, fine

    // Passata 2 - riscrittura filtrata su file temporaneo.
    File srcAgain = LittleFS.open(kEventLogPath, "r");
    File tmp = LittleFS.open(kEventLogRotationTmpPath, "w");
    if (!srcAgain || !tmp) return -1;

    bool writeOk = true;
    while (srcAgain.available()) {
      String line = srcAgain.readStringUntil('\n');
      if (line.length() == 0) continue;

      EventRecord rec{};
      if (!parseEventRecord(std::string(line.c_str()), rec)) continue;  // riga corrotta, scartata
      if (collector.contains(rec.id)) continue;  // eliminabile

      std::string outLine = serializeEventRecord(rec) + "\n";
      size_t written = tmp.print(outLine.c_str());
      if (written != outLine.size()) {
        writeOk = false;
        break;
      }
    }
    srcAgain.close();
    tmp.close();

    // Sez. 9.3.2 - rename atomico prima, timestamp NVS aggiornato dopo (dal chiamante).
    if (!writeOk || !LittleFS.rename(kEventLogRotationTmpPath, kEventLogPath)) {
      fsErrorCounter().recordFailure();
      return -1;
    }

    cycles++;
    capped = collector.capReached();  // sez. 9.3.1 - ripete solo se il tetto e' stato raggiunto
  }

  return cycles;
}

void cleanupStaleEventLogRotation() {
  if (LittleFS.exists(kEventLogRotationTmpPath)) {
    LittleFS.remove(kEventLogRotationTmpPath);
  }
}
