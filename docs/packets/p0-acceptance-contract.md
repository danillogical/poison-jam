# P0 acceptance contract

**Contract revision:** `P0-AC-r1` (2026-09-23)  
**Status:** Draft for plan review; no P0 packet is delivered or accepted by this document.  
**Authority:** `plan-jsrf-bare-minimum.md` owns packet status; this file holds its criterion details.  
**Current selection:** P0.1 is the sole next packet. P0.2–P0.7 remain sequential and cannot start until the preceding packet is accepted.

This contract makes the existing P0 proposal reproducible. It does not authorize a
build or guest run as part of plan review. The session must record a packet's exact
criterion IDs, evidence revision and attempts before implementation. Review and
escalation roles are defined only in `docs/agent-workflow.md`.

## Shared evidence and verdict rules

Record one row per criterion in `report-deepseek.md` (or its successor packet
record) with: criterion ID and contract revision; `PASS`, `FAIL` or `UNKNOWN`;
exact command and working directory; complete relevant environment; input and
artifact paths plus SHA-256; game and toolkit revisions and dirty-tree state; tool,
checker and schema versions; expected versus observed result; and the reviewer’s
criterion-specific disposition. `UNKNOWN` includes blocked access, missing tools or
inputs, malformed or unsupported evidence, insufficient exercise/coverage, an
identity mismatch, and reviewer `CANNOT VERIFY`. An advisor may settle an
interpretation or scope dispute; the measured result remains unchanged.

For guest-run criteria, preserve the run directory from `scripts/run-jsrf.py` and
bind its `metadata.json` to the game/toolkit state, XBE hash, executable/PDB/map
hashes and `build/Release/build-source.json`. Classify the **effective** settings:
only a verified `strict` run can support strict integration claims; `exploratory`
and `fixture` evidence can support only their bounded local claims. Missing,
malformed, conflicting or unexercised evidence is never an empty successful set.
For non-run criteria, record repository revisions/dirty state and checker/test
identity; do not invent a guest profile requirement.

Every negative or absence claim needs (1) a positive control proving the target
path and checker can observe the condition, (2) a negative/bad control the checker
must reject, and (3) a missing or malformed control where empty input could pass.
State the population and denominator before running; report every item as
pass/fail/unknown and include the expected total. A suite total alone cannot pass a
criterion. Any post-review edit reopens affected IDs for re-review.

## Existing command surface and tooling gaps

Commands below are present in this checkout. They are evidence inputs, not proof
that a P0 criterion already passes.

```powershell
Set-Location C:\Users\logic\Repos\my_xbox_game
C:\Python313\python.exe -X utf8 scripts\check-run-profile.py <run-dir>
C:\Python313\python.exe -X utf8 scripts\check-run-profile.py --all
C:\Python313\python.exe -X utf8 scripts\check-dump-mapping.py <run-dir> [<run-dir> ...]
C:\Python313\python.exe -X utf8 scripts\build-identity.py verify
C:\Python313\python.exe -X utf8 scripts\build-jsrf.py --parallel 1
C:\Python313\python.exe -X utf8 scripts\run-jsrf.py --seconds 30 --label <label>
C:\Python313\python.exe -X utf8 scripts\test-harness.py
ctest --test-dir build -C Release --output-on-failure
```

`run-jsrf.py` currently has no `--profile` option and defaults to guest-entry
checkpoints. Do not claim its current invocation enforces strict mode or use its
default for a probe. `check-recorded-reviews.py` exists but is a session-specific
reporting script, not a durable acceptance transaction. `scripts/check-agent-docs.py`
and a generation-provenance checker do not exist in this checkout. Every command
below marked **TOOLING REQUIRED** must be implemented and shown to run before its
criterion can be measured; the name is a required interface, not a claim that the
tool exists today.

## Sequenced packets and criteria

### P0.1 — Enforce strict/exploratory classification

