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
