#pragma once

#include <vector>

#include "UserConfig.h"

// Sez. 4.4 - persistenza di userconfig.json su LittleFS, riscritto per
// intero ad ogni modifica (write-then-rename, sez. 9.3.2). Non testabile
// via harness host-side (dipende da LittleFS reale).

// Ritorna true anche se il file non esiste ancora (nessuna preferenza
// personalizzata non e' un errore); false solo su un errore di lettura reale.
bool loadAllUserConfigs(std::vector<UserConfig>& out);

bool saveAllUserConfigs(const std::vector<UserConfig>& configs);
