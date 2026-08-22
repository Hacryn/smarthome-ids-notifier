#include <cassert>
#include <cstdio>

#include "../src/telegram/UnauthorizedRequestLog.h"

static void test_insert_under_capacity() {
  std::vector<UnauthorizedRequest> buffer;
  recordUnauthorizedRequest(buffer, {111, "u1", "mario", 1000});
  recordUnauthorizedRequest(buffer, {222, "u2", "", 1010});

  assert(buffer.size() == 2);
  assert(buffer[0].chatId == 111);
  assert(buffer[1].chatId == 222);
}

static void test_discards_oldest_when_full_with_new_chat_id() {
  std::vector<UnauthorizedRequest> buffer;
  for (int64_t i = 1; i <= 5; i++) {
    recordUnauthorizedRequest(buffer, {i, "u", "", static_cast<uint32_t>(1000 + i)});
  }
  assert(buffer.size() == kMaxUnauthorizedRequests);
  assert(buffer.front().chatId == 1);

  // A 6th, never-seen chat_id: the oldest (chatId 1) must be dropped.
  recordUnauthorizedRequest(buffer, {6, "u", "", 1006});
  assert(buffer.size() == kMaxUnauthorizedRequests);
  assert(buffer.front().chatId == 2);
  assert(buffer.back().chatId == 6);
  for (const auto& r : buffer) assert(r.chatId != 1);
}

static void test_existing_chat_id_updates_and_moves_to_end_without_growing() {
  std::vector<UnauthorizedRequest> buffer;
  recordUnauthorizedRequest(buffer, {111, "u1", "old_name", 1000});
  recordUnauthorizedRequest(buffer, {222, "u2", "", 1010});
  recordUnauthorizedRequest(buffer, {111, "u1", "new_name", 2000});  // same chat_id again

  assert(buffer.size() == 2);  // no duplicate created
  assert(buffer.back().chatId == 111);
  assert(buffer.back().username == "new_name");
  assert(buffer.back().ts == 2000);
  assert(buffer.front().chatId == 222);
}

static void test_insistent_single_chat_id_never_fills_the_buffer() {
  std::vector<UnauthorizedRequest> buffer;
  for (int i = 0; i < 20; i++) {
    recordUnauthorizedRequest(buffer, {111, "u1", "", static_cast<uint32_t>(1000 + i)});
  }
  assert(buffer.size() == 1);
  assert(buffer[0].ts == 1019);
}

int main() {
  test_insert_under_capacity();
  test_discards_oldest_when_full_with_new_chat_id();
  test_existing_chat_id_updates_and_moves_to_end_without_growing();
  test_insistent_single_chat_id_never_fills_the_buffer();

  printf("test_unauthorized_request_log: all tests passed\n");
  return 0;
}
