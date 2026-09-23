# P0.S — Disposable save-root prerequisite

Contract revision: `P0-S-r1`, 2026-09-23. Status: proposed for adequacy review.
Owner: main session; implementation worker and reviewer from the current workflow.
Governing requirement: preserve original assets and existing saves while obtaining
P0.1's fresh strict baseline. Advisor `/root/workflow_advisor` approved moving this
bounded prerequisite from P0.6 before P0.1-AC5. P0.6 remains pending overall.

Baseline: game `40ae5bd`, toolkit `484887b` plus recorded workflow/profile edits.
Measured: `src/main.c` passes NULL to `xbox_path_init`; Windows toolkit
`kernel_path.c` uses `SHGetFolderPathW` and initializes real partition images.
Changing the LOCALAPPDATA environment variable does not prove redirection.

Scope: explicit save-root selection, preflight, real path translation fixture and
runner isolation. No guest recovery, renderer change or full translation pass.
No original save/image is copied, truncated, opened for write or deleted by tests.
The parent owns build/run. Workers have disjoint file ownership.

## Criteria (all mandatory)

| ID | Procedure and oracle | PASS / FAIL / UNKNOWN |
|---|---|---|
| P0.S-AC1 | TOOLING REQUIRED: explicit game `--save-root=<absolute directory>` option, shared root-selection helper and native `jsrf_save_root_test` registered as CTest `jsrf_save_root`. Fixture calls the same helper and real `xbox_path_init`/`xbox_translate_path` as production, using a newly created temporary directory. Verify title/user/cache paths and Partition0–5 resolve under the selected directory; test a directory name containing spaces. | PASS only when every enumerated path matches the chosen root and production uses the tested helper before guest entry. A mock path layer or checking argv alone is insufficient. Wrong/default root is FAIL; unavailable native fixture is UNKNOWN. |
| P0.S-AC2 | Same native fixture: missing option value, relative path, nonexistent requested directory, regular-file root and a deterministic access-denied/unusable-root control. Verify no default-root initialization occurs on rejection. Provide a separate disposable directory with sentinel save bytes and compare it before/after. | PASS if invalid explicit roots fail before path initialization/guest entry, no fallback occurs and sentinel bytes remain identical. Silent fallback or writes outside chosen root FAIL; missing rejection witness UNKNOWN. No real user saves are the negative control. |
| P0.S-AC3 | Extend `tests/test_run_profiles.py` with an isolated launch sentinel: runner creates a new empty `save-root` inside its new run directory, passes the absolute root to the executable, and archives the resolved path and disposable=true. Exercise both ordinary and probe launches with spaces; preserve probe parsing/checkpoints. Invalid creation must stop before title launch. | PASS if sentinel sees exactly the chosen root and any requested probe independently, metadata roundtrips and the preexisting disposable sentinel remains unchanged. Reusing an old root, silently continuing after creation failure or losing a probe argument FAIL. |
| P0.S-AC4 | Parent configure/builds the new native target and game/collector, runs `ctest --test-dir build -C Release -R '^jsrf_save_root$' --output-on-failure`, then all current Release CTests. Archive output and source/build identity. No worker runs a concurrent build or guest. | PASS requires native fixture exercised with positive and bad controls, all required tests pass, and `scripts/build-identity.py verify` succeeds. Missing executable, stale build or skipped controls UNKNOWN. |

Commands above are required interfaces until implemented; do not claim they exist
before verifying them. Existing build wrapper must include the new test target;
P0.5 later replaces its manual inventory with an aggregate target.

## P0.1-AC5 amendment (revision P0.1-AC5-r2)

The original strict-profile/provenance criterion remains mandatory. Its live launch
additionally depends on accepted P0.S and must use the disposable root. Archive
runtime evidence of the actual resolved save/disk root and compare it with metadata.
A missing/mismatched runtime root is UNKNOWN/FAIL, never a presumed safe run.
No title launch against the existing save root is permitted for this evidence.
The caller still explicitly supplies `RECOMP_GPU_ACK=0`; the runner never injects
that setting. A guest stop may satisfy baseline capture, not progress or liveness.

Read this amendment with frozen `P0-AC-r1`; AC1–AC4 are unchanged. Record review
and both document hashes in P0.1's execution record before the isolated launch.
Reviewer must reproduce criteria; advisor judgment is not an execution result.
