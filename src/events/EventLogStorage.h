#pragma once

#include <stdint.h>

#include "EventLog.h"

inline const char* kEventLogPath = "/log.jsonl";

// Sez. 5.1 - append-only. Ritorna false se l'apertura fallisce o se i byte
// scritti non corrispondono alla riga attesa (sez. 9.4 gestisce il retry e
// il contatore di errori: qui solo l'esito grezzo dell'operazione).
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
