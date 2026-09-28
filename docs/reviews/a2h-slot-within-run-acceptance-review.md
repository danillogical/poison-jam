# Stage-1 acceptance review — `A2h-slot-within-run-attribution-r1` execution evidence

**Reviewer:** independent stage-1 acceptance reviewer (did not author the packet, did not execute the runs).
**Date:** 2026-09-28. **Mode:** falsification-first, artifact-only; the game was **not** run.

**Under review:** `docs/reviews/a2h-slot-within-run-execution-evidence.md` (211 lines),
against packet `docs/packets/a2h-slot-within-run-attribution.md` revision
`A2h-slot-within-run-attribution-r1`.

**Packet identity, re-measured:** SHA-256 `3865facc776bc64c6b0cfe8df6c6bd0287dbd6efc339c4e7ffaa7406667bec27`,
33 lines — **matches the frozen value** stated in the evidence record and in the review brief.

## Verdict

**ACCEPT-WITH-CORRECTIONS.** No acceptance criterion is falsified. Every load-bearing
claim I could check from artifacts reproduced. I found three **defects in the evidence
record** (one mislabelled thread identity, one imprecise writer-set phrase, one
incomplete guest-thread census) and **no defect in the execution**. The selected row
`O-COVERAGE` follows from the packet's own fail-closed rule and is the correct row.

## Criterion 1 — artifacts exist: AGREED

All five directories contain `jsrf_run.log`, `result.json`, `metadata.json`, `stacks.txt`,
`process.dmp`, `gpu-report.json`, `gpu-report.md`, `gpu-snapshots.jsonl`, `build-source.json`,
`jsrf_recomp.exe/.pdb/.map`, `jsrf_collect.exe/.pdb`, `source.zip`, `save-root/`.

`run_log_sha256` recomputed from each archived log with SHA-256:

| Run | `run_log_sha256` matches log | Measured hash (prefix) |
|---|---|---|
| 1 | yes | `8cc6fe6c02eed5db…` |
| 2 | yes | `89e190c1ba080359…` |
| 3 | yes | `0f877f9ca4d5d9e4…` |
| 4 | yes | `bff703eedede0991…` |
| 5 | yes | `29952459574dfcbd…` |

All five `result.json` report `outcome=unhandled_exception`, `exit_code=3762440515` (`0xE0424943`),
`dump_ok=true`, `missing_checkpoints=[]`, `checkpoints_passed=true`, `save_root_verified=true`,
duration 7.43–7.76 s (bounded by `--seconds 8`).

## Criterion 2 — artifacts match the commands: AGREED

Archived `settings` in every `metadata.json` are byte-identical in content:
`JSRF_TRACE_A2H_DR=1`, `JSRF_TRACE_A2H_SLOT=1`, `RECOMP_APU_TRAP=1`, `RECOMP_GPU_ACK=0`,
`RECOMP_KERNEL_LOG_BUDGET=100000` (plus runner-owned `JSRF_COLLECTED=1`, `JSRF_LOG_PATH`).
**None** of `RECOMP_APU_DSP_ACK` / `RECOMP_AC97_READY` / `JSRF_ALLOW_UNRESOLVED` /
`JSRF_ABI_CONTINUE` is present. `metadata.seconds = 8` in all five;
`run_profile.requested_profile = "strict"`, `classification = "strict"`,
`environment_classification = "strict"`, `reasons = []`.

`python -X utf8 scripts/check-run-profile.py <run-dir>` → **STRICT** for all five
(`checked 1; strict 1, exploratory 0, fixture 0, unknown 0, missing 0` each).

Identity, identical across all five: `exe_sha256 a7e324642a41…`,
`collector_sha256 5ff6cdf072b1…`, `xbe_sha256 fd1905575671…`,
game revision `b7252c9a49eb…`, toolkit revision `5528d00f4446…`.

**One documentation precision note (not a defect):** the packet's literal
`--profile strict --seconds 8` is consumed by `scripts/run-jsrf.py`; the archived
`metadata.command` is the *collector* invocation, which carries `"8"` as the seconds
argument and no `--profile` string. Strictness is recorded in
`profile_source: "explicit"` / `requested_profile: "strict"` and independently
re-classified STRICT by `check-run-profile.py`. So the criterion is met, but a future
reviewer grepping `metadata.command` for `--profile strict` will not find it.