**Next packet.** Complete the shared classifier, runner gate and archive schema
before relying on any new guest-level run. Existing evidence inputs include
`scripts/check-run-profile.py <run-dir>` and the A2 run records named in the P0.1
plan section.

| Criterion | Required result and nonvacuous controls | Procedure / readiness |
|---|---|---|
| `P0.1-AC1` | Classifier returns `strict`, `exploratory`, `UNKNOWN` or `MISSING` from the runtime's actual per-variable semantics and defaults, not a generic truthiness table. Evidence to encode: `RECOMP_AC97_READY` is enabled by any present value, including `0`; `JSRF_ALLOW_UNRESOLVED` and `JSRF_ABI_CONTINUE` are enabled by presence; `RECOMP_GPU_ACK` is enabled by default and for every value except exactly `0`; `RECOMP_APU_DSP_ACK` only acts on parsed nonzero addresses and clears those guest dwords. Test absent, empty, `0`, `false`, and active values where each runtime distinguishes them. A known A2g artifact with `RECOMP_AC97_READY=1` and `RECOMP_APU_DSP_ACK=0x803C0810` is exploratory; `RECOMP_GPU_ACK=0` is the strict control for that variable, while absent `RECOMP_GPU_ACK` is exploratory under the current runtime default. Malformed/duplicate settings, missing metadata and unknown schema must not classify strict. | **TOOLING REQUIRED:** `tests/test_run_profiles.py`, run with `C:\Python313\python.exe -X utf8 tests\test_run_profiles.py`. Record a fixture manifest and expected labels before running. Runtime semantics are measured at toolkit `src/kernel/xbox_memory_layout.c:684-686,1613` and `src/apu/apu_dsp.c:57-81`, plus game `src/recomp_manual.c:31,42` and `src/recomp/recovered/recovered.c:5717,5719`; recheck those exact call sites against the source identity under test. |
| `P0.1-AC2` | Explicit strict launch rejects prohibited effective settings before starting the collector/title. Because current `RECOMP_GPU_ACK` defaults active when absent, strict mode rejects absence and requires the caller to supply exactly `0`; the runner must never insert it. Empty/`0` presence-only flags remain prohibited. Clean strict and fixture controls launch their designated sentinel once; exploratory records reasons; no requested strict run silently downgrades. | **TOOLING REQUIRED:** runner `--profile strict|exploratory|fixture` plus launch-sentinel tests in `tests/test_run_profiles.py`. Current `run-jsrf.py` cannot satisfy this criterion. |
| `P0.1-AC3` | Archive records requested profile, effective settings including resolved defaults, classification/reasons, classifier version, exact command, XBE and executable hashes, and both repository identities. Reclassifying the archive yields the same result. Missing or conflicting fields yield `UNKNOWN`. | Same test command as AC1; compare archived metadata through the independent checker, not by reading only the runner's in-memory result. |
| `P0.1-AC4` | Check the four exact A2e/A2f/A2g artifacts in `docs/reviews/p0-1-historical-profiles.json` against their recorded metadata SHA-256 and runtime semantics. All four are expected `exploratory`; withdraw only their strict-integration claims, preserve original review provenance and structural/ABI evidence within exploratory scope, and leave review disposition pending until independently evaluated. A hash mismatch, missing run or changed expectation is `UNKNOWN` and requires reconciliation, never silent replacement. | Existing manifest identifies four paths/hashes. Run `C:\Python313\python.exe -X utf8 scripts\check-run-profile.py <each manifest path>` and verify the manifest hashes. Independent per-artifact disposition is still required. Do not infer an audited archive-wide total from a flagged-run count. |
| `P0.1-AC5` | Archive one fresh run requested as strict, bound to the verified build and actual effective environment. The caller explicitly supplies `RECOMP_GPU_ACK=0`; absence is rejected and the runner does not supply it. It may stop earlier than exploratory evidence; this criterion establishes profile/provenance only, not boot, audio, GPU completion or liveness. | **TOOLING REQUIRED:** strict runner option. Set `$env:RECOMP_GPU_ACK='0'` in the PowerShell process, then run `C:\Python313\python.exe -X utf8 scripts\run-jsrf.py --profile strict --seconds 30 --label p0-strict-baseline`; verify with `C:\Python313\python.exe -X utf8 scripts\check-run-profile.py <printed-run-dir>` and `C:\Python313\python.exe -X utf8 scripts\build-identity.py verify`. Any launch/access/tool failure is `UNKNOWN`, with the artifact/log retained. |

