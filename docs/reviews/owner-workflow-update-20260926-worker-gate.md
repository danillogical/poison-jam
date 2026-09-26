# Owner workflow update — implementation-worker progress gate (2026-09-26)

**Authority:** direct owner instruction, a `§3.4`-class workflow replacement.
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`.
**Status:** applied. No session restart; `A4s` not reopened; the authorized `A4b1-r4` work continues from
its current durable state.

## Required record

```text
WORKFLOW_RELOADED: YES
WORKFLOW_SHA256:   F4F5D029D45A96CE5E4CA61ED106C0EFDF7CFD3FC99EB5F331E4D22D12F04A0C
PLANNER_ROUTE:     claude/claude-opus-5-5 @ medium
ADVISOR_CHILD:     5c555969-dea9-4b47-be05-62aa0835cde2
ACTIVE_WORKER_PROGRESS_GATE_APPLIED: YES
```

## The file changed, and was re-read from disk

| | SHA-256 |
|---|---|
| previous reload (Planner → Opus) | `973CDDEF0C5206349E66DE15A6C0E830F85ACAD488231D3867F25B583E911748` |
| **on disk now** | **`F4F5D029D45A96CE5E4CA61ED106C0EFDF7CFD3FC99EB5F331E4D22D12F04A0C`** |
| match | **NO — the file was replaced** |

48193 bytes, 873 lines, modified `2026-09-26 00:53:57`, uncommitted in the game repo (the owner's edit;
**not** committed or normalised).

**Diff: 117 insertions / 21 deletions.** The Session read the added lines rather than assuming the
change was limited to what the owner described.

## Staffing is unchanged — verified, not assumed

The `§1` roster was read from the new file. All six DSH rows are **identical** to the previous reload:

| Role | DSH |
|---|---|
| Session | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| Worker subagents | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Planner** | **Claude Opus 5.5 @ `medium`** (`LIVE_RESOLVE`) |
| **Persistent advisor** | **Claude Opus 5.5 @ `high`** (`LIVE_RESOLVE`) |
| Acceptance stage 1 | `workbuddy-ai/hy4-preview-f` @ `high` |
| Acceptance stage 2 | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |

So **no route re-resolution was needed**: the Planner route resolved earlier this session as
**`claude/claude-opus-5-5`**, one canonical match, `medium` advertised, and that resolution stands. The
**existing persistent Advisor child `5c555969-…` is preserved** — not replaced, not reprobed, per the
owner's instruction and `§4.4`.

## What the new revision adds

**The implementation-worker anti-loop progress gate (`§2.2`, lines 147-180)** — the only substantive
addition. Quoted in full so the Session applies the same test the document states:

> **Implementation-worker progress gate.** This gate applies to bounded implementation workers, not
> read-only/log-analysis workers.
>
> - By **15 tool calls**, the worker must have produced at least one concrete execution artifact: an
>   edit, compile/build attempt, test run, generated fixture, or a bounded blocker report.
> - If it has not, it stops broad investigation and either performs the smallest safe
>   implementation/compile step immediately or returns `BLOCKED`.
> - After the first implementation attempt, further source reads must be tied to a **specific observed
>   compiler, linker, test, or runtime failure** and state what that read is expected to resolve.
> - A worker may not restart architecture/source exploration from first principles after an
>   implementation failure unless new contradictory evidence invalidates the prior design premise.
> - If **two consecutive 10-call stretches** produce no new artifact, measurement, or narrowed blocker,
>   the worker stops and returns control to the Session.
> - Re-reading the same files or reconsidering already-settled design alternatives without new
>   contradictory evidence counts as **no progress**.
> - On genuine ambiguity, required scope/policy change, or inability to proceed inside the assigned
>   write scope, the worker returns:
>
> ```text
> STATUS: BLOCKED
> EXACT REQUIREMENT:
> EXACT BLOCKER:
> EVIDENCE:
> WHY CURRENT WRITE SCOPE CANNOT SATISFY IT:
> SMALLEST DECISION/CHANGE NEEDED:
> ```
>
> A no-progress or blocker return is an escalation signal. The Session does **not** automatically spawn
> an identical replacement worker on the same brief; it first decides whether the blocker belongs to
> the Planner, Advisor, or a revised bounded worker brief.

Also carried in the diff (already in force from the previous reload): the bounded-initiative rules for
the Planner, and the `§5.1.5(3)` rule that a packet materially authored by an Opus 5.5 Medium Planner
gets its binding adequacy review from a **fresh** Opus 5.5 Medium Planner.

## Applied immediately to the active worker

**`ACTIVE_WORKER_PROGRESS_GATE_APPLIED: YES`.**

The active worker, child **`1fe1b4f2-6e2d-45dc-8741-1359389432c0`** (rewriting the `AC-FIX` fixture for
the Advisor's `FIFO_READ` ruling (C)), **had already exceeded the new first-artifact budget**, so per the
owner's instruction it was **not** given another fresh 15-call window. The owner's bounded correction was
sent verbatim in substance:

> **STOP RESEARCH CHURN AND EXECUTE OR REPORT A BLOCKER.**
>
> You have already exceeded the workflow's implementation-worker progress budget.
>
> Within your next **3 tool calls**, do one of:
>
> **A.** produce the smallest safe implementation artifact and compile/test it;
> **OR**
> **B.** return the workflow's bounded `BLOCKED` report.
>
> Do not perform additional broad source surveys. Further reads are allowed only if tied to a specific
> observed compile/link/test/runtime failure. Do not restart the design from first principles.

The message also carried the `BLOCKED` report form and a factual orientation list of the production
changes already made (the `is_gp`-gated read-arm hook, `DSPDMAState.is_gp`, the latch mirroring, the
per-FIFO classification), so the worker need not re-derive them — which is itself in the spirit of the
gate's "no re-reading" clause.

**The correction was recorded as issued under the new workflow.** If the worker returns `BLOCKED` or
no-progress, the Session **will not** spawn an identical replacement: the blocker goes through the
escalation ladder (Planner / Advisor / revised bounded brief) first.

## Scope, baseline and rulings preserved

Per the owner — *"Preserve the current packet, current accepted baseline, existing Advisor child, and
all durable rulings. Do not change scope merely because the workflow changed"* — nothing else changed:

| Preserved | State |
|---|---|
| Active packet | **`A4b1-r4`**, `6DD62A57…35C38`, 445 lines, `ADEQUATE`, promoted |
| Accepted baseline | toolkit `M` = `3f8bf67c…`; `A4s` accepted, pushed, **not reopened** |
| Advisor child | `5c555969-dea9-4b47-be05-62aa0835cde2` (Opus 5.5 `high`), **reused** |
| Durable rulings | the `[GPIN]` accounting ruling, the `FIFO_READ` ruling (C) + addendum, the `A4s` AC'97 hunk rulings, Q1/Q2 — all binding, none reopened |
| Write scope | unchanged: toolkit `src/apu/**`, `NOTICE`, `LICENSES/**`, `tests/` + `CMakeLists.txt` for the fixture; no game `src/` edit |
| Toolkit HEAD | `4d841d0` (7 commits: vendor → port → licences → latch mirror → FIFO hook → NOTICE fix) |
