---
name: implement-plan-on-branch
description: Use whenever the user asks to implement a plan for this project (SmartHome IDS Notifier) — phrases like "implement the plan", "go ahead and build this", "let's implement <plan-name>". Before writing any code, branch off main, then commit and push the implementation work to that branch as it progresses, and once the implementation is finished, tell the user the exact branch name so they can find and review it. Do not implement plan work directly on main.
---

# Implement a plan on its own branch

Implementing directly on `main` makes a half-finished plan indistinguishable from the project's stable state, and makes it awkward to review or discard the work in isolation. This skill's job is the git bookkeeping around implementation — branch, commit, push, report — not the implementation itself, which proceeds however the plan (and this project's [CLAUDE.md](../../../CLAUDE.md) conventions) call for.

## 1. Before branching: make sure main is clean and current

Run `git status` first — if there's uncommitted work unrelated to this plan, that's the user's in-progress work; don't discard it. Stash it (`git stash -u`) or ask, per this project's usual safety rules, rather than branching over it silently.

Fetch and make sure you're branching from an up-to-date `main`:
```bash
git checkout main
git pull origin main
```

## 2. Create the branch

Name the branch after the plan. If the plan is already saved as `.plans/<slug>.md` (see [save-plan](../save-plan/SKILL.md)), reuse that slug directly as the branch name — it keeps the branch traceable back to the plan file with no translation needed. If the plan only exists in conversation, derive a short kebab-case slug from what it does.

```bash
git checkout -b <slug>
```

If a branch with that name already exists locally or on `origin`, that's a signal worth surfacing to the user rather than silently overwriting or picking a random suffix — ask whether to reuse it, delete it, or pick a different name.

## 3. Implement, committing as you go

Follow the plan's sections in order. Commit at meaningful checkpoints — e.g. one commit per plan section/step, or per coherent chunk of work — rather than a single giant commit at the very end; this keeps the branch's history reviewable, consistent with this project's existing commit style (see `git log` for examples). Standard commit hygiene still applies: create new commits rather than amending, don't skip hooks, review staged content before committing.

After each commit (or at least before finishing), push the branch:
```bash
git push -u origin <slug>
```
Using `-u` on the first push sets the upstream so subsequent pushes are a plain `git push`. Pushing here is expected and pre-authorized by this skill's normal operation — it's a scoped, non-`main`, non-force push to the branch this skill just created, not a case that needs a separate confirmation each time.

## 4. When the implementation is done

Once the plan is fully implemented (and, per the plan's own `## Verification` section, checked), make sure the last commit is pushed, then tell the user the exact branch name — e.g. "Implemented on branch `<slug>`, pushed to origin." Don't merge to `main` or open a PR as part of this skill; that's a separate, explicit step the user asks for when they're ready.
