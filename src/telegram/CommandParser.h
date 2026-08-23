#pragma once

#include <stdint.h>

#include <string>

// Sec. 8.1 - text fallback "/closeevent <id> [timestamp]" (admin-only,
// verified by the caller). Returns false if the text isn't the recognized
// command or the id isn't 32 hex characters long.
bool parseCloseEventCommand(const std::string& text, std::string& outId, bool& hasTimestamp,
                             uint32_t& outTimestamp);

// Sec. 11.1 - form shared by the admin commands "/command <unsigned int>"
// (/setretention, /setgraceperiod, /setretryinterval, /setmaxretries,
// /setnetthreshold, /setaggregatethreshold). Returns false if the command
// doesn't match or the argument isn't a valid integer.
bool parseSingleUintCommand(const std::string& text, const char* commandName, uint32_t& outValue);

// Sec. 4.5 - form shared by "/adduser <chat_id>", "/removeuser <chat_id>",
// "/promoteuser <chat_id>" - the argument is signed (groups have negative
// chat_ids, sec. 4.2).
bool parseSingleInt64Command(const std::string& text, const char* commandName, int64_t& outValue);

// Sec. 4.5 - "/resetusers CONFERMA": destructive operation protected by an
// explicit confirmation word in the same message (no multi-step stateful
// flow, to stay stateless).
bool parseResetUsersCommand(const std::string& text);

// Sec. 11.2 - "/notify <type> on|off". outEnabled valid only if it returns true.
bool parseNotifyCommand(const std::string& text, std::string& outTypeName, bool& outEnabled);

// Sec. 11.2 - "/setdateformat <format>": everything after the command
// (including any spaces) is the strftime format, taken verbatim.
bool parseSetDateFormatCommand(const std::string& text, std::string& outFormat);

// Sec. 11.2 - "/settimezone <preset>".
bool parseSetTimezoneCommand(const std::string& text, std::string& outPresetName);

// Sec. 12.1 - "/log [n]". outN valid only if it returns true; if the
// argument is absent hasArg is false and outN isn't set (the caller
// applies the default).
bool parseLogCommand(const std::string& text, bool& hasArg, uint32_t& outN);

// Sec. 12.3 - "/dump <target> [chat_id]". outTarget is one of "log",
// "notif", "userconfig", "users". outChatId/outHasChatId are only set (and
// only required in the command text) for "notif" and "userconfig" - "log"
// and "users" reject a trailing argument to avoid ambiguity.
bool parseDumpCommand(const std::string& text, std::string& outTarget, int64_t& outChatId,
                      bool& outHasChatId);

// Sec. 12.3 - "/resetlog CONFERMA": destructive, same CONFERMA pattern as
// /resetusers (no multi-step stateful flow).
bool parseResetLogCommand(const std::string& text);

// Sec. 12.3 - "/resetnotif <chat_id> CONFERMA".
bool parseResetNotifCommand(const std::string& text, int64_t& outChatId);

// Sec. 12.3 - "/resetuserconfig <chat_id> CONFERMA".
bool parseResetUserConfigCommand(const std::string& text, int64_t& outChatId);

// Sec. 3.4.3 - "/setalarm <GENERALE|INTERNO|GARAGE> <ON|OFF>". outZoneToken
// is the raw token, validated against ALARM_COMMANDS by the caller (not
// here, to keep this module free of the AlarmCommandTypes dependency, same
// separation CommandParser already keeps from EventTypes elsewhere).
bool parseSetAlarmCommand(const std::string& text, std::string& outZoneToken, bool& outArm);
