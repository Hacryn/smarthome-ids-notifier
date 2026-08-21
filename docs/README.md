# Documentation

- [architecture.md](architecture.md) — concurrency model, component overview, data flow, robustness notes
- [data-model.md](data-model.md) — on-disk (LittleFS) and NVS file/record formats
- [modules.md](modules.md) — source tree, folder by folder, what's pure/testable vs. hardware-bound
- [commands.md](commands.md) — full Telegram command reference
- [configuration.md](configuration.md) — secrets, global config (NVS), per-user preferences
- [testing.md](testing.md) — dual verification strategy and how to run it
- [hardware-setup.md](hardware-setup.md) — wiring, pin assignment, toolchain setup

These documents describe the firmware as implemented in this repository. The original requirements and design rationale — including alternatives considered and rejected — live in [../DESIGN.md](../DESIGN.md); where this documentation and DESIGN.md disagree on a detail, the code (and this documentation, kept in sync with it) is the source of truth for current behavior.
