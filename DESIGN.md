# Bentel Alarm Monitoring System with Telegram Notifications
## Design, Requirements, and Technical Specification Document

**Version:** 1.00
**Date:** August 2026
**Target platform:** Arduino Nano ESP32

---

## 1. System objective

Build a system that monitors the status of a Bentel alarm control panel via an Arduino Nano ESP32, sends real-time Telegram notifications on event open/close (alarm, reboot, network issues, power loss, etc.), and maintains a persistent, queryable historical log of events, with robust handling of connectivity and power interruptions, access restricted to authorized users, and per-user configurable preferences.

**Note on power supply**: the Arduino will be powered by the alarm control panel, which has a backup battery in case of a power outage. Reboots due to power loss are therefore expected to be **rare events**; interruptions of **connectivity alone** (router/ISP, not necessarily on battery) remain the most plausible and frequent failure case, and are the ones the notification recovery system is primarily designed for.

### 1.1 Architectural constraint: independence from external systems

The device is **deliberately autonomous**: it does not depend on any MQTT broker, home automation system (Home Assistant or similar), or local server. The ESP32 talks directly to the Telegram API and handles persistence, history, retry, users, and preferences on its own.

This choice is deliberate and motivates the complexity of sections 4-11 (which, in the presence of an upstream system, would largely be delegable): **shortening the failure chain between alarm detection and the user's phone**. Every intermediate component would be an additional point of failure between the event and the notification, unacceptable in a security system.

Corollary: the system has no external observer that could notice a total failure of its own. State verification is **manual**, via the `/status` command (section 12) — a conscious choice, preferred over an automatic periodic heartbeat that would generate unwanted recurring messages.

---

## 2. Hardware requirements

### 2.1 Reading alarm state

The Bentel panel exposes configurable PGM (programmable) outputs usable as status indicators. **The PGMs used are configured as relay outputs (dry NA/NC contact)**: direct connection to the ESP32 pins, with no need for additional isolation, since the contact is mechanically isolated from the panel's circuitry. The reading pin must be configured as `INPUT_PULLUP`, with invertible logic depending on whether an NA or NC contact is used (`active_low` field in the table in section 3.2.1).

**Note on NC contacts and boot pins**: an NC contact (normally closed, opens on alarm) holds the pin at LOW at rest. If the chosen pin is an ESP32-S3 strapping pin, this resting level can interfere with the boot sequence. Where possible, an NA contact (normally open, rests at HIGH thanks to the pull-up) should therefore be preferred on any boot pins, or wiring an NC-configured zone to those specific pins should be avoided. The list of Nano ESP32 strapping pins must be checked against the official pinout when assigning pins.

**Note — more event types, more physical inputs**: with the introduction of multiple event types tied to distinct contacts on the panel (internal alarm, garage alarm, power loss), it's likely that **more dedicated PGM outputs** will be needed on the panel (one per zone/condition to be monitored separately), and consequently **one ESP32 reading pin per each**. The number of digital pins available on the Nano ESP32 and the availability of configurable PGM outputs on the panel must be checked against the final number of types to be monitored. For "power loss" in particular, many Bentel panels expose a dedicated PGM for "mains fault / 230V missing", active while the system is still powered by the backup battery.

### 2.2 Debounce

A software debounce mechanism (recommended threshold: 300 ms) is needed to avoid false triggers on signal state transitions. The implementation is constrained by the concurrency model described in section 3.3: debounce **cannot be implemented by polling in the main loop**, because the loop is blocked for seconds by network calls.

---

## 3. Software architecture

### 3.1 Main components

| Component | Responsibility |
|---|---|
| Alarm pin reading (ISR + queue) | Transition detection via interrupt, with debounce and deferred dating (section 3.3) |
| Telegram client (FastBot2) | Sending notifications, receiving commands and inline button callbacks; natively exposes the structured outcome of each send (sections 3.5, 6.5) |
| NTP sync + timezone management | Obtaining real timestamps (Unix epoch, UTC) and converting to local time with automatic DST handling |
| Persistent time anchor (NVS) | Maintaining a usable time reference before NTP sync (section 5.4) |
| Connectivity and reconnection management | Exponential backoff, detection of the `NETWORK_ISSUE` condition (section 3.4) |
| Event log (LittleFS, `log.jsonl`) | Persistence of event history (detection only, not notifications) |
| Per-user notification log (LittleFS) | Tracking of missed/to-be-recovered sends, one file per chat (architecture decided, see section 7) |
| User and permission management (whitelist) | `chat_id` authorization, standard/admin user distinction, notification recipient filtering |
| Global configuration management (NVS) | Persistent system settings, modifiable only by admin users |
| Per-user configuration management (LittleFS) | Individual preferences (date format, timezone, notified event types) |
| Notification recovery engine | Detection and resending of undelivered notifications, with grace period and scheduled retry |
| Send rate limiter | Compliance with Telegram API rate limits (section 6.6) |
| Rotation engine | Periodic cleanup of the event log and the notification log |
| Filesystem monitoring | Monitoring of LittleFS space and write errors (section 9.4) |

### 3.2 Event types

The system must support multiple event types, extensible in the future. Each type is mapped to a numeric enum value for storage optimization (see section 5.2):

| `type` enum value | Type | Nature | Notification sent |
|---|---|---|---|
| `0` | `REBOOT` — Arduino reboot | Instant (`INSTANT`) | `INSTANT` |
| `1` | `POWER_LOSS` — power outage (230V mains missing) | Has duration (`START`/`END`) | `START` and `END` |
| `2` | `NETWORK_ISSUE` — network connectivity issue | Has duration (`START`/`END`) | **`END` only** (see 3.2.3) |
| `10` | `ALARM_GENERAL` — general alarm | Has duration (`START`/`END`) | `START` and `END` |
| `11` | `ALARM_INTERNAL` — internal alarm | Has duration (`START`/`END`) | `START` and `END` |
| `12` | `ALARM_GARAGE` — garage alarm | Has duration (`START`/`END`) | `START` and `END` |

*(other types can be appended to the enumeration, without breaking compatibility with existing logs — never reuse/renumber values already assigned)*

#### 3.2.1 Type configuration table

Each type is described in the firmware by an entry in a single static table, which is **the single source of truth** for the mapping:

| Field | Meaning |
|---|---|
| `type` | Enum value (never reassigned) |
| `label` | Human-readable label for Telegram messages |
| `pin` | Associated ESP32 pin, or "none" for internally-generated events (`REBOOT`, `NETWORK_ISSUE`) |
| `active_low` | Signal polarity (depends on open collector / NA / NC) |
| `enabled` | If `false`, the type is **completely disabled**: no interrupt registered, no log row, no notification |
| `notify_policy` | `START_AND_END`, `ONLY_END`, `INSTANT` |

The `enabled` flag allows a type that isn't wired up or isn't wanted to be disabled at the firmware level, without removing its enum value (which stays reserved so as not to invalidate historical logs).

#### 3.2.2 Note on `ALARM_GENERAL` and overlap with zones

If the "general alarm" PGM on the panel is configured as an OR of the zones, a garage alarm will trigger **both** the `ALARM_GARAGE` pin **and** the `ALARM_GENERAL` one, generating two distinct events and two notifications for the same physical fact.

**This behavior is accepted by choice**: `ALARM_GENERAL` is logged and notified whenever its pin activates, with no suppression logic or temporal correlation with other zones (which would introduce timing edge cases that are hard to make reliable). The resulting noise is mitigated at two levels, both already provided for:

- **Per user**: `/notify ALARM_GENERAL off` (section 11.2) disables the notification for whoever doesn't want it, while still leaving the event in the history.
- **Globally**: `enabled = false` in the table in section 3.2.1 disables the type entirely.

#### 3.2.3 Note on `NETWORK_ISSUE` — why only `END` is notified

The `START` of a connectivity issue occurs, by definition, when connectivity is absent: its notification would **systematically** fail, always ending up in the recovery queue and then being delivered on restoration, seconds after the `END` notification. Two messages in quick succession for a single already-concluded fact.

The `notify_policy` of `NETWORK_ISSUE` is therefore `ONLY_END`:

- The `START` row **is still written** to `log.jsonl` at the moment of detection, so the history preserves the exact instant the outage began and it's queryable via `/log`.
- No notification is generated (neither sent, nor queued for recovery) for that `START`.
- On reconnection, the `END` notification includes the **outage duration** calculated from the two timestamps, e.g. *"Connectivity restored — down for 1h 28m (from 14:02 to 15:30)"*.

