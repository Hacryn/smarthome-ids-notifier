#pragma once

#include <string>

#include "../events/EventTypes.h"

// Pure text-building for the two per-event notification message shapes
// (live and recovered). Extracted out of NotificationEngine so the emoji/
// marker formatting is host-testable (see test/test_notification_message_text.cpp).

// Sec. 6.1 - live notification text: "<emoji> <label> <start/end marker> (<ts>)".
std::string buildEventMessageText(const char* emoji, const char* label, EventStatus status,
                                   const std::string& formattedTs);

// Sec. 6.4 - recovered notification text, with the "[recuperata]" prefix
// when presented as a recovery.
std::string buildRecoveryMessageText(const char* emoji, const char* label,
                                      const std::string& formattedTs, bool isRecovered);
