# Configuration

There are three layers of configuration, at three different scopes and stored in three different places.

## 1. Secrets (`secrets.h`, not versioned)

Copied from [`secrets.h.example`](../secrets.h.example) and filled in before the first build:

```c
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

#define TELEGRAM_BOT_TOKEN "123456789:AAExampleTokenReplaceMe"

// Initial chat_id, automatically promoted to admin on first boot
#define ONBOARDING_CHAT_ID 111111111LL
```

`secrets.h` is `.gitignore`d — only `secrets.h.example` (with placeholder values) is committed, so a clean checkout fails to compile with a clear error rather than silently building with bogus credentials.

`ONBOARDING_CHAT_ID` only matters once: on the very first boot, if `users.json` is empty or missing, that chat ID is automatically added as the first admin. After that, whitelist management happens entirely through Telegram commands (`/adduser`, `/promoteuser`, etc. — see [commands.md](commands.md)) or by editing `users.json` directly on the device.

Values are stored **in plaintext** in flash — a deliberate choice explained in [architecture.md](architecture.md#known-deliberately-accepted-limitations).

## 2. Global configuration (NVS, admin-managed)

Seven values, all admin-settable via Telegram (`/setretention`, `/setgraceperiod`, `/setretryinterval`, `/setmaxretries`, `/setnetthreshold`, `/setaggregatethreshold`, `/setanchorinterval` — see [commands.md](commands.md) for exact syntax and defaults). Persisted to NVS immediately on change, loaded once at boot into a shared in-RAM struct (`globalConfig()`, see [`src/config/GlobalConfigStorage.h`](../src/config/GlobalConfigStorage.h)) that every consumer reads from directly — no reboot needed for a change to take effect.

| Setting | Default | Unit |
|---|---|---|
| Log/notification retention | 52 | weeks |
| Grace period | 5 | minutes |
| Retry interval | 60 | minutes |
| Max retries before abandoning | 24 | attempts |
| Network-issue threshold | 120 | seconds |
| Aggregation threshold | 3 | pending notifications |
| NTP anchor persistence interval | 360 | minutes |

## 3. Per-user preferences (`userconfig.json`, self-service)

Each authorized user manages their own via `/setdateformat`, `/settimezone`, and `/notify` (see [commands.md](commands.md)):

| Preference | Default |
|---|---|
| Date/time format | `%Y-%m-%dT%H:%M:%SZ` (ISO 8601) |
| Timezone | UTC |
| Per-event-type notifications | all enabled |

Disabling a notification type only stops *delivery to that user* — the event is still always recorded in `log.jsonl` and visible to everyone via `/log`. To disable a type system-wide instead (e.g. because it's not wired up), flip its `enabled` flag in the `EVENT_TYPES` table (see [hardware-setup.md](hardware-setup.md)) — that's a firmware-level decision, not a runtime one.

See [data-model.md](data-model.md#userconfigjson--per-user-preferences) for the on-disk schema of this file.
