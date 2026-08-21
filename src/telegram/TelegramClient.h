#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "SendOutcomeClassifier.h"

// Sez. 3.5 - client FastBot2. Non testabile via harness host-side (dipende
// da WiFi/TLS reali).
void initTelegramClient(const char* token);

// Sez. 6.5/6.6 - invia un messaggio e ne classifica l'esito. Gestisce in
// proprio fino a 3 ritentativi immediati su 429, rispettando retry_after
// (non contati come fallimento). L'intervallo minimo di 1100ms tra invii
// (sez. 6.6) NON e' applicato qui: e' responsabilita' del chiamante tramite
// RateLimiter, cosi' che una coda di invii (fase 8) possa restare non
// bloccante rispetto al loop.
SendOutcomeCategory sendTelegramMessage(int64_t chatId, const char* text);

struct InlineButton {
  std::string label;
  std::string callbackData;
};

// Sez. 8.1 - invio con tastiera inline, un bottone per riga. Usato solo per
// il riepilogo eventi aperti destinato agli admin (invio diretto, non
// tracciato in sez. 7: e' un messaggio di sistema, non la notifica di un
// evento). Stessa classificazione/retry di sendTelegramMessage.
SendOutcomeCategory sendMessageWithButtons(int64_t chatId, const char* text,
                                            const std::vector<InlineButton>& buttons);

// Sez. 8.1 - tipi primitivi per non far trapelare i tipi FastBot2 fuori da
// questo modulo (Notifier.ino resta libero di non includere FastBot2.h).
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

// Registra i gestori per callback query (bottoni inline) e messaggi di
// testo in arrivo. Da chiamare una sola volta in setup(), dopo
// initTelegramClient().
void setTelegramUpdateHandlers(CallbackHandler onCallback, CommandHandler onCommand);

// Da chiamare ad ogni ciclo di loop: elabora gli aggiornamenti in arrivo
// (long polling, sez. 3.5.1).
void tickTelegramUpdates();

// Sez. 8.1 - risposta immediata alla callback query (rimuove lo spinner).
void answerCallback(const std::string& queryId, const std::string& text);
