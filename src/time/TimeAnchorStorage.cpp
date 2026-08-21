#include "TimeAnchorStorage.h"

#include <Preferences.h>

namespace {
const char* kNamespace = "notifier";
const char* kKeyLastEpoch = "last_epoch";
}  // namespace

uint32_t loadLastEpoch() {
  Preferences prefs;
  prefs.begin(kNamespace, true);
  uint32_t epoch = prefs.getULong(kKeyLastEpoch, 0);
  prefs.end();
  return epoch;
}

void saveLastEpoch(uint32_t epoch) {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putULong(kKeyLastEpoch, epoch);
  prefs.end();
}
