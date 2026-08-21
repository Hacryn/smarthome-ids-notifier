#include "OpenEventsManager.h"

#include <LittleFS.h>
#include <string.h>

#include "../notifications/NotificationEngine.h"
#include "../notifications/NotificationFolder.h"
#include "../notifications/NotificationLogStorage.h"
#include "../notifications/NotificationPresentation.h"
#include "../telegram/CallbackData.h"
#include "../telegram/TelegramClient.h"
#include "../time/TimeAnchor.h"
#include "EventLogStorage.h"

std::vector<OpenEvent> detectOpenEvents() {
  std::vector<EventRecord> rows;

  File f = LittleFS.open(kEventLogPath, "r");
  if (f) {
    while (f.available()) {
      String line = f.readStringUntil('\n');
      if (line.length() == 0) continue;

      EventRecord rec{};
      if (parseEventRecord(std::string(line.c_str()), rec)) rows.push_back(rec);
    }
    f.close();
  }

  return findOpenEvents(rows);
}

bool closeOpenEvent(const std::vector<AuthorizedUser>& users, const std::string& id,
                     uint32_t closeTs, uint32_t& lastWrittenTs) {
  std::vector<OpenEvent> open = detectOpenEvents();
  const OpenEvent* match = nullptr;
  for (const auto& ev : open) {
    if (id == ev.id) {
      match = &ev;
      break;
    }
  }
  if (!match) return false;  // gia' chiuso, o mai esistito

  ClampedTimestamp clamped = applyMonotonicClamp(closeTs, lastWrittenTs);

  EventRecord rec{};
  memcpy(rec.id, match->id, 32);
  rec.id[32] = '\0';
  rec.type = match->type;
  rec.status = EventStatus::END;
  rec.ts = clamped.ts;
  rec.approx = true;  // sempre vero finche' non esiste una fonte NTP (fase successiva)

  if (!appendEventRecord(rec)) return false;
  lastWrittenTs = clamped.ts;

  const EventTypeConfig* cfg = findEventTypeConfig(rec.type);
  if (cfg && shouldNotifyForStatus(cfg->notify_policy, rec.status)) {
    notifyEvent(users, rec.id, rec.type, rec.status, rec.ts, rec.approx);
  }
  return true;
}

namespace {

std::string buildSummaryText(const std::vector<OpenEvent>& openEvents,
                              const std::vector<NotificationRecord>& nearAbandonment) {
  std::string text;

  if (!openEvents.empty()) {
    text += "Eventi ancora aperti:\n";
    for (const auto& ev : openEvents) {
      const EventTypeConfig* cfg = findEventTypeConfig(ev.type);
      text += "- ";
      text += cfg ? cfg->label : "Evento";
      text += " (";
      if (ev.approx) text += "~";
      text += std::to_string(ev.startTs);
      text += ")\n";
    }
  }

  if (!nearAbandonment.empty()) {
    if (!text.empty()) text += "\n";
    text += "Notifiche in difficolta' (vicine alla rinuncia):\n";
    for (const auto& rec : nearAbandonment) {
      text += "- id ";
      text += std::string(rec.id).substr(0, 8);
      text += "... (tentativi: ";
      text += std::to_string(rec.n);
      text += ")\n";
    }
  }

  return text;
}

}  // namespace

void sendOpenEventsSummary(const std::vector<AuthorizedUser>& users) {
  std::vector<OpenEvent> openEvents = detectOpenEvents();

  for (const auto& user : users) {
    auto state = loadNotificationState(user.chatId);
    std::vector<NotificationRecord> nearAbandonment;
    for (const auto& entry : state) {
      const NotificationRecord& rec = entry.second;
      if (rec.state == NotifyState::PENDING && isNearAbandonment(rec.n, kMaxRetries)) {
        nearAbandonment.push_back(rec);
      }
    }

    std::string text = buildSummaryText(openEvents, nearAbandonment);
    if (text.empty()) continue;  // nulla da segnalare a questo utente

    if (user.admin) {
      std::vector<InlineButton> buttons;
      for (const auto& ev : openEvents) {
        const EventTypeConfig* cfg = findEventTypeConfig(ev.type);
        std::string label = std::string("Chiudi: ") + (cfg ? cfg->label : "Evento");
        buttons.push_back({label, closeEventCallbackData(ev.id)});
      }
      sendMessageWithButtons(user.chatId, text.c_str(), buttons);
    } else {
      sendTelegramMessage(user.chatId, text.c_str());
    }
  }
}
