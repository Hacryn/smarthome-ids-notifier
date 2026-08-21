#pragma once

#include <stdint.h>

#include <list>
#include <map>
#include <string>

#include "EventLog.h"

// Sez. 6.7/12.1 - formatta una durata in secondi come "Xm" o "Xh Ym".
std::string formatDurationSeconds(uint32_t seconds);

// Sez. 12.1 - un evento aggregato: le righe START/END con lo stesso id
// unite in una sola voce con la durata calcolabile dal chiamante.
struct AggregatedEvent {
  char id[33];
  EventType type;
  uint32_t startTs;
  bool startApprox;
  bool isInstant;  // sez. 3.2 - REBOOT e simili: nessun END atteso
  bool hasEnd;
  uint32_t endTs;
  bool endApprox;
};

// Sez. 12.1 - "il file viene letto una sola volta in streaming, mantenendo
// in RAM un ring buffer degli ultimi N eventi aggregati (non delle righe)".
// Alimentata riga per riga in ordine (observe()), non richiede mai l'intero
// file in memoria. Una riga END per un id gia' scartato dal ring (perche'
// piu' vecchio degli ultimi N) viene ignorata silenziosamente.
class AggregatedEventLog {
 public:
  explicit AggregatedEventLog(size_t maxEvents);

  void observe(const EventRecord& rec);

  // In ordine cronologico crescente (dal piu' vecchio al piu' recente tra
  // quelli conservati).
  const std::list<AggregatedEvent>& events() const { return ring_; }

 private:
  void pushNew(const EventRecord& rec, bool isInstant);

  size_t maxEvents_;
  std::list<AggregatedEvent> ring_;
  std::map<std::string, std::list<AggregatedEvent>::iterator> idToIter_;
};
