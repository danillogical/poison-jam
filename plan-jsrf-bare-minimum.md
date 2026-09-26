# Jet Set Radio Future: Windows port milestone plan

This file is the **execution authority**. It owns the current packet, current
blocker, packet status, next action, and milestone acceptance. Do not discover work
by scanning historical documents or old status tables. `docs/agent-workflow.md` owns
roles, the packet lifecycle and escalation; `AGENTS.md` owns operating/build/runtime
discipline; `docs/jsrf-run-profiles.md` owns evidence-profile semantics.

## CURRENT PACKET — `A4s-r6` (toolkit sync) — **ACCEPTED 2026-09-25, `R-SAME`, PUSHED**

> **RESULT: the merge succeeded, the strict stop is UNCHANGED, and `M` is ACCEPTED and PUSHED.**
> Every gate passed. Toolkit `main` is at **`M` = `3f8bf67c450861aefcbc376698750bc1446bc9dd`** (parents
> `0d7929c`, `766ecef`), clean, and now **published to the owner's fork**.
>
> - **Acceptance:** **stage-1 `ACCEPT`**, `BLOCKING: NONE`, **all ten mandatory criteria `AGREED`**
>   (`docs/reviews/a4s-r6-acceptance-review-stage1.md`; reviewer child
>   `f6ba7864-07ae-4388-9583-9a265e63c935`, `workbuddy-ai/hy4-preview-f` @ `high`, route verified live).
>   Per §2.2 a first-stage `ACCEPT` is **final** and is not passed to a second stage. State: **accepted**.
> - **Closure push — DONE** (owner policy, all five checks): `main` → `origin`, fast-forward
>   `766ecef..3f8bf67`. Verified: `git rev-parse origin/main` = `M` and `git ls-remote origin
>   refs/heads/main` = `M`. **No force, no `upstream` push** (`upstream/main` still `766ecef`, untouched).
>   Tracking moved to `origin/main` (recorded, not reverted). Log:
>   `docs/reviews/owner-push-policy-xboxrecomp-fork.md`.
>
> **The reviewer independently reproduced more than the Session measured** — worth keeping: positive
> controls for `AC-MERGE` (b)/(c) (**143** and **28** excess on the two parents, so those checks *can*
> fail); a positive control for the `|= MCPX_AC97_CODEC_READY` grep (**1** on `766ecef`, so the zero at
> `M` is meaningful); **link falsification** for `AC-BUILD` (all **147** sources in the run's
> `build-source.json` hash-match the `M` worktree); an independent conflict-set guard via
> `git merge-tree --write-tree`; and **AST-based** verification of hunks 5/6/9 instead of text
> comparison.
>
> **One documentation error it caught was CORRECTED:** `docs/reviews/a4s-r6-ac-merge.md` had named the
> hunk-8 function `test_seeds_align_16`, which does not exist; the hunk sits in
> `test_seeds_drops_unaligned_targets` (L96), where HA-LOCAL is correct. A record error, not a resolution
> defect.
>
> **Deferred advisories (recorded, not reopening the packet):** step-4 evidence-table labelling (raw
> preview **15** vs resolved-all-ours **3** should be labelled separately); the `AC-STRUCT`
> duplicates-not-omissions limit; the pre-existing set-G failure; the 13-byte PE-timestamp exe delta; and
> the reviewer's own disclosed instrument errors.
>
> **Two claim limits and one gated lead carry forward** — see the block at the end of this section.
>
> - **Step 1** — recovery point discharged as **verify-and-reuse** of the pre-existing `a4s-pre-sync`
>   (per **Q-B**); nothing created, deleted, or force-moved.
> - **Step 2** — controls: build OK; `ctest` 12/12; K4 27 tests OK; KX 33 modules / Ran sum 133 with zero
>   per-module differences; set G 11 files / 10 pass / 1 fail (carve-out by identity).
> - **Step 3** — the **9-hunk guard passed exactly** (5 files, 9 hunks, every section and line span
>   matching); all 9 hunks resolved **by exact text** per the pre-ruled table.
> - **Step 4 `AC-STRUCT` PASS** (0 findings, 0 UNKNOWN; all controls green) — and it **caught a real
>   defect in the Session's own first resolution**: edit (b) implemented as a blanket text delete left
>   switch 4 with a duplicate `case 138` and **switch 5 with none at all**, silently dropping ordinal-138
>   dispatch, which the build would never have caught (`docs/reviews/a4s-r6-ac-struct-catch.md`). The
>   merge was aborted, re-run deterministically, and the rule corrected to *keep the first occurrence per
>   switch* — the **operative reading of edit (b)**, confirmed by the Advisor.
> - **Step 5 build PASS** (relinked; no structural diagnostics, so the `R-BUILD` backstop never arose).
> - **Step 6 `AC-TEST`** — C **12/12** incl. the named witness `jsrf_inplace_event_bridge`; G same
>   failing identity as step 2; K4 30 tests OK; KX 56 modules with **0 regressions, 0 lost coverage**.
> - **`AC-MERGE` PASS** (16/16 targeted checks; the `(d-twin)` deletion witness took four attempts and
>   all three failures were mine — *"credited"* is load-bearing, `docs/reviews/a4s-r6-ac-merge.md`).
> - **`AC-KEEP` (iv)/(v)/(vi) PASS** — three model ranges byte-identical (anchor counts 1/1/1, working
>   can-fail controls); deleted names **0** in `SCOPE`; all four controls match **2/2/0/0**; and the
>   **(vi) `[A3A]` witness on the run: `gc=0x00000002 gs=0x00000100`** — GC bit1 and GS bit8 both set,
>   matching the reference control exactly.
> - **`AC-INV` PASS** — new env names exactly `RECOMP_APU_MIXDOWN_ALL` + `RECOMP_USB_PORT`, none removed.
> - **`AC-GEN` PASS**, **`AC-NOPUSH` PASS** (`ahead 25`, remotes exact, no push/fetch during execution).
> - **`M` conforms to Appendix A verbatim** — HA-COMBINED-1/-2 and Expected-5/6/9 byte-for-byte, plus
>   10/10 structural checks (`docs/reviews/a4s-r6-appendix-a-conformance.md`).
> - **Step 7 — the one strict run:** `logs/runs/20260925-210315-113-a4s-sync-strict`. **V holds**
>   (STRICT; dump `matches: 1, content-mismatch: 0`); **`outcome = diagnostic_deadline`**;
>   **`B = 0x803C0000`**; **`W = 3`**; **`F = 2`** (identical to the `A4a-r2` R0 control). **No rerun.**
> - **Row `R-SAME`:** V holds; `outcome = diagnostic_deadline`; `B` usable and `= 0x803C0000`; `W = 3`;
>   `F ≥ 1`. All six gate rows and `R-INVALID`/`R-MOVED` were evaluated first and are unmatched. **The
>   stop is unchanged — still the DSP pending-word spin** at `loc_001A18D0` (`recomp_0005.c`), pending
>   word `MEM32(0x803C0810) = 3`.
>
> **Next packet:** Planner revises **`A4b1` → `A4b1-r2`** (`PREMISE_CHANGED`, §5.4(2)) — new baseline =
> toolkit **`M`**, exe
> `E45026C3DF5AACAF3D66FCC1E17D9C6C1A12247864D58D0911CDBA8435A2A3C7`, this run as the reference R0, and
> upstream's `src/apu/apu_dsp.c`/`CMakeLists.txt` as the starting state; then `A4b2`. **`A4b1`/`A4b2` are
> no longer parked behind `A4s`** — `A4s` now has a durable final disposition.
>
> **Closure push (owner policy) — after ACCEPT only.** `R-SAME` leaves `main` at `M`, so
> `git push -u origin main` is permitted once acceptance returns `ACCEPT` and the five pre-push checks
> hold. **The acceptance reviewer must be shown the Q-C claim limit explicitly**; acceptance with that
> recorded limit satisfies the policy's "active packet's tests/acceptance passed" — not all 56 KX modules
> passing.
>
> **CLAIM LIMIT (Q-C) — must appear in the acceptance review.** *KX does not exercise 7 pytest-dependent
> upstream modules under `unittest`* (`test_block_dispatch`, `test_incdec_carry`, `test_incdec_result`,
> `test_lifter_double_shift`, `test_lifter_result_clobber`, `test_sar_width`, `test_x87_classification`).
> *The lifter/translator conflict resolutions (hunks 5–7 and 9) are therefore witnessed only by K4 and the
> existing KX modules, not by upstream's own tests of those paths.* Harmless **for this packet only**
> because `AC-GEN` holds — no regeneration, so `M`'s lifter produced none of the linked code. The 7 are
> **not a FAIL**: per Q-C a module whose only error is an absent third-party import ran no test, and all
> four fail-closed conditions were verified. **No `pytest` was installed and none was borrowed.**
>
> **GATED LEAD (binding on later planning).** Before **any** packet regenerates or relifts with the
> toolkit at `M` or later, all **56+** KX modules — including these 7 — must run under **real `pytest`**
> in an **owner-authorized** environment; parametrized and fixture tests included.
>
> **`AC-STRUCT` claim limit (Q-C).** It **detects duplicates, not omissions** — the switch-5 damage was
> invisible to it, and is excluded only by the explicit post-condition (one `case 138` per switch). For
> any future revision, **each HA edit should carry an expected-count post-condition enforced as an
> `AC-MERGE` check**.

