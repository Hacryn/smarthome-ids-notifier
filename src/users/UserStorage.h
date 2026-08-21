#pragma once

#include <vector>

#include "UserList.h"

// Sez. 4.4 - persistenza di users.json su LittleFS, riscritto per intero ad
// ogni modifica con pattern write-then-rename (sez. 9.3.2). Non testabile
// via harness host-side (dipende da LittleFS reale).

// Ritorna true anche se il file non esiste ancora (whitelist vuota al primo
// avvio non e' un errore); false solo su un errore di lettura/parsing reale.
bool loadUsers(std::vector<AuthorizedUser>& out);

bool saveUsers(const std::vector<AuthorizedUser>& users);
