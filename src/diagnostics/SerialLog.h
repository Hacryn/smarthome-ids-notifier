#pragma once

// Hardware-bound (depends on Serial); not testable via the host-side
// harness. Always active - the cost is a few Serial.printf calls, USB-only.
// Prefix format: "[LOG|WARN|ERR] <millis()>ms: <message>".

void logInfo(const char* fmt, ...);
void logWarn(const char* fmt, ...);
void logErr(const char* fmt, ...);
