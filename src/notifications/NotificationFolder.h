#pragma once

#include <map>
#include <string>
#include <vector>

#include "NotificationRecord.h"

// Sez. 7.2 - applica in ordine le righe del file: l'ultima riga per
// ciascuna coppia (id, status) e' quella valida.
std::map<std::string, NotificationRecord> foldNotificationRecords(
    const std::vector<NotificationRecord>& rows);

// Sottoinsieme delle voci ancora PENDING (quelle da recuperare, sez. 6.2).
std::vector<NotificationRecord> pendingFrom(const std::map<std::string, NotificationRecord>& latest);
