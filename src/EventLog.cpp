#include "EventLog.h"

#include <ArduinoJson.h>
#include <string.h>

std::string serializeEventRecord(const EventRecord& rec) {
  JsonDocument doc;
  doc["id"] = rec.id;
  doc["type"] = static_cast<uint8_t>(rec.type);
  doc["status"] = static_cast<uint8_t>(rec.status);
  doc["ts"] = rec.ts;
  if (rec.approx) doc["a"] = 1;

  std::string out;
  serializeJson(doc, out);
  return out;
}

bool parseEventRecord(const std::string& line, EventRecord& out) {
  JsonDocument doc;
  if (deserializeJson(doc, line)) return false;

  JsonVariantConst id = doc["id"];
  JsonVariantConst type = doc["type"];
  JsonVariantConst status = doc["status"];
  JsonVariantConst ts = doc["ts"];
  if (id.isNull() || type.isNull() || status.isNull() || ts.isNull()) return false;

  const char* idStr = id.as<const char*>();
  if (idStr == nullptr || strlen(idStr) != 32) return false;

  strncpy(out.id, idStr, sizeof(out.id) - 1);
  out.id[32] = '\0';
  out.type = static_cast<EventType>(type.as<uint8_t>());
  out.status = static_cast<EventStatus>(status.as<uint8_t>());
  out.ts = ts.as<uint32_t>();
  out.approx = doc["a"].as<int>() == 1;  // doc["a"] assente -> 0

  return true;
}
