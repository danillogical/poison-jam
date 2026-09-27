# `A4b2` — Advisor ruling on both adequacy blockers (AC-BOOT redesign, AC-CLEAR claim narrowing)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Advisor:** persistent handle **`muse_FkNhGaXtV9P5`**, model **`muse-spark-1.3-contributor`**,
`reasoningEffort: max` (§4.4).
**Consulted under:** §2.3 (technical decision-maker), §4.1 `Planner -> Advisor` / §5.5 methodology
referral.
**Question:** what must `A4b2-r4` claim and measure, given that `AC-BOOT` has now carried a blocking
defect in two consecutive adequacy verdicts (§5.5) and that the `AC-CLEAR` PASS condition provably
cannot establish that the guest's wait ended.

Recorded **verbatim** as §3.3 and §4.4 require. The Advisor's turn was sent with **no effort override**;
the session default is `max`, and a no-override turn on the same handle immediately afterwards reported
`reasoningEffort: max`. The ruling turn's tier is therefore **inferred** to be `max` on that basis, not
directly observed (its send timed out before reporting the field; the text was collected with
`muse_session_read`, which does not return the effort field).

---

## Ruling (verbatim)

```text
RULING:

Q1 — AC-BOOT shape: (iii) something else. Redesign AC-BOOT from first-instance
validation to EVERY-INSTANCE validation plus reconciliation. Require: every
[GPBOOT] header in the R1 log present with contiguous n=1..N (headers self-number;
n equals post-increment s.boots); N equals boots on the LAST counts line; every
block satisfies the current per-block predicates (valid gprst/prev transition,
sge0_va=B, all 0x173 words equal I). A complete-but-divergent block keeps the
existing FAIL shape (→ R2-NOBOOT); missing/non-contiguous/incomplete blocks or a
count mismatch is UNKNOWN. Warn the Planner: the per-bootstrap counts emission
precedes boots++, so reconciliation uses the last counts line, never adjacency.
This changes what AC-BOOT measures (first → all + reconcile) and SATISFIES §5.5;
no split and no discovery packet — all deciding evidence already exists in the R1
log and evaluation stays mechanical parsing. AC-RUN is untouched: its claim is
run-level retirement (still true), and the GP_CLEAR latch's insns>0 already binds
execution to the governing boot because counters reset per bootstrap.
  - Reject (i) bind-via-counts: fragile. boots++ runs on the guest-MMIO thread;
    the post-latch counts emission runs on the APU thread as a separate write
    after the latch line. A reset landing in that window binds the clear to the
    wrong instance. Validate-all needs no cross-thread ordering at all.
  - Reject (ii)-alone: satisfiability risk. Whether the title re-bootstraps in a
    full run is UNKNOWN (A4a showed one transition in one run); fail-closed on a
    second boot could make the packet systematically unpassable. Validate-all is
    equally safe and strictly more satisfiable.

Q2 — AC-CLEAR and the claim: (ii) NARROW THE CLAIM, alone — no wait-exit witness
in this packet. Restate Establishes in Q1-condition-1's own operative terms: the
3→0 transition at B+0x810 was performed by the GP engine's memory-write path
while executing validated image I, after the anchored store was recorded, with
no synthetic ack, no instrumented competing CPU zero, and all pre-exchange GP
inputs guest-written-by-region or modelled. Drop "satisfied the pending-word
wait" as a guest-state assertion. The exchanged-3 identity stays
anchor-CORROBORATED (the binding watch-ledger wording), never anchor-proven:
state the record-before-store race as a claim limit. Wf/F stay corroboration-only
with an explicit limit: final Wf=3/F≥1 is COMPATIBLE with R2-PASS (later
re-store/re-entry) and R2-PASS establishes no guest observation of the 0, no spin
exit, no progress past the spin. AC-CLEAR's predicate needs NO change — it already
decides exactly the transition-plus-attribution Q1 condition 1 requires; the
defect is claim wording, not the criterion. Steps 4–8 row routing is unchanged.
  - No (i): a wait-exit gate would make PASS depend on deadline timing plus the
    currently-UNKNOWN re-entry behavior — systematically unsatisfiable if the
    guest submits commands repeatedly — F is sampled stacks (weak absence), and
    Q1's sequencing FORBIDS strict boot-progress-past-spin claims before a
    PIO_FREE model packet, "not inside A4b". A wait-exit witness belongs to a
    future packet; the re-entry measurement is its follow-up lead, NOT a
    prerequisite of this revision (narrowing dissolves the dependence).
  - NO AMENDMENT to a4b-watch-ledger-ruling.md or to Q1. The ledger ruling's PASS
    predicate establishes the exchange/attribution and stands; Q1's conditions
    1–2 ARE the narrowed claim's content. Interpretation recorded as a case
    ruling: Q1's "this wait was satisfied" (Q1:54-55) is defined by its
    conditions 1–2 — "the value that ends the wait is the 0" (Q1:23-24) — because
    condition 4 explicitly excludes guest progress/liveness past the spin from
    the strict claim. The packet's error was promoting that into guest-state
    satisfaction.

BASIS (load-bearing only):
- Observed: both blockers verbatim and the Session's mechanism table
  (a4b2-r4-adequacy-review.md:19,38-39,73-94); packet claim/ACs/rows
  (a4b2-gp-clears-pending-word.md:17,71-76,84-96,127); r3 B1 vs "AC-CLEAR
  unchanged and sound" (a4b1-a4b2-r3-adequacy-review.md:32-41,75) → §5.5
  triggers for AC-BOOT only.
- Observed: re-bootstrap path (gp_ep.c:358-360,362,398) with counter reset
  (apu_watch.c:682-684); [GPBOOT] header prints n=s.boots post-increment,
  uncapped straight fprintf+fflush (apu_watch.c:1009-1022; gp_ep.c:373
  precedes :398); per-bootstrap counts emission precedes boots++
  (gp_ep.c:364 vs :373); latch line then counts line are separate writes
  (apu_watch.c:166-170); record-before-store (apu_watch.c:746-755; packet:36);
  anchor-is-corroboration + seq-approximate (a4b-watch-ledger-ruling.md:38,44,58);
  non-atomic s.boots++ (apu_watch.c:682) with GP vs guest writers on different
  threads (predecessor-observed, ibid.:84).
- Observed: Q1 condition 1 operative text (:30-36), "the value that ends the wait
  is the 0" (:23-24), condition-4 exclusions (:47-55), PIO_FREE-first sequencing
  (:12-13, :64-66); O-6 acceptance "+0x810 cleared by GP execution in a STRICT
  run" (a4a-dsp-pending-word.md:53).
- Inferred: post-latch boots binding has a real (if narrow) cross-thread race
  window; a final-state wait-exit gate risks systematic unsatisfiability under
  legitimate re-entry in a deadline-terminated run.
- Uncertain: whether the title re-bootstraps in a full run; whether
  sub_001A19D6 repeats (Session's open boundary — explicitly not needed for
  this revision).

REVERSED BY: a bootstrap path that increments boots without emitting [GPBOOT]
under R1 conditions (breaks reconciliation — then fail closed on boots>1 instead);
direct evidence that Q1's "wait was satisfied" was intended to require observed
guest exit (then narrowing is insufficient and the PIO_FREE-first sequence
applies); a second GP DMA write path bypassing the latch point (predecessor's
own reverser).

RECORD IN: the next A4b2 review record, verbatim with Advisor handle/model/
reasoningEffort; the Q1-scope interpretation recorded there as a case ruling
(§3.3), NOT as an edit to either prior ruling. Revision grounds remain §5.4
After-INADEQUATE; this ruling discharges the §5.5 methodology referral for
AC-BOOT. Re-review per §5.4 by a non-authoring Planner.
```

