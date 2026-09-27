# Owner decision — new staffing authority, and `A4b2` planning resumed

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Status:** `A4b2` planning is **RESUMED**. The `docs/reviews/blocker-20260926-claude-route-down.md`
suspension is **lifted by owner decision**.

This record exists because the plan's `CURRENT PACKET` block and the blocker record both carried an
owner decision that the current owner instruction supersedes. §3.4 reserves staffing to the owner, so
only an owner decision could lift it; this is that decision, recorded verbatim.

**Startup receipt for the session that applied it:**
`docs/reviews/startup-20260927-session-9f8c9988.md`.

## The owner instruction (verbatim, abridged to the operative clauses)

Delivered as a direct new-session handoff. Per §4.1 an owner message is an owner instruction, outranks
every role's ruling, and is followed and recorded.

> **NEW SESSION HANDOFF — FOLLOW CURRENT docs/agent-workflow.md**
> …The repository has a new staffing/workflow authority, so do not use staffing instructions, child
> IDs, or route assumptions from the previous session. …
>
> **Current staffing.** For DSH, the current authoritative roles are:
> - Session: `workbuddy-ai/deepseek-v4.1-flash` @ `max`
> - Workers: `workbuddy-ai/deepseek-v4.1-flash` @ `max`
> - Planner: **GPT-6 Sol** @ `high`, provider `codex`, resolve route live
> - Persistent Advisor: **Muse Spark 1.3** @ `max` via the `muse-worker` skill
> - Acceptance reviewer: `workbuddy-ai/hy4-preview-f` @ `high`
> - Acceptance second stage: `workbuddy-ai/deepseek-v4.1-flash` @ `max`
> - Final unresolved acceptance adjudicator: fresh **GPT-6 Sol** @ `high`
>
> Do not substitute another model if any required route is unavailable.
>
> **The staffing change does not invalidate previously accepted work or recorded technical rulings by
> itself.**
>
> **A4b2 resume point.** … The previous Planner's A4b2-r4 sketch had already received a recorded
> **SHAPE: PROCEED**. That ruling remains operative unless current evidence establishes
> PREMISE_CHANGED. Do not repeat the shape preflight solely because the Planner/Advisor models changed.
>
> Do not reuse old Planner child `cee46374-344e-4698-a67d-c066ed569deb`. It belongs to the old staffing
> configuration and previous top-level session.
>
> After startup, if current repository/plan state still confirms the same A4b2 resume point and no
> load-bearing premise has changed:
> 1. Spawn a fresh GPT-6 Sol High Planner.
> 2. Give it the frozen A4b2-r4 planning brief, existing cleared sketch, recorded SHAPE: PROCEED
>    ruling, sketch-verification record, and relevant packet/review files.
> 3. Tell it to continue from the cleared sketch and write the packet body, not restart planning from
>    first principles.
> 4. Preserve the previously settled A4b2 rulings unless direct current evidence invalidates a premise.
> 5. Once the change packet revision is complete, submit that exact revision to a different fresh
>    GPT-6 Sol High Planner for the §5.3 adequacy review. The authoring Planner may not serve as its
>    own change-packet adequacy reviewer.
> 6. On ADEQUATE, freeze and promote that exact revision immediately according to the workflow.
>
> …Do not re-litigate already settled A4b2 points without new contradictory evidence…

## What this decides

| Question | Decision |
|---|---|
| Is the old `claude/claude-opus-5-5` Planner/Advisor route still the §1 roster? | **No.** Superseded. |
| Who is the DSH Planner? | **GPT-6 Sol** @ `high`, `provider: codex` |
| Who is the DSH Persistent Advisor? | **Muse Spark 1.3** @ `max`, `muse-worker` skill |
| Is the `A4b2` planning suspension lifted? | **Yes.** |
| Is a substitute authorized? | Yes — the substitutes are now **named and required**; the old "no substitute authorized for either judgment role" clause no longer governs. |
| Does the staffing change invalidate `A4b1`'s acceptance or any recorded ruling? | **No.** |
| Is the old Planner child reusable? | **No.** `cee46374-344e-4698-a67d-c066ed569deb` is retired with the old configuration. |
| Must the shape preflight be repeated? | **No.** The recorded `SHAPE: PROCEED` stands unless `PREMISE_CHANGED`. |

