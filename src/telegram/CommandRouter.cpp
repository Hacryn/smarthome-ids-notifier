#include "CommandRouter.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <esp_system.h>

#include <string>
#include <vector>

#include "../config/GlobalConfigStorage.h"
#include "../config/TimestampFormatter.h"
#include "../config/TimezonePresets.h"
#include "../config/UserConfig.h"
#include "../config/UserConfigStorage.h"
#include "../diagnostics/SerialLog.h"
#include "../events/EventAggregator.h"
#include "../events/EventLogStorage.h"
#include "../events/OpenEventsManager.h"
#include "../network/WifiManager.h"
#include "../notifications/NotificationEngine.h"
#include "../notifications/NotificationFolder.h"
#include "../notifications/NotificationLogStorage.h"
#include "../pins/PinMonitor.h"
#include "../rotation/FilesystemHealth.h"
#include "../rotation/FsErrorCounter.h"
#include "../rotation/RotationEngine.h"
#include "../rotation/RotationStorage.h"
#include "../time/Clock.h"
#include "../users/UserStorage.h"
#include "CommandParser.h"

namespace {

std::vector<AuthorizedUser>* g_users = nullptr;
uint32_t* g_lastWrittenTs = nullptr;

uint32_t nowEpoch() { return currentEpoch(); }

void reply(int64_t chatId, const std::string& text) { sendTelegramMessage(chatId, text.c_str()); }

void upsertUserConfig(std::vector<UserConfig>& configs, const UserConfig& updated) {
  for (auto& c : configs) {
    if (c.chatId == updated.chatId) {
      c = updated;
      return;
    }
  }
  configs.push_back(updated);
}

// Sec. 11.2 - load, apply the change, save. Shared by /notify,
// /setdateformat, /settimezone.
template <typename Mutator>
void modifyUserConfig(int64_t chatId, Mutator mutate) {
  std::vector<UserConfig> configs;
  loadAllUserConfigs(configs);
  UserConfig cfg = findOrDefaultUserConfig(configs, chatId);
  mutate(cfg);
  upsertUserConfig(configs, cfg);
  saveAllUserConfigs(configs);
}

const char* resetReasonText(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
      return "accensione";
    case ESP_RST_SW:
      return "riavvio software";
    case ESP_RST_PANIC:
      return "panic/crash";
    case ESP_RST_INT_WDT:
      return "watchdog (interrupt)";
    case ESP_RST_TASK_WDT:
      return "watchdog (task)";
    case ESP_RST_WDT:
      return "watchdog";
    case ESP_RST_BROWNOUT:
      return "brownout (alimentazione)";
    default:
      return "sconosciuta";
  }
}

// Sec. 4.3 - admin commands silently ignore only unauthorized senders
// (already filtered upstream); a user who's authorized but not an admin
// gets an explicit rejection.
bool requireAdmin(int64_t chatId, bool admin) {
  if (!admin) reply(chatId, "Comando riservato agli admin.");
  return admin;
}

void handleCloseEvent(int64_t chatId, bool admin, const std::string& id, bool hasTs, uint32_t ts) {
  if (!requireAdmin(chatId, admin)) return;

  uint32_t closeTs = hasTs ? ts : nowEpoch();
  bool closed = closeOpenEvent(*g_users, id, closeTs, *g_lastWrittenTs);
  reply(chatId, closed ? "Evento chiuso." : "Evento gia' chiuso o non trovato.");
}

void handleAddUser(int64_t chatId, bool admin, int64_t targetId) {
  if (!requireAdmin(chatId, admin)) return;

  bool added = addUser(*g_users, targetId, false, nowEpoch());
  if (added) saveUsers(*g_users);
  reply(chatId, added ? "Utente aggiunto." : "Utente gia' presente.");
}

void handleRemoveUser(int64_t chatId, bool admin, int64_t targetId) {
  if (!requireAdmin(chatId, admin)) return;

  bool removed = removeUser(*g_users, targetId);
  if (removed) saveUsers(*g_users);
  reply(chatId, removed ? "Utente rimosso." : "Utente non trovato.");
}

void handlePromoteUser(int64_t chatId, bool admin, int64_t targetId) {
  if (!requireAdmin(chatId, admin)) return;

  bool ok = setAdminFlag(*g_users, targetId, true);
  if (ok) saveUsers(*g_users);
  reply(chatId, ok ? "Utente promosso ad admin." : "Utente non trovato.");
}

