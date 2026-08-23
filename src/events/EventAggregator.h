#pragma once

#include <stdint.h>

#include <list>
#include <map>
#include <string>

#include "EventLog.h"

// Sec. 6.7/12.1 - formats a duration in seconds as "Xm" or "Xh Ym".
std::string formatDurationSeconds(uint32_t seconds);

// Sec. 12.1 - an aggregated event: the START/END rows sharing the same id
// merged into a single entry with a duration the caller can compute.
struct AggregatedEvent {
  char id[33];
  EventType type;
  uint32_t startTs;
  bool startApprox;
  bool isInstant;  // sec. 3.2 - REBOOT and similar: no END expected
  bool hasEnd;
  uint32_t endTs;
  bool endApprox;
  // Sec. 3.4.3 - requester identity, carried straight through from the
  // underlying EventRecord (0/"" = not applicable). No pairing concern:
  // ARM_*/DISARM_* are always isInstant (single-row).
  int64_t chatId = 0;
  std::string username;
};

// Sec. 12.1 - "the file is read once, in streaming, keeping in RAM a ring
// buffer of the last N aggregated events (not of rows)". Fed one row at a
// time in order (observe()), never requires the whole file in memory. An
// END row for an id already evicted from the ring (because it's older than
// the last N) is silently ignored.
class AggregatedEventLog {
 public:
  explicit AggregatedEventLog(size_t maxEvents);

  void observe(const EventRecord& rec);

  // In increasing chronological order (from the oldest to the most recent
  // among those kept).
  const std::list<AggregatedEvent>& events() const { return ring_; }

 private:
  void pushNew(const EventRecord& rec, bool isInstant);

  size_t maxEvents_;
  std::list<AggregatedEvent> ring_;
  std::map<std::string, std::list<AggregatedEvent>::iterator> idToIter_;
};