The `POWER_LOSS` case, on the other hand, remains `START_AND_END`: if the router isn't on a UPS, the `START` will fail and will be recovered by the normal mechanism in section 6 — correct behavior, because a power loss is a significant event in its own right even after the fact.

### 3.3 Concurrency model: detection vs. network

**Problem.** The application loop makes synchronous HTTPS calls to Telegram (`getUpdates` in long polling, `sendMessage`, TLS handshake). On ESP32 these calls block execution for **seconds**, and up to the configured timeout under degraded network conditions. A debounce implemented by polling in the loop would lose any transition that occurred during those windows — potentially, the alarm itself.

**Solution adopted: interrupt + FreeRTOS queue + retroactive dating.**

1. An `attachInterrupt()` in `CHANGE` mode is registered on every enabled pin. The ISR is declared `IRAM_ATTR` and does **exactly one thing**: enqueue a record `{pin_index, level, millis()}` via `xQueueSendFromISR()`. No allocation, no I/O, no blocking call inside the ISR.
2. The queue has a fixed depth (32 elements, amply sufficient: 32 transitions during a single network block already represent an anomalous condition). On queue-full, the overflow is **counted** and reported in `/status`, never silently ignored.
3. As soon as it becomes free, the main loop drains the queue and applies debounce against the `millis()` values recorded **by the ISR**, not against processing time: a transition is confirmed if it isn't followed by another transition on the same pin within 300 ms.
4. **Retroactive dating**: the event's timestamp isn't the processing instant but is reconstructed backward from the `millis()` captured in the ISR:

   ```
   event_ts = current_epoch - (millis_now - millis_ISR) / 1000
   ```

   This way, an alarm that fired while the system was blocked in a 10-second TLS timeout is dated correctly, not 10 seconds later.

**Consequences and invariants:**

- No transition is ever lost, regardless of how long network blocks last. An alarm pulse shorter than the block still results in a pair of transitions in the queue, and is fully reconstructed (`START` and `END`).
- **All LittleFS access happens exclusively from the main loop.** The ISR never touches the filesystem. No mutex is therefore needed on the FS, which isn't thread-safe: this is an invariant to preserve in any future extension.
- No additional FreeRTOS task is introduced: retroactive dating makes a dedicated detection task unnecessary, keeping the firmware single-flow and easier to reason about.
- The **hardware watchdog** (ESP32 Task WDT) is enabled on the application loop with a timeout above the maximum configured network timeout (30 s vs. 10 s), so a genuine block triggers a reboot — which in turn generates a notified `REBOOT` event, making the failure visible.

### 3.4 Connectivity and reconnection management

#### 3.4.1 Operational definition of "network issue"

For `NETWORK_ISSUE` purposes, what matters isn't WiFi status but **reachability of the Telegram API**: the most common case (router on, ISP line down) has WiFi perfectly associated and no useful connectivity.

The network-issue condition is therefore defined as:

> WiFi disconnected **or** failure of the most recent application-level calls to `api.telegram.org` (notification sends and command replies)

sustained continuously for longer than the **configured threshold** (default: **120 seconds**, `/setnetthreshold`).

