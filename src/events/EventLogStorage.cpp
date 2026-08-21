#include "EventLogStorage.h"

#include <LittleFS.h>
#include <string.h>

#include <string>

#include "../rotation/FsErrorCounter.h"

namespace {
bool writeEventLine(const std::string& line) {
  File f = LittleFS.open(kEventLogPath, "a");
  if (!f) return false;

  size_t written = f.print(line.c_str());
  f.close();
  return written == line.size();
}
}  // namespace

bool appendEventRecord(const EventRecord& rec) {
  std::string line = serializeEventRecord(rec) + "\n";

  if (writeEventLine(line)) return true;
  if (writeEventLine(line)) return true;  // sec. 9.4 - a single retry

  fsErrorCounter().recordFailure();
  return false;
}

uint32_t readLastWrittenTimestamp() {
  File f = LittleFS.open(kEventLogPath, "r");
  if (!f) return 0;

  long size = static_cast<long>(f.size());
  if (size == 0) {
    f.close();
    return 0;
  }

  // Skip a possible trailing newline, then walk back to the previous
  // newline (or the start of the file) to isolate the last complete row.
  long pos = size - 1;
  f.seek(pos);
  if (f.peek() == '\n') pos--;
  long lineEnd = pos + 1;

  while (pos > 0) {
    f.seek(pos - 1);
    if (f.peek() == '\n') break;
    pos--;
  }
  long lineStart = (pos > 0) ? pos : 0;

  long len = lineEnd - lineStart;
  if (len <= 0) {
    f.close();
    return 0;
  }

  std::string line(static_cast<size_t>(len), '\0');
  f.seek(lineStart);
  f.readBytes(&line[0], len);
  f.close();

  EventRecord rec{};
  if (!parseEventRecord(line, rec)) return 0;
  return rec.ts;
}

bool findEventRecordById(const char* id, EventStatus status, EventRecord& out) {
  File f = LittleFS.open(kEventLogPath, "r");
  if (!f) return false;

  bool found = false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (line.length() == 0) continue;

    EventRecord rec{};
    if (!parseEventRecord(std::string(line.c_str()), rec)) continue;
    if (rec.status == status && strcmp(rec.id, id) == 0) {
      out = rec;
      found = true;
      break;
    }
  }
  f.close();
  return found;
}
