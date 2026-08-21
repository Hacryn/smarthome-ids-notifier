# Data Model

All persistent state lives either on LittleFS (JSON / JSON Lines files) or in NVS (scalar key/value pairs via `Preferences`). Every LittleFS file that's rewritten wholesale (not appended) uses a write-then-atomic-rename pattern: write to a `.tmp` path, verify the byte count written, then `LittleFS.rename()` over the real path. Any stale `.tmp` file left behind by a rotation interrupted mid-write is cleaned up at the next boot.

## `log.jsonl` — event log

Append-only, one JSON object per line, written by [`src/events/EventLogStorage.h`](../src/events/EventLogStorage.h). Never modified in place; rotation rewrites the whole file (see [architecture.md](architecture.md#rotation-and-space-monitoring)).

```json
{"id":"a1b2c3d4e5f60718293a4b5c6d7e8f90","type":12,"status":0,"ts":1755500000,"a":1}
```

| Field | Meaning |
|---|---|
| `id` | 32-hex-char event ID (16 random bytes from `esp_random()`, no dashes). `START`/`END` rows of the same duration event share an `id`. |
| `type` | `EventType` enum value — see the table in [`src/events/EventTypes.h`](../src/events/EventTypes.h) (`REBOOT=0`, `POWER_LOSS=1`, `NETWORK_ISSUE=2`, `ALARM_GENERAL=10`, `ALARM_INTERNAL=11`, `ALARM_GARAGE=12`). Values are never reassigned; new types are appended. |
| `status` | `0`=`START`, `1`=`END`, `2`=`INSTANT` (single-row events like `REBOOT`). |
| `ts` | Unix epoch (UTC) of detection, retroactively dated (see [architecture.md](architecture.md)). |
| `a` | Present (`1`) only when the timestamp is approximate — omitted entirely otherwise, so it costs nothing on the common case. |

### Monotonicity

Because the file is append-only, a backward NTP correction or an over-optimistic anchor estimate could otherwise produce a non-increasing timestamp sequence, breaking `/log` ordering, rotation age calculations, and duration reconstruction. The firmware keeps `last_written_ts` in RAM (seeded at boot by reading the file's last line backwards, without a full scan) and clamps every write:

```
ts_written = max(ts_calculated, last_written_ts)
```

If the clamp fires, the row is marked `a:1` even if NTP is synced — the value no longer reflects the true detection instant. Pure logic in [`src/time/TimeAnchor.h`](../src/time/TimeAnchor.h).

## `notif_<chat_id>.jsonl` — per-user delivery tracking

One file per authorized user, containing **only** rows for sends that didn't succeed on the first attempt (see [`src/notifications/NotificationRecord.h`](../src/notifications/NotificationRecord.h)). A healthy bot's file for a given user stays empty or near-empty. Negative chat IDs (Telegram groups) get their sign replaced with a `g` prefix in the filename (`notif_g1001234567890.jsonl`), since a filename can't start with `-`.

```json
{"id":"a1b2c3d4e5f60718293a4b5c6d7e8f90","status":1,"ts":1755500000,"state":0,"n":1}
{"id":"a1b2c3d4e5f60718293a4b5c6d7e8f90","status":1,"ts":1755500910,"state":1}
```

| Field | Meaning |
|---|---|
| `id` | Same ID as the corresponding `log.jsonl` row. |
| `status` | Which notification: `0`=`NOTIFIED_INSTANT`, `1`=`NOTIFIED_START`, `2`=`NOTIFIED_END`. |
| `ts` | For a `PENDING` row: the original event's timestamp. For `RESOLVED`: when the send actually succeeded. For `ABANDONED`: when it was given up on. |
| `state` | `0`=`PENDING`, `1`=`RESOLVED`, `2`=`ABANDONED`. |
| `n` | Failed-attempt counter. Present on `PENDING`/`ABANDONED`, omitted on `RESOLVED`. |

**Reconstructing current state**: for a given `(id, status)` pair, the *last* row in the file wins — the file is folded in one streaming pass into an in-memory map (see [`src/notifications/NotificationFolder.h`](../src/notifications/NotificationFolder.h)), never fully materialized beyond that map. A pending notification with no later `RESOLVED`/`ABANDONED` row for the same pair is still outstanding.

Rotation removes `RESOLVED`/`ABANDONED` rows older than the retention period; `PENDING` rows are always protected regardless of age (a notification can only leave `PENDING` by resolving, being abandoned after `max_retries`, or being permanently undeliverable).

## `users.json` — whitelist

```json
{
  "authorized": [
    {"chat_id": 111111111, "admin": true, "added_ts": 1755000000},
    {"chat_id": -1001234567890, "admin": false, "added_ts": 1755600000}
  ]
}
```

`chat_id` is always `int64_t` (groups/supergroups exceed 32 bits). `added_ts` gates which historical events a newly-added user gets caught up on: events older than their `added_ts` are excluded from notification (both live and recovery flows) — `/log` history is unaffected and remains fully visible to anyone authorized. Rewritten in full on every change. Pure model + JSON (de)serialization in [`src/users/UserList.h`](../src/users/UserList.h); LittleFS I/O in [`src/users/UserStorage.h`](../src/users/UserStorage.h).

## `userconfig.json` — per-user preferences

A single JSON object keyed by `chat_id` (as a string, since JSON object keys are always strings):

```json
{
  "111111111": {"date_format": "%d/%m/%Y %H:%M", "timezone": 1, "notify_disabled": [11]},
  "-1001234567890": {"date_format": "%Y-%m-%dT%H:%M:%SZ", "timezone": 0, "notify_disabled": []}
}
```

| Field | Meaning |
|---|---|
| `date_format` | A `strftime()` format string. Default `"%Y-%m-%dT%H:%M:%SZ"` (ISO 8601). |
| `timezone` | A `TimezonePreset` enum value — see [`src/config/TimezonePresets.h`](../src/config/TimezonePresets.h). Default `0` (UTC). |
| `notify_disabled` | List of `EventType` values this user has turned notifications *off* for (empty = everything enabled). This filters delivery only — the event is still always written to `log.jsonl`. |

A user with no entry gets all-default preferences (`findOrDefaultUserConfig`). Model + serialization in [`src/config/UserConfig.h`](../src/config/UserConfig.h); storage in [`src/config/UserConfigStorage.h`](../src/config/UserConfigStorage.h).

## NVS keys (`Preferences`, namespace `"notifier"`)

| Key | Written by | Meaning |
|---|---|---|
| `last_epoch` | [`TimeAnchorStorage`](../src/time/TimeAnchorStorage.h) | Fallback time anchor (see [architecture.md](architecture.md#time-ntp-with-a-persistent-fallback-anchor)). |
| `last_rotation` | [`RotationStorage`](../src/rotation/RotationStorage.h) | Epoch of the last successfully committed log rotation. |
| `cfg_retention_w`, `cfg_grace_s`, `cfg_retry_min`, `cfg_max_retry`, `cfg_net_thr_s`, `cfg_agg_thr` | [`GlobalConfigStorage`](../src/config/GlobalConfigStorage.h) | The six admin-configurable global settings — see [configuration.md](configuration.md). |

## In-memory-only state

Not persisted anywhere, rebuilt fresh on every boot: the pin debounce state machines, the outbound message queue, the notification retry timer, the WiFi backoff attempt counter, the filesystem error counter, and the NTP-sync status. None of it needs to survive a reboot — a fresh boot naturally re-derives everything relevant from `log.jsonl`, `notif_*.jsonl`, and NVS.
