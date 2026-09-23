# P0.1 execution and acceptance record

Status: **ACCEPTED 2026-09-23.** All five mandatory criteria AGREED by the
independent acceptance reviewer on unchanged contract `P0-AC-r1`. Ruling, measured
basis, applied change, re-review and accepted revision hashes:
`docs/reviews/p0-1-vblank-adjudication.md`. Prior review:
`docs/reviews/p0-1-final-review.md`.
Contract: `P0-AC-r1`, exact criteria `P0.1-AC1` through `P0.1-AC5` in
`docs/packets/p0-acceptance-contract.md`, SHA256
`E91A44E0F2DC2CB15FA217ED79B5DC17C846A68270A2DD8A2B865667E55C2C10` — **unchanged**;
the correction was an implementation change inside the frozen contract, not a
criterion change. The linked frozen criterion text governs; no reinterpretation by
this record.

## Baseline and plan review

- Game `40ae5bdfd85f9547a80bd4f89109934d7f4182e7`, toolkit
  `484887b88ff39f86d17c819375993340ebae972e`; workflow edits are dirty.
- Parent command `C:\Python313\python.exe scripts/build-identity.py verify`
  from the game root returned 0 before implementation.
- Plan adequacy reviewer `/root/workflow_acceptance`, Luna Max, first rejected
  AC2's permission to insert GPU_ACK=0. Author corrected it to caller-supplied
  exact0, rejecting absence. Reviewer then AGREED to implementation at the contract
  hash above. This is not acceptance of an implemented packet.
- The first approval covered hash `6C413468...`; the author subsequently bound
  AC4 to the explicit four-run manifest. The worker detected that hash change and
  paused before mutation. Reviewer re-read successor `E91A44E0...`, verified all
  four original metadata hashes, and AGREED to implementation. Contract frozen.
- Advisor `/root/workflow_advisor` ruled against injecting safe defaults and
  against generic Boolean parsing; parent and worker follow that ruling.

## Ownership and attempt log

Attempt 1: `/root/audit_dsh_session`, requested GPT-6 Luna Max, owns runner, profile
checker/shared module, focused tests and profile documentation. Parent owns
historical adjudication, integration, build/run and acceptance. Other workers
must not edit those files or launch the guest concurrently.

Known-good/known-bad controls and required missing-input cases are frozen in the
contract before implementation. Parent enumerated the four historical runs in
`p0-1-historical-profiles.json` and verified original metadata SHA256 values.
All four contain AC97_READY=1 and DSP_ACK=0x803C0810; no artifacts were altered.

## Evidence index

Advisor-approved dependency adjustment: P0.1-AC5 must not launch against the
existing save root. `docs/packets/p0-save-isolation.md` defines early prerequisite
P0.S and AC5-r2 (strict profile/provenance plus observed disposable-root identity).
P0.1-AC1–AC4 remain unchanged. Reviewer AGREED to implement P0.S at SHA256
`54A8E85AE4D7E4F560BCCD7C5984AFB0DBD6B6A3BD5AE1987934CD54D8668AA5`.
`/root/harden_plan` owns native save-root selection, its native fixture, main,
collector argument forwarding, CMake and the one build-target-list addition.
The profile worker retains sole ownership of runner and profile tests, including
P0.S-AC3. No title has launched. Remaining P0.6 work stays in its original sequence.

| Criterion | Evidence/result | Review |
|---|---|---|
| P0.1-AC1 | Shared classifier; 31 tests; retired-override layer added; verdicts unchanged archive-wide | **AGREED** |
| P0.1-AC2 | Prelaunch sentinels pass; strict launch rejects retired names before any child; strict record/archive also refuse them; over-rejection controls added | **AGREED** |
| P0.1-AC3 | Archive bytes/root witnesses checked; revision-aware retired annotation; 649-archive sweep: 0 verdicts changed | **AGREED** |
| P0.1-AC4 | Four original metadata hashes reproduced; per-artifact disposition reconciled in the manifest, reaccepting nothing | **AGREED** |
| P0.1-AC5 | New strict disposable-root baseline reclassified by reviewer | **AGREED** for profile/provenance only |

**P0.1 is accepted.** Every mandatory criterion is measured and independently
reviewed on the same evidence revision. The acceptance covers
profile/provenance correctness only, not boot, audio, GPU or liveness. Any further
edit to the accepted files reopens the affected IDs.

### Consolidated acceptance review (2026-09-23)

Reviewer `workbuddy-ai/hy4-preview-f` @ `high` (child
`ee31290c-bfb6-48a1-8305-f331f6098355`), different model family from the session,
falsification pass on the frozen revision. Result: all five criteria AGREED.
Its first turn died with a provider error (`PI_AI_ERROR`) producing no
dispositions; recorded as a route failure, not a verdict, and resumed on the same
child. Falsification attempts that failed: forged archives with the retired name in
inherited / effective / both / top-level `settings`; a registry-stripped
differential over all 649 archives (**0 verdict changes**); over-breadth controls
including `RECOMP_KERNEL_LOG_BUDGET`, `RECOMP_MMIO_TRACE`, an undocumented name and
`RECOMP_VBLANKX`. No path found by which a strict launch, record or archive carries
a retired override.

One reviewer residual was assessed and deliberately **not** adopted: normalizing
environment names so a trailing-space spelling is rejected. The runtime's
`getenv("RECOMP_VBLANK")` matches exactly, so that spelling is a name the binary
never reads; rejecting it would re-introduce the over-broad guard the advisor
withdrew. Reasoning recorded in the adjudication file rather than silently adopted.

### VBLANK adjudication and correction (2026-09-23, fresh DSH session)

