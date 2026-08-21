#include "UserStorage.h"

#include <LittleFS.h>

namespace {
const char* kUsersPath = "/users.json";
const char* kUsersTmpPath = "/users.json.tmp";
}  // namespace

bool loadUsers(std::vector<AuthorizedUser>& out) {
  out.clear();
  if (!LittleFS.exists(kUsersPath)) return true;

  File f = LittleFS.open(kUsersPath, "r");
  if (!f) return false;

  std::string content(f.size(), '\0');
  f.readBytes(&content[0], content.size());
  f.close();

  return parseUsers(content, out);
}

bool saveUsers(const std::vector<AuthorizedUser>& users) {
  File f = LittleFS.open(kUsersTmpPath, "w");
  if (!f) return false;

  std::string json = serializeUsers(users);
  size_t written = f.print(json.c_str());
  f.close();
  if (written != json.size()) return false;

  // Sez. 9.3.2 - rinomina atomica sopra il file esistente, non un
  // remove()+rename() separati (che non sarebbero atomici).
  return LittleFS.rename(kUsersTmpPath, kUsersPath);
}
