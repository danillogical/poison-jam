# `A4b2` revision history — non-authoritative

**This file is non-authoritative.** Per `docs/agent-workflow.md` §5.7 it records revision narrative only.
It is **not** reviewed for accuracy, it is **not** part of any contract, and it **loses to the contract on
any conflict**. The operative contract is `docs/packets/a4b2-gp-clears-pending-word.md` at whatever
revision is promoted in `plan-jsrf-bare-minimum.md`'s `CURRENT PACKET` block.

Created 2026-09-27 by the Session because the frozen packet carries a pointer to this file and no such
file existed. Written as record-keeping (§3.2: a missing record pointer is a non-blocking issue), not as
a contract change.

## Revision chain

| Revision | Status | Review | Recorded by |
|---|---|---|---|
| `A4b2-r1` | draft | `docs/reviews/a4b1-a4b2-adequacy-review.md` | git `8735165` |
| `A4b2-r2` | draft | `INADEQUATE` — `docs/reviews/a4b1-a4b2-r2-adequacy-review.md` | git `127d203` |
| `A4b2-r3` | draft | `INADEQUATE` — `docs/reviews/a4b1-a4b2-r3-adequacy-review.md` | git `fea49f1` |
| `A4b2-r4` (sketch only) | draft | — | git `11e03aa` |
| `A4b2-r4` (written body) | draft | `INADEQUATE` — `docs/reviews/a4b2-r4-adequacy-review.md` | **not committed**; SHA-256 `65BCB7F0C10C393205412C7F10D033604ED63BC982AC23C6967DC8F162F2D250`, 134 lines |
| `A4b2-r5` | draft | `INADEQUATE` — `docs/reviews/a4b2-r5-adequacy-review.md` | **not committed**; SHA-256 `652088DB528E2ECEEC7E9954530FCBD0C6A9228A144B0AE9066B785FDDD40B8E`, 134 lines |
| `A4b2-r6` | draft | pending | **not committed** |

**No `A4b2` revision has ever been `ADEQUATE`.** `A4b2` has no promoted revision, so no implementation is
authorized. `CURRENT PACKET` names `A4b1-r4` (accepted, pushed, complete).

## Durability gap — recorded so it is not mistaken for provenance

`A4b2` is at **`A4b2-r3` in git**. The **written `r4` and `r5` bodies were never committed**: r4's body was
authored in the working tree and overwritten by r5, and r5's body was overwritten by r6. Their content
survives only as the line citations inside their adequacy-review records:

- r4's blockers cite packet lines 74–76 (`AC-BOOT`), 87–90 (`AC-CLEAR`), 127 (`R2-PASS`), 17–19 (claim), 36 (step 1).
- r5's blocker cites packet lines 76, 116, 119, 122, 129.

A future reader cannot diff r4 or r5 from git. **Adequacy verdicts bind to the exact bytes each reviewer
saw and named by SHA-256**, so those verdicts remain valid for the revisions they name; only the ability to
re-read those bytes is lost. Any packet text quoted in a review record is the surviving witness.

## Revision grounds

- **`r2`** — write-once ledger replaces capped-log attribution.
- **`r3`** — define latch fields, make `[GPIN]` lossless, repin the P2 seam.
- **`r4`** — §5.4(2) `PREMISE_CHANGED` + §5.4 After-INADEQUATE + §5.4(3). Folds in
  `docs/reviews/a4b2-r4-planning-rulings.md`: grounds corrected; `AC-BOOT` compares `sge0_va` with the
  `803CC000` conjunct demoted; `AC-INPUTS` rewritten on the first `[GPIN] at_clear` block with a stated
  inference plus two can-fail checks; `0xFFFFB3` reclassified STUB leaving **five** modelled PERIPH
  offsets; P3 bound to the build-identity commit; NDEBUG coverage narrowed to a claim limit.
- **`r5`** — §5.4 After-INADEQUATE. Repairs r4's two blockers per
  `docs/reviews/a4b2-r4-advisor-blocker-ruling.md`: `AC-BOOT` redesigned to every-instance validation plus
  last-counts reconciliation; the claim narrowed to the `3→0` GP memory-write-path transition with the
  anchor corroborating only. `AC-CLEAR`'s predicate unchanged.
- **`r6`** — §5.4 After-INADEQUATE. Repairs r5's blocker: a first-match decision row intercepted
  `AC-BOOT`'s directed FAIL, routing a fully decided divergent-image case to `R2-UNKNOWN`/Planner instead
  of `R2-NOBOOT`/`A4c`.

## Disposition

Revisions `r1`–`r3` were INADEQUATE on `[GPIN]` accounting (two consecutive on the same mechanism, which
triggered the §5.5 redesign recorded in `docs/reviews/a4b-gpin-accounting-ruling.md`). `r4` and `r5` were
INADEQUATE on `AC-BOOT`, and the §5.5 determination for that pair is recorded in the `r6` planning round.
