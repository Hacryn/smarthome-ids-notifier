#include <cassert>
#include <cstdio>
#include <cstring>

#include "../src/events/OpenEventsTracker.h"

static void setRec(EventRecord& rec, const char* id, EventType type, EventStatus status,
                    uint32_t ts, bool approx = false) {
  strcpy(rec.id, id);
  rec.type = type;
  rec.status = status;
  rec.ts = ts;
  rec.approx = approx;
}

static void test_start_without_end_is_open() {
  std::vector<EventRecord> rows(1);
  setRec(rows[0], "a1b2c3d4e5f60718293a4b5c6d7e8f90", EventType::ALARM_GARAGE, EventStatus::START,
         1000, true);

  auto open = findOpenEvents(rows);
  assert(open.size() == 1);
  assert(strcmp(open[0].id, "a1b2c3d4e5f60718293a4b5c6d7e8f90") == 0);
  assert(open[0].type == EventType::ALARM_GARAGE);
  assert(open[0].startTs == 1000);
  assert(open[0].approx);
}

static void test_start_with_end_is_not_open() {
  std::vector<EventRecord> rows(2);
  setRec(rows[0], "a1b2c3d4e5f60718293a4b5c6d7e8f90", EventType::ALARM_GARAGE, EventStatus::START,
         1000);
  setRec(rows[1], "a1b2c3d4e5f60718293a4b5c6d7e8f90", EventType::ALARM_GARAGE, EventStatus::END,
         1300);

  assert(findOpenEvents(rows).empty());
}

static void test_multiple_ids_tracked_independently() {
  std::vector<EventRecord> rows(3);
  setRec(rows[0], "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", EventType::ALARM_GARAGE, EventStatus::START,
         1000);
  setRec(rows[1], "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", EventType::POWER_LOSS, EventStatus::START,
         1100);
  setRec(rows[2], "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", EventType::ALARM_GARAGE, EventStatus::END,
         1200);

  auto open = findOpenEvents(rows);
  assert(open.size() == 1);
  assert(strcmp(open[0].id, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb") == 0);
}

static void test_instant_event_never_open() {
  std::vector<EventRecord> rows(1);
  setRec(rows[0], "cccccccccccccccccccccccccccccccc", EventType::REBOOT, EventStatus::INSTANT,
         1000);
  assert(findOpenEvents(rows).empty());
}

static void test_most_recent_open_event_is_selected_per_type() {
  std::vector<EventRecord> rows(2);
  setRec(rows[0], "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", EventType::ALARM_GENERAL,
         EventStatus::START, 1000);
  setRec(rows[1], "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", EventType::ALARM_GENERAL,
         EventStatus::START, 1100);

  OpenEvent out{};
  assert(findMostRecentOpenEventForType(rows, EventType::ALARM_GENERAL, out));
  assert(strcmp(out.id, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb") == 0);
  assert(out.startTs == 1100);
  assert(!findMostRecentOpenEventForType(rows, EventType::POWER_LOSS, out));
}

int main() {
  test_start_without_end_is_open();
  test_start_with_end_is_not_open();
  test_multiple_ids_tracked_independently();
  test_instant_event_never_open();
  test_most_recent_open_event_is_selected_per_type();

  printf("test_open_events: all tests passed\n");
  return 0;
}
