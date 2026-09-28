# Advisor ruling — A2h critical path MOVED to the NULL thunk slot; producer line PARKED

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, `reasoningEffort: max`.
**Turn:** the critical-path referral raised by the Session during `A2h-named-producer-frame-r1` execution.
**Authority:** `docs/agent-workflow.md` §2.3 (Advisor), §4 (escalation).

**Ruling recorded verbatim below.** The Session did not switch packets unilaterally; it routed the
critical-path question with evidence, and the Advisor ruled that the path has moved.

---

## RULING

### Q1 — Critical path MOVED to the NULL slot. Yes, the line optimized the wrong question — say it plainly, then precisely

**Yes: several packets pursued "who made the 571MB size" on the premise that the OOM kills the run. That premise is false** — `test eax,eax; jl` at `0x00149E56`–`58` plus the clean `STATUS_NO_MEMORY` error return (verified bytes, quoted identically in both records) prove the OOM is handled. The run dies later, elsewhere, on the NULL thunk slot. **The critical path is now "what zeroed / what read as zero at `0x1C4064`".** "Both" is wrong as concurrency (single critical path); "neither" is wrong (the crash ends every trapped run at ≈4.77 s — nothing downstream proceeds without understanding it).

Fairness requires the exact fault location: pursuing the producer was CORRECT given the premise. The failure is that an **untested lead hardened into accepted fact** — the causal-slice packet itself warned the unchecked-NULL inference "must itself be tested, not assumed" (packet:16), and instead `a2h-mechanism.md` titled itself with it and cited the bare sequence (`0xC0000017` → NULL ICALL → `0xE0424943`) as evidence for "no check" — circular: sequence → inferred no-check → no-check explains sequence. Terminal-event attribution from temporal co-occurrence without reading the six bytes past the call is the lapse. That is a **compliance failure with existing discipline** (§2.4.1 observed/inferred labeling + §2.4.2 cheap direct checks), not a policy gap — so no new policy; the lesson is recorded (Q4) and the compliance point binds the next packet.

**Producer line: PARKED, not retired**, with explicit reactivation conditions (either reopens it): (i) the NULL investigation shows the slot death is downstream of allocation-failure error handling; (ii) a later gate needs the size explained after the NULL issue resolves. The byte-identical-35-runs anomaly stays unexplained and stays recorded. The producer work product transfers intact (same function: frame/caller/ABI binding, no-writer proof, call-site census, tested tools — all directly serve the NULL analysis).

### Q2 — Smallest correct next packet: triage discovery (offline mining + existing `RECOMP_KERNEL_WATCH`), then demand-driven decisive instrument

**Discovery** (no behavior change), one packet, two phases, outcome-routed seconds:

