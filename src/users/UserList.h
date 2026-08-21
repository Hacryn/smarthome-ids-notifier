#pragma once

#include <stdint.h>

#include <string>
#include <vector>

// Sez. 4.2/4.4 - chat_id sempre a 64 bit con segno (gruppi/supergruppi
// superano i 32 bit).
struct AuthorizedUser {
  int64_t chatId;
  bool admin;
  uint32_t addedTs;
};

bool isAuthorized(const std::vector<AuthorizedUser>& users, int64_t chatId);

// false anche se il chat_id non e' in whitelist (non solo se non e' admin).
bool isAdmin(const std::vector<AuthorizedUser>& users, int64_t chatId);

// Sez. 4.5 - ritorna false se il chat_id e' gia' presente (nessun duplicato).
bool addUser(std::vector<AuthorizedUser>& users, int64_t chatId, bool admin, uint32_t addedTs);

// Ritorna false se il chat_id non era presente.
bool removeUser(std::vector<AuthorizedUser>& users, int64_t chatId);

// Promozione/rimozione del flag admin (sez. 4.5). Ritorna false se il
// chat_id non era presente.
bool setAdminFlag(std::vector<AuthorizedUser>& users, int64_t chatId, bool admin);

// Sez. 4.5 - reset completo, operazione distruttiva (la conferma e'
// responsabilita' del livello comandi, non ancora implementato).
void resetUsers(std::vector<AuthorizedUser>& users);

// Sez. 4.5 - onboarding: se la whitelist e' vuota, il chat_id iniziale
// (da secrets.h) diventa automaticamente il primo admin. Ritorna true se
// ha effettivamente agito (per decidere se persistere il cambiamento).
bool ensureOnboardingAdmin(std::vector<AuthorizedUser>& users, int64_t onboardingChatId,
                            uint32_t nowTs);

// Sez. 4.4 - schema users.json: {"authorized": [{"chat_id","admin","added_ts"}, ...]}.
std::string serializeUsers(const std::vector<AuthorizedUser>& users);
bool parseUsers(const std::string& json, std::vector<AuthorizedUser>& out);
