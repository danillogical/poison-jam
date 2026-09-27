# `A4b2-r4` adequacy review — `INADEQUATE` (two independent operative blockers)

**Reviewer:** fresh Planner child `57fba91f-e96a-4562-beab-387599293e5f`, `codex/gpt-6-sol` @ `high` (§1,
§5.1.5(3)). Not the authoring child `b608eb13-a4d9-494d-87e9-20fb15937cf6`.
**Revision reviewed:** `docs/packets/a4b2-gp-clears-pending-word.md`, `A4b2-r4`, SHA-256
**`65BCB7F0C10C393205412C7F10D033604ED63BC982AC23C6967DC8F162F2D250`** (134 lines, LF), hash recomputed
by the reviewer.
**Recorded verbatim** from the reviewer's own response, as §2.2.10 requires. Written to disk **before**
any revision work began.

---

## Verdict block (verbatim)

```text
REVISION:          A4b2-r4; SHA-256 65BCB7F0C10C393205412C7F10D033604ED63BC982AC23C6967DC8F162F2D250 (recomputed; 134 LF lines).
READ:              docs/agent-workflow.md:239–356,499–824; plan-jsrf-bare-minimum.md:1–152; docs/jsrf-run-profiles.md:1–81,137–179,208–243; docs/packets/a4b2-gp-clears-pending-word.md:1–134; docs/reviews/a4b-gpin-accounting-ruling.md:1–101; docs/reviews/a4b-watch-ledger-ruling.md:1–106; docs/reviews/a4b1-a4b2-r3-adequacy-review.md:1–77; docs/reviews/a4b2-r4-planning-rulings.md:1–122; docs/reviews/a4b1-r4-acport-step4-enumeration.md:104–176; docs/reviews/a4b1-r4-stage2-acceptance-review.md:1–85; docs/reviews/a4b1-r4-implementation-verification.md:1–110; docs/reviews/a4b2-r4-session-mechanical-verification.md:1–149 (lead only); toolkit src/apu/apu_watch.c:150–219,330–501,508–689,690–797,800–925,930–1032; toolkit src/apu/apu_watch.h:35–119; toolkit src/apu/dsp/gp_ep.c:80–174,235–259,344–423,585–693; toolkit src/apu/dsp/dsp.c:48–95; toolkit src/apu/dsp/dsp_dma.c:307–359; toolkit src/apu/dsp/interp/dsp_cpu.c:896–935, dsp_cpu_regs.h:114–123; toolkit src/apu/apu_core.c:628–683, apu_regs.h:317–339; src/recomp/gen/recomp_0000.c:135272–135291,135395–135413,135861–135880; src/recomp/gen/recomp_0005.c:6514–6533,6738–6757,8081–8095; logs/runs/20260924-191833-331-a4a-r2-trap-trace/{metadata.json:1–110,result.json:1–14,jsrf_run.log:3180–3294}; logs/runs/20260926-010303-411-a4b1-default/{metadata.json:1–108,result.json:1–14,stacks.txt:20845–20846}. Read-only checks: current SHA and 134 LF lines, XBE hash FD190557…F9C, game HEAD a000662/toolkit HEAD 3a3c7c1, git diff of code from archived baseline empty, pinned EP grep 14 permitted hits, generated +0x810 pattern 4 hits.
PREMISE_FRESHNESS: BOUNDED — observed archived STRICT A4a trap startup and accepted default pending spin, source/XBE/toolkit identities unchanged; no current R1 measurement, but P1–P3/G1–G4 fail closed on stale identity or missing evidence. No PREMISE_CHANGED against binding Advisor rulings.
BLOCKING:          (1) AC-BOOT lines 74–76 and R2-PASS lines 17,127: it validates only the *first* emitted GPBOOT image I and merely counts boots>=1; the source gp_ep.c:354–399 can bootstrap/emit again on a later reset, and apu_watch.c:677–685 resets frame/insn counters. If first bootstrap loads I but a second loads different program before GP_CLEAR, first block, counters, GP_CLEAR and input qualifiers can all PASS while the clearing GP did not execute I. Required: bind verified image/bootstrap instance to the GP_CLEAR instance, or fail closed on intervening second bootstrap/reload. (2) AC-CLEAR lines 87–90 and R2-PASS line 127: historical successful GP_CLEAR plus seq ordering alone is called satisfaction of the title's pending-word *wait*, with Wf/F explicitly corroboration-only. In the actual record-before-store mechanism (apu_watch.c:735–753; packet step 1 line 36), CPU can pause after its anchor hook while a stale 3 is exchanged by GP, then resume storing 3 and remain spinning (Wf=3,F>=1); GP_CLEAR and all other ACs can PASS though that guest wait was never satisfied by the exchange. Required: fail closed when final watched state/spin contradicts success, and establish that the anchored store actually preceded/explains the GP exchange (or narrow the R2-PASS claim to what CAS alone proves); do not treat pre-store seq as a causal store-completion witness.
DEFERRED:          AC-NOCPU line 102's six selected XBE ranges establish six selected instrumented sites, not a complete population of all computed writes; the packet expressly limits census and attributes by CAS, so no blocking every-access claim. The packet's default-path zero GP* line rests also on structural untrapped launch, not solely absence from a capped log.
DECISIONS:         Respect five-offset PERIPH classification and admitted first at_clear block with both can-fail checks; binding Advisor ruling and directly inspected source support them; reverse only on changed source/premise or a failing in-run check. Count six instrumented call sites, not all possible computed stores; scope explicitly limited and CAS supplies attribution; reverse if claim or row relies on complete dynamic access census. Do not promote; two false-PASS scenarios affect operative contract, and no ruling may convert missing causal evidence to PASS; reverse after a reviewed revision excludes both scenarios.
VERDICT:           INADEQUATE
```

