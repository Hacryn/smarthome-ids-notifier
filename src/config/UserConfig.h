#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../events/EventTypes.h"
#include "TimezonePresets.h"

// Sez. 11.2 - preferenze per singolo utente.
struct UserConfig {
  int64_t chatId = 0;
  std::string dateFormat = "%Y-%m-%dT%H:%M:%SZ";  // default ISO 8601
  TimezonePreset timezone = TimezonePreset::UTC;   // default UTC
  std::vector<EventType> disabledTypes;            // default: tutti abilitati (vuoto)
};

bool isNotifyEnabledForUser(const UserConfig& cfg, EventType type);
void setNotifyEnabled(UserConfig& cfg, EventType type, bool enabled);

// Sez. 4.4 - schema userconfig.json, indicizzato per chat_id (chiave stringa).
std::string serializeUserConfigs(const std::vector<UserConfig>& configs);
bool parseUserConfigs(const std::string& json, std::vector<UserConfig>& out);

// Ritorna la entry se presente, altrimenti una UserConfig di default per quel chat_id.
UserConfig findOrDefaultUserConfig(const std::vector<UserConfig>& configs, int64_t chatId);
