#pragma once

#include <stdint.h>

#include <vector>

#include "EventLog.h"

struct OpenEvent {
  char id[33];
  EventType type;
  uint32_t startTs;
  bool approx;
};

// Sez. 8 - dato uno stream di righe (nell'ordine del file), ritorna gli
// eventi con durata ancora privi di riga END (aperti). Le righe INSTANT
// (es. REBOOT) non aprono/chiudono mai nulla.
std::vector<OpenEvent> findOpenEvents(const std::vector<EventRecord>& rows);
