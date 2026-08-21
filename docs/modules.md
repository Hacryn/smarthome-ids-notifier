# Source Modules

`Notifier.ino` is the entry point: it owns the few pieces of global state that don't naturally belong to one module (the whitelist, the last-written-timestamp, the pin debouncers, one-shot boot flags), wires everything together in `setup()`, and drives it all from `loop()`. Everything else lives under `src/`, one subfolder per functional area, mirroring the corresponding section of [DESIGN.md](../DESIGN.md).

Within each folder, files split into two kinds:
- **Pure** — no Arduino/ESP32/LittleFS/network dependency, portable to a desktop compiler, covered by a host-side test in `test/` (see [testing.md](testing.md)).
- **Hardware-bound** — depends on real ESP32 peripherals (flash, WiFi, TLS, NVS, GPIO/ISR), verified only by `arduino-cli compile` (structural correctness) and, ultimately, by running on the device.

## `src/events/`

The event log itself: types, schema, storage, and the two derived views built on top of it.

| File | Kind | Responsibility |
|---|---|---|
| `EventTypes.h` | Pure | The `EventType`/`EventStatus`/`NotifyPolicy` enums and the `EVENT_TYPES` table — the single source of truth mapping a type to its label, GPIO pin, polarity, and notification policy. |
| `EventLog.h/.cpp` | Pure | `EventRecord` struct, JSON (de)serialization for one `log.jsonl` line. |
| `EventLogStorage.h/.cpp` | Hardware | Real LittleFS append (with retry + shared error counter), backward-read of the last line (for boot-time monotonicity seeding), and lookup of a specific `(id, status)` row (used during notification recovery to recover the type/approx flag not duplicated in the per-user files). |
| `EventId.h/.cpp` | Hardware | 32-hex-char ID generation via `esp_random()`. |
| `EventTiming.h/.cpp` | Pure | The retroactive-dating formula. |
| `EventAggregator.h/.cpp` | Pure | Pairs `START`/`END` rows sharing an `id` into one aggregated event with a computed duration, in a bounded ring buffer fed one line at a time — the logic behind `/log`, reused by the notification-aggregation message text. |
| `OpenEventsTracker.h/.cpp` | Pure | Given a stream of log rows, finds events with a `START` but no matching `END` ("open" events). |
| `OpenEventsManager.h/.cpp` | Hardware | Detects open events from the real log, closes one (verifying it's still open, to guard against a double-click or a duplicate `/closeevent`), and sends the open-events summary (with inline "close" buttons for admins). |

## `src/pins/`

The ISR/debounce layer described in [architecture.md](architecture.md).

| File | Kind | Responsibility |
|---|---|---|
| `PinMonitor.h/.cpp` | Hardware | `attachInterruptArg` registration, the FreeRTOS queue, overflow counting. |
| `PinDebounce.h/.cpp` | Pure | The debounce state machine and `active_low` level interpretation. |

## `src/time/`

The system clock.

| File | Kind | Responsibility |
|---|---|---|
| `Clock.h/.cpp` | Hardware | `currentEpoch()` — the one function the rest of the firmware calls for "now". NTP sync lifecycle, fallback to the anchor before/without sync. |
| `ClockPolicy.h/.cpp` | Pure | The epoch-plausibility threshold used to detect a genuine NTP sync vs. an unsynced clock near 1970. |
| `TimeAnchor.h/.cpp` | Pure | Anchor-based epoch estimation and the monotonicity clamp. |
| `TimeAnchorStorage.h/.cpp` | Hardware | NVS persistence of the anchor. |

## `src/network/`

WiFi connectivity and the `NETWORK_ISSUE` event.

| File | Kind | Responsibility |
|---|---|---|
| `WifiManager.h/.cpp` | Hardware | Non-blocking connect/reconnect with backoff; exposes SSID/RSSI/attempt count for `/status`. |
| `WifiBackoff.h/.cpp` | Pure | The exponential backoff schedule (5 s → 300 s cap) and the "every 10th attempt, full reconnect cycle" rule. |
| `NetworkIssueTracker.h/.cpp` | Pure | State machine deciding when a connectivity gap becomes a logged `NETWORK_ISSUE` (only after the configurable threshold, dating the `START` to when connectivity was actually lost, not when the threshold was crossed). |

## `src/telegram/`

Everything that talks to the Telegram Bot API.

| File | Kind | Responsibility |
|---|---|---|
| `TelegramClient.h/.cpp` | Hardware | Wraps FastBot2: outbound sends (plain and with inline keyboards) with throttling retry, incoming update dispatch (callback queries and text commands) translated into plain structs so the rest of the firmware never needs FastBot2's types. |
| `SendOutcomeClassifier.h/.cpp` | Pure | Classifies a raw send result into success / transient (network or server) / throttling / permanent-recipient / system-error. |
| `RateLimiter.h/.cpp` | Pure | Non-blocking minimum-interval gate between sends. |
| `CommandRouter.h/.cpp` | Hardware | The command dispatcher: whitelist check, admin gating, and the implementation of every command in [commands.md](commands.md). |
| `CommandParser.h/.cpp` | Pure | Text parsing for every command's arguments. |
| `CallbackData.h/.cpp` | Pure | `callback_data` encoding/decoding for the inline "close event" buttons. |

## `src/notifications/`

Delivery tracking and the recovery/retry/aggregation logic.

| File | Kind | Responsibility |
|---|---|---|
| `NotificationEngine.h/.cpp` | Hardware | The outbound queue, the live-send flow, the recovery scan, admin alerting. The central orchestrator of this folder. |
| `NotificationRecord.h/.cpp` | Pure | The `notif_<chat_id>.jsonl` schema, path formatting, (de)serialization. |
| `NotificationFolder.h/.cpp` | Pure | Folds a stream of rows into current per-`(id,status)` state. |
| `NotificationPresentation.h/.cpp` | Pure | Grace-period/"recovered" presentation decision, aggregation threshold, near-abandonment heuristic, max-retries check. |
| `RetryTimer.h/.cpp` | Pure | The single retry-timer state machine, including reentrancy protection against a success-triggered scan nesting inside an in-progress scan. |
| `NotificationLogStorage.h/.cpp` | Hardware | Real per-user file I/O. |

## `src/rotation/`

Weekly log rotation and filesystem health.

| File | Kind | Responsibility |
|---|---|---|
| `RotationEngine.h/.cpp` | Hardware | Periodic entry point: checks space, decides whether rotation is due, triggers it, tracks degraded-mode state. |
| `RotationPolicy.h/.cpp` | Pure | Retention cutoff math, weekly-due check, the 80%/95% space thresholds. |
| `RotationCollector.h/.cpp` | Pure | The bounded (256-ID, ~4 KB) deletable-ID collector for `log.jsonl`'s two-pass rewrite. |
| `IdBinary.h/.cpp` | Pure | Hex ↔ 16-byte binary ID conversion (used to keep the collector's memory footprint small). |
| `EventLogRotation.h/.cpp` | Hardware | The two-pass rewrite of `log.jsonl` itself. |
| `NotificationLogRotation.h/.cpp` | Hardware | Per-user notification log rewrite (simpler single-pass, since these files are bounded by design). |
| `RotationStorage.h/.cpp` | Hardware | NVS persistence of the last rotation timestamp. |
| `FsErrorCounter.h/.cpp` | Pure | The shared write-failure counter every LittleFS writer reports to. |

## `src/config/`

Global and per-user configuration, plus timestamp formatting.

| File | Kind | Responsibility |
|---|---|---|
| `GlobalConfig.h` | Pure | The struct holding the six admin-configurable values, with their defaults. |
| `GlobalConfigStorage.h/.cpp` | Hardware | NVS persistence plus the shared in-RAM `globalConfig()` accessor every consumer reads from. |
| `UserConfig.h/.cpp` | Pure | Per-user preferences model, notify-type toggle, JSON (de)serialization. |
| `UserConfigStorage.h/.cpp` | Hardware | LittleFS I/O for `userconfig.json`. |
| `TimezonePresets.h` | Pure | The preset-name-to-POSIX-TZ-string table. |
| `TimestampFormatter.h/.cpp` | Hardware | Formats an epoch per a user's timezone/date format (sets/restores the process-global `TZ` env var around the conversion — the only place in the firmware that touches it). |

## `src/users/`

The whitelist.

| File | Kind | Responsibility |
|---|---|---|
| `UserList.h/.cpp` | Pure | Whitelist operations (add/remove/promote/reset), onboarding, JSON (de)serialization. |
| `UserStorage.h/.cpp` | Hardware | LittleFS I/O for `users.json`. |

## Dependency direction

Roughly: `events` and `users` are the lowest layer (no dependency on anything else in `src/`); `time`, `network`, `telegram/{SendOutcomeClassifier,RateLimiter}` sit alongside them; `config` depends on `events` (for `EventType`); `notifications` depends on `events`, `config`, `telegram`, and `rotation` (for the degraded-mode write guard); `rotation` depends on `events` and `notifications`' storage layer; `telegram/CommandRouter` sits on top of nearly everything, being the command surface. There's one intentional two-way relationship between `notifications` and `rotation` (degraded mode suspends certain writes; rotation alerts go out through the notification engine) — both directions are confined to `.cpp` files including the other module's header, never header-to-header, so it doesn't create a circular include.
