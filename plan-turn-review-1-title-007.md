# Independent Turn Review 1 — title-007

TURN_REVIEW: FIX

## Scope and independence

Reviewed the completed-turn game baseline `b349768ef126684b836678a3f23079cac8d67774` and toolkit baseline `1f86fbbd3f9f35576f9ef11704936b74143d31b5`, the [start plan](<plan-turn-start-title-007.md>), [updated turn plan](<plan-turn-updated-title-007.md>), and actual archived evidence. No source edits, commits, pushes, asset edits, or save edits were made by this reviewer. The requested review artifact is the only repository file this reviewer created; reproducer files are outside the repository.

The Orchestrator briefly edited the offline decoder during review, producing a transient game CTest failure. It was reverted. A subsequent `just ctest` independently passed **45/45**. An isolated load of the committed decoder passed **61 tests, 2 skipped**. The transient failure is not a defect of the reviewed commit. Toolkit CTest independently passed **11/11**; the focused toolkit Python suite passed **30 tests**. The reported complete 213-test Python suite and mutation checks were not independently rerun. Remediation edits started after the reviewer reported the admission-counter defect; this review does not certify those uncommitted changes.

## PLAN_EVOLUTION

### Material changes from the start plan

1. The first decision used an offline read of `0x80084000` rather than a presenter override. The surface offset `0x00084000` is not the low guest VA: that low VA reads XBE code. This was the right, non-perturbing measurement.
2. The investigation corrected the apparent disclaimer freeze into a timed hold. This legitimately redirected effort away from keyboard/input speculation toward submission admission and capacity at the transition.
3. A real witness truncation was repaired; 39 runtime-witnessed methods were added to the generated table with no removals. The next stop changed to capacity.
4. Whole-submission rollback changed to bounded, whole-packet unit commits, preserving earlier units on later rejection and withholding the final fence until the walk completes. This was a material compatibility change, explicitly recorded as an approximation.
5. Subsequent runs progressed beyond capacity to a further 29-method class and then a long black interval in an exploratory run. M15 was correctly left unclaimed. The updated completion language broadened the local stopping criterion to F8c/F8d, while the proposed synchronized black-frame role trace remained unexecuted.

### Evidence supporting those changes

- Independently recomputed the sixadmitted dump's RGB565 conversion and packed-RGB FNV-1a: `0x80084000` is black, hash `156ed4086987e325`; `0x8011C000` and `0x801B2000` both hash `8205f3a6d2e48df5`, have 44,478 nonzero pixels, and render the graffiti disclaimer. The latter hash matches the last published framebuffer sample. The relevant dump mapping gate passed.
- Reproduced disclaimer entry/exit at presents **1461 → 2424** across the three cited runs. Their timings differ, but the present counts agree. The earlier bounded captures ended before the transition.
- Independently verified all 39 witness records, log SHA-256 `bd2df809e859d804f68966b29e09ed313f8dca06847242e55529d9bd1f9c3395`, method set, and line provenance after the witness helper's intentional whitespace stripping. Table comparison is **376 → 415 NV097 methods, +39, zero removals**.
- Inspected the actual unit implementation and tests: whole-packet word yield, per-unit sink reset, global packet-cap rejection, retained structural bounds, unit GET advancement, and final-fence gating. These are substantive continuation, not a one-measurement turn.
- The [witness2 report](<logs/runs/20261007-081204-797-title007-witness2/gpu-report.json>) records **4533 successes, zero rejections, presents 3120**. Independently checked 68 black published samples from presents 2450 to 3120 (t=506–697 seconds), and all three frozen surfaces are zero. This supports an unexplained persistent-black observation, not title completion or fidelity.
- Inspected the [xemu title sequence](<logs/workers/title007/xemu/deliverable/jsrf-title-sequence-xemu.png>) and [capture implementation](<logs/workers/title007/xemu/window-capture.py>): the comparator includes the JSRF emblem and “PLEASE PRESS START TO BEGIN”; the capture loop injects no input. The recomp skyline image is not that accepted title.

### Assessment

The broad redirection is evidence-driven and useful. Do not revert to the original speculative input path or demand M15 unconditionally merely because the starting plan preferred another sequence. The turn did continue substantially through witness repair, inventory admission, capacity repair, later-stop characterization, and black-surface inspection.

Nevertheless, the evidence does not justify pruning presentation/source selection globally, the live next action contradicts the completed work, and the next discriminating black-interval measurement is only described. This is a justified checkpoint for findings, but not a clean completed turn against the owner's continued critical-path objective. Remediation should preserve the progress, repair the narrow defect below, correct the records, and actually start the next black-frame investigation.

## BLOCKING

### B1 — Multi-unit unknown-admission count is incorrect

