# `A4b2-r5` adequacy review — `INADEQUATE` (row precedence intercepts `AC-BOOT` FAIL)

**Reviewer:** fresh Planner child `6f59b63d-9094-47e3-9773-26302f53d946`, `codex/gpt-6-sol` @ `high`
(§1, §5.1.5(3)). Not the authoring child `dc828854-0cd8-4282-99fa-ab7fa6a45832` (r5) nor
`b608eb13-a4d9-494d-87e9-20fb15937cf6` (r4).
**Revision reviewed:** `docs/packets/a4b2-gp-clears-pending-word.md`, `A4b2-r5`, SHA-256
**`652088DB528E2ECEEC7E9954530FCBD0C6A9228A144B0AE9066B785FDDD40B8E`** (independently recomputed,
134 lines).
**Review type:** §5.4 re-review after `INADEQUATE` — the blocking defects, the changed regions, and a
contract-completeness check.
**Recorded verbatim** from the reviewer's own response, as §2.2.10 requires. Written to disk **before**
any revision work began.

---

## Verdict block (verbatim)

```text
REVISION:          A4b2-r5; SHA-256 652088DB528E2ECEEC7E9954530FCBD0C6A9228A144B0AE9066B785FDDD40B8E (independently recomputed; 134 lines).
READ:              docs/packets/a4b2-gp-clears-pending-word.md:1–134; docs/reviews/a4b2-r4-adequacy-review.md:1–144; docs/reviews/a4b2-r4-advisor-blocker-ruling.md:1–172; docs/reviews/a4b2-r4-session-mechanical-verification.md:1–189 (command verification lead); docs/agent-workflow.md:1–893; plan-jsrf-bare-minimum.md:1–110; docs/jsrf-run-profiles.md:1–220; docs/reviews/a4b-watch-ledger-ruling.md:1–106; docs/reviews/a4b-q1-advisor-ruling.md:1–100; docs/packets/a4a-dsp-pending-word.md:34–61; toolkit src/apu/dsp/gp_ep.c:75–179,340–421,420–524,570–679; toolkit src/apu/apu_watch.c:120–229,650–779,785–889,985–1038; toolkit src/apu/apu_core.c:420–471,630–685; archived logs/runs/20260924-191833-331-a4a-r2-trap-trace/{metadata.json:1–120,result.json:1–14,jsrf_run.log:3185–3214} and logs/runs/20260926-010303-411-a4b1-default/{metadata.json:1–105,result.json:1–14}. Read-only git identity/status/XBE SHA checks: game a000662, toolkit 3a3c7c1 clean, XBE FD190557…F3EF9C; current game has unrelated dirty docs plus draft.
PREMISE_FRESHNESS: BOUNDED — observed archived strict trap startup and accepted default pending spin with matching XBE/toolkit identities; no current trapped R1 result, but P1–P3/G1–G4 fail closed if stale or unavailable. No new changed premise.
BLOCKING:          AC-BOOT:76 versus first-match R2-UNKNOWN:119 / R2-NOBOOT:122 (and exhaustiveness:129). Concrete scenario: full uncapped log has first valid image I bootstrap and second complete valid GPRST bootstrap from correctly translated B loading different program J, contiguous headers n=1,2 and last counts boots=2, then J executes GP_CLEAR CAS of 3→0 after CPU_ANCHOR with insns>0. AC-BOOT expressly FAIL→R2-NOBOOT on J divergence; AC-CLEAR can PASS, so higher-priority R2-UNKNOWN clause 'AC-CLEAR PASS while AC-BOOT ... not PASS' matches first and wrongly routes to Planner/UNKNOWN instead of binding complete-divergent FAIL→R2-NOBOOT/A4c. Required: make cross-criterion UNKNOWN clause apply only to AC-BOOT/AC-RUN UNKNOWN (or otherwise avoid intercepting AC-BOOT FAIL), and explicitly preserve R2-NOBOOT priority for complete divergent block, while no AC-INPUTS evaluation outside qualifying PASS path.
DEFERRED:          NONE (no unchanged-text advisory hunting; nonexistent revision-log pointer, if noticed, is non-blocking record-pointer issue).
DECISIONS:         Retain every-instance/last-count AC-BOOT redesign; source counts before increment and emits uncapped numbered full blocks, missing header/count yields UNKNOWN; reverse on measured bypass or non-lossless source.
                   Retain narrowed transition-only claim and byte-identical steps 1–8 AC-CLEAR predicate per session's r4/r5 direct comparison; record-before-store race and Wf=3/F≥1 compatible, no guest wait-exit claim; reverse on a residual guest-state assertion or changed predicate.
                   Block promotion because changed AC-BOOT FAIL state is intercepted by first-match UNKNOWN row; reverse when row ordering/condition fixes concrete misroute and new exact bytes receive independent adequacy review.
VERDICT:           INADEQUATE
```

