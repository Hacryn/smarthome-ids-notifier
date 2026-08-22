# Telegram Commands

Every command is checked against the whitelist first: a message from a `chat_id` not in `users.json` gets no response at all, not even an error — this is deliberate (see [DESIGN.md](../DESIGN.md) §4.2), so an unauthorized user (or someone who's merely guessed the bot's username) can't tell the bot exists or is functioning. A whitelisted-but-non-admin user attempting an admin-only command *does* get an explicit rejection, since that user already knows the bot exists.

Implemented in [`src/telegram/CommandRouter.cpp`](../src/telegram/CommandRouter.cpp); argument parsing in [`src/telegram/CommandParser.cpp`](../src/telegram/CommandParser.cpp).

## Available to any authorized user

| Command | Effect |
|---|---|
| `/log [n]` | Shows the last `n` aggregated events (`START`/`END` pairs sharing the same event ID, merged with a computed duration), most recent first. Default `n` is 10, capped at 50. Still-open events show `APERTO`; approximate timestamps are prefixed `~`. |
| `/status` | Uptime (`<d>g <h>h <m>m <s>s`) and last reset cause, alarm state per type (`ALARM_GENERAL`/`ALARM_INTERNAL`/`ALARM_GARAGE`, from the logged open event rather than the raw pin level), 230V mains state (from the `POWER_LOSS` open event), WiFi status (SSID/RSSI or backoff attempt), NTP sync status, open-event count, pending/abandoned notification counts and retry-timer state, LittleFS usage and write-error count (with degraded-mode flag), ISR queue overflow count, last rotation timestamp (formatted per the caller's preferences), last system-level send error. |
| `/config` | Shows the firmware version, your own preferences (date format, timezone, disabled notification types). Admins also see the current global configuration. |
| `/setdateformat <format>` | Sets your own display date/time format — a literal `strftime()` pattern, e.g. `/setdateformat %d/%m/%Y %H:%M`. Everything after the command is taken verbatim. |
| `/settimezone <preset>` | Sets your own timezone. `<preset>` is one of the names in [`src/config/TimezonePresets.h`](../src/config/TimezonePresets.h): `UTC` (default), `Europe/Rome`, `Europe/Berlin`, `Europe/London`, `Europe/Moscow`, `America/New_York`, `America/Los_Angeles`. |
| `/notify <type> on\|off` | Enables/disables notifications for one event type, for you only — the event is still always written to the log. `<type>` is the type's `commandName` from `EVENT_TYPES`: `REBOOT`, `POWER_LOSS`, `NETWORK_ISSUE`, `ALARM_GENERAL`, `ALARM_INTERNAL`, `ALARM_GARAGE`. |

## Admin only

| Command | Effect |
|---|---|
| `/closeevent <id> [timestamp]` | Manually closes an open event by its full 32-char ID (from `/log` or the open-events summary). Optional explicit close epoch; defaults to now. This is the text fallback — closing via the inline button on the open-events summary is the primary path and doesn't require typing the ID. |
| `/adduser <chat_id>` | Adds a chat ID to the whitelist as a standard (non-admin) user. |
| `/removeuser <chat_id>` | Removes a chat ID from the whitelist. |
| `/promoteuser <chat_id>` | Grants the admin flag to an already-whitelisted user. |
| `/resetusers CONFERMA` | **Destructive.** Empties the whitelist entirely. Requires the literal confirmation word `CONFERMA` in the same message — there's no multi-step confirmation flow, by design (see [testing.md](testing.md) for why that keeps the command handler stateless). Note this also locks out whoever ran it; the whitelist is only repopulated (with the `secrets.h` onboarding admin) on the next reboot. |
| `/setretention <weeks>` | Global log/notification retention period. Default 52. |
| `/setgraceperiod <minutes>` | How long after an event before a recovered notification gets the "recovered" prefix. Default 5. |
| `/setretryinterval <minutes>` | Delay before a failed notification's next scheduled retry scan. Default 60. |
| `/setmaxretries <n>` | Failed attempts after which a pending notification is abandoned. Default 24. |
| `/setnetthreshold <seconds>` | Minimum outage duration before a `NETWORK_ISSUE` event is logged. Default 120. |
| `/setaggregatethreshold <n>` | Pending-notification count above which a recovery batch is sent as one aggregated message instead of one per event. Default 3. |
| `/setanchorinterval <minutes>` | How often the NTP fallback time anchor is persisted to NVS while time is valid. Default 360 (6 hours). |
| `/dump <target> [chat_id]` | Sends the raw target file as a Telegram document (debug dump, no reformatting). `<target>` is `log` (`log.jsonl`), `notif <chat_id>` (`notif_<chat_id>.jsonl`), `userconfig <chat_id>` (the whole `userconfig.json` — not filtered, it's already indexed by `chat_id` internally), or `users` (`users.json`). Replies with an error if the file doesn't exist yet or the send fails. |
| `/resetlog CONFERMA` | **Destructive.** Deletes `log.jsonl` (event history), also resetting the in-memory monotonicity baseline. Same `CONFERMA`-in-message pattern as `/resetusers`. |
| `/resetnotif <chat_id> CONFERMA` | **Destructive.** Deletes `notif_<chat_id>.jsonl` for the given user, clearing their pending/abandoned/retry state. |
| `/resetuserconfig <chat_id> CONFERMA` | **Destructive.** Removes the given user's entry from `userconfig.json`, reverting them to the preference defaults. |

All seven `/setXxx` global-config commands persist immediately to NVS and take effect on the next relevant check — no reboot required.

`chat_id` arguments accept negative numbers (Telegram group/supergroup IDs).
