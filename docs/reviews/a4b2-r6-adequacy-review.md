# `A4b2-r6` adequacy review — `ADEQUATE`

**Reviewer:** fresh Planner child `36e21ac4-ffcf-49b6-a3ae-80abe39da281`, `codex/gpt-6-sol` @ `high`
(§1, §5.1.5(3)). Not the authoring child `636dc591-2e30-41fb-8cf6-46d108135165`.
**Revision reviewed:** `docs/packets/a4b2-gp-clears-pending-word.md`, `A4b2-r6`, SHA-256
**`3273409262437D42A500A1E988E39CB25234C2E5E2A96007A9BE74FEB6B250E5`** (134 lines).
**Review type:** §5.4 re-review after `INADEQUATE` — the blocking defect, the changed regions, and a
contract-completeness check.
**Recorded verbatim** from the reviewer's own response, as §2.2.10 requires. Written to disk **before**
promotion.

---

## Binding review conclusion (verbatim)

> Binding review conclusion: ADEQUATE for r6 hash 3273409262437D42A500A1E988E39CB25234C2E5E2A96007A9BE74FEB6B250E5; BLOCKING NONE; PREMISE_FRESHNESS BOUNDED. I directly read full packet and r5 verbatim verdict/blocker, run profiles, archived strict trap/default metadata/results, toolkit GP bootstrap/counts/header source. :119 now tests only literal UNKNOWN statuses, not directed FAIL, and AC-INPUTS evaluated UNKNOWN only on all-four-PASS path; :120–121 intentionally precede :122–124 and :129's 'otherwise' correctly describes them. First=I, second=J, N=last boots=2, CLEAR PASS now reaches NOBOOT if no higher-priority independent CPU/unattributed result. r5 actual bytes are not preserved (history:29–38); byte-identity of unchanged regions is established by Session's diff report, not independently reconstructible, but current :71–76 and :88–95 retain reviewed predicates; this is provenance limitation, not operative blocker. Formal block follows shortly; do not edit packet on ADEQUATE—freeze/promote identical hash same step.

**Verdict: `ADEQUATE`.** `BLOCKING: NONE`. `PREMISE_FRESHNESS: BOUNDED`.

Per §5.3, `ADEQUATE` is exactly `BLOCKING: NONE` with `PREMISE_FRESHNESS` not `FAIL`. `BOUNDED` is
permitted: the premise holds for the target revision with stated uncertainty, and the packet fails closed
if it is stale (P1–P3 and G1–G4 stop or return UNKNOWN on stale identity or missing evidence).

## What the reviewer independently confirmed

Recorded as the reviewer's own observations, from its message:

| Confirmed | Detail |
|---|---|
| Revision and hash | `A4b2-r6`, `32734092…B250E5` |
| The blocker is repaired | *":119 now tests only literal UNKNOWN statuses, not directed FAIL"* |
| The r5 blocker scenario now routes correctly | *"First=I, second=J, N=last boots=2, CLEAR PASS now reaches NOBOOT if no higher-priority independent CPU/unattributed result"* |
| `AC-INPUTS` conditional evaluation preserved | *"AC-INPUTS evaluated UNKNOWN only on all-four-PASS path"* |
| Row priority is intentional, not accidental | *":120–121 intentionally precede :122–124 and :129's 'otherwise' correctly describes them"* — i.e. `R2-CPU`/`R2-UNATTRIBUTED` correctly outrank `R2-NOBOOT`/`R2-NOFRAMES`/`R2-NOEXEC`, which was the shadowing risk the Session flagged |
| The exhaustiveness note is truthful | `:129`'s *"otherwise"* chaining correctly describes the actual precedence |
| `READ` (its own report) | full packet; the r5 verdict and blocker verbatim; run profiles; archived strict-trap and default `metadata.json`/`result.json`; toolkit GP bootstrap/counts/header source |

## Provenance limitation recorded by the reviewer (non-blocking)

> *"r5 actual bytes are not preserved (history:29–38); byte-identity of unchanged regions is established by Session's diff report, not independently reconstructible, but current :71–76 and :88–95 retain reviewed predicates; this is provenance limitation, not operative blocker."*

The reviewer expressly classified this as a **provenance limitation, not an operative blocker**. It cites
`docs/reviews/a4b2-revision-history.md:29-38` — the durability-gap section the Session recorded when it
created that file. Two consequences, stated so they are not mistaken later:

- The **reviewed predicates survive in the current bytes**: `AC-BOOT` `:71-76` and `AC-CLEAR` `:88-95` are
  present and were read directly by this reviewer.
- The claim that those regions are **byte-identical to r5** rests on the Session's diff report, not on an
  independent reconstruction, because r5's body was never committed. Adequacy verdicts bind by SHA-256 to
  the bytes each reviewer named, so the r5 verdict stands for the r5 bytes; only re-diffing is lost.

## Session actions on this verdict

1. **This record written from the reviewer's own response before promotion** (§2.2.10).
2. **The packet was not edited** on `ADEQUATE` — the reviewer's explicit instruction, and §5.3's *"no
   discretion to revise first."*
3. **`A4b2-r6` at `32734092…B250E5` was frozen and promoted into `CURRENT PACKET` in the same step**
   (§5.3), byte-identical with no revision.
4. The formal §5.3 block, when the reviewer returns it, is appended below **verbatim**. It restates the
   same verdict and does not change it.

## Formal §5.3 review block (verbatim) — the reviewer's authoritative final response

