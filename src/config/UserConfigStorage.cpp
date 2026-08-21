#include "UserConfigStorage.h"

#include <LittleFS.h>

#include "../rotation/FsErrorCounter.h"

namespace {
const char* kPath = "/userconfig.json";
const char* kTmpPath = "/userconfig.json.tmp";

bool writeConfigsTmp(const std::string& json) {
  File f = LittleFS.open(kTmpPath, "w");
  if (!f) return false;

  size_t written = f.print(json.c_str());
  f.close();
  return written == json.size();
}
}  // namespace

bool loadAllUserConfigs(std::vector<UserConfig>& out) {
  out.clear();
  if (!LittleFS.exists(kPath)) return true;

  File f = LittleFS.open(kPath, "r");
  if (!f) return false;

  std::string content(f.size(), '\0');
  f.readBytes(&content[0], content.size());
  f.close();

  return parseUserConfigs(content, out);
}

bool saveAllUserConfigs(const std::vector<UserConfig>& configs) {
  std::string json = serializeUserConfigs(configs);

  bool written = writeConfigsTmp(json);
  if (!written) written = writeConfigsTmp(json);  // sec. 9.4 - a single retry
  if (!written) {
    fsErrorCounter().recordFailure();
    return false;
  }

  return LittleFS.rename(kTmpPath, kPath);  // sec. 9.3.2 - atomic rename
}