`VERDICT` is `INADEQUATE` because `BLOCKING` is not `NONE` (§5.3). `PREMISE_FRESHNESS: BOUNDED` would not
by itself have prevented `ADEQUATE`; the blockers do.

## Final closing block (verbatim) — the reviewer's authoritative final response

The reviewer's closing message restates the same verdict with tightened wording and a shortened
`DEFERRED`. Recorded because it is the reviewer's final response and because it states the two required
outcomes most precisely. **Verdict unchanged: `INADEQUATE`, same two blockers.**

```text
REVISION:          `A4b2-r4`; SHA-256 `65BCB7F0C10C393205412C7F10D033604ED63BC982AC23C6967DC8F162F2D250` (independently verified; 134 lines)
READ:              `docs/packets/a4b2-gp-clears-pending-word.md:1–134`; `docs/agent-workflow.md:239–356,499–824`; `docs/jsrf-run-profiles.md:1–81,137–179,208–243`; `plan-jsrf-bare-minimum.md:1–152`; `docs/reviews/a4b-gpin-accounting-ruling.md:1–101`; `docs/reviews/a4b-watch-ledger-ruling.md:1–106`; `docs/reviews/a4b1-a4b2-r3-adequacy-review.md:1–77`; `docs/reviews/a4b2-r4-planning-rulings.md:1–122`; `docs/reviews/a4b1-r4-acport-step4-enumeration.md:104–176`; `docs/reviews/a4b1-r4-stage2-acceptance-review.md:1–85`; `docs/reviews/a4b1-r4-implementation-verification.md:1–110`; `docs/reviews/a4b2-r4-session-mechanical-verification.md:1–149` (lead only); toolkit `src/apu/apu_watch.c:150–219,330–501,508–797,800–925,930–1032`, `src/apu/apu_watch.h:35–119`, `src/apu/dsp/gp_ep.c:80–174,235–259,344–423,585–693`, `src/apu/dsp/dsp.c:48–95`, `src/apu/dsp/dsp_dma.c:307–359`, `src/apu/dsp/interp/dsp_cpu.c:896–935`, `src/apu/dsp/interp/dsp_cpu_regs.h:114–123`, `src/apu/apu_core.c:628–683`, `src/apu/apu_regs.h:317–339`; game `src/recomp/gen/recomp_0000.c:135272–135291,135395–135413,135861–135880`, `src/recomp/gen/recomp_0005.c:6514–6533,6738–6757,8081–8095`; archived `20260924-191833-331-a4a-r2-trap-trace/{metadata.json:1–110,result.json:1–14,jsrf_run.log:3180–3294}` and `20260926-010303-411-a4b1-default/{metadata.json:1–108,result.json:1–14,stacks.txt:20845–20846}`. Read-only checks independently confirmed the XBE hash, game/toolkit identities, four generated `+0x810` text hits, and 14 permitted P3 grep hits.
PREMISE_FRESHNESS: BOUNDED — the archived strict trap startup and default pending-word stop exist, and the XBE and toolkit identities match; a current trapped-run result remains unmeasured. The packet’s identity and evidence gates fail closed on that uncertainty.
BLOCKING:          (1) `AC-BOOT`, lines 74–76; the criterion checks only the **first** `[GPBOOT]` image. A first bootstrap can load image `I`, followed by another reset/bootstrap loading different instructions before `GP_CLEAR`; the first block and the later retirement/clear witnesses can all pass, falsely attributing that clear to execution of `I`. The pinned `gp_ep.c:354–399` permits another bootstrap and emits another block. **Required:** bind the verified image to the bootstrap governing the clear, or fail closed on an intervening bootstrap.
                   (2) `AC-CLEAR`, lines 87–90, and `R2-PASS`, line 127; a successful historical exchange is treated as satisfaction of the guest’s pending-word wait while final `Wf` and `F` are corroboration only. The CPU anchor hook runs **before** its store (`apu_watch.c:735–753`). If the CPU pauses after that hook, the GP exchanges a pre-existing `3`, and the CPU then stores its anchored `3` and remains spinning, all specified PASS predicates can hold with `Wf=3` and `F≥1`. **Required:** fail closed when final word/spin contradicts wait satisfaction, and establish the anchored store’s causal ordering—or narrow the PASS claim to what the exchange actually proves. Pre-store sequence order alone does not establish it.
DEFERRED:          The six XBE ranges enumerate the selected instrumentation sites, not every computed write. The packet expressly limits that census and bases GP-write attribution on the compare-exchange, so this is not an additional blocker.
DECISIONS:         Retain the five-offset PERIPH classification, the `0xFFFFB3` stub classification, and conditional admission of the first `at_clear` block; they follow binding rulings and inspected source; reverse on changed source or failed in-run checks.
                   Do not promote this revision; the two operative false-PASS scenarios require revised criteria, not an adequacy ruling that substitutes for missing evidence; reverse when a reviewed revision excludes both.
VERDICT:           INADEQUATE
```

