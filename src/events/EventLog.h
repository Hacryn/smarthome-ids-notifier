#pragma once

#include <stdint.h>

#include <string>

#include "EventTypes.h"

struct EventRecord {
  char id[33];  // 32 hex characters + terminator (sec. 5.2)
  EventType type;
  EventStatus status;
  uint32_t ts;
  bool approx = false;  // "a" field from sec. 5.2 (timestamp reconstructed from the NVS anchor)
  // Sec. 3.4.3 - requester identity for a remote arm/disarm command, 0/""
  // (the defaults) meaning "not applicable" (real chat_ids are never 0).
  // username is a snapshot at request time, not re-resolved later.
  int64_t chatId = 0;
  std::string username;
};

// Serializes per the schema in sec. 5.2. The "a" field is omitted if approx == false.
std::string serializeEventRecord(const EventRecord& rec);

// Returns false if the row isn't valid JSON or a required field is missing.
bool parseEventRecord(const std::string& line, EventRecord& out);
