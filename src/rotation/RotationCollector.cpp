#include "RotationCollector.h"

#include "IdBinary.h"

DeletableIdCollector::DeletableIdCollector(uint32_t cutoff, size_t cap)
    : cutoff_(cutoff), cap_(cap) {}

void DeletableIdCollector::observe(const EventRecord& rec) {
  if (capReached_) return;
  if (rec.status != EventStatus::END && rec.status != EventStatus::INSTANT) return;
  if (rec.ts >= cutoff_) return;

  std::array<uint8_t, 16> bin{};
  if (!hexIdToBinary(rec.id, bin)) return;  // difensivo: id malformato, mai atteso

  ids_.push_back(bin);
  if (ids_.size() >= cap_) capReached_ = true;
}

bool DeletableIdCollector::contains(const char* hexId) const {
  std::array<uint8_t, 16> bin{};
  if (!hexIdToBinary(hexId, bin)) return false;

  for (const auto& id : ids_) {
    if (id == bin) return true;
  }
  return false;
}
