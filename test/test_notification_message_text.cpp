#include <cassert>
#include <cstdio>

#include "../src/notifications/NotificationMessageText.h"

namespace {
// UTF-8 byte sequences, matching NotificationMessageText.cpp's markers.
const char* kAlarmEmoji = "\xF0\x9F\x9A\xA8";       // rocket-siren, ALARM_GENERAL
const char* kStartMarker = "\xE2\x96\xB6\xEF\xB8\x8F";
const char* kEndMarker = "\xE2\x9C\x85";
const char* kRecoveredMarker = "\xE2\x8F\xAA";  // rewind (must match NotificationMessageText.cpp)
}  // namespace

static void test_event_message_start_has_emoji_and_start_marker() {
  std::string text = buildEventMessageText(kAlarmEmoji, "Allarme generale", EventStatus::START,
                                            "2025-01-01T00:00:00Z");
  assert(text.find(kAlarmEmoji) != std::string::npos);
  assert(text.find(kStartMarker) != std::string::npos);
  assert(text.find(kEndMarker) == std::string::npos);
}

static void test_event_message_end_has_end_marker() {
  std::string text = buildEventMessageText(kAlarmEmoji, "Allarme generale", EventStatus::END,
                                            "2025-01-01T00:10:00Z");
  assert(text.find(kEndMarker) != std::string::npos);
  assert(text.find(kStartMarker) == std::string::npos);
}

static void test_event_message_instant_has_no_marker() {
  std::string text =
      buildEventMessageText(kAlarmEmoji, "Riavvio", EventStatus::INSTANT, "2025-01-01T00:00:00Z");
  assert(text.find(kStartMarker) == std::string::npos);
  assert(text.find(kEndMarker) == std::string::npos);
  assert(text.find(kAlarmEmoji) != std::string::npos);
}

static void test_recovery_message_has_emoji_and_recovered_prefix() {
  std::string text = buildRecoveryMessageText(kAlarmEmoji, "Allarme generale", EventStatus::START,
                                               "2025-01-01T00:00:00Z", /*isRecovered=*/true);
  assert(text.find(kRecoveredMarker) == 0);  // marker leads the text
  assert(text.find("[recuperata]") != std::string::npos);
  assert(text.find(kAlarmEmoji) != std::string::npos);
}

static void test_recovery_message_without_recovered_prefix() {
  std::string text = buildRecoveryMessageText(kAlarmEmoji, "Allarme generale", EventStatus::START,
                                               "2025-01-01T00:00:00Z", /*isRecovered=*/false);
  assert(text.find("[recuperata]") == std::string::npos);
  assert(text.find(kRecoveredMarker) == std::string::npos);
  assert(text.find(kAlarmEmoji) != std::string::npos);
}

static void test_recovery_message_has_recovered_emoji_marker() {
  std::string text = buildRecoveryMessageText(kAlarmEmoji, "Allarme generale", EventStatus::START,
                                               "2025-01-01T00:00:00Z", /*isRecovered=*/true);
  assert(text.find(kRecoveredMarker) != std::string::npos);
  assert(text.find("[recuperata]") != std::string::npos);
}

static void test_recovery_message_start_end_markers_present() {
  std::string startText = buildRecoveryMessageText(
      kAlarmEmoji, "Allarme generale", EventStatus::START, "2025-01-01T00:00:00Z", false);
  assert(startText.find(kStartMarker) != std::string::npos);
  assert(startText.find(kEndMarker) == std::string::npos);

  std::string endText = buildRecoveryMessageText(kAlarmEmoji, "Allarme generale", EventStatus::END,
                                                  "2025-01-01T00:10:00Z", false);
  assert(endText.find(kEndMarker) != std::string::npos);
  assert(endText.find(kStartMarker) == std::string::npos);
}

int main() {
  test_event_message_start_has_emoji_and_start_marker();
  test_event_message_end_has_end_marker();
  test_event_message_instant_has_no_marker();
  test_recovery_message_has_emoji_and_recovered_prefix();
  test_recovery_message_without_recovered_prefix();
  test_recovery_message_has_recovered_emoji_marker();
  test_recovery_message_start_end_markers_present();

  printf("test_notification_message_text: all tests passed\n");
  return 0;
}
