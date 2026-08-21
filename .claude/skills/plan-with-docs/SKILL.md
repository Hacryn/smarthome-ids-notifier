---
name: plan-with-docs
description: Use whenever the user asks to create or modify an implementation plan, roadmap, or design proposal for this project (SmartHome IDS Notifier) — phrases like "let's plan", "how should we implement X", "update the plan for Y", "what's the approach for Z". Grounds the plan in the project's actual documented requirements and architecture by reading README.md, DESIGN.md, and the relevant files in docs/ before proposing or revising anything, so the plan doesn't contradict or duplicate what's already specified. Make sure to use this even if the user doesn't explicitly mention the docs.
---

# Plan with project docs

This project's requirements, architecture, and conventions are already written down. A plan that ignores them risks re-deriving (and getting wrong) decisions that were already made deliberately — e.g. the no-MQTT-broker constraint, the ISR/debounce concurrency model, the atomic-commit log format. Read before planning, not after.

## Steps

1. **Read [README.md](../../../README.md)** for the project's scope, features, and quickstart flow — the outward-facing shape of the system.

2. **Read [DESIGN.md](../../../DESIGN.md)** for the full requirements and design rationale. This is the authoritative source for *why* things are built the way they are (e.g. section 1.1's independence constraint, section 3's concurrency model). A plan that conflicts with a documented rationale needs to either respect it or explicitly flag the conflict to the user — don't silently override it.

3. **Read the docs/ files relevant to the plan's area**, not all of them reflexively:
   - Touching architecture, concurrency, or component boundaries → [docs/architecture.md](../../../docs/architecture.md)
   - Touching persisted files, NVS, or on-disk formats → [docs/data-model.md](../../../docs/data-model.md)
   - Touching source layout or adding/moving modules → [docs/modules.md](../../../docs/modules.md)
   - Touching Telegram commands → [docs/commands.md](../../../docs/commands.md)
   - Touching secrets, global config, or per-user preferences → [docs/configuration.md](../../../docs/configuration.md)
   - Touching the test harness or CI-equivalent checks → [docs/testing.md](../../../docs/testing.md)
   - Touching pin assignments or wiring → [docs/hardware-setup.md](../../../docs/hardware-setup.md)
   - LittleFS-specific issues → [docs/littlefs-troubleshooting.md](../../../docs/littlefs-troubleshooting.md)

   If unsure which apply, skim [docs/README.md](../../../docs/README.md) for the index rather than opening every file.

4. **Write or revise the plan** so it's consistent with what's documented:
   - Reuse existing terminology and component names instead of inventing new ones.
   - If the plan requires deviating from something DESIGN.md states as a deliberate decision, call that out explicitly to the user rather than quietly contradicting it.
   - If the plan fills a gap the docs don't cover, that's fine — just don't assume it's a gap without checking first.

Following this project's [CLAUDE.md](../../../CLAUDE.md) conventions (surface assumptions, state tradeoffs, keep the plan surgical) still applies — this skill only adds "check the docs first" to that.