## Criterion 3 — row correctly selected: AGREED

I classified each run myself from the logs against the packet's TARGET predicate
(exactly one fatal terminal, raw target `0`, continuation `0x0014982E`, slot `0x001C4064`).

| Run | Terminal evidence I measured | Class |
|---|---|---|
| 1 | `[ICALL] invalid target 0x00000000 tid=50616 … return=0014982E`; `[A2HSLOT] terminal tid=50616 target=00000000 slot=001C4064 call=#5555` | TARGET |
| 2 | same shape, `tid=56204`, `target=00000000`, `return=0014982E`, `call=#5555` | TARGET |
| 3 | `[ICALL] Failed to resolve VA 0x001D5078 (tid=61900)`; **no** `[A2HSLOT] terminal` line | NON-TARGET |
| 4 | same shape as 1/2, `tid=67512`, `target=00000000`, `return=0014982E`, `call=#5555` | TARGET |
| 5 | `[A2HSLOT] terminal tid=66468 target=00000001 slot=001C4064 call=#246`; `return=0018CE73` | NON-TARGET |

**Observed targets 3/5; complete-coverage TARGET realizations K = 3; coverage-disqualified
targets 0** — as claimed, and K = 3 ≥ the packet's K ≥ 2. Runs 3 and 5 are correctly
excluded rather than forced into a row. Each target run carries exactly **one** terminal line,
so the packet's multi-terminal discard rule discarded nothing (I confirm: one `terminal`
line per run in 1, 2, 4).

**Arming failure, independently verified — the row's basis is real.** In every run's
`stacks.txt` (note: these lines are in `stacks.txt`, **not** in `jsrf_run.log`; the
evidence record quotes them without saying where they live — see corrections):

- `GUEST_DR_ARM why=handshake ok=0 armed=10 failed=9 collision=0 canonical=00000000001D4064 aliases=28 dr7=000D0001 tids=10`
- exactly **nine** `GUEST_DR_ARM_FAIL tid=… why=create_thread reason=no_mapping_offset`
- **zero** `GUEST_DR_HIT` / `GUEST_DR_HIT_MIXED` / `GUEST_DR_HIT_UNSERVICED`
- `GUEST_DR_DISARM ok=1 cleared=17 failed=0 dr7_nonzero=0 hits=0 hit_overflow=0`

So no run armed all threads and no run produced a DR hit. I specifically tried to falsify
this by searching the logs for `armed=1` — the only hit is the **alias** census
(`[A2HSLOT] alias census armed=1 mapped=28 protected=28`), a different quantity; the
`alias census armed=1` (one alias page bound) must not be confused with all-thread DR
arming. The evidence record does not conflate them, but the token `armed=1` in the log
invites exactly that error; I note it for the successor.

**Anchors verified in all five runs:** `NtAllocateVirtualMemory … size=598869040`;
`xbox_HeapAlloc: out of memory (requested 598869040, used 12715008/50855936)`;
`[KERNEL] → returned 0xC0000017`; and identity-1 dispatch prefix **5555** (I measured the
maximum `[KERNEL] #N:` index for the identity-1 tid from each registry: 50616, 56204,
10116, 67512, 66428 all reach `#5555`).

## Run-5 zero witness: CONFIRMED (with one sharpening)

The claimed record is present verbatim at run-5 log lines 34412–34414:

```text
[A2HSLOT] tid=66468 call=#246 ordinal=224 slot=001C4064 value=00000000 phase=before
[A2HSLOT] tid=66468 call=#246 ordinal=224 slot=001C4064 value=00000000 phase=after
[A2HSLOT] write tid=66468 slot=001C4064 before=FE000104 after=00000000 phase=boundary
```