## Session verification of the ruling's new load-bearing citations

The Advisor reported two findings **beyond the brief** that the repair depends on. Both were checked by
the Session before the ruling was relied on. **All rows are Session observations** at toolkit `3a3c7c1`.

| Advisor citation | Session check | Result |
|---|---|---|
| `[GPBOOT]` headers self-number with post-increment `s.boots`, uncapped | `apu_watch.c:1009-1011` — `fprintf(stderr, "[GPBOOT] n=%u …", s.boots, …)`, a plain `fprintf` + `fflush` with **no cap**; `gp_ep.c:373` `apu_watch_gp_bootstrap(1)` (which does `s.boots++`) precedes `:398` `apu_watch_trace_gpboot(...)` | **CONFIRMED** — so the header's `n` is the post-increment boot ordinal, and headers cannot be dropped |
| the per-bootstrap counts emission **precedes** `boots++` | `gp_ep.c:364` calls `apu_watch_gp_bootstrap_done(dsp->is_gp)` → `emit_counts()` (`apu_watch.c:694-700`), while `:373` calls `apu_watch_gp_bootstrap(1)` → `s.boots++` (`:677-685`). Emission is **before** the increment | **CONFIRMED** — this is exactly why the ruling warns reconciliation must use the **last** counts line and never adjacency |
| latch line and counts line are separate writes | `apu_watch.c:166-170` — the latch emitter is followed by a separate counts emission | **CONFIRMED** |
| `s.boots++` is non-atomic | `apu_watch.c:682` `s.boots++;` (plain increment, not `InterlockedIncrement`) | **CONFIRMED** |