### P0.2 — Make review ingestion a durable acceptance transaction

**Prerequisite:** P0.1 accepted. The current checker is not a transaction or a
general review index; repair or replace it without relying on a session-specific
constant or generic verdict words.

| Criterion | Required result and controls | Procedure / readiness |
|---|---|---|
| `P0.2-AC1` | A tracked record binds packet ID/revision, criterion IDs, evidence revision and hashes, parent/child/turn identity, route/effort as returned by the harness, exact per-criterion verdicts, findings and follow-up disposition. Missing fields cannot close a packet. | **TOOLING REQUIRED:** versioned review-record schema/index and validator. |
| `P0.2-AC2` | Fixtures include two parents with identical labels, unrelated `ACCEPT` text, a verdict in a later completed turn, unreadable cache, unknown schema and no recorded review. Correct ancestry and exact review identity are required; each invalid case stays pending. | **TOOLING REQUIRED:** `tests/test_recorded_reviews.py`; command `C:\Python313\python.exe -X utf8 tests\test_recorded_reviews.py`. |
| `P0.2-AC3` | `AGREED`, `DISAGREED` and `CANNOT VERIFY` are stored per criterion. CANNOT VERIFY, missing projection data and post-review edits remain pending until that criterion is evaluated again. Reviewer assessment of owner-supplied evidence is recorded before acceptance. | Same fixture test plus a read-only acceptance check **TOOLING REQUIRED**; report every criterion's state, not a packet-level keyword match. |
| `P0.2-AC4` | Reconcile A2f/A2g historical review records from available original artifacts. If provenance cannot be recovered, record `UNKNOWN` and preserve the evidence; do not invent a reviewer or verdict. | **TOOLING REQUIRED:** explicit historical record list and hashes; adjudication cites source artifact/turn for each row. |

The old P0.2 basis that workflow lacked a CANNOT VERIFY closure path is historical.
The current closure rules are in `docs/agent-workflow.md` §2.7; P0.2 tests durable
recording and transaction behavior, not a missing current workflow rule.

### P0.3 — Finish the single-authority split and bound onboarding

**Prerequisite:** P0.2 accepted. Keep roster, session loop and escalation policy
only in `docs/agent-workflow.md`; keep acceptance/status here; keep the current
blocker in the report's CURRENT STATE. Preserve dated evidence as history.

| Criterion | Required result and controls | Procedure / readiness |
|---|---|---|
| `P0.3-AC1` | Cold-read of `AGENTS.md`, the plan and CURRENT STATE resolves the same next packet and evidence revision. No active duplicated role roster, DSH invocation mechanics or retired role mandate remains; historical identities stay labeled historical. | **TOOLING REQUIRED:** doc audit checker and fixture corpus. |
| `P0.3-AC2` | Checker passes current documents and fails injected stale active reviewer policy, broken command path, stale authority link and instruction file over its configured byte budget. Positive and each negative control must fail for the intended reason. | **TOOLING REQUIRED:** `scripts/check-agent-docs.py --check` and `tests/test_agent_docs.py`; exact test command `C:\Python313\python.exe -X utf8 tests\test_agent_docs.py`. |
| `P0.3-AC3` | Keep a conservative `AGENTS.md` size budget below 65,536 bytes and retain headroom; report measured UTF-8 bytes. Do not truncate or delete historical evidence to meet the limit. | Run checker and report bytes/limit; missing checker is `UNKNOWN`. |

### P0.4 — Separate capture validity from guest-memory integrity

