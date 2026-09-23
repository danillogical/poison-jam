# P0.2 implementation acceptance — rounds 1 and 2, fixes, and round 3

**Reviewer:** `workbuddy-ai/hy4-preview-f` @ `high` (child `11d0cc43…`), a different
model family from the session. Read-only.

## Round 1: AC4 AGREED, AC1/AC2/AC3 DISAGREED

**All three disagreements were verified by the session before anything was changed,
and all three were real.** This review is the most valuable of the session: it found
fail-opens in the very machinery meant to prevent unsupported acceptance claims.

| finding | verified | disposition |
|---|---|---|
| **AC1** — identity fields checked for *presence* only, never value | **Confirmed.** `parent_id`, `child_id`, `turn_id`, `requested_model`, `requested_effort` and `harness` set to arbitrary values **all at once** (`parent_id=session-TOTALLY-DIFFERENT`, `child_id=deadbeef-…`, `turn_id=99`, `model=gpt-6-luna`, `effort=low`) still returned **acceptance-eligible** | **FIXED** |
| **AC2** — three contract-named fixtures absent | **Confirmed.** No fixture for two parents with identical labels, none for unrelated `ACCEPT` text through an adapter, none for unreadable source / unknown schema via the adapters | **FIXED** |
| **AC3** — the `delta` block is never read | **Confirmed.** `"delta"` occurred **0 times** in `jsrf_review_records.py`; a record carrying `delta.affected_criteria=['P0.1-AC1']` still returned eligible, so a re-bind could launder a stale review | **FIXED** |
| **Exit codes** — interface requires 2 for invalid input | **Confirmed.** Every non-eligible status returned 1, so INVALID was indistinguishable from FAILED | **FIXED** |
| **AC4** — historical reconciliation honest | **AGREED**, fully verified by the reviewer against both originals | carried |

## What changed

**1. Identity is now verified against the source, not trusted.** A record must
declare `identity_evidence.source_session`, and the validator **re-opens that
original log** and compares what it actually says:

- `child_id` and `parent_id` must match the session header
- `turn_id` must be a **completed** turn in that session (an errored turn is not a
  verdict)
- `harness` must match the source's own record type (a DSH log vs a Codex rollout)
- `requested_model` / `requested_effort` must match the descriptor, compared on the
  model component so a fully-qualified route (`provider/model`) is accepted
- every identity field must also be a **nonempty string**, not merely present

All seven falsifications now exit **2**.

**2. The `delta` block is enforced.** A re-bind is not a re-review, and that rule
was previously unenforced. A delta must record a `reason` and declare
`affected_criteria`; a **non-empty** list leaves the packet **pending**, because the
human confirmation the amendment requires is not machine-checkable.

**3. Exit codes follow the interface.** INVALID → 2, FAILED → 1, ELIGIBLE → 0.

**4. The three missing AC2 fixtures exist**, routed through the source adapters
rather than the classifier alone: identical labels separated by ancestry, identical
child ids with different parents, unrelated `ACCEPT` prose, an unreadable source, a
missing source, and an unknown schema version.

**5. A producer guard for the attribution bug** that produced this whole round: the
producer now refuses a footer naming criteria outside the requested packet, and
requires the footer population to match the independent manifest. It also
**preserves an existing `delta` block** on re-run — measured, rewriting the P0.1
record had silently dropped it.

**6. The published schema is now ENFORCED, not documented.** The reviewer's residual
note was that `check-recorded-reviews.py` never validated against
`docs/reviews/review-record.schema.json` — no `jsonschema` call existed anywhere in
`scripts/` — so "the schema matches the validator" was a test-only property. A
missing schema is now a finding rather than a silent skip, and a small structural
checker covers the keywords the schema actually uses (`type`, `required`, `enum`,
`const`, `minLength`, `minItems`, `pattern`, `properties`, `items`), because
`jsonschema` is not installed and is not a project dependency.

**That change surfaced a real conflict, resolved in favour of the amendment.** The
schema declared `minLength: 1` on `procedure`/`observed_result` and `minItems: 1` on
`evidence`, so an empty value came out **INVALID** (exit 2) — but the amendment
specifies **CANNOT VERIFY** (exit 1) for exactly those cases. The amendment is
right: an empty procedure is a *well-formed record with insufficient evidence*, not
a malformed one, and collapsing the two would make an unverified criterion look
like a broken record. The schema now documents why those keywords are deliberately
absent, and the ordering is asserted by tests.

**7. Auditing the checker found it enforcing less than the schema claimed.** A
hand-written checker is only as good as its keyword coverage, so the two keyword
sets were compared mechanically: the schema uses `additionalProperties`, and the
checker **ignored it**. It is used on `reviewed_files` entries, criterion entries and
`identity_evidence`, so an undeclared key passed. Now enforced — measured, adding
`identity_evidence.smuggled` is rejected — and a standing test fails if the schema
ever uses a structural keyword the checker does not handle.

