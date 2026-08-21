#pragma once

#include <array>
#include <stdint.h>
#include <vector>

#include "../events/EventLog.h"

constexpr size_t kMaxDeletableIdsPerPass = 256;  // sec. 9.3.1 - ~4 KB of RAM

// Sec. 9.3.1 - pass 1 of the rotation algorithm: collects (in 16-byte
// binary form) the ids of deletable events - those with an END/INSTANT
// row whose ts is earlier than the cutoff - up to a fixed cap. Designed to
// be fed one row at a time in streaming (observe()), never requiring the
// whole file in memory.
class DeletableIdCollector {
 public:
  explicit DeletableIdCollector(uint32_t cutoff, size_t cap = kMaxDeletableIdsPerPass);

  // Silently ignores the row if the cap has already been reached, if it
  // isn't a closing row (END/INSTANT), or if it's after the cutoff.
  void observe(const EventRecord& rec);

  bool capReached() const { return capReached_; }
  const std::vector<std::array<uint8_t, 16>>& ids() const { return ids_; }

  // Sec. 9.3.1 pass 2 - true if the (hex) id is among those collected.
  bool contains(const char* hexId) const;

 private:
  uint32_t cutoff_;
  size_t cap_;
  std::vector<std::array<uint8_t, 16>> ids_;
  bool capReached_ = false;
};