- **Exp1 — offline log mining, zero new code.** (a) OOM-vs-NULL ordering + last-N-ICALL context before the failure (frames, ordinals, anomalies); (b) **count provenance**: ICALL log cap/lossless status (`RECOMP_KERNEL_LOG_BUDGET` vs actual lines; ordinal continuity incl. the #5551/#5553 gap); tool-computed dispatch counts from a NAMED artifact — **neither 1177 nor 1909 is citable until then** (same claim, different numbers, neither names run+method); (c) **thread check** (`tid=` is already in every ICALL line — verify single-threaded or attribute); (d) **nr-baseline contrast** (the run WITHOUT the NULL call — establish comparability first, then what differs); (e) backward-edge exclusion for the faulting region (loop-back would make "passes" same-invocation iterations — doesn't disturb ordering, matters for frame modeling).
- **Exp2 — `RECOMP_KERNEL_WATCH=0x1C4064` run(s), existing gate, zero new code.** I verified the facility exists and fits exactly: samples the VA either side of every bridge call, names a bridge ordinal that changed it, distinguishes bridge-change from guest-side change by timing (`kernel_bridge.c:8921-8937,9026-9094`). Plus a **write-once first-0-transition latch** (new, off-by-default, NR pattern) as the authoritative transition record — the KWATCH series corroborates with a completeness check (call-# continuity + budget accounting).
- **Outcomes route to named seconds, not back to Advisor by default:** bridge-named change → toolkit install/relocation audit (static OK — the ordinal names the culprit); guest-side change → **page-guard write history** on the slot page from install (filtered to `0x1C4064`; install writes = positive control; every write with guest RIP/value/order; absence of zero-write = never-zeroed); no-transition-at-boundaries → read-path/displacement analysis (transient pair? tearing? live-displacement between samples?).

**Decisive evidence for this packet:** ordered slot-value trajectory with bridge/guest attribution + write-once transition (who/when/which-ordinal) + complete count provenance. That closes zeroed-vs-never-zeroed-vs-transient at triage level; writer RIP (if guest-side) is packet two's job.

### Q3 — Toolkit-seam approach CONFIRMED with two redirections; toolkit change proportionate

- **Seam verified:** `kernel_thunk_dispatch()` (`kernel_bridge.c:8944`) sees every bridge call with slot/ordinal/`esp`/ret and owns the call count ✓. Callback pattern verified: 20+ toolkit `recomp_diag_record` call sites + game impl (`src/diagnostics.c:35`), whose ring recycles (`count % capacity`) — so the Planner is right that a write-once record is needed ✓. Ring inadequacy, TOCTOU bounding ("bounded to the live slot at the bridge, fail closed"), and P4-bridge-before-inheritance obligations all confirmed as stated.
- **Redirection 1 (instrument order):** lead with existing `RECOMP_KERNEL_WATCH` (Q2 Exp2) before new seam work; page-guard is conditional on its outcome. Rationale: KWATCH decides toolkit-vs-guest-vs-neither with zero new code; a bridge-named change needs no page-guard at all.
- **Redirection 2 (mechanism, verified by reading the ICALL path — this closes a real alternative):** the logged `0` is the **raw slot read**, not a resolution artifact. `RECOMP_ICALL` checks `IS_CODE(_va)` (recomp_types.h:865) **before** any lookup/resolution (:869+), and `recomp_icall_not_code_log` logs that raw value then raises `0xE0424943 NONCONTINUABLE` (recomp_manual.c:35-43) unless the bypass env is set. So "what zeroed the slot" is mechanically the right question (the Halo-comment alternative — resolution yielding 0 — does not apply to this path), and the NULL ICALL **is** immediately terminal (the macro's skip-and-continue is dead code in strict runs). The Session's framing stands; no "unresolvable-but-nonzero" branch is needed.
- **Proportionality: toolkit change is proportionate.** (i) It is the only compliant route — generated-code edits are owner-forbidden for evidence-gathering, and offline analysis cannot see live slot values or writers. (ii) NR precedent: gated diagnostics + closure controls across multiple toolkit revs. (iii) The pin constrains production behavior, not off-by-default diagnostics. **Shape preflight REQUIRED** (invasive conditional instrument + critical-path reorientation = material + methodology). The draft's "OOM-activation slot" phrasing must be corrected to name the 1178th-call transition precisely — do not inherit OOM framing into the new line.

### Q4 — Downstream inventory: no §5.4(2) anywhere; three errata obligations, two explicit non-actions

- **No frozen contract premised OOM-fatality.** Verified: causal-slice criteria key on producer-chain evidence and explicitly inoculated ("must itself be tested"; Defect-B repair out of scope; "sequence is not proof of lacking guards"); producer-frame criteria key on frame/write-link; O-OPEN rows stand on missing-witness grounds. **No packet is reopened.**
- **Obligation 1 — causal-slice evidence (accepted): RESTORE + APPEND.** The Session replaced the original sentence with the correction block (original survives only inside the quote). That breaks review-byte provenance (§2.4.7). Repair: restore the original sentence verbatim in place, keep the correction as a dated appended erratum below it. Content stands; form must be fixed. No re-review (§3.2).
- **Obligation 2 — `a2h-mechanism.md`: dated correction section, original preserved.** Title + Defect B + mechanism steps 3–4 assert the false mechanism (and cite the bare sequence as evidence for "no check" — the circularity). Defect A (571MB commit characterization) STANDS (handling-independent); the baseline-variance paragraph must be reframed (variance is in the NULL event, not OOM handling).
- **Obligation 3 — operating history: dated UPDATE entry, never rewrite.** Its A2h text correctly framed an open question ("whether... or...") + temporal claims — no false assertion; it needs the answer appended (OOM handled; NULL separate pass; question now writer-identity).
- **Explicit non-actions:** `A4b2-r7`/`r8`, `A4b1-r4` (r8's packet has zero OOM/`E0424943` mentions — verified; r8's acceptance describes log ORDER, not causation; decision inputs pre-crash); P4 bridge (identity logic, untouched by mechanism); `pio-free-strict-horizon` "pre-OOM prefix" + r8 "OOM class" labels (horizon/sequence language, mechanism-free — still true).

### Q5 — Reasoning check: sound throughout, with four required follow-throughs

- **`jl`/signed and clean return: CORRECT.** `0xC0000017` bit31 set → taken; standard NTSTATUS idiom. "Handled" proven at one level.
- **Caller behavior: OPEN, correctly left open.** The function returns the error to `0x17C926`; what the caller does with it is unexamined. Bounded follow-up (read the caller) **only if** the NULL investigation suggests an error-path link — not now (it changes nothing about next-packet design; adding it now is scope creep).
- **Different passes: sound modulo two checks, both assigned to Exp1** — thread uniformity (`tid=` already logged) and backward-edge exclusion for the faulting region.
- **1177/1909: irreconcilable as stated, NEITHER citable.** Same definition, different numbers, neither names run+method (most likely different runs, but "most likely" is not evidence). Forbid both until Exp1's tool count lands. This is the seventh instance of the project's hand-count failure mode — hence binding for the next packet: **no hand counts in decision inputs; tool-computed quantities with positive controls and loss accounting.**
- **Co-occurrence≠causation, dump inadmissible (incl. resisting the apparent dump-`0`): correct**, noted as exemplary restraint under the project's own scarred rule.
- **Brief imprecision (mine to flag, yours to fix):** "RegionSize formula **at `0x0014980E**" matches no record (read site is `0x00149800`, add site `0x00149E24`) — presumably a typo for the former. Confirm-or-correct; cite per-PC henceforth.
- **The referral itself was correct** — no unilateral packet switch, critical-path question routed with evidence. That is exactly §4 working.

**BASIS:** observed — producer evidence whole (frame/caller/O-OPEN/NULL lead); causal-slice packet (inoculation lines :7,:16,:41) + evidence correction block (:481-528); mechanism doc title/Defect-B (:1,:71-96); dispatch (:8944-8974), patch loop (:9198-9254), KWATCH facility (:8915-9094), ICALL macro order (recomp_types.h:858-876), raise site (recomp_manual.c:35-43), diag ring recycle (diagnostics.c:35-49); r8 packet zero-mention + acceptance sequence-only text. Inferred — critical-path move (terminal-event rule + handled-OOM exclusion); KWATCH-first instrument order (zero-code triage dominates); V2-equivalent count discipline. Uncertain — 1177-vs-1909 provenance (hence neither citable); writer identity; OOM↔NULL relatedness (all Exp1/Exp2 scope, not assumptions).

**REVERSED BY:** Exp1 showing NULL precedes the OOM-handling return in one pass (contradicts terminality logic — implausible given NONCONTINUABLE raise, but stated); writer-ID showing the zero-write comes from the OOM error path itself (reactivates producer per Q1 conditions); non-reproduction of the 1177-successes→0 pattern (mechanism varies per run → line re-scoped).

**RECORD IN:** this ruling verbatim with handle/model/effort in a new A2h review record; Q4 errata obligations bind the Session (restore+append, dated corrections, no silent edits); Q2 packet shape + Q5 Exp1 checklist bind the Planner brief; producer line PARKED with Q1 reactivation conditions (not retired, no packet).

---

## Session actions taken on this ruling

| # | Obligation | Action |
|---|---|---|
| 1 | Q4 Obligation 1 — causal-slice evidence **restore + append** | **DONE** — original sentence restored verbatim; correction kept as a dated appended erratum |
| 2 | Q4 Obligation 2 — `a2h-mechanism.md` dated correction | **DONE** — dated correction section; original title and Defect B preserved; Defect A stands |
| 3 | Q4 Obligation 3 — operating history dated update | **DONE** — dated UPDATE entry appended, original text never rewritten |
| 4 | Q1 — park the producer line | **DONE** — plan records PARKED with both reactivation conditions |
| 5 | Q5 — 1177/1909 **neither citable** | **DONE** — both counts withdrawn from the evidence record; Exp1 must tool-compute them |
| 6 | Q5 — confirm-or-correct the `0x0014980E` citation | **DONE** — confirmed: `0x0014980E` **is** the producer store (`mov [ebp-0x24], eax`); the read is `0x00149800` and the add is `0x00149E24`. Cited per-PC from here on |
| 7 | Q2 — the next packet is a **NULL-slot triage discovery** | **IN PROGRESS** — the Planner's `a2h-live-oom-arg2-bridge` draft is **superseded** and must be re-briefed |
| 8 | Q3 — toolkit change proportionate, **shape preflight REQUIRED** | **DONE** — preflight obtained; **`SHAPE: REDIRECT`** with four binding corrections. Recorded below |

---

# Shape preflight (2026-09-27) — `SHAPE: REDIRECT`

**Turn:** `01a0e651-2195-7000-9152-373b3be1bffd`, handle `muse_FkNhGaXtV9P5`, `muse-spark-1.3-contributor`, `max`.
**Raised because:** the Planner's packet needs a toolkit change (the Exp2 latch), which the owner's toolkit
constraint makes preflight-mandatory. The Planner cannot reach the Muse handle, so the Session relayed the
sketch and returned the ruling.

```text
SHAPE: REDIRECT
REASON: Sketch direction right (discovery triage, KWATCH lead, named seconds), but multithreading (now
source-proven) forces per-thread attribution the sketch lacks; global latch + unattributed prints cannot
soundly support O-GUEST/O-NO-TRANSITION.
POLICY_ISSUE: NONE
REVERSED_BY: latch/tid-augmentation infeasible at the seam (re-refer for redesign); a strictly cheaper
decisive instrument identified; invalidation of the per-thread premise (source-verified at
kernel_bridge.c:316, so unlikely).
```

## Binding corrections (apply verbatim; no second preflight unless infeasible)

1. **Per-thread latch, not global.** Fixed **N** slots keyed by **live-read `tid`** (N from Exp1's tid census
   + stated margin), each **write-once** (CAS-taken, never overwrite/recycle); record =
   `(tid, thread-call#, ordinal, before, after, QPC ticks, intra/inter flag)`; **N+1th thread → explicit
   `UNKNOWN`.** Rationale: distinct threads can observe **distinct first-transition contexts** (intra- vs
   inter-bridge → **different rows**); first-arrival ≠ first-transition under races; **ticks order
   cross-thread.** N is a stated finite universe (§6.1.6); **overflow is fail-closed.**
2. **`tid` augmentation on `[KERNEL]`/`[KWATCH]` prints, same scope.** Verified: **no** kernel/dispatch/KWATCH
   print carries tid (zero `GetCurrentThreadId` in `kernel_bridge.c`), and the counter is `RECOMP_TLS`
   (`:316`, genuine `__declspec(thread)`/`__thread`, no reset path — **the decreases + 916 reused-different
   indices PROVE ≥2 threads**). **Unattributed multiplexed series cannot support per-thread windows or
   negatives.**
3. **Row keying + precedence.** Rows key on **`(tid, call#)`**; completeness = **per-thread index continuity +
   budget headroom**; precedence: any intra-bridge valid→0 → **`O-BRIDGE`**, else earliest-tick inter-bridge
   → **`O-GUEST`**, else **`O-NO-BOUNDARY-TRANSITION`**. **Latch-vs-series disagreement → `O-OPEN`** (fail
   closed).
4. **Two pre-specified runs, same build (AMENDS "at most one"):** **Run 1 all gates OFF (inertness control),
   Run 2 all ON (authoritative).** No third run without re-referral — *"this bounds fishing, not
   measurement."*

> ### ⚠ Authority correction: the "one bounded attempt" rule was the **Session's**, not the owner's
>
> The Planner **correctly refused** to adopt the two-run phasing, on the ground that *"direct human user
> expressly binds 'One bounded attempt maximum'"* and that an Advisor cannot override a direct-human
> restriction. **The refusal was right in form and the premise was wrong — and the error was the Session's.**
>
> **Provenance check:** `docs/agent-workflow.md` contains **no** run-count rule (its "bounded" uses concern
> *questions* and *read sets*). The frozen packet `a2h-named-producer-frame.md` contains **none**. The only
> "at most once" wording in the repository is in **`docs/packets/a2h-live-oom-arg2-bridge.md:22` — the
> Session's own Planner brief for a packet that was superseded and never promoted.**
>
> **So the constraint was the Session's drafting language, mis-attributed to the owner in the brief.** The
> Session has amended it: **the intent was anti-fishing — never run repeatedly until a desired row appears —
> and two pre-specified same-build runs with different gate states is a control, not fishing.**
>
> **Corrected constraint, now binding:** *at most **two** guest runs, same build, both pre-specified before
> the first — Run 1 gates OFF (inertness control), Run 2 gates ON (authoritative); no third run without
> Advisor re-referral; never rerun to chase a desired row.*
>
> **The Planner's proposed fallback was declined**, and the reason is recorded because it is the substantive
> point: it offered one run with *fixture-tested* inertness as the control, but **fixture-tested inertness is
> not run-time inertness** — a fixture cannot show that the toolkit change does not perturb the *real* guest,
> and a one-run design cannot distinguish "the slot was already zero" from "my instrument changed the
> timing." **If the two-run design proves infeasible, the packet fails closed rather than collapsing to one
> run and calling it controlled.**
>
> **This is the second time this window that the Session's own drafting was mistaken for an external rule.**
> The first was the stale draft-hash pin. Both are recorded: **a constraint's provenance must be checked
> before it is used to refuse an authorized instruction.**

## The split question — DECLINED as separate promotion, ABSORBED as phasing

The Session asked whether to run a **zero-new-code KWATCH-only** attempt first and promote the latch
separately. **Declined:** *"the latch is needed regardless (Q2), so a KWATCH-only first promotion saves ~30
lines at the cost of a full extra cycle + cross-build transfer burden on the critical path."* **Run 1 doubles
as the `O-BRIDGE` fast-path read AND the inertness control**; all rows still require latch confirmation from
**Run 2**, same build, zero marginal cost.

## Seam: **stands — redirect *within* it, not away from it**

`O-BRIDGE`-positive does **not** need the KWATCH deficiencies fixed (change prints are **self-bracketing**:
stack-local before/after around one call — sound unattributed). **`O-GUEST`, `O-NO-BOUNDARY-TRANSITION`,
completeness and first-transition ordering DO need the latch + tid prints.** So **the latch carries every row
as primary; KWATCH corroborates** (trend, existence, cross-check).

**Confirmed deficiencies** (the Planner's findings, independently confirmed): `KWATCH_ALL` is changed-only via
a racy global `seen` (`:9037-9044`); the bridge-change print carries **no index** (`:9089-9092`); and **no tid
anywhere.**

## Two methodological corrections to the Session

- **Count discipline:** *"1177/1908 are re-citable ONLY as (artifact, query, value) triples with query
  definitions — the '1909' hand count is already falsified by its own tool (1908), which is exactly why."*
- **Budget grounding:** *"'RECOMP_KERNEL_LOG_BUDGET absent from log' is methodologically unsound — env vars
  aren't log text. Ground budget from run metadata/recipe … and state it."* **The Session's log-text search was
  the wrong method**; the Planner's independent metadata read (`budget=100000`) is the correct grounding.

**Advisory precedent (citable, not standing policy):** kernel-dispatch indices are **per-thread**; cite
**`(tid,#N)`** or establish single-threaded context. **The printed "4274 total" is one thread's count — never
cite it as a total.**

## Session actions on this preflight

| # | Correction | Status |
|---|---|---|
| 1 | Per-thread latch + tid augmentation + `(tid, call#)` row keying + two-run phasing | **BOUND TO THE PLANNER** — relayed verbatim |
| 2 | Tool adopted as the packet's accounting instrument | **IN PROGRESS** — extending for metadata budget, both ICALL forms, log hashing, per-thread completeness |
| 3 | Count discipline `(artifact, query, value)` | **APPLIED** — tool output carries the artifact path and query definition |
| 4 | Budget grounded from metadata, not log text | **APPLIED** — tool reads `result.json`/metadata, not the log |
| 5 | P4 discovery-transfer re-establishment before inheritance | **RECORDED** in the packet's obligations |
| 6 | Session proposed replacing the latch with a log-line record; **Planner refused with citations** | **SESSION WRONG — WITHDRAWN.** See below |

---

## A Session error the Planner caught: "reliably archived" is not "lossless"

**What the Session proposed.** Arguing that `scripts/run-jsrf.py` sets
`os.environ['JSRF_LOG_PATH'] = str(run_dir / 'jsrf_run.log')`, the Session suggested the **guaranteed
archived log** could serve as the authoritative record, making the fixed per-thread CAS latch unnecessary and
dissolving the 128-slot overflow concern.

**The Planner refused, citing the rules verbatim.** The Session **verified both citations before conceding**:

- **`docs/agent-workflow.md:794-798` (§6.1.6):** *"**Decision inputs are lossless by construction.** A
  criterion may select a row only from a record that cannot drop the deciding event: **a write-once latch or
  an uncapped counter, updated at the event by the code that performs it.** Capped, sampled, rate-limited or
  first-N logs are **observation only**, and **no row may depend on the presence or absence of such a
  line.** Absence of a witness is never a positive attribution."*
- **`docs/jsrf-run-profiles.md:237-243`:** *"A decision input must also be **bounded by construction** …
  **an overflow counter is a bug detector** — if a record can overflow because the run was long or busy, the
  key is wrong."*

**The Session's error, stated precisely:** it conflated **"the log is reliably archived"** with **"the log is
lossless."** `JSRF_LOG_PATH` guarantees the file exists and receives what was **printed**; it says nothing
about whether every event **was** printed. With a 100000-line budget, concurrent stderr, and a
`RaiseException` that can terminate before a flush, **a line can be dropped — and a dropped line is exactly
the deciding event.** **The proposal would have built a decision row on a capped log, which is the specific
error §6.1.6 names.**

**Resolution:** the **fixed per-thread CAS latch stands as the authoritative record**; the existing `[ICALL]`
print gains **two gated fields** (live slot value, latest per-thread call #) as **corroboration and positive
control only**; and the packet states that **`KWATCH` is observation-only for the same reason** — sampled and
change-gated, so **no row may rest on the absence of a KWATCH sample.**

**This is the third Session reasoning error this window caught by measurement or review rather than by
self-check** (after the stale draft-hash pin, and the mis-attributed "one attempt" constraint). **The pattern
is consistent: the Session has repeatedly treated a document's or a tool's *existence and reliability* as
sufficient warrant for a *decision input*, when the rule requires the input to be lossless by construction.**
Recorded so the next packet's design starts from the rule rather than rediscovering it.
