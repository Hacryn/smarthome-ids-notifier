#include "SerialLog.h"

#include <Arduino.h>

#include <cstdarg>
#include <cstdio>

namespace {

void logWithPrefix(const char* level, const char* fmt, va_list args) {
  char message[192];
  vsnprintf(message, sizeof(message), fmt, args);
  Serial.printf("[%s] %lums: %s\n", level, static_cast<unsigned long>(millis()), message);
}

}  // namespace

void logInfo(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  logWithPrefix("LOG", fmt, args);
  va_end(args);
}

void logWarn(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  logWithPrefix("WARN", fmt, args);
  va_end(args);
}

void logErr(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  logWithPrefix("ERR", fmt, args);
  va_end(args);
}