```text
REVISION:          A4b2-r6; SHA-256 3273409262437D42A500A1E988E39CB25234C2E5E2A96007A9BE74FEB6B250E5 (independently Get-FileHash; packet read lines 1–134).
READ:              docs/packets/a4b2-gp-clears-pending-word.md:1–134; docs/reviews/a4b2-r5-adequacy-review.md:1–128 (verbatim verdict/blocker and Session verification); docs/reviews/a4b2-r4-adequacy-review.md:1–144; docs/reviews/a4b2-r4-advisor-blocker-ruling.md:1–172; docs/reviews/a4b2-r4-session-mechanical-verification.md:135–260 (lead, not substitute); docs/reviews/a4b2-revision-history.md:1–61; docs/agent-workflow.md:1–893; plan-jsrf-bare-minimum.md:1–629; docs/jsrf-run-profiles.md:1–381; docs/reviews/a4b1-r4-stage2-acceptance-review.md:1–139; toolkit src/apu/dsp/gp_ep.c:344–423,625–696; toolkit src/apu/apu_watch.c:665–725,998–1037; archived logs/runs/20260924-191833-331-a4a-r2-trap-trace/{metadata.json:1–137,result.json:1–14,jsrf_run.log:3185–3294}; logs/runs/20260926-010303-411-a4b1-default/{metadata.json:1–110,result.json:1–14}. Read-only Get-FileHash packet/XBE; git rev-parse/status in both repositories (game a000662…, toolkit clean 3a3c7c1…). No build/guest run/write.
PREMISE_FRESHNESS: BOUNDED — observed archived explicit STRICT trap startup and accepted STRICT default diagnostic stop, XBE hash FD190557…F3EF9C and pinned toolkit identity; no current r6 R1 run. P1–P3/G1–G4 fail closed on changed premises or unavailable/malformed evidence; archived trap source was older 0d7929c, not falsely claimed as a current GP-clear witness.
BLOCKING:          NONE
DEFERRED:          NONE (no advisory hunting in untouched text; r5 exact bytes were not preserved, so the claimed byte-for-byte r5→r6 three-region diff cannot independently be recreated from git, although Session reports it; current packet and r5's quoted operative text suffice for the review).
DECISIONS:         Retain every-instance AC-BOOT and last-count reconciliation: directly read source confirms counts emitted before increment, post-increment uncapped numbered headers and 64 eight-word lines; reverse on a measured unrecorded bootstrap or a non-lossless log.
                   Retain transition-only claim and AC-CLEAR steps 1–8: directly read :17–19,:88–96,:127 disavow guest observation/exit/progress and treat pre-store anchor as corroboration; r5 review and Session diff report these predicates unchanged (actual r5 byte-identity inferred, not independently provable since r5 body unpreserved); reverse on recovered contrary bytes or a widened guest-state PASS assertion.
                   Repair r5 directed-FAIL interception: observed :119 tests only literal UNKNOWN of a non-input AC or evaluated AC-INPUTS UNKNOWN, so AC-BOOT FAIL never satisfies it; :120–121's earlier CPU/unattributed branches are independent, explicit priorities, and absent those :122 receives the complete n1=I,n2=J,last boots=2,CLEAR PASS scenario. Likewise AC-RUN FAIL reaches :123/:124 unless a higher-priority independent result exists; :129's 'otherwise' states that precedence. Reverse on a legal decided FAIL satisfying :119 or an unhandled combination.
                   Preserve conditional AC-INPUTS: observed :108 and :129 demand BOOT/RUN/CLEAR/NOCPU all PASS, otherwise NOT EVALUATED; qualifying-path evaluated UNKNOWN routes :119, FAIL :126, PASS :127. No failed criterion is waived or deemed PASS; reverse if a run classification contradicts those predicates.
VERDICT:           ADEQUATE
```

`VERDICT` is `ADEQUATE` because `BLOCKING` is `NONE` and `PREMISE_FRESHNESS` is not `FAIL` (§5.3).

**Two reviewer observations worth carrying forward:**

- **`PREMISE_FRESHNESS: BOUNDED` names a specific limitation the Session had not stated:** the archived trap
  run's recorded toolkit source was **`0d7929c`**, not the current pinned `3a3c7c1`. The reviewer notes it
  was *"not falsely claimed as a current GP-clear witness"* — correct, and the packet's motivating evidence
  section already treats that run as historical context. The packet fails closed on stale identity via
  P1–P3 and G1–G4, which is what makes `BOUNDED` sufficient rather than `FAIL`.
- **The `DEFERRED` item is the same provenance limitation**, restated: the claimed byte-for-byte r5→r6
  three-region diff cannot be independently recreated from git because r5's body was never committed. The
  reviewer's own words: *"current packet and r5's quoted operative text suffice for the review."* Classified
  **non-blocking**, and it does not reopen anything.

## Consequential state

- **`A4b2-r6` is the promoted packet.** It is the only packet authorized to execute.
- **`A4b1-r4` remains accepted, pushed and complete.** Not reopened.
- **The next authorized action** is `A4b2-r6` execution by the Session, in game-only write scope:
  `src/recomp/gen/recomp_0000.c`, `src/recomp/gen/recomp_0005.c` (step-1 calls/declarations only),
  `src/diagnostics.c` (the forwarder only), `docs/reviews/a4b2-*.md`.
- **`AC-INPUTS` is `NOT EVALUATED`** unless `AC-BOOT`, `AC-RUN`, `AC-CLEAR` and `AC-NOCPU` all PASS.
