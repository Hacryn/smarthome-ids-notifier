#include "NotificationEngine.h"

#include <string.h>

#include <map>
#include <string>

#include "../config/GlobalConfigStorage.h"
#include "../config/TimestampFormatter.h"
#include "../config/UserConfig.h"
#include "../config/UserConfigStorage.h"
#include "../diagnostics/SerialLog.h"
#include "../events/EventAggregator.h"
#include "../events/EventLogStorage.h"
#include "../rotation/RotationEngine.h"
#include "../telegram/RateLimiter.h"
#include "../telegram/TelegramClient.h"
#include "NotificationFolder.h"
#include "NotificationLogStorage.h"
#include "NotificationMessageText.h"
#include "NotificationPresentation.h"
#include "RetryTimer.h"

namespace {

struct OutboundMessage {
  int64_t chatId = 0;
  std::string text;
  bool trackDelivery = false;  // if true, an outcome other than success updates sec. 7
  bool isScanMessage = false;  // if true, counts toward scan-completion tracking (sec. 6.3.1)
  char eventId[33] = {0};
  NotifyStatus notifyStatus = NotifyStatus::NOTIFIED_INSTANT;
  uint32_t eventTs = 0;
  uint32_t attemptCountBefore = 0;
};

std::vector<OutboundMessage> g_queue;
RateLimiter g_rateLimiter;
RetryTimer g_retryTimer;

uint32_t g_scanMessagesInFlight = 0;
bool g_scanHadPendingResult = false;
std::string g_lastSystemError;  // sec. 12.2 - exposed in /status

void setEventId(OutboundMessage& msg, const char* id) {
  memcpy(msg.eventId, id, 32);
  msg.eventId[32] = '\0';
}

// Sec. 6.7 - groups by id, so a pending START+END pair ends up on a single
// row with the duration, as in the document's example (same pairing logic
// already written for /log in EventAggregator, rewritten here because it
// operates on NotificationRecord/NotifyStatus - sec. 7.2 - instead of
// log.jsonl's EventRecord/EventStatus).
std::string buildAggregatedMessageText(const std::vector<NotificationRecord>& pending,
                                        const UserConfig& userCfg) {
  struct Group {
    const NotificationRecord* start = nullptr;
    const NotificationRecord* end = nullptr;
    const NotificationRecord* instant = nullptr;
  };

  std::vector<std::string> order;  // first appearance, for stable rendering
  std::map<std::string, Group> groups;

  for (const auto& rec : pending) {
    std::string id(rec.id);
    if (groups.find(id) == groups.end()) order.push_back(id);

    Group& g = groups[id];
    if (rec.status == NotifyStatus::NOTIFIED_START) {
      g.start = &rec;
    } else if (rec.status == NotifyStatus::NOTIFIED_END) {
      g.end = &rec;
    } else {
      g.instant = &rec;
    }
  }

  std::string text = "[recuperate] " + std::to_string(pending.size()) + " notifiche:\n";
  for (const auto& id : order) {
    const Group& g = groups[id];
    const NotificationRecord* anyRec = g.start ? g.start : (g.end ? g.end : g.instant);
    NotifyStatus lookupStatus = anyRec->status;

    EventRecord original{};
    bool found = findEventRecordById(anyRec->id, notifyStatusToEventStatus(lookupStatus), original);
    const char* label = "Evento";
    const char* emoji = "";
    bool approx = true;
    if (found) {
      const EventTypeConfig* cfg = findEventTypeConfig(original.type);
      if (cfg) {
        label = cfg->label;
        emoji = cfg->emoji;
      }
      approx = original.approx;
    }

    text += "- ";
    text += emoji;
    text += " ";
    text += label;
    text += " ";
    if (g.instant) {
      text += formatTimestampForUser(g.instant->ts, userCfg, approx);
    } else if (g.start && g.end) {
      text += formatTimestampForUser(g.start->ts, userCfg, approx) + " -> " +
              formatTimestampForUser(g.end->ts, userCfg, approx) + " (" +
              formatDurationSeconds(g.end->ts - g.start->ts) + ")";
    } else if (g.start) {
      text += formatTimestampForUser(g.start->ts, userCfg, approx) + " -> APERTO";
    } else {
      text += "-> " + formatTimestampForUser(g.end->ts, userCfg, approx);
    }
    text += "\n";
  }
  return text;
}

const char* outcomeLabel(SendOutcomeCategory outcome) {
  switch (outcome) {
    case SendOutcomeCategory::SUCCESS:
      return "success";
    case SendOutcomeCategory::TRANSIENT_NETWORK:
      return "transient_network";
    case SendOutcomeCategory::TRANSIENT_SERVER:
      return "transient_server";
    case SendOutcomeCategory::THROTTLING:
      return "throttling";
    case SendOutcomeCategory::PERMANENT_RECIPIENT:
      return "permanent_recipient";
    case SendOutcomeCategory::SYSTEM_ERROR:
      return "system_error";
  }
  return "unknown";
}

void writeNotificationState(const OutboundMessage& msg, NotifyState state, uint32_t n,
                             uint32_t nowEpoch) {
  // Sec. 9.4 - in degraded mode (space >= 95%) non-essential writes
  // (PENDING rows) are suspended; the event log keeps write priority.
  if (state == NotifyState::PENDING && isFilesystemDegraded()) return;

  NotificationRecord rec{};
  memcpy(rec.id, msg.eventId, 32);
  rec.id[32] = '\0';
  rec.status = msg.notifyStatus;
  rec.ts = (state == NotifyState::PENDING) ? msg.eventTs : nowEpoch;  // sec. 7.2
  rec.state = state;
  rec.n = n;
  appendNotificationRecord(msg.chatId, rec);
}

// Returns true if this outcome should trigger an early scan (sec. 6.3):
// only a success in the normal flow, never during a scan.
bool handleSendOutcome(const std::vector<AuthorizedUser>& users, const OutboundMessage& msg,
                        SendOutcomeCategory outcome, uint32_t nowMillis, uint32_t nowEpoch) {
  bool triggerScan = false;
  bool stillPending = false;

  if (msg.trackDelivery) {
    if (outcome == SendOutcomeCategory::SUCCESS) {
      // Sec. 7.2 - no row if the success is on the first (and only) attempt.
      if (msg.attemptCountBefore > 0) {
        writeNotificationState(msg, NotifyState::RESOLVED, 0, nowEpoch);
      }
      if (!msg.isScanMessage && g_retryTimer.onNormalFlowSuccess()) {
        triggerScan = true;
      }
    } else if (outcome == SendOutcomeCategory::PERMANENT_RECIPIENT) {
      writeNotificationState(msg, NotifyState::ABANDONED, msg.attemptCountBefore + 1, nowEpoch);
      notifyAdmins(users, "Notifica abbandonata (destinatario irraggiungibile): chat_id=" +
                               std::to_string(msg.chatId));
    } else {
      if (outcome == SendOutcomeCategory::SYSTEM_ERROR) {
        g_lastSystemError = "Errore di sistema nell'invio (es. token non valido)";
      }

      uint32_t newCount = msg.attemptCountBefore + 1;
      if (exceedsMaxRetries(newCount, globalConfig().maxRetries)) {
        writeNotificationState(msg, NotifyState::ABANDONED, newCount, nowEpoch);
      } else {
        writeNotificationState(msg, NotifyState::PENDING, newCount, nowEpoch);
        stillPending = true;
        if (!msg.isScanMessage) {
          g_retryTimer.onTransientFailure(nowMillis, globalConfig().retryIntervalMinutes * 60000UL);
        }
      }
    }
  }

  if (msg.isScanMessage) {
    if (stillPending) g_scanHadPendingResult = true;
    if (g_scanMessagesInFlight > 0) g_scanMessagesInFlight--;
  }

  return triggerScan;
}

}  // namespace

