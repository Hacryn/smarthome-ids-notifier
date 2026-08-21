#include "NotificationEngine.h"

#include <string.h>

#include <string>

#include "../config/TimestampFormatter.h"
#include "../config/UserConfig.h"
#include "../config/UserConfigStorage.h"
#include "../events/EventLogStorage.h"
#include "../rotation/RotationEngine.h"
#include "../telegram/RateLimiter.h"
#include "../telegram/TelegramClient.h"
#include "NotificationFolder.h"
#include "NotificationLogStorage.h"
#include "NotificationPresentation.h"
#include "RetryTimer.h"

namespace {

struct OutboundMessage {
  int64_t chatId = 0;
  std::string text;
  bool trackDelivery = false;  // se true, un esito diverso da successo aggiorna sez. 7
  bool isScanMessage = false;  // se true, partecipa al conteggio di fine scansione (sez. 6.3.1)
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

void setEventId(OutboundMessage& msg, const char* id) {
  memcpy(msg.eventId, id, 32);
  msg.eventId[32] = '\0';
}

// NOTA: l'aggregazione START+END con durata come nell'esempio di sez. 6.7
// non e' replicata qui - quella logica arriva con /log (sez. 12.1), che
// la implementera' una sola volta e verra' riusata.
std::string buildEventMessageText(const char* label, EventStatus status,
                                   const std::string& formattedTs) {
  std::string text = label;
  if (status == EventStatus::START) {
    text += " - inizio";
  } else if (status == EventStatus::END) {
    text += " - fine";
  }
  text += " (" + formattedTs + ")";
  return text;
}

std::string buildRecoveryMessageText(const char* label, const std::string& formattedTs,
                                      bool isRecovered) {
  std::string text;
  if (isRecovered) text += "[recuperata] ";  // sez. 6.4
  text += label;
  text += " (" + formattedTs + ")";
  return text;
}

std::string buildAggregatedMessageText(const std::vector<NotificationRecord>& pending,
                                        const UserConfig& userCfg) {
  std::string text = "[recuperate] " + std::to_string(pending.size()) + " notifiche:\n";
  for (const auto& rec : pending) {
    EventRecord original{};
    bool found = findEventRecordById(rec.id, notifyStatusToEventStatus(rec.status), original);
    const char* label = "Evento";
    bool approx = true;
    if (found) {
      const EventTypeConfig* cfg = findEventTypeConfig(original.type);
      if (cfg) label = cfg->label;
      approx = original.approx;
    }
    text += "- ";
    text += label;
    text += " (" + formatTimestampForUser(rec.ts, userCfg, approx) + ")\n";
  }
  return text;
}

void writeNotificationState(const OutboundMessage& msg, NotifyState state, uint32_t n,
                             uint32_t nowEpoch) {
  // Sez. 9.4 - in modalita' degradata (spazio >= 95%) le scritture non
  // essenziali (righe PENDING) vengono sospese; il log eventi resta prioritario.
  if (state == NotifyState::PENDING && isFilesystemDegraded()) return;

  NotificationRecord rec{};
  memcpy(rec.id, msg.eventId, 32);
  rec.id[32] = '\0';
  rec.status = msg.notifyStatus;
  rec.ts = (state == NotifyState::PENDING) ? msg.eventTs : nowEpoch;  // sez. 7.2
  rec.state = state;
  rec.n = n;
  appendNotificationRecord(msg.chatId, rec);
}

// Ritorna true se questo esito deve scatenare una scansione anticipata
// (sez. 6.3): solo un successo nel flusso normale, mai durante una scansione.
bool handleSendOutcome(const std::vector<AuthorizedUser>& users, const OutboundMessage& msg,
                        SendOutcomeCategory outcome, uint32_t nowMillis, uint32_t nowEpoch) {
  bool triggerScan = false;
  bool stillPending = false;

  if (msg.trackDelivery) {
    if (outcome == SendOutcomeCategory::SUCCESS) {
      // Sez. 7.2 - nessuna riga se il successo e' al primo (e unico) tentativo.
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
      uint32_t newCount = msg.attemptCountBefore + 1;
      if (exceedsMaxRetries(newCount, kMaxRetries)) {
        writeNotificationState(msg, NotifyState::ABANDONED, newCount, nowEpoch);
      } else {
        writeNotificationState(msg, NotifyState::PENDING, newCount, nowEpoch);
        stillPending = true;
        if (!msg.isScanMessage) {
          g_retryTimer.onTransientFailure(nowMillis, kRetryIntervalMs);
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

void notifyEvent(const std::vector<AuthorizedUser>& users, const char* id, EventType type,
                  EventStatus status, uint32_t eventTs, bool eventApprox) {
  const EventTypeConfig* cfg = findEventTypeConfig(type);
  const char* label = cfg ? cfg->label : "Evento";
  NotifyStatus notifyStatus = eventStatusToNotifyStatus(status);

  std::vector<UserConfig> userConfigs;
  loadAllUserConfigs(userConfigs);

  for (const auto& user : users) {
    if (user.addedTs > eventTs) continue;  // sez. 4.6

    UserConfig userCfg = findOrDefaultUserConfig(userConfigs, user.chatId);
    if (!isNotifyEnabledForUser(userCfg, type)) continue;  // sez. 11.2

    std::string formattedTs = formatTimestampForUser(eventTs, userCfg, eventApprox);

    OutboundMessage msg;
    msg.chatId = user.chatId;
    msg.text = buildEventMessageText(label, status, formattedTs);
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
  if (g_retryTimer.scanInProgress()) return;  // sez. 6.3.1 - richiesta scartata

  std::vector<UserConfig> userConfigs;
  loadAllUserConfigs(userConfigs);

  std::vector<OutboundMessage> scanMessages;

  for (const auto& user : users) {
    std::vector<NotificationRecord> pending = pendingFrom(loadNotificationState(user.chatId));
    if (pending.empty()) continue;

    UserConfig userCfg = findOrDefaultUserConfig(userConfigs, user.chatId);

    if (shouldAggregate(pending.size(), kAggregateThreshold)) {
      // Sez. 6.7 - un unico messaggio; il tracciamento resta individuale per voce.
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
        bool approx = found ? original.approx : true;  // non trovato -> prudenza
        const char* label = "Evento";
        if (found) {
          const EventTypeConfig* cfg = findEventTypeConfig(original.type);
          if (cfg) label = cfg->label;
        }
        RecoveryPresentation pres = decideRecoveryPresentation(nowEpoch, rec.ts, approx, kGracePeriodSec);
        std::string formattedTs = formatTimestampForUser(rec.ts, userCfg, pres.isApprox);

        OutboundMessage msg;
        msg.chatId = user.chatId;
        msg.text = buildRecoveryMessageText(label, formattedTs, pres.isRecovered);
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

  if (scanMessages.empty()) return;  // niente da recuperare: il timer resta com'era

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
    bool triggerScan = handleSendOutcome(users, msg, outcome, nowMillis, nowEpoch);

    if (msg.isScanMessage && g_scanMessagesInFlight == 0 && g_retryTimer.scanInProgress()) {
      g_retryTimer.endScan(!g_scanHadPendingResult, nowMillis, kRetryIntervalMs);
    }

    if (triggerScan) {
      runRecoveryScan(users, nowMillis, nowEpoch);
    }
  }

  if (g_retryTimer.isDue(nowMillis)) {
    runRecoveryScan(users, nowMillis, nowEpoch);
  }
}
