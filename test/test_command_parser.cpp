#include <cassert>
#include <cstdio>

#include "../src/telegram/CommandParser.h"

static void test_single_uint_command() {
  uint32_t value = 0;
  assert(parseSingleUintCommand("/setretention 52", "/setretention", value));
  assert(value == 52);

  assert(!parseSingleUintCommand("/setgraceperiod 5", "/setretention", value));  // different command
  assert(!parseSingleUintCommand("/setretention -5", "/setretention", value));   // negative not allowed
  assert(!parseSingleUintCommand("/setretention abc", "/setretention", value));
  assert(!parseSingleUintCommand("/setretention", "/setretention", value));  // missing argument
}

static void test_single_int64_command() {
  int64_t value = 0;
  assert(parseSingleInt64Command("/adduser 111111111", "/adduser", value));
  assert(value == 111111111LL);

  assert(parseSingleInt64Command("/adduser -1001234567890", "/adduser", value));  // group
  assert(value == -1001234567890LL);

  assert(!parseSingleInt64Command("/adduser -", "/adduser", value));
  assert(!parseSingleInt64Command("/adduser abc", "/adduser", value));
}

static void test_reset_users_requires_confirmation_word() {
  assert(parseResetUsersCommand("/resetusers CONFERMA"));
  assert(!parseResetUsersCommand("/resetusers"));
  assert(!parseResetUsersCommand("/resetusers conferma"));  // case-sensitive (Italian word by design)
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

  assert(!parseSetDateFormatCommand("/setdateformat ", format));  // empty after the command
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

static void test_dump_command_log_and_users_take_no_argument() {
  std::string target;
  int64_t chatId = 0;
  bool hasChatId = true;

  assert(parseDumpCommand("/dump log", target, chatId, hasChatId));
  assert(target == "log");
  assert(!hasChatId);

  assert(parseDumpCommand("/dump users", target, chatId, hasChatId));
  assert(target == "users");
  assert(!hasChatId);

  assert(!parseDumpCommand("/dump log 111111111", target, chatId, hasChatId));  // unexpected arg
}

static void test_dump_command_notif_and_userconfig_require_chat_id() {
  std::string target;
  int64_t chatId = 0;
  bool hasChatId = false;

  assert(parseDumpCommand("/dump notif 111111111", target, chatId, hasChatId));
  assert(target == "notif");
  assert(hasChatId);
  assert(chatId == 111111111LL);

  assert(parseDumpCommand("/dump userconfig -1001234567890", target, chatId, hasChatId));
  assert(target == "userconfig");
  assert(chatId == -1001234567890LL);

  assert(!parseDumpCommand("/dump notif", target, chatId, hasChatId));       // missing chat_id
  assert(!parseDumpCommand("/dump notif abc", target, chatId, hasChatId));   // not a number
}

static void test_dump_command_rejects_unknown_target() {
  std::string target;
  int64_t chatId = 0;
  bool hasChatId = false;
  assert(!parseDumpCommand("/dump nonexistent", target, chatId, hasChatId));
  assert(!parseDumpCommand("/status", target, chatId, hasChatId));
}

static void test_reset_log_requires_confirmation_word() {
  assert(parseResetLogCommand("/resetlog CONFERMA"));
  assert(!parseResetLogCommand("/resetlog"));
  assert(!parseResetLogCommand("/resetlog conferma"));
}

static void test_reset_notif_requires_chat_id_and_confirmation() {
  int64_t chatId = 0;
  assert(parseResetNotifCommand("/resetnotif 111111111 CONFERMA", chatId));
  assert(chatId == 111111111LL);

  assert(!parseResetNotifCommand("/resetnotif 111111111", chatId));            // missing CONFERMA
  assert(!parseResetNotifCommand("/resetnotif abc CONFERMA", chatId));         // not a number
  assert(!parseResetNotifCommand("/resetnotif 111111111 conferma", chatId));   // case-sensitive
}

static void test_reset_userconfig_requires_chat_id_and_confirmation() {
  int64_t chatId = 0;
  assert(parseResetUserConfigCommand("/resetuserconfig 111111111 CONFERMA", chatId));
  assert(chatId == 111111111LL);

  assert(!parseResetUserConfigCommand("/resetuserconfig 111111111", chatId));
  assert(!parseResetUserConfigCommand("/resetuserconfig CONFERMA", chatId));
}

static void test_set_alarm_command() {
  std::string zone;
  bool arm = false;
  assert(parseSetAlarmCommand("/setalarm GENERALE ON", zone, arm));
  assert(zone == "GENERALE");
  assert(arm);

  assert(parseSetAlarmCommand("/setalarm GARAGE OFF", zone, arm));
  assert(zone == "GARAGE");
  assert(!arm);

  assert(!parseSetAlarmCommand("/setalarm GENERALE on", zone, arm));  // case-sensitive
  assert(!parseSetAlarmCommand("/setalarm GENERALE MAYBE", zone, arm));
  assert(!parseSetAlarmCommand("/setalarm GENERALE", zone, arm));  // missing on/off
  assert(!parseSetAlarmCommand("/setalarm", zone, arm));            // missing everything
  assert(!parseSetAlarmCommand("/status", zone, arm));
}

int main() {
  test_single_uint_command();
  test_single_int64_command();
  test_reset_users_requires_confirmation_word();
  test_notify_command();
  test_set_date_format_command();
  test_set_timezone_command();
  test_log_command();
  test_dump_command_log_and_users_take_no_argument();
  test_dump_command_notif_and_userconfig_require_chat_id();
  test_dump_command_rejects_unknown_target();
  test_reset_log_requires_confirmation_word();
  test_reset_notif_requires_chat_id_and_confirmation();
  test_reset_userconfig_requires_chat_id_and_confirmation();
  test_set_alarm_command();

  printf("test_command_parser: all tests passed\n");
  return 0;
}
