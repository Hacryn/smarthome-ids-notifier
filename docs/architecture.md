# Architecture

## Design constraint: no external dependencies

The device is deliberately autonomous: no MQTT broker, no home automation hub, no local server. The ESP32 talks directly to the Telegram Bot API and handles persistence, history, retry, users, and preferences itself. Every intermediate hop would be an additional point of failure between an alarm and the phone in your pocket — the whole architecture optimizes for shortening that chain, at the cost of more firmware complexity than a "just publish to MQTT" design would need.

One consequence: there's no external watcher that would notice if the device died entirely. State is checked manually via `/status` (see [commands.md](commands.md)), not via an automatic heartbeat that would generate unwanted recurring messages.

## Concurrency model: detection vs. network I/O

The main loop makes synchronous HTTPS calls to Telegram (long-polling `getUpdates`, `sendMessage`, TLS handshakes). On ESP32 these block execution for **seconds**, up to the configured timeout under degraded network conditions. A debounce implemented by polling GPIOs in the main loop would lose any transition that happened during one of those blocks — potentially the alarm signal itself.

The fix is interrupt + queue + retroactive dating:

1. Every monitored pin gets an `attachInterruptArg()` in `CHANGE` mode. The ISR (`IRAM_ATTR`) does exactly one thing: push `{event_type_index, level, millis()}` onto a FreeRTOS queue via `xQueueSendFromISR()`. No allocation, no I/O, no blocking call inside the ISR.
2. The queue has a fixed depth of 32. On overflow, a counter increments (exposed in `/status`) rather than silently dropping data.
3. The main loop drains the queue as soon as it's free and applies debounce (default 300 ms) against the `millis()` timestamps captured *by the ISR*, not against processing time: a transition is confirmed only if no further transition on the same pin follows within the debounce window.
4. **Retroactive dating**: the event timestamp isn't "now" but is reconstructed from the ISR's `millis()` capture:

   ```
   event_ts = current_epoch - (millis_now - millis_at_isr) / 1000
   ```

   An alarm that fired while the loop was stuck in a 10-second TLS timeout is still dated correctly, not 10 seconds late.

**Invariants this establishes, and that any change to this code must preserve:**
- No transition is ever lost, regardless of how long a network call blocks.
- All LittleFS access happens exclusively from the main loop. The ISR never touches the filesystem — no mutex is needed because of this, not despite it.
- No dedicated FreeRTOS task exists for pin detection; retroactive dating makes one unnecessary, keeping the firmware single-flow.
- A hardware Task Watchdog (30 s timeout, above the ~10 s network timeout) is enabled on the loop task. A genuine hang triggers a reset, which itself produces a notified `REBOOT` event — turning a silent failure into a visible one.

Implementation: [`src/pins/PinMonitor.h`](../src/pins/PinMonitor.h) (ISR + queue), [`src/pins/PinDebounce.h`](../src/pins/PinDebounce.h) (debounce state machine, pure), [`src/events/EventTiming.h`](../src/events/EventTiming.h) (retroactive dating formula, pure).

## Non-blocking outbound queue

The same "never block the loop" constraint applies to sending. A single alarm event can require notifying several users; rate-limiting each Telegram send to a minimum 1.1 s interval (Telegram's API limits) while doing that synchronously would stall the loop for seconds. Instead, `NotificationEngine` (see [modules.md](modules.md)) enqueues one `OutboundMessage` per recipient and drains the queue one message per loop tick, gated by a non-blocking rate limiter. This is the one significant departure from a literal reading of the original design notes, forced by the same constraint that motivates the ISR/queue split above.

## Event log as the single source of truth

`log.jsonl` (append-only, LittleFS) is the canonical detection record — every event is written here regardless of whether any notification about it ever succeeds. Notification delivery state is tracked separately, per recipient, in `notif_<chat_id>.jsonl` files, using an inverse log (only failed/pending deliveries are recorded — a healthy bot's file stays empty). See [data-model.md](data-model.md) for both schemas.

This separation is what makes recovery possible: after a connectivity gap, the firmware doesn't need to "guess" what happened — it re-derives pending notifications by folding each user's delivery log and cross-referencing `log.jsonl` for the event details (type, approximate-timestamp flag) that aren't duplicated in the smaller per-user files.

## Time: NTP with a persistent fallback anchor

The ESP32 has no battery-backed RTC. `src/time/Clock.h` is the single source of "what time is it" for the whole firmware:

- On WiFi connect (first connection and every reconnect), it starts an NTP sync in UTC.
- Until the first sync succeeds, `currentEpoch()` falls back to `last_epoch_anchor + millis()/1000`, where the anchor is persisted to NVS every `anchorPersistIntervalMinutes` (admin-configurable, default 360 = 6 hours, via `/setanchorinterval`) while time is valid, and immediately after every successful sync. This is what lets a reboot during a network outage still produce plausible (if approximate) timestamps instead of dating everything to 1970.
- Every event row carries an `approx` flag (`"a":1` in the JSON, rendered as a `~` prefix) whenever it wasn't derived from a live NTP-synced clock, or whenever [monotonicity clamping](data-model.md#monotonicity) had to correct it.

## Status LED

`loop()` also drives the Nano ESP32's built-in RGB LED as a glanceable state indicator, non-blocking (no `delay()` — blink timing comes from `millis()`). The open-alarm/power-loss check behind it is re-read from the log every 2 s rather than tracked incrementally, so it can't drift out of sync with a manual `/closeevent`. See [hardware-setup.md](hardware-setup.md#status-led) for the color mapping and [`src/diagnostics/StatusLedPolicy.h`](../src/diagnostics/StatusLedPolicy.h) for the priority rule.

## Rotation and space monitoring

LittleFS has no selective row deletion, so weekly rotation is a filtered full rewrite: read the file in streaming, collect deletable event IDs (closed events past the retention cutoff) up to a fixed 256-ID cap to bound RAM, rewrite to a temp file, and atomically rename over the original — the rename happens *before* the "last rotation" NVS timestamp is updated, so a power loss mid-rotation is idempotent, never a skipped cycle. Filesystem usage is checked at boot, after every rotation, and periodically; crossing 80% forces an early rotation, crossing 95% suspends non-essential writes (pending-notification rows) while keeping event detection writes prioritized. See [`src/rotation/`](../src/rotation/) and [modules.md](modules.md).

## Known, deliberately accepted limitations

- **No galvanic isolation** on the PGM inputs: the panel outputs are wired as dry relay contacts, mechanically isolated already.
- **TLS uses `setInsecure()`** (no certificate pinning) — deliberate, to avoid the silent failure mode where a pinned root CA expires or rotates and the bot can no longer notify anyone about its own failure. The residual exposure is a MITM attack on the local network, considered outside the threat model for a home installation.
- **No automatic external health check** — see the first section above.
- **Secrets stored in plaintext flash** (`secrets.h`, not committed) — the threat model assumes physical access to the device already means bigger problems than a leaked bot token; the mitigation on compromise is regenerating the token via BotFather.
