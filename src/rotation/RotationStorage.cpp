#include "RotationStorage.h"

#include <Preferences.h>

namespace {
const char* kNamespace = "notifier";
const char* kKeyLastRotation = "last_rotation";
}  // namespace

uint32_t loadLastRotationEpoch() {
  Preferences prefs;
  prefs.begin(kNamespace, true);
  uint32_t epoch = prefs.getULong(kKeyLastRotation, 0);
  prefs.end();
  return epoch;
}

void saveLastRotationEpoch(uint32_t epoch) {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putULong(kKeyLastRotation, epoch);
  prefs.end();
}
