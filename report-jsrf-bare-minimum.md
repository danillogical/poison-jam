# JSRF bare-minimum port: live work report

> **Where the current state lives.** This file is the audit / Grok-lineage report:
> workflow decisions, the A1-A5 audit sequence and delegation policy, newest first.
> The **current blocker, evidence revision and next packet** are maintained in the
> `CURRENT STATE` block at the top of `report-deepseek.md`, which is where the
> hourly automation and the DeepSeek sessions write. Read that block first, then
> come back here for the reasoning behind a decision. Run profiles — what a run is
> allowed to be evidence for — are in `docs/jsrf-run-profiles.md`.
>
> Both files are live and neither supersedes the other; they are two agent lines.

This report is updated during the work. Bold **Decision** entries identify
choices made on my own recommendation. Results distinguish implemented work
from verified behavior. Milestone acceptance/status remains in the plan.

### P0.1–P0.7 all accepted — 2026-09-23 (later)

**Superseding the entry below.** The paragraph that follows this one records the state
at the handoff: P0.1 unaccepted with a live reviewer dispute, P0.2–P0.7 unimplemented.
Every part of that has since been resolved, and this entry is the current state.

| packet | disposition | evidence |
|---|---|---|
| P0.S | AC1–AC4 AGREED | `docs/reviews/p0-1-execution.md` |
| P0.1 | AC1–AC5 AGREED, contract `P0-AC-r1` unchanged | `docs/reviews/p0-1-vblank-adjudication.md` |
| P0.2 | **AC1–AC4 AGREED** at review round 5; amendment `r14` ADEQUATE | `docs/reviews/p0-2-acceptance.md` |
| P0.3–P0.7 | **all 18 criteria AGREED** on one evidence revision | `docs/reviews/p0-3-to-p0-7-acceptance.md` |

**The P0.1 dispute was resolved, not overridden.** The reviewer's AC1–AC3 disagreement
went to the persistent advisor, whose call is final; the ruling held that the
classification defect was real and that the **implementation change was inside the
frozen contract rather than a change to it** — a retired-override layer resolved
relative to a boundary commit. The reviewer re-reviewed and AGREED all five.

**Decision: P0.2's review was allowed to run five rounds rather than being closed when
it first looked adequate.** Four rounds refuted it, and **four of the five AC2 defects
were in the fix for the previous one**. The sequence is a single mistake reproduced at
five levels: every attempt tried to separate a citation from a qualification by
*vocabulary* — a hedge word list (12 of 18 leaked), a citation allowlist (14 of 20
leaked, defeated by the session's own attacks), a noun-only list (`(line 0)` and
`(tests 0/0)` are identical in shape to the honest `(tests 21/21)`), and a
verifiable-path rule (absolute paths escaped the repository, 5 of 5). What finally held
was asking a question about the world: does this path resolve **inside the repository
the review is about**.

**Decision: two of the defects were found by attacking my own fix rather than waiting
for the reviewer**, and both had the same shape — *a transformation applied before the
guard that needed the original*. `lstrip('./')` ate the dots of `..` before the `..`
check ran, and the word test ran on the string after paths were stripped, so a hedge in
path shape was deleted before it was examined. Self-attack is now the first step after
any rule change here, not the last.

**One earlier claim of mine was wrong and is corrected in place.** `report-deepseek.md`
briefly recorded P0.2's AC1–AC4 as AGREED, citing a reviewer that had in fact reviewed
the *amendment document* and returned INADEQUATE. Its only `REVIEW_CRITERIA_JSON` line
was a **quoted counterexample inside a fenced block**, placed there to show that a
naive scanner would promote it. The validator refuses that line end-to-end, but the
session had read it by eye. **A reviewer's verdict is about the revision it saw, and
nothing else** — two artefacts, two reviews.

Three durable records now validate `acceptance-eligible`:
`docs/reviews/P0.1/`, `docs/reviews/P0.2/`, `docs/reviews/P0.3/`. 377 Python tests
across 8 suites, Release CTest 12/12, all 17 pinned generated files byte-identical,
identity verify 0. No generated or recovered guest code was touched.

### Two-harness workflow hardening — 2026-09-23

**Superseded by the entry above; kept for the reasoning at the handoff.**
**Stopped at the user's request; handoff: `docs/handoffs/20260923-p0-dsh.md`.**
P0.S is accepted. P0.1 final review disagrees on AC1–AC3's RECOMP_VBLANK coverage
and agrees on AC4/AC5. **Decision: leave P0.1 unaccepted and preserve the dispute
for DSH's persistent advisor, rather than silently override the reviewer.** The
earlier runtime-removal observation and broader active profile policy both need
to be considered. P0.2–P0.7 remain unimplemented. All changes are uncommitted.

P0.S is accepted after reviewer reproduction of all four criteria. The first new
strict baseline is `logs/runs/20260923-013448-357-p0-strict-baseline`, with verified
build and disposable-root identity. It exits through HalReturnToFirmware(2) after
1.92 seconds, without a dump. **Decision: record this as profile/provenance
evidence only; normal_exit/0 does not establish title success or liveness.**
P0.1 review adjudication is pending; the corrected profile regression suite is 19/19.

**Decision: install user-scoped Python `zstandard==0.25.0` to read original DSH
review logs.** `C:\Python313\python.exe -m pip install --user zstandard` succeeded.
This avoids relying on truncated projection-cache summaries; no GUI software or
user action was needed. Reading the originals is reconnaissance until P0.2 opens.

P0 initial integration: localized Release build passed, all **12/12 CTests passed**,
profile regressions **17/17 passed**, and all 17 pinned generated files stayed
byte-identical. The new native test exercised disposable root selection, real
toolkit partition/T/U/Z mapping, access denial, sentinels and cleanup. Two fixture
link failures were resolved after advisor consultation by linking real path and
symbolic-link code; only logging is stubbed in that fixture.

**Decision: withdraw strict-integration claims for the four named historical A2
runs while retaining their bounded structural/ABI/captured-memory observations.**
The classifier reproduced all four as exploratory with original metadata hashes
unchanged (`logs/p0-1-historical-profiles.log`). This reclassification preceded
the fresh strict baseline recorded above; P0.1's VBLANK disagreement remains open.

**Decision: execute P0 with Luna Max workers, with the advisor-approved disposable
save-root prerequisite before the first new strict run.** Current packet details
and acceptance evidence are in `docs/reviews/p0-1-execution.md`. The parent
configured/built localized native changes without a full translation or recovery
regeneration; generated-code provenance is a later P0 obligation and this harness
change does not justify replacing the guest baseline. The later isolated launch
is recorded above.

**Decision: use the current Luna Max / DeepSeek Flash Max worker roster, require
an actual reviewer invocation and same-child Astra continuation at startup, and
harden the P0 prerequisite contracts before resuming guest work.** Roster and
mechanics live only in `docs/agent-workflow.md`; AGENTS points to the startup gate.
The receipt template is `docs/session-start-template.md`. Luna Max workers were
used for plan editing and DSH-session analysis; Astra advised and passed a Codex
continuation check. DSH probes must still run inside a fresh DSH session.

This is workflow/plan delivery, not acceptance of P0 implementation or a new game
checkpoint. Preserve original historical reviews and classify their evidence rather
than replacing their provenance with current model names.

### Audit follow-up plan — 2026-09-22

**Decision: prioritize status/evidence reconciliation, interrupt-to-event delivery,
GPU action semantics, and translation entry/branch correctness in that order.**
The plan now begins with packets A1–A5, explicit acceptance criteria, Suggested
Agent labels and independent review gates. Historical next-step claims are
subordinate to that active sequence. As requested, unmet or ambiguous acceptance
requires checking with the advisor before dependent work or a change to criteria.
This update schedules the work; it does not claim any implementation packet passed.
Validation: documentation diff and packet/acceptance structure checked; no code,
build or runtime changes.

### Python bounded runner — 2026-09-21

**Decision: make `scripts/run-jsrf.py` the bounded launch/archive runner.** It
uses the stdlib JSON parser, so Windows PowerShell 5.1 and cmd.exe both work.
`scripts/run-jsrf.ps1` now only forwards to that file. `scripts/test-harness.py`
calls Python instead of `pwsh`. A 5-second capture
`logs/runs/20260921-000121-027-python-runner/` kept the expected
`0x00194ADD` stop (`0xE0424943`), dump_ok, checkpoints, and GPU analysis.

### Orchestrator effort — 2026-09-21

**Decision: run the Grok 4.6 parent orchestrator at xHigh.** Packet
decomposition in this repo is ABI and evidence work. Subagents inherit the
parent model, so a High-only leaf belongs in a separate High session.
Independent review stays a fresh High context. See `grok-role-map.md`.

**Decision: the parent orchestrator is now Grok 4.7 high.** This replaces
the Grok 4.6 xHigh parent. Packet roles stay the same. `spawn_subagent`
still inherits the parent, so children of this session are Grok 4.7 high.
Review stays a fresh context. See `grok-role-map.md`.

**Decision: the parent orchestrator is Grok 4.7 xhigh, and it reviews.**
This replaces the short-lived Grok 4.7 high parent. Children inherit xhigh.
A spawned reviewer is a fresh context, not a resume of the implementer.

### Grok 4.6 role map — 2026-09-20

**Decision: Keep the Codex packet roles and escalation gates, and map them
onto Grok 4.6 High / xHigh in `grok-role-map.md`.** High is the default
orchestrator and worker budget. xHigh is the expert gate (two High failures,
or ABI / weak-memory / multithreading / contradictory evidence) and plan-mode
architecture. Review is a fresh High context. The parent session orchestrates
because Grok children cannot spawn children; raise `/effort xhigh` on the
parent for the current `0x00193D90` callback/signal/reentry packet.

This is a workflow overlay only. No runtime work, build, test, game run or
status claim changed in this entry.

### Current delegation policy — 2026-09-19

**Decision: Use the user's standing delegation authorization to route JSRF
implementation packets to price-conscious Luna workers by
default, with narrow escalation gates.** Sol low effort decomposes 11b and
reviews evidence; Luna medium effort handles bounded disassembly, approved
recovery, ABI/behavior tests, offline harness/decoder work, documentation and
localized fixes. Routine GPU/register/mapping implementation moves to Luna once
the contract is clear. Home Qwen may prepare optional parallel log, symbol,
reconstruction or test drafts, but no critical prerequisite depends on it.

Sol medium effort is reserved for the same failure after two Luna attempts or
concrete ambiguity in ABI/translation, weak memory, multithreading, subsystem
boundaries or contradictory evidence. Astra medium effort is reserved for a
wrong architecture, interface or task graph and returns implementation to
Luna. Terra low effort reviews meaningful changes and may batch closely related
packets before their combined result is accepted or a dependent milestone starts.
Workers run their own isolated acceptance checks before that review.
Every packet must identify its milestone, expected outcome, source range/files,
facts versus hypotheses, ABI constraints, baseline artifact, exact acceptance,
file ownership and the single owner for build/regeneration/run.

This is a workflow change only. No runtime work, build, test, game run or
status claim changed in this entry; the game remains at the documented 11a
handoff with 11b next.

## Current status — verified checkpoint, 2026-09-12

### GPU harness pass complete; handoff refreshed

**Decision: Add frozen GPU snapshots, bounded offline packet inspection and
controlled progress/stall/corruption/unreadable-memory probes before extending
the renderer.** Reports identify synthetic fixtures, actual acknowledgement mode,
unreadable values and decoder limits. They do not infer execution from GET alone.

All **13 harness probes and 5 Release CTests pass**. New coverage includes 20
offline inspection tests and native D3D11 clear/copy/query/readback of all 4,096
pixels. Both WARP and the NVIDIA RTX 4070 Laptop GPU pass with zero debug errors.
The runner archives GPU snapshots and generated reports; build identity now checks
collector/game/native-fixture binaries and matching symbols as well as source.

