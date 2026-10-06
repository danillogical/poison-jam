# title-003 — Review 1 remediation plan

Planner guidance under [workflow §2/§4](<docs/agent-workflow.md>), not an approval gate. The Orchestrator owns implementation and may adapt with `PLAN_CHANGE`. Keep the [start plan](<plan-turn-start-title-003.md>) and [review](<plan-turn-review-1-title-003.md>) immutable; correct the [living plan](<plan-turn-updated-title-003.md>) and durable records.

## Preserve; do not reopen

The reviewed zero-baseline gate, 50 tightened containers / 63 consumed addresses, 48 additions (38 PROVED / 10 INFERRED), justified `0xB5F3A` removal / `0xB5EB0` restoration, and g07's exercised stop-20 confirmation remain valid. Reinvestigate only on contrary evidence. Stop 21 remains **NOT runtime-confirmed**; title/M15 are unclaimed, and g07's 1000-present disclaimer freeze is established. Previous `just check` / CTest 38/38 results describe their measured tree, not automatically the remediation tree.

Planner fetched/status-checked both repositories: toolkit `6e6e056` clean/in sync; game `82a2c40` ahead one. A pre-existing [patch-configuration edit](<config/generated-patches.json#L99-L105>) already changes L42→L02 and expands the reason, without changing patch payloads. Preserve it and unrelated edits. Planner writes only this artifact.

## Dependency order

### 1. Close the sole blocker — records only

1. Verify the existing `remove-b5f3a-dispatch` L02 correction. Extend [ledger L02](<docs/jsrf-compatibility-ledger.md#L48>) to explicitly cover that dispatch patch, `0xB5F3A` removal, restored `0xB5EB0..0xB6732`, 50 tightened spans and **3110 + 48 = 3158; −1 false entry = 3157**. Make the current count clear while retaining historical counts. Leave APC-timing L42 unchanged.
2. Correct patch-specific L42 citations in [Current work](<plan-jsrf-bare-minimum.md#L56-L213>), [TR §15](<docs/jsrf-technical-record.md#L1995-L2070>) and the [living plan](<plan-turn-updated-title-003.md>). No global L42 replacement.
3. Published commit `64945a3` cannot be corrected in place safely: add a **forward erratum** in TR and the corrective commit message stating its patch citation means L02. Do not amend/rebase published history or force-push; retain the review's quotation of the error.

**Smallest proof:** inspect patch-ID→L02→relevant ledger note and active citations; diff confirms unchanged patch scope/`before`/`after`; run `python -X utf8 scripts/patch-generated.py --check` and `just check`. ID existence alone is insufficient. **No relift, rebuild or game run needed for the blocker.**

### 2. Fix evidence/claim drift before choosing runtime identity — records/docstrings only

| Correction now | Smallest useful check |
|---|---|
| [Manifest](<config/recovered-functions.json>) container evidence | Derive the original 50-container set from base `b065050` versus the repaired state, not today's zero-finding gate. Give each an authoritative current-end sentence naming [the detector](<scripts/check-hidden-entries.py>); replace stale body-end assertions or mark them historical. Audit the 46 reported contradictions to zero unqualified current-end contradictions. Compare all fields except `evidence`: starts, ends, stack contracts and other semantics unchanged. |
| “Two independent reproductions” in plan/TR/living plan | Reword as **shared-source consistency checks**. Do not rely on unarchived prototype 50/52 agreement or “unprompted” rediscovery as reproducible/independent evidence. Retain committed bad→good controls and the review's base-manifest replay. |
| “Three defects caught by tests / each now a guard” | Correct mechanisms: dropped additions — ad-hoc set difference; duplicates — linker; `0xB3C30` — [ownership test](<tests/test_recovery_span_ownership.py>). Distinguish existing checks from unimplemented repair-script regressions. |
| [Repair-script docstring](<scripts/repair-hidden-entries.py#L27-L30>) | **Delete/narrow unsupported pre-write-check promises**, rather than implementing a new writer validation system here. Compare description with `main()`: it does not check gate success, liftability or new overlap before writing. Do not rerun this writer to repair completed spans. |
| [Detector docstring](<scripts/check-hidden-entries.py>) and matching records | Say **50/63 on the pre-repair manifest**, not 48/59 “current.” Document silent dropping of out-of-span arms / early table-read termination when some arm is in span. Do not promise universal complete table enumeration; retain the reviewed 50-container-specific soundness evidence. `OVERLAP` covers evidenced starts in the **over-run past the own-body end**, not all overlapping spans. No analyzer/gate behaviour change. |
| Small census/provenance inconsistencies | Use **43 HIDDEN_ENTRY + 5 OVERLAP + 2 SHADOWED = 50**, **48/50** database `tail_jump_alias` records, median additions size **366** if retained. Reconcile provenance incidents chronologically: four reported overwrites total, final preserved history 41 amendments / 3 regenerations; intermediate snapshots remain historical. |

**Proof:** scoped semantic diffs, source-to-claim inspection, detector selfcheck/unit tests, stack-depth selfcheck and `just check` on the resulting tree. No compiled-game rebuild for docstrings or records; evidence-only JSON requires no behavioural relift. Preserve provenance history during any identity refresh. These checks do not establish new writer guarantees.

### 3. Correct scope and backlog — records plus narrow static witnesses

- **Misdispatch:** add retrospective `PLAN_CHANGE` to the living plan: the `0xE9A40`, `0x100AB0`, `0x1199C0`, `0x13A340` census was not run; stronger over-run detection and the g07 chain were prioritized. State it remains open. `MISDISPATCH=0` does not clear those entries or the shim population. Do not invent evidence that justified dropping them.
- **Under-wide discriminator:** acknowledge that “the entry's own decoded table arm lies outside its own span” is byte-decidable and better-defined than the noisy 71 adjacent pairs. Carry **25** as reviewer-measured until reproduced against a named manifest. Smallest useful replay: table/extent/resolution witnesses for `0xB06E0` (`0xB09D9` fatal stub), `0x10A0E0`, `0x433C0`. An external arm is a candidate, not automatic proof of a false split. Backlog full inventory, classification, targeted repairs/new gate; no bulk widening now.
- **New overlap:** retract “real continuations / legitimate splits” for the nine 2–5-byte `0x200A5`–`0x200F7` micro-entries contained by `0x1FFF0 [0x1FFF0,0x2015F)`: identity is unproven/suspect, and packed-data dwords do not prove functions. Smallest check: enumerate all nine and inspect the owner's internal labels/calls. Keep aggregate overlap 7→3 with its counting convention; nine contained starts is a different count. Backlog micro-entry classification/removal and general overlap reporting. No deletion/boundary change now: reviewed owner is self-contained, with no demonstrated current runtime hazard.

**Why defer code work:** full shim census, 25-entry under-wide repairs/gating, micro-entry cleanup, repair-writer enforcement/tests, shared-walker coverage hardening and the known provenance-writer history defect need separate controls. Bundling them would reopen verified work and delay the next runtime answer. Record the backlog durably, not merely here. These record/static checks need **no rebuild/run**.

### 4. g08 is the explicit next runtime action

Put near the top of Current work: **bounded exploratory g08 on the repaired final build; exercise `0xB5EB0`, confirm/refute stop 21, identify the earliest subsequent failure if reached.**

- Verify final build/source identity first. Metadata-only changes alter archived hashes despite identical emitted code: use guarded `just build` if required for trustworthy identity, **not full regeneration**. Preserve provenance history. Actual guest/generated-code changes require rebuild, targeted ABI/ownership regressions and full CTest before launch.
- Match g07's recorded effective exploratory settings, diagnostic coverage and relied-on ledger IDs, using L02 for this patch. No unresolved-call/ABI-continuation bypass. Use a useful bound comparable to g07's 900 s budget (failure at 241 s). `just explore-run` defaults to **5 s**: use the supported runner with an explicit sufficient budget instead of a non-exercising smoke run.
- Archive and validate profile/provenance/result; measure repaired target/arm exposure, ABI-verified return, first fatal target/caller, presents and inspected frame. Check dump mapping before interpreting XBE-backed memory.
- **Decide:** exercised repaired path with ABI-verified return/no old failure supports confirmation; same trap refutes it; no exposure is **NOT EXERCISED**. Deadline or absence alone is insufficient. Do not reopen g07's stop-20 confirmation due to g08 nondeterminism or equate chain advancement with title progress. Follow useful subsequent stops under Orchestrator judgment, recording material expansion.

**Re-run required for this advisory: yes. Rebuild only for current identity or compiled-code changes, not the attribution correction.** A non-exercising g08 leaves the advisory open with a coverage experiment; it does not reinstate the ledger blocker.

### 5. Owner notice and durable closure

- **In parallel with record work**, directly notify the owner of three failed Advisor spawns, no ruling and no fallback. Staffing changes are owner-reserved (§7); seek disposition if a route change is needed. Do not repeatedly respawn or choose a substitute. Proof is an owner-facing message, not a buried plan note; ordinary remediation need not wait.
- Recover each historical push boundary/result for `b065050..d8632ce` from available evidence and add `PUSHED_TO: / BRANCH: / COMMIT: / REMOTE_URL: / RESULT:` records. Where unrecoverable, state the gap; current remote reachability proves publication, not an invented historical push transcript.
- Validate the final tree with `just check`, targeted controls and final-build CTest (38/38 unless deliberately expanded). Records-only changes need no rebuild solely for CTest. Synchronize project plan/TR/ledger/living plan with corrections, g08 outcome and backlog; do not create a session report.
- Commit durable work and publish the intended fast-forward state including unpushed `82a2c40`, observing [operating-guide](<AGENTS.md>) clean-tree/origin/test/public-repo safety gates. Toolkit first (unchanged/in-sync can report up-to-date), then game; record actual push outcomes. No force-push or toolkit-upstream push.

## Completion / advisory disposition

**Blocker closes** when L02 describes the patch/delta, active citations agree, published-message erratum exists and final-tree record/patch checks pass. No automatic second-review gate.

**Do now:** all evidence/wording corrections, narrow under-wide/overlap witnesses, retrospective scope record, explicit g08 measurement, push records/publication and direct owner notice. **Backlog:** broader code changes/censuses listed in §3. This addresses each review finding without silently declaring deferred work resolved; the sole blocking fix remains records-only.
