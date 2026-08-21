#include "TelegramClient.h"

#include <FastBot2.h>

namespace {

FastBot2 g_bot;
constexpr int kMaxThrottleRetries = 3;  // sez. 6.6

SendOutcomeCategory classifyResult(fb::Result& r) {
  RawSendOutcome outcome;
  outcome.isError = r.isError();
  outcome.isEmpty = r.isEmpty();
  outcome.errorCode = (outcome.isError && !outcome.isEmpty) ? r.getErrorCode().toInt32() : 0;
  return classifySendOutcome(outcome);
}

}  // namespace

void initTelegramClient(const char* token) {
  g_bot.setToken(token);
  g_bot.setPollMode(fb::Poll::Long, 60000);  // sez. 3.5.1
}

SendOutcomeCategory sendTelegramMessage(int64_t chatId, const char* text) {
  for (int attempt = 0; attempt < kMaxThrottleRetries; attempt++) {
    fb::Result r = g_bot.sendMessage(fb::Message(text, fb::ID(static_cast<long long>(chatId))));
    SendOutcomeCategory category = classifyResult(r);

    if (category != SendOutcomeCategory::THROTTLING) return category;

    uint32_t retryAfterSec = r._parser["parameters"]["retry_after"];
    delay(retryAfterSec * 1000UL);
  }
  // Terzo 429 consecutivo: passa al normale meccanismo di retry programmato (fase 8).
  return SendOutcomeCategory::THROTTLING;
}
