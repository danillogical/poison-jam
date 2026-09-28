# `A2h-arming-coverage-attribution-r2` — stage-1 acceptance review (independent)

**Reviewer:** independent stage-1 reviewer (did not author the packet, did not execute the runs).
**Date:** 2026-09-28. **Evidence under review:** `docs/reviews/a2h-arming-coverage-execution-evidence.md`.
**Packet:** `docs/packets/a2h-arming-coverage-attribution.md`, revision `A2h-arming-coverage-attribution-r2`,
23 lines, SHA-256 `80E9425977547BC1EE44DD95645F72ACF4F7941857A39B6DB67B427E7DD81D84` — **verified by
re-hashing the file on disk and counting 23 lines; both match**.

**Method.** Archive-only. The game was **not run**; every finding below is re-derived from the six
archived run directories, from `git`, and from re-running the three scripts named in the review brief.

---

## Verdict per criterion

### Criterion 1 — Artifacts exist

**AGREED.** All six run directories exist and each carries the full artifact set: `jsrf_run.log`,
`result.json`, `metadata.json`, `stacks.txt`, `process.dmp`, `jsrf_recomp.exe` (+`.pdb`/`.map`),
`jsrf_collect.exe` (+`.pdb`), `build-source.json`, `source.zip`, GPU artifacts and `save-root/`.

`run_log_sha256` in each `metadata.json` was recomputed with `Get-FileHash -Algorithm SHA256` over
`jsrf_run.log` and compared to the archived value. **All six match** (`MATCH=True` ×6).

### Criterion 2 — Artifacts match the prescribed commands

**AGREED.** I read the archived `settings` array out of each `metadata.json` directly rather than
trusting the evidence's prose:

- **ON runs (1-5):** `JSRF_TRACE_A2H_DR=1`, `JSRF_TRACE_A2H_SLOT=1`, `RECOMP_APU_TRAP=1`,
  `RECOMP_GPU_ACK=0`, `RECOMP_KERNEL_LOG_BUDGET=100000` (plus runner-internal `JSRF_COLLECTED`,
  `JSRF_LOG_PATH`). **No** `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED` or
  `JSRF_ABI_CONTINUE` in any run.
- **OFF run:** identical non-gate environment, **no** `JSRF_TRACE_A2H_DR`/`JSRF_TRACE_A2H_SLOT`.

`python -X utf8 scripts/check-run-profile.py <run-dir>` returns **`… STRICT`** for all six;
summary line `checked 1; strict 1, exploratory 0, fixture 0, unknown 0, missing 0` per invocation.
`metadata.seconds == 8` and `run_profile.requested_profile == strict` in all six.

Identity: `exe_sha256`, `collector_sha256` and `xbe_sha256` are **byte-identical across all six**
(`a7e32464…dead9`, `95440ec4…b3ef7`, `fd190557…3ef9c`), as is `build_source_sha256` (`68d3db32…`).

**One observation, not a defect:** `repository_identities.project.revision` is `bdb500f…` for the OFF
run and `2d97e7e…` for all five ON runs. `git diff --stat bdb500f 2d97e7e1` shows that delta is a
**single documentation file** (`docs/reviews/a2h-arming-coverage-fresh-off-evidence.md`, +79 lines) —
no source change. Binary identity across all six is what the packet's "same built binaries" requires,
and that holds. Both trees were clean (`project.patch`/`toolkit.patch` zero-length).

### Criterion 3 — The row is correctly selected

**AGREED.** I classified each run myself from the archived logs against the stated TARGET predicate
(exactly one fatal terminal; raw target `0`; continuation `0x0014982E`; slot `0x001C4064`):

| Run | Fatal terminal (from log) | Raw target | Continuation | Class |
|---|---|---|---|---|
| 1 | `[ICALL] Failed to resolve VA 0x001D5078` | n/a (unresolved) | n/a | **NON-TARGET** |
| 2 | `[ICALL] invalid target 0x00000000 tid=56288 esp=00F7FD00 return=0014982E` | `0x00000000` | `0x0014982E` | **TARGET** |
| 3 | `[ICALL] invalid target 0x3E800000 … return=00147DE2` | `0x3E800000` | `0x00147DE2` | **NON-TARGET** |
| 4 | `[ICALL] invalid target 0x41200000 … return=00147D36` | `0x41200000` | `0x00147D36` | **NON-TARGET** |
| 5 | `[ICALL] Failed to resolve VA 0x001D5078` | n/a (unresolved) | n/a | **NON-TARGET** |

