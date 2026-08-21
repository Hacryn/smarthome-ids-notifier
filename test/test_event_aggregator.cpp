#include <cassert>
#include <cstdio>
#include <cstring>

#include "../src/events/EventAggregator.h"

static void setRec(EventRecord& rec, const char* id, EventType type, EventStatus status,
                    uint32_t ts, bool approx = false) {
  strcpy(rec.id, id);
  rec.type = type;
  rec.status = status;
  rec.ts = ts;
  rec.approx = approx;
}

static void test_start_end_pair_aggregated() {
  AggregatedEventLog log(10);
  EventRecord start{}, end{};
  setRec(start, "a1b2c3d4e5f60718293a4b5c6d7e8f90", EventType::ALARM_GARAGE, EventStatus::START, 1000);
  setRec(end, "a1b2c3d4e5f60718293a4b5c6d7e8f90", EventType::ALARM_GARAGE, EventStatus::END, 1300);

  log.observe(start);
  log.observe(end);

  assert(log.events().size() == 1);
  const AggregatedEvent& ev = log.events().front();
  assert(ev.hasEnd);
  assert(ev.startTs == 1000);
  assert(ev.endTs == 1300);
  assert(!ev.isInstant);
}

static void test_open_event_has_no_end() {
  AggregatedEventLog log(10);
  EventRecord start{};
  setRec(start, "a1b2c3d4e5f60718293a4b5c6d7e8f90", EventType::ALARM_GARAGE, EventStatus::START,
         1000, true);
  log.observe(start);

  assert(log.events().size() == 1);
  assert(!log.events().front().hasEnd);
  assert(log.events().front().startApprox);
}

static void test_instant_event_never_has_end() {
  AggregatedEventLog log(10);
  EventRecord instant{};
  setRec(instant, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", EventType::REBOOT, EventStatus::INSTANT, 500);
  log.observe(instant);

  assert(log.events().size() == 1);
  assert(log.events().front().isInstant);
  assert(!log.events().front().hasEnd);
}

static void test_ring_buffer_keeps_only_last_n() {
  AggregatedEventLog log(2);
  char id[33];
  for (int i = 0; i < 5; i++) {
    snprintf(id, sizeof(id), "%032d", i);
    EventRecord rec{};
    setRec(rec, id, EventType::ALARM_GARAGE, EventStatus::INSTANT, 1000 + i);
    log.observe(rec);
  }

  assert(log.events().size() == 2);
  // Devono restare gli ultimi due osservati (id "3" e "4").
  auto it = log.events().begin();
  assert(it->startTs == 1003);
  ++it;
  assert(it->startTs == 1004);
}

static void test_orphan_end_for_evicted_id_is_ignored() {
  AggregatedEventLog log(1);
  EventRecord first{}, second{}, endForFirst{};
  setRec(first, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", EventType::ALARM_GARAGE, EventStatus::START, 1000);
  setRec(second, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", EventType::ALARM_GARAGE, EventStatus::START, 1100);
  setRec(endForFirst, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", EventType::ALARM_GARAGE, EventStatus::END,
         1200);

  log.observe(first);
  log.observe(second);  // "first" scartato dal ring (cap=1)
  log.observe(endForFirst);  // orfano, ignorato senza crash

  assert(log.events().size() == 1);
  assert(!log.events().front().hasEnd);  // "second" resta aperto
}

static void test_format_duration_seconds() {
  assert(formatDurationSeconds(300) == "5m");
  assert(formatDurationSeconds(1980) == "33m");
  assert(formatDurationSeconds(4680) == "1h 18m");
  assert(formatDurationSeconds(0) == "0m");
}

int main() {
  test_format_duration_seconds();
  test_start_end_pair_aggregated();
  test_open_event_has_no_end();
  test_instant_event_never_has_end();
  test_ring_buffer_keeps_only_last_n();
  test_orphan_end_for_evicted_id_is_ignored();

  printf("test_event_aggregator: tutti i test superati\n");
  return 0;
}
