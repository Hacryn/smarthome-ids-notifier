#include <cassert>
#include <cstdio>
#include <cstring>

#include "../src/notifications/NotificationFolder.h"
#include "../src/notifications/NotificationRecord.h"

static const char* kId = "a1b2c3d4e5f60718293a4b5c6d7e8f90";

static void test_status_mapping_roundtrip() {
  assert(eventStatusToNotifyStatus(EventStatus::START) == NotifyStatus::NOTIFIED_START);
  assert(eventStatusToNotifyStatus(EventStatus::END) == NotifyStatus::NOTIFIED_END);
  assert(eventStatusToNotifyStatus(EventStatus::INSTANT) == NotifyStatus::NOTIFIED_INSTANT);

  assert(notifyStatusToEventStatus(NotifyStatus::NOTIFIED_START) == EventStatus::START);
  assert(notifyStatusToEventStatus(NotifyStatus::NOTIFIED_END) == EventStatus::END);
  assert(notifyStatusToEventStatus(NotifyStatus::NOTIFIED_INSTANT) == EventStatus::INSTANT);
}

static void test_notification_log_path_positive_and_negative() {
  assert(notificationLogPath(111111111LL) == "/notif_111111111.jsonl");
  assert(notificationLogPath(-1001234567890LL) == "/notif_g1001234567890.jsonl");  // sez. 4.2
}

static void test_serialize_omits_n_on_resolved() {
  NotificationRecord rec{};
  strcpy(rec.id, kId);
  rec.status = NotifyStatus::NOTIFIED_START;
  rec.ts = 1755500000;
  rec.state = NotifyState::RESOLVED;
  rec.n = 5;  // non deve comparire

  std::string json = serializeNotificationRecord(rec);
  assert(json.find("\"n\"") == std::string::npos);
}

static void test_serialize_includes_n_on_pending_and_abandoned() {
  NotificationRecord pending{};
  strcpy(pending.id, kId);
  pending.status = NotifyStatus::NOTIFIED_END;
  pending.ts = 1755500000;
  pending.state = NotifyState::PENDING;
  pending.n = 3;
  assert(serializeNotificationRecord(pending).find("\"n\":3") != std::string::npos);

  NotificationRecord abandoned = pending;
  abandoned.state = NotifyState::ABANDONED;
  abandoned.n = 25;
  assert(serializeNotificationRecord(abandoned).find("\"n\":25") != std::string::npos);
}

static void test_roundtrip() {
  NotificationRecord original{};
  strcpy(original.id, kId);
  original.status = NotifyStatus::NOTIFIED_START;
  original.ts = 1755500000;
  original.state = NotifyState::PENDING;
  original.n = 1;

  NotificationRecord parsed{};
  assert(parseNotificationRecord(serializeNotificationRecord(original), parsed));
  assert(strcmp(parsed.id, original.id) == 0);
  assert(parsed.status == original.status);
  assert(parsed.ts == original.ts);
  assert(parsed.state == original.state);
  assert(parsed.n == original.n);
}

static void test_parse_missing_n_defaults_to_zero() {
  NotificationRecord out{};
  std::string line =
      "{\"id\":\"" + std::string(kId) + "\",\"status\":2,\"ts\":1755500910,\"state\":1}";
  assert(parseNotificationRecord(line, out));
  assert(out.n == 0);
}

static void test_fold_last_row_wins() {
  std::vector<NotificationRecord> rows;
  NotificationRecord r1{};
  strcpy(r1.id, kId);
  r1.status = NotifyStatus::NOTIFIED_START;
  r1.ts = 1000;
  r1.state = NotifyState::PENDING;
  r1.n = 1;
  rows.push_back(r1);

  NotificationRecord r2 = r1;
  r2.state = NotifyState::RESOLVED;
  r2.ts = 2000;
  rows.push_back(r2);

  auto latest = foldNotificationRecords(rows);
  assert(latest.size() == 1);
  auto it = latest.find(notificationKey(kId, NotifyStatus::NOTIFIED_START));
  assert(it != latest.end());
  assert(it->second.state == NotifyState::RESOLVED);
  assert(it->second.ts == 2000);
}

static void test_fold_distinguishes_status_within_same_id() {
  std::vector<NotificationRecord> rows;
  NotificationRecord start{};
  strcpy(start.id, kId);
  start.status = NotifyStatus::NOTIFIED_START;
  start.ts = 1000;
  start.state = NotifyState::PENDING;
  start.n = 1;

  NotificationRecord end = start;
  end.status = NotifyStatus::NOTIFIED_END;

  rows.push_back(start);
  rows.push_back(end);

  auto latest = foldNotificationRecords(rows);
  assert(latest.size() == 2);  // stesso id, status diversi -> due voci
}

static void test_pending_from_filters_by_state() {
  std::vector<NotificationRecord> rows;
  NotificationRecord pending{};
  strcpy(pending.id, kId);
  pending.status = NotifyStatus::NOTIFIED_START;
  pending.state = NotifyState::PENDING;

  NotificationRecord resolved{};
  strcpy(resolved.id, kId);
  resolved.status = NotifyStatus::NOTIFIED_END;
  resolved.state = NotifyState::RESOLVED;

  rows.push_back(pending);
  rows.push_back(resolved);

  auto latest = foldNotificationRecords(rows);
  auto pendingList = pendingFrom(latest);
  assert(pendingList.size() == 1);
  assert(pendingList[0].status == NotifyStatus::NOTIFIED_START);
}

int main() {
  test_status_mapping_roundtrip();
  test_notification_log_path_positive_and_negative();
  test_serialize_omits_n_on_resolved();
  test_serialize_includes_n_on_pending_and_abandoned();
  test_roundtrip();
  test_parse_missing_n_defaults_to_zero();
  test_fold_last_row_wins();
  test_fold_distinguishes_status_within_same_id();
  test_pending_from_filters_by_state();

  printf("test_notification_record: tutti i test superati\n");
  return 0;
}