void notifyAdmins(const std::vector<AuthorizedUser>& users, const std::string& text) {
  for (const auto& user : users) {
    if (!user.admin) continue;
    OutboundMessage msg;
    msg.chatId = user.chatId;
    msg.text = text;
    g_queue.push_back(msg);
  }
}

bool isRetryTimerActive() { return g_retryTimer.isActive(); }

std::string lastSystemError() { return g_lastSystemError; }

void notifyEvent(const std::vector<AuthorizedUser>& users, const char* id, EventType type,
                  EventStatus status, uint32_t eventTs, bool eventApprox) {
  const EventTypeConfig* cfg = findEventTypeConfig(type);
  const char* label = cfg ? cfg->label : "Evento";
  const char* emoji = cfg ? cfg->emoji : "";
  NotifyStatus notifyStatus = eventStatusToNotifyStatus(status);

  std::vector<UserConfig> userConfigs;
  loadAllUserConfigs(userConfigs);

  for (const auto& user : users) {
    if (user.addedTs > eventTs) continue;  // sec. 4.6

    UserConfig userCfg = findOrDefaultUserConfig(userConfigs, user.chatId);
    if (!isNotifyEnabledForUser(userCfg, type)) continue;  // sec. 11.2

    std::string formattedTs = formatTimestampForUser(eventTs, userCfg, eventApprox);

    OutboundMessage msg;
    msg.chatId = user.chatId;
    msg.text = buildEventMessageText(emoji, label, status, formattedTs);
    msg.trackDelivery = true;
    setEventId(msg, id);
    msg.notifyStatus = notifyStatus;
    msg.eventTs = eventTs;
    msg.attemptCountBefore = 0;
    g_queue.push_back(msg);
  }
}