**Prerequisite:** P0.3 accepted. Preserve actual guest-VA reads; displacement is
evidence about guest memory, never a read correction.

| Criterion | Required result and controls | Procedure / readiness |
|---|---|---|
| `P0.4-AC1` | Fixture results separate structural readability, stack/register location support, image-content `MATCH`/`CONTENT_MISMATCH`, `UNREADABLE` and `MISSING`. A validly captured overwritten image remains readable at actual VAs. | Existing inspect command: `C:\Python313\python.exe -X utf8 scripts\inspect-jsrf.py memory <run-dir> 0x00011000 16`. **TOOLING REQUIRED:** `tests/test_dump_mapping.py`, command `C:\Python313\python.exe -X utf8 tests\test_dump_mapping.py`. |
| `P0.4-AC2` | Positive intact control matches `.text` bytes `8b512c85d28b4130c70190431c00741c`; negative displaced-image control is detected while preserving actual guest bytes; malformed/truncated and named-missing controls fail closed. A single text or stack word cannot certify every region. | Existing checker: `C:\Python313\python.exe -X utf8 scripts\check-dump-mapping.py <run-dir>`. Add fixture controls; named missing input must be exercised. |
| `P0.4-AC3` | On the cited A2g artifact, actual guest-VA reads retain the zero thunk slot, and the logged return VA is checked at its recorded ESP. Any shifted comparison is labeled provenance only. | **TOOLING REQUIRED:** exact artifact fixture/manifest and separate structural/content output. Reviewer reproduces each read; do not use a global displacement correction. |

### P0.5 — Make the Python wrapper the guarded build entry point

**Prerequisite:** P0.4 accepted. Keep `scripts/build-jsrf.py` canonical. Do not
regenerate full translation as part of this packet.

| Criterion | Required result and controls | Procedure / readiness |
|---|---|---|
| `P0.5-AC1` | Preflight reports resolved Python/CMake/CTest and dependencies. Tests distinguish missing tools, invalid parallel count, normalized environment, ordinary compiler failure, and the measured silent confined signature; only the measured signature permits a serial retry, preserving the first log. | Existing build: `C:\Python313\python.exe -X utf8 scripts\build-jsrf.py --parallel 1`. **TOOLING REQUIRED:** `tests/test_build_jsrf.py`; command `C:\Python313\python.exe -X utf8 tests\test_build_jsrf.py`. |
| `P0.5-AC2` | Build target inventory is generated from CMake/CTest requirements, not a second hand-maintained list. The built inventory equals the discovered required test targets; no target is `Not Run` because a failed link deleted its executable. | **TOOLING REQUIRED:** aggregate CMake acceptance target, configured Python binding and inventory report. |
| `P0.5-AC3` | Fresh configure, guarded Release build and full CTest pass; actual expected test count and every test name/result are recorded. | `C:\Python313\python.exe -X utf8 scripts\build-jsrf.py --parallel 1`; then `ctest --test-dir build -C Release --output-on-failure`. Any blocked test remains in the denominator as `UNKNOWN`, not omitted. |
| `P0.5-AC4` | Before/after build identity binds source, executable, collector, PDB/map, configuration, resolved tool versions and Python interpreter. | Existing `scripts/build-identity.py before|after|verify`; extend schema/tests before relying on new fields. **TOOLING REQUIRED:** identity-schema regression in `tests/test_build_jsrf.py`. |

### P0.6 — Make harness permissions and probe expectations explicit

**Prerequisite:** P0.5 accepted. Classify environmental inability separately from
guest or test failure; preserve existing saves and original assets.