`VERDICT` is `INADEQUATE` because `BLOCKING` is not `NONE` (§5.3). `PREMISE_FRESHNESS: BOUNDED` would not
by itself have prevented `ADEQUATE`.

## Final closing block (verbatim) — the reviewer's authoritative final response

The closing message restates the same single blocker with tightened wording and `DEFERRED: NONE`.
**Verdict unchanged: `INADEQUATE`, same one blocker.**

```text
REVISION:          `A4b2-r5`; SHA-256 `652088DB528E2ECEEC7E9954530FCBD0C6A9228A144B0AE9066B785FDDD40B8E` (independently verified; 134 lines)
READ:              `docs/packets/a4b2-gp-clears-pending-word.md:1–134`; `docs/reviews/a4b2-r4-adequacy-review.md:1–144`; `docs/reviews/a4b2-r4-advisor-blocker-ruling.md:1–172`; `docs/reviews/a4b2-r4-session-mechanical-verification.md:1–189` (command-verification lead); `docs/agent-workflow.md:1–893`; `plan-jsrf-bare-minimum.md:1–110`; `docs/jsrf-run-profiles.md:1–220`; `docs/reviews/a4b-watch-ledger-ruling.md:1–106`; `docs/reviews/a4b-q1-advisor-ruling.md:1–100`; `docs/packets/a4a-dsp-pending-word.md:34–61`; toolkit `src/apu/dsp/gp_ep.c:75–179,340–524,570–679`, `src/apu/apu_watch.c:120–229,650–889,985–1038`, `src/apu/apu_core.c:420–471,630–685`; archived strict trap `20260924-191833-331-a4a-r2-trap-trace/{metadata.json:1–120,result.json:1–14,jsrf_run.log:3185–3214}`; accepted default `20260926-010303-411-a4b1-default/{metadata.json:1–105,result.json:1–14}`. Read-only identity checks confirmed game `a000662`, clean toolkit `3a3c7c1`, and XBE SHA-256 `FD190557…F3EF9C`.
PREMISE_FRESHNESS: BOUNDED — the archived strict trap startup and accepted default stop remain supported at matching identities; current R1 is unmeasured. Preconditions and gates fail closed on stale or missing evidence.
BLOCKING:          `AC-BOOT:76` versus first-match `R2-UNKNOWN:119` and `R2-NOBOOT:122`. A complete first bootstrap loads `I`; a second complete, correctly translated bootstrap loads different program `J`. Headers `n=1,2` and last counts `boots=2` reconcile. If `J` then executes the qualifying GP clear, `AC-BOOT` is expressly **FAIL → R2-NOBOOT**, while `AC-CLEAR` can PASS. The earlier `R2-UNKNOWN` clause matches “AC-CLEAR PASS while AC-BOOT … is not PASS,” incorrectly routing this fully decided divergent-image case to UNKNOWN/Planner rather than NOBOOT/`A4c`. **Required:** prevent that clause from intercepting an `AC-BOOT` FAIL, while preserving UNKNOWN for incomplete evidence and `AC-INPUTS`’ conditional evaluation.
DEFERRED:          NONE
DECISIONS:         Retain every-instance bootstrap validation and last-count reconciliation; the inspected source supports their ordering and uncapped records. Reverse on evidence of an unrecorded boot path.
                   Retain the narrowed transition-only claim and unchanged `AC-CLEAR` steps 1–8; the packet explicitly disclaims guest observation, spin exit, and progress. Reverse if a guest-state assertion or changed PASS predicate is found.
                   Do not promote these bytes; the concrete first-match misroute is blocking. Reverse after a revised row excludes it and that revision is reviewed.
VERDICT:           INADEQUATE
```