void handleResetUsers(int64_t chatId, bool admin) {
  if (!requireAdmin(chatId, admin)) return;

  resetUsers(*g_users);
  saveUsers(*g_users);
  reply(chatId, "Whitelist azzerata. Al prossimo riavvio l'admin di secrets.h verra' ripristinato.");
}

void handleNotify(int64_t chatId, const std::string& typeName, bool enabled) {
  const EventTypeConfig* cfg = findEventTypeConfigByCommandName(typeName.c_str());
  if (!cfg) {
    reply(chatId, "Tipo di evento non riconosciuto.");
    return;
  }

  modifyUserConfig(chatId, [&](UserConfig& c) { setNotifyEnabled(c, cfg->type, enabled); });
  reply(chatId, std::string(cfg->label) + (enabled ? " abilitato." : " disabilitato."));
}

void handleSetDateFormat(int64_t chatId, const std::string& format) {
  modifyUserConfig(chatId, [&](UserConfig& c) { c.dateFormat = format; });
  reply(chatId, "Formato data aggiornato.");
}

void handleSetTimezone(int64_t chatId, const std::string& presetName) {
  const TimezonePresetInfo* preset = findTimezonePresetByName(presetName.c_str());
  if (!preset) {
    reply(chatId, "Fuso orario non riconosciuto.");
    return;
  }

  modifyUserConfig(chatId, [&](UserConfig& c) { c.timezone = preset->preset; });
  reply(chatId, std::string("Fuso orario impostato: ") + preset->name);
}

void handleSetGlobalUint(int64_t chatId, bool admin, uint32_t GlobalConfig::*field, uint32_t value,
                          const char* label) {
  if (!requireAdmin(chatId, admin)) return;

  globalConfig().*field = value;
  saveGlobalConfig(globalConfig());
  reply(chatId, std::string(label) + " impostato a " + std::to_string(value) + ".");
}

void handleLog(int64_t chatId, bool hasArg, uint32_t n) {
  constexpr uint32_t kDefaultLogEntries = 10;
  constexpr uint32_t kMaxLogEntries = 50;  // sec. 12.1 - imposed hard cap

  uint32_t count = hasArg ? n : kDefaultLogEntries;
  if (count == 0) count = 1;
  if (count > kMaxLogEntries) count = kMaxLogEntries;

  AggregatedEventLog aggregator(count);
  File f = LittleFS.open(kEventLogPath, "r");
  if (f) {
    while (f.available()) {
      String line = f.readStringUntil('\n');
      if (line.length() == 0) continue;
      EventRecord rec{};
      if (parseEventRecord(std::string(line.c_str()), rec)) aggregator.observe(rec);
    }
    f.close();
  }

  if (aggregator.events().empty()) {
    reply(chatId, "Nessun evento nel registro.");
    return;
  }

  std::vector<UserConfig> configs;
  loadAllUserConfigs(configs);
  UserConfig userCfg = findOrDefaultUserConfig(configs, chatId);

  std::string text;
  for (auto it = aggregator.events().rbegin(); it != aggregator.events().rend(); ++it) {
    const AggregatedEvent& ev = *it;
    const EventTypeConfig* cfg = findEventTypeConfig(ev.type);
    text += cfg ? cfg->label : "Evento";
    text += "  ";
    text += formatTimestampForUser(ev.startTs, userCfg, ev.startApprox);

    if (!ev.isInstant) {
      text += " -> ";
      if (ev.hasEnd) {
        text += formatTimestampForUser(ev.endTs, userCfg, ev.endApprox);
        text += " (" + formatDurationSeconds(ev.endTs - ev.startTs) + ")";
      } else {
        text += "APERTO";
      }
    }
    text += "\n";
  }
  reply(chatId, text);
}