**Decision: Disable only GPU acknowledgement mutations in the synthetic probes,
keeping kernel/APU clocks running.** `RECOMP_GPU_ACK=0` is opt-in; ordinary game
behavior remains unchanged. The four fixtures verify progress observations,
a stalled waiting thread, truncated packets and an unreadable aperture.

**Decision: Validate native graphics tools with a separate headless D3D11 fixture.**
It proves clear/copy/readback and capture plumbing; it does not prove guest
rendering, shaders, draws or presentation. RenderDoc 1.46.0 was installed with
winget (`BaldurKarlsson.RenderDoc`); the user installed Nsight Systems 2026.5.1.
The D3D11 debug layer was already available. No further installation is required.

RenderDoc capture is verified at
`logs/native-gpu/20260912-231530-540/renderdoc_capture.rdc`: the fixture confirmed
capture and converted XML contains the resource, annotation, ClearRenderTargetView
and CopyResource. The Nsight attempt in that directory failed from an argument
bug; it is superseded by the corrected run below.

Nsight produced `logs/native-gpu/20260912-231601-345/nsight.nsys-rep`.
**Decision: Record Nsight as partial tracing, because report creation alone does
not prove complete profiling.** Its exported SQLite has two D3D11 annotation
entries but warns 'DX11 profiling might have not been started correctly.' It
reports 11 DX11 events. Full D3D11 API timing remains unverified; the wrapper
currently validates report creation only. This does not block the GPU contract
audit. Exact commands, paths and remaining limits are in AGENTS.md.

Final ordinary run: `logs/runs/20260912-231756-543-gpu-harness-final/`.
It retains the expected missing helper `0x00194ADD`, six native threads, 52 named
frames, successful dump/analysis, one GPU snapshot and no dropped snapshots.
All 616 initializer words independently match the XBE again. GET/PUT are zero:
`submission_unestablished` is the correct report before GPU submission. There
is still no game frame. The original game checkpoint below is historical.

**Decision: Finish with a detailed AGENTS.md handoff and defer further renderer
work to the next session, following the user's remaining-usage request.** The
guide now records source ownership, commands, all GPU probe expectations, tool
versions, capture evidence, limits and a bounded next 11a packet. At that
historical point the pass was uncommitted on top of game `185a564` and toolkit
`b3de85c61f5fef002eabf57828c43786e62b6b60`; current committed checkpoint hashes
are recorded in `AGENTS.md` and below.

### Verified game checkpoint before this harness pass

The first memmove failure, 14 missing startup initializers, and the observed
heap hang are repaired. Two translator defects caused incorrect reference
counting and GUID comparisons; both have compiled regressions. Recovery now
includes 20 functions and six corrected parent boundaries. The game passes
video queries and allocates a 512 KiB GPU pushbuffer. There is no game frame yet.

The current first stop is missing GPU hardware setup at `0x00194ADD`, called
inside `0x00192090`. Latest ordinary run:
`logs/runs/20260912-224831-990-gpu-memory-capture-verified/`. It captures the expected
fatal diagnostic, six native threads, 52 named frames and a valid dump, now
including the separate physical/pushbuffer memory and register backing.
All 616 initializer words still match the original XBE. This is a reproducible
failure checkpoint, not successful game startup.

Verification: all three Release CTests pass (36,900 memmove, 288 atomic, 300
string comparison cases, WBINVD helper/flag coverage and 11 GPU register/clock
contracts); 27 toolkit unit tests pass;
all nine harness runs pass their expected outcomes. Evidence:
`logs/harness-test-results.json` and the archived runs it names.

The harness is sufficient for the next startup investigations: it captures
all-thread native stacks, guest state, lock/wait histories, worker crashes,
deadlocks and spins. A forced dispatch race failed before its TLS fix and
passes afterward. Complete guest thread bootstrap, semaphore/multiple-wait
edge cases and wider scheduling perturbations remain open. Stack traces alone
do not establish race freedom.

**Decision: Treat GPU setup as a hardware-model milestone, preserving guest
device/resource layouts and actual command completion.** The next work is
documented as packets 11a–11d in the plan and AGENTS.md. The existing toolkit
acknowledgement worker advances some GPU state without executing commands;
that behavior must be reconciled with the NV2A backend before relying on it
for rendering or fence completion. No blanket success stub was added to pass
this boundary.

The register/MMIO/PCI/interrupt audit and next acceptance gates are recorded in
`docs/jsrf-gpu-setup-contract.md`. It identifies missing PFIFO/USER submission,
detached VRAM in the existing hook, and remaining GPIO/reset semantics. Milestone
11a has concrete evidence and verified early register contracts, but remains partial.

The build wrapper now regenerates reviewed recovered code and rejects TODOs
before compiling. The runner rejects stale source/executable identity. Original
game assets remain unchanged; source changes span the game and sibling toolkit.

### Local Git checkpoint

The user requested a local repository and commit. The game repository had
already been initialized for source diffs; this checkpoint creates its first
commit, including generated source, recovery manifests, diagnostics, tests and
these working documents. Original assets, saves, builds, dumps and logs remain
excluded. No remote publication is involved.

**Decision: Commit the companion toolkit fixes separately and record their exact
revision so the game checkpoint identifies both halves of the working port.**
Toolkit commit: `b3de85c61f5fef002eabf57828c43786e62b6b60`. Existing unrelated
`xboxrecomp/recomp_run.log` remains untracked. The earlier staged-only Git entries
in this historical report are superseded by this local committed checkpoint.

### GPU dump coverage improvement

The current toolkit allocates its contiguous physical window separately from
canonical guest RAM. The existing dumps therefore lacked actual pushbuffer
bytes, even though low-memory device fields were present. The failed read is
saved in `logs/gpu-dump-before.txt`.

**Decision: Add the separate 64 MiB contiguous window (including reserved GPU
instance memory) and the current 16 MiB register backing to frozen captures.**
Capture only fully committed readable ranges; a future trapped MMIO model must
provide its own state snapshot instead. Extend the harness with a marker in a
real contiguous allocation and verify its bytes in the dump. These are distinct
allocations today, so this does not duplicate canonical RAM aliases.

Verified: all nine harness runs pass again, including actual marker bytes in
the added dump range. The final ordinary run also allows reading guest
`0x80001000` from its dump. Existing all-thread capture and initializer checks
still pass. Dumps are larger because these independently allocated regions
were previously omitted. Future trapped GPU state still needs a model snapshot.

### GPU interface audit and clock regression

The standalone NV2A reset sets PLL coefficient `0x00011C01` but calculates its
clock without that coefficient's divide-by-two factor. The new register-only
compiled fixture failed before the fix: rewriting the unchanged register changed
466666648 Hz to 233333324 Hz. Reset now uses the existing register-write path.
All 11 register/clock checks pass afterward; evidence is in
`logs/gpu-registers-before.txt` and `logs/gpu-registers-after.txt`. This backend
is not yet connected to ordinary game startup, so this is a backend correctness
fix, not a claim of additional guest execution.

**Decision: Correct and test the existing GPU register contracts before wiring
them into guest startup.** The toolkit's PFIFO/USER submission path is still
stubbed; its MMIO hook allocates detached VRAM and does not by itself install
a complete interception path. Merely enabling that hook cannot establish GPU
execution. Preserve these as explicit integration requirements.

### Pushbuffer allocation and ABI instrumentation correction

The recovered device initializer now reaches real pushbuffer allocation at
`0x00191020`. Its first run stopped in the diagnostic wrapper: ESP and
EBX/ESI/EDI all matched, but `g_ebp` changed inside an existing heap callee.
In this toolkit, generated EBP values are C locals; `g_ebp` is published frame
metadata and is not consistently restored on return. The original pushbuffer
routine does not modify architectural EBP.

**Decision: Check only the authoritative shared registers and ESP in recovered
routine wrappers, and label that coverage explicitly.** Keep the extra metadata
check for straight-line initializers, which do not call frame-establishing
callees. Do not restore metadata or alter guest behavior to make the checker
pass. A general architectural-EBP audit remains separate work.
Evidence: `logs/runs/20260912-222558-532-pushbuffer-allocation/`.

### Verified display contract and GPU initialization

Video probe `logs/runs/20260912-222114-713-video-contracts-verified/` passes:
24 mode entries, groups of four formats, 212-byte caps copy and boundary
canaries, valid/invalid capability results, out-of-range mode rejection, and
all tested guest return contracts. All nine harness probes subsequently pass
(`logs/harness-test-results.json`).

Ordinary startup now reaches the device initializer `0x00192090`, called by
`0x0018E460`. The captured guest parameters request width 640, height 480,
format `0x11`, and two back buffers. The guest device is at `0x0019B200`.
Artifact: `logs/runs/20260912-222116-464-video-capabilities/`.

**Decision: Preserve the guest device and resource layout while recovering
initialization; do not store native COM pointers in its 32-bit fields.** The
toolkit has D3D11 and NV2A components, but their existence is not evidence that
this game's submission path is connected. The current memory-layout worker
acknowledges busy bits and DMA_GET without proving rendering completion.