- **Packet:** `docs/packets/a4s-toolkit-sync.md`, revision **`A4s-r6`**, class **change**, frozen
  SHA-256 **`75207C41B8E9964D3E1467F5E980D1B8DE4CB75A1E0AFE954F59D40FA27D86E3`** (**374 lines**).
  **This is the packet that was executed.** It replaced `A4s-r5` at the canonical path **byte-identically
  to the reviewed revision**; `A4s-r5` remains recoverable from git
  (`git show HEAD:docs/packets/a4s-toolkit-sync.md`, blob `f918b998c20a5be2bcc832fbde527a5754634183`).
- **Status: EXECUTED (see the result block above).** Promotion was **byte-identical with no revision**,
  exactly as §5.3 requires: `ADEQUATE` ends plan iteration, so the packet was **not** edited — the stale
  "358 lines" note in the packet and its tool record (the scanner is really 355 lines) is a **recorded
  advisory only** and is used in **no** predicate. Editing it would have changed the SHA and invalidated
  the review.
- **Adequacy:** **`VERDICT: ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED` — recorded
  **verbatim** with the reviewer's child ID and route in `docs/reviews/a4s-r6-adequacy-review.md`
  (fresh Planner child `95429607-3d77-44a9-8f38-47e978ece759`, `workbuddy-ai/kimi-k3`, effort omitted).
  It independently re-verified both pinned tool hashes and found the hunk-1/2 C text **byte-identical**
  to the Advisor's ruling, the 9-hunk table complete, and H2 to be the Advisor's **edit-application**
  version rather than the rejected precondition.
- **The review's one persistence risk is CLOSED** (`DEFERRED 1`). The reviewer asked the Session to
  confirm `logs/a4s/conflicted-*` survive until the `AC-STRUCT` positive control runs. Rather than merely
  confirming existence — they are in **gitignored** `logs/`, so nothing guarantees them — the Session
  tested whether the risk is real, and **it is not**:
  - the fidelity source is **durable in git**: the raw preview **`75083476` is a git tree object**,
    recoverable with `git show 75083476:<path>`;
  - the saved files are **not** byte-identical to the preview blobs, and the reason matters: the saved
    files are **diff3** (3 `|||||||` base markers) while `git merge-tree --write-tree` emits **2-way**
    markers (0 base markers) — the same 2-way/diff3 distinction that caused an earlier Session error;
  - **it does not change the control**: resolving **both** sources to all-ours gives **byte-identical
    content for all five conflicted files**, and both give **exactly 3 findings, 0 UNKNOWN** over `SCOPE`
    (`dup-case case 138 [8019,8322]`, `dup-case case 138 [8481,8847]`, `dup-def bridge_KeResetEvent
    [1377,6713]`) — precisely the 2 + 1 the criterion expects.
  - **A Session error found and corrected while closing it:** the first attempt resolved **all five**
    conflicted files, including three under `tools/recomp/` that are **outside `SCOPE`** (toolkit tooling,
    not build inputs of `jsrf_recomp.exe`). Feeding Python to a C-oriented scanner produced **79 spurious
    `dup-def` findings and one UNKNOWN** in `lifter.py`. The corrected control scans only the two in-scope
    files. Recorded because it is exactly the mistake the `SCOPE` guard exists to prevent.
