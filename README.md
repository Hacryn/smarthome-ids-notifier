# SmartHome IDS Notifier

Standalone Arduino Nano ESP32 firmware that watches a Bentel alarm control panel and sends real-time Telegram notifications on alarms, reboots, power loss, and network outages. It keeps a persistent, queryable event history, recovers missed notifications after connectivity gaps, and manages its own users, permissions, and per-user preferences directly from Telegram — no MQTT broker, home automation hub, or local server involved.

Full requirements and design rationale live in [DESIGN.md](DESIGN.md). This README covers what you need to build and flash it; see [docs/](docs/) for architecture, data formats, and the full command reference.

## Features

- Real-time Telegram notifications for alarms, reboots, power loss (PGM contact), and network outages — interrupt-driven, so no transition is lost even while the firmware is blocked on a network call.
- Persistent event log (`log.jsonl`) and per-user notification delivery tracking, with automatic retry, grace-period-aware "recovered" notices, and message aggregation after a long outage.
- Multi-user whitelist with an admin/standard permission split, managed entirely via Telegram commands.
- Per-user timezone, date format, and per-event-type notification preferences.
- Automatic weekly log rotation with configurable retention, atomic (power-loss-safe) commits, and filesystem space monitoring.
- NTP time sync with a NVS-backed fallback clock for the period before the first sync (or after a reboot with no connectivity).
- A full Telegram command set: history queries, live status, whitelist and configuration management (see [docs/commands.md](docs/commands.md)).

## Quickstart

### Prerequisites

- An **Arduino Nano ESP32** board.
- [Arduino CLI](https://arduino.github.io/arduino-cli/) or the Arduino IDE, with the `arduino:esp32` core installed (this project was built and verified against core `2.0.18`, board FQBN `arduino:esp32:nano_nora`).
- A Telegram bot token (create one via [@BotFather](https://t.me/BotFather)) and your own Telegram numeric chat ID (e.g. via [@userinfobot](https://t.me/userinfobot)).

### 1. Configure secrets

Copy the template and fill in your WiFi and Telegram credentials:

```bash
cp secrets.h.example secrets.h
```

`secrets.h` is git-ignored — it's never committed. See [docs/configuration.md](docs/configuration.md) for what each field means and how the whitelist bootstraps itself from `ONBOARDING_CHAT_ID`.

### 2. Install libraries

If you're using `arduino-cli` with a project-local sketchbook (recommended, keeps libraries out of your global Arduino folder):

```bash
export ARDUINO_DIRECTORIES_USER="$(pwd)/.arduino-user"
arduino-cli lib install ArduinoJson FastBot2
```

Both pull in their own dependencies automatically (GSON, GyverHTTP, StringUtils, GTL for FastBot2).

### 3. Wire the alarm panel contacts

The PGM outputs on the Bentel panel are wired as dry relay contacts directly to ESP32 GPIOs (`INPUT_PULLUP`, no isolation needed). **The pin assignments in [src/events/EventTypes.h](src/events/EventTypes.h) are placeholders** — update them to match your actual wiring before relying on this for anything real. See [docs/hardware-setup.md](docs/hardware-setup.md) for the full wiring notes, including the strapping-pin caveat for NC contacts.

### 4. Compile and flash

```bash
export ARDUINO_DIRECTORIES_USER="$(pwd)/.arduino-user"
arduino-cli compile --fqbn arduino:esp32:nano_nora .
arduino-cli upload --fqbn arduino:esp32:nano_nora -p <your-serial-port> .
```

### 5. First boot

On first boot with an empty `users.json`, the `ONBOARDING_CHAT_ID` from `secrets.h` is automatically promoted to admin. Message your bot from that account to confirm it responds to `/status`.

## Testing

The project uses dual verification: a host-side g++ test harness for all hardware-independent logic (`test/`), and `arduino-cli compile` for the full sketch. See [docs/testing.md](docs/testing.md) for how to run both and what is (and isn't) covered without physical hardware.

## Documentation

- [docs/architecture.md](docs/architecture.md) — concurrency model, component overview, data flow
- [docs/data-model.md](docs/data-model.md) — on-disk/NVS file formats
- [docs/modules.md](docs/modules.md) — source tree, module by module
- [docs/commands.md](docs/commands.md) — full Telegram command reference
- [docs/configuration.md](docs/configuration.md) — secrets, global config, per-user preferences
- [docs/testing.md](docs/testing.md) — how to run the test suite
- [docs/hardware-setup.md](docs/hardware-setup.md) — wiring and pin assignment notes
