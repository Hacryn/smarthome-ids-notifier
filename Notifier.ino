#include <LittleFS.h>
#include <esp_task_wdt.h>

#include "secrets.h"
#include "src/diagnostics/SerialLog.h"
#include "src/config/GlobalConfigStorage.h"
#include "src/events/EventId.h"
#include "src/events/EventLogStorage.h"
#include "src/events/EventTiming.h"
#include "src/events/EventTypes.h"
#include "src/events/OpenEventsManager.h"
#include "src/network/NetworkIssueTracker.h"
#include "src/network/WifiManager.h"
#include "src/notifications/NotificationEngine.h"
#include "src/pins/PinDebounce.h"
#include "src/pins/PinMonitor.h"
#include "src/rotation/FsErrorCounter.h"
#include "src/rotation/FilesystemHealth.h"
#include "src/rotation/RotationEngine.h"
#include "src/telegram/CallbackData.h"
#include "src/telegram/CommandRouter.h"
#include "src/telegram/TelegramClient.h"
#include "src/time/Clock.h"
#include "src/time/TimeAnchor.h"
#include "src/time/TimeAnchorStorage.h"
#include "src/users/UserList.h"
#include "src/users/UserStorage.h"

// Sec. 9 - periodic maintenance (space + rotation due), not on every loop cycle.
constexpr uint32_t kMaintenanceIntervalMs = 10UL * 60UL * 1000UL;

// Sec. 3.3 - one debouncer per EVENT_TYPES entry (entries with no pin stay unused).
PinDebouncer g_debouncers[EVENT_TYPES_COUNT];

// Sec. 5.4 - last ts written to the log (monotonicity, sec. 5.4.3). The
// time anchor and NTP sync state now live entirely in Clock.
uint32_t g_lastWrittenTs = 0;

// Sec. 13 - detects the "just connected" edge to (re)start NTP on every
// connection, not only the first (sec. 13: "on connection and periodically").
bool g_wifiWasConnected = false;

NetworkIssueTracker g_networkIssueTracker;

// Sec. 4 - whitelist of authorized users.
std::vector<AuthorizedUser> g_users;

// Sec. 6.2/8 - recovery scan and open-events summary, one-time on the
// first WiFi connection (boot).
bool g_bootTasksDone = false;

// Sec. 9.4 - true if LittleFS.begin() was reformatted at boot because it
// was unmountable: to be reported to admins as soon as connectivity is ready.
bool g_needsFsFormatAlert = false;

uint32_t g_lastMaintenanceMillis = 0;

bool initFilesystem() {
  bool mounted = LittleFS.begin(false);
  bool formatted = false;

  // A format is allowed only if the initial mount fails: a successful mount
  // with anomalous diagnostics must not automatically wipe data.
  if (!mounted) {
    Serial.println("LittleFS not mountable, attempting a format...");
    mounted = LittleFS.begin(true);
    formatted = mounted;
    if (mounted) {
      g_needsFsFormatAlert = true;
    } else {
      setFilesystemHealth(FilesystemHealth::MOUNT_FAILED);
      Serial.println("LittleFS unrecoverable.");
      return false;
    }
  }

  if (LittleFS.totalBytes() == 0) {
    setFilesystemHealth(FilesystemHealth::ZERO_CAPACITY);
    Serial.println("LittleFS mounted but with zero capacity.");
    return false;
  }

  constexpr const char* kProbePath = "/.littlefs-probe";
  File probe = LittleFS.open(kProbePath, "w");
  if (!probe) {
    setFilesystemHealth(FilesystemHealth::PROBE_OPEN_FAILED);
    Serial.println("LittleFS: opening the probe file failed.");
    return false;
  }

  size_t written = probe.print("ok");
  probe.close();
  if (written != 2) {
    setFilesystemHealth(FilesystemHealth::PROBE_WRITE_FAILED);
    Serial.println("LittleFS: writing the probe file failed.");
    return false;
  }

  if (!LittleFS.remove(kProbePath)) {
    setFilesystemHealth(FilesystemHealth::PROBE_REMOVE_FAILED);
    Serial.println("LittleFS: removing the probe file failed.");
    return false;
  }

  setFilesystemHealth(formatted ? FilesystemHealth::READY_AFTER_FORMAT
                                 : FilesystemHealth::READY);
  Serial.print("LittleFS: ");
  Serial.println(filesystemHealthText());
  return true;
}

