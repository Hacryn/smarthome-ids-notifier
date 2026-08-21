#pragma once

#include <stdint.h>

#include <vector>

#include "../users/UserList.h"
#include "TelegramClient.h"

// Sec. 12 - Telegram command dispatcher. Not testable via the host-side
// harness (hardware-bound orchestration); parsing of individual commands
// (CommandParser) is tested separately.

// Registers the references to the system state needed to execute
// commands. Call once in setup(), before registering handleIncomingCommand
// as the CommandHandler.
void initCommandRouter(std::vector<AuthorizedUser>& users, uint32_t& lastWrittenTs);

// Sec. 4.2/12 - whitelist checked here (unauthorized senders are silently
// ignored); admin-only commands are explicitly rejected if the sender is
// authorized but not an admin. Signature compatible with CommandHandler,
// directly registerable.
void handleIncomingCommand(const IncomingCommand& cmd);