The persistent advisor (`workbuddy-ai/kimi-k3`, substituted for Astra by user
instruction) ruled that both recorded positions were partly right and that the
reviewer's measurement was correct. Full ruling, the `git merge-base` measured
basis, the applied change and the post-correction verification are in
`docs/reviews/p0-1-vblank-adjudication.md`.

Applied inside frozen `P0-AC-r1`: `RETIRED_OVERRIDES` registry and revision-aware
resolver in `scripts/jsrf_run_profile.py`; fail-closed strict-launch rejection of
retired names; archive-side revision-determined handling; new "Retired overrides"
section naming `docs/jsrf-run-profiles.md` the single authority; stale
`AGENTS.md:22` sentence corrected; focused suite 19 → 29 tests. A second defect
found by the session's own falsification pass — a strict *record* could carry a
retired name in its effective-but-not-inherited settings — is closed in both the
record and validation paths.

Post-correction measurements: tests **29/29 OK**; `build-identity.py verify` 0;
A2 vblank-probe EXPLORATORY with the recorded-revision reason added; strict
baseline STRICT unchanged; archive-wide reclassification unchanged; 649-archive
sweep showing **1** archive carrying a retired override and **0** verdicts
changed. AC4/AC5 are untouched by this change.

### Integration pass 1

Parent configured successfully (`logs/p0-s-configure.log`) and attempted all
current native targets without generation (`logs/p0-s-build.log`). Game and
collector linked; new save-root fixture did not link because the toolkit archive
pulled unrelated dispatch/diagnostic symbols. No native tests or guest launched.
Luna owns the bounded fixture linkage correction; path-layer code stays real.
The reviewer also required binding copied map/build identity and log witness to
actual archive bytes. The profile worker is addressing these before final review.

Integration pass 2 (`logs/p0-s-build-2.log`) narrowed fixture linkage to one
missing symbolic-link lookup. Two same-class failures triggered advisor review.
**Decision: compile the real path and symbolic-link implementation into the
fixture, with only its unrelated logging sink stubbed.** The advisor required
real registry behavior if criteria exercise it; T/U/Z necessarily call lookup,
so an empty-return stub would not satisfy the contract. Luna is adding
`kernel_io.c` as the bounded dependency. No title launched.

Integration pass 3: `logs/p0-s-build-3.log` succeeded; all 12 discovered Release
CTest cases passed (`logs/p0-s-ctest.log`, `.xml`, and `p0-s-lasttest.log`).
`build-identity.py verify` returned 0. All 17 members of
`p0-full-generated-baseline.json` retained their bytes. Parent profile suite
passed 17 cases; the historical classifier returned four exploratory results
with all original metadata hashes matching. Source/artifact digests are pinned
in `p0-1-evidence.json`. Independent criterion review is in progress.

Parent reproduced a remaining AC3 defect: legacy metadata containing only
`settings: [{name: RECOMP_GPU_ACK, value: "0"}]` was classified strict without
any archive identity. Worker is changing safe-looking legacy inputs to UNKNOWN;
enabled synthetic settings still establish exploratory. No guest launched while
this missing-provenance case remains open.

### P0.S accepted — independent reviewer disposition

`/root/workflow_acceptance` returned all four P0.S criteria AGREED on the
unchanged native evidence in `p0-1-evidence.json` (SHA256
`FFD1E8A16FE67FA2C549BBA00C9BE56E4A9F8881210C87D57BFC48D29C928F53`).
It replayed the native CTest (1/1 PASS) and build identity verification (exit 0).

| ID | Reviewer reproduction/evidence | Disposition |
|---|---|---|
| P0.S-AC1 | Main preflight before init; real path fixture checks all six partitions and T/U/Z; actual runtime Partition0 translation witness | AGREED |
| P0.S-AC2 | Invalid root matrix, ACL denial before path init, preserved separate sentinel, no fallback | AGREED |
| P0.S-AC3 | Fresh empty archive root, spaced ordinary/probe argv, launch sentinel, creation-failure rejection, runtime root witnesses | AGREED |
| P0.S-AC4 | Parent full build, 12/12 Release CTest; reviewer native replay and identity verify | AGREED |

Build log SHA256 `C501218D991D5E4A34EFB5ACAEBBB45BFCA255163EAD283B06AC64E04CFD72AA`;
CTest log `132645ED48A75138294585AB6C5204C84087B1F8DA73A36BA42175F6648C6165`;
JUnit `6D3A042FB319F4B979CB1279ECA51D06423FA8ED79A6D947742037CAAF15D936`.
This permits P0.1-AC5's disposable-root launch; it does not accept P0.1 overall.
Legacy provenance correction now passes 19 focused tests, retained in
`logs/p0-1-profile-tests-final.log`; final P0.1 review remains pending.

### AC5 live evidence delivered

Caller explicitly supplied `RECOMP_GPU_ACK=0`, with AC97_READY, APU_DSP_ACK,
ALLOW_UNRESOLVED and ABI_CONTINUE absent. Command from game root:
`C:\Python313\python.exe -X utf8 scripts/run-jsrf.py --profile strict --seconds 30 --label p0-strict-baseline`.
Artifact `logs/runs/20260923-013448-357-p0-strict-baseline`; classifier returned
0/STRICT, build identity verify returned 0 after launch. Both runtime save-root
markers match its new disposable root. Final code/log/metadata identities are in
`p0-1-final-evidence.json`.

The title exited after 1.92 seconds through HalReturnToFirmware(2), logged as
normal_exit/0 with both checkpoints. There is no dump on this early normal exit.
This satisfies profile/provenance evidence only, not boot, GPU, audio or liveness.
The new disposable disk state also differs from historical runs. No cause for
the exit has been established by P0.1. Final independent review is still pending.