Telegram reachability is inferred from the outcome of real sends (`TelegramReachabilityTracker`, section 6.5's `TRANSIENT_NETWORK` category) rather than from a dedicated probe or from the long-polling `getUpdates` cycle itself: FastBot2's own error hooks for the polling loop are marked for removal upstream, so nothing durable can be built on them. The practical consequence is that the signal only updates when an application-level send is actually attempted — during a period of pure silence (no event, no admin interaction) it won't refresh — but an alarm firing always attempts a send, which is exactly the case that matters.

- The threshold exists so as not to generate events for micro-interruptions and router reboots, which typically recover within 60-90 seconds. Too low a value would fill the log with noise.
- The `END` of `NETWORK_ISSUE` is logged on the **first successful call** to the API, with `ts` equal to that instant.
- If connectivity returns **before** the threshold expires, no event is logged: the outage is considered a negligible blip.

#### 3.4.2 Reconnection backoff

Reconnection attempts follow a **capped exponential backoff**, so as not to saturate the loop nor waste energy on useless attempts during prolonged outages:

| Attempt | Wait before next attempt |
|---|---|
| 1 | 5 s |
| 2 | 10 s |
| 3 | 20 s |
| 4 | 40 s |
| 5 | 80 s |
| 6 | 160 s |
| 7 and beyond | 300 s (cap) |

- The backoff counter **resets** on every successful reconnection.
- The wait is implemented with a non-blocking timer (`millis()` comparison), never with `delay()`: the loop must remain free to drain the interrupt queue and apply debounce (section 3.3).
- Every 10 consecutive failed attempts, a full `WiFi.disconnect()` + `WiFi.begin()` cycle is attempted, to recover from anomalous WiFi stack states that a plain `reconnect()` doesn't resolve.
- The 300 s cap guarantees that, once the network is restored, the maximum detection delay is 5 minutes; NTP sync and the notification recovery scan (section 6.2) immediately follow the return of connectivity.

### 3.5 Telegram client library (FastBot2)

**Library chosen: FastBot2** (GyverLibs), instead of UniversalTelegramBot evaluated in an early draft of this document. The reason for the choice is specific to point 6.5: `sendMessage()` **returns an `fb::Result` object directly**, not a plain `bool`, with direct access to the fields of the Telegram response (`isError()`, `getErrorCode()`, `getError()`, and the internal parser for `parameters.retry_after`) — the send-outcome classification required by 6.5 is therefore obtainable with the library's public API, with no wrapper or local modification.

#### 3.5.1 Polling mode

FastBot2 offers three modes (`bot.setPollMode(...)`), selectable with a direct trade-off between responsiveness and loop blocking:

| Mode | Behavior |
|---|---|
| `Sync` (default) | `tick()` waits for the response internally; under degraded network conditions it can block up to the configured timeout |
| `Async` | `tick()` doesn't wait for the polling response, but a send requested **while a poll is in progress** forces a blocking reconnection of ~1 s |
| `Long` | Asynchronous long polling (recommended timeout ≥ 20 s); updates arrive as soon as they're available. A send requested during polling has the same reconnection cost as `Async` |

**Mode adopted: `Long`**, with a 60 s timeout, for the fastest delivery of incoming commands. The library exposes `isPolling()` to know whether a long-poll cycle is in progress; a send issued from outside the update handler (`onUpdate`) — which is exactly the case of notifications generated by events detected on pins, asynchronous with respect to the Telegram cycle — **can therefore incur the ~1 s reconnection block**, regardless of the mode chosen.

This doesn't introduce a new requirement: it's exactly the kind of network block already assumed possible in section 3.3, covered by the ISR + queue + retroactive-dating architecture. No pin transition is lost as a result of this block, and the watchdog (30 s) stays amply above the worst case (~1 s).

#### 3.5.2 Coexistence with ArduinoJson

FastBot2 internally uses **GSON** (by the same author) to parse Telegram API responses — a dependency of the library itself, not a project choice. **ArduinoJson remains the library used for every file in the project** (`users.json`, `userconfig.json`, `log.jsonl` and `notif_<chat_id>.jsonl` rows, sections 4.4, 5.2, 7.2): this is an explicit decision, not an oversight. The two libraries are independent and share no buffers or types, so coexistence carries no risk; the cost is only a few extra KB of flash for having two JSON parsers in the firmware, deemed acceptable against the benefit of not having to rewrite the file read/write logic already specified in this document.

**Note on `chat_id`**: FastBot2 represents identifiers (`fb::ID`) internally as a string (22-character buffer), explicitly constructible from a 64-bit integer (`long long`). Passing `chat_id` as a native `int64_t` (never through an intermediate 32-bit type) therefore avoids any truncation even on the Telegram side of the chain; the `int64_t` requirement from section 4.2 remains necessary regardless for the part of the system written with ArduinoJson (whitelist, configurations), where the truncation risk is real.

---

## 4. User, permission, and security management

### 4.1 Motivation

Telegram provides no native access-control mechanism for bots: anyone who knows the bot's username can message it. Protection is therefore entirely handled at the application level, via a whitelist of authorized `chat_id`s.

### 4.2 Whitelist

Every incoming message (command) is checked against the whitelist before being processed:

- If the sender's `chat_id` **is not on the whitelist**, the message is **silently ignored** (no response), so as not to reveal the bot's existence/operation to whoever guesses the username.
- If the `chat_id` **is on the whitelist**, the command is executed according to the associated permissions.

The same check applies to **callback queries** generated by inline buttons (section 8): authorization is re-evaluated at the moment of the click, never taken for granted just because the button is visible.

The same whitelist also governs **notification sending**: when an event generates a notification, it's sent exclusively to `chat_id`s present on the whitelist, never to unauthorized recipients.

**Implementation note on `chat_id`s**: Telegram `chat_id`s are **signed 64-bit** integers. Private chats have positive values that today fit within 32 bits, but groups and supergroups use **negative** values that far exceed them (format `-100XXXXXXXXXX`). They must therefore be represented as `int64_t` at every point in the system — RAM structs, JSON parsing (ArduinoJson must be explicitly instructed on the 64-bit type), comparisons, and formatting. A `long` on ESP32 is 32 bits and would produce **silent** truncation, with the result that an authorized group would never be recognized.

When a `chat_id` is used to build a filename (section 7), the minus sign must be replaced with a text prefix (e.g. `notif_g1001234567890.jsonl`) to avoid filenames starting with problematic characters.

### 4.3 Permission levels

For the current phase, a single boolean `admin` flag is provided, with no granular permissions:

- **Standard user**: can view the history (`/log`), check system status (`/status`), view and modify **their own** personal preferences (date format, timezone, notified event types).
- **Admin user**: in addition to the above, can modify **global** system configurations (retention, grace period, retry interval, max retries, network threshold), **manually close open events** (via inline button or `/closeevent`), and **manage the whitelist itself** (see 4.5).

This binary distinction is considered sufficient for personal/family use; the storage schema chosen is nonetheless prepared for the future addition of more granular permissions without requiring restructuring.

### 4.4 User and configuration storage

| File/Storage | Content | Format |
|---|---|---|
| `users.json` (LittleFS) | Whitelist of authorized `chat_id`s, with an `admin` flag and an added-date for each | JSON, rewritten in full (write-then-rename) on every change |
| `userconfig.json` (LittleFS) | Per-user preferences: date format, timezone, notified event types | JSON indexed by `chat_id`, rewritten in full on every change |
| NVS (Preferences) | **Global** system configurations, schema version, time anchor, last-rotation timestamp | Scalar key-value pairs |

Illustrative example of `users.json`:
```json
{
  "authorized": [
    {"chat_id": 111111111, "admin": true, "added_ts": 1755000000},
    {"chat_id": 222222222, "admin": false, "added_ts": 1755600000}
  ]
}
```

### 4.5 Initial population and operational whitelist management

- **Initial onboarding**: an initial `chat_id` is **defined in the secrets file** (section 4.7) at setup time, and automatically becomes the first admin user on first boot (populating `users.json` if still empty/absent).
- **Whitelist management commands** (admin-only):
  - Adding a new authorized user
  - Removing a user
  - Promoting/revoking the admin flag for an existing user
  - Fully resetting the whitelist (use with caution — worth requiring explicit confirmation given the destructive nature)

*(Exact command syntax in section 12.)*

### 4.6 Filtering events prior to a user's addition

To avoid a newly-added user receiving a mass send of all past historical notifications, the `added_ts` field already present in `users.json` is used as a filter: any event with an origin timestamp earlier than `added_ts` is **excluded** from notification sending for that user, both in the live flow and during recovery. Historical `/log` remains fully browsable by anyone authorized, regardless of this date.

### 4.7 Secrets management

The Telegram bot token, WiFi credentials, and the onboarding `chat_id` live in a separate **`secrets.h`** file, included by the main sketch.

- With the Arduino IDE, the file appears as an **additional tab** of the sketch when placed in the same folder, so it stays conveniently editable without touching the main source.
- `secrets.h` is **excluded from version control** (`.gitignore`). A `secrets.h.example` with the same structure and placeholder values is committed to the repository, so a build on a clean machine fails with a clear error instead of anomalous runtime behavior.
- Values are stored **in plaintext** in the firmware. This is a conscious choice: ESP32-S3 flash encryption is not used.

  **Threat model to keep in mind**: the device is installed *inside* the alarm control panel. Whoever gains physical access to its interior can dump the flash and recover the bot token, then being able to send arbitrary messages to users (though not read the history nor command the panel). The risk is considered acceptable given that whoever has already opened the panel has more immediate problems to cause; if the token is compromised, the mitigation is regenerating it via BotFather and reflashing the device.

---

## 5. Event log data model

### 5.1 Storage format

The event log is saved in **JSON Lines** format (`log.jsonl`) on **LittleFS**, with an **append-only** approach: no existing row is ever modified. This file contains **only event detection** (not notification state, which lives in the separate log described in section 7). The log remains **single and shared** across all authorized users.

### 5.2 Record schema

```json
{"id": "<hex UUID v4, 32 characters>", "type": <enum, see 3.2>, "status": <enum, see below>, "ts": <Unix epoch, UTC>, "a": 1}
```

- **`id`**: a v4 UUID generated via the ESP32's hardware random generator (`esp_random()`), represented in **hexadecimal without dashes (32 characters)** instead of the standard dashed text format (36 characters) — a saving of 4 characters per occurrence, besides simplifying parsing. Requires no counter persisted in NVS. The `START`/`END` rows of the same duration event share the same `id`: `START` creates the ID, while `END` resolves and reuses the currently open ID for that event type, including after a reboot by inspecting `log.jsonl`. An `END` without an open matching `START`, or a duplicate `START` while the type is already open, is ignored. The `id` is never manually typed by users in the ordinary flow (see section 8: closing events happens via inline buttons).
- **`type`**: numeric enum value per the table in section 3.2, instead of a text string (e.g. `0` instead of `"ALARM_GENERAL"`).
- **`status`**: numeric enum value:

  | Value | Meaning |
  |---|---|
  | `0` | `START` |
  | `1` | `END` |
  | `2` | `INSTANT` |

- **`ts`**: Unix epoch (UTC) of the detection instant, written immediately and retroactively dated per section 3.3.
- **`a`** (*approximate*): timestamp-quality flag, see section 5.4. **Present only if `1`**; in the normal case (time synced via NTP) the field is **omitted**, so it costs nothing in storage for the vast majority of rows.

Concrete example:
```json
{"id":"a1b2c3d4e5f60718293a4b5c6d7e8f90","type":0,"status":0,"ts":1755500000}
```

**Note on maintainability**: the enum → meaning mapping (table in section 3.2.1) must be kept in a single place in the code (shared header) and never reassigned for values already in use, so as not to invalidate the meaning of rows already written to existing logs.

### 5.3 Timestamp formatting

Timestamps are **always stored as Unix epoch**. Conversion to a human-readable format and to the correct timezone happens exclusively at display time (`/log` command) or when sending a notification, according to the preferences **of the individual recipient user** (date format and timezone).

### 5.4 Timestamp reliability and monotonicity

The ESP32 has no battery-backed RTC: real time comes only from NTP, which requires connectivity. Since the most probable failure scenario is precisely the absence of network (section 1), a sensible timestamp must be guaranteed **even before the first NTP sync** — otherwise a reboot during a network outage would produce events dated 1970, with cascading effects on grace period, rotation, and ordering.

#### 5.4.1 Persistent time anchor (NVS)

- While time is valid (NTP synced), the system saves the current epoch to NVS (`last_epoch` key) at the **configured anchor persistence interval** (default: **6 hours**, admin-configurable via `/setanchorinterval`, section 11.1), as well as immediately after every successful NTP sync.
- At boot, before NTP is available, the working time is reconstructed as:

  ```
  estimated_ts = last_epoch + millis() / 1000
  ```

- Every row written with a time reconstructed this way carries the **`"a": 1`** flag. On the first successful NTP sync, the flag stops being applied to subsequent rows; **rows already written are not corrected retroactively**, consistent with the append-only nature of the log.
- **NVS wear**: at the default 6-hour interval, 4 writes per day of a single 64-bit value (was 144/day at the original 10-minute interval; even an admin-configured minimum of a few minutes stays well within NVS wear-leveling margins). NVS partition wear-leveling aggregates hundreds of writes per page before requiring an erase, bringing effective wear down to a negligible number of erase cycles per year against the useful life of the flash.

#### 5.4.2 Consequences of the `a` flag on behavior

| Area | Treatment of a row with `a: 1` |
|---|---|
| Notification | The timestamp is shown prefixed with `~` (e.g. `~14:02`) and the notification always carries the recovery prefix, **regardless of the grace period**: since the time gap isn't reliable, it makes no sense to decide based on it (see 6.4) |
| `/log` | Same `~` marking in the rendering |
| Rotation | Treated as a normal row: the anchor still guarantees a plausible value, not a 1970 that would trigger its immediate deletion |

#### 5.4.3 Guaranteed monotonicity

A backward NTP correction (or an anchor estimate above the real time) could produce non-increasing timestamps in an append-only file, breaking `/log` ordering, rotation age calculations, and duration reconstruction.

The system therefore keeps `last_written_ts` in RAM, **initialized at boot by reading the last line of `log.jsonl`** (reading backward from the end of the file, without a full scan), and applies a clamp before every write:

```
written_ts = max(calculated_ts, last_written_ts)
```

If the clamp fired, the row is marked with `a: 1`, because the value written no longer corresponds to the real instant of detection.

### 5.5 Schema version

The on-disk data format version is stored in NVS as an integer (`schema_ver` key, initial value `1`), and checked at every boot against the constant compiled into the firmware:

- **Match**: normal startup.
- **On-disk version older**: the migration foreseen for that version jump is executed; the NVS value is updated **only after** the migration succeeds.
- **On-disk version newer than the firmware** (accidental downgrade): the system **does not touch existing files**, enters degraded mode, and notifies admins. An old firmware rewriting data in a format it doesn't understand is the fastest way to lose the history.

The version must be incremented every time the structure of `log.jsonl`, of the notification files, or the meaning of an existing field changes. It must **not** be incremented for the simple addition of a new enum value at the end (an operation backward-compatible by construction).

---

## 6. Notification, recovery, and grace period logic

### 6.1 Normal flow

1. An event is detected (a confirmed transition on a pin per section 3.3, or internally generated: reboot, network issue) and immediately written to `log.jsonl` (section 5).
2. If the type's `notify_policy` calls for a notification for that `status` (section 3.2.1), a Telegram send is attempted to every whitelisted `chat_id` for whom that event type is enabled in their personal preferences (and whose `added_ts` precedes the event, see 4.6).
3. The outcome of the send for each recipient is classified per section 6.5 and tracked per the architecture in section 7 (Proposal E).

**Semantics of "delivery"**: the system tracks exclusively whether the message was **accepted by the Telegram API**, not whether the user received or read it. This is the correct guarantee to pursue: once Telegram has replied `ok: true`, delivery to the user's device is Telegram's responsibility, which it carries out even if the recipient is offline at that moment. There is no (nor is there a need for) any read-receipt mechanism.

### 6.2 Recovery scan

The notification log is **never scanned continuously**. The recovery scan runs exclusively on three occasions:

- **On boot** of the Arduino (always, one-time).
- **On connectivity restoration** after a `NETWORK_ISSUE` event (section 3.4).
- **On expiry of the scheduled retry timer** (see 6.3).

### 6.3 Scheduled retry

Every **transient** send failure (whether of a "new" notification or one already in recovery — see the classification in 6.5) is handled via a **non-blocking timer** (based on comparing `millis()`/current time, without polling the file), with a duration **configurable in minutes (default: 60)**:

- If a send **fails** and the timer isn't already active, it's **started** with the configured duration.
- If a send **fails** while the timer is already active (pending), the timer is **reset** to the full configured value.
- If a send **succeeds** (for any notification) while the timer is active, the timer **is not simply canceled**: an early recovery scan (6.2) fires immediately. The outcome of this scan determines the timer's final state, per the same rule that applies on natural expiry (next point).
- On timer **expiry** (natural or early), a recovery scan is run: if every pending send succeeds (or is marked `ABANDONED`), the timer is **canceled**; if even one fails transiently, the timer is **restarted** with the configured duration.

#### 6.3.1 Reentrancy protection (`scan_in_progress`)

The rule "a success triggers an early scan" refers exclusively to successes obtained in the **normal flow** (6.1), never to those obtained **inside** a recovery scan — which would otherwise recursively trigger a new scan on the first successful send.

The system therefore keeps a boolean **`scan_in_progress`** flag, set at the start of the recovery scan and cleared at its conclusion (even on early exit due to error). While the flag is active:

- No success can trigger an early scan.
- No new scan can be started: a scan request received in this window is simply discarded (the in-progress scan will cover it anyway, since it re-reads the complete pending state).

### 6.4 Grace period

For every pending notification found by recovery, at send time the time gap between the current instant and the `ts` of the original event (in `log.jsonl`) is computed:

- If the gap is **within the configured grace period** (default: **1 minute**), the notification is sent as a **normal notification**, with no delay indication.
- If the gap **exceeds the grace period**, the notification is sent with an explicit prefix (e.g. "⏪ Recovered notification") and the original timestamp formatted per the recipient's format and timezone.
- If the original timestamp carries the `a: 1` flag (section 5.4), the notification is **always** sent with the recovery prefix and with the timestamp marked `~`, regardless of the computed gap.

### 6.5 Classification of send outcomes

Not every failure deserves a retry: some API responses indicate a **permanent** condition that no number of retries will resolve. Without this distinction, a single chat that's no longer reachable (a user who blocked the bot) would keep the retry timer armed forever, with a useless HTTPS attempt every hour and a recurring flag in the periodic summary.

| Category | Detected condition | Treatment |
|---|---|---|
| **Success** | HTTP 200 with `"ok": true` in the body | Notification marked `RESOLVED` (section 7) |
| **Transient — network** | DNS/TCP/TLS failure, timeout, WiFi disconnected | `PENDING`, scheduled retry |
| **Transient — server** | HTTP 5xx | `PENDING`, scheduled retry |
| **Throttling** | HTTP 429 | Handled by the rate limiter (section 6.6), doesn't count as a failure until immediate retries are exhausted |
| **Permanent — recipient** | HTTP 403 (`bot was blocked by the user`, `user is deactivated`), HTTP 400 (`chat not found`) | Notification marked `ABANDONED`, **no further attempt**; admins notified with the `chat_id` involved |
| **System error** | HTTP 401 (invalid token), HTTP 404 on the endpoint | No send is possible to anyone: pending ones **stay `PENDING`**, the error is logged and exposed in `/status`. Nothing is marked `ABANDONED`, because the problem isn't the recipient's |

**Limit on transient attempts**: a notification that accumulates more than `max_retries` failed attempts (default: **24**, equal to 24 hours with the default retry interval) is marked `ABANDONED` and flagged in the summary, so that an unresolvable pending item doesn't stay protected from rotation indefinitely.

**Implementation note (FastBot2)**: unlike libraries that return a plain `bool`, FastBot2's `bot.sendMessage(msg)` (section 3.5) **returns an `fb::Result` directly**, which exposes everything needed for classification with no wrapper and no library modification:

```cpp
fb::Result r = bot.sendMessage(msg);

if (r.isEmpty()) {
    // No JSON body received: connection failure (DNS/TCP/TLS/timeout, or no
    // network connectivity at all) -> Transient - network. Checked first:
    // isError() only compares the parsed "ok" field, which stays false (not
    // true) when no response was received - a real success always has a
    // body, so isEmpty must win regardless of isError.
} else if (!r.isError()) {
    // Success: "ok": true in the body
} else {
    // JSON body with "ok": false: r.getErrorCode() and r.getError() mirror
    // exactly the error_code/description returned by Telegram
    int code = r.getErrorCode().toInt32();
    // 403/400 -> Permanent - recipient
    // 401/404 -> System error
    // 429     -> Throttling; retry_after via the internal parser:
    uint32_t retryAfter = r._parser["parameters"]["retry_after"];
    // 5xx     -> Transient - server
}
```

The `error_code` field returned in Telegram's JSON body **numerically matches** the HTTP status of the request (it's the same convention used by the Bot API), so the table above applies unchanged when reading `getErrorCode()` in place of the HTTP status. The distinction "no JSON body" (connection failure, `isEmpty()`) versus "JSON body with an error" (`isError()` with `error_code` populated) is what separates transient network failures from those explicitly reported by the API.