`docs/agent-workflow.md` §1 (DSH column) already names exactly this staffing, and the working tree
carries that edit (110 insertions, 44 deletions against `HEAD`). The plan block was the remaining
document that still asserted the superseded decision.

## Advisor ruling — the Session's scoped plan edit (recorded verbatim)

**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, reported
`reasoningEffort: max` (turn `01a0e25b-4f52-7000-aca4-f69bcbbe646a`). Consulted under §2.3; the
question was whether replacing the stale suspension paragraph was procedurally correct and whether
anything else stood between it and spawning the Planner.

> ```text
> RULING: The Session's scoped action is correct. Record the owner staffing/resumption
> decision verbatim in a dated review record, then replace the stale suspension paragraph
> in the plan's CURRENT PACKET block with a short pointer to that record and to the
> resumption. No other procedural step stands between that and spawning the fresh GPT-6
> Sol High Planner to write the A4b2-r4 body from the cleared sketch — provided the §0
> startup checks pass, including exactly-one-route LIVE_RESOLVE for the Planner row.
> The stale "no substitute authorized" text is not a blocker once the superseding owner
> decision is recorded; it is a contradiction with a live owner instruction if left.
>
> Three binding clarifications:
> 1. The plan edit is confined to the suspension/blocker paragraph. It must not touch
>    any packet contract text, any recorded ruling, or the settled A4b2 substance list.
> 2. The prior ruling's "fresh Opus 5.5 Medium Planner" adequacy-reviewer line
>    (a4b2-r4-planning-rulings.md:119-120) is superseded AS TO STAFFING ONLY by the
>    owner decision; its substance — fresh child, not the authoring child, full §5.3
>    review, not a delta — stands, and matches the owner's step 5.
> 3. The Session's "no load-bearing premise changed" re-verification must be cited by
>    artifact in the new record (what was checked, what hash/tree it showed). It does
>    not replace the §5.6 PREMISE_FRESHNESS judgment, which the adequacy reviewer
>    makes independently.
>
> BASIS:
> - Observed: plan-jsrf-bare-minimum.md:18-54 carries the old suspension text
>   ("no substitute authorized", "will not route Planner or Advisor work elsewhere").
> - Observed: docs/agent-workflow.md:60-68 (§1 DSH column) already names exactly the
>   staffing the Session quotes — Sol High/codex Planner, Muse Spark 1.3 Advisor,
>   deepseek Session/workers/second-stage, Hy4 first-stage, fresh Sol adjudicator.
> - Observed: SHAPE: PROCEED is recorded (a4b2-r4-planning-rulings.md:28), with the
>   full-fresh-§5.3 obligation (ibid.:33,118-120).
> - Inferred: the owner handoff text is as the Session quotes it (reported verbatim,
>   not observed by me; corroborated by the matching §1 roster already in the tree
>   and by my own session priming, which stated identical staffing).
> - Uncertain: the working-tree dirty state of the §1 edit (shell denied); immaterial
>   to this ruling since the roster content itself is confirmed.
>
> REVERSED BY: the quoted owner instruction shown to be misquoted or absent (then the
> old "no substitute" owner decision still governs and planning stays BLOCKED); a
> load-bearing premise of the cleared sketch shown changed by direct current evidence
> (then re-preflight, not silent resumption).
>
> RECORD IN: the new Session review record (verbatim, with Advisor handle/model/
> reasoningEffort per §3.3/§4.4); plan CURRENT PACKET block carries only the pointer.
> The §1 roster content needs no change — it already matches.
> ```

All three clarifications are applied. Clarification 2 is noted at the end of this record so the
staffing-only supersession is not mistaken for a reversal of the substantive obligation.

## Session re-verification — no load-bearing premise changed

Clarification 3 requires this cited by artifact. Measured directly this session at toolkit
`3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d` (clean), game `a000662aef3bb4d7088f3fa367d19e7ce7c157df`.
Every item is a **Session observation**, not an inference.

