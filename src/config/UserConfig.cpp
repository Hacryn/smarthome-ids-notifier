#include "UserConfig.h"

#include <ArduinoJson.h>

#include <algorithm>

bool isNotifyEnabledForUser(const UserConfig& cfg, EventType type) {
  return std::find(cfg.disabledTypes.begin(), cfg.disabledTypes.end(), type) ==
         cfg.disabledTypes.end();
}

void setNotifyEnabled(UserConfig& cfg, EventType type, bool enabled) {
  auto it = std::find(cfg.disabledTypes.begin(), cfg.disabledTypes.end(), type);
  bool currentlyDisabled = it != cfg.disabledTypes.end();

  if (enabled && currentlyDisabled) {
    cfg.disabledTypes.erase(it);
  } else if (!enabled && !currentlyDisabled) {
    cfg.disabledTypes.push_back(type);
  }
}

std::string serializeUserConfigs(const std::vector<UserConfig>& configs) {
  JsonDocument doc;
  for (const auto& cfg : configs) {
    JsonObject obj = doc[std::to_string(cfg.chatId)].to<JsonObject>();
    obj["date_format"] = cfg.dateFormat;
    obj["timezone"] = static_cast<uint8_t>(cfg.timezone);

    JsonArray disabled = obj["notify_disabled"].to<JsonArray>();
    for (EventType t : cfg.disabledTypes) disabled.add(static_cast<uint8_t>(t));
  }

  std::string out;
  serializeJson(doc, out);
  return out;
}

bool parseUserConfigs(const std::string& json, std::vector<UserConfig>& out) {
  out.clear();
  if (json.empty()) return true;  // missing/empty file -> no config, not an error

  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;

  JsonObjectConst root = doc.as<JsonObjectConst>();
  for (JsonPairConst kv : root) {
    UserConfig cfg;
    cfg.chatId = std::stoll(kv.key().c_str());

    JsonObjectConst obj = kv.value().as<JsonObjectConst>();

    JsonVariantConst df = obj["date_format"];
    if (!df.isNull()) cfg.dateFormat = df.as<const char*>();

    JsonVariantConst tz = obj["timezone"];
    if (!tz.isNull()) cfg.timezone = static_cast<TimezonePreset>(tz.as<uint8_t>());

    JsonVariantConst disabled = obj["notify_disabled"];
    if (disabled.is<JsonArrayConst>()) {
      for (JsonVariantConst v : disabled.as<JsonArrayConst>()) {
        cfg.disabledTypes.push_back(static_cast<EventType>(v.as<uint8_t>()));
      }
    }

    out.push_back(cfg);
  }
  return true;
}

UserConfig findOrDefaultUserConfig(const std::vector<UserConfig>& configs, int64_t chatId) {
  for (const auto& cfg : configs) {
    if (cfg.chatId == chatId) return cfg;
  }
  UserConfig def;
  def.chatId = chatId;
  return def;
}