void runRecoveryScan(const std::vector<AuthorizedUser>& users, uint32_t nowMillis,
                      uint32_t nowEpoch) {
  if (g_retryTimer.scanInProgress()) return;  // sec. 6.3.1 - request discarded

  std::vector<UserConfig> userConfigs;
  loadAllUserConfigs(userConfigs);

  std::vector<OutboundMessage> scanMessages;

  for (const auto& user : users) {
    std::vector<NotificationRecord> pending = pendingFrom(loadNotificationState(user.chatId));
    if (pending.empty()) continue;

    UserConfig userCfg = findOrDefaultUserConfig(userConfigs, user.chatId);

    if (shouldAggregate(pending.size(), globalConfig().aggregateThreshold)) {
      // Sec. 6.7 - a single message; tracking stays individual per entry.
      std::string text = buildAggregatedMessageText(pending, userCfg);
      for (const auto& rec : pending) {
        OutboundMessage msg;
        msg.chatId = user.chatId;
        msg.text = text;
        msg.trackDelivery = true;
        msg.isScanMessage = true;
        setEventId(msg, rec.id);
        msg.notifyStatus = rec.status;
        msg.eventTs = rec.ts;
        msg.attemptCountBefore = rec.n;
        scanMessages.push_back(msg);
      }
    } else {
      for (const auto& rec : pending) {
        EventRecord original{};
        bool found = findEventRecordById(rec.id, notifyStatusToEventStatus(rec.status), original);
        bool approx = found ? original.approx : true;  // not found -> err on the side of caution
        const char* label = "Evento";
        const char* emoji = "";
        if (found) {
          const EventTypeConfig* cfg = findEventTypeConfig(original.type);
          if (cfg) {
            label = cfg->label;
            emoji = cfg->emoji;
          }
        }
        RecoveryPresentation pres =
            decideRecoveryPresentation(nowEpoch, rec.ts, approx, globalConfig().gracePeriodSec);
        std::string formattedTs = formatTimestampForUser(rec.ts, userCfg, pres.isApprox);

        OutboundMessage msg;
        msg.chatId = user.chatId;
        msg.text = buildRecoveryMessageText(emoji, label, formattedTs, pres.isRecovered);
        msg.trackDelivery = true;
        msg.isScanMessage = true;
        setEventId(msg, rec.id);
        msg.notifyStatus = rec.status;
        msg.eventTs = rec.ts;
        msg.attemptCountBefore = rec.n;
        scanMessages.push_back(msg);
      }
    }
  }

  if (scanMessages.empty()) return;  // nothing to recover: the timer stays as-is

  g_retryTimer.beginScan();
  g_scanMessagesInFlight = static_cast<uint32_t>(scanMessages.size());
  g_scanHadPendingResult = false;
  for (auto& msg : scanMessages) g_queue.push_back(msg);
}

void tickNotificationEngine(const std::vector<AuthorizedUser>& users, uint32_t nowMillis,
                             uint32_t nowEpoch) {
  if (!g_queue.empty() && g_rateLimiter.tryConsume(nowMillis)) {
    OutboundMessage msg = g_queue.front();
    g_queue.erase(g_queue.begin());

    SendOutcomeCategory outcome = sendTelegramMessage(msg.chatId, msg.text.c_str());
    logInfo("Send outcome: chat_id=%lld outcome=%s", static_cast<long long>(msg.chatId),
            outcomeLabel(outcome));
    bool triggerScan = handleSendOutcome(users, msg, outcome, nowMillis, nowEpoch);

    if (msg.isScanMessage && g_scanMessagesInFlight == 0 && g_retryTimer.scanInProgress()) {
      g_retryTimer.endScan(!g_scanHadPendingResult, nowMillis,
                           globalConfig().retryIntervalMinutes * 60000UL);
    }

    if (triggerScan) {
      runRecoveryScan(users, nowMillis, nowEpoch);
    }
  }

  if (g_retryTimer.isDue(nowMillis)) {
    runRecoveryScan(users, nowMillis, nowEpoch);
  }
}
