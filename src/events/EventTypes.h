#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Sez. 3.2 di DESIGN.md - valori enum mai riassegnati, estendibili solo in coda.
enum class EventType : uint8_t {
  REBOOT = 0,
  POWER_LOSS = 1,
  NETWORK_ISSUE = 2,
  ALARM_GENERAL = 10,
  ALARM_INTERNAL = 11,
  ALARM_GARAGE = 12,
};

// Sez. 5.2 - status della riga di log.
enum class EventStatus : uint8_t {
  START = 0,
  END = 1,
  INSTANT = 2,
};

// Sez. 3.2.1 - politica di notifica per tipo.
enum class NotifyPolicy : uint8_t {
  START_AND_END,
  ONLY_END,
  INSTANT,
};

struct EventTypeConfig {
  EventType type;
  const char* label;
  const char* commandName;  // sez. 12 - identificatore usato da /notify <tipo> on|off
  int pin;  // -1 = nessuno (evento generato internamente: REBOOT, NETWORK_ISSUE)
  bool active_low;
  bool enabled;
  NotifyPolicy notify_policy;
};

// Sez. 3.2.1 - unico punto di verita della mappatura tipo -> comportamento.
// TODO(hardware): i numeri di pin sono placeholder, da assegnare in base al
// cablaggio reale delle PGM sulla centralina (sez. 2.1) prima della fase 4.
inline const EventTypeConfig EVENT_TYPES[] = {
    {EventType::REBOOT, "Riavvio", "REBOOT", -1, false, true, NotifyPolicy::INSTANT},
    {EventType::POWER_LOSS, "Mancanza rete 230V", "POWER_LOSS", 4, false, true,
     NotifyPolicy::START_AND_END},
    {EventType::NETWORK_ISSUE, "Problema di rete", "NETWORK_ISSUE", -1, false, true,
     NotifyPolicy::ONLY_END},
    {EventType::ALARM_GENERAL, "Allarme generale", "ALARM_GENERAL", 5, false, true,
     NotifyPolicy::START_AND_END},
    {EventType::ALARM_INTERNAL, "Allarme interno", "ALARM_INTERNAL", 6, false, true,
     NotifyPolicy::START_AND_END},
    {EventType::ALARM_GARAGE, "Allarme garage", "ALARM_GARAGE", 7, false, true,
     NotifyPolicy::START_AND_END},
};
constexpr size_t EVENT_TYPES_COUNT = sizeof(EVENT_TYPES) / sizeof(EVENT_TYPES[0]);

inline const EventTypeConfig* findEventTypeConfig(EventType type) {
  for (size_t i = 0; i < EVENT_TYPES_COUNT; i++) {
    if (EVENT_TYPES[i].type == type) return &EVENT_TYPES[i];
  }
  return nullptr;
}

inline const EventTypeConfig* findEventTypeConfigByCommandName(const char* name) {
  for (size_t i = 0; i < EVENT_TYPES_COUNT; i++) {
    if (strcmp(EVENT_TYPES[i].commandName, name) == 0) return &EVENT_TYPES[i];
  }
  return nullptr;
}

// Sez. 3.2.1/3.2.3 - se una notifica va inviata per quello specifico status,
// secondo la politica del tipo (es. NETWORK_ISSUE notifica solo END).
inline bool shouldNotifyForStatus(NotifyPolicy policy, EventStatus status) {
  switch (policy) {
    case NotifyPolicy::START_AND_END:
      return status == EventStatus::START || status == EventStatus::END;
    case NotifyPolicy::ONLY_END:
      return status == EventStatus::END;
    case NotifyPolicy::INSTANT:
      return status == EventStatus::INSTANT;
  }
  return false;
}
