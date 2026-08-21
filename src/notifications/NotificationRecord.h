#pragma once

#include <stdint.h>

#include <string>

#include "../events/EventTypes.h"

// Sez. 7.2 - quale notifica dell'evento si sta tracciando.
enum class NotifyStatus : uint8_t {
  NOTIFIED_INSTANT = 0,
  NOTIFIED_START = 1,
  NOTIFIED_END = 2,
};

enum class NotifyState : uint8_t {
  PENDING = 0,
  RESOLVED = 1,
  ABANDONED = 2,
};

struct NotificationRecord {
  char id[33];  // stesso id dell'evento in log.jsonl (sez. 5.2)
  NotifyStatus status;
  uint32_t ts;
  NotifyState state;
  uint32_t n = 0;  // rilevante solo per PENDING/ABANDONED (sez. 7.2)
};

NotifyStatus eventStatusToNotifyStatus(EventStatus status);
EventStatus notifyStatusToEventStatus(NotifyStatus status);

// Sez. 4.2 - il segno meno del chat_id va sostituito da un prefisso testuale
// quando compone un nome file. Pura formattazione, nessuna dipendenza da FS.
std::string notificationLogPath(int64_t chatId);

// Chiave di raggruppamento (id, status) per la ricostruzione dello stato
// pendente (sez. 7.2 - "ultima riga vince" per ciascuna coppia).
std::string notificationKey(const char* id, NotifyStatus status);

// Sez. 7.2 - schema: {"id","status","ts","state","n"?}. "n" omesso su RESOLVED.
std::string serializeNotificationRecord(const NotificationRecord& rec);
bool parseNotificationRecord(const std::string& line, NotificationRecord& out);
