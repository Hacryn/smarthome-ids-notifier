#include "CommandRouter.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_system.h>

#include <algorithm>
#include <string>
#include <vector>

#include "../config/GlobalConfigStorage.h"
#include "../config/TimestampFormatter.h"
#include "../config/TimezonePresets.h"
#include "../config/UserConfig.h"
#include "../config/UserConfigStorage.h"
#include "../config/Version.h"
#include "../diagnostics/SerialLog.h"
#include "../events/EventAggregator.h"
#include "../events/EventLogStorage.h"
#include "../events/OpenEventsManager.h"
#include "../network/WifiManager.h"
#include "../notifications/NotificationEngine.h"
#include "../notifications/NotificationFolder.h"
#include "../notifications/NotificationLogStorage.h"
#include "../notifications/NotificationRecord.h"
#include "../pins/PinMonitor.h"
#include "../rotation/FilesystemHealth.h"
#include "../rotation/FsErrorCounter.h"
#include "../rotation/RotationEngine.h"
#include "../rotation/RotationStorage.h"
#include "../time/Clock.h"
#include "../users/UserStorage.h"
#include "CommandParser.h"
#include "HelpPaginator.h"
#include "UnauthorizedRequestLog.h"

namespace {

std::vector<AuthorizedUser>* g_users = nullptr;
uint32_t* g_lastWrittenTs = nullptr;

// Sec. 12.4 - RAM-only, never persisted, reset on reboot (see UnauthorizedRequestLog.h).
std::vector<UnauthorizedRequest> g_unauthorizedRequests;

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

// Sec. 12.4 - lists every whitelisted user with their cached username.
void handleListUsers(int64_t chatId, bool admin) {
  if (!requireAdmin(chatId, admin)) return;

  std::string text =
      "\xF0\x9F\x91\xA5 Utenti autorizzati (" + std::to_string(g_users->size()) + "):\n";
  for (const auto& user : *g_users) {
    text += "- " + std::to_string(user.chatId) + " - " + (user.admin ? "admin" : "standard") +
            " - " + (user.username.empty() ? "(username sconosciuto)" : "@" + user.username) +
            "\n";
  }
  reply(chatId, text);
}

// Sec. 12.4 - the RAM-only unauthorized-request buffer, most recent first.
void handleRequests(int64_t chatId, bool admin) {
  if (!requireAdmin(chatId, admin)) return;

  if (g_unauthorizedRequests.empty()) {
    reply(chatId, "\xF0\x9F\x93\xA8 Nessuna richiesta non autorizzata registrata.");
    return;
  }

  std::vector<UserConfig> configs;
  loadAllUserConfigs(configs);
  UserConfig userCfg = findOrDefaultUserConfig(configs, chatId);

  std::string text = "\xF0\x9F\x93\xA8 Ultime richieste non autorizzate (max " +
                      std::to_string(kMaxUnauthorizedRequests) + "):\n";
  for (auto it = g_unauthorizedRequests.rbegin(); it != g_unauthorizedRequests.rend(); ++it) {
    text += "- chat_id=" + std::to_string(it->chatId) + ", user_id=" + it->userId +
            ", username=" + (it->username.empty() ? "(nessuno)" : "@" + it->username) +
            ", alle " + formatTimestampForUser(it->ts, userCfg, false) + "\n";
  }
  reply(chatId, text);
}

// Sec. 12.3 - raw file dump for debugging, sent as a Telegram document.
// target is one of "log"/"notif"/"userconfig"/"users", already validated
// by CommandParser; targetChatId is only meaningful for "notif".
void handleDump(int64_t chatId, bool admin, const std::string& target, int64_t targetChatId) {
  if (!requireAdmin(chatId, admin)) return;

  std::string path, filename;
  if (target == "log") {
    path = kEventLogPath;
    filename = "log.jsonl";
  } else if (target == "notif") {
    path = notificationLogPath(targetChatId);
    filename = "notif_" + std::to_string(targetChatId) + ".jsonl";
  } else if (target == "userconfig") {
    path = kUserConfigPath;
    filename = "userconfig.json";
  } else if (target == "users") {
    path = kUsersPath;
    filename = "users.json";
  } else {
    return;  // unreachable: CommandParser already validated the target
  }

  if (!LittleFS.exists(path.c_str())) {
    reply(chatId, "File non trovato.");
    return;
  }
  if (!sendDocumentFromFile(chatId, path, filename)) reply(chatId, "Invio del file fallito.");
}

// Sec. 12.3 - destructive resets, CONFERMA-protected (same pattern as
// handleResetUsers below).
void handleResetLog(int64_t chatId, bool admin) {
  if (!requireAdmin(chatId, admin)) return;

  bool removed = !LittleFS.exists(kEventLogPath) || LittleFS.remove(kEventLogPath);
  if (removed) *g_lastWrittenTs = 0;  // sec. 5.4.3 - the clamp must forget the old baseline too
  reply(chatId, removed ? "Storico eventi cancellato." : "Cancellazione fallita.");
}

void handleResetNotif(int64_t chatId, bool admin, int64_t targetChatId) {
  if (!requireAdmin(chatId, admin)) return;

  std::string path = notificationLogPath(targetChatId);
  bool removed = !LittleFS.exists(path.c_str()) || LittleFS.remove(path.c_str());
  reply(chatId,
        removed ? "Stato notifiche azzerato per l'utente indicato." : "Cancellazione fallita.");
}

void handleResetUserConfig(int64_t chatId, bool admin, int64_t targetChatId) {
  if (!requireAdmin(chatId, admin)) return;

  std::vector<UserConfig> configs;
  loadAllUserConfigs(configs);
  size_t before = configs.size();
  configs.erase(std::remove_if(configs.begin(), configs.end(),
                                [&](const UserConfig& c) { return c.chatId == targetChatId; }),
                configs.end());
  if (configs.size() == before) {
    reply(chatId, "Nessuna preferenza personalizzata trovata per l'utente indicato.");
    return;
  }
  reply(chatId, saveAllUserConfigs(configs) ? "Preferenze utente ripristinate ai default."
                                             : "Salvataggio fallito.");
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

// Sec. 12.2 - emoji-tagged /status blocks (plan point 4).
constexpr const char* kUptimeEmoji = "\xF0\x9F\x95\x92";     // clock
constexpr const char* kAlarmsEmoji = "\xF0\x9F\x9A\xA8";     // rotating light
constexpr const char* kPowerEmoji = "\xF0\x9F\x94\x8C";      // plug
constexpr const char* kWifiEmoji = "\xF0\x9F\x93\xB6";       // antenna bars
constexpr const char* kTelegramReachEmoji = "\xF0\x9F\x93\xA1";  // satellite antenna
constexpr const char* kNtpEmoji = "\xE2\x8F\xB1\xEF\xB8\x8F";  // stopwatch
constexpr const char* kOpenEventsEmoji = "\xF0\x9F\x93\x82";  // open folder
constexpr const char* kNotifEmoji = "\xF0\x9F\x94\x94";      // bell
constexpr const char* kFsEmoji = "\xF0\x9F\x92\xBE";         // floppy disk
constexpr const char* kRotationEmoji = "\xF0\x9F\x94\x81";   // repeat
constexpr const char* kErrorEmoji = "\xE2\x9A\xA0\xEF\xB8\x8F";  // warning

std::string formatUptime(uint32_t totalSeconds) {
  uint32_t days = totalSeconds / 86400;
  uint32_t hours = (totalSeconds % 86400) / 3600;
  uint32_t minutes = (totalSeconds % 3600) / 60;
  uint32_t seconds = totalSeconds % 60;
  return std::to_string(days) + "g " + std::to_string(hours) + "h " + std::to_string(minutes) +
         "m " + std::to_string(seconds) + "s";
}

// Sec. 8 - "attivo da <ts>" (open event still logged) or "normale", using
// the debounced/logged state instead of the instantaneous pin level.
std::string alarmStateLine(EventType type, const UserConfig& userCfg) {
  const EventTypeConfig* cfg = findEventTypeConfig(type);
  std::string label = cfg ? cfg->label : "Evento";

  OpenEvent open{};
  if (findOpenEventOfType(type, open)) {
    return "- " + label + ": attivo da " + formatTimestampForUser(open.startTs, userCfg, open.approx);
  }
  return "- " + label + ": normale";
}

void handleStatus(int64_t chatId) {
  std::vector<UserConfig> configs;
  loadAllUserConfigs(configs);
  UserConfig userCfg = findOrDefaultUserConfig(configs, chatId);

  std::string text;

  text += kUptimeEmoji;
  text += " Uptime: " + formatUptime(millis() / 1000) + ", causa ultimo riavvio: ";
  text += resetReasonText(esp_reset_reason());
  text += "\n";
  text += "Versione firmware: " + std::string(FIRMWARE_VERSION) + "\n\n";

  text += kAlarmsEmoji;
  text += " Stato allarmi:\n";
  text += alarmStateLine(EventType::ALARM_GENERAL, userCfg) + "\n";
  text += alarmStateLine(EventType::ALARM_INTERNAL, userCfg) + "\n";
  text += alarmStateLine(EventType::ALARM_GARAGE, userCfg) + "\n\n";

  text += kPowerEmoji;
  text += " Rete 230V: ";
  OpenEvent powerOpen{};
  if (findOpenEventOfType(EventType::POWER_LOSS, powerOpen)) {
    text += "assente da " + formatTimestampForUser(powerOpen.startTs, userCfg, powerOpen.approx);
  } else {
    text += "presente";
  }
  text += "\n\n";

  text += kWifiEmoji;
  text += " WiFi: ";
  if (isWifiConnected()) {
    text += "connesso a " + wifiSsid() + ", RSSI " + std::to_string(wifiRssi()) + " dBm, IP " +
            std::string(WiFi.localIP().toString().c_str()) + " (" +
            (isStaticIpActive() ? "statico" : "DHCP") + ")";
  } else {
    text += "disconnesso, tentativo di backoff #" + std::to_string(wifiCurrentBackoffAttempt());
  }
  text += "\n\n";

  text += kTelegramReachEmoji;
  text += " Telegram: ";
  text += isTelegramReachable() ? "raggiungibile"
                                 : "non raggiungibile di recente (ultimi invii falliti per problemi di connessione)";
  text += "\n\n";

  text += kNtpEmoji;
  text += " Sincronizzazione oraria: ";
  text += isTimeSynced() ? "NTP sincronizzato" : "non sincronizzata, orario stimato dall'ancora NVS";
  text += "\n\n";

  std::vector<OpenEvent> open = detectOpenEvents();
  text += kOpenEventsEmoji;
  text += " Eventi aperti: " + std::to_string(open.size()) + "\n\n";

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
  text += kNotifEmoji;
  text += " Notifiche pendenti: " + std::to_string(pendingCount) +
          ", abbandonate: " + std::to_string(abandonedCount) +
          ", timer di retry attivo: " + (isRetryTimerActive() ? "si" : "no") + "\n\n";

  text += kFsEmoji;
  text += " LittleFS: " + std::to_string(LittleFS.usedBytes()) + "/" +
          std::to_string(LittleFS.totalBytes()) +
          " byte, errori di scrittura: " + std::to_string(fsErrorCounter().count());
  if (isFilesystemDegraded()) text += " (MODALITA' DEGRADATA)";
  text += "\n";
  text += "Diagnostica LittleFS: ";
  text += filesystemHealthText();
  text += "\n";
  text += "Overflow coda interrupt pin: " + std::to_string(pinQueueOverflowCount()) + "\n\n";

  text += kRotationEmoji;
  uint32_t lastRotation = loadLastRotationEpoch();
  text += " Ultima rotazione: " +
          (lastRotation == 0 ? std::string("mai eseguita")
                              : formatTimestampForUser(lastRotation, userCfg, false));

  std::string sysErr = lastSystemError();
  if (!sysErr.empty()) {
    text += "\n\n";
    text += kErrorEmoji;
    text += " Ultimo errore di sistema: " + sysErr;
  }

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

// Sec. 14 - one HTML block per command: "<b>syntax</b>\ndescription\nEsempio:
// <code>...</code>" (example line omitted when there's none). Placeholders
// in syntax must already be &lt;/&gt;-escaped by the caller - this is
// static compile-time content, so no runtime HTML escaping is done.
std::string helpBlock(const char* syntax, const char* description, const char* example = nullptr) {
  std::string text = "<b>" + std::string(syntax) + "</b>\n" + description;
  if (example) text += "\nEsempio: <code>" + std::string(example) + "</code>";
  return text;
}

// Sec. 14 - content mirrors docs/commands.md exactly (the single source of
// truth); keep both in sync when a command is added/removed/changed.
std::vector<std::string> buildHelpBlocks(bool admin) {
  std::vector<std::string> blocks = {
      helpBlock("/log [n]", "Mostra gli ultimi eventi registrati.", "/log 20"),
      helpBlock("/status", "Stato del sistema: allarmi, rete, WiFi, storage."),
      helpBlock("/config", "Mostra le tue preferenze e la configurazione."),
      helpBlock("/setdateformat &lt;format&gt;", "Imposta il formato data/ora personale.",
                "/setdateformat %d/%m/%Y %H:%M"),
      helpBlock("/settimezone &lt;preset&gt;", "Imposta il tuo fuso orario.",
                "/settimezone Europe/Rome"),
      helpBlock("/notify &lt;type&gt; on|off",
                "Abilita o disabilita le notifiche per un tipo di evento.",
                "/notify ALARM_GARAGE off"),
      helpBlock("/help", "Mostra questa guida ai comandi disponibili."),
  };

  if (!admin) return blocks;

  blocks.push_back("<b>Gestione whitelist</b>");
  blocks.push_back(
      helpBlock("/adduser &lt;chat_id&gt;", "Aggiunge un utente alla whitelist.", "/adduser 123456789"));
  blocks.push_back(helpBlock("/removeuser &lt;chat_id&gt;", "Rimuove un utente dalla whitelist."));
  blocks.push_back(helpBlock("/promoteuser &lt;chat_id&gt;", "Promuove un utente ad admin."));
  blocks.push_back(helpBlock("/listusers", "Elenca gli utenti autorizzati."));
  blocks.push_back(helpBlock("/resetusers CONFERMA", "Svuota la whitelist (distruttivo)."));
  blocks.push_back(helpBlock("/requests", "Mostra i tentativi di accesso non autorizzati."));

  blocks.push_back("<b>Configurazione globale</b>");
  blocks.push_back(
      helpBlock("/setretention &lt;weeks&gt;", "Imposta la retention di log e notifiche."));
  blocks.push_back(helpBlock("/setgraceperiod &lt;minutes&gt;",
                              "Imposta il grace period di recupero notifiche."));
  blocks.push_back(
      helpBlock("/setretryinterval &lt;minutes&gt;", "Imposta l'intervallo tra i retry."));
  blocks.push_back(helpBlock("/setmaxretries &lt;n&gt;",
                              "Imposta i tentativi massimi prima dell'abbandono."));
  blocks.push_back(
      helpBlock("/setnetthreshold &lt;seconds&gt;", "Imposta la soglia per un problema di rete."));
  blocks.push_back(
      helpBlock("/setaggregatethreshold &lt;n&gt;", "Imposta la soglia di aggregazione notifiche."));
  blocks.push_back(helpBlock("/setanchorinterval &lt;minutes&gt;",
                              "Imposta l'intervallo di persistenza dell'ancora NTP."));

  blocks.push_back("<b>Diagnostica e gestione dati</b>");
  blocks.push_back(helpBlock("/closeevent &lt;id&gt; [timestamp]",
                              "Chiude manualmente un evento aperto."));
  blocks.push_back(helpBlock("/dump &lt;target&gt; [chat_id]",
                              "Invia un file grezzo come documento (debug).", "/dump notif 123456789"));
  blocks.push_back(helpBlock("/resetlog CONFERMA", "Svuota lo storico eventi (distruttivo)."));
  blocks.push_back(helpBlock("/resetnotif &lt;chat_id&gt; CONFERMA",
                              "Azzera lo stato notifiche di un utente (distruttivo)."));
  blocks.push_back(helpBlock("/resetuserconfig &lt;chat_id&gt; CONFERMA",
                              "Azzera le preferenze di un utente (distruttivo)."));

  return blocks;
}

void handleHelp(int64_t chatId, bool admin) {
  constexpr size_t kMaxPageBytes = 3500;  // margin under Telegram's 4096-char limit

  std::vector<std::string> pages = paginateBlocks(buildHelpBlocks(admin), kMaxPageBytes);
  for (size_t i = 0; i < pages.size(); i++) {
    std::string header = "\xF0\x9F\x93\x96 Guida comandi \xE2\x80\x94 pagina " +
                          std::to_string(i + 1) + "/" + std::to_string(pages.size()) + "\n\n";
    sendFormattedMessage(chatId, header + pages[i]);
  }
}

}  // namespace

void initCommandRouter(std::vector<AuthorizedUser>& users, uint32_t& lastWrittenTs) {
  g_users = &users;
  g_lastWrittenTs = &lastWrittenTs;
}

void handleIncomingCommand(const IncomingCommand& cmd) {
  if (g_users == nullptr || !isAuthorized(*g_users, cmd.chatId)) {  // sec. 4.2
    logWarn("Command rejected (not whitelisted): chat_id=%lld", static_cast<long long>(cmd.chatId));
    recordUnauthorizedRequest(g_unauthorizedRequests,
                               {cmd.chatId, cmd.fromUserId, cmd.fromUsername, currentEpoch()});
    return;  // no reply: an unauthorized chat_id must never learn the bot exists (sec. 4.2)
  }

  bool admin = isAdmin(*g_users, cmd.chatId);
  logInfo("Command received: chat_id=%lld admin=%d text=%s", static_cast<long long>(cmd.chatId),
          admin, cmd.text.c_str());

  // Sec. 12.4 - passive username cache update, for /listusers. Only writes
  // to LittleFS when the value actually changed.
  if (!cmd.fromUsername.empty() && updateUsername(*g_users, cmd.chatId, cmd.fromUsername)) {
    saveUsers(*g_users);
  }

  std::string id, strArg, dumpTarget;
  bool hasTs = false, hasArg = false, boolArg = false, hasChatIdArg = false;
  uint32_t uintArg = 0, tsArg = 0;
  int64_t int64Arg = 0, dumpChatId = 0;

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
  } else if (parseDumpCommand(cmd.text, dumpTarget, dumpChatId, hasChatIdArg)) {
    handleDump(cmd.chatId, admin, dumpTarget, dumpChatId);
  } else if (parseResetLogCommand(cmd.text)) {
    handleResetLog(cmd.chatId, admin);
  } else if (parseResetNotifCommand(cmd.text, int64Arg)) {
    handleResetNotif(cmd.chatId, admin, int64Arg);
  } else if (parseResetUserConfigCommand(cmd.text, int64Arg)) {
    handleResetUserConfig(cmd.chatId, admin, int64Arg);
  } else if (cmd.text == "/listusers") {
    handleListUsers(cmd.chatId, admin);
  } else if (cmd.text == "/requests") {
    handleRequests(cmd.chatId, admin);
  } else if (cmd.text == "/status") {
    handleStatus(cmd.chatId);
  } else if (cmd.text == "/config") {
    handleConfig(cmd.chatId, admin);
  } else if (cmd.text == "/help") {
    handleHelp(cmd.chatId, admin);
  }
  // Unrecognized command: no reply (avoids noise on free-form text).
}
