#include <LittleFS.h>

namespace {

bool removeIfPresent(const char* path) {
  if (!LittleFS.exists(path)) return true;
  return LittleFS.remove(path);
}

}  // namespace

void setup() {
  Serial.begin(115200);

  // Do not auto-format: this tool must remove only the event history,
  // never users or configuration.
  if (!LittleFS.begin(false)) {
    Serial.println("LittleFS not available: no file removed.");
    return;
  }

  bool logRemoved = removeIfPresent("/log.jsonl");
  bool tempRemoved = removeIfPresent("/log.jsonl.tmp");

  if (logRemoved && tempRemoved) {
    Serial.println("Event history removed.");
  } else {
    Serial.println("Failed to remove the event history.");
  }
}

void loop() {}
