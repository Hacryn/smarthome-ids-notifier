#include "OpenEventsTracker.h"

#include <string.h>

#include <map>
#include <string>

std::vector<OpenEvent> findOpenEvents(const std::vector<EventRecord>& rows) {
  std::map<std::string, OpenEvent> open;

  for (const auto& rec : rows) {
    std::string id(rec.id);

    if (rec.status == EventStatus::START) {
      OpenEvent ev{};
      strncpy(ev.id, rec.id, sizeof(ev.id) - 1);
      ev.id[32] = '\0';
      ev.type = rec.type;
      ev.startTs = rec.ts;
      ev.approx = rec.approx;
      open[id] = ev;
    } else if (rec.status == EventStatus::END) {
      open.erase(id);
    }
  }

  std::vector<OpenEvent> result;
  result.reserve(open.size());
  for (const auto& [key, ev] : open) {
    (void)key;
    result.push_back(ev);
  }
  return result;
}

bool findMostRecentOpenEventForType(const std::vector<EventRecord>& rows, EventType type,
                                    OpenEvent& out) {
  bool found = false;
  for (const OpenEvent& ev : findOpenEvents(rows)) {
    if (ev.type != type) continue;
    if (!found || ev.startTs > out.startTs) {
      out = ev;
      found = true;
    }
  }
  return found;
}