**All five match the evidence's table exactly.** Every run has exactly **one** `[EXCEPTION]` line and
exactly **one** fatal `[ICALL]` line, so "exactly one fatal terminal" is satisfied in each — no
competing terminal, no ambiguity about counting. Run 2's slot is confirmed by
`[A2HSLOT] terminal tid=56288 target=00000000 slot=001C4064 live=00000000 call=#5555 observed=0`.

**Anchors verified in all six runs** (including the OFF): `NtAllocateVirtualMemory … size=598869040`;
`xbox_HeapAlloc: out of memory (requested 598869040, used 12715008/50855936)`; `returned 0xC0000017`;
and the identity-1 dispatch prefix `#5555` (`[KERNEL] #5555: ordinal 294 (slot 64) … ret=0x00149F5D`).

**K = 1, observed 1/5 → `O-COVERAGE` on the K ≥ 2 requirement.** The packet states: *"K≥2 agreeing
targets is required for general attribution; K=1 preserves only its run-local row and `UNKNOWN`
generality."* That is the rule that governs, and the evidence applies it.

### The arming claim (17 / 10 / 7) — **CONFIRMED, independently counted**

This was the claim I most wanted to falsify. I counted the records myself from each `stacks.txt`:

| Run | `GUEST_DR_ARM_OK` lines | `why=handshake` | `why=create_thread` |
|---|---|---|---|
| ON-1 | **17** | **10** | **7** |
| ON-2 | **17** | **10** | **7** |
| ON-3 | **17** | **10** | **7** |
| ON-4 | **17** | **10** | **7** |
| ON-5 | **17** | **10** | **7** |

And in all five runs the two summary records read **exactly** as claimed:

```text
GUEST_DR_ARM_TERMINAL arms_recorded=17 armed_before_handshake=0 armed_at_or_after_handshake=17
  arm_attempt_failures=9 collision=0 create_thread_events=16 exit_events=0 deferred_births=9
  failed_births=0 recovered_by_sweep=9 failed_never_recovered=0 exited_unarmed=0 birth_rows=26
  handshake_seq=19 last_seq=61 handshake_seen=1 terminal=61 last_fail_reason=no_mapping_offset
GUEST_DR_ARM_RECONCILE arm_tid_list=17 arm_tid_overflow=0 disarm_cleared=17
  disarm_cleared_is_not_an_arm_count=1 distinct_armed_tids=17 pre_handshake_births=9
  pre_handshake_exits=0 mapping_unavailable_births=9 orphan_exits=0 incomplete=0
```

I did not stop at the aggregates. In run 2 I dumped **every** `GUEST_DR_*` record and cross-checked the
ledger three ways, and they agree:

- **10** `GUEST_DR_ARM_OK … why=handshake phase=at_handshake` (tids 56288, 66052, 52980, 20228, 2528,
  34424, 66232, 50076, 23400, 31108);
- **7** `GUEST_DR_ARM_OK … why=create_thread phase=post_handshake` (tids **43608, 44924, 45812, 12632,
  36812, 57184, 59892**) — **the seven post-handshake records are present**, each with its own
  `dr7_readback=00000000000D0001` and `canonical=00000000001D4064`;
- the 17 `GUEST_DR_ARM_TID_TERMINAL index=0..16` entries are `source=handshake` for index 0-9 and
  `source=create_thread` for index 10-16 — **an independent 10/7 split**;
- the 26 `GUEST_DR_BIRTH_ROW` rows are `state=deferred reason=no_mapping_offset` for index 0-8 (the
  nine pre-mapping births), then `state=armed` for index 9-25 (17 armed), giving 9 + 17 = 26.

**The nine failed birth attempts are visibly recovered by the sweep**: the nine `GUEST_DR_ARM_FAIL
… why=create_thread reason=no_mapping_offset` tids (66052, 52980, 20228, 2528, 34424, 66232, 50076,
23400, 31108) all reappear as `why=handshake` `ARM_OK` rows, and the record says
`recovered_by_sweep=9 failed_never_recovered=0 exited_unarmed=0`. So the evidence's reading of
`failed=9` as failed *attempts*, not unarmed threads, is directly supported.

