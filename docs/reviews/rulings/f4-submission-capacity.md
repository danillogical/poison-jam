# F4 submission capacity — Advisor ruling

2026-10-01. Persistent Advisor child `4e6d87e1-f748-48b3-a0a4-a6e5728bfeee`, provider `claude`, model `claude-opus-5-5`, effort `high`; startup continuity probe PASS. Owner-directed F4 chore, no packet promoted. The following is the Advisor response verbatim (apart from this record heading).

## Historical status

This record preserves historical decisions and their reversal conditions; they are not open tasks. The owner has parked the later F5 observer line without retry; that park prevails over older observer next-step wording. No new fixture, unit or history scrub is authorized by metadata cleanup.

## Hazards carried from the superseded capacity reading

The stale 256-cap comment concealed the actual 1024-entry limit, and enlarging only the sink would leave the second, local staged-array cap. This ruling carries those hazards explicitly: both capacities follow the existing word budget. Incremental commit would alter rejection atomicity, carry/hold, action commit and semaphore rollback; keeping whole-submission atomicity is a deliberate model approximation, not a hardware-fidelity claim. These hazards and tradeoffs are stated in the response below; no new conclusion is added.

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
**kernel wrapper**; the **core passes all committed entries to a four-argument callback** — see the
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

**F4 — MET, exploratory (Advisor-attributed summary, 2026-10-01; not verbatim).** Frames exist. The
60 s run `20261001-033129-276-f4-a-smoke-60s` (game `f676b1c` / toolkit `a71f937`) produced a
**coherent guest image** — the "Presented by SEGA" title card — so **F4 is met under the exploratory
profile**, evidenced by that guest frame. This is **not** a milestone acceptance: acceptance is
carried with the **F6 milestone Review**. **W14 was reset at 10:32:32 UTC** on the first critical-path
frame finding; the old extension is consumed and closed.

**180 s observation run `20261001-033805-242-f5-sequence-180s` — OBSERVED, no F5 causal ruling yet.**
Same pair and profile as the 60 s run, plus the observation `RECOMP_FB_DUMP` (prefix
`logs/workers/f4a-sequence/f`). Exact identity: `started_utc 2026-10-01T10:38:05.948693+00:00`,
`duration_seconds 182.688193`, Stop **10:41:08.636886**; `diagnostic_deadline`, exit 3, dump and GPU
report OK, 21 threads, 179 named frames, **exploratory** (requested exploratory, `GPU_ACK` default).
Mapping gate: **1 match, 0 mismatch**.

- **25 BMP dumps, 2 distinct hashes, in ordered blocks:** `f000–f007` are one byte-identical hash
  (the black frame), `f008–f024` are another (the SEGA frame). Within these **files** there is no
  alternation. *(This is a statement about the dumped files only — it is **not** a global claim that
  the guest never animates; the window one-shot below shows the window at a different instant.)*
- The **8-then-17 split is attributed to the two dump writers** — the executor's draw path
  (`nv2a_pb_exec.c:2206-2209`, capped by `FB_DUMP_AFTER_DRAW` = 8 at `:2000`) and the periodic report
  (`:3124`, one picture per report) — with the once-only notice at `if (seq == 1)` (`:962-964`).
  **INFERRED, not directly logged:** there is no per-dump log line, so the split is matched by
  **code plus file mtimes**, not by a per-file log witness. The **mtimes themselves are OBSERVED**
  (10:38:09–10:38:10 for the first block, 10:38:20 → 10:41:01 at a 10 s cadence for the second), and
  the run logged 19 reports for 25 files.
- **Provenance of the BMP pixels (verified in source):** the executor dump reads a **guest DRAW
  surface** via `mem + dma_resolve(s_gpu.drawn_offset ? drawn_offset : color_offset)` with clip and
  pitch (`:937-940`); it **does not touch the window or `AVSetPCRTC`**. The once-only notice prints
  **`color_offset`**, which may differ from the `drawn_offset` actually read, so **the notice's
  address is not a reliable source for a given file's contents** (reported for later files).
- **A third, different artifact — the window dump:** the extensionless file `…\f` (921,654 B, sha256
  `9E7541A8ABC68B31FAA3F4553867DCD70A7364EB5FC35B60B7055344E5EE7401`, mtime **10:38:19**, "BM")
  comes from `xbox_FramebufferDumpBmp` in **`src/video/fb_present.c`** (`:212`, guard `!s_rgb ||
  !s_fb_va` at `:219-220`), called one-shot at `frames == 600` (`:342`) with the prefix passed
  **raw** — hence no `.bmp` suffix, unlike the executor's `"%s%03d.bmp"` at `nv2a_pb_exec.c:919`.
  **Accepted by the F5 ruling as exactly "one-shot window frame at 10:38:19.247 (`frames==600`,
  `fb_present.c:341`), SEGA", with nothing claimed about the window after that** — so it does **not**
  establish a static window over the run.
- **The window dump is the stronger present/scanout witness** — the code's own note (`fb_present.c:322-332`)
  is that the window "follows the address AvSetDisplayMode gave, which is what the CRTC scans and
  therefore what a person sees", while the executor dump "follows its draw surface", and with double
  buffering those differ. The **F5 ruling accepts the one-shot frame** as stated above; the **present
  call path remains INFERRED** (no log witness, and not needed for F4).
- **Counts are LOWER BOUNDS, and the two runs must not be mixed.** For **this 180 s run**: the last
  report is `L205921–205927` (**clears 2943, draws 2943, flips 987, stalls 987**, 14 non-NV097 skips,
  4899 triangles, 394,019,568 pixels), and the `[GPU] clear #N` stream continues past it to
  **`clear #3000` at L207880** of 209257 — so activity continued after the last report and **no
  end-of-run totals exist**. Separately, in the **60 s run** (`20261001-033129-276-f4-a-smoke-60s`,
  119913 lines) the last report is `L115261–115267` (**clears 840, draws 840, flips 286, stalls 286**)
  with **`clear #900` at L117696**. The `#900`/L117696 figures belong to the **60 s** run and must
  **not** be cited as the 180 s run's post-report activity.
- **No overclaim:** no deadline-success, no liveness from the deadline, no strict horizon claim, and
  no F5 causal ruling. `RAW BIN` figures (the Advisor's own MATCH-gate export, VA `0x8011C000`,
  RGB565, 614400 bytes, sha `95CE9EC0…`) are **Advisor-attributed**, not re-measured here; the `.bin`
  is not present under `logs/workers/f4a-frames/`.

## Appendix — Advisor F4 runtime ruling, VERBATIM

Advisor child `4e6d87e1-f748-48b3-a0a4-a6e5728bfeee`; provider `claude`; model `claude-opus-5-5`;
effort `high`; continuability probe satisfied (same child confirmed continuity after these messages).
The three sections below are the **exact received Advisor text**, copied from the Session's context and
preserved verbatim. Nothing here is reconstructed or paraphrased; the summary above is the attributed
reading, this appendix is the authority. **No images or raw binaries are embedded** — only references
and provenance. The final section is explicitly **not** Advisor text.

### F4 RESULT RULING

F4 RESULT RULING: FRAMES EXIST. The guest renders the "Presented by SEGA" logo. W14 is reset. Next: F5/F6.
OBSERVED (I extracted it myself):
- check-dump-mapping: matches 1 / mismatch 0.
- I exported the final [GPU] block's draw surface 0x8011C000 (640x480, pitch 1280, so RGB565) from process.dmp (inspect-jsrf memory, 614400 B, sha256 95CE9EC0…) and converted it to PNG. It shows a clean, correctly coloured "Presented by SEGA®" logo on black. Saved as scratch: logs/workers/f4a-frames/final-surface-0x8011C000.png, sha256 396DC92B….
- The clear surface 0x80084000 is byte-identical. INFERRED: the final pass copies the back buffer, which the [TEXUSE] 0x80084000 640x480 lin, 286 batches, suggests.
- Log 115261-115267 (lower bound at that line): draws 840, textured 840, tris 1394, flips 286, flip stalls 286, 14 non-NV097 skipped, 95,949 unhandled methods (242 distinct).
- 0 invalid ICALL/exception/ABI/[UNIMPL]/budget/sink/unsupported.
CLASSIFICATION:
- F4 "frames" is MET as an exploratory bare-minimum result. Ledger IDs: L14, L15, L17, L18 (via the commit consumer, toolkit a71f937), L20-L25, L39, L40; L16 set but inert under the owner; L19 dormant.
- No strict or fidelity claim. The unhandled-method count is advisory: a picture is correct, but effects may be missing.
- Draws rising across successive reports (147 to 840) plus a coherent image is the semantic liveness witness the profiles doc asks for. diagnostic_deadline alone was not; your rejection of the worker's "guest live" claim was right.
- Record F4 in the plan/TR with this run, the PNG (re-derivable by the command above), and the ledger list. It reaches acceptance with the F6 milestone review per plan:842, not separately.
W14: the line produced a critical-path finding (first rendered frame), so the ceiling clock resets to 10:32:32 UTC under the standard 3-packet/4 h rule. The extension I granted is consumed and closed.
NEXT (cheapest, one run, observation-only addition):
- Same profile and pair, plus RECOMP_FB_DUMP=<run-dir>\fb\f. It is observation-only per run-profiles:227, and dump_surface_bmp runs on each 10 s report.
- Duration: 180 s.
- Question: does the dumped sequence progress past the SEGA logo (more logos, the Sofdec intro, the title screen), or freeze on one image?
- Readout: list each BMP with its log line and draws/flips at that report, then view them. If the image stays constant while draws rise, the guest is likely in a movie/logo wait. That is F5: inspect the Sofdec/XMV path, with the skip-or-patch option per plan:840. If it reaches the title screen, capture the frame for F6 and take the xemu screenshot of the same screen (T3 oracle) for the eye comparison.
- If FB_DUMP produces nothing (clip/format guard at nv2a_pb_exec.c:912), the fallback is the dump-extraction method above at the deadline only. No code change.
- ALSO advisory: 286 flips in ~62 s is roughly 4.6 fps. INFERRED: the executor is slow. That is not blocking for the bare minimum, but if the run times out before the title screen, use a longer deadline before any optimisation.
REVERSED BY: the 180 s sequence showing a corrupted or wrong image on later frames (format assumption wrong), or the logo being produced by a fixture or static path rather than guest draws (draws=0 at a report).

### IMAGE EVIDENCE clarification

