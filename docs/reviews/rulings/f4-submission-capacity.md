# F4 submission capacity — Advisor ruling

2026-10-01. Persistent Advisor child `4e6d87e1-f748-48b3-a0a4-a6e5728bfeee`, provider `claude`, model `claude-opus-5-5`, effort `high`; startup continuity probe PASS. Owner-directed F4 chore, no packet promoted. The following is the Advisor response verbatim (apart from this record heading).

## Response

F4 SINK RULING (Persistent Advisor 4e6d87e1; record verbatim in plan/TR)

RULING: Option A′. Size the commit staging to the walk's existing word budget, keep whole-submission atomicity, and make "sink_capacity" structurally unreachable on the main path. No incremental commit in this packet.
1. Add one constant, e.g. NV2A_SUBMIT_MAX_WORDS = 4096. Use it in nv2a_core.c:1501 and :1569 (the word budget), and for BOTH `staged[]` (nv2a_core.c:1442, a local 1024-entry array that is the real second cap; enlarging only pfifo.sink[] would not fix F4) and `pfifo.sink[]` (nv2a_state.h:174). Move staged[] off the stack into pfifo state; it would be 48 KB at the new size.
2. Each staged method consumes a parameter word, so staged_count ≤ words ≤ 4096. Add `_Static_assert(capacity >= NV2A_SUBMIT_MAX_WORDS)`. Keep the :1606 and :1495 guards against the new capacity as defensive invariants: they are now unreachable except by a carry bug, which must still reject.
3. Fix the stale comment at :1461-1467. The cap is 1024, not 256; the sink payload is written but not read by production code, and sink_count is a diagnostic/test seam.
4. Do not touch the rejection rules, REJECT_CASE atomicity (test_nv2a_contract.c:746-761), the 1024-packet budget, or action staging/commit.

BASIS:
- OBSERVED (I read the source): the :1606 guard is the only F4 rejection, and it bounds staged[] (:1442, :1558) as well as sink.
- OBSERVED: sink[] payload has no production reader. grep of toolkit src finds only the stores at :1613-1616; snapshot :999 copies sink_count only.
- OBSERVED: the F4 submission (worker brief §5, which takes it from the plan/TR; I did not re-decode it) is 1364 words, 259 packets, 1109 methods. That is inside both the 4096-word and the 1024-packet budgets, so method capacity is the only thing that fails.
- INFERRED: xemu, Mercenaries and this toolkit's own nv2a_pb_scan.c all dispatch per method with no rollback, so whole-submission atomicity is a model approximation, not hardware.
- Fidelity vs cost: atomicity buys clean, attributable rejection diagnostics (GET pinned, nothing applied) for every unsupported or unknown method. That is how the earlier `unsupported_method` stops were found. Incremental commit is more faithful: GET stops at the faulting packet and earlier methods take effect. But it rewrites the test contract, carry/hold, action_commit and the semaphore rollback, which makes it a packet of its own, not a cheap F4 fix.
- Atomicity is now the deliberate choice, so it must be admitted.
UNCERTAIN: whether the next guest submission exceeds 4096 words. Producer PUT kick sizes have not been measured. The comment at :1571-1574 also assumes a jump-free walk cannot hit the budget; that holds only while a submission is ≤16 KB.

TESTS (owner requirement): all in game tests/test_nv2a_contract.c.
(a) A multi-packet stream with more than 1024 staged methods, e.g. the real shape of about 1109 methods over several packets, using an implemented non-trapping method or NOP with param 0. Assert acceptance, sink_count == N, GET == PUT and submit_successes+1. It must FAIL on the pre-change toolkit with sink_capacity; record that run.
(b) One packet with count 1025 and PUT covering all 1026 words is accepted.
(c) Replace :836-838 "USER sink capacity". If the fixture buffer can hold more than 4096 words, write a word-budget REJECT_CASE that keeps the atomicity checks. Otherwise rely on the _Static_assert, plus the existing packet-budget case at :830, and state that a method overflow is unreachable by construction.
Keep every REJECT_CASE unchanged.

LEDGER: add an entry, since none exists (I grepped). "NV2A submission walk commits each submission all-or-nothing, with a per-walk budget of 4096 words / 1024 packets; hardware dispatches per method." Class: Approximated. Site: nv2a_core.c submit walk. Always-on, no switch. Removal gate: the incremental-commit packet. The F4 smoke run record cites this ledger ID. This entry admits a deliberate model choice; it does not add a new modelled cause.