## The reviewer's intermediate findings (verbatim, in order)

The reviewer reported its findings as it confirmed them, before the formal block. Recorded because they
are part of its response and because blocker (2) was reached by two routes.

1. **First report — AC-CLEAR / `R2-PASS` final-state contradiction.**
   > "Review ongoing; found potential blocking AC-CLEAR/R2-PASS: AC-CLEAR makes final Wf/F corroboration only, so an early successful GP_CLEAR followed by a later guest re-anchor of 3 leaves final Wf=3,F>=1 (wait still spinning), yet AC-CLEAR and R2-PASS can hold. Checking source and other criteria now; no promotion pending verdict."

2. **Second report — AC-BOOT bootstrap-instance gap.**
   > "Confirmed stronger blocking scenario in packet AC-BOOT: line 74/76 checks only FIRST [GPBOOT] PRAM image and counts boots>=1; toolkit gp_ep.c:354-399 invokes dsp_bootstrap and emits [GPBOOT] on every reset transition, watch.c:677-685 resets run counters on each bootstrap. A second GPRST boot loading non-I code before GP_CLEAR can leave first block valid and all ACs PASS, falsely claiming I executed/cleared. Need bind PRAM/image to boot in force at clearing exchange or fail closed on second boot."

3. **Third report — the record-before-store causal gap.**
   > "Further confirmed AC-CLEAR false attribution risk: apu_watch.c:735-753 records CPU_ANCHOR BEFORE the actual guest store (packet step 1 explicitly call before store). Watch-ledger ruling lines 35-45,86 acknowledges seq only approximate. With CPU preempted after anchor hook, an existing/other writer's 3 can be exchanged by GP (GP_CLEAR.seq > anchor.seq), then CPU resumes writing 3 and remains at loc_001A18D0; AC-CLEAR line 89 PASS ignores final Wf=3,F>=1 (corroboration only), and R2-PASS can be selected despite this command's wait never clearing. Recommend blocking repair binding anchor to actual exchanged 3 or requiring stable final clearing plus the command's wait exit, not treating seq as causality."