**The tolerance the fixes needed is recorded, not hidden.** `P0.4-AC1` and
`P0.4-AC3` in the real P0.3–P0.7 review came out `CANNOT VERIFY` on first
re-validation, because the reviewer wrote `**P0.4-AC1 AGREED** (evidence).` and
`**P0.4-AC3 AGREED, reproduced.**` — a bolded verdict followed by its evidence. The
no-trailing-clause rule refused both. Two bounded tolerances were added (a
parenthesised citation; a short appositive restating how the verdict was reached),
each with a matching negative test proving a hedge in the same position is still
refused, and a hedge *inside* the parenthesis is still refused. The amendment is now
`r13`.


## Round 2: AC1, AC3, AC4 AGREED — AC2 DISAGREED on one residual

Round 1's three disagreements were accepted as fixed. Round 2 found **one new
blocking defect**, and it was a defect in round 1's *own fix*:

**The parenthetical tolerance I added to satisfy the real P0.4 cases was an
enumeration, and it failed OPEN.** I guarded against a hedge inside the parenthesis
with a word list (`not|never|pending|await|unless|if|would|provisional|…`). The
reviewer defeated it: **12 of 18** hedged forms corroborated a false `AGREED`,
including `(for now)`, `(in part)`, `(under protest)`, `(except for AC2)`,
`(on hold)`, `(was held back)`, `(needs the rerun)` and `(yet AC2 fails)` — every one
of which the amendment itself records as withholding. My two negative tests only
tried `pending`/`provisional`/`not yet`, i.e. words already on the list, so they
passed while the hole was open.

**This is the r5–r7 mistake reproduced inside the fix for it.** The amendment's own
history records that the vocabulary of withholding is unbounded and that enumerating
it fails; I then enumerated it again, in a new place.

**The fix inverts the direction.** A parenthesis is admitted only when *all* its
content is citation-like — paths, numbers, hashes, and a small evidence vocabulary.
Any word not on that list **refuses**, so an unlisted hedge fails **CLOSED**. All 18
measured hedges are now refused and all 9 honest citations admitted, including the
two real P0.4 cases. The first fail-closed attempt refused two honest citations
because stripping backticks before paths split `tests/test_dump_controls.py` into
`tests/test` + `dump controls.py`, leaving `dump` behind; the ordering is now
documented in the code.

The reviewer explicitly confirmed the **appositive** tolerance is safe, because the
tail after the appositive is still judged — `AGREED, reproduced awaiting the rerun.`
correctly stays pending.

## AC2's residual, now closed: the verdict was not bound to its turn

The reviewer's remaining finding was the sharpest of the two rounds: **ancestry was
verified, but the verdict body was not.** `verdict_text` was bound only by the
tracked-file hash and by `contradiction_evidence` containment, both of which a
self-authored record satisfies. A record could therefore declare a *genuinely
verified* ancestry — real child, real parent, real completed turn, correct route —
and supply an **invented verdict body**, which is the part that carries the
dispositions. Verifying ancestry without verifying the verdict verifies the part
that decides nothing.

The validator now re-opens the source session, reads the text of the completed turn
`turn_id` names, and requires `verdict_text` to **equal** it. Measured:

| case | result |
|---|---|
| unmodified real record | exit 0 |
| invented `verdict_text` | exit **2** |
| invented, with a self-consistent `contradiction_evidence` | exit **2** |
| real verdict plus a paraphrased sentence | exit **2** |
| CRLF-normalised real verdict | exit 0 — tolerated, because a producer may rewrite line endings |
| completed turn with no readable assistant text | exit **2** |

## Verification

- `tests/test_recorded_reviews.py` **160 tests** (was 112 at round 1), exit 0
- `tests/test_codex_export.py` **30 tests**, exit 0
- **359 Python tests** across 8 suites, all exit 0
- Both real records validate `acceptance-eligible`, exit 0
- 18 of 18 measured parenthetical hedges refused; 9 of 9 honest citations admitted
- Four verdict-binding falsifications exit 2; CRLF normalisation exits 0

## Status

**Round 3 re-review requested** for AC2 only. AC1, AC3 and AC4 were AGREED at round
2, and the changes since — the parenthetical rule and the verdict binding — are both
AC2 evidence, so they should carry unless the reviewer finds the changes touch them.

**What this review sequence demonstrates.** Two rounds, five blocking defects, and
every one of them was a check that asked whether something was *present* rather than
whether it was *true*. The most instructive is the last: the fix for round 1's
parenthetical problem reintroduced the exact enumeration the amendment's own history
records as unclosable, and only an adversarial re-review caught it. A fix is a
hypothesis until something tries to break it.