void handleRawTransition(const PinTransition& t) {
  g_debouncers[t.eventTypeIndex].onTransition(t.level, t.millisAtIsr);
}

// Sec. 9.4 - the first filesystem write error must always be reported.
void alertOnFirstFsError() {
  if (fsErrorCounter().count() == 1) {
    notifyAdmins(g_users, "Primo errore di scrittura filesystem rilevato.");
  }
}

// Sec. 8.1 point 1 - authorization must be re-evaluated at click time,
// never taken for granted just because the button was visible.
void onTelegramCallback(const IncomingCallback& cb) {
  if (!isAdmin(g_users, cb.fromChatId)) return;

  std::string id;
  if (!parseCloseEventCallbackData(cb.data, id)) return;

  bool closed = closeOpenEvent(g_users, id, currentEpoch(), g_lastWrittenTs);
  answerCallback(cb.queryId, closed ? "Evento chiuso" : "Evento gia' chiuso o non trovato");
}

// Writes a row to the log and triggers the normal notification (sec. 6.1),
// reused for pin events, NETWORK_ISSUE, and REBOOT: same
// clamp+append+notify+error-alert pattern in all three cases.
void logAndNotifyEvent(EventType type, EventStatus status, uint32_t rawTs) {
  ClampedTimestamp clamped = applyMonotonicClamp(rawTs, g_lastWrittenTs);

  EventRecord rec{};
  rec.type = type;
  rec.status = status;
  rec.ts = clamped.ts;
  // Sec. 5.4.2/5.4.3 - approximate if time isn't synced via NTP, or if the
  // monotonicity clamp fired (regardless of NTP).
  rec.approx = !isTimeSynced() || clamped.wasClamped;

  if (status == EventStatus::START) {
    OpenEvent existing{};
    if (findOpenEventOfType(type, existing)) return;
    generateEventId(rec.id);
  } else if (status == EventStatus::END) {
    OpenEvent existing{};
    if (!findOpenEventOfType(type, existing)) return;
    memcpy(rec.id, existing.id, sizeof(rec.id));
  } else {
    generateEventId(rec.id);
  }

  if (appendEventRecord(rec)) {
    g_lastWrittenTs = clamped.ts;

    const EventTypeConfig* cfg = findEventTypeConfig(type);
    if (cfg && shouldNotifyForStatus(cfg->notify_policy, rec.status)) {
      notifyEvent(g_users, rec.id, rec.type, rec.status, rec.ts, rec.approx);
    }
  } else {
    alertOnFirstFsError();
  }
}

void setup() {
  Serial.begin(115200);

  // Sec. 3.3/13 - Task WDT on the application loop, timeout above the
  // maximum network timeout (30s vs. 10s). A genuine block triggers a
  // reboot, which in turn generates a notified REBOOT event (below),
  // making a failure visible that would otherwise be silent.
  esp_task_wdt_init(30, true);
  enableLoopWDT();

  initFilesystem();

  // Sec. 5.4.1 - loads the NVS anchor: the only time source until NTP
  // syncs (below, on the first WiFi connection); from that point on, rows
  // stop being marked approximate (sec. 5.4.2).
  initClock();
  g_lastWrittenTs = readLastWrittenTimestamp();

  // Sec. 4.5 - onboarding: if users.json is empty/absent, the chat_id from
  // secrets.h automatically becomes the first admin.
  if (!loadUsers(g_users)) {
    Serial.println("Reading users.json failed");
  }
  if (ensureOnboardingAdmin(g_users, ONBOARDING_CHAT_ID, currentEpoch())) {
    if (!saveUsers(g_users)) {
      Serial.println("Writing users.json failed");
    }
  }

  // Sec. 3.2/13 - REBOOT made visible on every boot (instant, always
  // notified), including one triggered by the watchdog above.
  logAndNotifyEvent(EventType::REBOOT, EventStatus::INSTANT, currentEpoch());

  // Sec. 9.3.2 - cleanup of leftover temp files from a rotation
  // interrupted by a blackout.
  cleanupStaleRotationFiles(g_users);

  // Sec. 11.1 - global configuration (defaults if not yet present in NVS).
  initGlobalConfigStore();

  initPinMonitor();
  initWifi(WIFI_SSID, WIFI_PASSWORD);
  initTelegramClient(TELEGRAM_BOT_TOKEN);
  initCommandRouter(g_users, g_lastWrittenTs);
  setTelegramUpdateHandlers(onTelegramCallback, handleIncomingCommand);
}