- **Readiness gates the Session has already verified:** both pinned gating tools match their pins —
  `scripts/check-merge-structure.py` = `A1FDCE26755DAB3A0B6AA6E23D380A8F95E097439C2BE91D6318BF4402AA1AFF`
  and `tests/test_merge_structure.py` = `A42F4A1E2C473892DC28A0F5B21923F4BC0AEFCFFE859C7FC20006F49DB0744C`
  — and the scanner's four controls reproduce (`0d7929c` 87/109/**0**, `766ecef` 87/111/**0**,
  `75083476` raw 90/111/**15**, worktree 87/109/**0**; fixtures **14/14 OK**).
- **Set-G baseline at this revision: 11 files, 10 pass / 1 fail** (the one failure is `E2 =
  `tests/test_ac2_provenance.py`, pre-existing and pristine-HEAD-reproduced). It has moved **three times
  this session** (8/2 → 9/1 → 10/1), which is why the packet requires the carve-out to be **re-derived
  from each execution's own step-2 run by test identity, never carried forward**.
- **Next action: Planner revises `A4b1` → `A4b1-r2`** (`PREMISE_CHANGED`, §5.4(2)), then `A4b2`.
  `A4s` now has a durable final disposition, so **`A4b1`/`A4b2` are no longer parked behind it**.

### `A4s-r6` execution — **COMPLETE**, steps 1–8 all run

The full narrative is in `docs/reviews/a4s-r6-execution-evidence.md`; the summary is in the result block
at the top of this `CURRENT PACKET` section. Points worth keeping here:

- **P0 passed under the Advisor's interpretation rulings** — **Q-A** (a pin on a tracked file identifies
  the **committed blob**; P0.10 passed by **branch (a)**, the literal working-tree hash, which equals the
  pin) and **Q-B** (proceed, discharging P0.7/step 1 as **verify-and-reuse** of the pre-existing
  `a4s-pre-sync` under four fail-closed conditions, all of which held). **Neither required a packet
  revision** (§5.4 interpretation rulings), and **no `.gitattributes` was added** (owner/packet-scope
  work, per the Advisor).
- **`AC-STRUCT` earned its place.** It caught a real defect in the **Session's own** first resolution —
  a blanket text delete of `case 138` that left switch 4 with a duplicate (compile error) and **switch 5
  with none at all**, silently dropping ordinal-138 dispatch. **The build would never have caught the
  second one.** Direct evidence for the Advisor's ruling that `AC-STRUCT` must run **before** the build.
- **The `AC-MERGE` `(d-twin)` deletion witness took four attempts and all three failures were mine** —
  the word *"credited"* is load-bearing. Recorded in full, including the two false alarms caused by
  over-broad file-wide text searches (`docs/reviews/a4s-r6-ac-merge.md`,
  `docs/reviews/a4s-r6-appendix-a-conformance.md`).
- **A recurring Session failure mode, recorded because it repeated:** every verification failure in this
  execution was an **over-broad text match** — a file-wide search used where the criterion was
  function-scoped, or a substring test matching a longer identifier. **No defect in `M` was ever found
  by these.** The lesson is that each criterion's scope must be implemented literally, and the targeted
  per-region checks are what make the results meaningful.
- **The KX/`pytest` question was resolved by the Advisor as `Q-C`: NOT `R-TEST`.** The 7 modules are
  *"not exercised: new at M, dependency absent (pytest)"* — **not a FAIL and not a PASS witness** — under
  four fail-closed conditions, **all verified**. **No `pytest` was installed, and none was borrowed**
  from the unrelated venv the Advisor identified. The claim limit and the gated lead are in the result
  block above and **must be shown to the acceptance reviewer**.

**Current blocker / next action.** **`A4s` is COMPLETE** — `A4s-r6` is executed, **accepted** (`ACCEPT`,
all ten criteria `AGREED`), and **pushed** to the owner's fork; the toolkit has a durable final
disposition at `M`. **The current work is the Planner's revision of `A4b1` → `A4b1-r2`**
(`PREMISE_CHANGED`, §5.4(2)), then `A4b2`.

**`A4b1-r2` — IN FLIGHT (2026-09-25).** The Session completed the premise reconnaissance and froze the
brief; the Kimi K3 Planner is writing the packet.

- **Frozen brief:** `docs/reviews/a4b1-r2-planning-brief.md` (the task, the verified baseline, the
  ordered reading list, the measured premise delta, the four premise answers, the pytest lead, and the
  mandatory sketch → Opus shape-preflight workflow).
- **Premise re-check:** `docs/reviews/a4b1-r2-premise-recheck.md`. **A4s changed exactly two of the files
  `A4b1` touches** — `src/apu/apu_dsp.c` (+56/−3) and `src/apu/CMakeLists.txt` (+7/−1);
  `apu_core.c`, `apu_state.h`, `apu.h` and `apu_mmio_hook.c` are **byte-identical**. So the APU surface
  is almost untouched, which is why this is a premise re-check and not a redesign.
  - **One premise changed in form, in `A4b1`'s favour:** `bridge_MmGetPhysicalAddress` no longer returns
    the VA itself; it delegates to `xbox_MmGetPhysicalAddress`, and the *deleted* comment had warned the
    delegate *"would return a native pointer"*. **Read directly, it returns no native pointer** — the VA
    unchanged outside the contiguous arena, `va − XBOX_CONTIG_BASE` inside it. The address premise
    **holds and is better founded**, because A4s removed a real inconsistency where the bridge translated
    and the delegate did not.
  - **The inverse is already documented in-tree** (`docs/reviews/a4b1-r2-inverse-precision.md`):
    `XBOX_CONTIG_BASE = 0x80000000` = the contiguous window, and `xbox_memory_layout.c:843-845` states the
    round trip — *"The contiguous window IS the physical-address view, so OR-ing its base is the
    documented round trip, not a guess"* → `physical P → XBOX_CONTIG_BASE | (P & 0x0FFFFFFF)`. So Device
    semantics 3 needs a **citable wording correction, not an invention**.
  - **The `& 0x03FFFFFF` hazard is real and pre-existing:** 8 live sites, including `apu_shim.h:101-123`
    and `apu_vp.c:846` — both **byte-identical** across the baseline move. Note the masks differ
    (`0x03FFFFFF` = 26 bits vs the round trip's `0x0FFFFFFF` = 28 bits), which is exactly the distinction
    Device semantics 3 exists to prevent.
  - **The `MIXDOWN_ALL` question is RESOLVED by source read** — it was the plan's flagged *uncertain and
    load-bearing* item. `monitor.frame_buf` is `int16_t[256][2]` at `apu_state.h:500` inside the **host**
    `MCPXAPUState` struct, its instances are host allocations, and there are **zero** guest-mapping
    references to it. So the default-on path writes only host monitor storage: **not new device
    behaviour, no new criterion.** It remains a `jsrf-run-profiles.md` classification item.
- **Everything else holds:** P-B, P-C, P-G (byte-identical), P-E, P-D's gate, and the strict stop. All
  `A4a` R0/R1 observations, the watch-ledger ruling, the Q1/Q2 rulings, the checkpoint-40 constraints
  and the `[GPIN]` redesign remain **admissible and un-reopened** — a baseline change alone does not
  reopen a technical ruling.

The new baseline for that revision: toolkit **`M`** = `3f8bf67c450861aefcbc376698750bc1446bc9dd`; exe
`E45026C3DF5AACAF3D66FCC1E17D9C6C1A12247864D58D0911CDBA8435A2A3C7`; the run
`logs/runs/20260925-210315-113-a4s-sync-strict` as the reference R0.

**Two claim limits and one gated lead carry forward into that planning:**

1. **KX does not exercise 7 pytest-dependent upstream modules under `unittest`** — `test_block_dispatch`,
   `test_incdec_carry`, `test_incdec_result`, `test_lifter_double_shift`, `test_lifter_result_clobber`,
   `test_sar_width`, `test_x87_classification`. *The lifter/translator conflict resolutions (hunks 5–7 and
   9) are therefore witnessed only by K4 and the existing KX modules, not by upstream's own tests of those
   paths.* Harmless for `A4s-r6` only because `AC-GEN` held (no regeneration). **`A4b1-r2` does not
   regenerate either** (regeneration is already one of its non-goals), so this lead stays **gated and
   unexercised** — no `pytest` work is manufactured to clear it early.
2. **`AC-STRUCT` detects duplicates, not omissions** — it did not see that one switch had lost `case 138`
   entirely; that class is excluded only by an explicit expected-count post-condition. **For any future
   revision, each HA edit should carry such a post-condition, enforced as an `AC-MERGE` check.**
3. **GATED LEAD (binding):** before **any** packet regenerates or relifts with the toolkit at `M` or
   later, all **56+** KX modules — including these 7 — must run under **real `pytest`** in an
   **owner-authorized** environment; parametrized and fixture tests included. **Do not install, borrow, or
   otherwise introduce `pytest` without owner authorization.**

### Predecessor — `A4s-r5` — **EXECUTED 2026-09-25, selected `R-CONFLICT`** (superseded by `A4s-r6`)

- **Packet (superseded):** `docs/packets/a4s-toolkit-sync.md`, revision **`A4s-r5`**, class **change**,
  frozen SHA-256 **`09DA9413C028D61BD28D9E4007AF6DDE3474ED6869F77B0095D03DB2C4FB86FB`** (263 lines).
  Recoverable from git; no longer the file at that path.
- **Status: executed. Selected row `R-CONFLICT`** (fail-closed, as predicted). **Toolkit `main` is left
  at `0d7929c`** (rolled back; working tree clean). **No push was performed at execution** — `main` at
  `0d7929c` is not a descendant of `origin/main`, so per the Closure rule: **"no push: main at
  `0d7929c`"**. (The accepted **baseline** was later published to the fork's `jsrf/integration` branch
  under the owner's standing push policy — see the push log above; `main` itself is still unpushed.)
- **Execution evidence:** `docs/reviews/a4s-execution-evidence.md`. **Conflict inventory:**
  `docs/reviews/a4s-r5-conflict-inventory.md` + raw hunk text in `logs/a4s/conflict-hunk-inventory.txt`.
- **Result:** the merge attempt produced **5 conflicted files, all inside the packet's 10-file set**
  (`kernel_bridge.c`, `xbox_memory_layout.c`, `lifter.py`, `test_icall_feedback.py`, `translator.py`)
  and **9 conflict hunks: 1 × HA, 1 × H1, 2 × H2, 5 × UNDECIDED**. Five UNDECIDED hunks select
  `R-CONFLICT`; the packet does not authorize choosing a side by judgment for them. `AC-MERGE`,
  `AC-KEEP`, `AC-INV`, `AC-BUILD`, `AC-TEST` and the strict run were **not reached** (row 2 precedes
  them); their read-only pre-checks are recorded as non-binding supporting evidence.
- **Pre-merge controls (step 2, all on `0d7929c`):** build OK; pre-merge exe
  `9597FF7C2A377265ABA8DBB90B461EBE763E02D65432E9DFA13ACD925539C553` (matches the recorded
  expectation); `ctest` **12/12**; set K4 **27 tests OK**; set KX **33 modules, Ran sum = 133**;
  set G **8 pass / 2 fail**, both failures **measured pre-existing** (see the `E1`/`E2` follow-ups
  below — `E1` now passes, so the current set-G baseline is **9 pass / 1 fail**).
- **Rollback rebuild exe SHA-256:** `AEC1F0FF7FB944DA44487EA15C3E94FF342751D4168E699F544A58895CF083C3`
  — **differs** from step 2; the packet says record, not gate, so no row turns on it. Cause **measured**:
  `/Zi` + `/DEBUG:FULL` with no `/Brepro`, so a **relink** stamps fresh PE timestamps. Step 2's build
  found the tree already up to date and **did not relink** (which is why it matched the archived
  `9597FF7C…`); the merge attempt and `merge --abort` then rewrote **58 toolkit files** (mtimes only —
  `git status` clean, `git diff HEAD` empty), forcing the rollback build to relink. A **no-op build was
  measured not to relink**. The two exes are identical in size, layout and all but **13 bytes** (four
  PE timestamp fields plus one stamp byte).
  Analysis: `docs/reviews/a4s-r5-rollback-exe-hash.md`. *(An earlier note here called the rebuild
  "deterministic"; that inference was wrong and is retracted — see that record.)*
- **Adequacy:** **`ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED`, `DEFERRED: NONE`
  — `docs/reviews/a4s-r5-adequacy-review.md`, fresh Planner child `f339b306-0473-4777-a211-78271453f2b9`.
- **Baseline recorded at execution:** game **`c1cdb91`** (the commit that adds the promotion;
  the plan previously named `b7d6af5`, which is the preceding startup-receipt commit — advisory,
  not revised), toolkit `main` **`0d7929c`**, `upstream/main` = `origin/main` = `v0.11.0^{commit}` =
  `766ecef`, merge base `051a128`, no `a4s-*` branch existed before step 1 (`a4s-pre-sync` now exists
  at `0d7929c` and still resolves to it).
- **Revision log:** `docs/reviews/a4s-revision-history.md` (non-authoritative).

**Push log — first durable-checkpoint push (owner-authorized, 2026-09-25).**

```text
PUSHED_TO:  https://github.com/danillogical/xboxrecomp.git   (remote `origin` = the owner's fork)
BRANCH:     jsrf/integration   (NEW branch; `main` cannot fast-forward yet)
COMMIT:     0d7929c86771dd0b971941592fd4f15436116e82
REMOTE_URL: https://github.com/danillogical/xboxrecomp.git
RESULT:     SUCCESS — verified independently with `git ls-remote origin`, which reports
            0d7929c…  refs/heads/jsrf/integration and 766ecef…  refs/heads/main (unchanged)
```

The 24 commits published are the **accepted toolkit baseline** (`A4a-r2`, `A3a-r25`, `A4p-r1` all
accepted on `0d7929c`; baseline tests re-measured this session: ctest 12/12, K4 27 OK, KX 133 OK). The
`A4s-r5` `R-CONFLICT` attempt produced **no commit**, so none of its output is in this push. **No force,
no `upstream` push, `main` untouched.** `main` stays at `766ecef` because it cannot be fast-forwarded
until the `A4s` merge lands `766ecef` in local `main`'s history; the owner's branch-handling rule covers
exactly this case. Full record and all five pre-push checks:
`docs/reviews/owner-push-policy-xboxrecomp-fork.md`.

**`A4s-r6` packet delivered; adequacy review completed and superseded by execution.** (This paragraph and
the ones that follow are the **planning-phase record**; the outcome is in the `CURRENT PACKET` result
block at the top of this file.) `docs/packets/a4s-r6-toolkit-sync.md`,
revision **`A4s-r6`**, SHA-256 **`75207C41B8E9964D3E1467F5E980D1B8DE4CB75A1E0AFE954F59D40FA27D86E3`**,
**374 lines** (the Planner's reported "325" was a non-empty-line miscount; the project convention is
total lines — `A4s-r5` is cited as 263, its LF count). The frozen `A4s-r5` is **untouched**
(`09DA9413…86FB`). A **fresh Kimi Planner** performed the binding §5.3 adequacy review of that exact
SHA, returning **`ADEQUATE`** (`BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED`) — recorded in
`docs/reviews/a4s-r6-adequacy-review.md`. Per §5.3, `ADEQUATE` requires only `BLOCKING = NONE` and
`PREMISE_FRESHNESS` not `FAIL`; on `ADEQUATE` the revision was frozen and promoted in the same step,
with no polishing pass.

**Current blocker / next action.** The Advisor's hunk rulings are **recorded and binding**
(`docs/reviews/a4s-r6-advisor-hunk-ruling.md`). The frozen `A4s-r6` planning brief was dispatched to the
Planner (`workbuddy-ai/kimi-k3`, effort omitted per the owner's instruction). The Planner wrote its
**sketch** (`docs/packets/a4s-r6-toolkit-sync.md`, 14 lines, ≤20 tool calls) and the Session relayed it
for the **mandatory shape preflight**. The Advisor returned **`SHAPE: PROCEED`** with three bounded
policy items to write into the packet:

1. **Gating tools must be tracked and pinned.** `AC-STRUCT`'s scanner, and any verifier the packet
   relies on, must live at a **tracked** path (e.g. the game repo's `scripts/`) pinned by SHA-256 —
   **not** in `logs/a4s/`, which is gitignored and which `AGENTS.md` forbids for durable helper source.
   Its controls must be packet criteria (positive on the merge preview; zero on both parents; UNKNOWN
   for an unparseable merge-changed file).
2. **The pre-ruled table does not exempt run-profiles rules 1–5.** Keep `AC-INV` and the `SCOPE` guard
   exactly as they are; and if the real merge produces a hunk outside the recorded 9, **or** a recorded
   hunk whose base/ours/theirs text differs from `logs/a4s/conflict-hunk-inventory.txt`, the result is
   **`R-CONFLICT`** — never resolved by analogy.
3. **"Expected `R-SAME`" is a forecast only** — no criterion may assume it. `R-PUSH` follows the owner
   push policy: all five pre-push checks, and **never** after `R-CONFLICT`, a rollback, or `INADEQUATE`.

The Advisor's stated **REVERSED_BY** conditions, which return the packet to sketch: the real merge's
conflict set differing from the 9 recorded hunks; the verifier showing edit-application H2 changes any
recorded classification other than hunk 5's; or the scanner failing its positive/negative control on the
pinned trees.

**Session support already delivered to the Planner** (`docs/reviews/a4s-r6-ac-struct-controls.md`):
measured control numbers for the scanner over `SCOPE` — `0d7929c` and `766ecef` both **0 findings**;
the raw preview **15** (12 marker lines + 3 structural); the resolved all-ours preview **3 structural,
0 markers**; resolved all-theirs **2** (resolving hunk 1 to upstream removes the duplicate
`bridge_KeResetEvent` that lives inside it). Two wording traps are recorded there: the criterion must
say **which tree** each number belongs to, and must name the **resolution** the control uses.

**The Advisor's rulings, in one line each** (full text and BASIS in the ruling record):

1. **Hunks 1–2 — COMBINED form**, fixed precedence: (1) in-place KEVENT if `guest_va_is_inplace_kevent`
   accepts, (2) `ke_shadow_lookup` handle if non-NULL, (3) `XBOX_TO_NATIVE` fallback. **Upstream's
   `bridge_resolve_handle` tier is NOT carried over** (for a nonzero VA it returns the token itself,
   which would pass a guest VA as a host HANDLE and make tier 3 unreachable). The ruling supplies the
   **exact C text** for all three functions; **exactly one** `bridge_KeResetEvent` survives, with the
   same three tiers. Two further edits are pre-ruled HA: delete upstream's duplicate clean-hunk
   `bridge_KeResetEvent`, and keep **one** `case 138` per switch (local's positions), located **by exact
   text, not line number**.
   - *Why not upstream alone:* the accepted, gating ctest `jsrf_inplace_event_bridge` asserts guest
     `SignalState` and previous-state values that only local's bodies write.
   - *Why not local alone:* upstream's timer model arrives in a **clean hunk** and registers timers only
     in the shadow table; ordinals 113 and 159 are both declared, so a wait on a timer would never see
     the event that fires it.
   - *Why in-place first:* `ke_shadow_remove` has **zero callers**, so shadow entries are never retired;
     and every reachable shadow populator writes type 8/9, which the sniffer rejects — so in-place-first
     never takes an object away from the shadow tier. It also defuses the `Size` hazard without deciding
     the `Size` question.
2. **Hunk 5 — per-line combined form** (derived by rule): `_FLAGS_UNDEFINED` loses `"lock xadd"`, keeps
   upstream's `"popfd"` and its comment; `_EFLAGS_SETTERS` keeps local's line; hunk 6's H2 result stands.
3. **Hunk 8 — LOCAL** (`"--functions", fns,`), pinned.
4. **Hunk 9 — full union**, local's members then upstream's, upstream's `_RESULT_SNAPSHOT_SETTERS` clause
   kept.
5. **H1 — accepted as proposed**: containment must include deletions.
   **H2 — the Advisor REJECTED the Session's added delete-vs-keep precondition** and kept the original
   one; H2's **action** must be defined as **EDIT APPLICATION** (apply each side's per-line edit to the
   base, then insert each side's insertions, local first), **not** a union of line presence. Ambiguous
   attribution → UNDECIDED. The Session **verified the Advisor's prediction exactly**: hunk 5 changes
   from UNDECIDED to H2 applies, every other hunk keeps its classification, and edit application
   produces the ruled `_FLAGS_UNDEFINED` text.
6. **`AC-MERGE`(d) gains a twin witness:** every base line a credited side deleted is absent from the
   result unless the other side replaced it.
7. **Post-resolution structural check required** (conflict markers; duplicate `case` values per switch
   per branch path; duplicate file-scope definitions per branch path), on the **resolved** tree
   **before** the build, over `SCOPE`, with positive and negative controls, selecting **`R-CONFLICT`,
   never `R-BUILD`**, plus a C2084/C2196/C2371 backstop. The Advisor ruled **against** a broader semantic
   review as unbounded. **H3 is not audited** (no corpus) — the packet must say so.

**Follow-up leads the Advisor recorded (NOT `A4s-r6` obligations):** the correct `Size` convention
(4 vs 16/40 — needs a primary source); tier 3's handle-typed treatment of a non-handle Ke* object
(pre-existing); `ke_shadow_remove` having no callers; the upstream timer model's observable behaviour
for JSRF; an indirect-thunk-call search behind "not declared ⇒ unreachable"; case 207's argument count
(local 36 vs upstream 40 — the merge silently took upstream's 40). Prior leads carry forward:
`MIXDOWN_ALL`/`USB_PORT` classification, E2, the function-style KX modules, and the exe-relink advisory.

**Staffing state — Planner route RESOLVED (owner instruction, 2026-09-25).** The new
`docs/agent-workflow.md` §1 names **Kimi K3** for the Planner with a `max` effort qualifier.
`workbuddy-ai/kimi-k3` resolves live as **exactly one** canonical match but advertises **no selectable
reasoning efforts**; the owner ruled directly: *"kimi k3 doesn't take an effort, so don't worry about
effort for kimi k3"*. Under §4.1 an owner instruction outranks every role's ruling, so **the row is
satisfied by invoking `workbuddy-ai/kimi-k3` with `reasoning_effort` omitted**, and **no Planner
blocker is in force**. `docs/agent-workflow.md` §1 was **not** edited (§3.4). Recorded in
`docs/reviews/startup-20260925-session-58e86358.md` and
`docs/reviews/a4s-r5-execution-startup-ruling.md`.

**Execution rulings (binding, recorded).** `docs/reviews/a4s-r5-execution-startup-ruling.md`:
**Q1** Planner effort (superseded by the owner instruction above); **Q2** P0.8 recorded as a literal
**FAIL** and execution proceeded under a **process exception** (the only dirty path is the owner's
`docs/agent-workflow.md`, a non-build, non-evidence file outside every path-scoped check) — the file was
**not** committed or normalized; **Q3** the `AC-INV` "new environment names" command cannot run as
written on PowerShell 5.1 (embedded `"` is stripped), so an **interpretation ruling** authorized one
substitute form with three controls (42 / 44 / empty-and-exit-1), all of which passed;
**Q4** two pre-existing set-G failures (`E1` `test_agent_docs.py`, `E2` `test_ac2_provenance.py`) are
**named exceptions** — measured byte-identically on a pristine HEAD extraction, hence not
merge-relevant — and the KX carve-out was **not** extended to set G wholesale. `AC-TEST` was not
reached, so Q4 is prospective.

**Follow-ups (recorded, not authorized work):**

- **E1 — RESOLVED as a side effect of this closure; recorded as `PREMISE_CHANGED`.** Four
  current-tense mentions of a since-retired route name tripped `scripts/check-agent-docs.py`'s
  `RETIRED_NAMES` check. They sat in the superseded staffing block that this closure rewrite replaced,
  so they are gone and `tests/test_agent_docs.py` now **passes** (`Ran 30 tests, OK`); the checker
  reports **0 findings**. No test, script, packet or frozen artifact was edited to achieve this.
  **Any re-run of `A4s-r5` must re-measure the set-G baseline** — it is now **9 pass / 1 fail**, not the
  8/2 measured at step 2. Full record: `docs/reviews/a4s-execution-evidence.md`, "Premise change to
  `Q4`'s E1 exception".
- **E2** — `tests/test_ac2_provenance.py` fails on `A0` and `P1` with clause-D reason "the classifier
  was NOT edited; step 3 requires line 391 to be corrected". Cause unconfirmed; diagnose before anyone
  relies on the AC2 provenance tool.
- **KX claim limit** — ≥16 function-style toolkit test modules are not exercised by `unittest`, before
  or after the merge; run them under their own runner (they have `__main__` blocks) or convert them.
- **CRLF working-tree drift** — the merge/abort rewrote 4 toolkit files to CRLF (`core.autocrlf = true`).
  `git status` reports no change and the CRLF→LF bytes hash exactly to the archived `A4a-r2` reference;
  recorded so a future raw-hash comparison is not misread as a regression.

**ROUTE STATE — RESOLVED (2026-09-25, `session-58e86358`); the senior-judgment routes are live.**
The previous state (both senior routes BLOCKED; the substitute unresolvable) is **superseded**, as is
the temporary owner-authorized staffing exception recorded in
`docs/reviews/startup-20260925-session-910703eb.md` — the owner replaced `docs/agent-workflow.md`, and
its §1 roster is now the only persisted staffing policy. This session resolved every §1 DSH row live:
**Claude Opus 5.5 @ `high`** (`claude/claude-opus-5-5`) spawned and passed the two-turn continuity and
read-yourself probe; **Kimi K3** (`workbuddy-ai/kimi-k3`) resolved and answered (effort omitted per the
owner instruction); first-stage reviewer `workbuddy-ai/hy4-preview-f` @ `high` and second-stage
`workbuddy-ai/deepseek-v4.1-flash` @ `max` both answered their probes. **No route was substituted.**
Receipt: `docs/reviews/startup-20260925-session-58e86358.md`.

**Preserved binding rulings.** The `A4s` AC'97 hunk rulings (`docs/reviews/a4s-ac97-hunk-ruling.md`,
including "Interpretation ruling 2: scope") and the `A4b` rulings remain **binding** and were
**preserved, not reopened** — the packet *cites* them rather than restating them, and the model/provider
change does not reopen a ruling.

**Superseded route state (historical).** The earlier `ROUTE STATE` and temporary-staffing blocks
recorded in this plan and in `docs/reviews/startup-20260925-session-910703eb.md` are **superseded**.
They described a since-replaced staffing arrangement under the previous workflow revision, including a
now-retired route that ran out of quota during the `A4s-r5` review. The owner has since replaced
`docs/agent-workflow.md`, whose §1 roster is the only persisted staffing policy. Prior route-failure
diagnoses are retained as provenance only:
`docs/reviews/route-failure-20260924-workbuddy-substitute.md`,
`docs/reviews/route-failure-20260924-claude-pool.md`. **`docs/agent-workflow.md` §1 was not edited**
by this session — §1 is an owner-reserved decision (§3.4).

**Planner research-sufficiency stopping rule (unchanged):** the Planner stops investigating as soon as
§5.1's planning test is met and leaves remaining unknowns to the packet.

**Queued in order:** (1) **DONE** — the `A4s-r5` re-review returned `ADEQUATE`, the revision was
promoted, and it has now been **executed** (row `R-CONFLICT`, above); (2) **`A4s-r6`** — the Planner
revision that resolves the five UNDECIDED hunks from the inventory above, with hunks 1, 2, 5, 8 and 9
going to the Advisor first and hunk 5's `H1` rule gap clarified; (3) the `A4b1-r4`/`A4b2-r4` revision
per the binding `[GPIN]` redesign. The existing Advisor rulings are **binding** and are not reopened by
a provider or staffing change.

**Revision history of this packet** (each round's blocker was in the same criterion, so the
Session ruled the last fix a **simplification** under §5.5, not a patch):

| Rev | Verdict | Blocker |
|---|---|---|
| `r1` | INADEQUATE | a false `R-MOVED` on a shifted `B` |
| `r2` | **ADEQUATE** | — (but the fork premise then changed) |
| `r3` | INADEQUATE | the new greps matched a **comment** and an **unbuilt scaffold** → guaranteed false FAIL |
| `r4` | INADEQUATE | `AC-KEEP` (iv)'s **end anchors were not unique** (`        }` ×37, `            }` ×20) → false FAIL on a preserved merge |
| `r5` | **ADEQUATE** | — promoted, then **executed** → `R-CONFLICT` (B1-r4 closed: unique-first-line anchor + fixed length 7/56/25 + byte compare; anchors 1/1/1) |

**The sync itself:** merge **`upstream/main`** (**v0.11.0**, `766ecef`) into local `main`,
rebuild **without regenerating `src/recomp/gen`**, run both repositories' tests, rerun **one
strict baseline**, and push nothing during execution. **If the strict stop moves, that is the
next brief.** Verified facts and the conflict surface: `docs/reviews/toolkit-sync-instruction.md`.

**The AC'97 hunk is pre-ruled** (`HA`): it resolves to the **LOCAL** `A3a-r25` model; upstream's
NABM trap enters **unarmed** with zero call sites. The Advisor's **"Interpretation ruling 2:
scope"** fixes the check's scope: `SCOPE` = the toolkit's **`src/` and `include/`**, deleted
names match only as **quoted string literals**, and comments/docs/tests/unbuilt scaffolds are
**inventoried, never failed**. See `docs/reviews/a4s-ac97-hunk-ruling.md`. In the executed attempt this
was the **only** pre-ruled hunk and it was classified `HA`; the five UNDECIDED hunks are the blocker.

**Push policy (owner) — superseded and broadened 2026-09-25.** The previous wording was *"push toolkit
`main` to `origin` when a packet closes"*. The owner has now made **durable-checkpoint pushes the
standing practice** for toolkit work: push after an accepted toolkit implementation, a completed
sync/merge packet, a materially useful commit later packets depend on, or a clean milestone boundary —
not only at the end of the project. Full text, the verified remote table, and the pre-push checklist:
**`docs/reviews/owner-push-policy-xboxrecomp-fork.md`**; operating summary in `AGENTS.md` "Toolkit
remotes".

**Remotes verified this session with `git remote -v` (names were not assumed):** the owner's fork is
**`origin`** → `https://github.com/danillogical/xboxrecomp.git` (fetch **and** push); `upstream` →
`https://github.com/sp00nznet/xboxrecomp.git` (fetch; push `DISABLED`). **The fork is already
configured, so no remote was added, renamed, or altered, and `upstream` was not touched.** The
instruction's suggested name `fork` was deliberately **not** used — a second name for the same URL is a
way to push to the wrong destination by accident. The game repository has no remote.

**A measured constraint that decides how a future push works.** `origin/main` is `766ecef` (*"Release
v0.11.0"*, i.e. upstream's release line) and is **not** an ancestor of local `main`; the two have
diverged **169 upstream-only / 24 local-only**. So `git push origin main` is currently **non-fast-forward
and would be rejected**. It becomes a valid fast-forward only once the `A4s` merge lands `766ecef` in
local `main`'s history — which is exactly what `A4s` is for, and what the packet's Closure clause
already assumes. Nothing is manufactured to force it; the workflow decides when a merge is valid.

**No push occurred in this execution** — `main` is at `0d7929c`, which is not a descendant of
`origin/main`, and `R-CONFLICT` is a **no-push state** under the new policy as well.

**Follow-up — two new upstream variables need a `jsrf-run-profiles.md` classification.** The sync brings
in two environment names that did not exist locally (Advisor-observed at the merge tree `75083476`, and
**independently re-measured by this session** with the authorized `-f` pattern-file form: 42 unique
names at `0d7929c`, 44 at `75083476`, difference exactly these two, none removed):
**`RECOMP_APU_MIXDOWN_ALL`** (`apu_dsp.c:91`, **default ON**, sums all 32 mixbins into the host monitor
buffer) and **`RECOMP_USB_PORT`** (`ohci.c:819`, selects the port the virtual pad appears on). Neither is a
classifier-listed or deleted name, so neither fails the merge; they are recorded as **"new unclassified
variable"** and need a classification edit. **Uncertain and load-bearing:** whether `MIXDOWN_ALL`'s
default-on path writes anything the **guest reads back** — the Advisor did not verify that
`monitor.frame_buf` is guest-invisible. **If it is guest-visible, it is new default-on device behaviour**,
and `A4b1` (which rewrites `apu_dsp.c`) must classify it before any strict APU claim.

**Candidate future blocker — the RR wait (Advisor lead, not a packet).** JSRF contains
upstream's AC'97 "Reset Registers" pattern: `0x1A6F6F mov byte [eax+0xFEC0010B],2`, then
`0x1A6F7F mov cl,[eax+0xFEC0010B]` / `and cl,2` / `0x1A6F88 test cl,cl` / `jne 0x1A6F88` — a
hoisted single read that spins on the RR bit self-clearing. The same RR write occurs at
`0x1A7406`. **Uncertain whether current runs reach it**; the A3a/A4a runs stop at the DSP spin
first, which is consistent with not reaching it. If a later run stalls at `0x1A6F88`, the RR
self-clear becomes its own admission packet (the trap then becomes a precondition of `A4b`
rather than a follow-up). It is **not** a reason to arm the trap in `A4s`.

**Then:** `A4b1`/`A4b2` (drafted, split by a fresh Planner from `A4b`) resume with
re-established baselines. Their in-flight adequacy verdicts remain useful as design
feedback. `A4b1` = port, licence, fixtures, unchanged default path, no guest-run claim,
toolkit-only writes; `A4b2` = the strict trap+trace run (boot, run, clear, no-CPU, inputs),
game-only writes, with preconditions P1 (`A4p-r1` ACCEPTED `O-GATE`) and P2 (`A4b1`
ACCEPTED).

## Draft packets — `A4b1`/`A4b2` (at r3; **the route is now live and `A4s` is complete, so the r4 revision is unblocked**)

- **`A4b1`:** `docs/packets/a4b1-gp-core-port.md`, `A4b1-r3`, SHA-256
  `321ABCF7C9319B5AC4661B384A21CA2D2CE39C792D9820C9C322B7979EC7AFDA` (373 lines). Claim: the
  pinned xemu GP core, GP MMIO routing and address-translated GP DMA are in tree with per-file
  provenance; synthetic ack removed; licence recorded; build+ctest green including a fixture;
  one strict default run matches A4a R0. No guest GP-behaviour claim; toolkit-only writes.
  **Its dependency on `A4s` is now SATISFIED** — `A4s-r6` is accepted and pushed, toolkit at `M`
  = `3f8bf67c450861aefcbc376698750bc1446bc9dd`. The revision to **`A4b1-r2`** (or `-r4`) takes `M` as
  its baseline, per the `CURRENT PACKET` next-action block above.
- **`A4b2`:** `docs/packets/a4b2-gp-clears-pending-word.md`, `A4b2-r3`, SHA-256
  `CFB8C0EBFBC9BE3CFB62677C4C756694DDE9663542E239570B639DED9F5BFCE9` (310 lines). Claim: in one
  strict run (`RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`) the `loc_001A18D0` wait was satisfied by
  modelled GP execution of the guest's own command. Preconditions P1 (`A4p` `O-GATE`) and P2
  (`A4b1` ACCEPTED). Game-only writes.
- **The ledger (ruling 1) is sound and unchanged.** Device semantics 6 is a **write-once
  watched-word ledger** (atomic `seq`, uncapped counters, per-class latches — `GP_CLEAR`,
  `GP_ZERO_OVER_ZERO`, `GP_ZERO_OVER_OTHER`, `GP_NONZERO_OVER`, `GP_PARTIAL`; CPU
  `ANCHOR`/`ZERO`/`ZERO_OVERFLOW`/`OTHER` via an exported `apu_watch_cpu_store`); Device
  semantics 7 is trace-only. Three independent reviews confirmed it faithful to the ruling and
  closed all seven r1 blocking defects.
- **The input accounting (`[GPIN]`) is being redesigned (ruling 3, §5.5 third trigger).** Three
  consecutive blocking verdicts: a lossy once-per-key log decided `AC-INPUTS`; the fix was a
  256-entry table; and that table's key universe is **1024 words** for MIXBUF alone
  (`GP_DSP_MIXBUF_BASE 0x001400` + `DSP_MIXBUFFER_SIZE 1024`, Session-verified), so one frame's
  mixbin sweep overflows it → false `R2-UNKNOWN`. The redesign keys **provenance classes over
  statically enumerated finite universes** (MIXBUF 32 bins, PERIPH 128, FIFO 6, DMA region
  class 4), with counters that **cannot overflow by construction**, a write-once `at_clear`
  freeze at `GP_CLEAR`, and `GPIN_OUT_OF_UNIVERSE` as a **bug detector**. See
  `docs/reviews/a4b-gpin-accounting-ruling.md`.
- **Split decision (Planner, one line):** split, because the port's scale risk is settled by
  build+ctest+one default run and should not wait for, or be reviewed with, the run criteria.
  The Advisor confirmed **do not merge the packets** — the leak was a decision rule placed in a
  packet that cannot change the code producing its input, not the split itself.
- **`AC-PIO` and `R-PIO-DATA` are deleted** from both — `A4p`'s `O-GATE` discharged Q1
  condition 3, so the criterion is retired rather than repaired.
- The former `docs/packets/a4b-gp-dsp-engine.md` (`A4b-r3`) is **superseded** by this split
  and retained as provenance only.
- **Open:** `A4s` is not accepted, so neither packet has a baseline yet. If a pinned xemu write
  path cannot route through the single GP write function, `A4b1` stops and goes to the Advisor.
- **Records:** `docs/reviews/a4b-watch-ledger-ruling.md` (ruling 1, verbatim);
  `docs/reviews/a4b-gpin-accounting-ruling.md` (ruling 3, verbatim);
  `docs/reviews/a4b1-a4b2-r2-adequacy-review.md` and `…-r3-…` (both `INADEQUATE`);
  `docs/reviews/a4b-pio-methodology-ruling.md`; `docs/reviews/a4b-xemu-pin.md`;
  `docs/reviews/a4b-q1-advisor-ruling.md`; `docs/reviews/a4b-q2-owner-decision.md`.

## Last closed packet — `A4p-r1` (discovery, ACCEPTED 2026-09-24)

- **Packet:** `docs/packets/a4p-pio-gate-analysis.md`, revision `A4p-r1`, class
  **discovery**, frozen SHA-256
  `B8BDBFAEC31213AA43D7A2BC8771326A73BE6CC124B6E2993BD2D33C57687A4B`.
- **Question:** are the 28 direct reads of `0xFE820010` in `DSOUND` gate-only under the
  locally-checked calling-convention rule C1–C4?
- **Result — selected outcome row `O-GATE`:** E0–E2 clean, **all 28 sites PASS**, 0 FAIL,
  0 UNKNOWN, E3 clean. Every loop is a threshold re-poll with no store; on every exit path
  `T` empties before any use, by immediate overwrite, by a callee-save `pop`, or by C1 at a
  call. **C3 was invoked at 0 of 28 sites.** The `0xFE800000` table resolves to voice-list
  registers (`0x2054…0x2074`), not `PIO_FREE`, and the two stray constants are unreferenced.
- **This discharges Q1 condition 3**, and therefore **retires `AC-PIO`**: `PIO_FREE` is
  gate-only, so the stub decides whether the guest reaches a point but not the data it
  writes. `A4b` cites this row as a precondition.
- **Claim limits:** a discovery packet never satisfies a strict criterion and never claims
  anything works (§5.8). The result rests on the **inferred** premise that DSOUND follows
  the standard x86 convention, checked at every boundary the analysis relies on but not
  everywhere; it cannot see a custom `edx`-return convention passed on untouched; it covers
  only the 28 direct reads, not register-indirect or computed access beyond E3, timing, or
  whether `0x80` is the true device value.
- **Records:** adequacy `docs/reviews/a4p-r1-adequacy-review.md` (`ADEQUATE`);
  execution `docs/reviews/a4p-execution-evidence.md` (SHA-256
  `3DF5D833E89863E5E9BD0264CC64154C6FEADBD6FE10FC1806F6323FF20981F9`); acceptance
  (`ACCEPT`, first stage, final) `docs/reviews/a4p-r1-acceptance-review.md`; revision log
  `docs/reviews/a4p-revision-history.md`.

**Follow-ups carried forward (not authorized until promoted in a packet):**

- **`A4b` ordinary repairs** (§5.4, different mechanism from the retired `AC-PIO`): the
  `[GPDMA] watch` cap must exempt the deciding `payload=0` line (Advisor ruling (d)), plus
  r3 deferred D1, D2, D4, D5. D3 is superseded by C4 and is discharged by `A4p`.
- **C3 caller-enumeration text** (in the frozen `A4p-r1`, deferred): it searches
  `sub_<ENTRY>(`, which matches only the definition; any future C3 use must enumerate
  callers by value — XBE `call rel32` to the entry, cross-checked by the normalised target
  literal in `RECOMP_ABI_CALL(0x<ENTRY>u, sub_<ENTRY>)`. Second instance of the
  `AGENTS.md` rule against enumerating by one spelling.
- **`PIO_FREE` model packet:** still required before the **first strict liveness or
  boot-progress criterion past the spin**. Finding for it: xemu's own `vp_read` calls its
  `0x80` a pretence, so the obvious secondary source cannot corroborate it.
- **Scope `A4b` to the whole GP block, not just GPRST:** R1 shows five GP offsets written.
- `gp_ep_reads` counts only `[APUMMIO] read` lines, so a GP read-modify-write logged as a
  write would not register. Tighten if the row is reused.
- Archive raw `ctest` output in run directories.
- `P0.1-AC1` re-review; moving `RECOMP_AC97_READY` into the classifier's
  `RETIRED_OVERRIDES` registry; the unmodelled AC'97 registers `0xFEC0017C` / `0xFEC00100`.
- Owner-reserved, unchanged: the `0xFE820010` "GP sample counter" / `PIO_FREE` naming
  conflict (`docs/jsrf-run-profiles.md:288-292`).

## `A4b` — parked, awaiting `A4p`'s outcome

**`A4b`, the GP DSP56300 engine**, is the packet `A4a-r2`'s row `O-6` selects, and both its
gates are answered (Q1 by the Advisor, Q2 by the owner). It is **not** promoted and has had
three revisions and two `INADEQUATE` verdicts, both on its `AC-PIO` criterion — which is why
the §5.5 redesign moved that criterion into `A4p`. When `A4p` returns `O-GATE`, the next
`A4b` revision deletes `AC-PIO` and `R-PIO-DATA`, adds the `A4p` precondition, and folds in
the ordinary repairs (different mechanism, §5.4): the `[GPDMA] watch` cap must exempt the
deciding `payload=0` line, plus r3 deferred D1, D2, D4 and D5. D3 is superseded by C4 and
lives in `A4p`. Its other criteria are otherwise unaffected.

**Records:** `docs/reviews/a4b-r3-adequacy-review.md` (second `INADEQUATE`, verbatim);
`docs/reviews/a4b-r2-adequacy-review.md` (first `INADEQUATE`); `docs/reviews/a4b-r2-session-checks.md`
(the import at `0x1C4004` = kernel ordinal 161 `KfLowerIrql`); `docs/reviews/a4b-r1-adequacy-attempt-1.md`
(`pending — reviewer unavailable`); `docs/reviews/a4b-q1-advisor-ruling.md` (Q1 plus the
PREMISE_CHANGED addendum and the condition-3 handoff to `A4p`); `docs/reviews/a4b-planning-rulings.md`
(checkpoint-40 plus the 3(ii) amendment); `docs/reviews/a4b-xemu-pin.md` (xemu pin
`67cc79e663038d1f55448c0f566b37dde016adf6`); `docs/reviews/a4b-q2-owner-decision.md`;
`docs/reviews/a4b-revision-history.md` (non-authoritative).

**General rules recorded** (Advisor-directed): (`docs/agent-workflow.md` §5.5) a criterion
whose evaluation is itself a multi-step static or dynamic analysis the Planner cannot
complete by reading belongs in a discovery packet that runs first, and the change packet
cites its accepted outcome as a precondition; (§6.1) a criterion claiming something about
*every* access to an address must derive its population from the original XBE with operands
normalised to `uint32`, reconcile by normalised **value** rather than spelling, freeze the
count and generating command, condition PASS on `count == frozen count`, and state what the
method cannot see. The lifter spelling fact is operating knowledge in `AGENTS.md` under
"Generated-source rules".

**General rule recorded** (Advisor-directed, `docs/agent-workflow.md` §6.1): a criterion
claiming something about *every* access to an address must derive its population from the
original XBE with operands normalised to `uint32`, reconcile against the generated code by
normalised **value** rather than spelling, freeze the count and generating command, condition
PASS on `count == frozen count`, and state what the method cannot see. A text search is a
lead, never a completeness witness. The lifter spelling fact (an `A1` moffs load → hex; a
ModRM `disp32` → signed decimal; every address ≥ `0x80000000` exposed) is now operating
knowledge in `AGENTS.md` under "Generated-source rules".

## Last closed packet — `A4a-r2` (discovery, ACCEPTED 2026-09-24)

- **Packet:** `docs/packets/a4a-dsp-pending-word.md`, revision `A4a-r2`, class
  **discovery**, frozen SHA-256
  `2366E18C583B3ED82124F0E5D3AAD14DB38B4D3CA493F1695E44FFE779722FBC`.
- **Question:** is the next packet the GP DSP56300 engine (`A4b`), and what must its scope
  include? Unknowns U1–U4: the last `GPRST` value; whether the title reads GP/EP at all;
  whether scratch page 0 holds the XBE `0x001BA0A0` image with the pending word `3` at
  `+0x810`; whether the stop holds on the instrumented build.
- **Result — selected outcome row `O-6`** ("the start write is the only GP interaction; the
  image is in place"), from two strict runs on game `5b58d13` / toolkit `0d7929c`:
  - `U1` answered: `last_GPRST = 0x00000003` (`&3 = 3`), and the trace now reports the
    values the guest actually wrote. The value oracle matched the original XBE immediates
    exactly: `write 0x3FF14 = 000000FF` (`0x001A582A`) and `write 0x3FFFC = 00000003`
    (`0x001A585E`), where the pre-change build logged `00000000` for both.
  - `U2` answered: **zero** GP/EP reads, with a coverage witness (the once-per-block read
    note is unbounded by the trace cap and never fired).
  - `U3` answered: `G = 803CC000`, `S0 = MEM32(G) = 803C0000 = B`, and all `0x5CC` bytes
    equal the XBE image at file offset `0x1A7D60`.
  - `U4` answered: both runs `diagnostic_deadline` with `F = 2` on the offset-unpinned
    frame pattern, i.e. the stop holds with and without the trap.
- **Instrumentation:** toolkit `0d7929c` (`src/apu/apu_mmio_hook.c` only, 39+/6−). T1
  reports the value the access moved; T2 names the 400-line cap when reached. Both sit
  inside the existing `RECOMP_APU_TRACE` block, off by default; the R0 leak check is their
  witness (0/0/0). exe `87971604…7ef` → `9597ff7c…c553`; `ctest` 12/12.
- **Claim limits:** a discovery packet claims observations only and never satisfies a
  strict criterion (§5.8). R1 reached the spin only after its `0xFE820010` polls were
  answered by the `PIO_FREE` stub constant `0x80`, so **its arrival at the spin is
  exploratory-grade however R1 classifies** (Advisor ruling B). Nothing here shows the GP
  would clear `+0x810` if it ran — only real GP DSP56300 execution can (ruling D).
- **Records:** adequacy `docs/reviews/a4a-r2-adequacy-review.md` (`ADEQUATE`,
  `BLOCKING: NONE`); execution `docs/reviews/a4a-execution-evidence.md`; acceptance
  (`ACCEPT`, first stage, final) `docs/reviews/a4a-r2-acceptance-review.md`; revision log
  `docs/reviews/a4a-revision-history.md` (non-authoritative). Superseded `A4a-r1` is
  preserved at game commit `5e739f6`.

**Follow-ups carried forward (not authorized until promoted in a packet):**

- **Gates `A4b`:** **Q1 — ANSWERED** (Advisor, 2026-09-24): the upstream `PIO_FREE` stub
  dependence does **not** contaminate A4b's strict claim, and a `PIO_FREE` model is neither
  a prerequisite for A4b nor part of it. Case ruling recorded verbatim in
  `docs/reviews/a4b-q1-advisor-ruling.md` (Advisor child
  `407c54a3-6ca4-4a65-835b-faf5355195cd`, route `claude/claude-opus-5-5` @ `high`); its
  general clarification is in `docs/jsrf-run-profiles.md` §"Feature enablement". A4b must
  still establish the ruling's four conditions (who wrote the 0; the GP's input
  provenance; that `PIO_FREE` only gates; the claim limits) and must cite the ruling.
  **Q2 — ANSWERED, owner decision (§3.4), 2026-09-24:** the owner accepts
  **GPL-2.0-or-later** for this open-source project, so porting xemu's DSP56300 core is
  permitted. Recorded in `docs/reviews/a4b-q2-owner-decision.md`. The licensing obstacle is
  removed; whether A4b ports the existing core or implements one independently is now a
  **technical** choice for the Planner. **Mechanical consequence for A4b's closure:** a
  binary linking a GPL-2.0-or-later core is a combined work that must ship under
  GPL-2.0-or-later, so closure must update `NOTICE` and the licence files (verbatim GPL text
  alongside the existing LGPL text). That is bookkeeping following from the decision, not a
  further owner question. A4b should also **pin** the xemu commit it ports; the sources
  fetched this session were `master` and are not pinned.
- **`PIO_FREE` model packet (Advisor-directed prerequisite):** required before the **first**
  strict liveness or boot-progress criterion past the spin (e.g. "the title reaches
  <checkpoint> in a strict run"). It is its own packet, placed before that criterion, not
  inside A4b. **Finding for it:** xemu master `hw/xbox/mcpx/apu/vp/vp.c` `vp_read` returns
  `0x80` for `NV1BA0_PIO_FREE` with the comment *"we don't simulate the queue for now,
  pretend to always be empty"* — the obvious secondary source describes itself as a
  pretence, so it cannot corroborate `0x80` as device state, and our `apu_vp.c` is
  xemu-derived and therefore not independent either. The `jsrf-run-profiles.md` reversal
  condition (ii) most likely needs a real FIFO/free-count model or a primary source, not a
  citation.
- **Scope `A4b` to the whole GP block, not just GPRST:** R1 shows five GP offsets written
  (`0x3FF00`, `0x3FF04`, `0x3FF10`, `0x3FF14`, `0x3FFFC`×3). Recorded as an acceptance
  advisory on `A4a-r2`.
- `gp_ep_reads` (as defined in `A4a-r2`) counts only `[APUMMIO] read` lines, so a GP
  read-modify-write logged as a write would not register. It did not bite here. Tighten if
  the row is reused.
- Archive raw `ctest` output in run directories; the packet's Closure named it but only
  `CTestCostData.txt` survives.
- `P0.1-AC1` re-review; moving `RECOMP_AC97_READY` into the classifier's
  `RETIRED_OVERRIDES` registry; the unmodelled AC'97 registers `0xFEC0017C` / `0xFEC00100`.
- Owner-reserved, unchanged: the `0xFE820010` "GP sample counter" / `PIO_FREE` naming
  conflict (`docs/jsrf-run-profiles.md:288-292`).

**Measured blocker (unchanged):** the guest spins at `loc_001A18D0`
(`recomp_0005.c:6748-6751`, live frame in `sub_001A1769`) on a DSP pending word at
`+0x810` that nothing clears. The run ends `diagnostic_deadline`.
`RECOMP_APU_DSP_ACK` stays forbidden as acceptance evidence. **Caution:** A3a's predicted
next stop (`div@0x001A2BFC`) came from an exploratory run with `RECOMP_GPU_ACK` enabled,
whose synthetic completion cleared this word; it is not a prediction of strict behaviour.

**Baseline (2026-09-24).** The accepted A3a work is committed: toolkit `c97ce2c`
(`src/kernel/xbox_memory_layout.c`) and game `a16350f` (`scripts/jsrf_run_profile.py`,
`scripts/ac2-provenance.py`, `tests/test_ac2_provenance.py`). Both trees were clean
after the baseline commits; any later dirty file is new work.

## Last closed packet — `A3a-r25` (ACCEPTED 2026-09-24)

- **Packet:** `docs/packets/a3a-ac97-codec-model.md`, revision `A3a-r25`, SHA-256
  `6F907A42EAC6FD02392E11EADEF127AE840D97A245E771CD15B63A387F0E5BDC`.
- **Change:** the environment-gated `RECOMP_AC97_READY` override was replaced by an
  always-on model in the `nv2a_ack_thread` loop:
  `GS(0xFEC00130).bit8 := GC(0xFEC0012C).bit1`, level-evaluated each tick, atomic,
  outside the `g_apu_mmio_trapped` gate; `GC` is read only.
- **Result:** one **strict** run selected `R-PASS`; `AC1`–`AC6` all PASS. The codec
  poll succeeds, the vector-6 ISR is connected, and the guest proceeds into DSP
  initialisation, where it now spins (the blocker above). The strict baseline
  previously self-relaunched before reaching the poll.
- **Claim limits:** one strict run's codec-presence wait was satisfied by modelled
  device state. This does not establish faithful hardware behaviour (secondary
  sources only), audio, a complete DSP handshake, liveness, or strict boot.
- **Records:** adequacy `docs/reviews/a3a-r25-adequacy-review.md`; execution
  `docs/reviews/a3a-execution-evidence.md`; acceptance (`ACCEPT`, no disagreements)
  `docs/reviews/a3a-r25-acceptance-review.md`.

**Follow-ups carried forward (not authorized until promoted in a packet):**

- `P0.1-AC1` is reopened by A3a (see P0 below).
- Move `RECOMP_AC97_READY` into the classifier's `RETIRED_OVERRIDES` registry and
  update `tests/test_run_profiles.py:560`. Its `removed_commit` boundary now exists
  (toolkit `c97ce2c`); the `added_commit` must be found in toolkit history.
- `RECOMP_APU_TRAP` belongs with the DSP packet.
- The AC'97 registers A3a does not model (`0xFEC0017C`, `0xFEC00100`) wait for a
  later packet.

## Retired packet — `A2h-r6` (premise refuted, 2026-09-23)

`docs/packets/a2h-writer-investigation.md` was retired at adequacy review, never
executed. Record: `docs/reviews/a2h-r6-adequacy-review.md`. Its premise — an invalid
target `0x00700010` dispatched from `0x0017DC23` — was refuted: that failure came
through the static-initializer walker's `tail_jump_alias` entry (`0x0017E58F`) and
was already fixed by commit `cb7cae2`; every run that logged it predates the fix.
`sub_0017DBBD`'s execution status is simply uninstrumented (UNKNOWN). Do not revive
this area without new evidence. Two lessons stand for any future packet that must
observe **inside** a generated function: it first needs a trace seam outside the
provenance-guarded files and a trace mode that keeps the **last** N records; and the
collector's call history plus the frozen dump should be tried first, since they
distinguish call sites with no code change.

## P0 — ACCEPTED

**P0.S and P0.1–P0.7 are accepted.** They are completed prerequisites, not active
implementation instructions. Do not reopen them unless a later edit invalidates an
affected accepted criterion; then reopen only that criterion and re-review it.

Durable evidence:

- `docs/reviews/p0-1-execution.md`
- `docs/reviews/p0-1-vblank-adjudication.md`
- `docs/reviews/p0-2-acceptance.md`
- `docs/reviews/p0-3-to-p0-7-acceptance.md`

The original P0 contracts under `docs/packets/` are frozen provenance; their old
`proposed`, `pending`, `next packet` and `TOOLING REQUIRED` wording is not current
work authorization.

### Reopened: `P0.1-AC1` (by `A3a-r25`, 2026-09-24)

Re-review only `P0.1-AC1` against the new runtime.

1. **Cited sites moved.** Its accepted evidence names `xbox_memory_layout.c:684-686,1613`
   (`docs/reviews/p0-1-vblank-adjudication.md:265`,
   `docs/packets/p0-acceptance-contract.md:78`). A3a changed that file, so both ranges
   shifted. The re-review should cite **symbols**, not line numbers.
2. **`P0.1-AC3` is affected in principle.** `_valid_new_archive` compares recorded
   reasons verbatim (`scripts/jsrf_run_profile.py:585-586`) while `CLASSIFIER_VERSION`
   stays `jsrf-run-profile/1`, so editing the reason string at `:391` makes an archive
   recorded under the old string unreclassifiable.
3. **Measured impact today is zero.** Of 650 archived runs, exactly one carries a
   `run_profile`, and it holds no AC97 reason. The classifier still treats the retired
   name as exploratory, which can never produce a false `STRICT`.

## Evidence and decision rules

- `MEASURED` means inspected source/artifact evidence with an identity/procedure.
  `INFERRED` means a hypothesis or expected consequence.
- Missing, malformed, stale, unexercised or `CANNOT VERIFY` evidence is never PASS.
- Only a verified **strict** run can support strict integration/boot/liveness/device
  claims. Exploratory/fixture evidence remains bounded to what it actually measured.
- A reviewer verifies or refutes each mandatory criterion.
- A disagreement goes to the Persistent advisor, whose ruling is final within the
  evidence invariants of `docs/agent-workflow.md` §2.4.
- A failed measurement cannot be converted to PASS by Planner, Session, reviewer or
  advisor interpretation.
- A post-review code/evidence change reopens the affected criterion.
- Original assets and existing saves remain unchanged.

## Roadmap policy

Older A1–A5 sequences, GPU/audio milestone diaries, historical `next packet`
statements, old model assignments and legacy status tables live in
`docs/jsrf-operating-history.md`. They are provenance only.

New executable work follows the packet lifecycle in `docs/agent-workflow.md` §5: the
Planner designs the packet, an adequacy review of the exact revision returns
`ADEQUATE`, and that frozen revision is promoted into **CURRENT PACKET** in the same
step. Historical evidence may motivate a packet, but a historical failure does not
imply the same failure exists in the target revision.

If no packet is explicitly promoted in this file, implementation is **BLOCKED**.
