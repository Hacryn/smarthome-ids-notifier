#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../events/EventTypes.h"
#include "TimezonePresets.h"

// Sec. 11.2 - per-user preferences.
struct UserConfig {
  int64_t chatId = 0;
  std::string dateFormat = "%Y-%m-%dT%H:%M:%SZ";  // default ISO 8601
  TimezonePreset timezone = TimezonePreset::UTC;   // default UTC
  std::vector<EventType> disabledTypes;            // default: all enabled (empty)
};

bool isNotifyEnabledForUser(const UserConfig& cfg, EventType type);
void setNotifyEnabled(UserConfig& cfg, EventType type, bool enabled);

// Sec. 4.4 - userconfig.json schema, indexed by chat_id (string key).
std::string serializeUserConfigs(const std::vector<UserConfig>& configs);
bool parseUserConfigs(const std::string& json, std::vector<UserConfig>& out);

// Returns the entry if present, otherwise a default UserConfig for that chat_id.
UserConfig findOrDefaultUserConfig(const std::vector<UserConfig>& configs, int64_t chatId);
