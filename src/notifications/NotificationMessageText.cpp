#include "NotificationMessageText.h"

namespace {
// U+25B6 U+FE0F (start) / U+2705 (end) - see plan point 2.
constexpr const char* kStartMarker = "\xE2\x96\xB6\xEF\xB8\x8F";
constexpr const char* kEndMarker = "\xE2\x9C\x85";
// U+23EA (rewind) - recovered-notification marker.
constexpr const char* kRecoveredMarker = "\xE2\x8F\xAA";
}  // namespace

std::string buildEventMessageText(const char* emoji, const char* label, EventStatus status,
                                   const std::string& formattedTs) {
  std::string text = std::string(emoji) + " " + label;
  if (status == EventStatus::START) {
    text += " ";
    text += kStartMarker;
  } else if (status == EventStatus::END) {
    text += " ";
    text += kEndMarker;
  }
  text += " (" + formattedTs + ")";
  return text;
}

std::string buildRecoveryMessageText(const char* emoji, const char* label, EventStatus status,
                                      const std::string& formattedTs, bool isRecovered) {
  std::string text;
  if (isRecovered) text += std::string(kRecoveredMarker) + " [recuperata] ";  // sec. 6.4
  text += buildEventMessageText(emoji, label, status, formattedTs);
  return text;
}