**Decision: Lower this path's WBINVD to an explicit host memory-ordering barrier
for the current coherent-memory model.** The recovery gate correctly rejected
a generated TODO at guest `0x0019243A`. No guest CPU cache is modeled; this
barrier orders host accesses and does not claim GPU command completion. Native
cache invalidation is a privileged operation and will not be executed.
See [Microsoft MemoryBarrier](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-memorybarrier)
and [WBINVD intrinsic](https://learn.microsoft.com/en-us/cpp/intrinsics/wbinvd).

### Silent stubs and video-mode recovery

Instrumenting all 541 unresolved direct-call stubs revealed an earlier missing
internal block at `0x00154DAA`, before the previously suspected XPP initializer.
The parent `0x00154D70` ended too soon. Extending it through the actual RET 12
restored this branch; it is not treated as a new function entry.

**Decision: Apply reviewed parent-boundary corrections in a dedicated manifest,
and regenerate only affected functions.** `config/boundary-fixes.json` records
branch and epilogue evidence. `scripts/relift-selected.py` applies instruction
and boundary corrections; `scripts/recover-functions.py` also includes those
boundaries in the merged database for future regeneration.

The next D3D path needed two real functions, `0x001935C0` (cached video setting)
and `0x001935DF` (mode table selection). Both execute and pass checked ESP and
nonvolatile returns. The count/enumeration functions `0x00199830` and
`0x001998D0` also end before their internal loops; reviewed extensions include
their epilogues and the enumeration jump-table targets. A small mode-flag
conversion routine at `0x00193580` is recovered from its original instructions.

**Decision: Continue through actual video initialization before selecting a
graphics replacement boundary.** These functions establish the game's display
requirements. Skipping them would discard useful evidence about its renderer.
No claim of a functioning host graphics device is made yet.

### Reference-count branch defect confirmed

The focused heap trace shows an actual repeated free of `0x0105D970`.
A diagnostic stop before its second consecutive free preserves the caller:
`sub_0015FD40` (release) -> `sub_00160EA0` (destructor) -> CRT free.
Artifact: `logs/runs/20260912-215944-093-heap-double-free/`.
Original `lock xadd [eax], edx; jne ...` branches on the updated count;
the generated branch reads `_flags`, initialized to zero and never updated.

**Decision: Fix LOCK-prefixed atomic flag handling in the translator, and
snapshot the atomic sum for later branches.** Re-reading the shared count
would introduce a race even if it appeared to fix this single-thread failure.
Regenerate only functions containing the affected operations, then verify
reference-count transitions and the ordinary boot path.

### Heap hang passed; next translation defects

Release CTests pass: 36,900 memmove cases and 288 atomic result/flag cases.
The original translator fails the compiled atomic regression when an
intervening write changes the count; evidence is under `logs/lifter-before/`.
After relifting 82 atomic-operation functions, the game passes the old heap
loop and reaches later initialization. Artifact:
`logs/runs/20260912-220538-767-atomic-flags/`. That run ends in a later access
violation after invalid indirect calls; it is progress, not successful boot.

**Decision: Stop on the first unresolved or invalid indirect call by default.**
The old fallback skips the call and continues with guessed stack recovery,
which can obscure the cause. `JSRF_ALLOW_UNRESOLVED=1` remains an explicit
investigation opt-out; ordinary runs will capture the first bad call.

**Decision: Correct repeated word/dword comparison flag tracking before
following later invalid pointers.** The observed GUID comparison sequence
`xor eax,eax; repe cmpsd; sete al` currently uses the old XOR result, so it
can report unequal GUIDs as equal. Tests include each mismatch position,
backward direction, and zero-count preservation of the incoming zero flag.

## Historical work record

These entries describe earlier states; the current status above supersedes
phrases such as 'awaiting verification' below.

## Harness work in progress

**Decision: Build an external Windows debugger/collector for crash and timeout
capture.** An in-process handler can become blocked by the same locks as the
game. The collector will stop all threads at a debug event, preserve their
native stacks, and write a dump before terminating the launched process.
Handled first-chance exceptions must continue to the game normally.

**Decision: Preserve optimized Release behavior while adding PDB symbols and a
linker map.** Changing optimization can hide bugs; matching symbols let us
resolve the actual failing build. Run archives will retain executable and
symbol identity rather than relying on whatever is currently in `build`.

**Decision: Include canonical guest RAM once in dumps, rather than every RAM
mirror.** This should preserve guest objects/stacks without duplicating the
same physical memory across the Xbox address aliases. Native thread stacks
and normal minidump metadata will also be retained. This needs verification.

**Decision: Establish local version control and preserve a source snapshot
before further edits.** Created `.gitignore` excluding original assets, saves,
logs, build outputs and analysis output. Initialized Git and staged an initial
source index for local diffs; no commits or remote publishing performed.
Pre-harness snapshot: `logs/snapshots/before-harness.zip`.

Implemented but not yet built/verified: compiler/linker symbol flags and
`tools/harness/collect.c`, the initial external collector. Runner integration,
thread registry, concurrent trace isolation and negative-test fixtures remain.

## Investigation record

### Startup-table recovery and stale-build protection

The first recovered initializer's 60 callback-table writes matched a small
reference interpreter reading the original XBE instructions. Nine adjacent
initializers have the same reviewed MOV/XOR/RET structure and separate
constructor-table references; their recovery is now built for validation.

An intermediate group build failed because a generated guest CRT declaration
(`flushall`) conflicted with a host stdio declaration. The subsequent run
accidentally used the previous executable. That artifact,
`logs/runs/20260912-214847-905-initializer-group/`, is **not evidence of the
group fix**. Removed the unnecessary generated declaration header from these
straight-line recovery functions.

**Decision: Add mandatory source/executable identity verification before a
run, plus a build wrapper that stops on errors.** This prevents repeating the
stale-executable mistake. The wrapper hashes sources before and after the
build, rejects edits during compilation, and records the executable hash.
The runner refuses missing or mismatched build identity. Use
`scripts/build-jsrf.ps1` for future builds.

New group artifact with verified build identity:
`logs/runs/20260912-215041-168-initializer-group-verified/`.
Recovery wrappers now verify guest ESP cleanup and preserved nonvolatile
registers on return; `scripts/verify-initializers.py` independently checks
the initialized words against original XBE instructions in the captured dump.

### Harness checkpoint and return to game startup

The forced kernel-dispatch test now passes:
`logs/runs/20260912-214122-414-dispatch-after/`. The expanded eight-probe suite
also passed; `logs/harness-test-results.json` lists every artifact directory.
The final ordinary game smoke run preserves its prior boot path and captures
the heap hang: `logs/runs/20260912-214337-093-harness-ready/`.

**Decision: Resume sequential startup recovery with the verified collector,
while retaining broader concurrency coverage as explicit open work.** The
fixture suite verifies native workers with guest stacks/TIBs, event bridges,
dispatch isolation, crash/deadlock/spin capture and thread histories. It does
not yet prove the complete guest thread bootstrap, all semaphore/multiple-wait
cases, or arbitrary races. The plan keeps those gaps open under 01c/01d.

**Decision: Recover `0x0018AFB0` as real translated code, using a small generated
recovery file rather than regenerating every function.** The original XBE
confirms a constructor-table reference at `0x001EB7E4`, a distinct aligned
entry after a preceding tail jump, and a RET at `0x0018B192`. Its body populates
callback tables near `0x00257CF0`. This is a real missed initializer, unlike
the internal memmove label fixed earlier. Incremental recovery will preserve
the existing working generated code and shorten rebuilds.

### Confirmed kernel-dispatch concurrency defect

A controlled two-thread test failed before the fix:
`logs/runs/20260912-214035-373-dispatch-before/` reports game exit 1 and a
missing `probe_dispatch` checkpoint. One thread selected
`KeRaiseIrqlToDpcLevel`; another selected `KeQueryInterruptTime` before the
first invoked its saved function pointer. A shared `g_kernel_dispatch_slot`
made the first thread execute the second thread's selection.

**Decision: Make the kernel dispatch selection thread-local, and preserve the
forced-interleaving test as a regression.** This is a demonstrated runtime
correctness defect, not merely missing log detail. Also isolate per-thread
kernel diagnostic counters and use the frozen per-thread histories instead
of the older live lock-owner report when the new diagnostics are enabled.

The original seven collector probes passed before this extension, and the
ordinary game run still reached its prior startup path with a successful dump:
`logs/runs/20260912-213837-112-thread-harness-game/`. Expanded probes now also
exercise event signal/reset/timeout behavior through the actual imported
kernel bridges and verify guest stack cleanup.

### First useful diagnosis from native stacks

The collector baseline resolves the main thread to `sub_001497DC`, through
`nh_malloc` / `malloc_0017C953` and game initialization. This identifies the
current CPU hang as a guest heap-allocation path. Other captured threads include
the toolkit's NV2A acknowledgement and kernel timer workers.

**Decision: Validate the new diagnostics with deliberate failures before
modifying that heap path.** The known missing startup initializers could affect
heap state, and a named stack by itself does not establish the corruption cause.

Implemented, awaiting build/probe verification: thread-local legacy ICALL rings;
stable per-thread diagnostic registry; bounded timestamped call/tail/lock/wait
history; guest-stack bounds and register locations; thread start/exit hooks;
and explicit healthy, worker-crash, deadlock, spin and handled-exception probes.
The collector now includes the registry and active guest-register values in
dumps, in addition to canonical guest RAM. Probe histories retain exited-thread
records without dereferencing their expired TLS addresses.

External capture follows Microsoft's recommendation to dump unstable processes
from another process: [MiniDumpWriteDump documentation](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-minidumpwritedump).

### Collector baseline verified

The corrected external-collector baseline completed with the expected
`diagnostic_deadline` outcome, both boot checkpoints present, seven captured
native threads, 57 named frames and a successful dump. Artifacts:
`logs/runs/20260912-213032-869-collector-baseline2/`.

The first attempt found two harness defects, now corrected: GUI-subsystem
stdout was not a valid stream for duplication, and a null/empty log could
produce no missing-checkpoint entries. No guest fix was inferred from that
failed attempt. Symbols and the dump now need the negative-test fixtures.

**Decision: Keep guest-thread diagnostic slots stable for a run and inspect
them only while the external debugger has stopped every thread.** This avoids
live reads of changing thread-local state and reuse of exited-thread pointers.
Each thread will write its own bounded history; overwritten entries and capacity
overflow will be reported explicitly. Thread registry integration is underway.

- No claim of additional game progress yet.
- Existing watchdog/raw guest-stack output remains useful but is not an
  all-thread or unwound stack trace.
- Existing shared ICALL diagnostics are not yet safe for concurrent writers.
- New collector must pass controlled worker-crash, handled-exception, deadlock
  and spin tests before its output is trusted for game diagnosis.

## Milestone 11a: GPU setup contract audit (documentation-only)

Audited original `0x00194ADD..0x00194C3F` and callees `0x0019460A`,
`0x00194635`, `0x00194676`, `0x00194780` against pinned xemu
`75650bd8cd91945f7b79774e2cee0b200ca373ff` and the toolkit `src/nv2a` register
definitions. No rebuild; no stubs; the helper was not faked to reach the next
missing call. `docs/jsrf-gpu-setup-contract.md` now carries the full decoded
register map, facts/hypotheses split, and the proposed single state owner.

Key corrections to the earlier interpretation:

- `0xFD001800/1804/1808` are **not** bus 0/dev 3 PCI config. They are the
  NV2A's own PCI mirrors in the PBUS block (`NV_PBUS_PCI_NV_0/1/2`): device
  ID, PCI command (bit 2 = bus master), and revision. `0xFD600140` is the
  PCRTC block inside the same 16 MiB aperture (`NV_PCRTC_INTR_EN_0` = vblank
  enable, written 0).
- `0xFD009200/0x9210` are **not** the core PLL. They are
  `NV_PTIMER_NUMERATOR/DENOMINATOR`: the 31.25 MHz ratio (constant 0x1DCD650,
  reduced by repeated /2 and /5) programs the GPU master-timer divisors.
  `0x194780` then writes `NV_PTIMER_TIME_0/1` with 0 and a value derived from
  KeQuerySystemTime + RtlTimeToTimeFields. The exact counter encoding/effect
  is still open; the current toolkit/xemu PTIMER handler ignores TIME writes.
  `NV_PTIMER_ALARM_0` gets 0xFFFFFFFF with the alarm interrupt disabled.
- `0xFD10020C` is `NV_PFB_CSTATUS` (modeled VRAM size in bytes), not a VRAM
  window offset. `0xFD400100/140` are `NV_PGRAPH_INTR` (W1C clear) and
  `NV_PGRAPH_INTR_EN`; `0xFD400720` is `NV_PGRAPH_FIFO`; `0xFD002100/2140`
  are `NV_PFIFO_INTR_0` (W1C clear) and `NV_PFIFO_INTR_EN_0` (loaded from
  device field +0x118, which is 0 in the baseline capture, so PFIFO
  interrupts start disabled); `0xFD000140/0x200` are `NV_PMC_INTR_EN_0`
  (=1, hardware enable) and `NV_PMC_ENABLE` (=0xFFFFFFFF, all blocks).
- `HalReadWritePCISpace(1, 0, 0x4C, ...)` targets the NV2A endpoint's
  vendor-specific PCI config window (bus 1/dev 0 behind the AGP bridge),
  while the setup code separately accesses PBUS `0xFD00184C`. xemu models
  neither access, and the XBE alone does not prove that they alias. Sharing
  those bytes belongs to the proposed 11b state-owner design; the current
  zero-read/discard-write HAL stub does not satisfy the read-modify-write.
- `OUT 0x80C0, 1` is a PM GPIO[0] write (PM base 0x8000 presumed). Pinned
  xemu models only GPIO[0] reads (toggling field-pin bit 5) and the 0x16
  aspect write; every other GPIO write is ignored with a FIXME, and the PM
  base configuration path is compiled out, so port 0x80C0 is unmodeled in
  xemu. The electrical effect stays an open hypothesis.

**Decision: propose the toolkit `src/nv2a` register model as the single
owner of the 0xFD000000 aperture and all NV2A PCI state, with a separate
small PM owner for the 0x8000 IO region.** The PBUS 0x800..0x8FF mirror and
the bus 1/dev 0 HalReadWritePCISpace path must read and write the same
config bytes (no second copy); PTIMER owns the master timer and must gain
evidence-backed TIME-register write semantics; PFB_CSTATUS reports the modeled VRAM size bound to the real guest
allocations; the acknowledgement worker reconciles against this one state
and is retired for the registers it covers once the model is live. This is
a 11b implementation proposal; no code was changed in this audit.

**Decision: classify the unresolvable items as recorded hypotheses instead
of guessing.** The GPIO[0] electrical effect, the vendor-specific 0x4C..0x4F
bit meanings, constant 0xFE502A, and the 7th KeInitializeInterrupt argument
are documented as such in the contract doc. They do not block 11b, and no
invented GPU status or successful port write is claimed.

Open for a later packet: implement the single owner (11b), then recover the
unrecovered callees 0x196800/0x196967/0x196A65/0x194533/0x196B34/0x196B6A/
0x196B9A plus 0x196E80/0x197C50/0x197BCF so 0x00192090 can return with real
initialized state (11c).

New local reference: `logs/references/gpu/nv2a.c` (pinned revision, 24,265
bytes, verified: 16 MiB BAR0, revision 0xA1, INTA pin, RAMIN at 0x700000).

### Review correction — Qwen 11a audit, 2026-09-19

Reviewing the original instructions and register definitions found four
material overstatements in the Qwen-produced contract. The original XBE writes
`0xF800` to PBUS offset `0x80C`, which is PCI configuration dword `0x0C` and
sets the latency-timer byte to `0xF8`; it is not a PCI-status W1C clear. PBUS
offset `0x830` is the expansion-ROM BAR, not min-grant/min-latency. At
`0x00194797`, `AND 0xFFFFFCFF` clears mask `0x00000300`, not `0x00400000`.
Finally, `NV_PGRAPH_FIFO = 0` clears the documented FIFO-access bit; calling it
a reset/flush was unsupported.

The review also narrowed the PTIMER claim. The code writes zero and a
calendar-derived value to TIME_0/TIME_1, but those registers are encoded timer
views rather than established low/high 32-bit halves, and the current
toolkit/xemu write handler ignores both addresses. The contract now records the
raw writes and leaves their effect as an 11b evidence requirement.

**Decision: retain milestone 11a as a completed documentation audit after
correcting its register table, because its acceptance criterion is an exact
instruction/address inventory plus a proposed owner, while the newly exposed
TIME-write and vendor-register semantics are explicitly bounded 11b gaps.**
No runtime source was changed and no build was needed for this review.

Independent Terra review found one further fact/proposal mix-up: the XBE
accesses PBUS `0x184C` and PCI configuration `0x4C` separately, while current
xemu/toolkit handlers implement neither. The documents now state shared backing
as the proposed 11b owner design, not as a proven hardware alias. Terra found no
other material issue after these corrections.

### Suggested-agent plan pass — 2026-09-19

Every milestone row now has a Suggested Agent label governed by the escalation
rules in AGENTS.md. Completed rows use the role suitable for analogous future
work rather than claiming who performed the historical work.

**Decision: split broad work into contract-first Luna packets before assigning
an Expert.** Milestones 06–10 now separate evidence/contract work from bounded
implementation or runtime validation. Current GPU work is decomposed further:
11b has separate MMIO ownership, PCI paths, PTIMER, PFIFO/USER submission and
completion/acknowledgement packets; 11c separates helper contracts from reviewed
recovery; 11d separates callback-context proof from interrupt delivery. Luna is
the suggested implementer, Terra reviews meaningful changes, Sol Light sequences
the parent milestone, and Sol Medium appears only as the documented escalation
when timer or concurrency evidence is genuinely contradictory.

## Milestone 11b1: single NV2A register owner — 2026-09-19

The first Luna implementation built and reached the baseline failure, but its
`PAGE_GUARD` aperture was unreadable to the collector: every captured register
was null. The second attempt added a shadow and disabled legacy acknowledgements,
but independent review found that its probe called the decoder directly instead
of exercising VEH, `PAGE_NOACCESS` could be swallowed, guard pages left a
same-page concurrency window, decoder flags were incomplete, and the shadow had
no coherent publication contract.

**Decision: after two Luna attempts failed the same acceptance boundary,
escalate 11b1 to Sol Medium and replace the guard design instead of adding more
exceptions to it.** The expert implementation keeps the BAR permanently
`PAGE_NOACCESS` while active, serializes every supported access through one SRW
owner lock, atomically disables and drains the legacy GPU mutator first, and
leaves its kernel/APU clock work running. Supported 8/16/32-bit MOV, TEST, CMP,
OR and AND forms now have checked bounds and architectural flags; unsupported,
cross-page and 64-bit forms remain diagnostic failures.

Frozen GPU reports now read an explicitly published model snapshot with an
odd/even generation protocol. The actual-dereference probe covers repeated
widths, RMW flags and two concurrent same-page workers. The unreadable probe
turns ownership/publication off and still captures null registers. Terra's
final lifecycle finding added idempotent teardown that disables publication,
clears the aperture pointer and restores `PAGE_READWRITE` before the mapping is
released; a dedicated lifecycle probe verifies post-teardown direct access and
dump capture.

Acceptance evidence:

- Release CTest: 5/5; toolkit unit tests: 27/27.
- Full harness: 14/14, recorded in `logs/harness-test-results.json`.
- Owner: `logs/runs/20260919-210732-912-11b1-expert-published/`.
- Unreadable: `logs/runs/20260919-210734-665-11b1-expert-unreadable-final/`.
- Lifecycle: `logs/runs/20260919-211211-700-test-gpu-mmio-lifecycle-0/`.
- Exact ordinary run: `logs/runs/20260919-210749-909-11b1-expert-final-exact/`;
  bounded deadline with valid dump/report, no new GPU boot-progress claim.

**Decision: mark 11b1 complete and continue with 11b2.** USER registers remain
storage-only; PCI HAL sharing, PTIMER TIME writes, command execution/completion,
ISR/DPC delivery and rendering are explicitly outside this checkpoint.

## Milestone 11b2: NV2A PCI configuration paths — 2026-09-19

The first Luna pass introduced one PCI backing for PBUS and
`HalReadWritePCISpace(1,0,...)`, but review found that writes could corrupt
immutable identity bytes, HAL bypassed the 11b1 owner lock, the test never
called the real HAL function, and mirror-crossing accesses were accepted.
The second pass fixed the first three and added a compiled HAL fixture. Its
boundary test still passed only because bytes beyond the mirror happened to be
zero, leaving the original range defect in place.

**Decision: apply the two-attempt escalation rule to the remaining PBUS range
defect rather than accepting a zero-valued false positive.** The expert fix
requires the complete 1/2/4-byte operation to fit configuration offsets
0x00..0x7F and uses nonzero sentinels on both sides of the boundary to prove
crossing reads return zero and writes change neither byte.

The completed contract keeps vendor/device and class/revision immutable,
supports the observed standard dword 0x0C and ROM BAR 0x30 writes, and shares
vendor offset 0x4C..0x4F between PBUS and the serialized HAL bridge by explicit
design. Unsupported buses/slots and invalid lengths/ranges retain zero-filled
read and ignored-write behavior. The new HAL fixture calls the actual bridge,
including the byte-0x4F update used by the game.

Validation: build wrapper passed; Release CTest 6/6; toolkit units 27/27;
NV2A fixture 25 contracts; HAL PCI bridge fixture passed. Bounded run
`logs/runs/20260919-212335-175-11b2/` retains the expected unresolved
`0x00194ADD`; 11b2 does not recover GPU setup or claim rendering progress.

**Decision: mark 11b2 complete and continue with 11b3 PTIMER semantics.**

## Milestone 11b3: PTIMER semantics — in progress 2026-09-19

The first Luna pass implemented the split 56-bit TIME registers, writable time
offsets, alarm pending/W1C behavior and PTIMER-to-PMC aggregation. Its focused
fixture passed 35 register contracts. Review did not accept that result: the
alarm depended on later PTIMER MMIO reads, enabling the reset/default alarm did
not evaluate an already-due deadline, a late poll advanced only one low-word
period, and the test used a host-time busy loop. The cited upstream source also
named moving `master`; the reference is now pinned to xemu commit
`f9b14039e5bb56ae2d8f028e31e7cc19f13f7e12`.

**Decision: keep 11b3 open and use Luna's second attempt to add deterministic
clock/service coverage and asynchronous alarm delivery before considering an
Expert escalation.** Acceptance requires expiry without another PTIMER MMIO
access, correct late-enable and multi-period behavior, PMC clearing after W1C,
and deterministic zero-divisor and TIME-offset boundary tests.

Luna's second pass added a deterministic clock and a public service function,
but repository-wide call-site inspection showed that only PTIMER MMIO handlers
and the fixture invoked it. The production runtime still could not deliver an
armed alarm while the guest waited without touching PTIMER.

**Decision: after two Luna attempts, escalate 11b3 to Sol Medium because the
remaining defect is the timer's runtime scheduling and owner-lifecycle
integration, not another isolated register calculation.** The Expert packet
must connect servicing to the existing serialized NV2A owner, prove clean
startup/shutdown, and retain deterministic boundary tests.

The Expert pass added a wakeable PTIMER service under the existing owner lock,
pinned behavior to xemu commit `f9b14039e5bb56ae2d8f028e31e7cc19f13f7e12`,
and expanded the core fixture to 45 contracts. Review then caught an event-handle
shutdown race and a torn snapshot read. The first full harness run also caught
the debugger freezing publication at odd generation 485 because unchanged timer
and USER reads republished too often.

The corrected owner detaches and signals the private timer event under the owner
lock before join/close, validates runtime snapshot pairs with the same even
generation, stores USER registers outside the exported snapshot, stages values
under the lock, and keeps the odd publication window to a bounded commit only
when visible state changes. The PTIMER runtime probe is now a required member of
the standard harness rather than a separately run artifact.

Acceptance evidence:

- Guarded build passed; Release CTest 6/6; toolkit unit tests 27/27.
- The NV2A fixture passes 45 deterministic register/clock contracts.
- Full harness passes 16/16, including `gpu-ptimer-runtime` at
  `logs/runs/20260919-220551-120-test-gpu-ptimer-runtime-0/`.
- Publication regression evidence:
  `logs/runs/20260919-220213-448-11b3-stall-publish-final/`, whose three
  snapshots all retain stable generation 6.
- Ordinary run `logs/runs/20260919-220450-611-11b3-final/` produces a valid
  dump/report and still stops at the expected unresolved `0x00194ADD`.

**Decision: mark 11b3 complete and continue with 11b4 bounded PFIFO/USER
submission.** This checkpoint proves timer state and delivery; it does not
claim command execution, GPU setup return, presentation or rendering.

## Milestone 11b4: bounded PFIFO/USER submission — in progress 2026-09-19

**Decision: split 11b4 so Luna first implements a checked guest-memory reader
and bounded packet/control decoder with a recording sink, exact GET movement and
fatal unsupported-state diagnostics.** Method execution and completion remain
separate acceptance boundaries: this first packet may not route arbitrary
methods into D3D11 or treat parsing as completed GPU work. The supported and
blocked probes must use the real `0x80000000` contiguous pushbuffer through the
PAGE_NOACCESS USER aperture.

Luna's first pass added a core decoder and four focused checks, but review found
that the real MMIO owner still shadowed the entire USER range, so no guest write
could reach it. The core test also used high-bit guest VAs while the existing
fixture and offline decoder define GET/PUT as physical byte offsets. Additional
gaps included stale published GET, a parameter-loop budget escape, a one-entry
"sink," incomplete `(pc, return)` loop identity and no real trapped-MMIO probe.

**Decision: keep 11b4a open for Luna's second attempt because the failures are
specific integration and coverage defects with a now-clear contract.** The
correction must route only recognized USER registers into the core, retain
physical GET/PUT offsets, publish GET and PUT coherently, record a bounded method
stream, and put both supported and blocked PAGE_NOACCESS submissions into the
standard harness.

Luna's second pass corrected the owner routing, physical-offset representation,
coherent GET/PUT publication, parameter budget, method sink and loop identity.
Repository inspection still found no new runtime probe selector or harness case;
the only USER writes remained the older synthetic progress fixture, which does
not assert the decoder sink or blocked diagnostic contract.

**Decision: apply the two-attempt escalation rule to 11b4a.** Sol Medium now
owns the real trapped-MMIO supported/blocked probes, full packet/control edge
matrix and audit of atomic GET/sink behavior. This keeps the decoder out of the
accepted path until the installed owner is tested end to end.

The Expert pass made submission atomic across the complete PUT range: GET, the
bounded `(subchannel, method, parameter)` sink, last-method state and accepted
stream count change only after every packet validates. The decoder uses physical
byte offsets into the exact 64 MiB contiguous mapping and has explicit failures
for invalid pointers and targets, unreadable spans, reserved/truncated packets,
loops, word/packet budgets, method overflow and sink capacity. It implements the
offline decoder's reviewed increment, non-increment, jump and one-slot
call/return forms; it does not call the renderer or signal completion.

Terra found that the first call/return test stopped at PUT before executing its
RETURN. The corrected layout calls beyond a distinct PUT, executes RETURN,
resumes at the saved PC, records the expected method and reaches PUT. Terra's
final review found no remaining issue.

Acceptance evidence:

- Guarded build passed; Release CTest 6/6; toolkit unit tests 27/27.
- NV2A fixture passes 115 deterministic contracts.
- Full harness passes 18/18. Real trapped-MMIO artifacts:
  `logs/runs/20260919-222516-506-test-gpu-submit-supported-0/` and
  `logs/runs/20260919-222519-082-test-gpu-submit-blocked-0/`.
- Supported submission publishes GET=PUT=0x18, four recorded methods and one
  accepted stream. Blocked submission retains GET=0, an empty sink and zero
  accepted streams with `reserved_opcode` at PUT=0x0C.
- Ordinary run `logs/runs/20260919-222542-637-11b4a-final/` retains a valid
  dump/report and the expected unresolved `0x00194ADD`.

**Decision: mark 11b4a complete and split remaining 11b4 work into 11b4b, a
reviewed method-effect subset.** Completion and acknowledgement ownership remain
11b5; decoding or recording a method is not completed GPU work.

## Milestone 11b4b1: first method boundary — 2026-09-19

**Decision: begin method execution with NV097 `NO_OPERATION` 0x0100 on
subchannel 0 only.** The current model has no object/class binding, so accepting
surface, draw or flip methods by number alone would falsely treat every
subchannel as NV097. NOP has no renderer effect and establishes the transactional
method gate without inventing class state.

The complete stream is validated before commit. Any method other than 0x0100 on
subchannel 0 produces `unsupported_method`, captures its exact subchannel,
method and parameter, and preserves GET, sink, last-method/parameter and accepted
stream count. Nothing calls `pgraph_method` or `pgraph_d3d11_method`, and no
completion or interrupt state changes. Attempted word/packet counters remain
diagnostic telemetry and are not accepted-work state.

Acceptance evidence:

- Guarded build and Release CTest 6/6 passed; the deterministic fixture covers
  115 contracts from the accepted 11b4a matrix plus the method gate.
- Terra found no functional issue. It confirmed that GPU probes intentionally
  require `probe_gpu`, while ordinary runs retain the default `guest_entry`
  checkpoint.
- Current full harness passes 18/18. NOP artifact:
  `logs/runs/20260919-223753-066-test-gpu-submit-supported-0/`; unsupported
  artifact: `logs/runs/20260919-223755-987-test-gpu-submit-blocked-0/`.
- The blocked real trapped-MMIO stream reports subchannel 5, method 0x0180,
  parameter 0xABCDEF01 with GET=0, sink=0 and zero accepted streams.

**Decision: mark 11b4b1 complete and make 11b4b2 recover observed class binding
before adding any state method.** Draws, clears, flips, renderer calls and
completion remain outside this checkpoint.

## Milestone 11b4b2: class binding and state methods — in progress 2026-09-19

Luna's first pass added a fixture binding seam and six surface-state methods,
but review found that the seam preinstalled active binding, so `SET_OBJECT` was
optional. The advertised real probe was absent from the runner and standard
harness. Binding rollback, rebinding and subchannel isolation were untested, and
all 32-bit surface parameters were accepted without a proven validation contract.

**Decision: keep 11b4b2 open for Luna's second attempt and narrow methods if
their parameter contract cannot be proven.** The fixture seam must be an object
handle-to-class lookup gated to fixture mode; only a transactional `SET_OBJECT`
packet may activate a subchannel. Acceptance also requires the bound probe in
the standard harness and rollback of binding plus every staged PGRAPH value.

Luna's second pass separated fixture lookup from active subchannel state and
made SET_OBJECT transactional, but still accepted format, pitch and offsets as
unchecked raw values. After two Luna attempts, the remaining issue was the
hardware parameter contract rather than fixture wiring.

**Decision: escalate 11b4b2 to Sol Medium and narrow the allowlist instead of
inventing format, pitch or offset validation.** The accepted effects are now
SET_OBJECT for an explicitly fixture-gated registered NV097 handle, NOP, and
clip H/V. Each clip register is exactly two 16-bit fields with no reserved bits,
so its full 32-bit domain is defined. Format, pitch, color offset and zeta offset
produce exact `unsupported_method` diagnostics and roll back the transaction.

Acceptance evidence:

- Guarded build passed; direct fixture passes 225 contracts; Release CTest 6/6.
- Full harness passes 19/19. Fresh bound artifact:
  `logs/runs/20260919-225819-468-test-gpu-submit-bound-0/`, with SET_OBJECT plus
  clip H/V, three sink records and GET=PUT=0x14.
- Tests cover disabled fixture execution, missing/invalid handles, binding,
  rebinding, subchannel isolation, packed boundary values, removed-method
  rejection and rollback of GET, sink, active bindings and both clip shadows.
- Terra's final review found the narrowed contract clean.

Ordinary run `logs/runs/20260919-225848-449-11b4b2-expert-final/` reached a
diagnostic deadline while another startup branch performed partition/save I/O.
It did not enter `0x00192090` or call `0x00194ADD`; GET/PUT stayed zero and the
unresolved fatal stub is unchanged. **Decision: record this only as a timed
branch observation, not GPU progress or evidence that `0x00194ADD` is solved.**

**Decision: mark fixture-scoped 11b4b2 complete and create 11b4b3 for the real
RAMIN/DMA object lookup.** No game method is accepted through the fixture seam,
and draws, clears, flips, renderer calls and completion remain pending.

## Milestone 11b4b3: real RAMIN lookup — evidence blocked 2026-09-19

Luna found that the NV2A owner still allocates a detached 1 MiB RAMIN, while
`MmClaimGpuInstanceMemory` returns guest instance memory near `0x83FE0000`
without connecting or populating RAMHT. Original execution in `0x00192090`
allocates device state and its pushbuffer, then calls unresolved `0x00194ADD` at
`0x00192135` before any usable object handle/class or packet setup. The fresh
ordinary capture did not enter this path, and the bound fixture is synthetic.

**Decision: do not invent RAMHT masks, alias detached memory or seed fake game
objects. Mark 11b4b3 blocked on 11c1 evidence and pivot to the exact
`0x00194ADD` setup-helper contract.** Once that path exposes the first real
instance-memory writes and handle/class, 11b4b3 can connect lookup with evidence.

## Milestone 11c1: `0x00194ADD` helper decomposition — in progress 2026-09-19

Original-code analysis confirms `0x00194ADD` is a no-stack-argument thiscall
initializer returning boolean EAX. It initializes DPC/dispatcher state, calls
four local setup helpers with exact failure edges, installs interrupt/shutdown
callbacks, writes three 256-byte identity ramps, performs OUT 0x80C0, updates
NV2A PCI byte 0x4F, and normalizes the final helper result. The parent has no
direct RAMIN write and remains blocked on deeper local helpers and imports.

**Decision: recover `0x0019460A` and `0x00194635` first as isolated leaves.**
Their complete original contracts cover base/register initialization and
identity/visible-memory outputs without unresolved callees. The parent remains
fatal, so this packet cannot be mistaken for a returning GPU initializer or
progress beyond `0x00194ADD`.

The first Luna pass generated instruction-equivalent bodies but added an
unnecessary signed-MMIO text rewrite and no test that called the wrappers. The
second pass removed the rewrite and added a 19-check direct ABI/MMIO fixture,
but linked all recovered functions with `/FORCE:UNRESOLVED`; MSVC emitted
LNK4088 and created an image despite unresolved symbols.

**Decision: after two Luna attempts, escalate the leaf test packaging to Sol
Medium.** Acceptance requires an isolated translation unit generated from the
same recovery source, a clean link with no unresolved suppression, the direct
19-check fixture and the complete CTest suite.

The Expert refactored recovery generation so each manifest entry's translated
body and checked wrapper are rendered once. Production `recovered.c` and the
manifest-selected `recovered_11c1_test.c` consume those same generated strings.
The focused target contains only the two leaves and links normally, without
`/FORCE`, broad stubs or unrelated recovery bodies. Terra's final review found
the packaging and production dispatch clean; `0x00194ADD` remains fatal.

Acceptance evidence: guarded build passed; the direct fixture passes 19/19 ABI
and MMIO checks; Release CTest passes 7/7; a clean focused rebuild reports zero
warnings/errors. Manual-function entries are retained to prevent duplicate
generated bodies and preserve future direct-call routing.

**Decision: mark the two 11c1 leaves complete and next decompose the six direct
dependencies inside `0x00194676`.** These leaves are not runtime GPU progress
because the unresolved parent still prevents reaching them.

Analysis of `0x00194676` found a deterministic seven-helper sequence with no
checked helper return. `0x00196800` is a self-contained PLL coefficient decoder
whose outputs feed the parent's PTIMER divisor reduction. `0x00196967` then
calls `MmClaimGpuInstanceMemory`, derives the guest instance addresses, programs
device offsets 0x2210/0x2214 and clears exactly 0x5000 bytes. It is the first
helper relevant to real instance backing, although it still creates no RAMHT
handle/class entry. Later helpers configure ports/register tables and defaults;
`0x00196B6A` clears two instance-derived dwords.

**Decision: implement and directly test `0x00196800` next, then recover
`0x00196967` before revisiting 11b4b3.** This order validates the parent's clock
inputs first and then reaches the allocation boundary without conflating either
step with RAMHT object creation.

### Accepted leaf `0x00196800..0x00196967`

The reviewed recovery decodes all three original PLL coefficient words, preserves
the guest ABI, retains the original zero-divisor path, and reproduces the preceding
context/MMIO side effects. The focused fixture passes 50 checks, including exact
field extraction, sequential truncating division, nonvolatile preservation, RET
cleanup, and a multiplication-overflow case. Release CTest passes 7/7.

Terra found that the first generated body used signed C multiplication for x86
`imul`, which is undefined when the product overflows, and that the fixture omitted
three observable prologue effects. The second Luna pass moved a scoped low-32-bit
multiply correction into `scripts/recover-functions.py`, regenerated both bodies,
and added the missing sentinels. Terra's re-review is clean.

**Decision: accept `0x00196800` only after replacing undefined signed overflow
with explicit x86 low-32-bit multiplication semantics, then proceed to
`0x00196967..0x00196A65`.** The accepted leaf does not establish GPU submission or
RAMHT state; it supplies the parent's clock inputs and tested original side effects.

### Accepted leaf `0x00196967..0x00196A65`

The recovered helper now claims exactly 0x5000 bytes through the original kernel
import, derives the original instance addresses and masked register values, clears
the exact inclusive-exclusive allocation range, clears device control bit zero,
and calls unresolved internal helper `0x00194520` with argument 8. Its return is
stored at context offset 0x13C. The focused fixture passes 81 checks across both
hardware-bit branches, clear-bound sentinels, 32-bit address arithmetic, ABI and
dependency calls; Release CTest passes 7/7 and the production executable links.

The allocation import and internal helper overrides exist only in the focused
test executable. Production retains fatal unresolved traps for `0x00194520` and
the parent `0x00194ADD`. Terra confirmed that the production and focused bodies
are identical and that neither seam changes real routing.

**Decision: accept `0x00196967` as verified instance-memory setup while keeping
`0x00194520` fatal, then recover the self-contained `0x00196A65..0x00196B34`
port/register initializer.** This establishes allocation-derived state but still
does not create a real RAMHT handle or class binding for 11b4b3.

### Accepted leaf `0x00196A65..0x00196B34`

The recovered initializer preserves the original selector/value byte sequence,
both branches based on the saved original byte, the two staged register masks,
and the final device-control mask. The focused suite remains at 81 checks and
exercises both branches, nontrivial masks, adjacent-byte isolation and ABI state;
Release CTest passes 7/7. Terra found the generated production and focused bodies
identical and the implementation exact to the original disassembly.

The fixture observes final port bytes rather than the intermediate write order.
The generated instruction sequence itself matches the original; enforcing every
intermediate byte write would require a write-tracing MMIO fixture. This does not
block the straight-line recovery, but remains a stated harness limit.

**Decision: accept the port/register leaf with its intermediate-write tracing
limit explicit, then recover the independent identity-ramp leaf
`0x00194533..0x00194570`.** Production continues to trap the parent and
`0x00194520`, so this acceptance makes no runtime-success claim.

### Accepted leaf `0x00194533..0x00194573`

The original `RET 4` occupies `0x00194570..0x00194572`, so the reviewed exclusive
manifest end is `0x00194573`. Tests cover argument groups 0 and 2, every byte of
all three 256-byte ramps in both cases, exact boundary sentinels, preservation of
the prior group, the final EAX value, stack cleanup and nonvolatile registers.
Release CTest passes 7/7 and the guarded production build passes identity checks.

Two Terra review attempts failed before inspection because the selected model was
at capacity. Local review compared the generated body to the original 22
instructions and found the address math, loop bound, three stores and ABI exact.

**Decision: accept this small mechanical leaf on exhaustive objective checks
after two reviewer-capacity failures, rather than retrying the same unavailable
reviewer again.** The next packet is the independent two-entry register table at
`0x00196B34..0x00196B6A`.

**Decision: record today's reusable recovery findings in the toolkit's
`docs/technical/lessons-learned.md`.** It now states that x86 wraparound must be
expressed with defined C arithmetic and that focused dependency seams must never
replace production fatal routing.

### Accepted leaf `0x00196B34..0x00196B6A`

The recovered two-iteration loop writes the exact interleaved descriptor map at
device offsets 0x8910 through 0x8944. The focused test asserts all twelve final
writes, the four-byte iteration spacing, both surrounding sentinels and the ABI.
The combined focused executable passes 1,675 checks; Release CTest passes 7/7
and the guarded production build passes.

Terra again failed before inspection because the model was at capacity. Local
review matched the generated 17-instruction loop, two iterations and every
resulting address to the original disassembly.

**Decision: accept this mechanical table leaf on exhaustive address/value checks
despite reviewer capacity, then proceed to `0x00196B6A..0x00196B9A`.** The next
leaf consumes context offset 0x13C, so it is the closest remaining direct leaf to
the real instance-memory boundary.

### Accepted leaf `0x00196B6A..0x00196B9A`

The recovered helper writes the low 16 bits of context offset 0x13C to the
original device register and clears exactly two dwords at the wrapped
instance-derived address. Normal and high-tag cases prove the low-half behavior,
32-bit add/shift wrap, both clear writes, adjacent sentinels and ABI. The focused
executable passes 1,694 checks; Release CTest passes 7/7 and the guarded build
passes identity checks.

Terra was again unavailable at capacity. Local review confirmed that the body
uses unsigned 32-bit architectural registers and matches all 15 original
instructions.

**Decision: accept the instance-derived clear on its normal and forced-wrap
behavior tests, then recover the final direct default-register leaf
`0x00196B9A..0x00196C0B`.** Parent integration remains blocked on the real
`0x00194520` helper rather than these straight-line leaves.

### Accepted leaf `0x00196B9A..0x00196C0B`

The recovered helper installs the three exact context defaults, copies context
offset 0x100 to device offset 0x2504, and clears the eleven original device
registers. Unique sentinels prove every target, source preservation and adjacent
state. Local review caught that the first fixture revision claimed but omitted
the observable EAX result; Luna's second pass added the assertion that EAX remains
the device base. The focused executable now passes 1,718 checks, Release CTest
passes 7/7, and the guarded build passes.

**Decision: require the omitted EAX assertion before accepting the final direct
leaf, then recover `0x00194520..0x00194533`.** That dependency increments context
offset 0x19C but returns the prior value, so replacing the temporary fixture seam
is necessary before the allocation helper's caller-visible result is trustworthy.

### Accepted dependency `0x00194520..0x00194533`

The real five-instruction helper now replaces the focused fake and the production
fatal trap. It returns the prior context+0x19C value in EAX, stores the argument-
incremented value with 32-bit wrap, and performs `RET 4`. Direct normal and
overflow tests plus the allocation-helper integration pass 1,724 focused checks;
Release CTest passes 7/7 and production links with one recovered definition.
Terra independently confirmed routing, generated-body identity and removal of the
unresolved stub.

The existing generated C4700 warnings for local EBP in frame-based `0x00196800`
and `0x00196967` remain the documented architectural-EBP limitation; they were
not introduced by this helper.

**Decision: accept the real dependency and integrate `0x00194676..0x00194780`
next.** All of that helper's direct callees are now recovered, so its combined
clock reduction, allocation, identity ramps and device defaults can be tested as
one original-XBE path without success stubs.

### Accepted integrated helper `0x00194676..0x00194780`

The recovered helper now runs its original leaf sequence end to end: PLL decode,
PTIMER fraction reduction, instance allocation, port/register setup, identity
ramps, instance-derived clear and defaults. With decoded context+C8 equal to 218,
the original reduction reaches numerator 0 and denominator 61,035. The focused
fixture uses a distinct pre-call sentinel so it proves the decoder ran, and also
checks context+B0 and device+0x9420 parent-owned writes. It passes 1,752 checks;
Release CTest passes 7/7 and the guarded build passes.

Terra rejected the first wrapper because it restored EBP/SEH metadata before
checking it, making preservation tautological. Luna removed that special case and
the unsupported EBP claim. Terra then found the three coverage gaps; after two
Luna attempts, Sol Medium added only the missing sentinels/assertions. The helper
now uses the standard ESP/EBX/ESI/EDI routine contract and retains the documented
architectural-EBP limitation.

**Decision: accept `0x00194676` only after removing the artificial frame-metadata
restoration and closing all integrated-call coverage gaps.** Next decompose
`0x00194780..0x001948B9`; it has time/date kernel imports and three unrecovered
direct callees, so it is not yet suitable for a single blind recovery packet.

### Decomposition of `0x00194780..0x001948B9`

The helper queries system time, converts it to time fields, performs signed
calendar arithmetic, and writes a derived timer value before calling three
unrecovered helpers. The imports are confirmed as `KeQuerySystemTime` and
`RtlTimeToTimeFields`. Exact epoch meaning remains a hypothesis; signed division,
32-bit multiplication wrap and the byte month table are part of the original
contract.

`0x00196E80..0x00196E92` is an independent two-register leaf. `0x00197BCF`
depends on channel snapshot helper `0x00196C83`. `0x00197C50` reaches polling,
interrupt and channel helpers `0x00196C0B`, `0x00194210`, `0x00193D90`,
`0x00196C4A` and `0x00197AAC`; it is not safe to recover as a success path yet.

**Decision: recover `0x00196E80` first and keep the larger PGRAPH/ISR paths
fatal.** Then build a deterministic time-import fixture, recover
`0x00196C83`/`0x00197BCF`, and address the service chain before integrating
`0x00194780`.

### Accepted leaf `0x00196E80..0x00196E92`

The recovered leaf loads the device pointer, writes 1 to device offsets 0x600100
and 0x600140, returns the device pointer in EAX, and preserves the leaf ABI.
Independent sentinels bracket both targets. The focused fixture passes 1,764
checks; Release CTest passes 7/7, the guarded build passes, and Terra's review is
clean. The parent and all larger dependencies remain fatal.

**Decision: accept the clock-control leaf and next decompose `0x00196C83`, the
state helper required by `0x00197BCF`.** Its channel-indexed copies need an exact
address contract before Luna implementation.

### Accepted channel helper `0x00196C83..0x00196E80`

The recovered helper snapshots thirteen live device fields into the masked old
channel slot and restores thirteen fields from the raw requested-channel slot.
Tests cover channels 0, 1, 2, 31 and raw 32; the latter proves x86 shift-count
masking for bit tests while slot addressing remains unmasked at table+0x800.
They also cover both old-active equal/unequal paths, requested promotion, saved
state restoration, sentinels, EAX and `RET 4`.

Terra found that the translated prologue published EBP/SEH metadata but `leave`
restored only the local C EBP. A manifest opt-in now saves/restores caller
`g_ebp` and `g_seh_ebp` for this balanced, no-call helper only; it adds no
tautological checker. Distinct sentinels prove restoration for arguments 1 and
32. Terra's re-review is clean. The focused fixture passes 1,889 checks and all
seven Release tests pass.

**Decision: accept `0x00196C83` only with its evidence-backed frame-metadata
opt-in, then recover its direct caller `0x00197BCF`.** Intermediate MMIO ordering
is preserved by generated instruction order but still lacks a write-trace fixture.

### Accepted PGRAPH reset helper `0x00197BCF..0x00197C50`

The recovered caller performs the original register initialization and ordered
toggles, calls the real channel-state helper with argument 1, and preserves its
ABI/frame metadata. The fixture proves the masked context value, representative
saved/restored slot state, final registers and dependency effects.

Terra found that the first acceptance run used an executable fourteen seconds
older than the regenerated focused source. Rebuilding the target from current
source yields 1,935 passing checks; all seven Release tests and `git diff --check`
pass. Terra's implementation, routing and fatal-boundary review is otherwise
clean.

**Decision: reject stale focused evidence even when production is current, then
accept `0x00197BCF` only after an explicit focused-target rebuild.** Next
decompose the polling/ISR/channel dependencies under `0x00197C50` before adding
any success path.

### `0x00197C50` service-chain boundary

The dependency graph contains a real cycle: `0x00196C0B` polls hardware status
and dispatches `0x00194210`/`0x00193D90`; `0x00194210` and `0x00197AAC` can call
back into the same service path. `0x00193D90` performs a timestamped callback,
waits on a device bit and has reentrancy/missed-signal implications.
`0x00196C4A` is only a trigger wrapper and cannot be accepted as isolated success.

The only independent leaf is `0x00193C40..0x00193C43` (`RDTSC; RET`). The next
queue helper `0x00193D10` still depends on scheduler/event contracts. Acceptance
of the larger chain needs bounded zero/nonzero polling cases, both service bits,
callback and missed-signal cases, timeout behavior and proof that a status bit
which never clears does not produce false completion.

**Decision: recover the pure timestamp leaf with Luna, then escalate the polling
and callback cycle to Sol Medium.** Do not recover `0x00197C50` or its trigger
wrappers as success paths until those concurrency contracts exist.

### Accepted timestamp leaf and publication contract

`0x00193C40..0x00193C43` now uses the established QPC-scaled Xbox timestamp
adapter and returns its exact low/high halves in EAX/EDX. Terra found that the
adapter published frequency before origin without synchronization, allowing a
concurrent first call to observe a huge value and later move backward. It also
found that the first focused test could not detect swapped or zero halves.

Sol Medium moved frequency+origin publication behind one Windows `INIT_ONCE`,
preserving the 733,333,333 Hz scaling. A test-only build runs 64 forced native-
thread first-call races, proves no caller escapes before origin publication, and
proves one shared origin with exact samples; a live loop checks 10,000
nondecreasing samples. The recovery fixture injects
`0x89ABCDEF01234567` and asserts EAX=`0x01234567`, EDX=`0x89ABCDEF`.
Test seams are absent from production outputs.

Current evidence: focused recovery 1,942 checks, Release CTest 8/8, toolkit
Python suite 27/27, and bounded run
`logs/runs/20260920-014707-773-timestamp-publication-final/`. The run has seven
native threads, 58 named frames, successful dump/checkpoints/GPU analysis, and
the unchanged expected fatal `0x00194ADD`; startup does not yet reach the new
timestamp leaf. Terra's cross-repository review is clean.

**Decision: do not accept a sequential timestamp test as evidence for a shared
multithreaded clock; require coherent one-time publication and a forced first-call
race.** With that contract accepted, move the polling/callback cycle to Sol Medium.

### Accepted bounded `0x00196C0B` service contract

A test-only harness now mirrors the original status loop and uses counted seams
for `0x00194210` and `0x00193D90`. Across 64 repetitions it covers already-idle,
native-worker clear, never-cleared 25 ms stall, each service bit independently
and together, captured-snapshot callback order, no false completion and bounded
same-context reentry classification. Production routing is unchanged.

Terra found that a generic Win32 event case was overclaimed as original missed-
signal evidence and that partial creation/join failures could leak a worker or
handle. Sol narrowed the event case to a future generic seam and made cleanup
cooperative on every path. Terra then found an infinite fallback join for direct
runs; integration now exits the test process after the one-second cooperative
deadline instead of returning past stack state or hanging forever. Final review
is clean. Three direct runs pass 1,984 checks each and Release CTest passes 9/9.

**Decision: accept this as a bounded polling-loop contract, not proof of the real
`0x00193D90` event/callback chain.** Luna may recover only `0x00196C0B` with
focused counted seams; production dependencies remain fatal until Sol establishes
the real signal source and same-context reentry ownership.

### Accepted polling helper `0x00196C0B..0x00196C4A`

The recovered production body preserves the original unbounded loop: it checks
device+0x400700, snapshots device+0x100 once per iteration, dispatches the two
service bits in original order, then rereads pending state. Focused counted seams
prove independent and combined callbacks, including the case where the first
callback clears pending but the captured second bit still dispatches. The focused
fixture passes 1,956 checks and Release CTest passes 9/9.

Production contains no seam or deadline code and still calls fatal unresolved
`0x00194210`/`0x00193D90`; the separate bounded harness owns stall diagnosis.
Terra's review is clean.

**Decision: accept only the polling helper, then decompose queue helper
`0x00193D10` with test scheduler seams.** The real callback, signal source and
same-context reentry cycle remain blocked for Sol Medium.

### Accepted queue helper `0x00193D10..0x00193D85`

The recovered helper calls the queued-payload seam, selects the two-slot ring by
index parity, optionally invokes the buffer seam before clearing active status,
sets device status bit 1, increments the index with 32-bit wrap, copies
context+0x1F4 to +0x1F8 and stores the second argument at +0x210. Production
`0x0018E500` and `0x0018E584` both remain fatal.

Terra found that the first generator revision injected a false device-status
reload into EAX; the original returns context+0x1F4. It also found the legacy
`0x0018E584` stub returned instead of aborting. Luna's second pass corrected both
and added callback-order, active-during-callback, clear-after-return, EBP and SEH
checks. Terra's re-review is clean. Focused recovery passes 1,988 checks and
Release CTest passes 9/9.

**Decision: reject the queue helper until its original EAX value and both fatal
dependency boundaries are proved.** The next implementation boundary is the real
`0x00193D90` callback/signal and `0x00194210`/`0x00197AAC` reentry cycle, owned by
Sol Medium; no downstream success path should be added before that contract.

### Local checkpoint commits

The accepted GPU harness/submission checkpoint is game `1e62b6f` and toolkit
`3db44d7`. Today's 11c1 recovery, timestamp leaf, bounded service harness and
queue-helper work is committed in game `9f0e4e0`; the coherent timestamp
publication runtime and regression are committed in toolkit `3359892`.
`xboxrecomp/recomp_run.log` remains the unrelated untracked file.

**Decision: checkpoint both repositories locally before beginning the real
callback/reentry packet.** This gives the next Sol worker an exact rollback and
comparison point while leaving original assets, builds, logs and saves excluded.

### Accepted `0x00193D90` / `0x00194210` / `0x00197AAC` contract

Original disassembly identifies the real signal as stdcall `KeSetEvent`
(IAT `0x001C4014`, ordinal 145) with Event=`context+0x1C8`, Increment=`1`,
Wait=`FALSE`, after a spin that writes `1` to `NV_PCRTC_INTR_0` and waits
until `NV_PMC_INTR_0_PCRTC` (`device+0x100 & 0x01000000`) is clear. The
object at `+0x1C8` is an in-place Type-0 NotificationEvent (Size 4, initial
SignalState 1) from `0x00194AFA`. The guest callback at `context+0x1C4` is
cdecl with one pointer to `{1F4, 1F0, reason}` and `add esp, 4`. Reason is
1 when `0x00193D10` ran, 2 when the threshold fired without a slot, else 0.
EAX returns 0; RET pops only the return address.

`0x00194210` is the `NV_PGRAPH_INTR_CONTEXT_SWITCH` (`0x1000`) path: FIFO
off, W1C that bit, spin on `device+0x400700`, then `0x00197AAC` with
channel `(trapped_addr & 0x1FFC) >> 20` which is 0. Nested `0x00194210`
from `0x00197AAC` does not take the context-switch path again after a
successful W1C of only `0x1000`.
`0x00196C0B` inside `0x00197AAC` is idle when STATUS is already 0.
`0x00196C4A` is the recovered channel-change wrapper.

The bounded fixture `tests/test_callback_reentry.c` covers already-clear and
never-clear PMC spins, native PMC clear, Type-0 pre-signal / reset-before-wait
/ missed-reset, cdecl callback ABI, queue reason codes, PCRTC byte restore,
same-context reentry without recursive false completion, STATUS stall with
FIFO left off, and per-thread EBP/SEH/IRQL isolation. 64 repetitions pass
5,440 checks. Release CTest is 10/10. The generic Win32 event in
`tests/test_service_chain.c` is still not this function's evidence.

**Decision: recover production `0x00193D90`/`0x00194210`/`0x00197AAC`/`0x00196C4A`
and finish the in-place event bridge before any further helper expansion.**
Independent review of the first event pass found missing `NtClearEvent`
routing, racy SignalState stores, silent 128-slot exhaustion, and Type/Size
heuristics that allowed `va+2` to cross canonical RAM. The second pass routes
`KeSetEvent`/`KeResetEvent`/`KeWaitForSingleObject`/`NtSetEvent`/`NtClearEvent`
through one critical-section registry, recreates the host event on Type reuse,
aborts diagnostically on exhaustion, and requires a 16-byte in-range header
with a self-linked or in-range wait list. `jsrf_inplace_event_bridge` drives
those bridges with forced set/wait/clear interleavings. HANDLE-typed
`NtCreateEvent` clear still works. PGRAPH_INTR W1C and the Python runner stay.

A follow-up pass resynchronizes the host event to guest SignalState on
acquire when no waiters are present, and routes `NtWaitForSingleObject` and
`NtPulseEvent` through the in-place model.

Independent review then left Type-1 consume-after-later-set **OPEN**, and
after that timeout/reset/pulse handoff accounting. The current pass uses
per-waiter credits (`active`, `in_host_wait`, `paid`). Set pays only a
waiter already inside `WaitForSingleObjectEx`. Timeout refunds a paid
credit into `SignalState`. Reset does not clear `paid`. Pulse pays only
`in_host_wait` waiters. Test gates freeze registration, host-wait entry,
host-wait return, and lock reacquire.

`jsrf_inplace_event_bridge` PASS 1,207 including those three gated races.
A further pass gives each waiter a private auto-reset host event so payment
matches the waiter awakened. Type-1 pulse pays exactly one `in_host_wait`
waiter. Two gated waiters plus two sets produce two completions and a
zero-time third wait times out; Type-1 pulse produces one completion, no
sticky leftover, then a cleanup set. Fixture PASS 1,280. Release CTest
11/11. Solo `logs/runs/20260921-012502-906-event-private-waiters/` stops at
`0x00194ADD` (`0xE0424943`). Keep `0x00193F70`, `0x00197C50`,
`0x00194780`, and `0x00194ADD` fatal. The four recovered helpers stay
unaccepted.

This packet is not independently accepted yet. Keep `0x00193F70`,
`0x00197C50`, `0x00194780`, and `0x00194ADD` fatal. Do not start
`0x00194780` or `0x00197C50` until that review is clean.

### Fresh-context continuation packet

A new agent should read `AGENTS.md`, `grok-role-map.md`,
`docs/jsrf-callback-reentry-contract.md`, the 11c1 plan row, and this report
tail. The four service helpers are recovered; the next work is independent
review of the event bridge, then a bounded game capture remains at
`0x00194ADD`.

**Decision: recover `0x00194ADD` and `0x00194780` from original XBE and
leave `0x00197C50` fatal.** Ordinary startup
`logs/runs/20260921-013349-836-gpu-setup-94add/` now dies at `0x00197C50`.
`OUT 0x80C0` is `xbox_outb` (logged). Session notes: `report-grok.md`.

**Decision: recover the remaining original-XBE setup leaves through
`0x001912A0` and stop at the first pushbuffer kick.** `0x00197C50`,
RAMIN allocators `0x001948B9`/`0x00194913`/`0x001949E2`, surface helpers
`0x00196E92`/`0x00194573`/`0x001945D6`, and framebuffer publish
`0x001912A0` are recovered. `0x00194ADD` returned. WBINVD ran. Ordinary
startup `logs/runs/20260921-020149-996-gpu-setup-912a0/` dies at
`0x001918E0` (`0xE0424943`), dump_ok. Focused 11c1 PASS 2197. Release
CTest 11/11. NV2A contracts PASS 228 including PFB WBC idle-after-flush.
Keep `0x00193F70`, `0x0018E120`, and `0x001918E0` fatal. `0x001918E0`
tail-jumps to `0x001917F0` and needs wrap/kick `0x001916B0`/`0x00191530`
plus `0x00190240` before recovery.

### PRAMIN instance backing — 2026-09-21

**Decision: complete the bounded PRAMIN-backing prerequisite before writing the
kick/GET contract.** `NV_PRAMIN` now covers the full 1 MiB aperture but remains
unbound until `MmClaimGpuInstanceMemory` validates and publishes the exact
top-of-contiguous range. The claim and NV2A initialization share the MMIO owner
lock; zero, oversized and contradictory claims fail, while the same claim is
idempotent. PRAMIN MMIO and `nv_dma_load` use one backing with exact bounds.

Two Luna passes established the mapping and tests. Independent review found a
64 KiB aperture truncation, unsafe claim arithmetic, unsynchronized binding,
failed-bind return, detached pre-claim RAMIN and null-backing cases. Sol fixed
those issues, and a fresh Terra re-review is clean. The focused NV2A fixture
passes 269 contracts, including both initialization orders, unbound behavior,
8/16/32-bit MMIO, descriptor visibility and boundary rejection. Release CTest
passes 11/11.

Bounded run `logs/runs/20260921-100901-149-pramin-backing/` has a successful
dump, seven native threads, 58 named frames and GPU analysis. `0x00194ADD` and
`0x001912A0` return; the first unresolved target remains `0x001918E0`. Dumped
memory at `0x83FFB000` contains the game-created instance table, including the
object pairs beginning at `+0x10`. This proves backing and visibility, not
RAMHT/class lookup, command execution or a successful kick.

### RAMHT handle-to-class lookup — 2026-09-21

**Decision: production SET_OBJECT reads the claimed PRAMIN RAMHT; the fixture
binding stays opt-in for 11b4b2 tests.** The captured table matches original
`0x001945D6` / `0x0019701F`: an 11-bit handle hash, an 8-byte pair at
PRAMIN+hash*8, and a 16-byte object at `tag<<4` whose word0 low byte is the
class. Handle `0xD` is context `0x8001049C` and object class `0x97` at
`0x83FFF9C0`.

Lookup rejects a missing, zero, invalid-bit, mismatched, out-of-range, or
class-zero handle with diagnostic `invalid_handle` and rolls the stream back.
NV097 clip methods still commit only after a successful bind. Focused NV2A
contracts PASS 304. Release CTest passes 11/11. Solo
`logs/runs/20260921-103827-656-ramht-lookup/` still stops at fatal
`0x001918E0` with dump and GPU analysis successful and GET=PUT=`0x1000`.
`0x001918E0` and its kick chain remain fatal. This is not a successful kick.

Independent review accepted the lookup: no blocking defects. SEARCH_128 stays
a single slot because `0x001945D6` inserts one entry, and the 11-bit fold
matches the game's 4K table. The next packet is the kick/GET contract.

## Kick/GET contract and the kick latch (2026-09-21)

Grok's session ended mid-packet with a large uncommitted GPU checkpoint in both
repositories. That work is now committed: game `8641320` (11c1 recovery through
the first pushbuffer kick) with toolkit `c98dbdc` (PRAMIN instance backing and
RAMHT lookup), plus `f8b0d0c` recording the toolkit revision in AGENTS.md.
Toolkit unit suite 27/27 and Release CTest 11/11 were re-verified at that
revision before committing.

**Decision: write the kick/GET contract before recovering the chain, not after.**
The plan already said not to recover `0x001918E0` without a kick contract, and
this session showed why. `docs/jsrf-kick-get-contract.md` now maps the ring
control block at `MEM32[0x0019DCE0]`, `0x00191530` (`RET 8`, fast exit when
`flags & 4`, otherwise a wrap-adjusting slow path), `0x001916B0`
(`push size, size/2`), `0x001916C0` (rewrites its own stack arguments before
tail-jumping into `0x00191530`), `0x00191390`, `0x00191440` (the back-pressure
wait that spins hardest if GET never advances), `0x001918E0`, `0x001917F0` and
`0x00190240`, including its two real callers at `0x0014D9D0`.

The load-bearing finding is that the kick is not a plain put-pointer write.
`0x00191270` sets **bit 16** of `NV_PFIFO_CACHE1_DMA_PUT` and then polls at
`0x00191290` until the engine clears it; `0x001912A0` inlines the same
sequence at `0x001912C6`. The previous model stored the raw value and never
cleared bit 16, so that poll could not terminate. That is why the two kick
sites were left fatal.

**Decision: the kick bit is a model-owned latch.** Store the offset, run the
pending submission, then clear bit 16 to acknowledge. The stored value is
masked rather than restored so an acknowledgement cannot undo a legitimate
GET/PUT advance. `kick_requests`, `kick_acks` and `kick_last_put` make an
unacknowledged kick distinguishable from one that ran and blocked, and a
blocked stream still acknowledges — otherwise a rejected method would present
as a hang instead of the `unsupported_method` diagnostic it actually is.
Implemented in toolkit `488286f`.

Two defects surfaced while testing this, both worth recording:

- The offset mask must be `0x1FFEFFFF`, not the `0x1FFFFFFF` already used for
  this register elsewhere. `0x1FFFFFFF` *contains* bit 16, so the generic mask
  silently preserved the kick bit and the poll stayed live. The first three
  test cases failed exactly here and the arithmetic looked impossible until
  the mask constant itself was printed.
- `NV_USER_DMA_PUT` is an alias of the PFIFO pointer, but `user_write` stored
  the USER-local `regs[]` slot while `user_read` and `nv2a_submit_pending` both
  read the PFIFO slots. A kick written through the USER aperture was therefore
  invisible to the submission engine. Both paths now use the canonical slot.

Add 13 kick-latch contract cases; the NV2A suite is 319, Release CTest 11/11.
Ordinary boot `logs/runs/20260921-111607-936-kick-ack/` is unchanged from the
committed baseline — still fatal at `0x001918E0`, `dump_ok`, `checkpoints_passed`,
GET=PUT=`0x1000`. The checkerboard is a repaired mechanism, not a booted menu.

**Decision: `0x00191270` stays out of the function database.** It is currently
uncalled; its bodies are inlined at `0x001912A0`. Seeding it on speculation
would create a function the game never enters.

The next packet may recover the kick chain `0x001918E0` / `0x001917F0` /
`0x001916B0` / `0x00191530`. Expect it to stop on `unsupported_method`: the
first stream `0x001918E0` emits contains roughly sixteen distinct method
numbers against the two currently accepted. That stop is the packet's success
condition and must be recorded as evidence, not worked around. `0x00193F70`
and `0x0018E120` stay fatal.

## The kick chain is recovered (2026-09-21)

Recovered the whole chain the previous checkpoint handed off, in dependency
order, with every boundary re-derived from `nop` padding rather than taken from
the database. Sixteen new entries in `config/recovered-functions.json` (70
total), covering `0x001918E0`, `0x001917F0`, `0x00191530`, `0x00191440`,
`0x00191390`, `0x001912A0`, `0x00191270`, `0x00191150`, `0x001910E0`,
`0x001910C0`, `0x00190FB0`, `0x001916B0`, `0x001916C0`, `0x00191710`,
`0x00191730`, `0x001917B0` and `0x00190240`.

`stack_args` was read off each real epilogue, not guessed. The chain is mostly
plain-`ret` thiscall, with `RET 8` on the three reservation/wait functions
(`0x00191530`, `0x00191440`, `0x001916C0`), `RET 4` on the single-argument
record and submit functions (`0x00191390`, `0x00191730`, `0x001917B0`,
`0x00190240`), and `RET 0xC` on the record copier `0x00190FB0` where three cdecl
arguments are popped by the callee.

Two structural decisions mattered:

- **`0x001910C0` and `0x001910E0` are separate functions.** `0x001910DF` is a
  `ret` and `0x001910E0` is entered only by fall-through. Registering a single
  span would have silently merged two functions, which is the exact failure the
  manifest exists to prevent.
- **`0x001918E0` and `0x001917F0` share one frame.** `0x001918E0` pushes
  EBX/ESI/EDI and ends in `jmp 0x001917F0`; only the target's epilogue at
  `0x001918D0` pops them. Both entries carry `stack_args: 0` and neither takes a
  frame check, because a tail-jump legitimately carries the pusher's frame
  across the boundary.

The single most useful fact the chain confirmed is *where the kick is actually
reached from*. `0x001918E0` and `0x001917F0` do **not** call the kick primitive
`0x00191270`; they write method records at the ring cursor and then call the
submit path `0x00190240`. `0x00191270` is reached from the back-pressure wait
`0x00191440`, which kicks after programming a `0x40110`/`0x40100` register pair
into the record. That matches `docs/jsrf-kick-get-contract.md` and is why that
contract had to exist before this packet could be verified.

### Three generator gaps this packet exposed

All three were latent; none was caused by the new entries, and each one
weakens the verify-before-recover rule if left alone.

1. **`config/manual-functions.json` accumulates the recovered set.** The
   generator appends every recovered address to it, and `relift-selected.py`
   feeds it back in as pre-existing symbol names. Recovering `0x00190240`
   therefore both protected it *and* left its truncated original definition in
   `recomp_0008.c`, producing `LNK2005: sub_00190240 already defined`.
   Resolved by removing the address from the file.
2. **A reviewed boundary fix does not widen the generated chunk.**
   `boundary-fixes.json` is consumed by `recover-functions.py`, but only
   `relift-selected.py boundaries` rewrites the body already in
   `src/recomp/gen/recomp_*.c`. The raw database had split the submit path at
   `0x0019025A`, where `je 0x00190250` and `jne 0x0019026C` made an internal
   branch target look like a new entry. Adding the `0x00190240`-`0x00190335`
   fix and running the boundary relift widened the chunk from 26 to 245 bytes.
   That in turn made four previously unreachable `call 0x00190FB0` sites live,
   so `recomp_0008.c` needed `extern void sub_00190FB0(void);`.
3. **The unresolved-trap unit cannot link into a focused fixture.** The
   generator decides which exposed calls need a trap by scanning `src/**/*.c`,
   so seams defined in `tests/*.c` are invisible to it and the full
   `recomp_stubs_unresolved.c` collided with the three counted seams
   `tests/test_recovery_11c1.c` already provides. The generator now also emits
   `src/recomp/gen/recomp_stubs_recovery_test.c` — the same traps minus any
   symbol a `tests/test_recovery_*.c` defines — and the focused target links
   that. Production keeps the full trap set.

### Two blockers found, not worked around

**The game repository's object store is missing.** `\\.git` contains `HEAD`,
`config`, `index` and `logs/` but no `objects/`, no loose refs and no
`packed-refs`, and no remote is configured, so every git command fails. The
working tree and all session work are intact. `.git/logs/HEAD` survives and
records ten commits up to tip `7e2c4f43`. Repair needs a backup or snapshot
restore of `objects/`, or a deliberate re-init and single re-commit; neither was
attempted without the user. The toolkit repository is healthy.

**The committed chunk tree cannot link: 524 undefined references.** The
generator emits two reference forms — `RECOMP_ABI_CALL(0xADDR, sub_ADDR)` and a
bare tail-jump `g_seh_ebp = ebp; sub_ADDR(); return;`, which is what a *jump*
whose target the database mistook for an entry becomes. Counting both against
the definitions present in `src/recomp/gen/` and `src/recomp/recovered/`: 3839
call targets, 1466 tail-jump targets, 4589 in union, of which **524 have no
definition and none carries a diagnostic trap**. The linker reports
`LNK1120: 473 unresolved externals`. The dominant cause is not missing
functions but misclassified jump targets: **442 of the 524 appear only as
tail-jumps**. `0x000110D0` and `0x00011162` are jump targets *inside* their
parents — `0x00011138` is `jne 0x000110D0`, a backward loop — and the
generator emitted them as `sub_000110D0(); return;`, which also skips the
parent epilogue. Traps were deliberately not added, because trapping a
non-entry turns a wrong tail-call into a wrong abort. This needs a
detection/boundary pass over the affected `0x0001xxxx` chunks first, then traps
only for genuine entries, and is the recommended next packet.