NEXT STOP: after the fix, run the F4 smoke run once.
- budget_exhausted with a jump-free 32-address trace → this is capacity, not a loop. Bring it back to me; I will rule then on a producer-bounded budget versus the incremental-commit packet.
- budget_exhausted with a cyclic trace, or a new method/unsupported stop → ordinary next blocker.
The W14 ceiling verdict comes at that bounded stop, as you asked.

REVERSED BY:
- a production reader of sink[] payload that needs a bounded size;
- evidence that the F4 submission exceeds 4096 words or 1024 packets (i.e. the brief's 1364/259 is wrong);
- or the post-fix smoke run hitting a jump-free budget_exhausted on its first stop. That would make A′ a one-step deferral, and I would then prefer the incremental-commit packet.

## Clarification (Advisor response verbatim)

CONFIRMED on both points.
1. "Keep every REJECT_CASE unchanged" means every REJECT_CASE other than "USER sink capacity" (:836-838). That one is replaced by the case accepting a single 1025-count packet, plus an explicit word-budget REJECT_CASE with the full atomicity checks if the fixture holds more than 4096 words. Otherwise, rely on the _Static_assert and state that the case is unreachable by construction. The REJECT_CASE macro (:746-761) itself does not change.
2. Ledger: the entry is a deliberate Approximated model choice, all-or-nothing commit per submission. It is not a claim about hardware and it changes no causal policy. Its removal gate is the incremental-commit packet.
REVERSED BY: none at this level. These are readings of my own ruling's text.

## Comment and concurrency clarification (Advisor response)

ACK, no objection.
1. Comment at :1571-1574: correcting it as a pure comment is within A′. Suggested sense: "A jump-free walk consumes at most PUT-GET words, so reaching the budget means either a jump/call moved pc (a cyclic trace) or the pending span exceeds the budget (a jump-free trace). The 32-address dump tells them apart." The comment edit must not change the code: the fprintf and the break stay exactly as they are.
2. Parallel exception: acceptable. The two pytest files are Python only and do not touch build/ or the game root. INFERRED from their imports of scripts/*.py, not run by me. Condition: the side worker must not run ctest/just test/just check while ca4b9205 holds unbuilt toolkit edits. Those commands would build or run against a mixed tree and give a misleading red. The implementation worker builds only after the side worker releases the slot.
REVERSED BY: the side worker's reproduction needing just check/ctest. In that case serialise it before the implementation build.

## Pytest side-item fault consult (Advisor response)

PYTEST SIDE CONSULT (quick; I read both test files and did not run them, since my run job was killed to leave the build/run slot to your worker).
test_transcribed_values.py, 1 failure. Ranking: (1) MOST LIKELY a record-content failure, not a code defect. RealTreeTests.test_the_delivered_records_pass (:139-146) runs scripts/check-transcribed-values.py over the LIVE records tree, so any new or updated record whose hex literals are uncited fails it. The fresh docs/reviews/startup-current.md, with child IDs and hashes, is a prime suspect (INFERRED). (2) Less likely: the test at :149 if the record count dropped. Cheapest discriminator: run `python -X utf8 scripts\check-transcribed-values.py` and read the flagged file/value. Fix the record by citing the source or adding Verified-by. Do NOT add the value to KNOWN_CONSTANTS just to silence the checker.
test_ttd_aliases.py, 4 failures. Ranking: (1) stale fixture or expectation against a changed script (query/census/verdict tooling); (2) a real script defect; (3) a missing local artifact is LEAST likely, because every artifact-dependent test calls skipTest when its trace or run is absent (:253, :270, :403, :417, :464, :505, :515, :581, :584, :597, :655). Exception (INFERRED): a PRESENT but changed or partial artifact can still fail. Cheapest discriminator: `pytest tests/test_ttd_aliases.py -q --tb=short -rfs`, then group the failures by class. HostBase, RamSize, Alias and QueryScript failures point to script drift (compare with `git log -3 -- <script>`). Failures in AdmissionVerdict, TerminalTool, CapTruncation or Census whose artifact exists point to local artifact drift, which is environment, not code.
Trivial fixes the worker may make: citing a record, or updating an expectation where a script commit intentionally changed the output. Anything that changes checker semantics comes back to me.
REVERSED BY: the checker output flagging a non-record file, or the ttd failures being assertion errors inside pure-function tests whose script has not changed since the test was last green, which would mean a real defect.

## Deferred (Advisor advisory)

ADVISORY (record under Deferred; it does not change the behaviour premise): the worker's first "Mercenaries has no NV2A" was a negative claim drawn from a truncated view, which violates §2.4.5/§2.4.6. A negative result from a fetched tree listing needs a positive witness: a root listing showing the expected top-level dirs, or a direct fetch of the path where the code should be. Future prior-art briefs should require "path fetched directly, HTTP status" for every negative.

## Session scheduling clarification

The independent side-item reproduction is Python-only; the implementation worker owns the sole
CMake/game build/run slot. It received permission to begin the focused CMake red/green experiment
without waiting for an unrelated Python process to finish. No full `just check`/`just test` is run
while that Python reproduction remains in progress. Source and build scopes remain disjoint.

**ERRATUM (2026-10-01, appended; the text above is the original rule and is unchanged).** This
rule was **not** observed: the full CTest validation ran 08:57-08:58 UTC while the side pytest
reproduction was in progress (08:54:54-~09:02:01). The Advisor did not grant that concurrency —
it approved the clarified plan with full check/test **deferred**, and its later A′ check cleared
full validation without restating the deferral. See "Schedule deviation" under the
fault-diagnosis ruling at the end of this record. No impact on results: the side tests read no
build artifacts, and the transcribed-values result was re-measured green afterwards.

## Post-implementation and W14 ruling (Advisor, 2026-10-01)

Advisor independently inspected both diffs: A′ conforms; both arrays statically cover the word
budget, staging is PFIFO-owned under its lock, heap allocation makes the state growth safe.
1024-packet budget, action staging and other rejection cases are unchanged. The 2048-word fixture
fallback is accepted; 1109 methods over 555 packets tests method count, not the real packet shape.
The red run's 513 packets match the old guard (INFERRED, not a printed diagnostic).
Optional diagnostic-is-OK assertion deferred to avoid churn after measured red/green; reversed by
a future failure of that case without a named diagnostic.

Run `20261001-020407-358-f4-capacity-fix-smoke`: Advisor directly inspected artifacts and profile.
Exploratory, 48.3 s diagnostic deadline; logged submissions #0–63 all OK, GET=PUT through
`0x47A84`; final GET=PUT `0x16648`. No sink or word-budget diagnostic. **Render, draw, clear and
flip are UNKNOWN for this capture, not zero** — corrected by the fault-diagnosis ruling below.
No strict horizon claim or F4 frame acceptance. **W14 CONTINUE**, ceiling not triggered: 3h08m
since the strict move at 05:57:40.3 UTC. Bound: **11:00 UTC or two more smoke iterations,
whichever comes first**, then return for the ceiling call unless a finding is accepted or a
strict run moves the horizon. Next is one read-only worker on this archive, no rerun: deadline
thread waits/ICALLs **first**; only if those point at GPU, flip/Present path and PCRTC/VBlank
pending versus interrupt-enable writers. Advisor re-ranking (INFERRED from zero draws/clears/clip):
non-GPU loading/audio/timer wait first, vblank gap second, D3D init before frame setup third;
ignored flip last. GPU-poll loop evidence reverses that ranking. **That re-ranking is WITHDRAWN**
by the fault-diagnosis ruling below: it rested on the zero draws/clears/clip figures, which this
capture does not contain. Reversed by a different strict stop, an unlogged capacity/budget rejection,
or frames appearing. No second run or fix is authorized by this diagnosis assignment alone.

## Side-item citation ruling and measured result

The four reported alias failures are UNKNOWN/not reproduced: 49 passed with current intact
artifacts. Reopen with failing test names and the reporting artifact state; no code defect/fix claim.
The one transcribed-values failure is a baseline record citation gap, not code or fixture drift:
`docs/reviews/t3-gdbstub-register-layout.md` row 98. Advisor inspected deterministic selftest and
unchanged script since `abd419c`, and reproduced matching output. Approved current re-check
citation only for row 98, not historical proof or live-guest rows 94–97; no Verified-by blanket
marker and no checker semantics change. Reversed by different output or a script diff.
Worker output `logs/workers/t3-xemu-diff-selftest-2026-10-01.txt`, SHA256
`3410CFD67B092ED0B9EA68760AE0329E75FE72032681BD4DB00D877405F95C32`, is five UTF-8 lines:
0 self-differences, 1 seeded difference, 12 truncation differences, all PASS. Current re-check
at `3be0adb`, citation commit `d9db283`; checker exit 0 and 10/10 transcribed tests pass.
Three uncited prose register literals remain undecidable/non-blocking, not reverified.
**Advisory:** logs are ignored; fresh checkouts without those artifacts still fail the historical
record checker. This is a host-local citation repair, not a clean-clone reproducibility fix.

## Fault-diagnosis ruling (Advisor, 2026-10-01) — verbatim

The following is the Advisor response verbatim (apart from this record heading and the
errata appended after it).

FAULT-DIAGNOSIS RULING (F4 drained, no frames), with the W14 clock and the record-accuracy answer. I read the artifacts myself.
A. EVIDENCE CORRECTION (blocking for any record that says "draws/clears 0")
OBSERVED: the only [GPU] executor report lines in the smoke log are 34-43 (draws 0, clears 0, pixels 0, triangles 0). They are printed during xbox_MemoryLayoutInit, BEFORE the guest runs (line 39 is the memory_ready checkpoint). They come from the end-of-run report path, nv2a_pb_scan.c:153-158 → nv2a_pb_exec_report. At runtime that report fires only when RECOMP_NV2A_TRACE is set (every 10 s, xbox_memory_layout.c:1132-1134) or with VERBOSE every 600 flips (nv2a_pb_exec.c:2698); neither was set. Also, submit printing is capped at the first 64 (nv2a_core.c:1731), so submits after #63 are unlogged.
→ This capture has NO end-of-run executor counters. "Draws/clears/pixels 0", and "surface clip 0" if it was taken from the same lines, are UNKNOWN, not observed. Withdraw them; that also withdraws my earlier re-ranking that rested on them. The gpu-report registers are storage only (its own warning).
B. WHAT IS OBSERVED (stacks.txt plus xdk-symbols, read myself)
- Main 6984 at capture: game code sub_00161C20 → 161A90 → 161920 → 1669A0 → XAPILIB XGetDevices (0x001BD5FF, +0xFF) → kernel log fprintf. It is polling input from game code, an ordinary per-frame activity. Its other repeating calls are DirectSoundDoWork (0x0019F260) and the DSOUND critical section (0x0019E438 = DirectSoundEnterCriticalSection, using CS 0x1BA050).
- 0x1BA04C sits in DSOUND's own state: 48 references, 43 `cmp …,0`, 4 `cmp …,edi`, and 1 write, `mov [0x1BA04C],1` at 0x001A2317, inside a small DSOUND method that calls 0x1A1C8A. It is DSOUND library state and NOT a render-arming flag. Drop it as the F4 lead.
- Thread 59696 is inside D3D BlockUntilVerticalBlank (ret 0x0018CE73, waiting in KeWaitForSingleObject), and that return site appears 1,622 times in the log. INFERRED: that thread is woken repeatedly, so vblank delivery (L15) works. Drop the interrupt-delivery hypothesis to low.
- INFERRED, from raw stack words, not unwound: D3D addresses lie in the main thread's live stack region (near SetStateUP, UpdateProjectionViewportTransform, SetScissors). This suggests the main thread recently ran D3D state calls, i.e. it has a render path.
NET: the guest looks like a running game loop (input, audio, D3D state, vblank pacing, continuous GPU submission). The open question is simply whether it renders and presents, and this capture cannot answer it.
C. CHEAPEST NEXT: ONE observation-only run (uses smoke iteration 1 of 2)
Same exploratory profile and ledger set as the last smoke, plus RECOMP_NV2A_TRACE=1 and RECOMP_PB_SCAN=1. Both are observation-only per docs/jsrf-run-profiles.md:225, so the profile is unchanged. Use a 60 s deadline. Read:
(1) the periodic [GPU] reports: draws, clears, pixels, flips and surface over time;
(2) the [PB] method inventory: are FLIP-related NV097 methods present, and their counts;
(3) PCRTC_START over time.
Outcomes:
- draws>0 and flips>0 → it renders; the fault is display/scanout (FB_WINDOW/present path).
- draws>0, flips 0 → Swap is never reached or never emitted; next, static check of the D3DDevice_Swap callers (0x00198F10).
- draws 0 with a full method inventory → a state or setup gap. Bring me the method list.
No code change in this run. If it gives nothing decisive, the second iteration comes back to me first.
D. W14 CLOCK
The 4 h figure is the trigger that obliges a ceiling call. My call at 3h08 was that call, made early at your request. Its output, "continue until 11:00 UTC or 2 smoke iterations", is the governing bound, so no second call is due at 09:57. No contradiction.
Baseline: use the Stop (UTC) column of the last MOVED row as recorded (05:57:40.3). It is conservative by about 92 s against start+duration; immaterial. Do not rewrite historical rows. Record one prospective convention in the ledger header: Stop = started_utc + duration from metadata.json.
E. RECORD ACCURACY (Session line 72-73)
I did NOT explicitly grant full validation concurrent with the Python tests. What I approved: the Session's clarified plan, with full check/test deferred until Python was done. My A′ implementation check then cleared full validation without restating that deferral. Suggested wording: "Full validation (CTest 08:57-08:58 UTC) overlapped the side pytest run (08:54:54-~09:02:01), contrary to the plan's deferral. The Advisor's A′ check cleared full validation without restating it. The side tests read no build artifacts, and the transcribed-values result was re-measured green after the record edits. Deviation recorded; no impact on results." Do not alter the verbatim ruling text.
REVERSED BY: the trace run showing draws>0 with flips>0 (A's withdrawal was then moot, and the fault is display); a full-memory read of the host executor counters contradicting A; or thread 59696's wakeups proving to be timeouts rather than vblank signals. A KeWait return status in the trace would show that, and would re-raise interrupt delivery.

### Errata: the earlier "no frames/flip/present" summary (this record, above)

The ruling's section A is blocking for that summary. Corrected reading: the smoke run's
capacity result stands, but its **render** result is UNKNOWN — the capture contains no
end-of-run executor counters, because `RECOMP_NV2A_TRACE` was unset. The `[GPU]` zero lines
at log 34–43 are printed during `xbox_MemoryLayoutInit` (log line 39 is the `memory_ready`
checkpoint), i.e. before the guest ran, and `submit` printing stops at #63, so later
submissions are unlogged. Zero counters are therefore **not observed** for this run, and the
earlier re-ranking that rested on them is withdrawn. The **historical** report triggers named in
section A (the 10 s `RECOMP_NV2A_TRACE` tick and the VERBOSE "every 600 flips" path,
`nv2a_pb_exec.c:2698`) describe the **pre-A** code and are left as history; the Architecture-A
ruling below **supersedes** them with a single **10 s consumer-lock cadence**.

**Scheduling deviation (Session record, lines 70-73).** Advisor wording, recorded exactly:
"Full validation (CTest 08:57-08:58 UTC) overlapped the side pytest run
(08:54:54-~09:02:01), contrary to the plan's deferral. The Advisor's A′ check cleared full
validation without restating it. The side tests read no build artifacts, and the
transcribed-values result was re-measured green after the record edits. Deviation recorded;
no impact on results." This was a **scheduling deviation** — an actual overlap contrary to the
plan's deferral, not a planned or intentional one and not an explicit grant; the verbatim ruling
text above is unaltered.

**Prospective ledger convention (Advisor, section D).** Stop = `started_utc + duration`. The
Advisor's §D text above is quoted verbatim and unaltered; **Session clarification (2026-10-01):**
its "from `metadata.json`" is shorthand and inaccurate as to the second field — `started_utc` is in
`metadata.json`, but `duration_seconds` is in `result.json`. The convention is therefore
`metadata.started_utc + result.duration_seconds`. Historical rows are not rewritten; the last MOVED
row's recorded `05:57:40.3` stands as the conservative baseline (about 92 s conservative).

## Scope ruling readout — Advisor-attributed summary (2026-10-01; not verbatim)

Advisor-attributed summary of the scope ruling on what the archive and the survey can decide. This
is a summary, **not** a verbatim quotation; the Advisor's own text is the authority. Facts marked
MEASURED were re-verified directly in the toolkit.

- **The `[GPU]` report carries no flip field.** Flips, if any, appear only through the `[PB]` method
  inventory as numeric counts — `0x012C` and `0x0130`, plus `0x0120`/`0x0124`/`0x0128`
  (Advisor OBSERVED). MEASURED: `src/nv2a/nv2a_regs.h:853-857` names `SET_FLIP_READ 0x0120`,
  `SET_FLIP_WRITE 0x0124`, `SET_FLIP_MODULO 0x0128`, `FLIP_INCREMENT_WRITE 0x012C`,
  `FLIP_STALL 0x0130`.
- **Survey-tool name bug (Advisor OBSERVED; MEASURED here).** `src/kernel/nv2a_pb_scan.c:108` labels
  method `0x0130` as `"SET_FLIP_READ"`, but `0x0130` is `NV097_FLIP_STALL`; `SET_FLIP_READ` is
  `0x0120`. A `[PB]` line reading `0x0130 SET_FLIP_READ` therefore names the **stall** method. The
  executor itself is unaffected — `src/kernel/nv2a_pb_exec.c:2679` and `:2702` handle the two methods
  separately (defined at `:187`/`:191`) — so this is a display-name defect in the survey only. **No
  fix inside the current run**; a one-line advisory for later.
- **No flip-absence claim without a positive witness.** The archive cannot assert "no flip was
  submitted". What would license it: `0x1720`/`0x1D70` observed **nonzero**, parse health good, and
  **no table truncation** on the same walker. MEASURED: `0x1D70` is
  `NV097_BACK_END_WRITE_SEMAPHORE_RELEASE` (`nv2a_regs.h:1274`); both `0x1D70` and `0x1720` are in
  the generated `src/nv2a/nv2a_method_table.c` (lines 21, 63).
- **The decision mapping from the fault-diagnosis ruling's outcome table still stands**, normalized:
  draws>0 with flips>0 → it renders and the fault is display/scanout; draws>0 with flips 0 → the Swap
  is **never sent**, which includes but is not limited to "never reached" — a Swap that is reached
  and not emitted stays in scope; draws 0 with a full inventory → a state/setup gap.
- **Scope of the reversal.** It covers the **survey** — what `[PB]` counts can show — and **not** the
  executor's own segments. A flip read/write dump from the executor path is **one** way to establish
  what the executor does with a flip, and it is needed **only if** the survey walker and the executor
  are not the same walker; it is **not an absolute requirement**. Where they are the same walker, a
  **live snapshot accessor** (a new API) answers the same question without a dump.

**Iteration 1 status (2026-10-01; historical — superseded by the Architecture-A root cause below).**
The first observation-only run was recorded as inconclusive at the time: the new log shows no
post-guest `[GPU]` or `[PB]` blocks (Session direct grep), so the counters the run was meant to
produce did not appear, and a worker was diagnosing the callback gate. **That "callback gate" label
and the "no fix authorized" state are superseded by Architecture A** (the instrument is inert under
the installed owner): the run's silence is explained, and the current authorization is the final GO
design recorded below. This paragraph is kept only as the historical reading of that moment.

## Architecture-A root-cause readout — Advisor-attributed summary (2026-10-01; not verbatim)

Advisor-attributed summary of the root cause the observation run exposed. Summary, **not** a
verbatim quotation. Measured and inferred are kept apart deliberately: an UNKNOWN counter must not
be replaced by a measured `0`, and an inference about code reachability must not be presented as a
measurement.

**Root cause (Advisor).** `src/main.c:529` installs the MMIO state owner
(`nv2a_hook_install_aperture`), which at `src/nv2a/nv2a_mmio_hook.c:681` calls
`xbox_Nv2aClaimRegisterOwner()`; that function (`src/kernel/xbox_memory_layout.c:173-179`) clears
`g_nv2a_ack_enabled`. The legacy GPU body of the ack thread
(`xbox_memory_layout.c:1048-1177`) — the busy-bit acknowledgements, the DMA_GET mirroring, the
pushbuffer scan, the executor call and the periodic report — sits **inside** that enabled-check, so
it never runs. The switch is therefore **set but inert**: `RECOMP_PB_EXEC`, `RECOMP_PB_SCAN` and
`RECOMP_NV2A_TRACE` have no effect on the current build. MEASURED in the toolkit source (the three
call sites above, and the enclosing `if (InterlockedCompareExchange(&g_nv2a_ack_enabled, 0, 0))`
at `:1048` closed at `:1177`).

**What is MEASURED in the observation run** `20261001-023335-656-f4-observation-60s`
(game `c01e292`, toolkit `e8a6e03`, `exe_sha256 1ecf8363…` — identical to the capacity smoke's):

- 64 `[PFIFO] submit` lines, all `diag=ok`, GET=PUT through `0x47A84` — the same committed-method
  evidence the capacity smoke produced. Submissions are **not** absent.
- Six `[GPU]` lines, all at log lines 32-38, i.e. **before** `guest_entry` (line 75): they belong to
  `xbox_MemoryLayoutInit` and are not end-of-run executor counters.
- **Zero** `[PB]` lines, and no line after `guest_entry` matching draw/flip/pixel/surface/rasterise.
- `result.json`: `diagnostic_deadline`, exit 3, 62.580312 s, dump OK, 21 threads, 165 named frames,
  1 snapshot, `gpu_report_ok` true. Profile **exploratory** (requested exploratory).

**MEASURED vs INFERRED — the guard on this readout.** The run's **runtime counters are UNKNOWN**:
with the legacy body inert and no post-guest report path, this capture produced no executor
counters, so "draws 0 / flips 0" cannot be read from it and must not be recorded as measured.
Separately, and not a measurement of this run, it is **INFERRED from the code path** that under the
installed owner the executor is never called and therefore no render work is executed. That is a
reachability inference, not an observation of zero rendering.

**Timing (metadata authoritative).** `metadata.json` `started_utc = 2026-10-01T09:33:36.447445+00:00`
plus `result.json` `duration_seconds = 62.580312` gives **09:34:39.027757** — the Stop value used for
this run's ledger row. A worker-reported wall interval (09:33:35 → 09:34:38) is about a second
earlier at both ends and is **not** the Stop source.

**Ruling A (Advisor) — what the fix must satisfy.**
- The owner's NV097-to-`PB_EXEC` path must be reached under the owner: **no second walk and no second
  GET**.
- A **rejection must stay atomic** — reject without executing.
- Fixtures must pin **clear/flip counts**.
- The run must produce a **post-guest periodic `[GPU]` report** as positive proof that the path is
  live.
- L18's text is updated **at the coordinated cross-repository checkpoint** with the accepted toolkit
  code — the toolkit commit's SHA recorded in the game's ledger record. This is **not** literally one
  git commit: the two live in **separate repositories**, so "same commit" means a coordinated
  checkpoint, not a single shared commit object. **Advisor ACK (2026-10-01) binding on this point:**
  "same commit" = the same checkpoint — toolkit code commit plus the game ledger record citing that
  toolkit SHA, **pushed toolkit first**. **Until the code is accepted, L18 keeps its old entry**; it
  is not marked "pending". No speculative acceptance.
- W14 is extended **until A's first smoke plus one diagnostic**; the inert switch is itself a **new
  instrument defect**.
- **No more inert reruns**: running the current build again cannot answer the question.

**Ledger row for this run.** Recorded as an exploratory, **NOMOVE** row in
`docs/reviews/strict-horizon-ledger.md` — a contrast observation, not strict progress and not a
horizon move. The prior run's `L16`/`L18` entries remain **set but inert** for this build.

**Approved design preflight — FINAL GO (Advisor; attributed summary, not a quotation).** This
supersedes the earlier preflight shape below and resolves its two pending items (order and
test-target links).

**Order (final):** capture per entry class → bindings → `action_commit` → consumer (ordered all
committed classes; the kernel wrapper executes the NV097 subset) → last method → GET.

**Interface refinement — Advisor APPROVED (attributed; amends the seam description above).** The
seam takes **four arguments — `(subch, class_id, method, param)`** — and **all committed entries are
passed to the core callback**; the **kernel wrapper** is what filters to NV097 and keeps the skip
count. This **replaces the earlier 3-argument, NV097-only-core-consumer** shape: the policy-free
core is the better factoring, because the core receives **all entry classes** — the executor's own
consumer is an NV097 **subset** — so it **preserves the class information without policy filtering**.
(The earlier phrasing "the core sees exactly what the executor sees" was misleading and is
superseded: the core sees *more* classes than the executor consumes, deliberately.) Conditions: the
staged class is **captured per entry, not taken from the final binding**; **order and
atomicity are unchanged**, with the callback still firing **after `action_commit`** and **inside**
the ok path. Tests must show that a **non-NV097 entry leaves the `EXEC` counters unchanged** while
the **wrapper's skip count increments**; the core's local-callback tests must check the **correct
class, including across a rebind**; and the **skip count is read for the report under the same
PFIFO lock**. This is **not a new shortcut and adds no ledger class entry**.

- **Runtime callback seam.** A **core static function plus a setter** — no environment variable and
  **no weak symbol**. The point of "no `extern`" is that the **core holds no `extern` reference to
  the executor**: the **kernel does register the seam** (that is how it is wired), so this must not
  be read as "no kernel registration". Registration happens **before the guest starts**; `PB_EXEC`
  presence is the only trigger.
- **Integration and test targets (REAL, not a new target).** Use the **existing HAL target links
  across the whole toolkit**; the fallback is a **dedicated real-exec minimal stub target** only if
  the HAL cannot be a fixture. The earlier "A2 new targets" proposal is **superseded**.
- **Standalone core tests** cover **order, atomicity and rebind** with a local callback.
- **Owner lock: a free getter on the existing active flag** — not a submission snapshot and not
  lock coupling. The legacy guard **logs once and skips `EXEC`**.
- **Report only under the consumer lock at a 10 s cadence.**
- **Flip accounting.** The accessor adds a **NEW `flip_stalls` counter**. `0x0130`
  (`NV097_FLIP_STALL`) is a **completed swap** and does call `FrameCounterFlip`, but it **does not
  increment the existing `s_gpu.flips`** (it does not advance `flip_write`); the separate
  `flip_stalls` counter tracks those completed stalls, so a run can tell the two swap paths apart.
  **`0x012C` (`FLIP_INCREMENT_WRITE`) is the `s_gpu.flips++`.** MEASURED at HEAD: `nv2a_pb_exec.c:2695`
  is the only `s_gpu.flips++`, and `:2708` is the `FrameCounterFlip()` call in the `FLIP_STALL` case.
- **Game-side registrant audit is mandatory:** **no blocking callbacks under the lock.**
- **L18's edit lands with the accepted code checkpoint**, not before.

*Superseded earlier shape (kept for history):* one stage per entry class, 16 bytes per entry,
explicitly not a final binding, feeding sink/capture/hook with a midstream-rebind fixture; only
NV097 entries ordered with skipped non-NV097 counted and reported (the skip now lives in the
**kernel wrapper**, which passes all committed entries to a four-argument core callback — see the
interface refinement above); a single caller on the PFIFO lock
with the REPORT on the same OWNER submit path at 10 s — `PB_EXEC` never the ack worker; the legacy
SCAN guard skips when the owner is active and logs once; the owner lock being BMPIO is acceptable
for exploratory scope; the frame counter/flip is external-callback-only and takes no NV2A lock; a
late CLEAR — **a CLEAR preceding a later rejected word** — leaves **no executor effects**: the
earlier CLEAR must not take effect when a subsequent word in the same stream is rejected, because
rejection rejects the **whole stream**. (The earlier wording "a late CLEAR is rejected" was
ambiguous and is superseded by this one.) No more inert reruns.

**Readout rule for counter reports (Advisor-attributed, 2026-10-01 10:02 UTC).** A post-`guest_entry`
`[GPU]` report is a **LOWER BOUND as of its own line**, never a capture total: quote that line plus the
later `[PFIFO] submit`/commit line if one exists, and count zeros **only up to that line**. **If** the
frames question is asked **and** the last report predates the final ~10 s, the **frame tail is
UNKNOWN**. A second iteration may add an end-of-run report trigger **only after a new Advisor consult
first**. **An immediate first NV097 report would prove** the consumer was called, before later methods.

**Fixture ruling — FrameCounterFlip call count (Advisor-attributed summary, 2026-10-01).** The A2
fixture asserts **`FrameCounterFlip` is called once on `0x0130`** — a **call count**, not a guest delta;
the parent's earlier phrasing was overspecified. **No full memory-layout fixture and no dedicated stub
target.** Instead: an atomic `InterlockedIncrement` as the **first statement** in `FrameCounterFlip`,
plus a **read-only `uint32 xbox_Nv2aFrameCounterFlipCalls` accessor**. The **HAL delta is 1 on an
`0x0130` stream and 0 on a rejected CLEAR**. The Advisor **observed no `FrameCounter` registration in
game `src/`**; a **full outside-`src`/generated audit is pending** (worker), so **do not assert there is
none project-wide until confirmed**. **Reverse if any game registration is found**: the fixture then
becomes a **guest delta**, mapped later in its own packet. The **counter walk is deliberately out of
scope** — JSRF registers none if confirmed; the **earlier counter-delta Session request is superseded**,
not a weakened criterion. No new history section.

**Architecture A — ACCEPTED / GO (Advisor, 2026-10-01; attributed summary).** The Advisor read the
nine source files itself (`+329/−10`) and **accepted** the change: **final validation green** — core
**5/5 in 2.34 s**, **30 lifter unittests**, game **29/29 CTest in 28.81 s**, all checks pass, and the
code is **conformant to the approved design**. **Committed: toolkit `a71f9374ddb2a6685b790493855c835842228212`**
(`fix(nv2a): execute owner-committed methods through the legacy renderer`, 9 files, +329/−10, tree
clean). The **actual `FrameCounter` audit found 0 host registrants** — evidence cite **"Advisor grep
2026-10-01"**: **0 game host registrations**, appearing in a **definition/header only** in the toolkit
— which is **different from** the worker's backend-only audit; the worker's ignored-audit item stays
**pending** and **does not block**. **Commit and push before the smoke** — a clean pair is required,
and the smoke history is not to be invented. Records consequence: the plan/TR move from **in progress
→ implemented and accepted**, with **runtime still pending** and **no frames claim**.

**Multi-target cause — conditional, not attributed (2026-10-01).** The multi-target build outcome is
recorded as a **conditional**: it reproduces **only if** the exact **two rerun commands** produce
`/−1` then `0` in the artifact; **otherwise it is UNKNOWN / non-reproduced**. Final validation is
**green and no GO block** — the worker is producing the artifact. **No extra durable cause
attribution is supported** and none is recorded.

## Session evidence qualification

The submission dimensions above are inherited measurements from the prior run's plan/TR, not a fresh Advisor decode. The initial worker negative claim that Mercenaries had no NV2A was withdrawn after discovering truncated tree coverage; corrected direct-source findings were supplied before this ruling. Primary reference URLs: https://github.com/xemu-project/xemu/blob/f9b14039e5bb56ae2d8f028e31e7cc19f13f7e12/hw/xbox/nv2a/pfifo.c and https://github.com/KraftMacAndChee/Mercenaries-Recompiled/blob/c978ee754e319c8593ee2260ac37b8262628f7c7/src/nv2a/nv2a_core.c . No hardware-fidelity claim is made by preserving local atomicity.
