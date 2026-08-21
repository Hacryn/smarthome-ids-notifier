#pragma once

#include <stdint.h>

#include "EventLog.h"

inline const char* kEventLogPath = "/log.jsonl";
inline const char* kEventLogRotationTmpPath = "/log.jsonl.tmp";

// Sez. 5.1 - append-only. Ritenta una volta in caso di fallimento (apertura
// o byte scritti insufficienti); se fallisce anche al secondo tentativo,
// incrementa il contatore condiviso di sez. 9.4 (fsErrorCounter()) e
// ritorna false. La notifica Telegram dell'evento non e' comunque bloccata
// da questo fallimento (il chiamante decide indipendentemente).
bool appendEventRecord(const EventRecord& rec);

// Sez. 5.4.3 - legge l'ultima riga del file risalendo dalla fine, senza
// scansione completa. Ritorna 0 se il file non esiste, e' vuoto, o
// l'ultima riga non e' un record valido.
uint32_t readLastWrittenTimestamp();

// Sez. 6.4/6.7 - cerca la riga (id, status) nel registro eventi, per
// recuperare type/approx durante una scansione di recupero (sez. 7.2 non
// duplica questi campi nel registro notifiche). Scansione lineare in
// streaming; accettabile perche' usata solo durante recuperi, rari.
bool findEventRecordById(const char* id, EventStatus status, EventRecord& out);
