#pragma once

#include <stdint.h>

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