4. **Final direction.**
   > "Final direction: INADEQUATE; two independent operative blockers. (1) AC-BOOT first GPBOOT only can validate a different bootstrap from GP execution that actually clears; toolkit proc_rst_write allows repeated bootstrap and resets GP counters. (2) AC-CLEAR PASS on historical GP_CLEAR CAS, Wf/F corroboration-only and no guest wait-exit witness: a GP zero exchange can be followed by CPU re-store 3, leaving Wf=3/F>=1, yet all ACs can PASS and R2-PASS claims the pending-word wait was satisfied. Premise freshness BOUNDED (current XBE/toolkit/default historical stop verified, no current trap R1 yet; gates fail closed). Full formal block forthcoming."

---

## Session independent verification of both blockers

§2.4.2 makes the reviewer's report a **lead**. Both blockers would send the packet back for revision, so
the Session reproduced the load-bearing mechanisms directly at toolkit `3a3c7c1` before routing the
repair. **All of the following are Session observations** (files read at the pinned commit).

### Blocker (1) — repeated bootstrap is real, and re-arms the run counters

| Claim | Measurement | Result |
|---|---|---|
| `proc_rst_write` bootstraps on **every** qualifying transition | `gp_ep.c:354-401` — the `else if` at `:358-360` fires on any `(not-both-bits → both-bits)` change of `val`/`oldval`, not only the first; it calls `dsp_bootstrap` (`:362`) and reaches the `[GPBOOT]` emission (`:398`) each time | **CONFIRMED** |
| A re-bootstrap resets the run counters | `apu_watch.c:677-685` `apu_watch_gp_bootstrap`: `s.boots++`, then `s.gp_frames = 0; s.gp_insns = 0;` | **CONFIRMED** |
| The bootstrap emission is not gated to the first boot | `gp_ep.c:373` `apu_watch_gp_bootstrap(1)` and `:394-399` run on each entry; only the 512-word PRAM read is trace-gated (`:394`) | **CONFIRMED** |
| `s.gpin` / `s.at_clear` are **not** cleared by a re-bootstrap | only `apu_watch_reset` clears them (`apu_watch.c:1059`, `:1083-1084` `memset(&s.gpin,…)`, `memset(&s.at_clear,…)`) | **CONFIRMED** — the write-once `at_clear` freeze survives across bootstraps |
| `boots` is printed on every counts line | `apu_watch.c:821-846` `emit_counts`, with `boots=%u` at `:827`; emitted after each bootstrap (`:699`) and at `gp_frames==1` or `%256==0` (`:710-713`) | **CONFIRMED** |

**Consequence.** The packet's `AC-BOOT` PASS requires only the **first** `[GPBOOT]` header plus
`boots≥1` (line 76), and `AC-RUN` PASS accepts **some** counts line with `boots≥1, gp_frames≥1,
gp_insns>0` (line 82). A second bootstrap loads a different program, resets `gp_frames`/`gp_insns`, and
re-arms both criteria. The first block stays valid, so `AC-BOOT` still PASSes, and the clear can be
performed by a program that is not image `I`. **Blocker (1) stands as reported.**

### Blocker (2) — the anchor is recorded before the store lands

| Claim | Measurement | Result |
|---|---|---|
| `apu_watch_cpu_store` records before the guest store | `apu_watch.c:735-756`: `seq = InterlockedIncrement(&s.seq)` (`:746`), then the anchor branch latches `CPU_ANCHOR` (`:748-755`). Packet step 1 (line 36) inserts the call **immediately before** each store | **CONFIRMED** |
| The watched word is read live on each call | `watch_w_va` (`:43-50`) reads `*(volatile uint32_t *)(g_apu_ram_ptr + APU_WATCH_BASE_VA) + APU_WATCH_WORD_OFF` on every invocation | **CONFIRMED** |
| The ruling already concedes the ordering is only approximate | `a4b-watch-ledger-ruling.md:38` — *"A CPU record is written before its store, so CPU seq order is only approximate."* And `:44` — *"The anchor is corroboration of which 3 was exchanged; CAS observed=3 already proves the word held 3."* | **CONFIRMED** |
| The packet makes `Wf`/`F` corroboration only | packet line 89: *"Record `Wf` and `F` as corroboration only."*; `R2-PASS` line 127 claims the wait was satisfied | **CONFIRMED** |

