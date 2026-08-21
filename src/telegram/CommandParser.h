#pragma once

#include <stdint.h>

#include <string>

// Sez. 8.1 - fallback testuale "/closeevent <id> [timestamp]" (riservato
// agli admin, verificato dal chiamante). Ritorna false se il testo non e'
// il comando riconosciuto o l'id non e' lungo 32 caratteri esadecimali.
bool parseCloseEventCommand(const std::string& text, std::string& outId, bool& hasTimestamp,
                             uint32_t& outTimestamp);
