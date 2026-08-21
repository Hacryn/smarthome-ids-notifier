#include "TimestampFormatter.h"

#include <stdlib.h>
#include <time.h>

#include "TimezonePresets.h"

std::string formatTimestampForUser(uint32_t epoch, const UserConfig& cfg, bool approx) {
  const TimezonePresetInfo* tz = findTimezonePreset(cfg.timezone);
  const char* posixTz = tz ? tz->posixTz : "UTC0";

  setenv("TZ", posixTz, 1);
  tzset();

  time_t t = static_cast<time_t>(epoch);
  struct tm tmVal;
  localtime_r(&t, &tmVal);

  char buf[64];
  if (strftime(buf, sizeof(buf), cfg.dateFormat.c_str(), &tmVal) == 0) {
    buf[0] = '\0';  // formato non valido/troppo lungo: meglio vuoto che troncato a meta'
  }

  setenv("TZ", "UTC0", 1);  // sez. 10.3 - ripristino dopo l'uso
  tzset();

  std::string result;
  if (approx) result += "~";
  result += buf;
  return result;
}
