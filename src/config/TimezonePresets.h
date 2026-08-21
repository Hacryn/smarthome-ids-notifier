#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Sez. 10.1 - set di preset predefiniti, mappati a stringhe TZ POSIX.
enum class TimezonePreset : uint8_t {
  UTC = 0,
  EUROPE_ROME = 1,
  EUROPE_BERLIN = 2,
  EUROPE_LONDON = 3,
  EUROPE_MOSCOW = 4,
  AMERICA_NEW_YORK = 5,
  AMERICA_LOS_ANGELES = 6,
};

struct TimezonePresetInfo {
  TimezonePreset preset;
  const char* name;     // usato dal comando /settimezone (fase successiva)
  const char* posixTz;  // sez. 10.1/10.2 - gestisce DST automaticamente
};

inline const TimezonePresetInfo TIMEZONE_PRESETS[] = {
    {TimezonePreset::UTC, "UTC", "UTC0"},
    {TimezonePreset::EUROPE_ROME, "Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {TimezonePreset::EUROPE_BERLIN, "Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {TimezonePreset::EUROPE_LONDON, "Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {TimezonePreset::EUROPE_MOSCOW, "Europe/Moscow", "MSK-3"},
    {TimezonePreset::AMERICA_NEW_YORK, "America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {TimezonePreset::AMERICA_LOS_ANGELES, "America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
};
constexpr size_t TIMEZONE_PRESETS_COUNT = sizeof(TIMEZONE_PRESETS) / sizeof(TIMEZONE_PRESETS[0]);

inline const TimezonePresetInfo* findTimezonePreset(TimezonePreset preset) {
  for (size_t i = 0; i < TIMEZONE_PRESETS_COUNT; i++) {
    if (TIMEZONE_PRESETS[i].preset == preset) return &TIMEZONE_PRESETS[i];
  }
  return nullptr;
}

inline const TimezonePresetInfo* findTimezonePresetByName(const char* name) {
  for (size_t i = 0; i < TIMEZONE_PRESETS_COUNT; i++) {
    if (strcmp(TIMEZONE_PRESETS[i].name, name) == 0) return &TIMEZONE_PRESETS[i];
  }
  return nullptr;
}
