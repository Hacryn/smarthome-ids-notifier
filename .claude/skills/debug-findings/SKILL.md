---
name: debug-findings
description: Use whenever the user asks to debug, investigate, or explain unexpected/wrong behavior in this project (SmartHome IDS Notifier) — phrases like "why is X happening", "there's a bug with Y", "debug this", "investigate why...". Once the investigation pins down a concrete, evidence-backed root cause, write it up as a facts-only .md file in .findings/ — what is happening and why, with code references, and nothing about how to fix it. Use this even if the user doesn't mention ".findings" or ask for a written report explicitly.
---

# Record debug findings

When a bug gets root-caused, the diagnosis itself is worth keeping even before anyone decides what to do about it — it can inform the fix, get handed to someone else, or get revisited later if the first fix attempt misses. Mixing the diagnosis with a proposed fix muddies both: a reader can't tell "this is what's true" from "this is what I'd try," and a wrong opinion about the fix can taint an otherwise solid diagnosis. This skill keeps them separate: **.findings/ is facts only.**

## When to write a findings file

Only after the investigation has produced an actual root cause — something you can point to in the code (or logs, or a reproduction) and explain causally, not a hypothesis or a shortlist of suspects. If you're still narrowing it down, keep investigating; don't write a findings file for "probably X or Y."

## Ground rule: no opinion on the fix

The file documents **what is causing the bug**, not what to do about it. Leave out:
- suggested fixes, workarounds, or remediation steps,
- "indicazione per la correzione" / "how to fix" sections,
- hedged opinions about which approach would be best.

If you also happen to know the fix, that's a separate conversation with the user — don't fold it into the findings file. This is a deliberate change from how some earlier findings in this repo were written (some include a fix-direction section); don't copy that pattern going forward.

## Steps

1. Investigate the reported behavior as you normally would — read the relevant code, trace the causal chain, reproduce if possible.
2. Once the root cause is confirmed (not just suspected), create a new file at `.findings/<kebab-case-slug>.md` describing the bug concisely (e.g. `.findings/reboot-real-timestamp-led-off-still-approx.md`).
3. Structure the file as:
   - `# Findings — <short title>`
   - One short paragraph of context: what was observed/reported, and under what circumstances (command run, commit, steps to reproduce) — enough for someone with no memory of this conversation to know what bug this file is about.
   - One section per distinct symptom if there's more than one, each explaining the causal chain down to the root cause, with `file:line`-style references (e.g. [`Notifier.ino:160`](../Notifier.ino:160)) to the exact code responsible.
   - If several symptoms share one underlying cause, say so explicitly (a "common cause" section) instead of repeating the explanation.
4. Do not add a status line, a fix section, or any next-step recommendation. If the user separately asks you to also propose or plan a fix, that's out of scope for this skill — handle it as its own step, not inside the findings file.