IMAGE EVIDENCE: the actual image already exists, so no inference is needed. Please read my previous message ("F4 RESULT RULING: FRAMES EXIST").
1. OBSERVED, not inferred: I extracted guest VA 0x8011C000 (the final report's draw surface, 640x480, pitch 1280, RGB565) from this run's process.dmp after a MATCH dump gate. I converted it and viewed it: "Presented by SEGA®", clean, correctly coloured. Artifact: logs/workers/f4a-frames/final-surface-0x8011C000.png (sha256 396DC92B…); raw bin sha256 95CE9EC0…. Reproduce with `python -X utf8 scripts\inspect-jsrf.py memory <run> 0x8011C000 614400 --out x.bin`, then an RGB565→PNG conversion (a trivial script; let the worker write it under logs/workers).
2. Correct labels:
   - "present / FB window was called": INFERRED, no log witness, and it is not needed for F4.
   - "the guest rendered a frame into its surface": OBSERVED from the image. The image is the F4 evidence.
   - The draw coordinates x −0.5..160, y −0.5..165 are a lower-bound range from the report. They are consistent with screen-space quads in a 2D logo pass, not a "quarter video", given the image. Whether it is a movie frame is answered by the image: it is a static logo.
3. Clear #900 at line 117696 (OBSERVED; #700 at 108751, #800 at 113318): accepted. The clear counter is DEMONSTRATED at ≥900 by that line, the log ends at 119913, and full end counts are absent. Record "≥900 clears by log line 117696" as a demonstrated lower bound.
4. Method inventory: UNKNOWN under the owner. Correct, as ruled.
5. Next step unchanged: the 180 s run with RECOMP_FB_DUMP for an image sequence. If the dump guard yields nothing, apply the dump-extraction method once at the deadline. That is the cheapest diagnostic and needs no code.

### External prefix permission

ACK. Prefix outside the run directory: fine, because the run ID is generated at launch. Conditions:
- Put it under logs/ (gitignored), e.g. logs/workers/f4a-fbdump-<UTC>/f.
- Have the record name that path, the env value used, and each BMP's sha256 and mtime.
- Map each BMP to the [GPU] report log line that produced it; dump_surface_bmp runs inside the report, so the order matches.
- Keep the BMPs out of the game repo's tracked paths. They are rendered from proprietary assets.
Waiting for the 180 s result as the F5 consult input.

### Session qualification after the 180 s run (NOT Advisor text)

The worker and Session found two executor dump sites (eight early draw-path dumps and seventeen report-cadence dumps). Only the first executor dump is logged. Exact per-BMP report-line correspondence is therefore not observed. These findings were supplied to the Advisor for the pending F5 consult; no per-file exact-line claim is accepted. The BMP sequence is guest draw-surface evidence, not window-buffer evidence. A separate extensionless file is a one-shot window-buffer capture at window-thread frame 600; it shows SEGA at that instant only.

**Prefix mapping, CORRECTED (Session, 2026-10-01 — not Advisor text, and superseding the Advisor's
"the order matches" expectation above).** The Advisor's prefix condition asked that each BMP map to the
report line that produced it. That mapping does **not** hold one-to-one here: the executor has **two**
dump sites — the draw path (`nv2a_pb_exec.c:2206-2209`, capped at 8) and the periodic report
(`:3124`) — and only the first dump is logged (`if (seq == 1)`, `:962-964`). Measured outcome: 25 BMPs
(8 black, then 17 SEGA) against 19 logged reports, with the 8-then-17 split matching the two writers
by timestamp (10:38:09–10:38:10 vs 10:38:20→10:41:01 at 10 s). **Per-file report-line correspondence is
INFERRED, not observed.** A pending F5 ruling will supersede this appendix section.
**[RESOLVED 2026-10-01: the F5 ruling has since arrived and is recorded verbatim in the F5 appendix
below. The sentence above is left as written, as the state at the time of the 180 s qualification.
The F5 ruling confirms the per-file mapping point and rejects the word "stop" for the frozen sample —
see the F5 appendix §1 and §2.]**

## Appendix — Advisor F5 cache-fill ruling, VERBATIM

Advisor child `4e6d87e1-f748-48b3-a0a4-a6e5728bfeee`; provider `claude`; model `claude-opus-5-5`;
effort `high`; continuability probe satisfied. The text below is the **exact received Advisor response**,
copied directly from the Session's context and preserved verbatim. Nothing is reconstructed or
paraphrased. **No images or raw binaries are embedded** — references and provenance only.

F5 CONSULT RULING: the logo phase is not a stall. The guest is doing its first-boot HDD cache fill, slowly. The next step is ONE long observation run (no code). Record F4 first.

1. WORDING CORRECTIONS
- Window: accepted as "one-shot window frame at 10:38:19.247 (frames==600, fb_present.c:341), SEGA". Nothing is claimed about the window after that.
- Brief §4 "stop is inside the submission walk": REJECT the word "stop". A single frozen sample in submit_read_word's VirtualQuery is a capture-time location, and the guest is live (below). Advisory: submit_read_word calls VirtualQuery once per pushbuffer word (nv2a_core.c), which is a plausible per-frame cost. It is a perf lead only, not a defect.

2. NEW EVIDENCE (OBSERVED, I read it myself from the 180 s run)
- The guest is filling its HDD cache from the DVD. 360 [PATH] opens of `\Device\Harddisk0\Partition5\Media\…~` (temp names), 184 distinct, steadily: 20-33 per 10 s report interval, through the end of the log (last: line 209119, e010.bin).
- save-root\Cache\Media\Cache holds Cache00-02.tbl, DmCache00-02.tbl, and JSRF_CACHE_COMPLETE00.CMP written at 10:39:41 (~96 s in), with Cache02 started at 10:39:42. Cache02.tbl names Cache03.tbl/DmCache03.tbl next. DmCache00.tbl lists the Mission/Event files being copied. Cache payload: 182 small files, 67.9 MB.
- Copy rate is ~1.0-1.3 distinct files/s in EVERY recent run, with or without the executor (48 s: 64 files; 60 s: 82 and 80; 180 s: 184). So the executor does not limit it.
- INFERRED: the "Presented by SEGA" screen covers the first-boot cache fill, which goes in stages (00 complete, then 01-03), and the logo stays until the fill finishes. UNCERTAIN: whether JSRF really gates the logo on fill completion. A long run settles it.
- Every run starts with an EMPTY disposable save-root (run-jsrf.py:306-313 refuses a non-empty one), so every run repeats the first-boot fill. A real console fills once.

3. NEXT (cheapest discriminator, one run, no code)
- 600 s, same clean pair and profile.
- Add RECOMP_FB_DUMP=<logs/workers/...>/f and RECOMP_FB_WINDOW_DUMP_EVERY=600 (window frame about every 10 s). Both are observation-only per run-profiles:227/230.
- Check disk ≥ 15 GB first: each run's save-root is about 5.3 GB of partition images.
- Readout, from the run's own artifacts:
  (a) per-10 s: draws/flips, BMP hash, window-dump hash;
  (b) mtimes of the JSRF_CACHE_COMPLETE*.CMP and Cache0N.tbl files;
  (c) the cached-file count per interval.
- Decision rows:
  - The image changes after a CACHE_COMPLETE (Smilebit logo, movie or title) → the logo is fill-gated; progress resumes. Then F5 is a movie check, and see 4.
  - The cache keeps growing and the image is unchanged at 600 s → fill-bound but longer than 600 s. Estimate the remaining fill from the DmCache tables, then decide between a longer run and 4.
  - The cache stops growing and the image is unchanged → a real stall. Bring the waits (stacks) to me.

4. PREPARED, NOT AUTHORISED YET: a seeded cache.
If the fill gates the logo, the path of least resistance is to start runs from a save-root whose cache was filled by a completed earlier run. That is the steady state of a real console, not a bypass. It needs:
- an explicit runner option (owner-visible) instead of the empty-root refusal;
- the seed's source run recorded in metadata;
- a ledger entry ("Initial disk state: HDD cache pre-filled from run X");
- the save/resume tests kept on empty roots.
I will rule on it after the 600 s readout.

5. RECORDS: commit F4 (records only) before the run, so the run identity is clean.

W14: the clock reset at 10:32:32 still governs. One 600 s run is well inside it.

REVERSED BY:
- the 600 s run showing the image change while no cache completion occurred (gate is not the fill);
- the cache rate collapsing to 0 with the image static (a stall);
- the owner's xemu (empty HDD cache) showing the SEGA logo for only a few seconds on first boot. In that case the fill is not what gates the logo, and the slow per-frame rate is the lead.

## Appendix — Advisor duration-cap ruling, VERBATIM (replacement child)

Replacement Persistent Advisor child `c0ecc88b-756e-4256-9852-1bd8b7398735`; `claude/claude-opus-5-5`
@ `high`; created in the same session under **§4.4 unreliable-state replacement**, **not a fallback**,
same exact route as the original (`4e6d87e1…`, which had become unreliable — successive turns failed
without a closing message despite a watcher ACK; cause transport UNKNOWN). The replacement was
**briefed from files, not seeded with conversation**. **Continuity: first fact read received, and the
second-turn marker subsequently PASS** — the marker `ADVISOR-RECOVERY-1112-CAP` was returned without
being repeated in the second prompt. **Route and `high` effort are established by the pinned parent
spawn record, not by the child's self-report** — the child correctly declined to self-verify its own
effort. Text below is the **exact agent message**, preserved verbatim; no images or binaries are
embedded.

ADVISOR-RECOVERY-1112-CAP (replacement Persistent Advisor, claude/claude-opus-5-5). Probe fact from cap brief: exact LAUNCH_UTC=2026-10-01T11:01:26Z (the 10:56:32Z figure was a pre-launch estimate, withdrawn).
RULING: Raise BOTH bounds to 600 — run-jsrf.py:218-219 and jsrf_run_profile.py:668 — preferably as one shared constant (e.g. MAX_RUN_SECONDS in jsrf_run_profile, used by the runner) so they cannot drift again. Add boundary regressions at both sites: 600 accept, 601 reject, 0 reject (runner parse + profile classifier → UNKNOWN on 601/0); existing profile tests unchanged and green. Commit (clean tree) before the run, then do the ONE 600 s exploratory run exactly as the F5 ruling §3 specifies (no seed). Do not use 300 s as a substitute: two 300 s fresh roots each restart the first-boot fill, so they cannot show the >300 s decision rows.
BASIS (observed): I read both sites; they are the only 300 caps in scripts/. Collector takes argv[1]*1000 ms with no cap (collect.c:2696); collector.wait = seconds+45 (run-jsrf.py:381) scales. build-identity.py fingerprints only .c/.h/.json/.txt/.cmake, so the exe SHA stays verified; the runner change is captured by the archived project revision/patch identity. Raising only the launcher would make the profile UNKNOWN (inferred from :668), so both must move.
BASIS (inferred): this is harness bound-widening, not guest/runtime code; every guard (identity verify, empty save-root, disk gate, archive, classification) is unchanged, so F5's "no code" intent (no guest-behaviour change) holds.
RECORD IN: f4-submission-capacity.md (one line after the F5 appendix) + plan F5 entry; cite the commit.
REVERSED BY: any other 300 s/duration bound found downstream (e.g. a checker or the archive path rejecting >300), the regressions failing, or the 600 s run failing for a duration-related reason — then stop and bring it back; or owner direction to use 300 s.

**One-line cap record (cite the commit):** the duration caps are **implemented** as **one shared
constant** so they cannot drift — `MAX_RUN_SECONDS = 600` in `scripts/jsrf_run_profile.py` (bound at
**`:676`**), imported and used by `scripts/run-jsrf.py` (bound and message at **`:219-220`**); the
earlier `:218-219` / `:668` citations above are the **Advisor's verbatim text and are unchanged**.
Boundary behaviour measured directly: `1 -> 1`, `300 -> 300`, `600 -> 600`, and `601`/`0`/`-5` ->
`SystemExit 2`.
**RED** (against the unmodified 300 caps, focused `DurationCapTests`): **4 failed, 2 passed,
6 subtests passed**, exit 1 — 600 rejected at parse, `MAX_RUN_SECONDS` absent, and archives for
`strict`/`exploratory` at `seconds=600` classified UNKNOWN (capture `logs/workers/f5-cap-RED.txt`).
**GREEN** (after the shared constant): focused **4 passed, 10 subtests passed**; full
`tests/test_run_profiles.py` **38 passed, 100 subtests passed** in **1.29 s**; `just check` **exit 0**
("check: all checkers passed"); `build-identity.py verify` **rc = 0** — scripts are outside the
fingerprint, so the **exe is unchanged**. Guards preserved: identity verify, empty save-root, disk
gate, archive, classification, and the negative/`bool` type guard. The code commit is the harness
commit whose message is `harness: permit bounded 600-second cache observations` (its own SHA is
recorded by the commit itself and by the run that follows, not restated here, to avoid a
self-referential cycle).

**Failed 600 s launch — a HARNESS LAUNCH FACT, not a runtime or guest fact.** The attempt **launched
at `2026-10-01T11:01:26Z` and failed with rc 2 on the dual 300 s cap**. Precisely: this was a
**harness/launcher failure** — **no game run occurred** and **no run artifact exists**; only
**launch logs** exist. It is therefore recorded as a **launch failure**, explicitly **not** a
strict-horizon entry and **not** a guest/runtime observation. (The `10:56:32Z` figure seen earlier was
a **pre-launch estimate, withdrawn**.)

## Appendix — Advisor window-watcher ruling, VERBATIM (original child)

Advisor child `4e6d87e1-f748-48b3-a0a4-a6e5728bfeee`; `claude` / `claude-opus-5-5` / `high`;
continuability probe satisfied and continuity re-ACKed. **Received after the F4 runtime records push
`e991fd22311b7fe64edb47e5971b0aaa55954a7c`.** Verbatim text follows.

ACK continuity: same child, 4e6d87e1. I never saw a provider error and the earlier turns produced no error text, so record the cause as transport UNKNOWN. This message is sent without other tool calls.
WATCHER RULING: APPROVED, conditioned on the fixed version (immutable validated bytes written with 'xb', captured stat, thread-failure exit 3) and its 7 passing self-tests, as you describe. I have not re-read the helper. Your direct read caught the second-read race, and the fix closes it by construction, which is enough for an observation tool.
Conditions:
1. Read-only on the game. It writes only under ignored logs/workers. It never opens the window file for write, never takes a lock and never signals the game. Runtime is bounded at ≤650 s.
2. Missed overwrites are expected, because the window rewrites the same path. The manifest is therefore a SAMPLE of window frames, not a complete series. Label it that way: a gap or skip means "not captured", never "no frame".
3. Every captured window BMP is cross-checked against the draw-surface BMP sequence (f000…) by time. If the two disagree (window ≠ draw surface at similar times), that is a finding to bring to me, not something to resolve.
4. Launch the 600 s run only after the game push of e991fd2 completes, with the same clean pair. No seed (seeding is not authorised).
5. The readout follows my F5 ruling §3: per-interval draws/flips, BMP and window hashes, CACHE_COMPLETE/Cache0N.tbl mtimes, and the cached-file count. Apply the decision rows exactly.
REVERSED BY: the watcher's manifest disagreeing with the files on disk after the run (hash mismatch), in which case its series is void and only the draw-surface BMPs count.

**Conditions carried as binding (all three the Session must apply):** **missed overwrites are
expected** — the manifest is a **SAMPLE**, so a gap means "not captured", **never** "no frame";
**near-time cross-check** every captured window BMP against the draw-surface sequence by time, and
bring any disagreement to the Advisor rather than resolving it; and a **hash mismatch voids** the
watcher series, leaving only the draw-surface BMPs.

## Appendix — Advisor save-root census ruling, VERBATIM

Persistent Advisor child `c0ecc88b-756e-4256-9852-1bd8b7398735`; `claude`/`claude-opus-5-5` @ `high`
(**parent-pinned spawn**; the §4.4 continuity marker `ADVISOR-RECOVERY-1112-CAP` belongs to the
**recovery + duration-cap consults only** — it is not a per-consult property and implies no fresh
continuity check). Received after cap commit `c4bcd2b…`, before the game push/launch. Exact received
agent message below, preserved verbatim.

ADVISOR QUICK RULING (save-root census):
RULING: AUTHORIZED under the existing F5 observation approval. Section 3(b)/(c) of the F5 ruling already asks for these readouts, and this is only a way to collect them. It is not a run-profile override and not a ledger shortcut.
Conditions: run it on the host, outside the guest, read-only. List the directory and stat files only: no opening or hashing of file contents, and never touch the Partition*.img files. Identify the save-root by the exact new run label; if there is no match or more than one, stop and fail. Write the JSONL outside the save-root, under logs/workers. Sample every 10 s and stop at the runner's exit or at 650 s. Make no change to the game's environment or arguments. Record the observer's source SHA in the run record; if you keep it, it belongs in scripts/, not logs/. If the observer fails, report it, but the run's own result.json still governs.
Not a new commit gate: as a scratch helper outside the game's code it doesn't change the build identity. Keep the post-run final mtimes/file counts from the save-root as the authoritative cross-check, and the [PATH]-open counts as a secondary, inferred measure.
BASIS (observed): run-jsrf.py:307-331 creates <run_dir>/save-root as an empty directory and passes it with --save-root, so it can be found on the host and holds the cache files listed in the F5 ruling. (inferred) Listing and stat on Windows uses attribute-only access and does not block the guest's writes.
REVERSED BY: any sign the observer affects the run (sharing/lock errors in jsrf_run.log, changes in copy rate compared with the earlier runs' ~1.0-1.3 files/s), or a census that disagrees with the post-run save-root state. If that happens, fall back to the final mtimes plus [PATH]-open counts.

**Scope note (Session):** this ruling authorizes **list + stat only**. The later **tiny-table
prefix/tail byte read** was a **separate read-only authorization whose exact text has NOT yet been
recovered** — it is **not** covered by this census ruling and must not be presented as such. A
separate explicit authorization is awaited and will be appended here when its actual text is available.

## Appendix — Advisor interrupted-run replacement ruling, VERBATIM

Same replacement child/route as above. Exact received agent message below, preserved verbatim.

ADVISOR QUICK RULING (truncated 600 s run):
RULING: Run 20261001-042821-012-f5-observe-600s-c4bcd2b is INVALID as F5 evidence: no F5 decision, no stall claim, no capacity claim. Keep the directory unchanged and record it as "interrupted at ~14 s; cause unknown; leading hypothesis: external process-tree termination (inferred)". ONE replacement 600 s run is AUTHORIZED: same pushed pair c4bcd2b/a71, same env/profile, fresh empty root, new label ending -retry1, no seed, census as approved. Conditions: the parent owns the job as a managed background job and stays to collect it with job_output (wait). Don't hand it to a child that ends its turn; use workers for analysis only. Re-run preflight first: identity verify, disk gate (≥15 GB after this run's save-root), no jsrf process alive.
BASIS (observed, I read the run dir myself): all artifacts were last written at 04:28:35 local, ~14 s after the directory was created. jsrf_run.log stops mid-line ("[KERNEL] #18721 … →") with no FATAL/E0424943/unresolved marker. stacks.txt has 17,720 lines, all first-chance DEBUG_EXCEPTION, and no THREAD capture section; the 180 s run had one, ending in diagnostic_deadline. result.json is 0 bytes. collect.c:2836-2843 opens result.json and then writes it, so the collector or runner died between the open and the write. A guest fault or deadline would have produced a capture plus an outcome.
BASIS (inferred): the run was killed abruptly from outside the game, consistent with the owning shell or job ending. Not proven.
REVERSED BY: if the -retry1 run also ends early with no capture while the parent still owns the job, the harness-ownership hypothesis is false. Then STOP: no third run; bring me stacks.txt, the log tail and the event logs. If a second-chance exception or fatal marker turns up in this run, it becomes a defect investigation instead.

**Session clarification (outside the quotation above — the Advisor's wording is preserved as written,
including "the collector or runner died between the open and the write").** A **0-byte `result.json`
is consistent with an unwritten or unflushed file** and is **not proof** of an exact failure window or
of any particular causality. No title or hardware-fidelity claim is made by this appendix.

## Appendix — Advisor no-rerun ruling, VERBATIM

Same replacement child/route. Received message, not a closing paraphrase. Exact text below.

ADVISOR QUICK ANSWER (seed / longer run, no F5 ruling yet):
RULING: Correct: no seed, no longer run, and no new run until the CMP/table check loop is explained from retry1's artifacts. The follow-up list you sent the worker matches what I asked for. Agreed on the table mtimes: the census is right; the tables are static after 04:40:08, and only the CMP mtimes move. The worker withdraws the "all tables change" claim unless it measures more than one distinct table mtime.
BASIS (observed): copying stopped at about 230 s, and every DVD table had been read by then. From about 230 s to 600 s the log shows no further payload or DVD opens, only the check loop. A longer run would most likely only repeat the loop (inferred). The F5 seed in §4 assumed "the fill gates the logo". Retry1 doesn't show that: the fill stopped and the SEGA screen continued. That is closest to F5 decision row 3 ("cache stops growing, image unchanged → bring the waits"). The game is still running and looping, though, not blocked in a wait. So the next step is the loop's caller and the condition it checks. That includes the stacks and the missing Cache09/CMP09 lead. It is not a cheaper way to fill the cache.
REVERSED BY: the corrected interval data showing payload opens or table growth after about 230 s (the fill isn't finished); or the image changing during the loop (progress is happening, so a longer run becomes the cheaper discriminator).

## Appendix — F5 directory-probe ruling (D1) and reply 3, VERBATIM

Received from **child `63c4869f-3689-41b2-89b8-f5429f8b5927`** — route **`claude` / `claude-opus-5-5`
@ `high`**, **parent-pinned spawn parameters** (not child self-verification). Continuity: **ACK 1 and
ACK 2 both received**. The ruling was delivered by `send_message` (the child's own `tool/call` record,
line 135, `arguments.message`, 6353 chars) — **not** by its closing `assistant` text, which is shorter
and is not the ruling. Copied verbatim; **no hidden reasoning is quoted, summarised or used**.

### F5 ADVISOR RULING (full A–E, as received)

F5 ADVISOR RULING (same-route recovery, claude-opus-5-5 high). Read-only throughout: original XBE disassembly, a 4-dword read of the import table and toolkit source. No run, no code change, no mutation.

A. HOW THE EXISTENCE PROBE WORKS (original raw, read by me)
- 24EF0(idx) builds the marker name (idx 9 = the literal with no number; otherwise %02d) and calls 25770(name).
- 25770 calls 145E7E(name, &finddata). If the result is -1 it returns 0. Otherwise it calls 145585(handle) and returns 1.
- 145585 is NtClose (thunk [0x1C3F60] = ordinal 187). 145E7E is an XAPI FindFirstFile:
  1. It splits off the directory part and keeps the leaf as the ANSI mask.
  2. It calls NtOpenFile (thunk [0x1C3F80] = ordinal 202) on the directory, with access 0x100001, share 3, options 0x4021 (DIRECTORY_FILE | SYNCHRONOUS_IO_NONALERT | OPEN_FOR_BACKUP_INTENT).
  3. It calls NtQueryDirectoryFile (thunk [0x1C3FB8] = ordinal 207) with args (h, 0, 0, 0, &iosb, buf, 0x148, class 1, &mask, RestartScan=0).
  4. If the status is below 0, it calls NtClose, sets LastError and returns -1. Otherwise it returns the handle.
  So "the marker exists" means "the first NtQueryDirectoryFile on a freshly opened directory handle succeeded". 24E10 (the Z: copy-exists check in state 2) uses the same 25770 path.
- Thunk ordinals came from the original XBE section read at VAs 0x1C3F60/68/80/B8, giving 187, 289, 202 and 207.

B. CANDIDATE DEFECT (toolkit source, Windows branch is live: kernel_file.c #if _WIN32 runs from line 70 to 732)
- kernel_file.c:590-623 keeps a static DIR_CONTEXT s_dir_contexts[64], keyed by the native HANDLE value (file_handle) and live while find_handle != NULL.
- In xbox_NtQueryDirectoryFile (:625-729), a FindFirstFileW that succeeds sets first_done=TRUE and leaves find_handle open. The context is released only when a later FindNextFileW fails (:680-686, :692-697).
- bridge_NtClose (kernel_bridge.c:734-752) and xbox_NtClose (kernel_file.c:316-324) never release that context. Nothing else references s_dir_contexts (grep).
- What follows from that:
  (i) Every successful probe leaks one context, with a stale HANDLE value and first_done=TRUE.
  (ii) Windows readily reuses a handle value right after CloseHandle. If the next probe's NtOpenFile receives the same value, find_or_create_dir_context matches the stale context. RestartScan=0 and first_done=TRUE then send it into FindNextFileW on the PREVIOUS search pattern. That fails, so the call returns STATUS_NO_MORE_FILES and clears the context. The guest sees "marker absent" for a file that exists. The probe after that starts fresh and succeeds.
  (iii) If the values are not reused, the 64-slot table fills after 64 successful probes. Every later query then returns STATUS_INSUFFICIENT_RESOURCES, and all probes fail.
- On real NT the scan state belongs to the file object, so a new open always starts a new scan.

C. WHY THIS FITS THE OBSERVED LOOP (25040 raw lines 43-69 and 153-170; I did not rely on worker wording)
- State 0, when [+48] != [+4C], needs 9 CONSECUTIVE successful 24EF0 probes (edi 0..8). Only then does it write [+48]=9 and state=5, create the unnumbered marker, and reach 11BE0/11C20 (the finish).
- Under (ii) the probe results alternate. Under (iii) they all fail. Either way 9 in a row never happens.
- Each failure goes to 2510D: [+48]=[+4C], [+50]=0, state 6. State 6 calls 256A0(0x1f, [+48]) and moves to state 7. State 7 reloads the count/pointer pair and moves to state 1.
- State 1 scans the bitmap from [+50]. Once this cache's bits are already set, it reaches state 5, which creates marker [+48], computes next = [+48]+1 mod 9 into [+4C], and sets state 0. That repeats indefinitely.
- That is exactly "payload copy finished, then roughly 994 marker creates with STATUS 0, then no F6".
- The creates succeed because NtCreateFile opens a regular file and never touches the directory-context table.
- The Information 2:1 expression at kernel_file.c:234 is still NOT causal here. 24E70 tests only the handle.
- Status: a strong source-level candidate whose mechanism is consistent with the evidence. Causality is NOT yet proven at runtime.

D. NEXT BOUNDED ACTION
1. Cheapest, no run, read-only: inspect the frozen host state in the existing retry1 run's process.dmp, using the archived binaries/symbols.
   - Read jsrf_recomp!s_dir_contexts, which is static in kernel_file.c and may be inlined or COMDAT-folded, so use the archived PDB. Read 64 × {file_handle, find_handle, first_done}.
   - Expected under (ii): at most a few entries have a non-NULL find_handle.
   - Expected under (iii): all 64 have a non-NULL find_handle and stale handle values.
   - Also record the "Ke"/handle-table state only if it is trivially available.
   - Run the mapping gate first if any guest-VA read is involved. This read is host-native, so the gate is still good practice but not strictly needed.
   - Precondition: check that process.dmp is a full-memory minidump that includes the .data/.bss of jsrf_recomp.exe. If it is not, or the symbol is not resolvable, record that as the result and stop. Do not substitute a guess.
2. If step 1 is unavailable or inconclusive, write a host-only unit test as the first code step. It needs a separate code packet and owner/parent authorisation; I am not authorising it here. The test opens a directory, makes one query with a mask that matches, closes, reopens the same directory with a different matching mask, and expects success. It should fail on the current toolkit a71f937. Write the test BEFORE any fix.
3. Fix shape, for the later packet and not now: release the directory context in the NtClose path, keyed by the native handle before CloseHandle. Optionally treat a context whose file_handle was closed as fresh. Keep FATX dot-directory filtering. The POSIX branch (:1136-1202) has its own table and needs the same check.
4. Not allowed: seeding markers, longer or blind runs, or a 600 s rerun before step 1 or step 2 resolves causality. Do not change line :234 as part of this.

E. CORRECTIONS ACCEPTED: everything in the parent's messages about [+48] vs [+4C], state 6 → 7, the count/pointer pair being consumed in state 7, and 24DB0's loop direction and mask. My ruling does not depend on the 24DB0 bitmap semantics, beyond "bits already set lead from state 1 to state 5", which is the ebx==edx path at 25144/2514E.

### F5 ADVISOR REPLY 3 (as received)

F5 ADVISOR REPLY 3

YES. In D2, "separate code packet and owner/parent authorisation" means its own packet with its own acceptance and record. It does NOT mean a new owner boundary. Within the original owner pre-authorisation (step 4 / continue until a §3.4 or W14 stop), the parent may approve the bounded RED-test-then-fix packet itself. Conditions:
(a) The test is written first and fails on a71f937.
(b) The fix is limited to releasing the directory-search context on close (Windows path; check the POSIX branch).
(c) The packet's tests and acceptance pass before any push. Toolkit is pushed first.
(d) No seeding, and no change to line :234 rolled into it.
(e) Escalate to the owner only if D1 or the RED test contradicts the mechanism in a way that needs a scope change, or if a genuine §3.4 condition comes up.
I have not re-read the owner prompt file. This rests on your citation of lines 27–57 and of §3.4/W14.

WILDCARD WITHDRAWAL: AGREED, from the original raw. At 145EED–145F04, the wrapper zeroes the mask length (match all) only when the leaf is exactly 3 characters "*.*". The marker leaf "JSRF_CACHE_COMPLETExx.CMP" (or the literal with no number) is not "*.*", so the exact-name mask applies. The mask handling is not a candidate defect.

I'll wait for the D1 result.

### F5 D2 DESIGN RULING (full, as received)

Received from the **same child `63c4869f-3689-41b2-89b8-f5429f8b5927`** by `send_message` (its
`tool/call` line 226, `arguments.message`, 4019 chars) — **not** its short closing text. Verbatim:

F5 D2 DESIGN RULING (same route; sources read: kernel_file.c 316-324, 590-729, 907-915, 1128-1237; kernel_bridge.c 734-752)

1. HOOK. Add one release function to kernel_file.c with a body in each branch (Windows and POSIX), e.g. `void xbox_dir_context_release(HANDLE h)`, declared in kernel.h. Call it at BOTH sites:
   a. `xbox_NtClose`, at both :316 and :907: inside the existing valid-handle check, BEFORE `CloseHandle`.
   b. `bridge_NtClose` (kernel_bridge.c:747-749): after `bridge_take_handle` and before `CloseHandle(h)`, only when `h` is valid. This is required because `bridge_NtClose` does NOT go through `xbox_NtClose`. Hooking only `xbox_NtClose` would give a false GREEN on the guest path.
   Release BEFORE the host close. If the order were reversed, another thread could receive the reused handle value and have its new context wiped.

2. RELEASE SEMANTICS. Take `s_dir_cs`, using the same lazy-init helper the code already has.
   - Windows: for every slot whose `file_handle == h`, regardless of `find_handle`, `FindClose` the find handle if it is non-NULL and not `INVALID_HANDLE_VALUE`, then zero the slot (`file_handle`, `find_handle`, `first_done`).
   - POSIX: for every slot whose `handle == h`, `closedir` it if it is non-NULL, then set `handle` and `dir` to NULL.
   - Invalid, NULL or synthetic handles (`0xDEAD0001`, `0xBEEF0010`): no-op, and keep the existing return values.
   - Do not change the lookup or query semantics, the dot-directory filtering, the `FindNextFile`-failure cleanup, or line :234.

3. RISKS, NOT IN SCOPE (record them, do not fix):
   - The lazy `InitializeCriticalSection` (the `s_dir_cs_init` flag) is a pre-existing race.
   - On Windows the query uses `ctx` outside the lock after lookup. A guest closing a handle while another thread is querying it is guest misuse.
   - Before acceptance, grep for any other path that `CloseHandle`s a taken token; any such path needs the same call.

4. DETERMINISTIC RED (actual Windows build, a separate test process per ctest). Use a fixture temp directory containing exactly one file `M.CMP`.
   Direct-API test:
   - Open the directory 64 times with `xbox_NtOpenFile`/`NtCreateFile`, using `DIRECTORY_FILE` and keeping all handles open. Query each once with the exact mask "M.CMP"; every query must succeed.
   - Close all 64 with `xbox_NtClose`.
   - Open a 65th handle and query it with "M.CMP".
   - On `a71f937` the 65th query fails either way. If the handle value is reused, the stale `first_done` context sends it to `FindNextFile`, which returns `NO_MORE_FILES`. If it is not reused, there is no free slot and it returns `INSUFFICIENT_RESOURCES`. So the RED does not depend on whether Windows reuses the value. Assert `STATUS_SUCCESS` and record the actual failing status in the RED record.
   - Add a churn assertion: 200 rounds of open, query, close, each must succeed. This covers the guest's real probe pattern.
   Bridge-path test (REQUIRED, no direct-only GREEN):
   - Run the same 64/65 sequence through `bridge_NtOpenFile`, `bridge_NtQueryDirectoryFile` and `bridge_NtClose`, with guest-memory handle, IOSB and ANSI_STRING mask.
   - If the toolkit has no bridge or kernel-thunk test harness, the packet must add the minimal one or record bridge coverage as NOT verified, which means NOT ACCEPTED. Check what exists first; I have not verified it.
   - `bridge_take_handle` must have resolved the token to the native `HANDLE` that the context is keyed by. The test proves this end to end.

5. ACCEPTANCE:
   - RED recorded on `a71f937`: both tests fail and the statuses are recorded.
   - GREEN after the fix.
   - The full toolkit ctest suite and the game `just test` / `just check` pass.
   - POSIX branch: compile it if a POSIX build exists; otherwise record "POSIX reviewed, not compiled". That does not block the Windows packet.
   - Push the toolkit first, then the game record.
   - No guest run until GREEN. No seeding and no :234 change.
   - The smoke run and profile policy come later, as you said.

**Packet:** the implementation packet is `docs/packets/f5-directory-context-close.md` —
**ACCEPTED** (RED verified, GREEN verified, 300 s smoke PASS-F5 criterion (a)).

### F5 D2 guard clarification (full, as received)

Received from the **same child `63c4869f…`** by `send_message` (its `tool/call` line 241, turn 7 step 1,
`arguments.message`, 1272 chars) — verbatim, not a summary:

F5 ADVISOR: D2 approval received. On the guard question:

- Invalid/synthetic handles: `xbox_dir_context_release` returns void and changes no return value.
  - In `xbox_NtClose`, call it only inside the existing `if (Handle && Handle != INVALID_HANDLE_VALUE)` block. The STATUS_INVALID_HANDLE path stays untouched.
  - In `bridge_NtClose`, call it only inside the existing `if (raw_handle && raw_handle != 0xDEAD0001u && raw_handle != 0xBEEF0010u)` block, after `bridge_take_handle`, and only when `h` is neither NULL nor INVALID_HANDLE_VALUE. `g_eax = 0` stays as it is.
  - The helper itself must also no-op on NULL/INVALID. Defence in depth: it is a cheap check, and it means no empty-slot match on NULL.
- IMPORTANT: Windows `find_or_create_dir_context` treats `find_handle == NULL` as a free slot. So release must also clear `file_handle`. Otherwise a stale `file_handle` can linger in a slot that looks free. That is harmless for lookup, which requires `find_handle != NULL`, but clear it anyway so the table state is unambiguous.
- Test for the guard: closing a synthetic `0xDEAD0001` through the bridge returns 0 and leaves the table unchanged. This is optional, and cheap if the harness exists.

Standing by for the worker's harness plan or the RED/GREEN results.

### F5 D2 harness-plan ACK (full, as received)

Received from the **same child `63c4869f…`** by `send_message` (`tool/call` line 256, turn 8 step 1,
12:56:18, 1236 chars) — verbatim, not a summary:

F5 ADVISOR: ACK on the harness plan. One required change: use NtOpenFile, not NtCreateFile.

- The guest path is NtOpenFile (thunk ordinal 202, from the original raw at 145F08-145F20). bridge_NtOpenFile (kernel_bridge.c:3340-3353) forwards to the same bridge_create_file_impl with disposition 1 (FILE_OPEN) and allocation 0. The two are functionally equivalent, but an NtOpenFile wrapper costs one more same-shape seam wrapper and matches the guest exactly. So: REQUIRE a fourth wrapper, xbox_test_bridge_NtOpenFile, for the RED/GREEN bridge test. The NtCreateFile wrapper is optional; drop it if it is not needed for fixture setup.
- Use the guest's exact arguments: DesiredAccess 0x100001, ShareAccess 3, OpenOptions 0x4021 (DIRECTORY_FILE | SYNCHRONOUS_IO_NONALERT | OPEN_FOR_BACKUP_INTENT). Query with FileInformationClass 1, Length 0x148, RestartScan 0, and an ANSI_STRING mask "M.CMP" in guest memory.
- Approved as stated: the toolkit fixture, the existing jsrf_test_write_stack seam, wrappers in scope, and POSIX cleanup compiled if a POSIX build is available, otherwise recorded as "reviewed".
- Noted: the grep confirms no other close site for owned file tokens.

No further design changes. Proceed when the parent authorises.

### F5 D2 budget ACK (full, as received)

Received from the **same child `63c4869f…`** by `send_message` (`tool/call` line 271, turn 9 step
1, 13:01:17, 1007 chars) — verbatim, not a summary:

F5 ADVISOR: ACK the budget count. No correction from my history.

- This advisor session saw only the following. I issued no other packet ruling and counted nothing else:
  - F5 source analysis: the CMP loop and the dir-context candidate.
  - D1: inconclusive because the dump has no coverage. Not a contradiction, and not a W14 reset.
  - D2: one bounded candidate fix unit, approved, not accepted, RED pending.
- Counting D2 as one candidate unit and not yet at the 3-packet threshold is consistent with what I hold. Removing the "no additional senior budget" exemption claim is correct.
- The ceiling stays at the original 4 h, 14:32:32 UTC, unless an accepted critical finding or horizon move resets it. A RED-then-GREEN D2 that later moves the guest past F5 would be the candidate for that. A source candidate alone is not.
- Caveat: I have not read the W14 text in docs/agent-workflow.md or plan §3.4 in this session. This ACK confirms consistency with my history only, not an independent rules audit.

**Session note on the budget:** the ACK confirms the count and states **no correction from its
history**; it also **caveats that it did not itself read W14** — the **authority is plan §3**, which the
parent read. Per that count, **D2 is one pending candidate, not three**; the **original ceiling time is
`14:32:32`** and there is **no reset** from D1 or from the source reading alone.

### F5 D2 protocol-correction ACK (full, as received)

Received from the **same child `63c4869f…`** by `send_message` (`tool/call` line 306, turn 10 step 5,
13:08:47, 1825 chars) — verbatim, not a summary:

F5 ADVISOR: ACK the protocol correction. It is not a new design, and there is no new count seam.

I read the current tests/dir_context_release_test.c (402 lines): RESTART_VAL=0 at every query site, argv direct|bridge, an explicit `failed` flag plus queried==200, and dir_close on query failure. It matches the D2 design.

WHY THE PRIOR 4/4 SPLIT IS EXPLAINED BY RESTART=1, NOT BY INHERENT FLAKINESS. On a71f937 the 65th open has two possible outcomes, and Restart decides how they come out:
- (a) Windows reuses one of the 64 closed values. The stale context has first_done=TRUE and its search pattern was dNNN\M.CMP with exactly one match.
  - Restart=0 → FindNextFileW → NO_MORE_FILES → FAIL.
  - Restart=1 → fresh FindFirstFileW → PASS. This is the masking.
- (b) No reuse. All 64 slots are occupied, so find_or_create returns NULL → INSUFFICIENT_RESOURCES → FAIL, whatever Restart is.
So Restart=1 gives a reuse-dependent result, and Restart=0 gives a deterministic RED. Churn is RED either way with Restart=0. With 65 distinct directories, a single-match pattern, and contexts held across the 64 opens, it is deterministic.

MINOR, not blocking:
1. Direct mode stores the native HANDLE as a uint32_t. That is fine for Windows kernel handles, which are below 2^32 in practice. Note it in a comment.
2. Churn runs in the same process after open64, so table state carries over. That is intended. On RED, record which subtest failed first and its status; you should expect 0xC000009A (INSUFFICIENT_RESOURCES) or 0x80000006 (NO_MORE_FILES).
3. The synthetic guard checks only the return value, not that the table is unchanged. That was the accepted optional scope.

Acceptance is unchanged: a recorded RED in both separate CTest processes on a71f937, then GREEN after the fix, then the full suites. No guest run before GREEN.

**Session note (HISTORICAL — at the protocol ACK, 13:08:47; superseded by the verified RED below).** The **initial 8-run, 4-fail / 4-pass result is
NOT an accepted RED**: the fixture used **`RestartScan 1` at the query sites**, which **masked the
stale-context path**, and it had **no direct mode** and reported **churn `failed_at 0`**, a **false
GREEN at round 0**. The corrected test uses **`RestartScan 0` at every site**, **separate direct and
bridge processes**, and an explicit **`queried == 200`** success condition — matching the D2 design,
**as the Advisor read it at 402 lines**. *At that time the RED was not yet verified; it is now* — a recorded RED in **both**
separate CTest processes on `a71f937` is pending, then GREEN after the fix, then the full suites, with
**no guest run before GREEN**. **No new count seam and no production fix** are involved, and the fix is
to a **prior report's mislabel — not inherent flakiness**.

### F5 D2 corrected RED — VERIFIED (2026-10-01)

Measured and **recorded before the production fix was authorized**; the implementation is **in
progress** at the time of writing. At RED time the toolkit carried **no `kernel_file.c` change** — the
parent's own check recorded **no production fix**, and **no timestamp is asserted here** (none is
inferred from file mtimes or from a worker's reading).

| Item | Direct mode | Bridge mode |
|---|---|---|
| opened / queried (subtest 1) | **64 / 64** | **64 / 64** |
| 65th distinct directory query | **FAIL — status `0x80000006`** | **FAIL — status `0x80000006`** |
| churn (subtest 2) | **FAIL at round 1 of 200**, after 1 successful query, status `0x80000006` | same |
| mode tag | `mode=direct` | `mode=bridge` |

**CTest: 2 tests failed out of 2 — `xbox_dir_context_release_direct` and
`xbox_dir_context_release_bridge` — rc 8, total 0.29 s.** Status `0x80000006` is
`STATUS_NO_MORE_FILES`, one of the two outcomes the design predicted (the other being
`0xC000009A`/`INSUFFICIENT_RESOURCES`); the **stale-context path** is therefore the one taken here.

**Raw artifacts (cite exactly what was read):** the **parent read `red-direct.log` (its initial ~70
lines)**, the **load-bearing counts in both modes from `red-ctest.log`**, and — after the worker's
conversion — the **converted raws `red-direct-raw-utf8.txt` (lines 73–82)** and
**`red-bridge-raw-utf8.txt` (lines 138–149)**, which **confirm the same counts and the same
`0x80000006` status** in both modes. The **UTF-16 originals are preserved** (`red-direct-raw.txt`,
`red-bridge-raw.txt`) with their source/output hash in the worker log. A worker claim of **5/5 repeats
per mode** is **worker-reported, not parent-verified**; the **2/2 RED in the main CTest log is the
parent-verified result** and stands independently of the repetition count.

**The earlier 8-run, 4-fail / 4-pass result is superseded and is NOT an accepted RED** — it used
`RestartScan 1`, which masked the stale-context path (see the protocol-correction ACK above).

**Cause is NOT proven** by this RED: it demonstrates the observable failure and its status, not that
the directory-context leak is the only or the actual cause of the guest's loop.

### F5 D2 corrected-RED ACK (full, as received)

Received from the **same child `63c4869f…`** by `send_message` (`tool/call` line 321, turn 11 step 1,
13:12:31, 1387 chars) — verbatim, not a summary:

F5 ADVISOR: ACK the corrected RED. It matches the mechanism.

- The 65th open succeeds and its query returns 0x80000006 (NO_MORE_FILES) in both the direct and bridge processes. That is path (a): a reused handle value matched a stale first_done context, so the query went down the FindNextFile path. The failure status is the same one I predicted for the guest probe.
- Churn: round 0 passes and round 1 fails. That is consistent. The 65th query's FindNextFile failure freed one slot, round 0 used that slot fresh and leaked it again, and round 1 then hit a stale or reused value.
- Proving no fix was in place with HEAD a71f937 plus an empty kernel_file diff is adequate.

The production-fix authorisation stays within the D2 scope:
- The helper in both branches.
- Both xbox_NtClose copies, and bridge_NtClose before the host close.
- Guards in place and return values unchanged.

The fixture's uintptr_t token change and comment correction are test-only and acceptable.

For GREEN review, send me:
(1) the diff of kernel_file.c, kernel_bridge.c and kernel.h;
(2) both ctest logs showing the 64/65, churn 200 and guard checks passing, with 5/5 repeats per mode;
(3) the full toolkit ctest result and the game `just test` / `just check` results;
(4) the POSIX compile result, or "reviewed, not compiled".

I will rule on the smoke profile and the short bound then. Not a 600 s blind run.

**GREEN review list (4 items, as the ACK requires):** (1) the diff of `kernel_file.c`,
`kernel_bridge.c` and `kernel.h`; (2) both ctest logs showing the **64/65, churn 200 and guard checks
passing, with 5/5 repeats per mode**; (3) the full toolkit ctest result and the game `just test` /
`just check` results; (4) the **POSIX compile result, or "reviewed, not compiled"**. The Advisor
**will rule on the smoke profile and the short bound then — not a 600 s blind run.**

**POSIX helper (parent-read, independent):** the parent read `kernel_file.c:1162-1192` — **all matching
handles closed with `closedir`, `handle`/`dir` cleared, guards and lock correct** — recorded as
**reviewed, not yet compiled**.

### F5 D2 GREEN — VERIFIED (2026-10-01); smoke PENDING

Fix applied; **all gates green**. Counts read from the archived logs (plain UTF-8, readable):

| Gate | Result | Source |
|---|---|---|
| **Focused** (D2 unit) | **2/2 passed**, **0.19 s** — `xbox_dir_context_release_direct`, `xbox_dir_context_release_bridge` | `green-ctest.log` |
| **Toolkit CTest** | **7/7 passed**, **2.71 s** | `green-toolkit-ctest.log` |
| **Lifter unittests** | **Ran 134, OK (skipped=1)** — 133 passed, 1 skip, **14.460 s** | `green-lifter.log` |
| **Game CTest** | **31/31 passed**, **31.32 s** | `green-game-test.log` |
| **Game `just check`** | **"check: all checkers passed"** | `green-game-check.log` |
| **Toolkit build** | clean | `green-toolkit-build.log` |

**D2 unit behaviour, measured (verbose logs):** direct mode — **opened 64 / queried 64**,
**65th open ok with `65th_query_status=0x00000000`**, **churn 200 rounds / 200 successful queries,
failed=no**, **guard skipped** (direct mode has no synthetic tokens); bridge mode — same 64/64 and
65th status `0`, churn 200/200, **guard: synthetic token close returned `0x00000000` (expected 0)**.
Counted checks: **6 passed in direct, 7 in bridge**.

**Attribution, stated precisely:** the **parent read the focused 2/2, the full toolkit CTest, the
lifter and the game logs**; a **five-repeats-per-mode** claim is **worker-reported — the parent did not
read repeat logs** — so it is **not** cited as verified. The toolkit log's own total is **2.71 s**
(the parent's earlier "2.67 s" is superseded by the log).

**POSIX:** the parent read the **Windows helper at `kernel_file.c:615-633`** and the **POSIX helper at
`:1174-1192`** — **reviewed, NOT compiled**.

**Status:** **GREEN verified**, **smoke PENDING** — the Advisor consult is in flight, and the Advisor
**will rule on the smoke profile and short bound**, explicitly **not a 600 s blind run**. **D2 is NOT
accepted yet**, and there has been **no guest run since the fix**. **No W14 reset** — the ceiling stays
`14:32:32` UTC.

**Advisor GREEN-consult status (2026-10-01).** The **GREEN consult failed one turn and recovered on the
same child** — `send_message`, **no fallback route** — and **no answer is currently received**.
Therefore **no current-Advisor GREEN approval is claimed**; the consult's outcome is **pending** and is
recorded only when the actual ruling text arrives.

### F5 D2 GREEN review + SMOKE RULING (full, as received)

Received from the **same child `63c4869f…`** by `send_message` (`tool/call` line 443, turn 15 step 1,
13:22:45, **4176 chars**) — verbatim, not a summary:

F5 ADVISOR GREEN REVIEW + SMOKE RULING (same route; own reads done)

GREEN: APPROVED, scope matches D2. What I read myself:
- Toolkit diff on a71f937, working tree (kernel.h, kernel_bridge.c, kernel_file.c, CMakeLists.txt, plus the untracked test).
  - Helper: Windows at :615, POSIX at :1176. Each takes the lock, clears every matching slot, and no-ops on NULL/INVALID.
  - Both xbox_NtClose copies call it inside the valid-handle check, before CloseHandle.
  - bridge_NtClose calls it after take, only when h is valid, before CloseHandle. Synthetic-handle exclusion and g_eax=0 unchanged.
  - Query code is untouched.
  - Test seam wrappers: NtOpenFile, NtQueryDirectoryFile, NtClose. Two separate CTest processes.
- Log tails: green-toolkit-ctest 7/7, green-game-test 31/31 (including #28/#29), green-game-check "all checkers passed". Bridge log shows the synthetic 0xDEAD0001 close.
- POSIX reviewed, not compiled: accepted as recorded.
- Before any toolkit push, the untracked tests/dir_context_release_test.c MUST be committed with the fix.

NEW SUPPORTING EVIDENCE from the existing retry1 jsrf_run.log (read-only):
- 1245 calls to ordinal 207 (NtQueryDirectoryFile) in total. 1021 of them are from 0x145F50 after the first marker line, returning 538 × 0, 482 × 0x80000006, and 1 × 1. The tail alternates 0 / 80000006, which is the pattern predicted by mechanism (ii), handle reuse.
- On a correct backend, an exact-name query for a marker that exists cannot return NO_MORE_FILES.
- Caveat: the longest run of consecutive successes is 9. Those may include other 145E7E callers (the 24E10 Z: copy-check, or other directories), so this does not refute the mechanism. Treat it as runtime corroboration, not as proof of causality.

TIMING CONSTRAINT, which decides the bound:
- In retry1 the run started at 04:36:30.
- The first marker, JSRF_CACHE_COMPLETE01, was created at 04:39:55, about 205 s in, after the payload copy.
- So 30 s or 60 s would NEVER reach F5 and cannot test the fix. Both are rejected.

SMOKE RULING:
- Duration: --seconds 300. That covers about 205 s to the marker phase plus about 95 s for the loop to resolve or recur. It is half of the old 600 s and fits inside the W14 ceiling with margin.
- Profile: EXPLORATORY. Use the identical environment of retry1: RECOMP_APU_TRAP=1, RECOMP_FB_WINDOW=1, RECOMP_FB_WINDOW_DUMP_EVERY=600, RECOMP_KERNEL_LOG_BUDGET=100000, RECOMP_NV2A_TRACE=1, RECOMP_PB_EXEC=1, RECOMP_PB_SCAN=1, with RECOMP_GPU_ACK absent (default). Point RECOMP_FB_DUMP at a NEW directory. Ledger IDs as the parent listed: L14–L18, L20–L25, L39, L40, with L19 dormant. The parent verifies these against the ledger.
- Use a fresh isolated disposable --save-root under the new run dir: no seed, no copied cache, no markers. No change to line :234. A Release build of the GREEN toolkit tree via just build, with the identity check passing.
- The run must be on a committed tree, or record the toolkit dirty state explicitly in the run record. The runner archives toolkit.patch.

INTERPRETATION (decide from result.json plus the log, no title claim):
- PASS-F5, meaning the fix moves the loop: at least one of these happens:
  (a) JSRF_CACHE_COMPLETE.CMP (unnumbered) is created;
  (b) after the first marker, the log shows the 0x145F50 ordinal-207 results without the alternating 80000006 pattern, AND marker creates stop repeating (bounded, roughly at most 9 per cache, not about 50 each);
  (c) new guest progress past the SEGA screen in the framebuffer dumps.
  (a) or (c) counts as horizon movement, which makes the parent's W14 reset evaluation eligible.
- FAIL-SAME: the 9 numbered markers are recreated repeatedly and ordinal 207 still returns 80000006 for existing markers. That means the mechanism is incomplete. Stop, do not extend the run, and bring me the log counts.
- FAIL-DIFFERENT: the 207 calls are all 0 but markers still loop. Then the cleanup is correct but not causal. Bring me the 25040 state evidence. No seeding.
- Any crash or new first failure: record the earliest actual failure and its caller per AGENTS.md.
- Run the mapping gate before any guest-VA dump read.

There are no further prerequisites from me. Proceed.

**Session notes (scope/attribution, not part of the ruling):**
- **GREEN: APPROVED**, scope matches D2. The Advisor read the toolkit diff, the helpers (Windows
  `:615`, POSIX `:1176`), both `xbox_NtClose` copies, `bridge_NtClose`, the untouched query code, the
  test-seam wrappers and the log tails itself. **Before any toolkit push, the untracked
  `tests/dir_context_release_test.c` MUST be committed with the fix.**
- **The ordinal-207 counts are ADVISOR-REPORTED and must be verified by the parent** — they are
  **runtime corroboration, not proof of causality** (the Advisor says so itself, noting the longest run
  of consecutive successes is 9 and that other `145E7E` callers may be included).
- **`--seconds 300` is authorised, EXPLORATORY**, because retry1's first marker appeared at **~205 s**
  (run start 04:36:30, marker 04:39:55), so **30 s and 60 s are rejected as never reaching F5**.
- **No W14 reset** until an eligible event (unnumbered marker creation or a new frame) actually occurs.

### F5 D2 authorised smoke — LAUNCH RECORD (historical; outcome now recorded above)

**Launch-time record. The outcome is recorded in the RESULT and PASS-F5 sections; this block is
retained as history.**

| Item | Value |
|---|---|
| Job | **`pwsh-3220`** (parent-owned) |
| `--seconds` | **300** |
| Label | **`f5-d2-context-release-300s`** |
| Profile | **exploratory** |
| Environment | the **exact 7-variable retry1 set**, with **`RECOMP_GPU_ACK` absent** and **no other inherited overrides** (`GetChildEnv` shows none) |
| `RECOMP_FB_DUMP` | **`logs/workers/f5-d2-smoke-300-framebuffers`** (new directory) |
| Build | `just build` job **`pwsh-3212`** collected success; identity recorded |
| Save-root | runner-created fresh run dir and save-root automatically (no CLI root setting needed) |
| Toolkit state | **dirty: 4 tracked files + the untracked fixture**; the runner archives the **patch carrying the fix** — the **untracked fixture is NOT in that archive** (a test snapshot is copied after the run, independently hashed: artifact provenance, not profile-verified) |
| Ledger IDs | **L14–L18, L20–L25, L39, L40; L19 dormant** — **independently verified by the parent** against the ledger |

**W14: no reset** from this launch. A reset becomes eligible only if the run produces an eligible
event — the unnumbered marker, or a new frame — per the smoke ruling's interpretation rows.

### F5 D2 authorised smoke — RUN IDENTITY (read from the run's own metadata)

| Field | Value |
|---|---|
| **Run ID** | **`20261001-062415-815-f5-d2-context-release-300s`** |
| `metadata.started_utc` | **`2026-10-01T13:24:16.649794+00:00`** |
| `metadata.seconds` | **300** |
| `result` | **`diagnostic_deadline`**, exit **3**, **302.951383 s** (see the RESULT section) |
| `exe_sha256` | **`74377ac6130e812b7a830492d4995bdc58efb8899ff5c1d6c23b7ee6c0c0424e`** — **differs from retry1's `7027fafa…`**: the fix build |
| Profile | requested **exploratory** / classification **exploratory**; `RECOMP_GPU_ACK` **absent** (`present: false`, effective default enabled) |
| Effective env | the **7 fixed overrides** — `RECOMP_APU_TRAP=1`, `RECOMP_FB_WINDOW=1`, `RECOMP_FB_WINDOW_DUMP_EVERY=600`, `RECOMP_KERNEL_LOG_BUDGET=100000`, `RECOMP_NV2A_TRACE=1`, `RECOMP_PB_EXEC=1`, `RECOMP_PB_SCAN=1` — **plus the new `RECOMP_FB_DUMP=logs/workers/f5-d2-smoke-300-framebuffers`** (8 settings in total; the **7 count excludes `FB_DUMP`**) |
| Game revision | **`15d8af1f694681a7341fa5e8b03b8900b6cef4ec`** |
| Toolkit revision | **`a71f9374ddb2a6685b790493855c835842228212`**, **`patch_sha256 4611158b…`**, **`status_sha256 e4905daf…`** — the runner **archived the patch carrying the fix**; the **untracked fixture is NOT in that archive** (the parent copies a test snapshot after the run, with an independent hash — artifact provenance, not profile-verified) |
| Save-root | runner-created, `disposable: true` |
| XBE | `xbe_sha256 fd190557…` (unchanged) |

**No W14 reset at launch time**; the reset came later from the actual marker event (see the PASS-F5
section: horizon `13:27:49.9575858Z`, ceiling `17:27:49.9575858Z`).

### F5 D2 smoke 300 s — RESULT and FRAME FINDING (2026-10-01)

**Run `20261001-062415-815-f5-d2-context-release-300s`** (fix build, `exe_sha256 74377ac6…`).

| Field | Value |
|---|---|
| `outcome` | **`diagnostic_deadline`** (expected — **not** a crash) |
| `exit_code` | **3** |
| `duration_seconds` | **302.951383** |
| `dump_ok` / `gpu_report_ok` | **true / true** |
| `native_threads` / `named_frames` | **21 / 166** |
| `gpu_snapshots` / dropped | **1 / 0** |
| `save_root_verified` / `checkpoints_passed` | **true / true** |
| Mapping gate | **matches 1**, zero mismatch / unreadable / missing |
| Profile | classifier **EXPLORATORY** as expected; `RECOMP_GPU_ACK` absent |

**Frame finding (parent-read; PNGs converted, viewed by eye).** **38 framebuffer files, 3 byte-hashes:**
the **base file** (no extension, 921654 B, sha `9E7541A8…`) is the **window capture showing SEGA**;
the **8 numbered draws `000–007`** are one hash (`9DC5CDCF…`) = **black**; the **29 numbered draws
`008–036`** are another hash (`905E9204…`) = **SEGA**. Evidence PNGs:
`logs/workers/f5-d2-frame-0.png` (SEGA window), `-1.png` (black), `-2.png` (the SEGA group).

**PASS-F5 criterion (a) IS MET — D2 ACCEPTED; W14 RESET at the actual event.** The **unnumbered
`JSRF_CACHE_COMPLETE.CMP` was created** (verified independently: 0 B,
`CreationTimeUtc == LastWriteTimeUtc == 2026-10-01T13:27:49.9575858Z`), the **CMP loop stopped**, and
state 0's nine-probe check completed. **Horizon movement at the F5 loop**, recorded as **"F5 CMP loop
resolved; new stop is after the marker phase"**. **W14 RESET from the actual event: horizon
`13:27:49.9575858Z`, ceiling `17:27:49.9575858Z`, reset count 0; the old `14:32:32` is historical and
superseded.** The **eligible event is the actual marker, NOT a frame**. **Not a title claim**: the
visible frame is still SEGA, so **criterion (c) "progress past SEGA" is NOT met**. **D2 is ACCEPTED**
as a **bounded F5 unit** — **not a workflow packet promotion; CURRENT PACKET remains none**. Allowed:
the **D2 commit/push per policy** (toolkit first, the test file included, the record listing the ledger
IDs). **Not allowed: a run extension, a longer run, or seeding.** **No further run is in progress.**
**The `FB_DUMP` value is a filename BASE, not a directory**: the writer appends `NNN.bmp`, and the
extension-less file is the window capture — the durable wording says **"new path"**.

**Marker-once qualification.** All **10 marker files have equal Creation and LastWrite times**, and the
**log shows each marker PATH exactly once** — but **"created exactly once" cannot be inferred from equal
Creation/LastWrite times alone**; it rests on the log's per-path counts (Advisor- and worker-observed,
still a **qualified** method, not a timestamp proof).

**New stop — F6 candidate, UNCLASSIFIED.** The **sampled callee region** is the
**`0x1A03xx–0x1A04xx` loop calling `19E438`**, which calls **ordinals 277 and 294** (~6300 times each,
both returning 0). **This is the Advisor's initial hypothesis, not the proven loop head.**
**Next finding (annotated, resolved by source):** the **ordinal names are resolved** — **277 =
`RtlEnterCriticalSection`**, **294 = `RtlLeaveCriticalSection`** — and **`19E438` is a shared
conditional-enter helper with ~50 callers**, its family being **3 float-wrapper framework methods**.
**277/294 are host-blocking critical sections, and a VOID-0 return does not prove a kernel wait.** So
the **poll condition is NOT established**; the **scope is a shared lock**, **not a loop head**, the
**actual next stop remains UNCLASSIFIED**, and **no fix and no run** are proposed. The **branch-polarity
error in the earlier worker report was caught by the parent and corrected by the worker.** Next cheap
action, **source-only, no
run**: disassemble **`19E438`** and its
callers at `0x1A0360–0x1A0500` to find the actual loop head, and **run the mapping gate before
reading guest VAs** from the new dump.

**Old ordinal-207 counts — Advisor ACCEPTED the corrections (annotated, not rewritten).** The GREEN
ruling's figures (1245 total; 1021 from `0x145F50`; 538×0 / 482×`0x80000006` / 1×1) **stand as
originally quoted**, annotated as **qualified**: **1245 → 1244** strict calls (the 1245 included the
**summary line, a false positive**); **1021/538/482/1 → 1022/539/482/1** because the window starts at
**COMPLETE00, not COMPLETE01** (off-by-one at the boundary); and **matching each return status to its
call by adjacency is approximate** because threads interleave, so "alternating 0/80000006" is
**supporting evidence only, not per-call proof**. **Where the 205 s came from:** retry1 save-root file
**CreationTime** — `COMPLETE01` at 04:39:55 local versus `started_utc` 11:36:30Z = 04:36:30 local,
≈**205 s**; **`COMPLETE00` at 04:38:07, ≈97 s in**; these are **host file timestamps, not guest log
time**, and last-write times reflect rewrites.

### F5 D2 audit ACK + 300 s smoke interpretation (full, as received)

Received from the **same child `63c4869f…`** by `send_message` (`tool/call` line 503, turn 17 step 1,
13:34:21, **3562 chars**) — verbatim, not a summary. **This is the actual received ACK; the
earlier "consult pending" wording is superseded by it.**

F5 ADVISOR: AUDIT ACK + 300 s SMOKE INTERPRETATION (own reads)

1. AUDIT CORRECTIONS ACCEPTED. Annotate my earlier ruling as qualified; do not rewrite it.
- 1245 → 1244 strict calls. The 1245 included the summary line, which was a false positive.
- 1021/538/482/1 → 1022/539/482/1, because the window starts at COMPLETE00, not COMPLETE01. Off by one at the boundary.
- Matching each return status to its call by adjacency is approximate, because threads interleave. So "alternating 0/80000006" is supporting evidence only; it does not prove per-call results.
- Where the 205 s came from: retry1 save-root file CreationTime. JSRF_CACHE_COMPLETE01.CMP was created at 04:39:55 local (Get-ChildItem CreationTime). metadata.started_utc is 11:36:30Z, which is 04:36:30 local, so the gap is about 205 s. COMPLETE00 was created at 04:38:07, about 97 s in. These are host file timestamps, not guest log time. Last-write times (04:46:2x) reflect rewrites.

2. NEW RUN 20261001-062415-815-f5-d2-context-release-300s, read by me:
- result.json: diagnostic_deadline, 302.95 s, dump_ok, save_root_verified, checkpoints passed. Settings are identical to retry1 apart from RECOMP_FB_DUMP. RECOMP_GPU_ACK is absent, so EXPLORATORY. toolkit.patch is 10531 bytes; the dirty GREEN tree was archived.
- Save root (CreationTime == LastWriteTime for all of them, so each marker was created exactly once):
  - COMPLETE00 at 06:25:50
  - COMPLETE01 to COMPLETE08 from 06:27:36 to 06:27:49
  - **JSRF_CACHE_COMPLETE.CMP (unnumbered) at 06:27:49**
- Log: each marker path appears exactly once (10 total). In retry1 there were 46–59 per marker and the unnumbered marker never appeared.
- After the unnumbered marker (log line about 234620): there are **0** ordinal-207 calls from 0x145F50. The log is dominated by 277/294 calls (about 6300 each) and sub_0019E438 pairs. That is the polling region at 0x1A0360–0x1A0500. Stack: 001A0413/0489/0383 → 19E438 → 277/294.
- Ordinal 207 from COMPLETE01 onward: 49 calls (28 × 0, 21 × 80000006), approximately matched. Some NO_MORE_FILES results are expected for markers that were genuinely absent on a fresh save root.
- Framebuffer: I read the dump myself. It is 640×480 and shows "Presented by SEGA". The parent's 38-BMP grouping (SEGA, black, SEGA) stands; I did not recount it.

3. RULING.
- **PASS-F5, criterion (a), is met.** The unnumbered JSRF_CACHE_COMPLETE.CMP was created, the CMP loop stopped (each marker created once), and state 0's nine-probe check completed.
- That is horizon movement at the F5 loop, which makes D2 eligible for packet acceptance. It is also the parent's W14 reset evaluation, so record it as "F5 CMP loop resolved; new stop is after the marker phase".
- Not a title claim. The visible frame is still SEGA, so "progress past SEGA", criterion (c), is NOT met.
- New stop, F6 candidate, unclassified: after completing the cache, the guest sits in the 0x1A03xx–0x1A04xx loop calling 19E438, which in turn calls ordinals 277 and 294, about 6300 times each.
- Both 277 and 294 return 0, so this looks like a wait/poll on a kernel object or state. Next cheap action, source-only, no run:
  - Resolve ordinals 277 and 294 by name from the toolkit thunk table.
  - Disassemble 19E438 and its callers at 0x1A0360–0x1A0500, and identify the condition being polled.
  - Run the mapping gate before reading guest VAs from the new dump.
- Allowed: the D2 commit/push per policy (toolkit first, the test file included, the RED/GREEN/smoke record listing the ledger IDs).
- Not allowed: a run extension, a longer run, or seeding.

**Annotation to the ruling above (added after the fact; the quoted ruling is unchanged).** The
**ordinal names are now resolved by source**: **277 = `RtlEnterCriticalSection`** and
**294 = `RtlLeaveCriticalSection`** — **host-blocking critical sections**, and a **VOID-0 return does
not prove a kernel wait**. **`19E438` is a shared conditional-enter helper with ~50 callers** (family:
**3 float-wrapper framework methods**), so the **scope is a shared lock, not a loop head**; the
**"guest sits in the 1A03 loop" reading is the sampled callee region and the Advisor's initial
hypothesis, NOT the proven loop head**, and the **actual next stop remains UNCLASSIFIED**. The earlier
worker report's **branch-polarity error was caught by the parent and corrected by the worker**. **No fix
is proposed and no danger is claimed.**

### F5 → F6 Advisor classification (full, as received)

Received from the **same child `63c4869f…`** by `send_message` (`tool/call` line 575, turn 19,
**3463 chars**) — verbatim, not a summary:

F5→F6 ADVISOR CLASSIFICATION (own reads: original disasm, new-run stacks.txt, log, gpu-report; read-only, no run)

1. WORKER CORRECTIONS ACCEPTED. The parent's branch polarity is right. "No defect anywhere" is withdrawn. 19E438 is a shared lock helper, not the head of a poll loop.

2. THE 277/294 TRAFFIC IS PER-FRAME RENDER WORK, NOT A POLL. Per-site counts after the unnumbered marker (log line 234620 onward, tid 66348):
- 277@19E452: 6180 = 12 × 515.
- 294 at 1A0403, 1A0479 and 1A04EF: 1545 each = 3 × 515.
- 294 at 19F288, 19E829 and 19F31A: 515 each.
- KfRaiseIrql/KfLowerIrql (160/161) at 1A1BA8/1A1BC0: 515 each.
- KeRaiseIrqlToDpcLevel/KfLowerIrql (129/161) at 1BD606/1BD61B: 514 each.
All of these lock-step at about 515, which is one set per frame. The E_FAIL paths (1A039E/042E/04A4) were reached 0 times. The fact that 1A0403/0479/04EF were reached says nothing more than that the callers ran. So about 515 frames ran after the cache phase, in about 90 s: the marker was at about 213 s, since started_utc is 13:24:16Z = 06:24:16 local and the unnumbered marker was created at 06:27:49. That is roughly 5.7 frames per second. This is a rate estimate only; nothing timestamps individual frames.

3. ROOT STACK AT CAPTURE (stacks.txt, THREAD 66348). xbe_entry → 147F53 → PsCreateSystemThreadEx → 147EBB → 147FB4 → 6F9E0 → 13F80 → 13A80 (game main loop) → 14D090 → 198F10 → 198ED0 → 191390 → 1912A0.
- 1912A0 is the push-buffer kickoff.
  - It sets bit 0x10000 at [[0x19DCE0]+0x2268]+0x100410, which is the NV2A PFB_WBC write-back flush, then spins until that bit clears. The capture PC is the write at recovered.c:363951.
  - It then writes PUT ([+0x2264]+0x40).
  - It then loops on 18E120 while GET != PUT or [PGRAPH+0x400700] != 0.
- gpu-report: PFB_WBC=0 and DMA GET == PUT == 0x5B50C. The flush and the queue were not stuck at capture.
- In retry1, the main thread was also in 1912A0, reached through 198F10 → 198BE0 → 1989B0 → 18DAA0 → 1916B0 → 191530. The CMP state machine was ticking inside the same frame loop.
So the guest is NOT blocked. It is presenting frames in its main loop, and the screen stays on SEGA. Other threads are doing periodic Suspend/Resume/Ob*/KeWait work (62120, 54036, 58916, 15880); no blocked wait was identified.

4. F6 CANDIDATE (unclassified): the logo sequence does not advance within about 90 s at about 5.7 fps. There are two hypotheses:
- (A) A frame- or time-counted sequence that is merely slow. For example, a 60 fps design running at 5.7 fps would take about 10 times as long.
- (B) A sequence step that is gated on something not happening, such as audio or a movie/stream event, given RECOMP_APU_TRAP=1, or a thread event.
Do NOT call it a stall or a hang yet.

5. NEXT CHEAPEST READ-ONLY ACTION (one stop):
- Disassemble 13A80's per-frame dispatch, the vtable calls through `call [edi+0x14]`, [edi+0x1c8] and [ecx+0x20] from 13CB2 to 13EE4, together with 14D090. Identify the active logo or scene object and its step/timer field.
- Then read that object's state and timer fields from the new dump. The mapping gate has already passed for 062415.
- Decide between (A) and (B) from whether the timer is counting, which source it counts from (frames, KeTickCount or QPC), and what step condition it is waiting on.
- No run, no fix, no seeding. Once (A) or (B) is classified, I'll rule on whether a longer bounded run or a targeted diagnostic is justified.
- The D2 closure is independent and stays accepted.

**Session notes (annotation only; the quoted ruling is unchanged):**
- **The earlier `1A03` "loop / poll" reading is SUPERSEDED** — it was the **sampled callee region** and
  the Advisor's **initial hypothesis**, **not the actual poll head**; `19E438` is a **shared lock
  helper** and the **scope is a shared lock**, not a loop head. The old verbatim text is **kept
  unchanged** above and is annotated here.
- **The 277/294 traffic is PER-FRAME RENDER WORK, not a poll** (277@19E452 = 12 × 515;
  294 sites = 3 × 515 and 1 × 515; IRQL pairs ≈ 515 each; **E_FAIL paths reached 0 times**). All
  lock-step at **≈515**, i.e. **one set per frame**.
- **≈515 frames after the marker over ≈90 s ≈ 5.7 fps** — a **rate estimate only**; nothing timestamps
  individual frames.
- **Capture state:** root stack `xbe_entry → 147F53 → PsCreateSystemThreadEx → 147EBB → 147FB4 → 6F9E0
  → 13F80 → 13A80 → 14D090 → 198F10 → 198ED0 → 191390 → 1912A0` (push-buffer kickoff);
  **PFB_WBC = 0** and **GET == PUT == 0x5B50C**, so the flush and queue were **not stuck at capture**.
- **Wording discipline:** this is a **snapshot at capture** and is **not proof of no stall at any other
  time**; the classification is **not** an absolute "not blocked" claim.
- **F6 candidate remains UNCLASSIFIED:** (A) a slow frame/time-counted sequence vs (B) a step gated on
  an event that has not happened (audio/movie/thread), given `RECOMP_APU_TRAP=1`. **Not called a stall
  or a hang.**
- **Next action — READ-ONLY, one stop, parent-assigned to the worker (already in progress):**
  disassemble **`13A80`**'s per-frame dispatch, the vtable calls (`call [edi+0x14]`, `[edi+0x1c8]`,
  `[ecx+0x20]`) from **`13CB2–13EE4`**, together with **`14D090`**; identify the active logo/scene
  object and its **step/timer field**, then read that object's state and timer fields **from the new
  dump** (the **mapping gate has already passed** for `062415`). **No run, no fix, no seeding** until
  (A)/(B) is classified. **The D2 closure is independent and stays accepted.**

### Lineage of the Advisor children (all four, §4.4 same-route, no fallback)

| # | Child | Route / effort | Why replaced | What it produced |
|---|---|---|---|---|
| 1 | `4e6d87e1-f748-48b3-a0a4-a6e5728bfeee` | `claude`/`claude-opus-5-5` @ `high`, continuable | became unreliable — turns ended with no answer (**15 × `EMPTY_RESPONSE`, 1 × `RATE_LIMIT`**; never recovered, turns 41–46) | the F4 runtime, F5 cache-fill, duration-cap and window-watcher rulings |
| 2 | `c0ecc88b-756e-4256-9852-1bd8b7398735` | same route / `high`, parent-pinned | failed after producing rulings (**2 × `EMPTY_RESPONSE`**, turns 11–12) | the save-root census, interrupted-run replacement and no-rerun rulings |
| 3 | `c623447b-19f2-4abf-83bb-bdd85719556e` | same route / `high`, parent-pinned | **earlier ACK received** (continuity proven); **zero turn errors — 3 turns completed — but no substantive answer delivered** | no technical ruling |
| 4 | **`63c4869f-3689-41b2-89b8-f5429f8b5927`** | same route / `high`, parent-pinned | **current**; **two D2 turns failed after its ACK, then the third on the same handle recovered** | **the A–E ruling, reply 3, the D2 design ruling, the guard clarification and the harness-plan ACK** |

The **§4.4 continuity marker is a one-time recovery artefact**, not a per-consult property. The
empty-content failure mode (`EMPTY_RESPONSE` after the adapter exhausted five retries) is
**established for children 1 and 2**; **child 3 had zero turn errors**, so it must **not** be grouped
with them. For **child 4's two failed D2 turns the error type is not asserted** — the failures are
recorded as failures, **without claiming `EMPTY_RESPONSE`**, and **no fallback route was used**.

### D1 status (INCONCLUSIVE — checked by the parent; exact counts from the UTF-8 raw)

Read from the primary raw files (UTF-8), not from a summary:

- **`symbol-locate-utf8.txt`:** `s_dir_contexts` preferred VA `0x0000000141087780`, RVA `0x1087780`;
  runtime module base `0x00007FF707E30000`; live VA `0x00007FF708EB7780`; **ranges containing that VA:
  0**; `DIR_CONTEXT` sizeof **616 bytes**, **64 entries = 39424 bytes**; **8-byte slots readable:
  0/4928** → **`s_dir_contexts` is NOT in the dump: INCONCLUSIVE, no result.**
- **`dump-coverage-utf8.txt`:** **113 memory ranges**; **4 ranges overlap the exe image**; covered
  **546112 of 56725504 bytes = 0.96%**; by section — **`.text` 768 bytes (0.01%)**, **`.data` 545344
  bytes (1.31%)**, with **`.rdata`, `.pdata`, `.rsrc`, `.reloc` NOT IN DUMP**. The **first pre-check
  line said "0 overlap"** and is **superseded** by the dump-coverage measurement.
- **Partial-dump flags:** the dump reports **`0x201120`** (hex) with **MemoryList 113 ranges**, **no
  full memory**.
- **The 64-entry table is the thing needed — coverage percentage is not the focus.** **Do not claim
  "no full data" from the stream type alone**: a full capture *could* be a MemoryList, so
  **coverage proof is what is absent**, and that is sufficient to call D1 inconclusive.

**D1 is INCONCLUSIVE** (checked by the parent). Precisely: **the array is absent from the capture;
its occupancy and any runtime causality are UNKNOWN**, so **no cause is proven**, and no coverage
amount or gap is invented here.

**D2 status (historical, then current).** *Historically:* at the time of the D1 inspection, **D2 had
not yet been approved**; the current Advisor's **D2 turn then failed twice after its ACK recovery on
the same handle**, and a **third attempt on that same handle recovered the full design** — the actual
error types of those two failures are **not asserted** and **no fallback route was used**. *Current:*
the **D2 design is approved and quoted above** (design ruling, guard clarification and harness-plan
ACK), and the **parent now approves the RED phase**; **no new run** is taken until RED/GREEN and the
test gate pass.

### No stop, no owner boundary

This needs **no session stop and no new owner boundary**: under the original owner pre-authorisation
(step 4 / continue until a §3.4 or W14 stop), the **parent may approve the bounded RED-test-then-fix
packet** — that is what reply 3 (a)–(e) authorises.

## W14 ceiling ruling on owner resume — 2026-10-01 18:19 UTC

Owner resumed after the quota-reset report. Prior Advisor child `63c4869f-3689-41b2-89b8-f5429f8b5927` still failed to deliver an answer (`EMPTY_RESPONSE`; cause unknown). Under workflow §4.4's unreliable-state exception, replacement Persistent Advisor **`356aed7e-07f5-4327-aa32-88bdfe21ac8b`** was created on the **same `claude`/`claude-opus-5-5` @ `high` route**, not a fallback. Its independent first-turn read and exact second-turn continuity marker were delivered before this ruling. It independently read plan §3:117–121 and 880–926, workflow §4.4, the turn-19 classification ruling and the external handoff.

**RULING: CONTINUE WITH A BOUND.** Advisor conservatively counts wall time: the owner pause does not reset or suspend the recorded clock. The ceiling was crossed at **17:27:49.9575858Z**. Source-only findings remain qualified leads, not an accepted critical-path finding; no new reset is accepted. The **13:27:49.9575858Z** reset was the accepted **exploratory CMP-marker event**, not a strict-horizon move; no strict ledger row or title claim is implied.

Bound ends at the earliest of **U1 plus conditional U2 completed/reported**, **20:15:00Z**, or **one packet**. Without an accepted critical-path finding or new eligible event at that bound, **STOP for an owner decision; no second ceiling call**. Eligible events: Advisor-accepted A/B classification, a scene/frame transition beyond SEGA in dumps, or a title frame.

- **U1:** one worker, source only, ≤45 minutes, no run. Original-XBE oracle: enumerate direct calls to `24480`, `24540`, `24600`, `24620`; inspect fade-class methods (`vtable 1C4D10`) for CURRENT `+98..+A4` or completion `+C0` writes; inspect `24650` side effects, walker helper `1BA8A0` for a gate before virtual `+4`, and whether `123E0→11070` is conditional per frame from `13A80`. Normalize addresses to uint32; generated/recovered code is a cross-check only. A confirmed per-frame reset/gate is a candidate B-type cause, returned to Advisor before any fix. Complete negative result conditionally authorizes U2; inconclusive is UNKNOWN and stops U1.
- **U2, only after U1's complete negative result:** existing binary SHA `74377ac6…`, same exploratory profile and ledger IDs **L14–L18, L20–L25, L39, L40; L19 dormant**, no changes or instrumentation. At most 300 seconds per observation run. Two canonical captures ≥60 seconds apart, preferably one run if supported unchanged; otherwise the Advisor explicitly permits **240 s and 300 s cross-run captures**, a weaker control. Mapping gate must pass, and objects must be rederived in each capture from registry/tree and IDs, not assumed VAs. Read fade `+98..+C0` and scene `+98..+B0`; frame-site count advancement is the positive control. Rising nonzero channel0 is candidate A with measured rate; bitwise zero at both captures with frames advancing returns to Advisor as update-not-applied/reset; changed scene state or frame hash is candidate progress. Failed mapping/object/control is UNKNOWN. Any required code/new instrumentation returns to Advisor instead of running.
- **Not authorized:** speculative patches, guards, seeding, forced completion, timer bypass, or a longer exploratory run merely to see. A/B remains UNCLASSIFIED, D2 stays accepted, and the pytest side item remains closed as recorded at §Side-item citation ruling.

Basis: source fixed per-call step and frozen CURRENT0/TARGET1 sharpen the observation question but do not establish call cadence or reset history. Reversed by the bounded original-source/canonical-capture evidence above. Session assigned U1 to the existing source worker; no new run/patch/build was started by this ruling record.

### Resume ruling clarification and push receipt

Advisor accepted the Session's observation-control correction: **independent 240 s/300 s runs are CANDIDATE only**, because object age and reset history differ; neither nonzero CURRENT nor zero CURRENT with more global frames classifies A/B or resets W14. Only two captures within one run, with object VA rederived in each dump, matching vtable/scene identity and advancing frame control, can support an Advisor classification ruling; even those are not Session self-classification. Earlier U2 predicates are superseded to this extent.

U1 also covers original-source per-frame reachable float/x87/32-bit stores at `+98..+A4`, `+A8..+B8`, `+C0` from dispatcher/walker and fade/scene vtables (`1C4D10`, `1CCFB8`), inlined setter bodies matched by store pattern, and indirect fade-vtable calls. Full global scan not required. Unclosed reachability/indirect targets within the **unchanged 45-minute budget** produce UNKNOWN with explicit uncovered set, not U2 authorization. Only a clean covered set with uncovered paths judged immaterial by the parent's own original-source read authorizes U2. Bound unchanged.

**PUSHED_TO:** origin · **BRANCH:** toolkit main / game master · **COMMIT:** toolkit `a8262014eec9cd8d720184f7f2fb7dce4652105d` (unchanged), game `1bdf5d55bdaf285b3d9e95e1fbd341672481301a` · **REMOTE_URL:** toolkit `https://github.com/danillogical/xboxrecomp.git`, game `https://github.com/danillogical/poison-jam.git` · **RESULT:** toolkit up-to-date first; game fast-forward `e189f1b..1bdf5d5`; both HEADs equal their origin branches. Both trees clean before push; two outgoing documentation blobs audited, zero secret hits, no game asset path or oversized blob. Documentation pre-commit gates passed; no rebuild needed. This receipt records that completed checkpoint, not acceptance of unfinished U1.

### U1 UNKNOWN and final W14 STOP — 2026-10-01 ~18:29–18:31 UTC

**Advisor delivered STOP for an owner decision.** U1 returned UNKNOWN: per-frame receiver/reachability and aliased/inline writers could not be closed, and the uncovered set was not demonstrated immaterial. Therefore **U2 is not authorized**. This reaches bound (a), before 20:15Z, with no accepted critical-path finding or eligible event: **no second ceiling call, no clock reset, no further run/patch/investigation until owner decision**. Advisor did not independently re-read the original instructions below; STOP rests on U1 UNKNOWN and the prior bound, not on accepting those source leads.

Qualified parent checks, not A/B classification:
- Original `12418–124C8`: each nonzero mode branch executes its selected walker then **JMP `124C3`**, skipping `11070`; **all four mode words zero** reaches `124B8/124BE→11070`. The frozen capture's words were zero, establishing only frozen eligibility, not historical invocation/cadence.
- Original `7C110–7C13C`: **ID1DDA present → JNE `7C13B`**, skipping `666F0`. Thus worker reset site `6677C` is excluded for this captured state-8 path, not globally.
- Original `1BA8A0–1BA8E7`: no branch, pointer loads then fixed 16-dword scratch/matrix stores and RET. This helper is not a conditional skip of the subsequent virtual call.
- Worker raw E8/store-byte scans are leads, not instruction-aligned exhaustive enumerations. Positive recovery of six known stores cannot prove completeness; displacement matching misses CURRENT0 stores through `fstp [ecx]` after address formation. Zero literal address references cannot exclude computed/copied indirect targets. Later claimed DSOUND route was rejected: proposed function range did not contain its VA and the quoted little-endian immediate was `0x246`, not `0x24600`. No writer-completeness or new reset-cause claim is accepted.

A/B remains **UNCLASSIFIED**, D2 remains accepted, pytest side item closed with its prior caveats, no F6/title claim. The last eligible W14 event remains the **exploratory** unnumbered CMP marker at **13:27:49.9575858Z**, not a strict-horizon move.

Advisor's owner options (**none authorized by this STOP**): **recommended** small observation-only packet with same-object reads within one run, focused RED/GREEN and Reviewer gating; alternatively one ~240 s exploratory cross-run candidate compared with existing 062415 (cannot classify), an explicitly ledgered pragmatic shortcut (not recommended before classification), or pause/end. Current packet remains none. Records-only closure and fresh §4.5 Reviewer follow.

**STOP checkpoint push receipt:** **PUSHED_TO:** origin · **BRANCH:** toolkit main / game master · **COMMIT:** toolkit `a8262014eec9cd8d720184f7f2fb7dce4652105d` unchanged, game `28f0e5202bbcf52afd03e2540a4c034c6f573d85` · **REMOTE_URL:** toolkit `https://github.com/danillogical/xboxrecomp.git`, game `https://github.com/danillogical/poison-jam.git` · **RESULT:** toolkit up-to-date first; game fast-forward `1bdf5d5..28f0e52`; game HEAD equals origin/master, toolkit HEAD equals origin/main at the completed checkpoint. Clean trees, two outgoing documentation blobs audited with zero secret hits, no assets/oversized blobs; pre-commit gates passed. No code/runtime acceptance claim. This receipt documents that completed push; its own subsequent records-only commit is operationally checked separately.

## Session evidence qualification

The submission dimensions above are inherited measurements from the prior run's plan/TR, not a fresh Advisor decode. The initial worker negative claim that Mercenaries had no NV2A was withdrawn after discovering truncated tree coverage; corrected direct-source findings were supplied before this ruling. Primary reference URLs: https://github.com/xemu-project/xemu/blob/f9b14039e5bb56ae2d8f028e31e7cc19f13f7e12/hw/xbox/nv2a/pfifo.c and https://github.com/KraftMacAndChee/Mercenaries-Recompiled/blob/c978ee754e319c8593ee2260ac37b8262628f7c7/src/nv2a/nv2a_core.c . No hardware-fidelity claim is made by preserving local atomicity.