**Zero `GUEST_DR_HIT` in all five runs** — no canonical-slot write was trapped. **The DR0 canonical is
`0x001D4064`** (the watch is armed on the slot's canonical form), consistent across every `ARM_OK`.

**Verified caveat, not a defect:** `GUEST_DR_ARM_RECONCILE_EXIT_RULE` reports `exits_recorded=0
exit_records_missing=0` with the note
`EXIT_THREAD_DEBUG_EVENT_is_not_delivered_to_a_DEBUG_ONLY_THIS_PROCESS_debugger`. The evidence
reproduces that caveat rather than implying exit completeness — correct. `GUEST_DR_ARM_PRE_MAPPING_BOUND`
gives `births_before_mapping=9 births_before_handshake=9 exits_before_handshake=0 pre_mapping_exits=0
decision=decidable_from_records window=NO_PRE_MAPPING_EXIT` in all five.

**Central claim: CONFIRMED.** 17 armed, split 10 handshake / 7 post-handshake, in all five ON runs.

### Criterion 5 — Fresh OFF is record-level inert

**AGREED.** From the log and `stacks.txt` of `20260928-030751-407-a2h-arming-coverage-inert-off`:

- `[A2HSLOT]` occurrences in the log: **0** (ON runs: ~14 900-15 220 each — the gates are live there);
- `GUEST_DR_` occurrences: **0 in `stacks.txt` and 0 in `jsrf_run.log`** (ON runs: 101 in `stacks.txt`).

I also extracted the frozen registry myself with
`python -X utf8 scripts/a2h-read-registry.py <off-run-dir>`. Everything is zero:

- `version=3`, `version_matches_reader=true`; latch `install_seen=0 install_raw=0x00000000
  install_value=0x00000000 install_ok=0 claimed=0 overflow=0 sequence=0 transition_count=0`;
- **watch** `armed=0 alias_count=0 mapped_mask=0x00000000 protect_mask=0x00000000 touched_count=0
  publish_failed=0 handshake_seen=0 class_overflow=0 class_claimed=0 terminal_seen=0
  terminal_target=0x00000000 terminal_slot=0x00000000 class_witness_count=0 alias_touch_count=0
  alias_write_count=0`, with all six class entries `valid=0` and zero-filled.

Record-level inertness, including the watch, is **confirmed by my own extraction**.

### Criterion 6 — The latent harness break and its fix

**Verified as claimed.** `python -X utf8 scripts/test-harness.py` **passes: 19 probes, all `PASS`,
exit 0** (`PASS: 19 harness probes; trapped USER submission, MMIO ownership/lifecycle, PTIMER runtime,
thread capture, GPU history/decoding, missing memory and stalled queues verified`). It does launch real
collector runs; that is expected and the probes completed.

**"Broken before this packet" — verified.** `git show 2c8765c^:scripts/test-harness.py` still contains
both pins the evidence describes:

```python
match = re.search(r'GUEST_REGISTRY address=([0-9A-F]+) version=1 claimed=(\d+) overflow=0', stacks)
        assert struct.unpack_from('<I',data,rva+address-base)[0] == 1
```

The registry version moved **1 → 2** at `009f624` (2026-09-28 00:14:28) and **2 → 3** at `f5b709d`
(2026-09-28 01:31:27), while `scripts/test-harness.py` had been untouched since `e336a1c`
(2026-09-21) — so the version moved **after** the harness was last modified, and the harness was stale
from `009f624` onward. Both of those commits predate this packet's promotion (`be2149b`,
2026-09-28 02:54:21). **The claim that this break predates the packet is confirmed by commit dates.**

I also checked the "nothing surfaced it" claim: `git grep -ln "test-harness" -- tests ctest CMakeLists.txt`
returns **nothing**, and none of the twelve `tests/test_*.py` guards invoke it. The suite does contain
`tests/test_harness_permissions.py`, which does **not** execute the harness — so a reader could easily
mis-read that filename as coverage. The evidence's process finding stands, and its decision to leave
the "should this be in the guard set?" question to the Session is the right call.

### Criterion 7 — No over-claiming

**AGREED — no over-claiming found.** Two checks:

1. **"NOT a coverage failure" is a fair reading.** The packet's `O-COVERAGE` triggers are *"any
   unattempted guest-dispatching tid or uncovered guest-dispatch interval"*, *"missing/ambiguous
   birth/arm/sweep/exit lifecycle"*, *"event loss/overflow/publication disagreement"*. In all five runs
   the arming ledger is complete and reconciled (`arms_recorded=17`, `distinct_armed_tids=17`,
   `arm_tid_overflow=0`, `failed_never_recovered=0`, `exited_unarmed=0`, `incomplete=0`), and no
   coverage-disqualifying condition appears. The shortfall is in the **number of TARGET realizations**
   (K = 1 < 2), which is a yield outcome, not a coverage defect. The distinction the evidence draws is
   real and matches the packet's own trigger list.
