# P0.3–P0.7 acceptance review — disposition

Reviewer: `workbuddy-ai/hy4-preview-f` @ `high` (child
`0ce92c62-d90a-4d8f-b0f0-20ffd82d8f80`), a different model family from the session.
Read-only; every load-bearing measurement reproduced independently.

**Verdict: all 18 criteria AGREED.** No file was edited by the reviewer.

```
REVIEW_CRITERIA_JSON: [{"id":"P0.3-AC1","disposition":"AGREED"},{"id":"P0.3-AC2","disposition":"AGREED"},
{"id":"P0.3-AC3","disposition":"AGREED"},{"id":"P0.4-AC1","disposition":"AGREED"},
{"id":"P0.4-AC2","disposition":"AGREED"},{"id":"P0.4-AC3","disposition":"AGREED"},
{"id":"P0.5-AC1","disposition":"AGREED"},{"id":"P0.5-AC2","disposition":"AGREED"},
{"id":"P0.5-AC3","disposition":"AGREED"},{"id":"P0.5-AC4","disposition":"AGREED"},
{"id":"P0.6-AC1","disposition":"AGREED"},{"id":"P0.6-AC2","disposition":"AGREED"},
{"id":"P0.6-AC3","disposition":"AGREED"},{"id":"P0.6-AC4","disposition":"AGREED"},
{"id":"P0.7-AC1","disposition":"AGREED"},{"id":"P0.7-AC2","disposition":"AGREED"},
{"id":"P0.7-AC3","disposition":"AGREED"},{"id":"P0.7-AC4","disposition":"AGREED"}]
```

## What the reviewer reproduced itself

| packet | measurement |
|---|---|
| P0.3 | suites 26/26; `check-agent-docs.py --check` exit 0; **AGENTS.md 50,541 / 65,536 bytes — 15,135 bytes real headroom** |
| P0.4 | 21/21; A2g thunk slot `0x001C4064` → `00000000` and logged ESP `0x00F7FD00` → `0014982E`, both read at **actual** guest VAs; `STRUCTURE_OK` + `CONTENT_MISMATCH`, exit 1 (fail-closed); no shift code exists |
| P0.5 | 27/27; `discover_required_targets()` genuinely shells `ctest -N` and parses every `CTestTestfile.cmake`; inventory says **12 tests** and includes `xbox_timestamp_test`; the old literal list (from `git show HEAD`) had **11**; no `^TARGETS = [` remains; **CTest 12/12, none "Not Run"** |
| P0.6 | 31/31; 18/18 probes mapped, all 10 `gpu-*` → `('memory_ready','probe_gpu')` with no `guest_entry`; six partition images enumerated; falsified by deleting `gpu-stall` → `complete False` |
| P0.7 | 29/29; manifest independently re-hashed — 17/17 outputs, 7/7 inputs + XBE, all 24 per-file marker counts reproduce; totals `RECOMP_ABI_CALL` 9547, `JSRF_ABI_CONTINUE` 9222, `g_seh_ebp` 6317, `RECOMP_GENERATED_CODE` 5; `--check-only` verified **externally** by before/after SHA-256 over `src`, `scripts`, `tests`, `docs`, `config`, `build\Release` → added `[]`, removed `[]`, changed `[]`; negative controls each fail for their own reason |

Read-only confirmed: all five suites leave `src/`, `scripts/`, `tests/`, `docs/`,
`config/` byte-identical. One apparent write — `docs/packets/p0-review-records.md`
during `test_harness_permissions.py` — is **idempotent** (three runs, hash
unchanged) and is not a protected generated output.

## Findings, and their disposition

The reviewer reported four items and explicitly said none reopens a criterion. All
four were acted on rather than noted, because three were real defects:

| finding | disposition |
|---|---|
| **P0.3-AC1 blind spot:** `check_next_packet_agreement` compared PLAN against REPORT only, so AGENTS.md was unchecked. Measured live: AGENTS.md said *"P0.1 awaits advisor adjudication … do not begin P0.2 yet"* while the plan and CURRENT STATE both recorded P0.1 accepted, and the checker returned no findings | **FIXED.** The stale text is corrected, and the checker now compares the *status verb for the same packet* against the plan. First attempt was still too weak — it only flagged packets the plan omitted, and both P0.1 and P0.2 appear there — so it was rewritten to detect a **contradicting verb**. Verified by reconstructing the exact stale corpus: the check now fires on it, and four regression tests cover the positive case, a historical note, a bare mention, and agreement |
| **P0.7-AC2 weakness:** `check_only()`'s `production_unchanged` compared a before/after from the *same* function, so it was near-tautological | **Recorded as a limitation.** The reviewer's own external hash diff is the real evidence for AC2 and it passed. Changing the flag to a fixed pre-recorded baseline is a follow-up, not a defect in what was claimed |
| **Two test-file names deviate from the contract** (`test_dump_mapping.py` → `test_dump_controls.py`; `test_probe_expectations.py` → `test_harness_permissions.py`) | **Recorded explicitly** in `p0-3-to-p0-7-evidence.json` under `contract_name_deviations`, with the reason for each. The contract is frozen, so the deviation is documented rather than silently renamed |
| **The P0.5-AC2 command in the evidence index raised `BuildError`** once no literal list survived to compare against | **FIXED.** `inventory_gaps()` now reports the comparison as *unavailable* rather than raising — a checker that fails when the defect is fixed is measuring the wrong thing — and the index names a command that runs |

## Residual uncertainty the reviewer recorded

- **P0.5-AC3:** the reviewer ran CTest against the already-configured build rather
  than executing a fresh `build-jsrf.py`. `build-identity.py verify` exits 0 and
  binds the build to current source. The session did run the full guarded build
  earlier in this session (CTest 12/12, identity 0).
- **P0.7-AC2:** the near-tautology above. External byte-identity is the evidence.

## Closure

All 18 criteria are `AGREED` on the same evidence revision, with the four findings
dispositioned. P0.3–P0.7 are **accepted** for their stated scope. The acceptance
establishes the tooling's own behaviour; it is not boot, audio, GPU or liveness
evidence, and P0.7-AC4's packet remains an *investigation* authorization, not a fix.