void handleStatus(int64_t chatId) {
  std::string text;

  text += "Uptime: " + std::to_string(millis() / 1000) + "s, causa ultimo riavvio: ";
  text += resetReasonText(esp_reset_reason());
  text += "\n";

  text += "Stato pin:\n";
  for (size_t i = 0; i < EVENT_TYPES_COUNT; i++) {
    const EventTypeConfig& cfg = EVENT_TYPES[i];
    if (!cfg.enabled || cfg.pin < 0) continue;
    text += "- ";
    text += cfg.label;
    text += ": ";
    text += digitalRead(cfg.pin) ? "HIGH" : "LOW";
    text += "\n";
  }

  text += "WiFi: ";
  if (isWifiConnected()) {
    text += "connesso a " + wifiSsid() + ", RSSI " + std::to_string(wifiRssi()) + " dBm";
  } else {
    text += "disconnesso, tentativo di backoff #" + std::to_string(wifiCurrentBackoffAttempt());
  }
  text += "\n";

  text += "Sincronizzazione oraria: ";
  text += isTimeSynced() ? "NTP sincronizzato" : "non sincronizzata, orario stimato dall'ancora NVS";
  text += "\n";

  std::vector<OpenEvent> open = detectOpenEvents();
  text += "Eventi aperti: " + std::to_string(open.size()) + "\n";

  uint32_t pendingCount = 0;
  uint32_t abandonedCount = 0;
  for (const auto& user : *g_users) {
    auto state = loadNotificationState(user.chatId);
    for (const auto& entry : state) {
      if (entry.second.state == NotifyState::PENDING) {
        pendingCount++;
      } else if (entry.second.state == NotifyState::ABANDONED) {
        abandonedCount++;
      }
    }
  }
  text += "Notifiche pendenti: " + std::to_string(pendingCount) +
          ", abbandonate: " + std::to_string(abandonedCount) +
          ", timer di retry attivo: " + (isRetryTimerActive() ? "si" : "no") + "\n";

  text += "LittleFS: " + std::to_string(LittleFS.usedBytes()) + "/" +
          std::to_string(LittleFS.totalBytes()) +
          " byte, errori di scrittura: " + std::to_string(fsErrorCounter().count());
  if (isFilesystemDegraded()) text += " (MODALITA' DEGRADATA)";
  text += "\n";
  text += "Diagnostica LittleFS: ";
  text += filesystemHealthText();
  text += "\n";

  text += "Overflow coda interrupt pin: " + std::to_string(pinQueueOverflowCount()) + "\n";

  std::string sysErr = lastSystemError();
  if (!sysErr.empty()) text += "Ultimo errore di sistema: " + sysErr + "\n";

  uint32_t lastRotation = loadLastRotationEpoch();
  text += "Ultima rotazione: " + (lastRotation == 0 ? std::string("mai eseguita")
                                                     : std::to_string(lastRotation));

  reply(chatId, text);
}

void handleConfig(int64_t chatId, bool admin) {
  std::vector<UserConfig> configs;
  loadAllUserConfigs(configs);
  UserConfig cfg = findOrDefaultUserConfig(configs, chatId);
  const TimezonePresetInfo* tz = findTimezonePreset(cfg.timezone);

  std::string text = "La tua configurazione:\n";
  text += "- Formato data: " + cfg.dateFormat + "\n";
  text += "- Fuso orario: " + std::string(tz ? tz->name : "?") + "\n";
  text += "- Notifiche disabilitate: ";
  if (cfg.disabledTypes.empty()) {
    text += "nessuna";
  } else {
    for (EventType t : cfg.disabledTypes) {
      const EventTypeConfig* tCfg = findEventTypeConfig(t);
      text += (tCfg ? tCfg->commandName : "?");
      text += " ";
    }
  }
  text += "\n";

  if (admin) {
    const GlobalConfig& g = globalConfig();
    text += "\nConfigurazione globale:\n";
    text += "- Retention: " + std::to_string(g.retentionWeeks) + " settimane\n";
    text += "- Grace period: " + std::to_string(g.gracePeriodSec) + " secondi\n";
    text += "- Intervallo retry: " + std::to_string(g.retryIntervalMinutes) + " minuti\n";
    text += "- Tentativi massimi: " + std::to_string(g.maxRetries) + "\n";
    text += "- Soglia problema di rete: " + std::to_string(g.networkIssueThresholdSec) +
            " secondi\n";
    text += "- Soglia aggregazione: " + std::to_string(g.aggregateThreshold) + "\n";
    text += "- Intervallo persistenza ancora NTP: " +
            std::to_string(g.anchorPersistIntervalMinutes) + " minuti";
  }

  reply(chatId, text);
}

}  // namespace

