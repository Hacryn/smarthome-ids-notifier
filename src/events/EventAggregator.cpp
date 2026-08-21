#include "EventAggregator.h"

#include <string.h>

std::string formatDurationSeconds(uint32_t seconds) {
  uint32_t minutes = seconds / 60;
  uint32_t hours = minutes / 60;
  minutes %= 60;

  if (hours > 0) {
    return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
  }
  return std::to_string(minutes) + "m";
}

AggregatedEventLog::AggregatedEventLog(size_t maxEvents) : maxEvents_(maxEvents) {}

void AggregatedEventLog::observe(const EventRecord& rec) {
  if (rec.status == EventStatus::START) {
    pushNew(rec, /*isInstant=*/false);
  } else if (rec.status == EventStatus::INSTANT) {
    pushNew(rec, /*isInstant=*/true);
  } else {  // END
    auto it = idToIter_.find(std::string(rec.id));
    if (it == idToIter_.end()) return;  // START gia' scartato dal ring, ignorata

    it->second->hasEnd = true;
    it->second->endTs = rec.ts;
    it->second->endApprox = rec.approx;
  }
}

void AggregatedEventLog::pushNew(const EventRecord& rec, bool isInstant) {
  AggregatedEvent ev{};
  strncpy(ev.id, rec.id, sizeof(ev.id) - 1);
  ev.id[32] = '\0';
  ev.type = rec.type;
  ev.startTs = rec.ts;
  ev.startApprox = rec.approx;
  ev.isInstant = isInstant;
  ev.hasEnd = false;

  ring_.push_back(ev);
  idToIter_[std::string(rec.id)] = std::prev(ring_.end());

  if (ring_.size() > maxEvents_) {
    idToIter_.erase(std::string(ring_.front().id));
    ring_.pop_front();
  }
}