At the reviewed baseline, [nv2a_core.c:1938](<C:/Users/logic/Repos/xboxrecomp/src/nv2a/nv2a_core.c#L1938>) adds whole-walk cumulative `admitted_total` at every unit commit. The yield resets the witness array count but not that total. Earlier admitted occurrences are counted again in later units.

**Independent reproduction:** a scratch test using the real diagnostics fixture and production core/table objects submits three non-incrementing unknown `0x1EA8` packets with counts 2047, 2043, 2047. It commits two units and reaches PUT with `diag=ok`. There are **6137** unknown occurrences, but published `admitted_unknown` is **10227 = 4090 + 6137**. The expected-count assertion fails. This is a diagnostic correctness defect introduced by unit commits, not an inferred rendering failure.

**Required fix:** count each committed unit's admissions exactly once, exclude rejected-unit admissions, preserve committed-prefix accounting across a later reject/retry, and add multi-unit regression coverage. Re-run relevant tests. Do not describe this as invalidating the verified 39 distinct witness methods; deduplicated witness identity is a separate measurement.

### B2 — Unsynchronized snapshot is overstated as global presentation elimination

The [live plan:63–69](<plan-jsrf-bare-minimum.md#L63-L69>), [turn plan:23–29](<plan-turn-updated-title-007.md#L23-L29>), and [technical record:3062–3072](<docs/jsrf-technical-record.md#L3062-L3072>) say “drawn = presented,” eliminate presentation, or assert the presenter is not stale/wrong. The compared current frozen surfaces and last asynchronous published hash are **not paired at the same flip**. The frozen current draw surface is black while other surfaces retain the disclaimer. That does not identify the selected source at the transition or rule out a role/latch/source mismatch later.

**Consequence:** this unsupported conclusion incorrectly removes a still-plausible critical-path branch. **Required correction:** limit the claim to the two retained disclaimer surfaces matching the last published disclaimer sample; explicitly retain presentation/source-role hypotheses until synchronized evidence distinguishes them. D3 is only a partial measurement, not the start plan's same-flip comparison completed.

### B3 — Live blocker/next action is stale, and redirected path is only named

The [live plan:78](<plan-jsrf-bare-minimum.md#L78>) calls capacity the current blocker; [live plan:91](<plan-jsrf-bare-minimum.md#L91>) calls it cleared; [next action:266–275](<plan-jsrf-bare-minimum.md#L266-L275>) still instructs finishing the already-committed F8d and updating an already-updated ledger. The [turn plan:103–108](<plan-turn-updated-title-007.md#L103-L108>) likewise says F8d is in flight. The [technical record:3295–3299](<docs/jsrf-technical-record.md#L3295-L3299>) names a synchronized trace but contains no result from starting it.

**Consequence:** a continuation would repeat a cleared local fix or stop at unexplained black instead of following the selected next path. **Required remediation:** make Current work/Next action unambiguously describe the current black observation and unresolved downstream admission evidence; execute at least one useful discriminating measurement in the black interval. Prefer paired source-role/surface/published-copy evidence keyed to flip and present serial, without `RECOMP_FB_VA` or mutations of guest/presenter behavior. Distinguish refusal, redirection, clearing, and source-selection possibilities. M15 need not be forced; a newly evidenced blocker plus an actually-started redirected investigation can be an honest stopping point.

## ADVISORY

- **Decode corroboration cites the wrong region/report.** [Technical record:3166–3169](<docs/jsrf-technical-record.md#L3166-L3169>) claims the named witness run's report decodes the same 39 methods. Its archived [pending report](<logs/runs/20261007-061613-201-title007-witness-full/gpu-report.md#L40-L48>) starts at GET `0x50B1C` and contains **35** missing methods, excluding preceding `0x0420–0x042C`. Runtime provenance independently establishes all 39. Cite the actual earlier decode/region if corroborating equality is retained; do not overwrite an archived report merely to manufacture equality.
- **Black-run attribution needs clarification.** The [archived build identity](<logs/runs/20261007-081204-797-title007-witness2/build-source.json>) gives core hash `4816db9840bd2ce37187e900cb5c53399fb49bae809c0a32a0c423ad58a77c64`, whereas committed `1f86fbb` core bytes hash `93edd031c6f7106e01d8978c7322a9dc117e803e62d569106312b6d5aa838994`. The table hash matches the commit and contains none of the subsequent 29 methods. Despite the recorded admit switch, the black-run log has no admit banner/witness lines and its report has `admitted_unknown=0`. The source archive does not contain toolkit core. These facts do not disprove black output, but its exact build/override causality should be explained or remeasured before claiming that committed unit semantics plus unknown admission cleared all downstream methods. No root cause is inferred here.
- The “1.7 billion pixels versus black” phrase is not a mathematical contradiction: cumulative writes and final content measure different things. The record itself acknowledges this. Prefer “unexplained persistent black despite cumulative draw activity,” and test clearing/destination/role timing.
- Global packet count remains a whole-walk cap, not a fresh per-unit packet budget. The ledger explains the actual rule later, but its opening per-unit “4096 words / 1024 packets” wording and the core consumer's old whole-submission comment should be aligned with implementation.
- The xemu image comparison establishes the accepted title was not observed; unsynchronized color statistics do not establish that the scenes are different guest scenes or identify a transform bug. Exploratory runs cannot establish fidelity. M15 remains open.
- Initial/final destinations and ancestry were checked: owner forks, upstream push disabled, intended branch tips at their remotes. Outgoing game objects were audited: eight blobs, largest 251,021 bytes, no `game/` paths, no >100 MB blob, secret audit zero hits. No unsafe push, blanket cap increase, or asset upload was found. Reviewer-start trees were clean except requested plan artifacts; subsequent Orchestrator edits were kept distinct from reviewed commits.
