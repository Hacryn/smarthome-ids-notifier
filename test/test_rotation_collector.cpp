#include <cassert>
#include <cstdio>
#include <cstring>

#include "../src/rotation/IdBinary.h"
#include "../src/rotation/RotationCollector.h"

static void setRec(EventRecord& rec, const char* id, EventStatus status, uint32_t ts) {
  strcpy(rec.id, id);
  rec.type = EventType::ALARM_GARAGE;
  rec.status = status;
  rec.ts = ts;
  rec.approx = false;
}

static void test_hex_id_to_binary_roundtrip() {
  std::array<uint8_t, 16> bin{};
  assert(hexIdToBinary("a1b2c3d4e5f60718293a4b5c6d7e8f90", bin));
  assert(bin[0] == 0xa1);
  assert(bin[1] == 0xb2);
  assert(bin[15] == 0x90);
}

static void test_hex_id_to_binary_rejects_wrong_length() {
  std::array<uint8_t, 16> bin{};
  assert(!hexIdToBinary("tooshort", bin));
}

static void test_hex_id_to_binary_rejects_invalid_chars() {
  std::array<uint8_t, 16> bin{};
  assert(!hexIdToBinary("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz", bin));
}

static void test_collector_ignores_rows_after_cutoff() {
  DeletableIdCollector c(/*cutoff=*/1000);
  EventRecord rec{};
  setRec(rec, "a1b2c3d4e5f60718293a4b5c6d7e8f90", EventStatus::END, 2000);  // after the cutoff
  c.observe(rec);
  assert(c.ids().empty());
}

static void test_collector_ignores_start_rows() {
  DeletableIdCollector c(1000);
  EventRecord rec{};
  setRec(rec, "a1b2c3d4e5f60718293a4b5c6d7e8f90", EventStatus::START, 500);
  c.observe(rec);
  assert(c.ids().empty());  // only END/INSTANT make an id deletable
}

static void test_collector_collects_end_and_instant_before_cutoff() {
  DeletableIdCollector c(1000);
  EventRecord end{};
  setRec(end, "a1b2c3d4e5f60718293a4b5c6d7e8f90", EventStatus::END, 500);
  EventRecord instant{};
  setRec(instant, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", EventStatus::INSTANT, 500);

  c.observe(end);
  c.observe(instant);

  assert(c.ids().size() == 2);
  assert(c.contains("a1b2c3d4e5f60718293a4b5c6d7e8f90"));
  assert(c.contains("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
  assert(!c.contains("cccccccccccccccccccccccccccccccc"));
}

static void test_collector_stops_at_cap() {
  DeletableIdCollector c(1000, /*cap=*/2);
  char id[33];
  for (int i = 0; i < 5; i++) {
    snprintf(id, sizeof(id), "%032d", i);
    EventRecord rec{};
    setRec(rec, id, EventStatus::END, 500);
    c.observe(rec);
  }
  assert(c.ids().size() == 2);
  assert(c.capReached());
}

int main() {
  test_hex_id_to_binary_roundtrip();
  test_hex_id_to_binary_rejects_wrong_length();
  test_hex_id_to_binary_rejects_invalid_chars();
  test_collector_ignores_rows_after_cutoff();
  test_collector_ignores_start_rows();
  test_collector_collects_end_and_instant_before_cutoff();
  test_collector_stops_at_cap();

  printf("test_rotation_collector: all tests passed\n");
  return 0;
}
