# Hardware Setup

## Board

Arduino Nano ESP32 (`arduino:esp32:nano_nora`). Powered by the alarm panel itself, which has its own battery backup — so power-loss reboots of the ESP32 are expected to be rare, while connectivity-only outages (router/ISP) are the common failure mode this firmware's recovery logic is built around.

## Panel wiring

The Bentel panel's PGM outputs are used in **relay contact mode** (dry contact, NA/NC), wired directly to ESP32 GPIOs configured `INPUT_PULLUP`. No optoisolator is needed — the contact is already mechanically isolated from the panel's own circuitry.

### ⚠️ Pin assignments are placeholders

The pin numbers in [`src/events/EventTypes.h`](../src/events/EventTypes.h) were never assigned against real wiring — they're development placeholders:

```c
inline const EventTypeConfig EVENT_TYPES[] = {
    {EventType::REBOOT,        "Riavvio",             "🔄", "REBOOT",        -1, false, true, NotifyPolicy::INSTANT},
    {EventType::POWER_LOSS,    "Mancanza rete 230V",  "⚡", "POWER_LOSS",     4, false, true, NotifyPolicy::START_AND_END},
    {EventType::NETWORK_ISSUE, "Problema di rete",    "📡", "NETWORK_ISSUE", -1, false, true, NotifyPolicy::ONLY_END},
    {EventType::ALARM_GENERAL, "Allarme generale",    "🚨", "ALARM_GENERAL",  5, false, true, NotifyPolicy::START_AND_END},
    {EventType::ALARM_INTERNAL,"Allarme interno",     "🔔", "ALARM_INTERNAL", 6, false, true, NotifyPolicy::START_AND_END},
    {EventType::ALARM_GARAGE,  "Allarme garage",      "🚗", "ALARM_GARAGE",   7, false, true, NotifyPolicy::START_AND_END},
};
```

`REBOOT` and `NETWORK_ISSUE` have `pin = -1` because they're generated internally, not from a GPIO. Before relying on this for anything real, **update the pin numbers to match your actual wiring**, then re-verify with `arduino-cli compile` and, ideally, a real transition on each pin.

### Strapping pins

A few Nano ESP32 GPIOs are strapping pins — their level at boot affects boot mode selection. An `NC` contact (normally closed, opens on alarm) rests `LOW`, which can interfere with boot if wired to one of those pins. Prefer wiring `NA` contacts (rest `HIGH` via the internal pull-up) to any strapping pin, or avoid wiring an `NC`-configured zone there at all. Check the official Nano ESP32 pinout for the current strapping-pin list before finalizing wiring — it's not reproduced here to avoid it going stale.

### Per-type fields, if you need to change the table

| Field | Meaning |
|---|---|
| `emoji` | Prefixed on the type's notification/log text (see [architecture.md](architecture.md)). |
| `pin` | GPIO number, or `-1` for internally-generated events. |
| `active_low` | `true` if the zone's contact is `NC` (active = pin reads `LOW`), `false` if `NA` (active = pin reads `HIGH`). |
| `enabled` | Set `false` to fully disable a type firmware-wide (no interrupt registered, nothing logged or notified) without renumbering the enum — enum values are never reassigned, so old log rows stay meaningful even if a type is later disabled. |
| `notify_policy` | `START_AND_END`, `ONLY_END` (used by `NETWORK_ISSUE` — see [architecture.md](architecture.md)), or `INSTANT` (used by `REBOOT`). |

## Status LED

The Nano ESP32's built-in RGB LED (discrete `LED_RED`/`LED_GREEN`/`LED_BLUE` GPIOs, active-low — not an addressable/NeoPixel LED, so no extra library is needed) reflects overall system state at a glance, with no wiring required:

| Color | Meaning |
|---|---|
| 🟢 Green (solid) | WiFi connected, NTP synced, no `ALARM_*`/`POWER_LOSS` event open. |
| 🟡 Yellow (solid) | WiFi disconnected/backing off, or NTP not synced. |
| 🔴 Red (blinking) | At least one `ALARM_*` or `POWER_LOSS` event currently open, WiFi/NTP otherwise fine. |
| 🔴🟡 Red/yellow (alternating) | An `ALARM_*`/`POWER_LOSS` event is open **and** WiFi is disconnected or NTP isn't synced — distinguishable at a glance from a plain alarm, since the alarm color alone can't also carry the connectivity information. |
| 🟣 Purple (solid) | LittleFS degraded mode (>95% full). |

Priority when multiple conditions hold: alarm+network/time > alarm > degraded > network/time > ok. See [`src/diagnostics/StatusLedPolicy.h`](../src/diagnostics/StatusLedPolicy.h) for the exact rule and [architecture.md](architecture.md) for how it's driven from `loop()`.

## Toolchain

| Tool | Used for |
|---|---|
| [`arduino-cli`](https://arduino.github.io/arduino-cli/) | Compiling and flashing the sketch. |
| `arduino:esp32` core, version `2.0.18` | The board package this was built and verified against. |
| `ArduinoJson` (v7) | JSON for every persisted file — chosen because, unlike GSON (which FastBot2 uses internally for its own parsing), it's the library already used project-wide; the two coexist without sharing buffers or types. |
| `FastBot2` (GyverLibs) | Telegram client. Chosen specifically because `sendMessage()` returns a structured `fb::Result` (`isError()`, `isEmpty()`, `getErrorCode()`, and the parsed `retry_after` on a 429) instead of a bare `bool`, which is what makes the outcome classification in [architecture.md](architecture.md) possible without a custom wrapper. Pulls in `GSON`, `GyverHTTP`, `StringUtils`, `GTL` as its own dependencies. |
| A desktop `g++` (any recent version, C++17) | Running the host-side test harness — see [testing.md](testing.md). Not part of the firmware build; only needed for development. |

Install everything into a project-local sketchbook so it doesn't pollute (or depend on) your global Arduino environment:

```bash
export ARDUINO_DIRECTORIES_USER="$(pwd)/.arduino-user"
arduino-cli core install arduino:esp32
arduino-cli lib install ArduinoJson FastBot2
```
