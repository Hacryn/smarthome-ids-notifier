---
name: save-plan
description: Use whenever an implementation plan for this project (SmartHome IDS Notifier) has reached a finished state — no open questions, no points still needing the user's input — and should be written to a .md file in .plans/. Also use whenever the user requests changes to a plan that's already saved there, so the existing file gets updated in place by revising the specific sections affected, never by appending a changelog or a new section that duplicates old content. Do not save a plan that still has open points; resolve or explicitly ask about those first.
---

# Save a finished plan to .plans/

A plan is worth keeping once it's actually decided — half-finished plans with open questions belong in the conversation, not in a file that looks authoritative. Once a plan is written down, `.plans/` becomes the durable record of what was decided and why; later revisions should leave that record accurate and current, not turn it into a log of every back-and-forth.

## When to write a new plan file

Only once the plan has no remaining open points and nothing left that needs the user's input or decision. If something is still unresolved, don't save yet — resolve it in conversation (or ask, per this project's [CLAUDE.md](../../../CLAUDE.md) guidance to surface confusion rather than guess) first. A plan produced via `ExitPlanMode` that the user has approved is the typical trigger, but any plan reaching a settled state in conversation counts.

## Language: always English

Write every new or updated plan file in English, regardless of the language the conversation happens in. Some existing files under `.plans/` are in Italian (written before this rule) — don't translate them retroactively as a side effect of an unrelated edit, but any file this skill writes or touches going forward is English only, including when applying a requested change to an old Italian plan (at that point, rewrite it in English rather than continuing it in Italian).

## File format

Follow the structure already used by the existing plans in `.plans/` (see e.g. [`.plans/reboot-real-timestamp.md`](../../../.plans/reboot-real-timestamp.md) or [`.plans/increase-reboot-ntp-wait-timeout.md`](../../../.plans/increase-reboot-ntp-wait-timeout.md) for concrete examples of the shape — translate the section labels to English as shown below rather than copying their Italian headings verbatim):

- Filename: `.plans/<kebab-case-slug>.md`, named after what the plan does.
- `# Plan — <title>` as the first line.
- `Status: **not yet implemented**` right under the title — this line tracks whether the plan has been implemented yet; update it later (e.g. to `**implemented** (commit \`<hash>\`)`) once the work lands, but that update is outside this skill's scope unless the user asks for it here.
- `References:` line listing the key files/docs/prior plans/findings the plan touches or depends on, as markdown links.
- `## Why` — the motivation/context, written so someone with no memory of this conversation understands why the plan exists.
- Numbered `## 1. ...`, `## 2. ...` sections, one per concrete piece of work, each naming the file(s) touched.
- A closing `## Verification` section describing how the plan's success will be checked (tests, manual verification steps, compile checks — whatever applies).

Match this structure and the existing files' level of detail; don't invent a different template.

## Updating an already-saved plan

When the user requests a change to a plan that already has a file in `.plans/`, edit that file in place:

- **Replace or revise the specific section(s)** the change affects — if section 2 changes, rewrite section 2. Don't leave the old version sitting above or below a new one.
- **Renumber sections** if the change adds, removes, or reorders a step, so the numbering stays sequential and doesn't skip or duplicate.
- **Do not append** a "changelog", "update log", "revision N" section, or a duplicate of a section that now only differs slightly from the original — the file should always read as the current, single, coherent plan, not a history of how it evolved. If the *history* of a decision genuinely matters (e.g. why an approach was rejected), fold that reasoning into the relevant section's prose instead of tacking on a separate log.
- If the change invalidates the `Riferimenti:` line or the `## Perché` framing, update those too — don't let them go stale just because the change was scoped to one section.

The test for any edit: could someone read the file top to bottom and understand the current plan correctly, with no leftover contradictions or superseded text sitting alongside the current version? If not, the edit isn't done.
