#pragma once

#include <stdint.h>

#include <string>
#include <vector>

// Sec. 4.2/4.4 - chat_id always a signed 64-bit value (groups/supergroups
// exceed 32 bits).
struct AuthorizedUser {
  int64_t chatId;
  bool admin;
  uint32_t addedTs;
};

bool isAuthorized(const std::vector<AuthorizedUser>& users, int64_t chatId);

// false also if the chat_id isn't on the whitelist (not just if it isn't admin).
bool isAdmin(const std::vector<AuthorizedUser>& users, int64_t chatId);

// Sec. 4.5 - returns false if the chat_id is already present (no duplicates).
bool addUser(std::vector<AuthorizedUser>& users, int64_t chatId, bool admin, uint32_t addedTs);

// Returns false if the chat_id wasn't present.
bool removeUser(std::vector<AuthorizedUser>& users, int64_t chatId);

// Promotes/revokes the admin flag (sec. 4.5). Returns false if the chat_id
// wasn't present.
bool setAdminFlag(std::vector<AuthorizedUser>& users, int64_t chatId, bool admin);

// Sec. 4.5 - full reset, a destructive operation (confirmation is the
// command layer's responsibility, not implemented here).
void resetUsers(std::vector<AuthorizedUser>& users);

// Sec. 4.5 - onboarding: if the whitelist is empty, the initial chat_id
// (from secrets.h) automatically becomes the first admin. Returns true if
// it actually acted (to decide whether to persist the change).
bool ensureOnboardingAdmin(std::vector<AuthorizedUser>& users, int64_t onboardingChatId,
                            uint32_t nowTs);

// Sec. 4.4 - users.json schema: {"authorized": [{"chat_id","admin","added_ts"}, ...]}.
std::string serializeUsers(const std::vector<AuthorizedUser>& users);
bool parseUsers(const std::string& json, std::vector<AuthorizedUser>& out);