## The reviewer's intermediate findings (verbatim, in order)

1. **Opening.**
   > "Review underway: r5 SHA independently matches 652088DB…40B8E; source confirms counts-before-increment and all-instance image checks. Probing subtle last-count stale-tail and false-FAIL scenarios before verdict; do not promote yet."

2. **Row conflict found.**
   > "Found concrete row conflict: r5 AC-BOOT:76 explicitly FAIL→R2-NOBOOT for complete divergent block, but decision row R2-UNKNOWN:119 has higher priority on AC-CLEAR PASS while AC-BOOT not PASS. Scenario complete boot image J≠I, counts reconcile, GP runs and CAS clears (AC-CLEAR PASS), AC-BOOT FAIL; literal first-match selects R2-UNKNOWN instead of mandated R2-NOBOOT/A4c. Investigating whether this rises to blocking under §3.1 vs existing intentional contradiction; promotion on hold."

3. **Confirmed blocking.**
   > "Confirmed §3.1 blocking row misrouting: packet :76 and binding Advisor ruling :25–32 require complete divergent [GPBOOT] → FAIL/R2-NOBOOT, but first-match R2-UNKNOWN :119 intercepts whenever AC-CLEAR PASS and AC-BOOT not PASS. Plausible n1=I,n2=J, complete N=2/last boots=2; anchor→J-executed GP_CLEAR CAS, insns>0; BOOT FAIL and CLEAR PASS; routes R2-UNKNOWN instead of R2-NOBOOT/A4c. This is changed AC-BOOT's row/claim-completeness issue, even though row text persisted. Verdict likely INADEQUATE; no promotion. Remedy narrow R2-UNKNOWN's cross-criterion clause to UNKNOWN-only, preserving FAIL→NOBOOT/NOFRAMES/NOEXEC priority (or otherwise explicitly resolve directed fail routing)."

---

## Session independent verification of the blocker

