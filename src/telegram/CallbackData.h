#pragma once

#include <string>

// Sec. 8.1 - "c:<id>" format (34 bytes, within the 64-byte limit Telegram
// imposes on callback_data).
std::string closeEventCallbackData(const char* id);

// Returns false if the prefix isn't "c:" or the id isn't 32 characters long.
bool parseCloseEventCallbackData(const std::string& data, std::string& outId);
