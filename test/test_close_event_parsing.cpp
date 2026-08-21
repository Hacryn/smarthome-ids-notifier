#include <cassert>
#include <cstdio>

#include "../src/telegram/CallbackData.h"
#include "../src/telegram/CommandParser.h"

static const char* kId = "a1b2c3d4e5f60718293a4b5c6d7e8f90";

static void test_callback_data_roundtrip() {
  std::string data = closeEventCallbackData(kId);
  assert(data == "c:a1b2c3d4e5f60718293a4b5c6d7e8f90");
  assert(data.size() == 34);  // sez. 8.1 - entro il limite di 64 byte

  std::string parsedId;
  assert(parseCloseEventCallbackData(data, parsedId));
  assert(parsedId == kId);
}

static void test_callback_data_rejects_wrong_prefix() {
  std::string out;
  assert(!parseCloseEventCallbackData("x:" + std::string(kId), out));
}

static void test_callback_data_rejects_wrong_length() {
  std::string out;
  assert(!parseCloseEventCallbackData("c:tooshort", out));
}

static void test_close_event_command_with_timestamp() {
  std::string id;
  bool hasTs = false;
  uint32_t ts = 0;
  std::string text = std::string("/closeevent ") + kId + " 1755500000";
  assert(parseCloseEventCommand(text, id, hasTs, ts));
  assert(id == kId);
  assert(hasTs);
  assert(ts == 1755500000);
}

static void test_close_event_command_without_timestamp() {
  std::string id;
  bool hasTs = true;  // deve essere azzerato dalla funzione
  uint32_t ts = 0;
  std::string text = std::string("/closeevent ") + kId;
  assert(parseCloseEventCommand(text, id, hasTs, ts));
  assert(id == kId);
  assert(!hasTs);
}

static void test_close_event_command_rejects_wrong_command() {
  std::string id;
  bool hasTs = false;
  uint32_t ts = 0;
  assert(!parseCloseEventCommand(std::string("/status ") + kId, id, hasTs, ts));
}

static void test_close_event_command_rejects_wrong_id_length() {
  std::string id;
  bool hasTs = false;
  uint32_t ts = 0;
  assert(!parseCloseEventCommand("/closeevent tooshort", id, hasTs, ts));
}

static void test_close_event_command_rejects_non_numeric_timestamp() {
  std::string id;
  bool hasTs = false;
  uint32_t ts = 0;
  std::string text = std::string("/closeevent ") + kId + " notanumber";
  assert(!parseCloseEventCommand(text, id, hasTs, ts));
}

int main() {
  test_callback_data_roundtrip();
  test_callback_data_rejects_wrong_prefix();
  test_callback_data_rejects_wrong_length();
  test_close_event_command_with_timestamp();
  test_close_event_command_without_timestamp();
  test_close_event_command_rejects_wrong_command();
  test_close_event_command_rejects_wrong_id_length();
  test_close_event_command_rejects_non_numeric_timestamp();

  printf("test_close_event_parsing: tutti i test superati\n");
  return 0;
}