### 6.6 Rate limiting

The Telegram API enforces rate limits (roughly ~1 message per second per single chat and ~30 messages per second overall), beyond which it responds `HTTP 429` with a `parameters.retry_after` field indicating the required wait in seconds.

The at-risk scenario is exactly the one this design anticipates: on return from a prolonged outage, the system sends in a burst the open-events summary, the pending-notifications summary, and every recovered notification, multiplied by the number of users. Without rate control, a `429` would result, which would be counted as a failure, re-arming the retry timer and producing further `429`s on the next cycle.

**Mechanism adopted:**

- All sends go through a single point that enforces a minimum interval of **1100 ms between two consecutive messages**, regardless of the recipient. With the expected number of users (few), this single constraint comfortably covers both the per-chat and the global limit, without requiring two separate counters.
- The wait is implemented with a non-blocking timer: the loop keeps servicing the interrupt queue (section 3.3) during the pause.
- On receiving a **429**, the `retry_after` value is honored in full and the send is **retried up to 3 times** without being counted as a failure. Only after the third consecutive `429` is the notification marked `PENDING` and handed to the normal scheduled-retry mechanism.

### 6.7 Aggregation of recovery messages

A recovery scan (section 6.2) can find multiple pending notifications for the same user in the same cycle — typical after a prolonged network outage, where several events pile up. Sending them as separate messages, even while respecting the rate limiter in section 6.6, still produces a barely-readable burst and consumes more API calls than necessary.

