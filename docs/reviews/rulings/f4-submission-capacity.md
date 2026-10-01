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
`0x47A84`; final GET=PUT `0x16648`. No sink or word-budget diagnostic; no frames/flip/present.
No strict horizon claim or F4 frame acceptance. **W14 CONTINUE**, ceiling not triggered: 3h08m
since the strict move at 05:57:40.3 UTC. Bound: **11:00 UTC or two more smoke iterations,
whichever comes first**, then return for the ceiling call unless a finding is accepted or a
strict run moves the horizon. Next is one read-only worker on this archive, no rerun: deadline
thread waits/ICALLs **first**; only if those point at GPU, flip/Present path and PCRTC/VBlank
pending versus interrupt-enable writers. Advisor re-ranking (INFERRED from zero draws/clears/clip):
non-GPU loading/audio/timer wait first, vblank gap second, D3D init before frame setup third;
ignored flip last. GPU-poll loop evidence reverses that ranking. Reversed by a different strict stop, an unlogged capacity/budget rejection,
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

## Session evidence qualification

The submission dimensions above are inherited measurements from the prior run's plan/TR, not a fresh Advisor decode. The initial worker negative claim that Mercenaries had no NV2A was withdrawn after discovering truncated tree coverage; corrected direct-source findings were supplied before this ruling. Primary reference URLs: https://github.com/xemu-project/xemu/blob/f9b14039e5bb56ae2d8f028e31e7cc19f13f7e12/hw/xbox/nv2a/pfifo.c and https://github.com/KraftMacAndChee/Mercenaries-Recompiled/blob/c978ee754e319c8593ee2260ac37b8262628f7c7/src/nv2a/nv2a_core.c . No hardware-fidelity claim is made by preserving local atomicity.
