#pragma once

#include <stdint.h>

// Sez. 9.1/9.3 - riscrittura filtrata di notif_<chat_id>.jsonl. A
// differenza del registro eventi, l'eliminabilita' e' decisa dalla
// piegatura (sez. 7.2 - "ultima riga vince" per ciascuna coppia
// id/status), non da una singola riga: il file e' per design quasi
// sempre vuoto o minimo (sez. 7.2), quindi non serve il tetto/ripetizione
// di sez. 9.3.1. Non testabile via harness host-side.
bool rotateNotificationLog(int64_t chatId, uint32_t cutoff);
