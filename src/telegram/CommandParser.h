#pragma once

#include <stdint.h>

#include <string>

// Sez. 8.1 - fallback testuale "/closeevent <id> [timestamp]" (riservato
// agli admin, verificato dal chiamante). Ritorna false se il testo non e'
// il comando riconosciuto o l'id non e' lungo 32 caratteri esadecimali.
bool parseCloseEventCommand(const std::string& text, std::string& outId, bool& hasTimestamp,
                             uint32_t& outTimestamp);

// Sez. 11.1 - forma comune ai comandi admin "/comando <intero senza segno>"
// (/setretention, /setgraceperiod, /setretryinterval, /setmaxretries,
// /setnetthreshold, /setaggregatethreshold). Ritorna false se il comando
// non corrisponde o l'argomento non e' un intero valido.
bool parseSingleUintCommand(const std::string& text, const char* commandName, uint32_t& outValue);

// Sez. 4.5 - forma comune a "/adduser <chat_id>", "/removeuser <chat_id>",
// "/promoteuser <chat_id>" - l'argomento e' con segno (i gruppi hanno
// chat_id negativi, sez. 4.2).
bool parseSingleInt64Command(const std::string& text, const char* commandName, int64_t& outValue);

// Sez. 4.5 - "/resetusers CONFERMA": operazione distruttiva protetta da una
// parola di conferma esplicita nello stesso messaggio (nessun flusso a piu'
// passaggi con stato, per restare stateless).
bool parseResetUsersCommand(const std::string& text);

// Sez. 11.2 - "/notify <tipo> on|off". outEnabled valido solo se ritorna true.
bool parseNotifyCommand(const std::string& text, std::string& outTypeName, bool& outEnabled);

// Sez. 11.2 - "/setdateformat <formato>": tutto cio' che segue il comando
// (compresi eventuali spazi) e' il formato strftime, preso alla lettera.
bool parseSetDateFormatCommand(const std::string& text, std::string& outFormat);

// Sez. 11.2 - "/settimezone <preset>".
bool parseSetTimezoneCommand(const std::string& text, std::string& outPresetName);

// Sez. 12.1 - "/log [n]". outN valido solo se ritorna true; se l'argomento
// e' assente hasArg e' false e outN non e' valorizzato (il chiamante
// applica il default).
bool parseLogCommand(const std::string& text, bool& hasArg, uint32_t& outN);