### Independent measurement: the observed bootstrap count in the archived strict trap run

The Session measured the one archived STRICT trap run (`logs/runs/20260924-191833-331-a4a-r2-trap-trace`)
to test the Advisor's `Uncertain` item — *"whether the title re-bootstraps in a full run is UNKNOWN"*:

| Measurement | Value |
|---|---|
| `[APUMMIO] write 0x3FFFC` (GPRST) lines | `00000000`, `00000001`, `00000003` — **one** transition to both-bits (`3`), from `1` |
| `[APUMMIO] write 0x02040` (GPSADDR) lines | **one** (`803CC000`) |
| `[APUMMIO]` total lines / cap | **344** against a **400** cap (`apu_mmio_hook.c:292`), with **no** "trace cap 400 reached" marker (`:309`) — the log is **complete**, so this is not an absence-without-coverage claim |
| `[GPBOOT] n=` headers | **zero** — expected: that archived run predates the `[GPBOOT]` instrumentation |

**Interpretation (Session, bounded):** in that one archived strict trap run the title bootstrapped the GP
**once**, and the `[APUMMIO]` record covering it is complete. This **corroborates** the Advisor's
`Uncertain` item without resolving it: one run is not a general claim, and `boots` is not observable in
that archive because `[GPBOOT]` did not exist yet. It does mean the validate-all design the Advisor chose
is very likely satisfiable — which is the point of preferring it over fail-closed-on-second-boot.

## Consequential obligations

1. **`A4b2-r5`** is the revision. Grounds remain **§5.4 After-INADEQUATE**.
2. **`AC-BOOT` is redesigned**, discharging the **§5.5** methodology referral for that criterion. It is
   **not** split and **not** moved to a discovery packet.
3. **`AC-CLEAR`'s predicate does not change.** Only the packet's **claim wording** narrows. Steps 4–8 row
   routing is unchanged.
4. **No amendment** to `docs/reviews/a4b-watch-ledger-ruling.md` or to
   `docs/reviews/a4b-q1-advisor-ruling.md`. The Q1-scope interpretation is recorded **here** as a case
   ruling (§3.3).
5. **Re-review** is by a Planner that did not author the revision (§5.4).
6. **Follow-up lead, not a prerequisite:** measuring whether `sub_001A19D6` (and thus the store-then-spin
   sequence) is re-entered. The Session's measurement boundary on this is recorded in
   `docs/reviews/a4b2-r4-adequacy-review.md`; narrowing the claim dissolves the packet's dependence on it.

## Case ruling — the scope of "the wait was satisfied" (§3.3)

Recorded here as the owning record, since the Advisor directed it be recorded as a case ruling and
**not** as an edit to either prior ruling:

> Q1's *"this wait was satisfied"* (`a4b-q1-advisor-ruling.md:54-55`) is **defined by its conditions
> 1–2** — *"the value that ends the wait is the 0"* (`:23-24`) — because condition 4 (`:47-55`)
> explicitly **excludes** guest progress and liveness past the spin from the strict claim.
> `A4b2-r4`'s error was promoting that defined scope into a **guest-state** satisfaction assertion.
> The corrected claim therefore states the transition and its attribution, not the guest's exit.