**Aggregation threshold** (default: **3**, configurable, `/setaggregatethreshold`): if the number of pending notifications found for a given `chat_id` in a single scan exceeds the threshold, they're grouped into **a single summary message** instead of being sent one by one — but each still uses the exact same per-event template as an ordinary live notification (section 6.1: `<emoji> <label> <start/end marker> (<ts>)`), one line per record, individually marked recovered or not per its own gap against the grace period (section 6.4):

```
[recuperate] 4 notifiche:
- 🔄 Riavvio (22-08-2026 16:05:00)
- ⏪ [recuperata] 📡 Problema di rete ▶️ (22-08-2026 16:10:12)
- ⏪ [recuperata] 🚨 Allarme generale ▶️ (22-08-2026 16:17:54)
- ⏪ [recuperata] 🚨 Allarme generale ✅ (22-08-2026 16:20:29)
```

- Below the threshold, behavior remains the ordinary one from section 6.4 (one message per notification, with or without the recovery prefix depending on the grace period).
- The aggregated message does **not** always carry the "recovered" marker: each line decides independently, exactly as in the non-aggregated case — a mix of marked and unmarked lines in the same message is expected. `START`/`END` pairs for the same event are **not** merged into an interval with a computed duration; they remain two separate lines, as in the live flow.
- **Tracking**: aggregation is only a grouping in the formatting of the Telegram send — the state of each notification in `notif_<chat_id>.jsonl` (section 7) stays individual. If the aggregated message send succeeds, every notification in the group is marked `RESOLVED` individually; if it fails, every notification in the group stays (or returns) `PENDING`, with its own `n` counter incremented, per the ordinary rules in section 6.5.
- Aggregation applies only within a single scan for a single user: it never merges events from different users, nor notifications found in different scans.

---

## 7. Notification storage architecture

### 7.1 Decision

**Architecture chosen: Proposal E — per-chat inverse log.** A dedicated file for each authorized user, e.g. `notif_<chat_id>.jsonl`, containing **only** rows for sends that **did not succeed on the first attempt** (no row for sends that succeeded immediately). The alternative proposals evaluated and rejected are documented in section 7.3, for future reference.

The purpose of the log is to track **whether the message was successfully sent to the Telegram API for that user**, not whether the user received or read it (see 6.1).

### 7.2 Record schema

```json
{"id": "<event uuid, same hex format as 5.2>", "status": <enum, see below>, "ts": <Unix epoch>, "state": <enum, see below>, "n": <attempts>}
```

- **`id`**: same identifier as the event in `log.jsonl` (section 5.2), for correlation.
- **`status`**: which notification is being tracked, numeric enum:

  | Value | Meaning |
  |---|---|
  | `0` | `NOTIFIED_INSTANT` |
  | `1` | `NOTIFIED_START` |
  | `2` | `NOTIFIED_END` |

- **`ts`**: for a `PENDING` row, the epoch of the original event (useful for grace-period calculation); for a `RESOLVED` row, the epoch of the moment the send actually succeeded; for an `ABANDONED` row, the epoch of when it was given up on.
- **`state`**: numeric enum:

  | Value | Meaning |
  |---|---|
  | `0` | `PENDING` — send transiently failed, awaiting recovery |
  | `1` | `RESOLVED` — send subsequently succeeded |
  | `2` | `ABANDONED` — send definitively given up on (permanent error or `max_retries` exceeded, see 6.5) |

- **`n`**: number of failed send attempts accumulated so far. Present on `PENDING` and `ABANDONED` rows, omitted on `RESOLVED`. This is the field against which exceeding `max_retries` is evaluated.

Typical sequence for a single missed send later recovered:
```json
{"id":"a1b2c3d4e5f60718293a4b5c6d7e8f90","status":0,"ts":1755500000,"state":0,"n":1}
{"id":"a1b2c3d4e5f60718293a4b5c6d7e8f90","status":0,"ts":1755500910,"state":1}
```