void loop() {
  drainPinTransitions(handleRawTransition);

  uint32_t nowMillis = millis();
  uint32_t epochNow = currentEpoch();

  for (size_t i = 0; i < EVENT_TYPES_COUNT; i++) {
    const EventTypeConfig& cfg = EVENT_TYPES[i];
    if (!cfg.enabled || cfg.pin < 0) continue;

    DebouncedTransition transition;
    if (!g_debouncers[i].poll(nowMillis, kPinDebounceMs, transition)) continue;

    EventStatus status = resolvePinEventStatus(transition.level, cfg.active_low);
    uint32_t rawTs = computeRetroactiveTimestamp(epochNow, nowMillis, transition.millisAtIsr);
    logInfo("Event detected: %s status=%d ts=%lu", cfg.label, static_cast<int>(status),
            static_cast<unsigned long>(rawTs));
    logAndNotifyEvent(cfg.type, status, rawTs);
  }

  tickWifi(nowMillis);
  tickClock(nowMillis);

  // TODO (phase 6): the definition in sec. 3.4.1 also requires Telegram
  // API reachability, not just WiFi status. For now NETWORK_ISSUE is based
  // solely on WiFi connection state.
  bool reachable = isWifiConnected();
  if (reachable && !g_wifiWasConnected) {
    // Sec. 13 - NTP on connection and on every reconnection, not just the first.
    beginNtpSync();
  }
  g_wifiWasConnected = reachable;

  NetworkIssueEvent netEv = g_networkIssueTracker.update(
      reachable, nowMillis, epochNow, globalConfig().networkIssueThresholdSec);
  if (netEv.kind == NetworkIssueEvent::Kind::STARTED) {
    logAndNotifyEvent(EventType::NETWORK_ISSUE, EventStatus::START, netEv.ts);
  } else if (netEv.kind == NetworkIssueEvent::Kind::ENDED) {
    logAndNotifyEvent(EventType::NETWORK_ISSUE, EventStatus::END, netEv.ts);
    // Sec. 6.2 - recovery scan on connectivity restoration.
    runRecoveryScan(g_users, nowMillis, epochNow);
  }

  if (!g_bootTasksDone && reachable) {
    // Sec. 6.2 - recovery scan at boot, one-time.
    runRecoveryScan(g_users, nowMillis, epochNow);
    // Sec. 8 - summary of events left open from a previous reboot.
    sendOpenEventsSummary(g_users);
    if (g_needsFsFormatAlert) {
      notifyAdmins(g_users, "LittleFS riformattato al boot (non montabile): storico perso.");
      g_needsFsFormatAlert = false;
    }
    // Sec. 9 - space check and any needed rotation, also at boot.
    performMaintenanceIfDue(g_users, epochNow, globalConfig().retentionWeeks);
    g_bootTasksDone = true;
  }

  if (nowMillis - g_lastMaintenanceMillis >= kMaintenanceIntervalMs) {
    // Sec. 9.2/9.4 - periodic check of space and rotation cadence.
    performMaintenanceIfDue(g_users, epochNow, globalConfig().retentionWeeks);
    g_lastMaintenanceMillis = nowMillis;
  }

  tickNotificationEngine(g_users, nowMillis, epochNow);
  tickTelegramUpdates();
}
