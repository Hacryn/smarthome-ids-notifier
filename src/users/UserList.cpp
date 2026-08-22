#include "UserList.h"

#include <ArduinoJson.h>
#include <algorithm>

namespace {
std::vector<AuthorizedUser>::iterator findUser(std::vector<AuthorizedUser>& users, int64_t chatId) {
  return std::find_if(users.begin(), users.end(),
                       [chatId](const AuthorizedUser& u) { return u.chatId == chatId; });
}
}  // namespace

bool isAuthorized(const std::vector<AuthorizedUser>& users, int64_t chatId) {
  for (const auto& u : users) {
    if (u.chatId == chatId) return true;
  }
  return false;
}

bool isAdmin(const std::vector<AuthorizedUser>& users, int64_t chatId) {
  for (const auto& u : users) {
    if (u.chatId == chatId) return u.admin;
  }
  return false;
}

bool addUser(std::vector<AuthorizedUser>& users, int64_t chatId, bool admin, uint32_t addedTs) {
  if (isAuthorized(users, chatId)) return false;
  users.push_back({chatId, admin, addedTs, ""});
  return true;
}

bool removeUser(std::vector<AuthorizedUser>& users, int64_t chatId) {
  auto it = findUser(users, chatId);
  if (it == users.end()) return false;
  users.erase(it);
  return true;
}

bool setAdminFlag(std::vector<AuthorizedUser>& users, int64_t chatId, bool admin) {
  auto it = findUser(users, chatId);
  if (it == users.end()) return false;
  it->admin = admin;
  return true;
}

void resetUsers(std::vector<AuthorizedUser>& users) { users.clear(); }

bool updateUsername(std::vector<AuthorizedUser>& users, int64_t chatId,
                     const std::string& username) {
  auto it = findUser(users, chatId);
  if (it == users.end() || it->username == username) return false;
  it->username = username;
  return true;
}

bool ensureOnboardingAdmin(std::vector<AuthorizedUser>& users, int64_t onboardingChatId,
                            uint32_t nowTs) {
  if (!users.empty()) return false;
  users.push_back({onboardingChatId, true, nowTs, ""});
  return true;
}

std::string serializeUsers(const std::vector<AuthorizedUser>& users) {
  JsonDocument doc;
  JsonArray arr = doc["authorized"].to<JsonArray>();
  for (const auto& u : users) {
    JsonObject obj = arr.add<JsonObject>();
    obj["chat_id"] = u.chatId;
    obj["admin"] = u.admin;
    obj["added_ts"] = u.addedTs;
    obj["username"] = u.username;
  }

  std::string out;
  serializeJson(doc, out);
  return out;
}

bool parseUsers(const std::string& json, std::vector<AuthorizedUser>& out) {
  out.clear();

  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;

  JsonVariantConst authorized = doc["authorized"];
  if (!authorized.is<JsonArrayConst>()) return false;

  for (JsonVariantConst entry : authorized.as<JsonArrayConst>()) {
    JsonVariantConst chatId = entry["chat_id"];
    JsonVariantConst admin = entry["admin"];
    JsonVariantConst addedTs = entry["added_ts"];
    if (chatId.isNull() || admin.isNull() || addedTs.isNull()) return false;

    AuthorizedUser u;
    u.chatId = chatId.as<int64_t>();
    u.admin = admin.as<bool>();
    u.addedTs = addedTs.as<uint32_t>();
    JsonVariantConst username = entry["username"];
    u.username = username.isNull() ? "" : username.as<std::string>();
    out.push_back(u);
  }
  return true;
}
