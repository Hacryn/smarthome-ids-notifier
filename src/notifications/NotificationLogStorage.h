#pragma once

#include <map>
#include <string>

#include "NotificationRecord.h"

// Sez. 7.1/7.2 - I/O reale su LittleFS per notif_<chat_id>.jsonl. Non
// testabile via harness host-side (dipende da LittleFS reale).

// Ricostruisce lo stato corrente (sez. 7.2) leggendo l'intero file in
// streaming, una riga alla volta. Un file assente equivale a "nessun
// pendente" (il caso comune, sez. 7.2), non un errore.
std::map<std::string, NotificationRecord> loadNotificationState(int64_t chatId);

bool appendNotificationRecord(int64_t chatId, const NotificationRecord& rec);