2. **No smuggled attribution.** The evidence names no writer, selects no writer row, and states
   generality is `UNKNOWN` with only run 2's run-local row preserved — exactly what *"K=1 preserves
   only its run-local row and `UNKNOWN` generality"* prescribes. It correctly declines to cite the
   zero-`GUEST_DR_HIT` result as a no-write finding (the packet forbids inferring a no-write from no DR
   hit), and its *"Recorded as contrastive evidence"* framing matches *"non-targets do not acquire
   writer rows"* for the four NON-TARGETs.

One **scope note** I record neither as agreement nor disagreement: the packet's *"If zero targets in
five … STOP and re-refer"* rule is written for K = 0, and the evidence applies it by analogy to K = 1.
The K = 1 sentence in the run plan is the directly governing text, and it produces the same outcome
(re-refer, no extension). The reasoning is sound even if the citation is to the neighbouring clause.

### Criterion 8 — No synthetic completion

**AGREED.** No `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED` or
`JSRF_ABI_CONTINUE` in any run's archived `settings`; `RECOMP_GPU_ACK=0` and `RECOMP_APU_TRAP=1` in all
six. `git diff --stat be2149b HEAD -- src/recomp/gen` is **empty** — no generated-source edits since
promotion. `git grep -n "0xE0424943" -- src` returns exactly two unchanged sites in
`src/recomp_manual.c` (`RaiseException(0xE0424943u, EXCEPTION_NONCONTINUABLE, 0, NULL)` at lines 33 and
69). `git status --porcelain` is **empty** — clean tree, no stray edits. `PIO_FREE` deferred,
`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened: confirmed by the empty diff and clean status; I did not
independently diff those legacy items beyond confirming no source tree change.

### Criterion 9 — N = 5 pre-specified and exhausted

**AGREED.** The packet's run plan specifies *"up to **N=5** independent bounded strict ON launches"*
and *"never extend N without Advisor referral"*. `Get-ChildItem logs/runs -Directory` filtered on
`*a2h-arming-coverage*` returns **exactly six** directories: the one fresh OFF plus ON-1 … ON-5. **No
sixth ON run exists**, and the evidence explicitly states "No sixth run."

---

## Disposition

**ACCEPT.** All nine criteria verified independently. The central claim — 17 armed threads, split 10
handshake / 7 post-handshake, with all seven previously-silent post-handshake arms now individually
recorded — was counted three independent ways in the archives and holds in all five ON runs. The fresh
OFF is inert at record level including the watch. The harness fix is real, and the claim that the break
predates this packet is confirmed by commit dates. No over-claiming, no synthetic completion, N = 5
exhausted with no sixth run.

### What I could not verify / scope limits

- **Guest-side liveness beyond the archived artifacts** — by instruction, no game was run. Every
  finding is archive-derived; I re-ran only `check-run-profile.py`, `a2h-read-registry.py`,
  `test-harness.py` and the three guard tests.
- **Per-tid guest-dispatch exhaustiveness** (the packet's operational dispatch criterion: proof that
  ICALL/`[KERNEL]` records exhaust guest dispatch for the run) — I confirmed 17 armed and
  `native_threads: 17` in every `result.json`, and `exit_events=0` / `exited_unarmed=0`, but I did not
  independently enumerate every dispatching tid against the dispatch witness set. This does not change
  the row: `O-COVERAGE` is already selected on K ≥ 2 regardless.
- **`arm_tid_list=17` vs. the arm-record count** reconciles (17 `ARM_TID_TERMINAL` entries in run 2),
  but I spot-checked the terminal tid list in run 2 only, not all five.
- `PIO_FREE`, `A4b2-r7/r8`, `A4b1-r4`, `0xFFFFB3`: confirmed untouched only via the empty
  `src/recomp/gen` diff and clean `git status`; I did not re-open those legacy decisions.

### Notes for the Session (not defects)

1. The OFF run carries a different `project.revision` (`bdb500f`) than the ON runs (`2d97e7e1`); the
   delta is a single added documentation file. Binaries are identical across all six, so the packet's
   identity requirement holds — but future packets may want to note this explicitly so the differing
   revision is not re-litigated as mixed identity.
2. `tests/test_harness_permissions.py` exists but does not execute `scripts/test-harness.py`. The
   evidence's workflow question — whether the harness belongs in the guard set — remains open and
   correctly deferred to the Session.

### Guard tests

`python -X utf8 tests/test_agent_docs.py` → **OK (30 tests)**;
`python -X utf8 tests/test_recorded_reviews.py` → **OK (178 tests)**;
`python -X utf8 tests/test_markdown_tables.py` → **OK (12 tests)**. No failing guard left.
