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
