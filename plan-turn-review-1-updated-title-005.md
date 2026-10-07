# Turn review-1 remediation plan — title-005

Turn Planner, resumed after `plan-turn-review-1-title-005.md` returned `FIX`. This is guidance for the
Orchestrator, not a gate (`docs/agent-workflow.md` §2). It covers the two blocking findings B1 and B2,
plus the advisories that are cheap to close.

**There is no code or behavior change.** The fix is toolkit `505cda5`, which adds `+0x1810` to the
generated admission table. The Reviewer reproduced it byte-for-byte and ruled out any rollback, so it
stays as it is. Rejection logic (L39) was not touched. Every item below corrects a record, a piece of
wording, or retained evidence.

## State when this plan was written (measured by the Planner, read-only)

The Orchestrator already committed and pushed a first remediation pass while this plan was being
written. That was game `7e5db3c` ("Records: review 1 remediation…"). `origin/master` equals
`7e5db3c`; per the local reflog it was pushed at 22:42:31. It changes only records: no file under
`src/`, `config/`, `scripts/`, `tests/` or `CMakeLists.txt`. Toolkit `505cda5` still equals
`origin/main`, and the toolkit tree is clean. So this plan is mostly an **audit of `7e5db3c` plus the
remaining gaps**, not a fresh to-do list.

| Finding | Done in `7e5db3c` | Still open (below) |
|---|---|---|
| B1: 1000→888 overclaim | TR §22 has a "Claim limit on the count itself" paragraph. The plan's Current work and backlog are qualified. The living plan has a remediation section. | R1: two unqualified sentences remain in the living plan (lines 33–37). Optional TR §22 wording (R5). |
| B2: missing receipts | Operating history (end of file) has receipts for toolkit `505cda5` and game `2d99b30`. It says order rests on local reflogs and was "not independently witnessed". | R2: add the server-side order witness. R3: record the outgoing-object audit scoped to this turn. R6: receipts for `7e5db3c` and the closing push. |
| Profile wording | Fixed in TR §22, the plan's Current work and F8 row, and the living plan. | — |
| Fence-audit qualification | Claimed in the living plan's remediation section. | **R4: not actually applied.** `plan-jsrf-bare-minimum.md` line 164 still says "occurs as a raw dword 0 times, so no indirect site exists". |
| Game test evidence | `just test` re-run. `build/Testing/Temporary/LastTest.log` now holds 44/44. | R7: that file is gitignored and overwritten by the next `ctest`. `just check` output is still not retained. |

## Remediation steps, by dependency and diagnostic value

**R4 — fence-audit sentence (record correction; highest value).** This is the one place where the
remediation record says something the repository does not do. Replace the clause at
`plan-jsrf-bare-minimum.md:163–164` with the Reviewer's wording. Two independent methods found exactly
five *direct* callers of `0x1912A0`, and no file-backed raw dword `0x1912A0` was found. **That absence
is not proof that computed indirect calls cannot exist.** Proof: after the edit,
`git grep -n "no indirect site" -- plan-jsrf-bare-minimum.md docs/` returns nothing.