**Consequence.** `GP_CLEAR` proves a successful `3→0` CAS at `W_va`. It does **not** prove that the
anchored store caused that `3`, nor that the guest's wait ended. The packet's claim is *"satisfied the
pending-word wait"* (line 17), and its `AC-CLEAR` PASS condition can hold while the guest is still
spinning at `loc_001A18D0` with `Wf=3`. **Blocker (2) stands as reported**, and it is a claim-versus-
evidence gap: the criterion faithfully implements the watch-ledger ruling's PASS condition
(`a4b-watch-ledger-ruling.md:42-44`), but that condition does not establish the claim this packet makes.

### One measured fact the repair will need, with its boundary

Re-entry to the spin is plausible and matters for choosing the repair, so the Session measured what it
could:

- The spin is `recomp_0005.c:6748-6751` — `loc_001A18D0: cmp MEM32(ebx),0` / `jne loc_001A18D0` — and the
  store of `3` is `:6746 MEM32(ebx) = eax;` immediately before it. **Observed.**
- The enclosing function is `sub_001A1769` (definition `:6547`). Its only call site is
  `recomp_0005.c:7046`, inside `sub_001A19D6`. **Observed.**
- `git grep` for a backward `goto loc_001A1A45` / `goto loc_001A1A5x` finds **nothing**, so the call site
  is not re-entered by a local loop inside `sub_001A19D6`. **Observed.**
- **Not measured:** whether `sub_001A19D6` or its callers are invoked repeatedly (for example once per
  frame), which would re-enter the store-then-spin sequence legitimately. **This is the boundary of the
  measurement and the Planner must treat it as open.**

That boundary matters: if the guest legitimately re-enters and re-stores `3`, then a blanket "final
`Wf=0`" requirement would be a **false-FAIL** requirement, and the repair must distinguish a legitimate
re-entry from the stale-`3` race. The Session does not decide this; it is packet design.

---

## Consequential obligations

1. **`A4b2-r4` is not promoted.** It remains a draft; `CURRENT PACKET` still names `A4b1-r4` only, and
   `A4b2` has no promoted revision. No implementation is authorized.
2. **The revision repairs both blockers** (§5.4 After-INADEQUATE). It may fix cheap operative advisories
   but adds **no narrative about the repairs** (§5.4, §5.7).
3. **§5.5 is in play for `AC-BOOT`.** It has now carried a blocking defect in **two consecutive**
   adequacy verdicts: r3's `A4b2` B1 (`sge0` undefined and unasserted, per
   `a4b1-a4b2-r3-adequacy-review.md:32-41`) and this revision's blocker (1) (bootstrap instance not bound
   to the clearing exchange). Under §5.5 the Planner must **redesign** that criterion — change what it
   measures, split it, or delete it — or take the methodology to the Advisor; a third patch of the same
   shape is not allowed. **`AC-CLEAR` is not a repeat:** the r3 review recorded it as *"unchanged and
   sound"* (`:75`), so blocker (2) is its first blocking finding.
4. **Blocker (2) implicates a recorded Advisor ruling's sufficiency, not its premise.** The packet
   implements `a4b-watch-ledger-ruling.md:42-44` faithfully. If the repair needs to change what
   `AC-CLEAR` may conclude, or to narrow the claim, that is a methodology/claim-scope question the
   Planner may take to the Advisor (§4.1 `Planner -> Advisor`).
5. **Re-review scope** (§5.4): the blocking defects, the changed regions, and a contract-completeness
   check. The re-review must be by a Planner that did not write the revision.
6. **Deferred advisories stay deferred.** The reviewer's two `DEFERRED` items are recorded above and are
   not obligations.
