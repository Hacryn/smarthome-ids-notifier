#include <cassert>
#include <cstdio>
#include <cstring>

#include "../src/config/TimezonePresets.h"

static void test_find_by_preset() {
  const TimezonePresetInfo* utc = findTimezonePreset(TimezonePreset::UTC);
  assert(utc != nullptr);
  assert(strcmp(utc->posixTz, "UTC0") == 0);

  const TimezonePresetInfo* rome = findTimezonePreset(TimezonePreset::EUROPE_ROME);
  assert(rome != nullptr);
  assert(strcmp(rome->posixTz, "CET-1CEST,M3.5.0,M10.5.0/3") == 0);
}

static void test_rome_and_berlin_share_posix_string() {
  const TimezonePresetInfo* rome = findTimezonePreset(TimezonePreset::EUROPE_ROME);
  const TimezonePresetInfo* berlin = findTimezonePreset(TimezonePreset::EUROPE_BERLIN);
  assert(strcmp(rome->posixTz, berlin->posixTz) == 0);  // sez. 10.1 - stesso fuso, stesse regole DST
}

static void test_find_by_name() {
  const TimezonePresetInfo* found = findTimezonePresetByName("America/Los_Angeles");
  assert(found != nullptr);
  assert(found->preset == TimezonePreset::AMERICA_LOS_ANGELES);

  assert(findTimezonePresetByName("Not/AZone") == nullptr);
}

static void test_all_presets_have_distinct_names() {
  for (size_t i = 0; i < TIMEZONE_PRESETS_COUNT; i++) {
    for (size_t j = i + 1; j < TIMEZONE_PRESETS_COUNT; j++) {
      assert(strcmp(TIMEZONE_PRESETS[i].name, TIMEZONE_PRESETS[j].name) != 0);
    }
  }
}

int main() {
  test_find_by_preset();
  test_rome_and_berlin_share_posix_string();
  test_find_by_name();
  test_all_presets_have_distinct_names();

  printf("test_timezone_presets: tutti i test superati\n");
  return 0;
}
