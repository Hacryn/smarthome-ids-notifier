#include <cassert>
#include <cstdio>
#include <cstring>

#include "../src/events/EventLog.h"
#include "../src/events/EventTypes.h"

static const char* kSampleId = "a1b2c3d4e5f60718293a4b5c6d7e8f90";

static void test_serialize_omits_a_when_not_approx() {
  EventRecord rec{};
  strcpy(rec.id, kSampleId);
  rec.type = EventType::REBOOT;
  rec.status = EventStatus::INSTANT;
  rec.ts = 1755500000;
  rec.approx = false;

  std::string json = serializeEventRecord(rec);
  assert(json.find("\"a\"") == std::string::npos);
  assert(json.find("\"ts\":1755500000") != std::string::npos);
}

static void test_serialize_includes_a_when_approx() {
  EventRecord rec{};
  strcpy(rec.id, kSampleId);
  rec.type = EventType::ALARM_GARAGE;
  rec.status = EventStatus::START;
  rec.ts = 1755500000;
  rec.approx = true;

  std::string json = serializeEventRecord(rec);
  assert(json.find("\"a\":1") != std::string::npos);
}

static void test_roundtrip() {
  EventRecord original{};
  strcpy(original.id, kSampleId);
  original.type = EventType::ALARM_GENERAL;
  original.status = EventStatus::END;
  original.ts = 1755500910;
  original.approx = false;

  std::string json = serializeEventRecord(original);

  EventRecord parsed{};
  bool ok = parseEventRecord(json, parsed);
  assert(ok);
  assert(strcmp(parsed.id, original.id) == 0);
  assert(parsed.type == original.type);
  assert(parsed.status == original.status);
  assert(parsed.ts == original.ts);
  assert(parsed.approx == original.approx);
}

static void test_parse_rejects_malformed_json() {
  EventRecord out{};
  assert(!parseEventRecord("not a json line", out));
}

static void test_parse_rejects_missing_field() {
  EventRecord out{};
  std::string line = "{\"id\":\"" + std::string(kSampleId) + "\",\"type\":0,\"status\":2}";
  assert(!parseEventRecord(line, out));  // manca "ts"
}

static void test_parse_rejects_wrong_id_length() {
  EventRecord out{};
  std::string line = "{\"id\":\"tooshort\",\"type\":0,\"status\":2,\"ts\":1755500000}";
  assert(!parseEventRecord(line, out));
}

static void test_event_type_table_lookup() {
  const EventTypeConfig* cfg = findEventTypeConfig(EventType::NETWORK_ISSUE);
  assert(cfg != nullptr);
  assert(cfg->notify_policy == NotifyPolicy::ONLY_END);
  assert(cfg->pin == -1);

  const EventTypeConfig* rebootCfg = findEventTypeConfig(EventType::REBOOT);
  assert(rebootCfg != nullptr);
  assert(rebootCfg->notify_policy == NotifyPolicy::INSTANT);
}

int main() {
  test_serialize_omits_a_when_not_approx();
  test_serialize_includes_a_when_approx();
  test_roundtrip();
  test_parse_rejects_malformed_json();
  test_parse_rejects_missing_field();
  test_parse_rejects_wrong_id_length();
  test_event_type_table_lookup();

  printf("test_event_log: tutti i test superati\n");
  return 0;
}
