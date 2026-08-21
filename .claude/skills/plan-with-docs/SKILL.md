---
name: plan-with-docs
description: Use whenever the user asks to create or modify an implementation plan, roadmap, or design proposal for this project (SmartHome IDS Notifier) — phrases like "let's plan", "how should we implement X", "update the plan for Y", "what's the approach for Z". Grounds the plan in the project's actual documented requirements and architecture by always reading README.md, then docs/Index.md to decide which other files (including DESIGN.md) are relevant, before proposing or revising anything, so the plan doesn't contradict or duplicate what's already specified. Make sure to use this even if the user doesn't explicitly mention the docs.
---

# Plan with project docs

This project's requirements, architecture, and conventions are already written down. A plan that ignores them risks re-deriving (and getting wrong) decisions that were already made deliberately — e.g. the no-MQTT-broker constraint, the ISR/debounce concurrency model, the atomic-commit log format. Read before planning, not after.

## Steps

1. **Always read [README.md](../../../README.md)** for the project's scope, features, and quickstart flow — the outward-facing shape of the system. This is unconditional, regardless of what the plan touches.

2. **Always read [docs/Index.md](../../../docs/Index.md)**. It gives a short description of every design/reference document in the repo, including DESIGN.md — use it to judge which of those files are actually relevant to the plan at hand, instead of guessing from filenames or opening everything.

3. **Read only the files Index.md points you to as relevant**, based on the plan's area — e.g. a plan touching concurrency or component boundaries needs the architecture doc, a plan touching Telegram behavior needs the commands doc, and so on. Always include [DESIGN.md](../../../DESIGN.md) when the plan could conflict with a documented requirement or deliberate design decision (e.g. the no-MQTT-broker constraint, the ISR/debounce concurrency model) — it's the authoritative source for *why* things are built the way they are, not just what they currently do. A plan that conflicts with a documented rationale needs to either respect it or explicitly flag the conflict to the user — don't silently override it.

4. **Write or revise the plan** so it's consistent with what's documented:
   - Reuse existing terminology and component names instead of inventing new ones.
   - If the plan requires deviating from something DESIGN.md states as a deliberate decision, call that out explicitly to the user rather than quietly contradicting it.
   - If the plan fills a gap the docs don't cover, that's fine — just don't assume it's a gap without checking first.

Following this project's [CLAUDE.md](../../../CLAUDE.md) conventions (surface assumptions, state tradeoffs, keep the plan surgical) still applies — this skill only adds "check the docs first" to that.
