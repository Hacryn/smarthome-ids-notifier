---
name: sync-docs-after-code
description: Use after implementing or modifying code in this project (SmartHome IDS Notifier) — new features, changed behavior, new commands, new config fields, new files/modules, changed pin assignments, changed data formats. Always checks README.md, then uses docs/Index.md to decide which other files (including DESIGN.md) describe the part of the system that just changed, and updates only the parts that are now factually wrong or missing, so the documentation doesn't silently drift from the code. Do not use this for planning-stage work or purely internal refactors with no user-visible or documented-behavior change.
---

# Sync docs after implementation

This project documents itself deliberately (README for quickstart, DESIGN.md for rationale, docs/ for current-state reference — see [docs/Index.md](../../../docs/Index.md) for what each file covers). Docs that fall out of sync with the code are worse than no docs, because they're actively misleading. This skill's job is to close that gap after a code change — nothing more.

## Ground rule: strictly necessary only

Per [CLAUDE.md](../../../CLAUDE.md)'s surgical-changes principle, only touch documentation that your code change actually invalidated. Do not:
- reformat or "improve" unrelated doc sections,
- add speculative documentation for behavior that doesn't exist yet,
- rewrite prose style you happen to dislike.

If a change has no user-visible or architecturally-relevant effect (internal refactor, variable rename, test-only change), the correct action is to update nothing — say so rather than inventing an edit.

## Steps

1. **Identify what the code change actually altered** from a documentation standpoint: new/changed Telegram command, new/changed config field or secret, new/changed on-disk or NVS format, new pin assignment, new source file/module, changed architecture or concurrency behavior, new documented feature, changed test invocation.

2. **Always check [README.md](../../../README.md)** — it's the outward-facing entry point (scope, features, quickstart), so check whether the change affects it regardless of area.

3. **Always read [docs/Index.md](../../../docs/Index.md)** and use its per-file descriptions to decide which other documents — including [DESIGN.md](../../../DESIGN.md) — actually describe the part of the system that changed. Don't scan every doc reflexively; only open the ones Index.md indicates are relevant to this change (e.g. a new Telegram command points to the commands doc, a changed on-disk format points to the data-model doc). If the change is architecturally significant enough to reflect a shift in requirements or rationale, not just an implementation detail, DESIGN.md may also need updating — see the next step for that distinction.

4. **Update DESIGN.md only when the requirement or rationale itself changed** — not just the implementation detail. DESIGN.md is the record of *why* a decision was made; if the code now does something DESIGN.md didn't anticipate, and that's a deliberate new decision (not a bug fix), add/amend the relevant section. If it's just an implementation detail shifting while the requirement stays the same, docs/ is the right place, not DESIGN.md.

5. **Respect the precedence rule already stated in [docs/Index.md](../../../docs/Index.md)**: docs/ describes current as-implemented behavior; DESIGN.md is requirements/rationale (including rejected alternatives). Keep that distinction — don't collapse implementation detail into DESIGN.md or historical rationale into docs/.

6. **Make the edit surgical**: change the specific line/table row/section that's now stale, matching the existing doc's style and structure. Don't touch adjacent unrelated content.

7. If, after checking, no documentation is actually stale, report that explicitly rather than making a cosmetic edit to justify having run the skill.