The blocker turns on row text and ordering, which the Session can verify mechanically without judging
blocking-vs-advisory (that classification is the reviewer's, §2.2.6). **All rows are Session observations**
read from the frozen r5 bytes.

| Claim | Measurement | Result |
|---|---|---|
| Rows are first-match-wins | packet `:116` — *"### Decision rows (in order, first match wins)"* | **CONFIRMED** |
| `R2-UNKNOWN` carries the cross-criterion clause | `:119` — *"**any AC other than AC-INPUTS** is UNKNOWN; AC-CLEAR PASS while AC-BOOT or AC-RUN is not PASS; …"* | **CONFIRMED** |
| `R2-UNKNOWN` precedes `R2-NOBOOT` | `R2-UNKNOWN` is the 2nd row (`:119`); `R2-NOBOOT` is the 5th (`:122`) — three rows intervene | **CONFIRMED** |
| `AC-BOOT` directs a complete divergent block to FAIL → `R2-NOBOOT` | `:76` — *"**FAIL → R2-NOBOOT:** after complete contiguous blocks and matching last-count reconciliation, at least one complete block has a divergent transition, translated `sge0_va`, or image word."* | **CONFIRMED** |
| The exhaustiveness note repeats the `R2-NOBOOT` promise | `:129` — *"After the first four rows, AC-BOOT FAIL selects NOBOOT"* | **CONFIRMED** |
| The clause is reachable with `AC-BOOT` FAIL and `AC-CLEAR` PASS simultaneously | The clause's condition is *"AC-BOOT **or** AC-RUN is **not PASS**"*, which FAIL satisfies; and `AC-BOOT` FAIL does not prevent `AC-CLEAR` PASS — `AC-CLEAR` steps 1–8 (`:88-95`) never consult `AC-BOOT`'s result | **CONFIRMED** |

**The contract contradicts itself in two places**: `AC-BOOT`'s own text (`:76`) and the exhaustiveness note
(`:129`) both promise `R2-NOBOOT`, while the first-match order delivers `R2-UNKNOWN`. A literal executor
following *"first match wins"* selects `R2-UNKNOWN`.

**The consequence is a wrong next action, not only a wrong label.** `R2-UNKNOWN` routes to the **Planner**
for re-planning; `R2-NOBOOT` routes to **`A4c`** bootstrap/SGE discovery. A divergent image is a *refuted*
premise with a known next packet, so routing it as "unknown" loses the direction the packet exists to give.

### Was this pre-existing or introduced?

Both facts are needed, and both are Session observations:

- **The row text persisted from r4.** The clause and the `AC-BOOT FAIL` shape are unchanged text, which the
  reviewer acknowledged (*"even though row text persisted"*).
- **But the conflict was not reachable before this revision.** r4's `AC-BOOT` inspected only the **first**
  `[GPBOOT]` block and could not FAIL on a later divergent bootstrap at all — that was precisely r4's
  blocker (1). The Advisor-mandated redesign to every-instance validation is what makes `AC-BOOT` FAIL
  reachable while `AC-CLEAR` PASSes. The reviewer's characterization is therefore correct: this is the
  **changed** `AC-BOOT`'s row/claim-completeness issue, and §5.4 puts it inside this revision's remit.

### The reviewer retained the redesign and the narrowed claim

Recorded because it bounds the next revision: the reviewer's `DECISIONS` **retain** the
every-instance/last-count `AC-BOOT` redesign and the **narrowed transition-only claim** with `AC-CLEAR`'s
steps 1–8 byte-identical. Neither of the two prior blockers is reopened. Only the row precedence is
blocking.

---

## Consequential obligations

1. **`A4b2-r5` is not promoted.** It remains a draft; `CURRENT PACKET` still names `A4b1-r4` only, and
   `A4b2` has no promoted revision. No implementation is authorized.
2. **The revision repairs the row-precedence defect** (§5.4 After-INADEQUATE). The reviewer's required
   outcome: make the cross-criterion clause apply **only** to `AC-BOOT`/`AC-RUN` **UNKNOWN** (or otherwise
   avoid intercepting `AC-BOOT` FAIL), explicitly preserve `R2-NOBOOT` priority for a complete divergent
   block, preserve `FAIL→NOFRAMES`/`NOEXEC` priority, and keep `AC-INPUTS` unevaluated outside the
   qualifying PASS path.
3. **No narrative about the repair** in the packet (§5.4, §5.7).
4. **The exhaustiveness note (`:129`) must be updated to match** any row-condition change; it currently
   asserts the outcome the order does not deliver.
5. **Re-review** by a Planner that did not author the revision (§5.4).
6. **The reviewer's `DEFERRED` is `NONE`.** It also noted the packet's revision-log pointer names a
   non-existent file; it classified that as a **non-blocking record-pointer issue** (§3.2), which the
   Session may repair as record-keeping without re-review.
7. **§5.5 consideration, recorded for the Planner.** `AC-BOOT` has now been implicated in three
   consecutive adequacy verdicts (r3 B1 `sge0`; r4 blocker 1 first-instance-only; r5 this row
   interception). The first two were defects in **what `AC-BOOT` measures**; this one is in **how its FAIL
   routes**, a different mechanism, and the reviewer expressly retained the measurement redesign. Whether
   that distinction is sufficient to avoid a further §5.5 redesign is a **Planner judgment** (§2.3), and
   the Planner may take it to the Advisor (§4.1). The Session does not decide it.
