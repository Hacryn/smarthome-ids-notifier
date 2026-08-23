#include <LittleFS.h>
#include <esp_task_wdt.h>

#include "secrets.h"
#include "src/diagnostics/SerialLog.h"
#include "src/diagnostics/StatusLed.h"
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

// Sec. 13 - max time to wait for a real NTP sync before REBOOT is logged
// with the estimated anchor as a fallback. The Task WDT (30s) is fed on
// every iteration of waitForNtpSyncOrTimeout(), so it isn't a constraint on
// this value: the real limit is just how much boot delay is acceptable when
// WiFi is slow/unstable. 20s gives comfortable margin over the worst case
// observed on real hardware (physical-button reset, ~11-12s for a first
// stable association - see .findings/
// reboot-real-timestamp-led-off-still-approx.md), while still being a
// bounded wait with the same silent fallback if the timeout elapses.
constexpr uint32_t kRebootNtpWaitTimeoutMs = 20000;

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

// Sec. 13 - drives the status LED's ALARM state. Re-checked periodically
// (not every loop cycle, to avoid a LittleFS read on every spin) rather
// than tracked incrementally, so it can't drift out of sync with a
// manual /closeevent (which doesn't go through logAndNotifyEvent below).
constexpr uint32_t kLedAlarmCheckIntervalMs = 2000;
uint32_t g_lastLedAlarmCheckMillis = 0;
bool g_ledAlarmOrPowerLossOpen = false;

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

// Sec. 13 - bounded wait for a real NTP sync before REBOOT is logged
// (below), so the one event guaranteed on every boot isn't always marked
// approximate. Reuses the existing beginNtpSync()/tickClock(), no
// duplicated NTP logic. Local to the sketch (not a new module): one-shot
// boot logic, not reusable elsewhere, not host-testable since it depends
// on real WiFi/time() (same treatment as Clock.cpp/WifiManager.cpp).
// esp_task_wdt_reset() is called explicitly every iteration: enableLoopWDT()
// already watches this task by this point in setup(), so this keeps the
// wait safe regardless.
void waitForNtpSyncOrTimeout(uint32_t timeoutMs) {
  uint32_t start = millis();
  bool ntpStarted = false;

  // Sec. 13 - read once here (rather than every 2s as in loop()) so the LED
  // reflects an already-open alarm/230V-power-loss from the very first tick
  // of this wait, not only once loop() detects it.
  OpenEvent tmp{};
  bool alarmOrPowerLossOpen = findOpenEventOfType(EventType::ALARM_GENERAL, tmp) ||
                               findOpenEventOfType(EventType::ALARM_INTERNAL, tmp) ||
                               findOpenEventOfType(EventType::ALARM_GARAGE, tmp) ||
                               findOpenEventOfType(EventType::POWER_LOSS, tmp);

  while (static_cast<uint32_t>(millis() - start) < timeoutMs) {
    esp_task_wdt_reset();

    tickWifi(millis());  // lets a connection retry happen within the timeout
    if (isWifiConnected() && !ntpStarted) {
      beginNtpSync();
      ntpStarted = true;
    }
    tickClock(millis());

    // Same reachable/LED-state computation as loop(), so the LED doesn't
    // stay off for the whole wait.
    bool reachable = isWifiConnected() && isTelegramReachable();
    StatusLedState ledState = decideStatusLedState(reachable, isTimeSynced(),
                                                    alarmOrPowerLossOpen, isFilesystemDegraded());
    tickStatusLed(millis(), ledState);

    if (isTimeSynced()) return;
    delay(50);
  }
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

  // Sec. 13 - "boot in progress" indicator: on for the whole duration of
  // setup(), off on exit (below). Pin separate from the RGB status LED
  // (LED_RED/GREEN/BLUE) - polarity HIGH=on, to be confirmed on first real
  // hardware test.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  logInfo("Entering setup()");

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

  // Sec. 9.3.2 - cleanup of leftover temp files from a rotation
  // interrupted by a blackout.
  cleanupStaleRotationFiles(g_users);

  // Sec. 11.1 - global configuration (defaults if not yet present in NVS).
  initGlobalConfigStore();

  // initPinMonitor()/initStatusLed() stay here, before the NTP wait below:
  // an alarm firing during the wait must still be queued by the ISR, never
  // missed while setup() waits for a real clock.
  initPinMonitor();
  initStatusLed();
  StaticIpConfig staticIp{STATIC_IP_ENABLED,   STATIC_IP_ADDRESS, STATIC_IP_GATEWAY,
                           STATIC_IP_SUBNET,    STATIC_IP_DNS1,    STATIC_IP_DNS2};
  initWifi(WIFI_SSID, WIFI_PASSWORD, staticIp);

  // Sec. 13 - bounded wait for a real NTP sync (see waitForNtpSyncOrTimeout
  // above) so REBOOT below can get a real timestamp instead of always being
  // marked approximate. Falls back silently to the anchor estimate if the
  // timeout elapses (WiFi down or slow) - same behavior as before this wait
  // existed.
  waitForNtpSyncOrTimeout(kRebootNtpWaitTimeoutMs);
  // Avoids a redundant beginNtpSync() call on the first loop() iteration.
  g_wifiWasConnected = isWifiConnected();

  // Sec. 3.2/13 - REBOOT made visible on every boot (instant, always
  // notified), including one triggered by the watchdog above.
  logAndNotifyEvent(EventType::REBOOT, EventStatus::INSTANT, currentEpoch());

  initTelegramClient(TELEGRAM_BOT_TOKEN);
  initCommandRouter(g_users, g_lastWrittenTs);
  setTelegramUpdateHandlers(onTelegramCallback, handleIncomingCommand);

  digitalWrite(LED_BUILTIN, LOW);
  logInfo("Exiting setup()");
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

  bool wifiConnected = isWifiConnected();
  if (wifiConnected && !g_wifiWasConnected) {
    // Sec. 13 - NTP on connection and on every reconnection, not just the first.
    beginNtpSync();
  }
  g_wifiWasConnected = wifiConnected;

  // Sec. 3.4.1 - NETWORK_ISSUE reachability combines WiFi connection state
  // with real Telegram send outcomes: WiFi down is still the dominant
  // signal (no send is possible either way), but WiFi up with Telegram
  // unreachable (e.g. a router with no working internet uplink) is now
  // caught too.
  bool reachable = wifiConnected && isTelegramReachable();
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

  // Sec. 13 - status LED, non-blocking (see StatusLedPolicy for the
  // priority rule between alarm/degraded/network-time/ok).
  if (nowMillis - g_lastLedAlarmCheckMillis >= kLedAlarmCheckIntervalMs) {
    OpenEvent tmp{};
    g_ledAlarmOrPowerLossOpen = findOpenEventOfType(EventType::ALARM_GENERAL, tmp) ||
                                 findOpenEventOfType(EventType::ALARM_INTERNAL, tmp) ||
                                 findOpenEventOfType(EventType::ALARM_GARAGE, tmp) ||
                                 findOpenEventOfType(EventType::POWER_LOSS, tmp);
    g_lastLedAlarmCheckMillis = nowMillis;
  }
  StatusLedState ledState = decideStatusLedState(reachable, isTimeSynced(),
                                                  g_ledAlarmOrPowerLossOpen, isFilesystemDegraded());
  tickStatusLed(nowMillis, ledState);
}
