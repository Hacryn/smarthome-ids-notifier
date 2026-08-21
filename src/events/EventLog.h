#pragma once

#include <stdint.h>

#include <string>

#include "EventTypes.h"

struct EventRecord {
  char id[33];  // 32 caratteri esadecimali + terminatore (sez. 5.2)
  EventType type;
  EventStatus status;
  uint32_t ts;
  bool approx = false;  // campo "a" di sez. 5.2 (timestamp ricostruito dall'ancora NVS)
};

// Serializza secondo lo schema di sez. 5.2. Il campo "a" e' omesso se approx == false.
std::string serializeEventRecord(const EventRecord& rec);

// Ritorna false se la riga non e' JSON valido o manca un campo obbligatorio.
bool parseEventRecord(const std::string& line, EventRecord& out);
