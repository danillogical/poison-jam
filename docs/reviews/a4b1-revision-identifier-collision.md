# A4b1: the revision-identifier collision (`A4b1-r2` is already used) — a Session error

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** **open — referred to the persistent Opus 5.5 Advisor** as an addendum to the `A4b1-r2` shape
preflight. **The error is the Session's.** Recorded immediately so it cannot propagate further.

## The finding

The identifier **`A4b1-r2` is already taken**, and the revision that used it has already been
adequacy-reviewed. Measured from git:

| Revision | Commit | Subject | Reviewed? |
|---|---|---|---|
| `A4b1-r1` | `8735165` | *"A4b split: draft A4b1 (port/licence/fixtures/default) and A4b2 for review"* | yes (`a4b1-a4b2-adequacy-review.md`) |
| **`A4b1-r2`** | **`127d203`** | *"A4b1-r2/A4b2-r2: write-once ledger replaces capped-log attribution"* | **yes** — `a4b1-a4b2-r2-adequacy-review.md`, two independent reviewers, `INADEQUATE` on hash `839E9BEC…D2BB7` |
| `A4b1-r3` | `fea49f1` | *"A4b1-r3/A4b2-r3: define latch fields, make [GPIN] lossless, repin P2 seam"* | **yes** — `a4b1-a4b2-r3-adequacy-review.md`, `INADEQUATE` |
| `A4b1-r4` | **never written** | — | — |

Each committed packet's **own header** confirms the mapping: `8735165` → `Contract revision: A4b1-r1`,
`127d203` → `A4b1-r2`, `fea49f1` → `A4b1-r3`.

**So `A4b1-r2` is not merely used — it is used *and consumed* by a completed review.** Reusing it would
make every record that cites "`A4b1-r2`" ambiguous between two different documents.

## The competing prescriptions

| Source | Says the next revision is |
|---|---|
| **git lineage** (r1, r2, r3 exist) | **`r4`** |
| `docs/reviews/a4b-gpin-accounting-ruling.md` `RECORD IN` | **`A4b1-r4`** |
| `plan-jsrf-bare-minimum.md` L548 heading | *"the **r4** revision is unblocked"* |
| **frozen `A4s` packet, `R-SAME` row** | *"Planner revises **`A4b1` → `A4b1-r2`**"* |
| `plan-jsrf-bare-minimum.md` L78 / L151 / L186 | `A4b1-r2` |
| **the owner's current directive** | *"1. `A4b1` → `A4b1-r2`"* |

## The error is the Session's

The Session wrote **`A4b1-r2`** into the `A4s-r6` planning brief, and from there it propagated into:

- the **frozen `A4s` packet's `R-SAME` row** (SHA `75207C41…`, accepted and closed — cannot be edited);
- the **plan** (three places plus the in-flight block);
- the **filenames of the Session's own `A4b1` records** — `a4b1-r2-premise-recheck.md`,
  `a4b1-r2-planning-brief.md`, `a4b1-r2-inverse-precision.md`, `a4b1-r2-gpin-lineage-conflict.md`.

**The Session did not check the existing lineage before naming the revision.** That is precisely the
*"do not copy assumptions from pre-`A4s` `A4b` planning without rechecking the premises"* failure the
owner's directive warns against, committed by the Session rather than by a Planner, and it should have
been caught during reconnaissance — the reconnaissance covered *source* premises thoroughly and the
*record* lineage not at all.

**It was the Planner that surfaced it**, by checking the packet's git history before writing the header
(*"last committed revision is r3 (commit fea49f1); no r4 exists"*).

## Why it also bears on the `[GPIN]` question

If the correct identifier is **`A4b1-r4`**, then it **agrees exactly** with the `[GPIN]` ruling's
`RECORD IN`, which already targets `A4b1-r4` for *"DS6 input accounting per (a); AC-FIX (ix′)/(x)/(xi);
fold in the r3 B2 fixes"*. The identifier and the content would then line up, and the earlier lineage
conflict largely dissolves: this revision simply **is** the `r4` the ruling asked for.

If instead the `[GPIN]` redesign is ruled **out** of this revision's scope, `r4` is *still* the correct
identifier (next sequential) and the redesign would be targeted at `r5`.

Either way the **name is `r4`**; the `[GPIN]` ruling decides only *what `r4` contains*.

## Questions put to the Advisor

1. Confirm the correct identifier is **`A4b1-r4`** (the Session's reading), on the ground that an
   already-reviewed identifier cannot be reused.
2. If the `[GPIN]` redesign is **in** scope → `r4` carries it, matching the ruling's `RECORD IN`. If
   **out** of scope → `r4` is still the name, and the redesign is targeted at `r5`. Confirm which.
3. Confirm that the frozen `A4s` packet's `R-SAME` row citing `A4b1-r2` is a **stale record** whose
   correction belongs in the **plan and the Session's review records**, not in the frozen packet — and
   that this is a **record correction**, requiring nothing of `A4s`.
4. Because the owner's directive literally says *"`A4b1` → `A4b1-r2`"*, state explicitly whether
   renaming to `r4` is **(a)** a mechanical record correction the Session applies and reports, or
   **(b)** a contradiction of an explicit owner instruction — which `docs/agent-workflow.md` §3.4
   reserves to the owner. **The Session's reading is (a)**: the substantive instruction is *"revise
   `A4b1` to re-bind to the new baseline"*, the name was almost certainly copied from the Session's own
   plan, and naming is a record-keeping matter. But §3.4 lists *"an Advisor ruling that would contradict
   an explicit owner instruction"* as owner-reserved, so the Session wants an explicit call rather than
   its own assumption.

## What has NOT been done

- **Nothing was renamed.** The plan, the records, and the Planner's draft file keep their current names
  pending the ruling.
- **The frozen `A4s` packet was not touched** — it is accepted, closed, and its hash is unchanged.
- **The Planner is holding the packet header**, correctly, pending the ruling.
