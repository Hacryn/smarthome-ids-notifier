#pragma once

#include <stddef.h>
#include <stdint.h>

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
  int pin;  // -1 = nessuno (evento generato internamente: REBOOT, NETWORK_ISSUE)
  bool active_low;
  bool enabled;
  NotifyPolicy notify_policy;
};

// Sez. 3.2.1 - unico punto di verita della mappatura tipo -> comportamento.
// TODO(hardware): i numeri di pin sono placeholder, da assegnare in base al
// cablaggio reale delle PGM sulla centralina (sez. 2.1) prima della fase 4.
inline const EventTypeConfig EVENT_TYPES[] = {
    {EventType::REBOOT, "Riavvio", -1, false, true, NotifyPolicy::INSTANT},
    {EventType::POWER_LOSS, "Mancanza rete 230V", 4, false, true,
     NotifyPolicy::START_AND_END},
    {EventType::NETWORK_ISSUE, "Problema di rete", -1, false, true,
     NotifyPolicy::ONLY_END},
    {EventType::ALARM_GENERAL, "Allarme generale", 5, false, true,
     NotifyPolicy::START_AND_END},
    {EventType::ALARM_INTERNAL, "Allarme interno", 6, false, true,
     NotifyPolicy::START_AND_END},
    {EventType::ALARM_GARAGE, "Allarme garage", 7, false, true,
     NotifyPolicy::START_AND_END},
};
constexpr size_t EVENT_TYPES_COUNT = sizeof(EVENT_TYPES) / sizeof(EVENT_TYPES[0]);

inline const EventTypeConfig* findEventTypeConfig(EventType type) {
  for (size_t i = 0; i < EVENT_TYPES_COUNT; i++) {
    if (EVENT_TYPES[i].type == type) return &EVENT_TYPES[i];
  }
  return nullptr;
}
