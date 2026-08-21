#include <cassert>
#include <cstdio>

#include "../src/telegram/RateLimiter.h"
#include "../src/telegram/SendOutcomeClassifier.h"

static void test_classify_success() {
  RawSendOutcome outcome{/*isError=*/false, /*isEmpty=*/false, /*errorCode=*/0};
  assert(classifySendOutcome(outcome) == SendOutcomeCategory::SUCCESS);
}

static void test_classify_transient_network_on_empty_body() {
  RawSendOutcome outcome{true, /*isEmpty=*/true, 0};
  assert(classifySendOutcome(outcome) == SendOutcomeCategory::TRANSIENT_NETWORK);
}

static void test_classify_transient_server_5xx() {
  RawSendOutcome outcome500{true, false, 500};
  RawSendOutcome outcome503{true, false, 503};
  assert(classifySendOutcome(outcome500) == SendOutcomeCategory::TRANSIENT_SERVER);
  assert(classifySendOutcome(outcome503) == SendOutcomeCategory::TRANSIENT_SERVER);
}

static void test_classify_throttling_429() {
  RawSendOutcome outcome{true, false, 429};
  assert(classifySendOutcome(outcome) == SendOutcomeCategory::THROTTLING);
}

static void test_classify_permanent_recipient() {
  RawSendOutcome outcome403{true, false, 403};
  RawSendOutcome outcome400{true, false, 400};
  assert(classifySendOutcome(outcome403) == SendOutcomeCategory::PERMANENT_RECIPIENT);
  assert(classifySendOutcome(outcome400) == SendOutcomeCategory::PERMANENT_RECIPIENT);
}

static void test_classify_system_error() {
  RawSendOutcome outcome401{true, false, 401};
  RawSendOutcome outcome404{true, false, 404};
  assert(classifySendOutcome(outcome401) == SendOutcomeCategory::SYSTEM_ERROR);
  assert(classifySendOutcome(outcome404) == SendOutcomeCategory::SYSTEM_ERROR);
}

static void test_classify_unknown_code_falls_back_to_system_error() {
  RawSendOutcome outcome{true, false, 418};  // code not covered by the table in sec. 6.5
  assert(classifySendOutcome(outcome) == SendOutcomeCategory::SYSTEM_ERROR);
}

static void test_rate_limiter_allows_first_send_immediately() {
  RateLimiter limiter;
  assert(limiter.tryConsume(0));
}

static void test_rate_limiter_blocks_within_interval() {
  RateLimiter limiter;
  assert(limiter.tryConsume(1000));
  assert(!limiter.tryConsume(1500));  // 500ms later, below the 1100ms minimum
}

static void test_rate_limiter_allows_after_interval() {
  RateLimiter limiter;
  assert(limiter.tryConsume(1000));
  assert(limiter.tryConsume(2100));  // exactly 1100ms later
}

int main() {
  test_classify_success();
  test_classify_transient_network_on_empty_body();
  test_classify_transient_server_5xx();
  test_classify_throttling_429();
  test_classify_permanent_recipient();
  test_classify_system_error();
  test_classify_unknown_code_falls_back_to_system_error();
  test_rate_limiter_allows_first_send_immediately();
  test_rate_limiter_blocks_within_interval();
  test_rate_limiter_allows_after_interval();

  printf("test_telegram_send: all tests passed\n");
  return 0;
}
