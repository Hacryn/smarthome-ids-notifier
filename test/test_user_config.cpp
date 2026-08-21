#include <cassert>
#include <cstdio>

#include "../src/config/UserConfig.h"

static void test_default_config_has_all_types_enabled() {
  UserConfig cfg;
  assert(isNotifyEnabledForUser(cfg, EventType::ALARM_GARAGE));
  assert(isNotifyEnabledForUser(cfg, EventType::NETWORK_ISSUE));
}

static void test_set_notify_disabled_then_enabled() {
  UserConfig cfg;
  setNotifyEnabled(cfg, EventType::ALARM_GARAGE, false);
  assert(!isNotifyEnabledForUser(cfg, EventType::ALARM_GARAGE));
  assert(isNotifyEnabledForUser(cfg, EventType::ALARM_INTERNAL));  // altri tipi non toccati

  setNotifyEnabled(cfg, EventType::ALARM_GARAGE, true);
  assert(isNotifyEnabledForUser(cfg, EventType::ALARM_GARAGE));
}

static void test_set_notify_is_idempotent() {
  UserConfig cfg;
  setNotifyEnabled(cfg, EventType::ALARM_GARAGE, false);
  setNotifyEnabled(cfg, EventType::ALARM_GARAGE, false);  // gia' disabilitato
  assert(cfg.disabledTypes.size() == 1);
}

static void test_find_or_default_returns_default_when_missing() {
  std::vector<UserConfig> configs;
  UserConfig cfg = findOrDefaultUserConfig(configs, 111111111LL);
  assert(cfg.chatId == 111111111LL);
  assert(cfg.timezone == TimezonePreset::UTC);
  assert(cfg.dateFormat == "%Y-%m-%dT%H:%M:%SZ");
}

static void test_serialize_roundtrip() {
  UserConfig a;
  a.chatId = 111111111LL;
  a.dateFormat = "%d/%m/%Y %H:%M";
  a.timezone = TimezonePreset::EUROPE_ROME;
  setNotifyEnabled(a, EventType::NETWORK_ISSUE, false);

  UserConfig b;
  b.chatId = -1001234567890LL;  // sez. 4.2 - id di gruppo, negativo

  std::string json = serializeUserConfigs({a, b});

  std::vector<UserConfig> parsed;
  assert(parseUserConfigs(json, parsed));
  assert(parsed.size() == 2);

  UserConfig parsedA = findOrDefaultUserConfig(parsed, 111111111LL);
  assert(parsedA.dateFormat == "%d/%m/%Y %H:%M");
  assert(parsedA.timezone == TimezonePreset::EUROPE_ROME);
  assert(!isNotifyEnabledForUser(parsedA, EventType::NETWORK_ISSUE));
  assert(isNotifyEnabledForUser(parsedA, EventType::ALARM_GARAGE));

  UserConfig parsedB = findOrDefaultUserConfig(parsed, -1001234567890LL);
  assert(parsedB.timezone == TimezonePreset::UTC);  // default, mai impostato
}

static void test_parse_empty_json_is_no_configs() {
  std::vector<UserConfig> out;
  assert(parseUserConfigs("", out));
  assert(out.empty());
}

static void test_parse_rejects_malformed_json() {
  std::vector<UserConfig> out;
  assert(!parseUserConfigs("not json", out));
}

int main() {
  test_default_config_has_all_types_enabled();
  test_set_notify_disabled_then_enabled();
  test_set_notify_is_idempotent();
  test_find_or_default_returns_default_when_missing();
  test_serialize_roundtrip();
  test_parse_empty_json_is_no_configs();
  test_parse_rejects_malformed_json();

  printf("test_user_config: tutti i test superati\n");
  return 0;
}
