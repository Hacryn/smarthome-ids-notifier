#include "TelegramClient.h"

#include <FastBot2.h>

namespace {

FastBot2 g_bot;
constexpr int kMaxThrottleRetries = 3;  // sec. 6.6

CallbackHandler g_callbackHandler = nullptr;
CommandHandler g_commandHandler = nullptr;

SendOutcomeCategory classifyResult(fb::Result& r) {
  RawSendOutcome outcome;
  outcome.isError = r.isError();
  outcome.isEmpty = r.isEmpty();
  outcome.errorCode = (outcome.isError && !outcome.isEmpty) ? r.getErrorCode().toInt32() : 0;
  return classifySendOutcome(outcome);
}

// Sec. 6.6 - up to 3 immediate retries on 429 honoring retry_after, not
// counted as a failure. Shared by every send variant.
SendOutcomeCategory sendWithThrottleRetry(const fb::Message& msg) {
  for (int attempt = 0; attempt < kMaxThrottleRetries; attempt++) {
    fb::Result r = g_bot.sendMessage(msg);
    SendOutcomeCategory category = classifyResult(r);

    if (category != SendOutcomeCategory::THROTTLING) return category;

    uint32_t retryAfterSec = r._parser["parameters"]["retry_after"];
    delay(retryAfterSec * 1000UL);
  }
  // Third consecutive 429: hands off to the normal scheduled-retry mechanism.
  return SendOutcomeCategory::THROTTLING;
}

void onUpdate(fb::Update& u) {
  if (u.isQuery()) {
    if (!g_callbackHandler) return;

    IncomingCallback cb;
    cb.fromChatId = u.query().from().id().toInt64();
    String queryId = u.query().id();
    String data = u.query().data();
    cb.queryId = std::string(queryId.c_str());
    cb.data = std::string(data.c_str());
    g_callbackHandler(cb);
  } else if (u.isMessage()) {
    if (!g_commandHandler) return;

    String text = u.message().text();
    if (text.length() == 0 || text[0] != '/') return;  // commands only

    IncomingCommand cmd;
    cmd.chatId = u.message().chat().id().toInt64();
    cmd.text = std::string(text.c_str());
    g_commandHandler(cmd);
  }
}

}  // namespace

void initTelegramClient(const char* token) {
  g_bot.setToken(token);
  g_bot.setPollMode(fb::Poll::Long, 60000);  // sec. 3.5.1
}

SendOutcomeCategory sendTelegramMessage(int64_t chatId, const char* text) {
  fb::Message msg(text, fb::ID(static_cast<long long>(chatId)));
  return sendWithThrottleRetry(msg);
}

SendOutcomeCategory sendMessageWithButtons(int64_t chatId, const char* text,
                                            const std::vector<InlineButton>& buttons) {
  fb::InlineKeyboard kb;
  for (const auto& b : buttons) {
    kb.addButton(b.label.c_str(), b.callbackData.c_str());
    kb.newRow();
  }

  fb::Message msg(text, fb::ID(static_cast<long long>(chatId)));
  msg.setKeyboard(&kb);
  return sendWithThrottleRetry(msg);
}

void setTelegramUpdateHandlers(CallbackHandler onCallback, CommandHandler onCommand) {
  g_callbackHandler = onCallback;
  g_commandHandler = onCommand;
  g_bot.attachUpdate(onUpdate);
}

void tickTelegramUpdates() { g_bot.tick(); }

void answerCallback(const std::string& queryId, const std::string& text) {
  g_bot.answerCallbackQuery(queryId.c_str(), text.c_str());
}
