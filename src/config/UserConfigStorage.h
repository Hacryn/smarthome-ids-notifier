#pragma once

#include <vector>

#include "UserConfig.h"

// Sec. 4.4 - persistence of userconfig.json on LittleFS, rewritten in full
// on every change (write-then-rename, sec. 9.3.2). Not testable via the
// host-side harness (depends on real LittleFS).

// Returns true even if the file doesn't exist yet (no custom preferences
// isn't an error); false only on a real read error.
bool loadAllUserConfigs(std::vector<UserConfig>& out);

bool saveAllUserConfigs(const std::vector<UserConfig>& configs);