**Behavior in the common case** (send succeeds on the first attempt, expected to be the vast majority of cases given the power guaranteed by the panel's battery): **no row is written** to this file. The file for a user whose bot is working normally stays empty or minimal almost all the time.

**Reconstructing pending state**: a notification is still to be recovered if a `PENDING` row exists for that `(id, status)` pair **not followed** by a `RESOLVED` or `ABANDONED` row for the same pair. Reconstruction happens by reading the file once, in streaming, while keeping an in-RAM map `(id, status) → most recent state`.

**Updating the attempt counter**: on every further failure, **a new row is not appended per attempt**; instead, a single updated `PENDING` row is appended with the new `n` value, which logically supersedes the previous one (the last row present for a given `(id, status)` pair is always the valid one). This keeps the file compact even during prolonged outages, and superseded rows are removed at the first rotation.

**Long-unresolved events**: a `PENDING` record approaching `max_retries` is flagged in the periodic summary together with open events (section 8), so as not to silently lose track of it. The transition to `ABANDONED` is in turn notified to admins.

**Removing a user**: consistent with the per-chat architecture, it's enough to delete the corresponding `notif_<chat_id>.jsonl` file, with no impact on other users.

### 7.3 Rejected alternative proposals

The following alternatives were evaluated but **not adopted**. They remain documented for reference, in case requirements emerge that make revisiting the choice preferable.

#### Proposal A — Single positive log, one row per (event, chat)

A single `notifications.jsonl` file, with one row for every successfully-notified event/recipient combination:
```json
{"id": "<event uuid>", "chat_id": 111, "status": "NOTIFIED_START", "ts": <send epoch>}
```

| Pros | Cons |
|---|---|
| Simple, consistent with the event-log schema | Writes and rows grow as *events × number of chats* |
| Complete audit trail of every notification received by every user | File grows faster, more flash wear, repeated recovery scans per chat |

#### Proposal B — Single positive log, one aggregated row per send round

A single row for every attempt "round", with the list of chats successfully served in that round:
```json
{"id": "<event uuid>", "status": "NOTIFIED_START", "ts": <epoch>, "chats": [111, 222]}
```

| Pros | Cons |
|---|---|
| Reduces writes (1 per round, not 1 per chat) | Still writes even in the trivial case (everyone receives it on the first try) |
| Keeps a complete positive log | More complex reconstruction (set union across multiple rows) |

#### Proposal C — Per-chat positive log (one file per user)

A separate file for each user, e.g. `notif_111.jsonl`, `notif_222.jsonl`, containing **all** successful sends (not just the recovered ones).

| Pros | Cons |
|---|---|
| Natural isolation: removing a user = deleting a file | As many files as there are chats |
| Independent rotation per user | Still writes on every success |

#### Proposal D — Inverse log (failed sends only), shared

Like the adopted Proposal E, but in a **single file shared** among all users instead of per-chat.

| Pros | Cons |
|---|---|
| Zero writes in the common case, like E | No per-user isolation: removing a user requires filtering/rewriting the shared file instead of deleting a dedicated one |
| Only one file to manage | Recovery scans still have to filter by chat within a more heterogeneous file |

#### Proposal F — Per-user watermark (no notification file)

A single integer per user (the epoch of the last successfully-notified event), saved in `userconfig.json`; recovery re-reads `log.jsonl` for all events with `ts` after the watermark. This would completely eliminate notification files and their rotation, unifying the mechanism with the `added_ts` filter from section 4.6.

| Pros | Cons |
|---|---|
| Removes an entire file, its rotation, and the sync with the event log | A failure on one event followed by a success on the next isn't representable: either the watermark is held back (risking a duplicate notification) or track of the failure is lost |
| No additional writes: the watermark lives in a file already rewritten for other reasons | No audit of which notifications were actually recovered |

Rejected in favor of Proposal E, which represents the exact state of every single notification with no ambiguity.

---

## 8. Open event management

If a duration event (e.g. `ALARM_GENERAL`, `ALARM_INTERNAL`, `ALARM_GARAGE`, `POWER_LOSS`, `NETWORK_ISSUE`) is still missing its `END` row at the time of an unexpected Arduino reboot:

- **It is not automatically closed.** It stays "open" in the log until manually handled.
- At boot, if open events predating the reboot are detected, the system sends a **summary message** to every authorized user listing the still-open events (type and start timestamp formatted per each recipient's preferences). The same summary also includes any `PENDING` notifications close to being abandoned (section 7.2).
- The same summary is also included at **every rotation cycle** (see section 9), as a periodic reminder.
- Open events are **always excluded from automatic deletion** during rotation, regardless of their age.

### 8.1 Closing via inline buttons

Manual closing happens **primarily via inline buttons** attached to the summary message, so as not to require the user to type a 32-character hex identifier from a smartphone — an impractical operation, required, on top of that, at exactly the least convenient moments.

- The summary message sent **to admins** includes an inline keyboard (`fb::InlineKeyboard`, section 3.5) with one button per open event, built dynamically in a loop (`addButton(label, data).newRow()` for each event — the number of open events isn't fixed), labeled readably (e.g. `Close: Garage alarm (14:02)`).
- The button's `callback_data` has the format `c:<id>` — 34 bytes, within the 64-byte limit imposed by Telegram.
- The summary sent to **standard users** is identical but **without buttons**: authorization isn't delegated to the command's mere invisibility.
- On receiving the callback query (`u.isQuery()` in FastBot2's `onUpdate` handler), the system:
  1. Re-evaluates the sender's authorization (`u.query().from().id()` must be an admin on the whitelist), **without trusting that the button was visible**: callbacks can be forwarded.
  2. Verifies that the indicated event (`u.query().data()`, parsing the `id` after the `c:` prefix) is still open (protection against a double-click and against a second admin who already closed the event).
  3. Writes the `END` row with `ts` equal to the moment of the click and triggers the normal notification flow for that `END`.
  4. Replies with `bot.answerCallbackQuery(u.query().id(), ...)` and updates the message's keyboard, removing the consumed button (`bot.editMenu(...)`, rebuilding the keyboard without the closed button). **Note**: if the query isn't explicitly answered, FastBot2 still sends an automatic empty reply after a timeout — the explicit call remains preferable regardless, to give immediate textual feedback ("Event closed") instead of leaving the spinner until the automatic timeout.

The text command `/closeevent <id> [timestamp]` (admin-only) remains available as a **fallback**, useful when the summary message is no longer reachable or when a close timestamp different from the current instant is wanted. The full `id` is obtainable from `/log`.

---

## 9. Log rotation

### 9.1 Retention policy

- The event log's validity period is **configurable in weeks** (default 52), settable only by admin users.
- Concluded events (with an `END` or `INSTANT` row present) older than the configured period are deleted.
- Still-open events (with no `END`) are **always protected** from deletion, regardless of age.
- `notif_<chat_id>.jsonl` files follow the same retention policy: `RESOLVED` and `ABANDONED` rows older than the configured period are removed during rotation; `PENDING` rows are always protected (consistent with the protection of open events). The transition to `ABANDONED` foreseen by section 6.5 guarantees that no pending item stays protected indefinitely.

### 9.2 Cadence

Given the available storage capacity (16 MB) and the low expected event volume, rotation runs on a **weekly cadence**, via a lightweight in-RAM check (comparison against the last-rotation timestamp, saved in NVS), with no file scan to determine whether rotation is due.

An **early** rotation can be triggered by available-space monitoring (section 9.4).

### 9.3 Execution mechanism

Since LittleFS doesn't support selective row deletion, rotation is implemented as a **filtered rewrite** of the entire file (for `log.jsonl` and for each `notif_<chat_id>.jsonl`).

#### 9.3.1 Two-pass algorithm with bounded RAM

Grouping *all* rows by `id` in memory would require RAM proportional to the file's size — unacceptable on a microcontroller, and a limit that would only manifest after months of operation. The algorithm is therefore two-pass, with **fixed, a-priori-known** memory usage:

1. **Pass 1 (collection)**: streaming read of the file, never holding it in memory. The set of **deletable** `id`s is built — events that have an `END`/`INSTANT` row and whose reference `ts` is earlier than the retention cutoff. IDs are stored in **16-byte binary form** (not as 32 hex characters), with a hard cap of **256 ids** ≈ **4 KB of RAM**. Once the cap is reached, collection stops.
2. **Pass 2 (rewrite)**: a second streaming read, writing to a temporary file every row whose `id` doesn't belong to the collected set.
3. **Repetition**: if pass 1 hit the 256-id cap, the entire cycle is **repeated** on the just-rewritten file, until a pass 1 completes without saturating the cap. Termination is guaranteed because every cycle removes at least one event.

This keeps memory usage constant regardless of file size, at the cost of multiple passes only in the (rare) cases of a substantial backlog.

#### 9.3.2 Commit order

The order of the final operations is binding for resilience to power interruptions:

1. Complete write of the temporary file.
2. `flush()` / `close()` of the temporary file.
3. **Atomic rename** over the original file (write-then-rename pattern; LittleFS guarantees rename atomicity even on power loss).
4. **Only after** the rename succeeds, update the last-rotation timestamp in NVS.

Reversing points 3 and 4 would mean a blackout in the intermediate window would register as "done" a rotation that changed nothing, skipping an entire weekly cycle. With the order given, the same blackout at most causes a rotation already done to be repeated — an idempotent, harmless operation.

A leftover temporary file from an interrupted rotation is detected and deleted at the next boot.

### 9.4 Filesystem space and error handling

The filesystem is a silent point of failure: a write that fails due to exhausted space or corruption would lose events with no one noticing.

**Space monitoring.** Usage (`LittleFS.usedBytes()` / `totalBytes()`) is checked at boot, after every rotation, and periodically:

| Threshold | Behavior |
|---|---|
| < 80% | Normal operation |
| ≥ 80% | Immediate **early rotation**, outside the weekly cadence, and admins notified |
| ≥ 95% | **Degraded mode**: admins notified; the event log continues to have write priority, while non-essential writes (`PENDING` notification rows) are suspended. Losing track of a notification attempt is preferable to losing event detection |

**Write errors.** Every write operation checks both the outcome of `open()` and the number of bytes actually written by `write()`/`println()` (a partial write on LittleFS doesn't raise an exception: it simply returns a lower count).

- On failure, the operation is retried once; if it fails again, a **filesystem error counter** is incremented.
- The counter is exposed in `/status` and the first error generates an admin notification.
- A write failure **does not block** the corresponding Telegram notification send: real-time notification has priority over history persistence.
- If `LittleFS.begin()` fails at boot, the system attempts a single `format()` **exclusively if the filesystem turns out to be unmountable** (never as a reaction to an error on a properly-mounted FS), and notifies admins of what happened: losing the history must always be a visible event, never a silent one.

---

## 10. Timezone and daylight-saving management

### 10.1 Preset timezones

The user chooses their own timezone from a **predefined set** of common options, avoiding the need to manually enter technical strings. Each option is internally mapped to a **POSIX-format TZ string**:

| Preset | Also covers | POSIX TZ string | DST |
|---|---|---|---|
| `UTC` | — (default) | `UTC0` | No |
| `Europe/Rome` | Italy | `CET-1CEST,M3.5.0,M10.5.0/3` | Yes (CET/CEST) |
| `Europe/Berlin` | Germany, France, Spain, Benelux (same timezone as Rome) | `CET-1CEST,M3.5.0,M10.5.0/3` | Yes (CET/CEST) |
| `Europe/London` | United Kingdom | `GMT0BST,M3.5.0/1,M10.5.0` | Yes (GMT/BST) |
| `Europe/Moscow` | European Russia | `MSK-3` | No |
| `America/New_York` | Eastern US | `EST5EDT,M3.2.0,M11.1.0` | Yes (EST/EDT) |
| `America/Los_Angeles` | Western US | `PST8PDT,M3.2.0,M11.1.0` | Yes (PST/PDT) |

`Europe/Rome` and `Europe/Berlin` share the same POSIX string (same timezone and same EU-wide DST-change rules): they're offered as distinct presets purely for selection convenience, not because they require a different rule. The list is meant to cover the expected family use (Italy as the main case, plus a few common timezones for relatives/friends elsewhere); it's extensible in the future by adding more rows to the table, with no impact on preferences already set by existing users.

### 10.2 Automatic daylight-saving (DST) handling

The POSIX format of the TZ string encodes both the standard offset and the daylight/standard time transition rule. By setting this string in the microcontroller's environment (`setenv("TZ", ...)` + `tzset()`, or the equivalent `configTzTime()`), every conversion from epoch to local date/time **automatically** handles the DST change, with no manual calculation and no external services.

### 10.3 Per-user timezone

The timezone is a **personal preference** saved in `userconfig.json` (section 4.4): each user sets their own timezone independently of the others. Since the `TZ` environment variable is global to the process, formatting for a specific recipient requires setting `TZ` immediately before the conversion and restoring it afterward (or concentrating every per-recipient formatting operation in a single, serialized point in the code).

---

## 11. Configuration

### 11.1 Global configuration (NVS, admin only)

| Parameter | Default | Command |
|---|---|---|
| Event/notification log validity period | 52 weeks | `/setretention <weeks>` |
| Notification recovery grace period | 5 minutes | `/setgraceperiod <minutes>` |
| Scheduled retry interval | 60 minutes | `/setretryinterval <minutes>` |
| Maximum number of attempts before giving up | 24 | `/setmaxretries <n>` |
| Duration threshold to generate `NETWORK_ISSUE` | 120 seconds | `/setnetthreshold <seconds>` |
| Recovered-notification aggregation threshold | 3 | `/setaggregatethreshold <n>` |
| NTP anchor persistence interval | 360 minutes (6 hours) | `/setanchorinterval <minutes>` |

Service NVS keys, not modifiable by command: `schema_ver` (5.5), `last_epoch` (5.4.1), last-rotation timestamp (9.3.2).

### 11.2 Per-user configuration (`userconfig.json`, each user on their own)

| Parameter | Default | Command |
|---|---|---|
| Date/time format | ISO 8601 | `/setdateformat <format>` |
| Timezone | `UTC` | `/settimezone <preset>` |
| Notified event types | All enabled | `/notify <event_type> on\|off` |

**Important note**: a user's personal whitelist of notified event types only filters **sending notifications to that specific user**, not **writing to the event log**. Every event is always recorded in the shared history, regardless of each user's notification preferences. (To instead disable a type system-wide, the `enabled` flag in the table in section 3.2.1 is used.)

---

## 12. Planned Telegram commands

| Command | Required permission | Function |
|---|---|---|
| `/log [n]` | Authorized user | Shows the last n **events** (not rows) from the log, aggregated and formatted per the requester's preferences — see 12.1 |
| `/status` | Authorized user | Current system status — see 12.2 |
| `/config` | Authorized user | Shows their own personal configuration (and, if admin, also the global one) |
| `/setdateformat <format>` | Authorized user | Sets their own date/time display format |
| `/settimezone <preset>` | Authorized user | Sets their own timezone from a predefined set |
| `/notify <event_type> on\|off` | Authorized user | Enables/disables the notification for one event type, for themselves |
| `/setretention <weeks>` | Admin | Sets the global log validity period in weeks |
| `/setgraceperiod <minutes>` | Admin | Sets the global grace period for notification recovery |
| `/setretryinterval <minutes>` | Admin | Sets the global scheduled-retry interval |
| `/setmaxretries <n>` | Admin | Sets the number of attempts beyond which a notification is abandoned |
| `/setnetthreshold <seconds>` | Admin | Sets the minimum duration of a connectivity outage for it to generate an event |
| `/setaggregatethreshold <n>` | Admin | Sets the threshold beyond which recovered notifications are grouped into a single message |
| `/setanchorinterval <minutes>` | Admin | Sets how often the NTP fallback time anchor is persisted to NVS |
| `/closeevent <id> [timestamp]` | Admin | Manually closes an event left open (text fallback for inline buttons, see 8.1) |
| `/adduser <chat_id>` | Admin | Adds a new `chat_id` to the whitelist |
| `/removeuser <chat_id>` | Admin | Removes a `chat_id` from the whitelist |
| `/promoteuser <chat_id>` | Admin | Changes the `admin` flag of an existing user, promoting them to `Admin` |
| `/resetusers` | Admin | Empties the whitelist back to default values (destructive operation, to be protected with confirmation) |

### 12.1 `/log` rendering

`/log` presents **aggregated events**, not individual file rows: `START`/`END` pairs sharing the same `id` are merged into a single row with the computed duration. The runtime event path guarantees that a duration `END` reuses the ID of the open `START`; unmatched `END` transitions are not rendered as standalone events.

```
Garage alarm         21/08 14:02 → 14:07  (5m)
Mains power loss      20/08 03:11 → 03:44  (33m)
Reboot                19/08 22:07
Internal alarm        19/08 08:30 → OPEN
```

**Implementation**: the file is read once, in streaming, keeping in RAM a **ring buffer of the last N aggregated events** (not of rows). Memory usage therefore depends on the requested `n` — with an imposed maximum cap — and not on the file's size. An event's `END` row updates the corresponding entry already present in the buffer; events still open are rendered explicitly as `OPEN`. Timestamps with the `a: 1` flag (section 5.4) are prefixed with `~`.

### 12.2 `/status` content

`/status` is the only tool for verifying system state, given the absence of an external observer (section 1.1). It must report:

- Uptime since last boot and the last reboot's cause (`esp_reset_reason()`)
- Current state of each monitored pin, with its type label
- List of currently open events
- WiFi status: connected/disconnected, SSID, RSSI, current backoff attempt
- Last successful NTP sync and current time validity (exact / estimated from anchor)
- Number of `PENDING` and `ABANDONED` notifications per user; retry-timer state
- LittleFS space used/total, filesystem error counter, degraded mode if any
- Interrupt queue overflow counter (section 3.3)
- Last system error logged (e.g. invalid token, section 6.5)
- Date of the last rotation performed

---

## 13. Robustness considerations

- **Galvanic isolation**: not necessary. The PGM outputs are used in relay configuration (dry contact), mechanically isolated from the panel's circuitry — no optoisolator required (section 2.1).
- **Loss of transitions during network I/O**: structurally solved by ISR + queue + retroactive dating (section 3.3); no transition can be lost due to a loop block.
- **Watchdog**: the Task WDT is enabled on the application loop (30 s timeout, above the 10 s network timeouts). A genuine block triggers a reboot, which in turn generates a notified `REBOOT` event — making a failure visible that would otherwise be silent.
- **Time synchronization**: the ESP32 has no battery-backed RTC; time is obtained via NTP on connection and periodically, in UTC. In the absence of NTP the system uses the time anchor persisted in NVS and marks timestamps as approximate (section 5.4). Conversion to local time (with automatic DST handling) happens only at presentation time, per each user's timezone.
- **Timestamp monotonicity**: guaranteed by a clamp on the last value written (section 5.4.3), a necessary precondition for rotation, ordering, and duration calculation.
- **Power**: guaranteed by the panel's backup battery; reboots due to power interruption are expected to be rare, unlike connectivity-only interruptions.
- **Single point of failure (connectivity)**: if the network connection drops, real-time notifications aren't possible; the recovery mechanism (grace period + scheduled retry) mitigates notification loss, but doesn't eliminate the delay in their delivery beyond the configured thresholds.
- **Total failure not automatically detectable**: by explicit choice (section 1.1) there's no heartbeat or external observer. A total hardware failure (power, a burned-out device) generates no alert: verification is up to the user via `/status`. This is the consciously accepted limit of the autonomous architecture.
- **Data integrity**: the write-then-rename pattern for rotation and for configuration/whitelist files, combined with the append-only approach for ordinary log writing, minimizes the risk of corruption on a power interruption. The rotation commit order (section 9.3.2) is binding.
- **Filesystem space and errors**: actively monitored with thresholds and a degraded mode (section 9.4); no write failure stays silent.
- **Permanent send failures**: distinguished from transient ones via API response classification (section 6.5), to avoid perpetual retries toward unreachable recipients.
- **Rate limiting**: sends are serialized with a minimum interval and `429` is handled by honoring `retry_after` (section 6.6), so a recovery burst doesn't turn into an error loop.
- **Transport security**: the TLS connection to `api.telegram.org` uses `setInsecure()`, with no server certificate validation. A conscious choice: it avoids the silent, hard-to-diagnose failure that occurs when a root CA pinned in the firmware expires or is rotated by Telegram, a situation in which the bot would stop working **precisely without being able to notify anyone of the problem**. The residual exposure is to a man-in-the-middle attack on the local network, considered outside the threat model of a home installation.
- **Access security**: no response is ever sent to a `chat_id` not present on the whitelist. Notifications are sent exclusively to authorized `chat_id`s, and only for events following their addition (see 4.6). Authorization is also re-evaluated on inline-button callback queries (section 8.1).
- **Secrets**: token and credentials in an unversioned `secrets.h`, in plaintext in flash; threat model and mitigation documented in section 4.7.
- **Flash wear**: the expected write volume for the event log (rare events) and for the notification log (writes only on failures, with the adopted Proposal E) is amply compatible with the internal flash memory's lifespan. The NVS time anchor adds 144 writes per day of a single scalar value, absorbed by wear-leveling (section 5.4.1).
- **Storage compactness**: the use of numeric enums for `type`/`status`/`state` and of a compact hexadecimal UUID format (32 characters) reduces the average size of each row. Note, however, that with 16 MB available and the expected event volume, **storage is not a design constraint**: future decisions should be made favoring readability, diagnosability, and usability over byte savings.

---

## 14. Recorded design decisions

| Topic | Decision |
|---|---|
| External dependencies | None: autonomous device, direct communication with Telegram, to shorten the failure chain |
| PGM interfacing | Relay outputs (dry NA/NC contact) on all zones; no additional isolation needed; prefer NA contacts on any ESP32-S3 strapping pins |
| Concurrency model | ISR `IRAM_ATTR` + FreeRTOS queue + debounce and retroactive dating in the loop; LittleFS access only from the loop (no mutex needed) |
| Event log structure | JSON Lines, append-only, contains only detection (`START`/`END`/`INSTANT`), not notifications |
| Instant events (e.g. REBOOT) | Dedicated enum value for `INSTANT` |
| Network events | Single `NETWORK_ISSUE` type with `START`/`END` in the log, but **only `END` notified**, with the outage duration in the text |
| Network-issue threshold | Absence of connectivity to the Telegram API (not just WiFi) for more than 120 s (configurable) |
| WiFi reconnection | Non-blocking exponential backoff 5→300 s, reset on reconnection |
| Alarm types | Distinct zones as separate types: `ALARM_GENERAL`, `ALARM_INTERNAL`, `ALARM_GARAGE`, each with its own dedicated pin/PGM |
| `ALARM_GENERAL`/zone overlap | Duplication accepted; mitigated by per-type enablement, both per-user (`/notify`) and global (`enabled` flag) |
| Power outage | `POWER_LOSS` type, an event with duration (`START`/`END`), detected via the panel's "mains fault" PGM |
| Storage optimization | `type`, `status` (and `state` for notifications) as numeric enums instead of text strings; storage is nonetheless not a design constraint |
| Event id format | v4 UUID in hex without dashes, 32 characters (unchanged) |
| Closing open events | **Inline buttons** (FastBot2 `InlineKeyboard`, built dynamically) in the admin summary message (`callback_data` `c:<id>`), with `/closeevent` as a text fallback |
| Timestamp without NTP | NVS time anchor saved at the configured interval (default 6h); reconstruction via `last_epoch + millis()`; `a: 1` flag on approximate rows |
| Timestamp monotonicity | Clamp on `last_written_ts`, initialized at boot from the log's last row |
| Schema version | Integer in NVS (`schema_ver`), checked at boot; downgrade → degraded mode without touching data |
| Notification storage | **Proposal E adopted**: inverse log, one file per chat (`notif_<chat_id>.jsonl`); proposals A-D and F rejected but documented in 7.3 |
| Notification-tracking semantics | Tracks acceptance by the Telegram API, not receipt or reading by the user |
| Telegram client | **FastBot2** (not UniversalTelegramBot): `sendMessage()` returns an `fb::Result` with direct access to `error_code`/`description`/`retry_after`, no wrapper needed for the classification in section 6.5 |
| Telegram polling mode | `Long` (asynchronous long polling, 60 s timeout); a send issued outside the `onUpdate` handler can incur ~1 s of reconnection blocking, covered by the ISR + queue architecture from 3.3 |
| JSON library | ArduinoJson for every project file (unchanged); GSON used only internally by FastBot2 to parse Telegram responses — two independent libraries, coexistence by choice |
| Send outcomes | Classified as success / transient / throttling / permanent-recipient / system error; retry only on transient ones; obtained from `fb::Result` with no wrapper |
| Notification terminal state | `ABANDONED` on permanent error or `max_retries` exceeded (default 24), with an `n` counter in the record |
| Rate limiting | Minimum 1100 ms interval between sends; `429` honored via `retry_after` and up to 3 immediate retries |
| Recovered-notification aggregation | Above a configurable threshold (default 3, per user per scan) pending notifications are grouped into a single message instead of N separate ones |
| Notifications recovered within the grace period (5 min default) | Sent as normal notifications, with no "recovery" prefix |
| Notifications recovered beyond the grace period | Sent with an explicit prefix and the original timestamp; always with the prefix if the timestamp is approximate |
| Unsuccessful recovery | Scheduled retry timer (default 60 min): reset on transient failure; a successful send in the normal flow triggers an early scan, protected against reentrancy by `scan_in_progress` |
| Open events after reboot | Not automatically closed, flagged via Telegram, closable with an inline button (admin only) |
| Events/notifications pending beyond the retention period | Always excluded from automatic deletion |
| Rotation cadence | Weekly, retention unit in weeks, applied to both the event log and the per-chat notification files |
| Rotation: RAM | Two passes with an id set capped at 256 (16 bytes each, ~4 KB), repeated as needed |
| Rotation: commit | Atomic rename first, NVS update after; leftover temp files cleaned up at boot |
| Filesystem full / errors | Thresholds at 80% (early rotation) and 95% (degraded mode); bytes-written check; error counter in `/status` |
| Event id generation | Random UUID via hardware RNG, no NVS counter needed |
| Access control | `chat_id` whitelist in `users.json`, no response to unauthorized senders, re-evaluated on callback queries |
| `chat_id` type | `int64_t` everywhere (groups/supergroups exceed 32 bits) |
| Permissions | Single boolean `admin` flag (no granular permissions for now) |
| Initial onboarding | Initial `chat_id` in `secrets.h`, automatically promoted to admin on first boot |
| Whitelist management | Dedicated commands: `/adduser`, `/removeuser`, `/promoteuser`, `/resetusers` |
| Secrets | Separate `secrets.h` file, in plaintext, unversioned (with a versioned `secrets.h.example`) |
| TLS | `setInsecure()`, to avoid the silent failure from certificate rotation |
| Status monitoring | No automatic heartbeat; manual verification via `/status` |
| Global configuration | NVS (retention, grace period, retry interval, max retries, network threshold) |
| Per-user configuration | JSON file on LittleFS (`userconfig.json`): date format, timezone, notified event types |
| `/log` rendering | Aggregated events with duration (`14:02 → 14:07, 5m`), ring buffer of the last N events |
| Timezone | Set of predefined presets mapped to POSIX TZ strings (UTC, Europe/Rome, Europe/Berlin, Europe/London, Europe/Moscow, America/New_York, America/Los_Angeles), automatic DST handling, default UTC, per-user preference |
| Power | Supplied by the panel (with backup battery): blackout reboots rare, network interruptions more likely |

---

## 15. Next steps

- Evaluate (optional, discussed separately) introducing commands to arm/disarm the alarm remotely — requires verifying hardware support on the panel and a careful additional security analysis before being formalized in this document.
