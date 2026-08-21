#include "CallbackData.h"

std::string closeEventCallbackData(const char* id) {
  std::string s = "c:";
  s += id;
  return s;
}

bool parseCloseEventCallbackData(const std::string& data, std::string& outId) {
  if (data.rfind("c:", 0) != 0) return false;

  std::string id = data.substr(2);
  if (id.length() != 32) return false;

  outId = id;
  return true;
}
