#include <cassert>
#include <cstdio>

#include "../src/telegram/CommandParser.h"

static void test_single_uint_command() {
  uint32_t value = 0;
  assert(parseSingleUintCommand("/setretention 52", "/setretention", value));
  assert(value == 52);

  assert(!parseSingleUintCommand("/setgraceperiod 5", "/setretention", value));  // comando diverso
  assert(!parseSingleUintCommand("/setretention -5", "/setretention", value));   // negativo non ammesso
  assert(!parseSingleUintCommand("/setretention abc", "/setretention", value));
  assert(!parseSingleUintCommand("/setretention", "/setretention", value));  // manca l'argomento
}

static void test_single_int64_command() {
  int64_t value = 0;
  assert(parseSingleInt64Command("/adduser 111111111", "/adduser", value));
  assert(value == 111111111LL);

  assert(parseSingleInt64Command("/adduser -1001234567890", "/adduser", value));  // gruppo
  assert(value == -1001234567890LL);

  assert(!parseSingleInt64Command("/adduser -", "/adduser", value));
  assert(!parseSingleInt64Command("/adduser abc", "/adduser", value));
}

static void test_reset_users_requires_confirmation_word() {
  assert(parseResetUsersCommand("/resetusers CONFERMA"));
  assert(!parseResetUsersCommand("/resetusers"));
  assert(!parseResetUsersCommand("/resetusers conferma"));  // case-sensitive
}

static void test_notify_command() {
  std::string type;
  bool enabled = false;
  assert(parseNotifyCommand("/notify ALARM_GARAGE off", type, enabled));
  assert(type == "ALARM_GARAGE");
  assert(!enabled);

  assert(parseNotifyCommand("/notify NETWORK_ISSUE on", type, enabled));
  assert(enabled);

  assert(!parseNotifyCommand("/notify ALARM_GARAGE maybe", type, enabled));
  assert(!parseNotifyCommand("/notify ALARM_GARAGE", type, enabled));
}

static void test_set_date_format_command() {
  std::string format;
  assert(parseSetDateFormatCommand("/setdateformat %d/%m/%Y %H:%M", format));
  assert(format == "%d/%m/%Y %H:%M");

  assert(!parseSetDateFormatCommand("/setdateformat ", format));  // vuoto dopo il comando
  assert(!parseSetDateFormatCommand("/settimezone UTC", format));
}

static void test_set_timezone_command() {
  std::string preset;
  assert(parseSetTimezoneCommand("/settimezone Europe/Rome", preset));
  assert(preset == "Europe/Rome");

  assert(!parseSetTimezoneCommand("/settimezone", preset));
}

static void test_log_command() {
  bool hasArg = true;
  uint32_t n = 0;
  assert(parseLogCommand("/log", hasArg, n));
  assert(!hasArg);

  assert(parseLogCommand("/log 20", hasArg, n));
  assert(hasArg);
  assert(n == 20);

  assert(!parseLogCommand("/log abc", hasArg, n));
  assert(!parseLogCommand("/status", hasArg, n));
}

int main() {
  test_single_uint_command();
  test_single_int64_command();
  test_reset_users_requires_confirmation_word();
  test_notify_command();
  test_set_date_format_command();
  test_set_timezone_command();
  test_log_command();

  printf("test_command_parser: tutti i test superati\n");
  return 0;
}