**R1 — remaining B1 wording in the living plan (record correction).** Lines 33–37 of
`plan-turn-updated-title-005.md` still say the A/B "reproduces the old behaviour … This confirms the
planner's inference". Change "reproduces the old behaviour" to "demonstrates the overwrite/recovery
mechanism (it reaches 620, not 888/1000)". Then limit "confirms" to what was actually shown: the ring
at GET in an archived dump is not the original blocker. The count is not confirmed. Line 42 ("So the
ceiling is exactly this one method") is supported by the admit-unknown run, which listed exactly one
method. Keep it, but say it is the *first* ceiling under this run's path; the six methods found later
show the scene continues to need more. Proof: re-read lines 26–45; no sentence there attributes the
exact 1000/888 counts.

**R2 — independent witness of push order (receipt evidence, B2).** The Planner found one. GitHub's
repository activity API records server-side push events, with `before` and `after` SHAs and UTC
timestamps:

- Toolkit, `GET https://api.github.com/repos/danillogical/xboxrecomp/activity`: `dc04dc0 → 505cda5` at
  `2026-10-07T05:19:39Z`.
- Game, `GET https://api.github.com/repos/danillogical/poison-jam/activity`: `5e7a1a3 → a8691e1` at
  `05:19:54Z`, then `a8691e1 → 2d99b30` at `05:21:31Z`.

So the toolkit push precedes the first game push by 15 s on the server's clock, and both are
fast-forwards (`before` equals the prior head). This agrees with the local reflogs (22:19:39 / 22:19:55
/ 22:21:32, −0700).

- The Orchestrator should re-fetch both endpoints itself before citing them; the Planner's reading is a
  lead.
- Record the result next to the existing receipts as a **second, server-side** witness.
- Remove or soften "not independently witnessed" accordingly. Do **not** call the order "verified by
  push output", because no push transcript was retained.
- If the API no longer returns these events, leave the reflog-only wording as it is.

**R3 — scope the outgoing-object audit to this turn (receipt accuracy).** The receipt text says "the
`secret-audit` 2 hits are the audit script's own pattern literals and predate this turn". That
describes a whole-history audit, not this turn's outgoing objects.

The Planner ran the scoped audit:

```text
git rev-list --objects 5e7a1a3..2d99b30   # piped to a list file
scripts/secret-audit.py <that list>
```

Result: **14 objects, TOTAL HITS 0**. All six paths are records under `docs/` or `plan-*`; there is no
`game/` path. Repeat it for the remediation push (`2d99b30..7e5db3c`) and the closing push, and record
each scoped result as "N objects, 0 hits". If a whole-history note is kept, keep it as context only.

**R5 — optional tightening in TR §22 (record correction, low cost).** `docs/jsrf-technical-record.md`
around line 2693 says "no title-screen frame exists". The Reviewer certified only the inspected
artifacts, so use: "no title-screen frame appears in the inspected presenter/flip dumps or
`[FBPRESENT]` hashes". The existing "Claim limit" paragraph is adequate for B1; do not expand it.

**R7 — make the test and check evidence durable (advisory; no code change).** The 44/44 log
(22:34–22:37, 44 `Test Passed`, 0 failed; Planner-read SHA-256 `5CA5653B…3925`) lives under
`build/`, which is gitignored and rewritten by the next `ctest`.

1. Copy that log to a local-only evidence location such as `logs/workers/title005/`. Also capture
   `just check` into the same place (the smallest command is `just check *> <file>`; it ends
   `check: all checkers passed`).
2. In the record, cite the counts, the timestamps, the SHA-256 and the tree it ran on. The build ran on
   code identical to `2d99b30`/`7e5db3c`, because neither commit touches code.

These artifacts stay outside Git. The record carries the numbers. Do not re-run anything mutating
beyond these two commands.

**R6 — closure, last (depends on R1–R5 and R7).**

1. Write the `7e5db3c` receipt from its actual push:
   - toolkit: up to date at `505cda5`, checked first;
   - game: fast-forward `2d99b30..7e5db3c`, with the R3 scoped audit.
2. Make the closing commit. Per §2 it moves anything durable out and **deletes all four
   `plan-turn-*-title-005.md` files, this one included**.
3. Before pushing: both trees clean, fast-forward only, destination `origin`. Push the toolkit first
   (expect "Everything up-to-date"), then the game.
4. The closing push cannot carry its own receipt. Record it either in the final response or in a
   receipt-only follow-up commit; the project has precedent for both. **Never write a receipt for a push
   that has not happened.**

## What must not be claimed

- That the 1000→888 difference, or either exact count, is explained or caused by the live mirror. It
  is a hypothesis consistent with a demonstrated mechanism, and TR §19's attribution stays
  **UNRESOLVED**.
- That the A/B reproduces 888 or 1000. It reached 620.
- That any run was free of exploratory settings. They are standard exploratory title runs that used
  neither the live-mirror switch nor the admit-unknown switch, and they are not fidelity evidence.
- M15, a title frame, or that a new frame hash means progress toward the title.
- That stop 28 (`0x81860`) works. It is **NOT EXERCISED**.
- That no indirect caller of `0x1912A0` exists.
- That toolkit-first order was observed from push output. Only the reflogs record it, plus the
  server-side activity events if R2 is confirmed.
- That a regression was ruled out beyond what was measured. The supported statement is narrower: the
  first rejection is identical in all five archived logs, so the `0x1810` rejection predates `dc04dc0`.

## Honest claim limit for this turn

> **Explained and cleared the *first* present ceiling; did not reach the title screen.**
>
> - **Explained, with three-way agreement:** the submission walk rejected at GET `0x8EF0` on
>   `unsupported_method 0x1810` (`NV097_DRAW_ARRAYS`). The method was missing from the generated
>   admission table, although the executor already implemented it. The same first rejection is present
>   in all five archived logs.
> - **Fixed in toolkit `505cda5`:** exactly `+0x1810`, regenerated reproducibly, with no rejection
>   relaxed.
> - **Cleared:** two exploratory title runs, with neither the live-mirror nor the admit-unknown switch,
>   reached presents 2410 and 1680 with zero rejections.
> - **Old-mirror mechanism demonstrated** by the A/B run: overwrite plus recovery past `0x8EF0`. The
>   exact historical 1000/888 counts remain unexplained.
> - **M15 not reached.** The next blockers are measured: six state methods, then `budget_exhausted`.

This is a bounded checkpoint toward the user's objective, not its completion. The next turn starts at
F8b.
