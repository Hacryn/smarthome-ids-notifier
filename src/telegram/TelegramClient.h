#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "SendOutcomeClassifier.h"

// Sec. 3.5 - FastBot2 client. Not testable via the host-side harness
// (depends on real WiFi/TLS).
void initTelegramClient(const char* token);

// Sec. 6.5/6.6 - sends a message and classifies its outcome. Handles up to
// 3 immediate retries on 429 internally, honoring retry_after (not
// counted as a failure). The minimum 1100ms interval between sends (sec.
// 6.6) is NOT applied here: it's the caller's responsibility via
// RateLimiter, so a send queue (phase 8) can stay non-blocking with
// respect to the loop.
SendOutcomeCategory sendTelegramMessage(int64_t chatId, const char* text);

struct InlineButton {
  std::string label;
  std::string callbackData;
};

// Sec. 8.1 - send with an inline keyboard, one button per row. Used only
// for the open-events summary sent to admins (direct send, not tracked in
// sec. 7: it's a system message, not an event notification). Same
// classification/retry as sendTelegramMessage.
SendOutcomeCategory sendMessageWithButtons(int64_t chatId, const char* text,
                                            const std::vector<InlineButton>& buttons);

// Sec. 12.3 - sends a LittleFS file as a Telegram document (raw dump, no
// reformatting). Returns false if the file can't be opened or the send
// fails (network/API), without distinguishing which - the caller only
// needs a yes/no to relay to the admin.
bool sendDocumentFromFile(int64_t chatId, const std::string& path, const std::string& filename);

// Sec. 8.1 - primitive types so FastBot2's own types don't leak outside
// this module (Notifier.ino stays free of including FastBot2.h).
struct IncomingCallback {
  int64_t fromChatId;
  std::string queryId;
  std::string data;
};

struct IncomingCommand {
  int64_t chatId;
  std::string text;
};

using CallbackHandler = void (*)(const IncomingCallback&);
using CommandHandler = void (*)(const IncomingCommand&);

// Registers the handlers for incoming callback queries (inline buttons)
// and text messages. Call once in setup(), after initTelegramClient().
void setTelegramUpdateHandlers(CallbackHandler onCallback, CommandHandler onCommand);

// Call on every loop cycle: processes incoming updates (long polling,
// sec. 3.5.1).
void tickTelegramUpdates();

// Sec. 8.1 - immediate reply to the callback query (removes the spinner).
void answerCallback(const std::string& queryId, const std::string& text);
