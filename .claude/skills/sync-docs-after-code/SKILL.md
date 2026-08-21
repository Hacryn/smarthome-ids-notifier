---
name: sync-docs-after-code
description: Use after implementing or modifying code in this project (SmartHome IDS Notifier) — new features, changed behavior, new commands, new config fields, new files/modules, changed pin assignments, changed data formats. Checks whether README.md, DESIGN.md, or the docs/ files describe the part of the system that just changed, and updates only the parts that are now factually wrong or missing, so the documentation doesn't silently drift from the code. Do not use this for planning-stage work or purely internal refactors with no user-visible or documented-behavior change.
---

# Sync docs after implementation

This project documents itself deliberately (README for quickstart, DESIGN.md for rationale, docs/ for current-state reference — see [docs/README.md](../../../docs/README.md)). Docs that fall out of sync with the code are worse than no docs, because they're actively misleading. This skill's job is to close that gap after a code change — nothing more.

## Ground rule: strictly necessary only

Per [CLAUDE.md](../../../CLAUDE.md)'s surgical-changes principle, only touch documentation that your code change actually invalidated. Do not:
- reformat or "improve" unrelated doc sections,
- add speculative documentation for behavior that doesn't exist yet,
- rewrite prose style you happen to dislike.

If a change has no user-visible or architecturally-relevant effect (internal refactor, variable rename, test-only change), the correct action is to update nothing — say so rather than inventing an edit.

## Steps

1. **Identify what the code change actually altered** from a documentation standpoint: new/changed Telegram command, new/changed config field or secret, new/changed on-disk or NVS format, new pin assignment, new source file/module, changed architecture or concurrency behavior, new documented feature, changed test invocation.

2. **Match the change to its doc(s)** — don't scan every file reflexively:
   - New/changed Telegram command → [docs/commands.md](../../../docs/commands.md)
   - New/changed secret, NVS global config, or per-user preference → [docs/configuration.md](../../../docs/configuration.md)
   - New/changed on-disk (LittleFS) or NVS record format → [docs/data-model.md](../../../docs/data-model.md)
   - New source file, moved/renamed module, changed pure-vs-hardware-bound boundary → [docs/modules.md](../../../docs/modules.md)
   - Changed concurrency model, component responsibilities, or data flow → [docs/architecture.md](../../../docs/architecture.md)
   - Changed pin assignment or wiring → [docs/hardware-setup.md](../../../docs/hardware-setup.md), and note [README.md](../../../README.md) also calls out that pin assignments are placeholders
   - Changed test harness usage or how to run tests → [docs/testing.md](../../../docs/testing.md)
   - User-facing feature list or quickstart steps changed → [README.md](../../../README.md)

3. **Update DESIGN.md only when the requirement or rationale itself changed** — not just the implementation detail. DESIGN.md is the record of *why* a decision was made; if the code now does something DESIGN.md didn't anticipate, and that's a deliberate new decision (not a bug fix), add/amend the relevant section. If it's just an implementation detail shifting while the requirement stays the same, docs/ is the right place, not DESIGN.md.

4. **Respect the precedence rule already stated in [docs/README.md](../../../docs/README.md)**: docs/ describes current as-implemented behavior; DESIGN.md is requirements/rationale (including rejected alternatives). Keep that distinction — don't collapse implementation detail into DESIGN.md or historical rationale into docs/.

5. **Make the edit surgical**: change the specific line/table row/section that's now stale, matching the existing doc's style and structure. Don't touch adjacent unrelated content.

6. If, after checking, no documentation is actually stale, report that explicitly rather than making a cosmetic edit to justify having run the skill.
