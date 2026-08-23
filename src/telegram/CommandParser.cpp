#include "CommandParser.h"

#include <sstream>

bool parseCloseEventCommand(const std::string& text, std::string& outId, bool& hasTimestamp,
                             uint32_t& outTimestamp) {
  std::istringstream iss(text);
  std::string command;
  iss >> command;
  if (command != "/closeevent") return false;

  std::string id;
  if (!(iss >> id) || id.length() != 32) return false;
  outId = id;

  hasTimestamp = false;
  std::string tsStr;
  if (iss >> tsStr) {
    if (tsStr.empty()) return true;
    for (char c : tsStr) {
      if (c < '0' || c > '9') return false;
    }
    outTimestamp = static_cast<uint32_t>(std::stoul(tsStr));
    hasTimestamp = true;
  }
  return true;
}

namespace {
bool isAllDigits(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
  }
  return true;
}

// Sec. 4.2 - chat_id is signed (groups have negative ids).
bool parseInt64Token(const std::string& value, int64_t& out) {
  if (value.empty()) return false;
  size_t start = (value[0] == '-') ? 1 : 0;
  if (start >= value.size() || !isAllDigits(value.substr(start))) return false;
  out = std::stoll(value);
  return true;
}
}  // namespace

bool parseSingleUintCommand(const std::string& text, const char* commandName, uint32_t& outValue) {
  std::istringstream iss(text);
  std::string command, value;
  iss >> command;
  if (command != commandName) return false;
  if (!(iss >> value) || !isAllDigits(value)) return false;

  outValue = static_cast<uint32_t>(std::stoul(value));
  return true;
}

bool parseSingleInt64Command(const std::string& text, const char* commandName, int64_t& outValue) {
  std::istringstream iss(text);
  std::string command, value;
  iss >> command;
  if (command != commandName) return false;
  if (!(iss >> value)) return false;

  size_t start = (value[0] == '-') ? 1 : 0;
  if (start >= value.size() || !isAllDigits(value.substr(start))) return false;

  outValue = std::stoll(value);
  return true;
}

bool parseResetUsersCommand(const std::string& text) { return text == "/resetusers CONFERMA"; }

bool parseNotifyCommand(const std::string& text, std::string& outTypeName, bool& outEnabled) {
  std::istringstream iss(text);
  std::string command, typeName, onOff;
  iss >> command;
  if (command != "/notify") return false;
  if (!(iss >> typeName) || !(iss >> onOff)) return false;

  if (onOff == "on") {
    outEnabled = true;
  } else if (onOff == "off") {
    outEnabled = false;
  } else {
    return false;
  }
  outTypeName = typeName;
  return true;
}

bool parseSetDateFormatCommand(const std::string& text, std::string& outFormat) {
  const std::string prefix = "/setdateformat ";
  if (text.rfind(prefix, 0) != 0) return false;

  std::string format = text.substr(prefix.size());
  if (format.empty()) return false;

  outFormat = format;
  return true;
}

bool parseSetTimezoneCommand(const std::string& text, std::string& outPresetName) {
  std::istringstream iss(text);
  std::string command, preset;
  iss >> command;
  if (command != "/settimezone") return false;
  if (!(iss >> preset)) return false;

  outPresetName = preset;
  return true;
}

bool parseLogCommand(const std::string& text, bool& hasArg, uint32_t& outN) {
  std::istringstream iss(text);
  std::string command, nStr;
  iss >> command;
  if (command != "/log") return false;

  hasArg = false;
  if (iss >> nStr) {
    if (!isAllDigits(nStr)) return false;
    outN = static_cast<uint32_t>(std::stoul(nStr));
    hasArg = true;
  }
  return true;
}

bool parseDumpCommand(const std::string& text, std::string& outTarget, int64_t& outChatId,
                      bool& outHasChatId) {
  std::istringstream iss(text);
  std::string command, target;
  iss >> command;
  if (command != "/dump") return false;
  if (!(iss >> target)) return false;

  bool needsChatId = (target == "notif" || target == "userconfig");
  bool isValidTarget = needsChatId || target == "log" || target == "users";
  if (!isValidTarget) return false;

  std::string chatIdStr;
  bool hasTrailing = static_cast<bool>(iss >> chatIdStr);

  if (needsChatId) {
    if (!hasTrailing || !parseInt64Token(chatIdStr, outChatId)) return false;
    outHasChatId = true;
  } else {
    if (hasTrailing) return false;  // no argument expected for log/users
    outHasChatId = false;
  }

  outTarget = target;
  return true;
}

bool parseResetLogCommand(const std::string& text) { return text == "/resetlog CONFERMA"; }

bool parseResetNotifCommand(const std::string& text, int64_t& outChatId) {
  std::istringstream iss(text);
  std::string command, chatIdStr, confirm;
  iss >> command;
  if (command != "/resetnotif") return false;
  if (!(iss >> chatIdStr) || !(iss >> confirm) || confirm != "CONFERMA") return false;

  return parseInt64Token(chatIdStr, outChatId);
}

bool parseResetUserConfigCommand(const std::string& text, int64_t& outChatId) {
  std::istringstream iss(text);
  std::string command, chatIdStr, confirm;
  iss >> command;
  if (command != "/resetuserconfig") return false;
  if (!(iss >> chatIdStr) || !(iss >> confirm) || confirm != "CONFERMA") return false;

  return parseInt64Token(chatIdStr, outChatId);
}

bool parseSetAlarmCommand(const std::string& text, std::string& outZoneToken, bool& outArm) {
  std::istringstream iss(text);
  std::string command, zoneToken, onOff;
  iss >> command;
  if (command != "/setalarm") return false;
  if (!(iss >> zoneToken) || !(iss >> onOff)) return false;

  if (onOff == "ON") {
    outArm = true;
  } else if (onOff == "OFF") {
    outArm = false;
  } else {
    return false;
  }
  outZoneToken = zoneToken;
  return true;
}