| Premise | Measurement | Result |
|---|---|---|
| `0xFFFFB3` is a placeholder, not a model | `dsp.c:56-57` — `case 0xFFFFB3: v = 0; // core->num_inst; // ??` | **CONFIRMED** — upstream's own `// ??` |
| Modelled PERIPH set is five offsets | `dsp.c:59, 65, 68, 71, 74` = `0xFFFFC5`, `0xFFFFD4`–`0xFFFFD7`; plus `0xFFFFB3` at `:56` which the ruling reclassifies STUB | **CONFIRMED** — five modelled |
| EP MMIO unrouted | `apu_core.c:648-650`; only `0x50000` mentions in `src/` are that comment and `:674` | **CONFIRMED** |
| `ep_ops` has no caller | four references only: `gp_ep.c:568` definition, `gp_ep.h:48` extern, comments `apu_core.c:30`, `apu_state.h:374` | **CONFIRMED** |
| `ep.regs[` written only in `ep_write` | `gp_ep.c:526` (read, `ep_read`), `:558`/`:559`/`:562` (writes, `ep_write`), `:612`/`:613`/`:650`/`:651` (reads, frame fn) | **CONFIRMED** |
| APU state is zero-initialised | `apu_core.c:543` — `calloc(1, sizeof(MCPXAPUState))` | **CONFIRMED** |
| Retired 256-entry table and `GPIN_OVERFLOW` gone | `git grep` for `GPIN_OVERFLOW\|gpin_table\|gpin\[256\]\|GPIN_MAX` → **one hit**, `apu_watch.h:13`, a comment saying the mechanism does not appear | **CONFIRMED** |
| `N_SITES = 16` ≥ 6 enumerated sites | `apu_watch.h:110`; `:85-108` lists the six sites at the sketch's locations | **CONFIRMED** |
| PERIPH universe is finite | `dsp_cpu_regs.h:120-121` — `DSP_PERIPH_BASE 0xFFFF80`, `DSP_PERIPH_SIZE 128` | **CONFIRMED** |
| `A4b1` is accepted and its toolkit commit is current | `git log` toolkit HEAD = `3a3c7c1`; clean; `origin/main` = `3a3c7c1`; `upstream/main` = `766ecef` | **CONFIRMED** |
| The packet still holds the cleared sketch over the intact r3 body | `a4b2-gp-clears-pending-word.md` SHA-256 `BDABDE4AD58DB1037AC05E3CF5D74D68726763E99B513356B4B5BF61810256CD`, **334 lines** — matches the blocker record's preserved state byte-for-byte | **CONFIRMED** |

**Result: no `PREMISE_CHANGED`.** The `SHAPE: PROCEED` ruling at
`docs/reviews/a4b2-r4-planning-rulings.md:28` remains operative, and no shape preflight is repeated.

This table is the **Session's** measurement and does not replace the §5.6 `PREMISE_FRESHNESS` judgment,
which the adequacy reviewer makes independently (Advisor clarification 3).

## Settled `A4b2` substance — carried forward unchanged

Recorded here only as a pointer so this record cannot be read as reopening anything. These remain
binding and are **not** re-litigated:

- grounds are **§5.4(2) + §5.4 After-INADEQUATE + §5.4(3)**, not "only a re-bind";
- the demoted `803CC000` GPSADDR conjunct;
- the `at_clear` inference and its two can-fail checks;
- `0xFFFFB3` = STUB, with **FIVE** modelled offsets (`0x45`, `0x54`–`0x57`);
- P3 bound to the **build-identity commit**, not "HEAD".

## Supersession note (Advisor clarification 2)

`docs/reviews/a4b2-r4-planning-rulings.md:119-120` requires the adequacy review go to "a **fresh Opus
5.5 Medium Planner**". That clause is **superseded as to staffing only** by the owner decision above.
Its substance is unchanged and still binding: the review goes to a **fresh child that did not author
the revision**, it is the **full §5.3 review**, and it is **not a delta review**. Under the current
roster that child is a **fresh GPT-6 Sol High Planner**.