| Criterion | Required result and controls | Procedure / readiness |
|---|---|---|
| `P0.6-AC1` | A permitted scratch-root control succeeds and a denied root returns `ENVIRONMENT_BLOCKED`; cleanup is verified. No alternate API is used to evade denial. | **TOOLING REQUIRED:** explicit scratch-root support and permission fixture in `tests/test_harness_permissions.py`; command `C:\Python313\python.exe -X utf8 tests\test_harness_permissions.py`. |
| `P0.6-AC2` | Every defined probe has one centralized expected checkpoint. Enumerate all `run-jsrf.py` probes; ordinary, every probe, unknown probe and explicit checkpoint override are tested. Probe runs do not inherit `guest_entry` by default. | **TOOLING REQUIRED:** shared probe map and `tests/test_probe_expectations.py`; command `C:\Python313\python.exe -X utf8 tests\test_probe_expectations.py`. Existing end-to-end probe suite: `C:\Python313\python.exe -X utf8 scripts\test-harness.py`. |
| `P0.6-AC3` | Preflight checks emulated disk directory/image access without changing bytes. A denied access is labeled `ENVIRONMENT_BLOCKED`; a positive access control is read/write-open then close with before/after hashes identical. Guest runs use an isolated disposable disk/save fixture until that preservation is guaranteed. | **TOOLING REQUIRED:** safe disk preflight and disposable fixture; live run is `UNKNOWN` until available. Preserve original/save hashes. |
| `P0.6-AC4` | Result schema keeps launch, capture, profile and semantic outcome separate; deadline and normal exit never imply liveness/title success. | Extend harness fixtures to assert each state independently; no suite count substitutes for these fields. |

### P0.7 — Guard regeneration provenance and publish the bounded next packet

**Prerequisite:** P0.6 accepted. Protect production generated chunks; no full
translation pass until its inputs and generated-tree baseline are pinned.

| Criterion | Required result and controls | Procedure / readiness |
|---|---|---|
| `P0.7-AC1` | Provenance manifest identifies both repository revisions/dirty state, XBE and analysis-input hashes, exact command/arguments, generator/tool versions, generation mode, generated outputs and protected ABI-instrumentation markers. Missing/unknown inputs are `UNKNOWN`. | **TOOLING REQUIRED:** manifest schema and `scripts/check-generation-provenance.py --check`; no such checker currently exists. |
| `P0.7-AC2` | Check-only/no-op generation writes only isolated candidate output and is reproducible except an enumerated allowlist. Production chunks are byte-identical before/after. | **TOOLING REQUIRED:** isolated candidate mode and `tests/test_generation_provenance.py`; command `C:\Python313\python.exe -X utf8 tests\test_generation_provenance.py`. |
| `P0.7-AC3` | Negative controls changing an input hash/recipe and removing each protected ABI marker are rejected; a missing manifest cannot pass. | Same test corpus; each mutation has an expected failing criterion and retained output. |
| `P0.7-AC4` | Publish only a bounded A2h writer-investigation packet with identity, strict/effective profile, existing evidence, attempt count, instrumented address and next discriminating experiment. A protective trap is containment, not a behavioral fix. | Final report/handoff reviewed against this ID. Do not restart a whole-image census. |

## Closure

P0.1–P0.7 are accepted only when every mandatory AC for that packet is `PASS`,
the required reviewer has evaluated every ID on the same evidence revision, and
the disposition is recorded before selecting the dependent packet. Any `FAIL`,
`UNKNOWN`, missing review or post-review edit leaves acceptance pending. The
reviewer role and disagreement path come from `docs/agent-workflow.md`; this file
does not copy a model roster or harness-specific invocation mechanics.

## Counterexample review

- **Broken behavior passes because the target never ran:** rejected by launch sentinel, probe coverage and explicit exercise witnesses.
- **Exploratory artifact is called strict:** rejected by effective-setting classification and archive reclassification.
- **Malformed/missing inputs become empty success:** every checker contract includes missing/malformed controls and `UNKNOWN`.
- **A suite passes while one target was skipped:** P0.5 requires discovered denominator and per-test result inventory.
- **A valid corrupted dump is discarded or shifted into a false repair:** P0.4 preserves actual-VA reads and separates structure from image content.
- **An unrelated later defect silently expands a packet:** stable IDs and post-review revision rules keep it as a follow-up unless an existing criterion fails.
