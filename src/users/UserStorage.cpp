#include "UserStorage.h"

#include <LittleFS.h>

#include "../rotation/FsErrorCounter.h"

namespace {
const char* kUsersTmpPath = "/users.json.tmp";

bool writeUsersTmp(const std::string& json) {
  File f = LittleFS.open(kUsersTmpPath, "w");
  if (!f) return false;

  size_t written = f.print(json.c_str());
  f.close();
  return written == json.size();
}
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
  std::string json = serializeUsers(users);

  bool written = writeUsersTmp(json);
  if (!written) written = writeUsersTmp(json);  // sec. 9.4 - a single retry
  if (!written) {
    fsErrorCounter().recordFailure();
    return false;
  }

  // Sec. 9.3.2 - atomic rename over the existing file, not a separate
  // remove()+rename() (which wouldn't be atomic).
  return LittleFS.rename(kUsersTmpPath, kUsersPath);
}