void initCommandRouter(std::vector<AuthorizedUser>& users, uint32_t& lastWrittenTs) {
  g_users = &users;
  g_lastWrittenTs = &lastWrittenTs;
}

void handleIncomingCommand(const IncomingCommand& cmd) {
  if (g_users == nullptr || !isAuthorized(*g_users, cmd.chatId)) {  // sec. 4.2
    logWarn("Command rejected (not whitelisted): chat_id=%lld", static_cast<long long>(cmd.chatId));
    return;
  }

  bool admin = isAdmin(*g_users, cmd.chatId);
  logInfo("Command received: chat_id=%lld admin=%d text=%s", static_cast<long long>(cmd.chatId),
          admin, cmd.text.c_str());

  std::string id, strArg;
  bool hasTs = false, hasArg = false, boolArg = false;
  uint32_t uintArg = 0, tsArg = 0;
  int64_t int64Arg = 0;

  if (parseCloseEventCommand(cmd.text, id, hasTs, tsArg)) {
    handleCloseEvent(cmd.chatId, admin, id, hasTs, tsArg);
  } else if (parseSingleInt64Command(cmd.text, "/adduser", int64Arg)) {
    handleAddUser(cmd.chatId, admin, int64Arg);
  } else if (parseSingleInt64Command(cmd.text, "/removeuser", int64Arg)) {
    handleRemoveUser(cmd.chatId, admin, int64Arg);
  } else if (parseSingleInt64Command(cmd.text, "/promoteuser", int64Arg)) {
    handlePromoteUser(cmd.chatId, admin, int64Arg);
  } else if (parseResetUsersCommand(cmd.text)) {
    handleResetUsers(cmd.chatId, admin);
  } else if (parseNotifyCommand(cmd.text, strArg, boolArg)) {
    handleNotify(cmd.chatId, strArg, boolArg);
  } else if (parseSetDateFormatCommand(cmd.text, strArg)) {
    handleSetDateFormat(cmd.chatId, strArg);
  } else if (parseSetTimezoneCommand(cmd.text, strArg)) {
    handleSetTimezone(cmd.chatId, strArg);
  } else if (parseSingleUintCommand(cmd.text, "/setretention", uintArg)) {
    handleSetGlobalUint(cmd.chatId, admin, &GlobalConfig::retentionWeeks, uintArg,
                         "Periodo di retention (settimane)");
  } else if (parseSingleUintCommand(cmd.text, "/setgraceperiod", uintArg)) {
    handleSetGlobalUint(cmd.chatId, admin, &GlobalConfig::gracePeriodSec, uintArg,
                         "Grace period (secondi)");
  } else if (parseSingleUintCommand(cmd.text, "/setretryinterval", uintArg)) {
    handleSetGlobalUint(cmd.chatId, admin, &GlobalConfig::retryIntervalMinutes, uintArg,
                         "Intervallo di retry (minuti)");
  } else if (parseSingleUintCommand(cmd.text, "/setmaxretries", uintArg)) {
    handleSetGlobalUint(cmd.chatId, admin, &GlobalConfig::maxRetries, uintArg,
                         "Numero massimo di tentativi");
  } else if (parseSingleUintCommand(cmd.text, "/setnetthreshold", uintArg)) {
    handleSetGlobalUint(cmd.chatId, admin, &GlobalConfig::networkIssueThresholdSec, uintArg,
                         "Soglia problema di rete (secondi)");
  } else if (parseSingleUintCommand(cmd.text, "/setaggregatethreshold", uintArg)) {
    handleSetGlobalUint(cmd.chatId, admin, &GlobalConfig::aggregateThreshold, uintArg,
                         "Soglia di aggregazione");
  } else if (parseSingleUintCommand(cmd.text, "/setanchorinterval", uintArg)) {
    handleSetGlobalUint(cmd.chatId, admin, &GlobalConfig::anchorPersistIntervalMinutes, uintArg,
                         "Intervallo persistenza ancora NTP (minuti)");
  } else if (parseLogCommand(cmd.text, hasArg, uintArg)) {
    handleLog(cmd.chatId, hasArg, uintArg);
  } else if (cmd.text == "/status") {
    handleStatus(cmd.chatId);
  } else if (cmd.text == "/config") {
    handleConfig(cmd.chatId, admin);
  }
  // Unrecognized command: no reply (avoids noise on free-form text).
}
