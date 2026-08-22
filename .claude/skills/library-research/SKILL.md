---
name: library-research
description: Use whenever a task requires choosing a third-party library/dependency to add to this project (SmartHome IDS Notifier) — e.g. "what library should I use for X", "add a library that does Y", or when your own plan calls for a new dependency and you need to pick one. Runs an extensive search across multiple candidates, rules out deprecated/unmaintained ones, weighs how well each fits the actual requirement and how strongly the community recommends it, and prefers a permissive commercial-friendly license — but not at the cost of picking a clearly worse library. Use this before adding any new dependency, not just when the user explicitly asks for a comparison.
---

# Research a library before adding it

Picking the first library that shows up in a search, or the one Claude "remembers" as popular from training data, risks landing on something abandoned, a poor fit, or superseded by something better that came later. This skill exists to slow that down into an actual comparison — extensive means several real candidates checked against real evidence, not one name rubber-stamped.

## What "extensive" means here

Don't stop at the first plausible result. Search broadly enough to surface the realistic candidate set — search the ecosystem's package registry (Arduino Library Manager / PlatformIO Registry for this project, since it targets `arduino:esp32`), search GitHub directly, and search for community discussion (comparison threads, "X vs Y", "recommended library for Z" on forums/Reddit/Stack Overflow/Hacker News). A single library name with no comparison isn't research — you need at least a couple of real alternatives to know the top pick actually beat something.

## Step 1: Pin down the requirement

Before searching, be explicit (to yourself and, if ambiguous, to the user) about what the library actually needs to do — the specific capability, not just a category. For this project that also means platform constraints: it must work with the `arduino:esp32` core (`arduino-cli compile --fqbn arduino:esp32:nano_nora`), and ideally not conflict with or duplicate what's already a dependency (`ArduinoJson`, `FastBot2` — check [docs/modules.md](../../../docs/modules.md) or `secrets.h.example`/build files if unsure what's already in use).

## Step 2: Gather candidates

Search for multiple options via the registry, GitHub, and general web search. For each candidate worth considering, collect:
- **Repository URL**, current version/release, and language/platform compatibility.
- **Maintenance signals**: date of the last commit/release, whether the repo is archived, whether the README or an issue thread says it's deprecated or superseded, how open issues/PRs are handled (ignored for years vs. actively triaged).
- **Fit signals**: does it actually implement the required capability, or just something adjacent that would need extra glue code? Check the README/examples, not just the tagline.
- **Endorsement signals**: star count as a rough signal (not decisive alone), explicit recommendations in comparison threads or official docs, how many other libraries/projects depend on it.
- **License**: read the actual license (not just assume from the badge) — SPDX identifier if available.

## Step 3: Rule out deprecated/unmaintained candidates

Drop a candidate if the repository is archived, the README explicitly says "deprecated" / "no longer maintained" / "use X instead", or there's been no meaningful activity for a very long time relative to how actively the ecosystem around it moves — a slow-but-stable, still-updated library is not the same as an abandoned one, so check the *reason* for inactivity where you can (stable and finished vs. actually dead) rather than applying a blanket cutoff.

## Step 4: Rank what's left

Among the surviving candidates, prioritize in this order:
1. **Requirement fit** — does it solve the actual problem cleanly, without heavy workarounds or missing pieces you'd have to build yourself?
2. **How strongly it's recommended/adopted** — genuinely well-regarded and widely used beats a good-on-paper but obscure alternative, since community usage surfaces bugs and edge cases you won't find by reading the README alone.
3. **License**: prefer a permissive license (MIT/BSD/Apache-2.0/similar) that allows commercial use without complication — this is a preference, not a hard filter. If a non-permissive, restrictively-licensed, or paid library is clearly and substantially better on fit and/or maintenance/adoption than the best permissively-licensed option, prefer the better library and say so explicitly rather than defaulting to the permissive one out of caution. Don't use license as a tiebreaker between two options that are otherwise roughly equivalent in quality — in that case, the permissive one wins.

## Step 5: Present the recommendation

State the chosen library, its license, and a short justification covering maintenance status, why it fits the requirement, and how it compares to the runner-up(s) — including, if relevant, why a more-permissively-licensed alternative was passed over. Don't add the dependency to the project until the user confirms the pick (adding a new library is more than a read-only research step).
