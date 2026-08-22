#pragma once

#include <vector>

#include "UserList.h"

inline const char* kUsersPath = "/users.json";

// Sec. 4.4 - persistence of users.json on LittleFS, rewritten in full on
// every change with a write-then-rename pattern (sec. 9.3.2). Not testable
// via the host-side harness (depends on real LittleFS).

// Returns true even if the file doesn't exist yet (an empty whitelist on
// first boot isn't an error); false only on a real read/parse error.
bool loadUsers(std::vector<AuthorizedUser>& out);

bool saveUsers(const std::vector<AuthorizedUser>& users);
