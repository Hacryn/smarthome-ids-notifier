# Testing

This project was built without access to physical Arduino hardware, so it uses two complementary, and equally necessary, forms of verification. Neither one alone is sufficient.

## 1. Host-side test harness (`test/`)

Every module with no Arduino/ESP32/LittleFS/network dependency — debounce logic, timestamp math, JSON (de)serialization, command parsing, retry state machines, rotation ID collection, and so on — is compiled and run directly on the development machine with a plain `g++`, independent of any Arduino toolchain. Each `test/test_*.cpp` file is a small `main()` with `assert()`-based cases and no test framework dependency.

```bash
g++ -std=c++17 -Wall -Wextra \
  -I ".arduino-user/libraries/ArduinoJson/src" \
  test/test_event_log.cpp src/events/EventLog.cpp \
  -o test/test_event_log.exe
./test/test_event_log.exe
```

The `-I` flag is only needed for harnesses that touch a module using `ArduinoJson` (it's a portable, non-Arduino-specific library, so it compiles fine here too). Check each test file's own includes to know which `src/*.cpp` files to compile alongside it — there's no single umbrella build script; each harness is compiled standalone as shown above with its dependencies listed explicitly.

As of this writing there are 17 harnesses, roughly one per pure module (see [modules.md](modules.md) for which files are "pure"), covering debounce/timing, the retry and grace-period/aggregation decision logic, JSON schemas for all four persisted file formats, command parsing, timezone presets, the rotation ID collector, and the epoch-plausibility check backing NTP-sync detection.

**What this proves**: the *logic* is correct, in isolation, for the cases exercised. It says nothing about whether the logic is wired up correctly to real hardware, or whether the hardware behaves as assumed.

## 2. Full-sketch compilation (`arduino-cli`)

```bash
export ARDUINO_DIRECTORIES_USER="$(pwd)/.arduino-user"
arduino-cli compile --fqbn arduino:esp32:nano_nora .
```

This compiles the entire sketch, including every hardware-bound module (LittleFS, WiFi, TLS, NVS, GPIO/ISR, FastBot2) against the real ESP32 Arduino core. It catches type errors, missing includes, and API misuse in code the host harness can't touch at all.

**What this proves**: the code is *structurally* correct and would run. It does not execute anything — a function that compiles cleanly but has an inverted condition, a wrong GPIO number, or a race that only shows up under real timing will pass this check without complaint.

### Project-local sketchbook

The default Arduino sketchbook location isn't always writable (e.g. inside a sandboxed environment). Point `arduino-cli` at a project-local one instead:

```bash
export ARDUINO_DIRECTORIES_USER="$(pwd)/.arduino-user"
arduino-cli core install arduino:esp32
arduino-cli lib install ArduinoJson FastBot2
```

`.arduino-user/` is `.gitignore`d.

## What neither check covers

Between them, the two checks above verify every line of code compiles and every unit of pure logic behaves as specified. What they **cannot** verify, because it depends on real timing, real electrical signals, or a real network:

- ISR timing and debounce behavior under an actual bouncing contact.
- Retroactive dating surviving an actual multi-second network block.
- LittleFS rotation surviving an actual power loss mid-write.
- WiFi reconnect/backoff against a real router.
- NTP actually syncing, and DST transitions actually working, against real time servers.
- Telegram message delivery, inline button callbacks, and command handling against the real Bot API.
- The Task Watchdog actually resetting the device on a genuine hang.

If you don't want to wire the device to the real alarm panel (or a breadboard) just to exercise the pin-event path, the cheapest option is a temporary debug command that injects a simulated transition directly into the pipeline downstream of the ISR — this tests everything from debounce onward (logging, notification, `/log`, open-event handling) without touching a GPIO, at the cost of not exercising the ISR/interrupt path itself. This isn't currently implemented in the tree; ask if you want it added.

Short version: **flash it and check `/status`, `/log`, and a live notification before trusting any of this on a real alarm.**
