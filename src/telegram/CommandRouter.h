#pragma once

#include <stdint.h>

#include <vector>

#include "../users/UserList.h"
#include "TelegramClient.h"

// Sez. 12 - dispatcher dei comandi Telegram. Non testabile via harness
// host-side (orchestrazione hardware-bound); il parsing dei singoli
// comandi (CommandParser) e' testato separatamente.

// Registra i riferimenti allo stato di sistema necessari all'esecuzione dei
// comandi. Da chiamare una sola volta in setup(), prima di registrare
// handleIncomingCommand come CommandHandler.
void initCommandRouter(std::vector<AuthorizedUser>& users, uint32_t& lastWrittenTs);

// Sez. 4.2/12 - whitelist verificata qui (i mittenti non autorizzati
// vengono ignorati silenziosamente); i comandi riservati agli admin
// vengono rifiutati esplicitamente se il mittente e' autorizzato ma non
// admin. Firma compatibile con CommandHandler, registrabile direttamente.
void handleIncomingCommand(const IncomingCommand& cmd);