I read `a2h_watch_sample_slot` at `xboxrecomp/src/kernel/kernel_bridge.c:9038-9055` myself.
The argument **holds**: it reads the live canonical slot (`BRIDGE_MEM32(A2H_SLOT_VA)`),
returns without reporting on the first sample (baseline only), and thereafter reports
**only** when `now != s_a2h_last_slot`. It therefore cannot emit a transition for an
unchanged value. The write is real, and it is a write to the **canonical** address
`0x001C4064`, not to an alias. Provenance is recorded as `JSRF_PROV_UNKNOWN`
(`jsrf_slot_watch_write(JSRF_PROV_UNKNOWN, …)`) — the emitter does not guess, and the
frozen registry agrees: run 5 `classes[4] valid=1 class_id=4 tid=66468 call_index=246
ordinal=224 before=0xFE000104 after=0x00000000 count=1`, `class_witness_count=1`,
while runs 1–4 have `class_witness_count=0`.

**Sharpening the evidence record does not state (and which strengthens it):** the two
`value=00000000` samples are *not* the two endpoints of the record. Call #246's
`phase=before` already read `00000000`; the record's `before=FE000104` is the sampler's
**previous** read (call #245's `phase=after`). The write is therefore bracketed strictly
**between call #245's after-sample and call #246's before-sample**, i.e. outside any
bridge body for call #246 — and the sampler only noticed at #246's after-boundary. This
is consistent with the record, not in tension with it, and it is a tighter bracket than
"within one bridge call".

**Post-boundary `FE000104` reads: zero, as claimed.** After the first zero sample the run
emits only the #246 `after`, the `write` line, one `[KERNEL] → returned`, then
`[ICALL] invalid target 0x00000001` and `[EXCEPTION] code=0xE0424943`. I measured 0
`value=FE000104` samples at or after the first zero sample, in any thread. The evidence's
stated limitation is accurate.

**What this does *not* establish, and the record says so:** no writer, no site, no native
RIP. Correct.

## Install positive control: PASSED in all five

`python -X utf8 scripts/a2h-read-registry.py <run-dir>` — every run:
`install_seen=1`, `install_raw=0x80000115`, `install_value=0xFE000104`, `install_ok=1`,
`install_record_present=true`, `version=3`, `version_matches_reader=true`,
`printed_version_matches=true`, `claimed=5`, `overflow=0`. Corroborated in each log by
`[A2HSLOT] install tid=… slot=001C4064 raw=80000115 installed=FE000104 index=65`.
`scripts/a2h-read-registry.py --self-test` → `SELF-TEST OK` (48 checks).

## Criterion 6 — no over-claiming: AGREED (with one wording correction)

The record states the sharpened writer bound is "**a finding, explicitly NOT as a row**",
names **no** writer and **no** site, and keeps the row `O-COVERAGE`. The packet's
fail-closed rule (line 31: unarmed/uncovered ⇒ `O-COVERAGE`, "never infer provenance from
silence"; line 25: arming gap ⇒ `O-COVERAGE`) makes `O-COVERAGE` **follow necessarily**
from the measured `ok=0 armed=10 failed=9` plus `hits=0`. I could not construct an
alternative row that the packet's own priority order would permit.

The alias leg (`touched_count=0`, `publish_failed=0`, `alias_count=28`, mapped mask
`0x0FFFFFFF` == protect mask, in all five registries) supports "no enumerated mirror was
touched" — a claim scoped to the 28 enumerated aliases, which is how the record words it.
Good.

## Criterion 7 — no synthetic completion: ABSENT

- None of `RECOMP_APU_DSP_ACK` / `RECOMP_AC97_READY` / `JSRF_ALLOW_UNRESOLVED` /
  `JSRF_ABI_CONTINUE` appears in any run's effective settings.
- `git diff --stat b7252c9a49eb…..HEAD` = **one file, the evidence document** (211
  insertions). No `src/recomp/gen/*.c` change; no source change of any kind.
- `RaiseException(0xE0424943u, EXCEPTION_NONCONTINUABLE, 0, NULL);` present unchanged
  (two sites in `src/`); untouched by the two post-packet commits.
- Working tree clean at review time (game and toolkit).
- `PIO_FREE` deferred, `A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened, `0xFFFFB3` unresolved:
  **asserted** in the record; I verified the absence of contrary edits in the diff but did
  **not** re-read the plan's deferred-item ledger — see *Cannot verify*.
- Toolkit `5528d00` unpushed: toolkit `git log` shows `5528d00` as HEAD with a clean tree;
  I did not interrogate remotes.

## Criterion 8 — N = 5 pre-specified and exhausted: AGREED

Packet line 11 pre-specifies "**N = 5 ON runs**, same build … even after early target
observations". Exactly five `-a2h-within-run-on-N` directories exist under `logs/runs`
(N = 1…5); a directory census of `logs/runs/20260928*` shows **no sixth** ON run.

## Corrections — defects in the evidence record (none in the execution)

**C1 (factual mislabel).** Evidence line 183 says run 5's armed thread is "`66428`, guest
identity 4". The run-5 frozen registry says otherwise: `slot 0 / tid 66428 / identity 1`
(start `0x00148023`, the main thread, dispatch `#5555`), and `slot 3 / tid 66468 /
identity 4`. So **66428 is guest identity 1** and **66468 is identity 4**. The surrounding
argument ("the armed thread is not the witnessing thread") is unaffected, but the label is
wrong and should be corrected.

**C2 (imprecise writer set).** Evidence §4 concludes the zero came from "one of the **nine
unarmed threads**". The nine are the nine `create_thread` *attempts* that failed; the run-5
witnessing thread `66468` was **never attempted at all** (it is absent from
`GUEST_DR_ARM_TID index=0..9`). The correct phrasing is "one of the threads the instrument
did not arm — the nine failed attempts **and** the never-attempted guest threads". The
record's own later section gets this right; §4 should be aligned with it.

**C3 (incomplete guest-thread census, understates the gap).** Evidence §"The defect…"
counts "**4 of 5** guest threads never attempted" for run 1. The frozen registry claims
only 5 threads, but the run-1 log shows a **sixth** guest-dispatching thread: `tid=57376`
reaches `[KERNEL] #286` and appears 1149 times in `stacks.txt` with
`DEBUG_EXCEPTION tid=57376 code=C0000005 first=1`. It was never armed and is absent from
the evidence's table. The true statement is **at least 5 of at least 6** guest-dispatching
threads were never attempted — which *strengthens* the record's point. Separately, a thread
raising ~1149 first-chance access violations is never mentioned anywhere in the record; it
does not change the row (`O-COVERAGE` either way) but should be disclosed as unexamined
contrastive data.

**C4 (provenance of the quoted DR lines).** The `GUEST_DR_ARM` / `GUEST_DR_ARM_FAIL` /
`GUEST_DR_HIT` block is quoted without stating it lives in **`stacks.txt`**, not
`jsrf_run.log`. It is absent from the log entirely (I measured 0 `GUEST_DR` lines in all
five logs). A reader checking the log alone would wrongly conclude the claim is
unsupported. State the file.

## Blocking issues

**NONE.** C1–C4 are corrections to the evidence record's prose, not falsifications of an
acceptance criterion; none changes the row, K, the counts, or the conclusion.

## Cannot verify / residual uncertainty

- The **read-path leg**: I cannot exclude that `BRIDGE_MEM32(0x001C4064)` and the guest's
  own read of `0x001C4064` resolve to different host storage. The instrument reads through
  the bridge macro; the packet keeps a three-leg read-path requirement and the row is
  `O-COVERAGE` regardless, so this does not move the verdict.
- `PIO_FREE` deferred / `A4b2-*` / `0xFFFFB3` status: taken from the record's assertion plus
  the empty source diff; the plan's deferred ledger was not re-read.
- Toolkit push state: HEAD/clean-tree verified only; remotes not interrogated.
- The OFF control carries "iff the executable SHA matches the archived Run-1 OFF". I
  verified the five ON runs share one exe SHA; I did **not** compare it against the
  archived OFF run's exe SHA, and no new OFF run was executed (the packet does not require
  one when identity is unchanged). The record's claim that the OFF control carries is
  therefore **not independently confirmed by me**.
- No dump-mapping/XBE-integrity gate (`scripts/check-dump-mapping.py`) was run; no claim in
  the record rests on XBE-backed dump content beyond the registry extraction, which I did
  re-derive.

## Guards

`python -X utf8 tests/test_agent_docs.py` → OK (30 tests).
`python -X utf8 tests/test_recorded_reviews.py` → OK (178 tests).
`python -X utf8 tests/test_markdown_tables.py` → OK (12 tests).
`python -X utf8 scripts/a2h-read-registry.py --self-test` → SELF-TEST OK.
