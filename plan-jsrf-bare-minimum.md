# Jet Set Radio Future: Windows port milestone plan

Agent onboarding, operating rules and handoff guidance live in `AGENTS.md`.
Maintain that guide as capabilities and commands change; this plan remains the
source of truth for milestone status and acceptance evidence.

Current checkpoint: `logs/runs/20260921-111607-936-kick-ack/`.
The kick chain is recovered (`0x001918E0` through the submit path
`0x00190240`), so the first missing call has moved past GPU setup.
**The link blocker is cleared as of 2026-09-21.** `build/Release/jsrf_recomp.exe`
now builds — 19,245,568 bytes, sha256 `bfbcf556e67519239ba17d95a4a2b2df...`,
with a verified source/executable identity stamp. It has never been executed, so
no checkpoint follows from it yet.

What actually blocked the link is worth keeping in the record, because none of
the three causes was the one previously recorded here. The dominant cause was
not a detection gap at all: `scripts/recover-functions.py` was overwriting
`src/recomp/gen/recomp_stubs_unresolved.c`, a file it does not own, deleting the
541 stub bodies the full translation pass had written and refilling it from its
own 70-entry view. The header declared 541 unresolved stubs while the file
defined 3. Two smaller causes sat underneath: mid-body `tail_jump_alias` targets
were emitted as tail calls (`sub_X(); return;`) instead of `goto loc_X`, and the
full pass was being run without the project's manual-function list. See
`report-deepseek.md` for the entries and their evidence.

Two defects are known and are **not** link blockers. Both are correctness
defects that a successful boot would not reveal:

1. **1032 live jumps are silently rewritten to `(void)0`.** The label guard in
   `tools/recomp/translator.py` (the block that rewrites a `goto` whose target
   is not a label in the same function) fires 2623 times across the chunks, and
   1032 of those sit inside a live `if`. The proof case is `0x00011133`
   `mov esi,[esi+0x34]` / `test esi,esi` / `jne 0x000110D0`, a linked-list
   traversal loop whose backward branch became a no-op, so the loop runs once.
   Nothing about this fails loudly.
2. **`tail_jump_alias` entries are still emitted as standalone bodies.** They
   are mid-body fragments whose control flow exits into their parent, so they
   cannot be lifted independently. Fixing 1 and 2 together means not emitting
   them as entries at all and giving each alias a symbol that jumps to the
   parent's block.

Two further items remain:

3. **The game repository's `.git` has no object store**, no loose refs, no
   `packed-refs` and no remote, so no commit can be made. The working tree is
   intact and `.git/logs/HEAD` lists ten commits up to `7e2c4f43`. Needs a
   backup/snapshot restore or a deliberate re-init and re-commit. Only the user
   can choose, and `git init` must not be run as a "fix".
4. **The toolkit fix is committed.** `tools/recomp/lifter.py`,
   `tools/recomp/translator.py` and the new
   `tools/recomp/test_tail_jump_into_batch.py` +
   `tools/recomp/test_alias_body_suppression.py` are toolkit revision
   `58a9cf9`, on top of `488286f`. The toolkit repo is healthy and the game
   repo's `AGENTS.md` records the revision.

## Goal and working rules

First deliver a small playable slice: boot from the extracted retail files,
reach the title/menu, start a new game, load the opening playable area, skate,
jump and spray graffiti, then save and resume. Expand that slice until the
whole campaign and unlockable content work. Audio, progression, transitions
and saves are part of a complete port. Resolution upgrades and mods come later.

This is an evidence-driven backlog, not a promise that the existing toolkit
already supports every required subsystem. Split each milestone further when
it reveals more than one independent defect. Finish and verify one fix before
moving to the next; update this document after each session.

- Keep original XBE/data unchanged. Store host saves separately and back them up
  before testing writes or migrations.
- Record the first failing guest address, caller, arguments, stack convention,
  expected behavior, change, and before/after evidence.
- Recover real code for missed game functions. Replace identified library or
  hardware boundaries with implementations that satisfy the caller's contract.
  Record every temporary stub and its removal criterion; returning success
  without producing required state is not completion.
- A disappearing error is insufficient: verify that the operation happened
  and later execution still reaches the previous checkpoint.
- Use the toolkit docs as examples, not proof that JSRF has Burnout's engine,
  addresses, memory needs, or behavior. Confirm against this XBE and, when
  available, the same scene on original hardware or xemu.
- Version the project and toolkit changes separately. The user requested a
  local committed checkpoint; the game source and companion toolkit changes
  are committed independently, with the toolkit revision recorded in AGENTS.md.
  Preserve later uncommitted work while reviewing changes. Do not commit game
  assets, generated build products, or saves.

## Baseline (2026-09-12)

Project: `C:\Users\logic\Repos\my_xbox_game`.
Toolkit: `C:\Users\logic\Repos\xboxrecomp`.

- Executable: `build\Release\jsrf_recomp.exe`; launch with the **project root**
  as working directory. `src/main.c` resolves both `game\default.xbe` and
  `jsrf_run.log` relative to the working directory.
- XBE title ID `0x5345000A`, XDK 4134, entry `0x00148023`.
- XBE SHA-256: `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C`.
- Existing analysis: 8,437 functions, 120 kernel imports, 11 loaded sections.
- Two old instances were stopped after preserving `logs/jsrf-before-session.log`.
- A fresh 15-second run reproduced the first missing target `0x0017D15C`.
  Baseline saved as `logs/jsrf-baseline.log`; process required termination.
- Four startup warnings precede it: unbridged ordinals 204, 144, 91 and 8.
  These remain unresolved work; they are not the failed runtime call fixed here.
- Later logged failures include `0x0018AFB0`, `0x0018B1A0`, `0x0018B390`,
  `0x0018B580`, `0x0018B770`, `0x0018B960`, `0x0018BB50`, `0x0018BD40`,
  `0x0018C1D0`, `0x0018C3C0`, `0x001BDAA9`, `0x0018E410`, `0x001A5299`,
  and `0x001A52A4`. This is observed coverage, not a complete missing-function list.

## First fix: CRT memmove

The target `0x0017D15C` is an internal basic block of `memmove` at
`0x0017CEC0`, not a standalone function. The function-identification output
names it memmove, and the original instructions confirm a three-argument copy
with an overlap check and forward/backward paths. The generated body ends at
`0x0017D15A`, before the backward-copy remainder blocks and epilogues.
Its negative-index jump table at `0x0017D150` is emitted as `RECOMP_ITAIL`.
Failure skips real copy/return work and leaves the guest frame incorrectly handled.

Implemented the routine in `src/jsrf_crt.c` using host `memmove` after
translating Xbox addresses. Arguments are `[esp+4]=destination`,
`[esp+8]=source`, `[esp+12]=size`; EAX returns the guest destination and ESP
advances by four bytes for RET. The caller removes the three cdecl arguments.
The implementation leaves the guest nonvolatile registers untouched.

Removed its broken generated definition from `recomp_0007.c`, kept the existing
symbol for direct callers and dispatch, and registered it in manual lookup.
`config/manual-functions.json` preserves this decision during regeneration.
Do not add `0x0017D15C` as a no-op or an independent function seed.

Validation: Release build and the `jsrf_memmove` CTest pass. The test checks
36,900 cases with sizes 0–1024, all four alignments, both overlap directions,
identical pointers, disjoint ranges, full-buffer bounds, return value, and
stack cleanup. The first corrected game run removed this failure and retained
all later missing-target checkpoints, starting at `0x0018AFB0`.

Enabling the toolkit watchdog exposed a separate host diagnostic defect:
`xboxrecomp/src/kernel/xbox_memory_layout.c` used `getenv` without including
`stdlib.h`. Added that include to supply the proper pointer-return declaration
on x64. An intermediate diagnostic run crashed before guest entry; its archived
log is retained and must not be counted as game progress.

Final verification: `logs/jsrf-verified-20260912-210815-058.log` records the
replacement copying 48 bytes on its first invocation, no `0x0017D15C` failure
and no access violation. The next failed target is `0x0018AFB0`. The watchdog
captures the remaining hang at 15 seconds with 388 indirect calls and guest
ESP `0x00F7FC24`, then exits with code 3. No game process was left running.
This establishes a repaired operation, not a booted menu or a resolved hang.
Build warnings remain in generated code and the toolkit; the new CRT source
builds without warnings. `logs/build-first-fix.log` holds the final build output.

## Repeatable workflow

From the project root in PowerShell:

```powershell
.\scripts\build-jsrf.ps1
ctest --test-dir build -C Release --output-on-failure
python -X utf8 scripts/run-jsrf.py --seconds 15 --label checkpoint
Get-Content .\jsrf_run.log -Tail 50
Select-String .\jsrf_run.log -Pattern '\[JSRF\]|Failed to resolve|\[CRASH\]|\[WATCHDOG\]'
```

The build wrapper regenerates reviewed recovery functions and compiled test
fixtures, stops on failure, and verifies source identity across compilation.
The runner refuses stale builds or concurrent game copies. It uses the project
working directory, an external debugger with a deadline, and archived symbols,
source, log, thread report and dump. It restores the environment and stops only
its launched process tree. Inspect `result.json`: a diagnostic stop is not game
completion. Direct interactive launch remains possible from the project root,
but the bounded runner is the acceptance and investigation workflow.

When regeneration becomes necessary, run from the toolkit root and pass this
project's analysis directories explicitly:

```powershell
python -m tools.recomp ..\my_xbox_game\game\default.xbe --all --split 1000 `
  --game-name "Jet Set Radio Future" `
  --disasm-dir ..\my_xbox_game\tools\disasm\output `
  --func-id-dir ..\my_xbox_game\tools\func_id\output `
  --manual-functions ..\my_xbox_game\config\manual-functions.json `
  --gen-dir ..\my_xbox_game\src\recomp\gen
```

This is the original full-regeneration command, not the current incremental
workflow. It has NOT been validated with all subsequent recovery changes.
Before full regeneration, stage an analysis directory whose `functions.json`
is the merged `functions.recovered.json`, and point `--disasm-dir` there.
Do not overwrite the original database. Snapshot generated files first and review
the diff. Re-running disassembly can lose recovered names. Check generated
headers, dispatch and direct callers as well as the changed body. Keep manual
lookup and the manual-function manifest synchronized.

## Harness audit and improvement gate (2026-09-12)

This audit describes the harness BEFORE improvements. Its original output was
sufficient for sequential triage but lacked reliable multithreaded capture.
Stages 01a/01b and core 01c/01d are now implemented and verified below. Broader
thread-bootstrap, wait and scheduling coverage remains explicitly open.

### What the original output captured

- `src/main.c` logs the native fault instruction/address and the faulting
  thread's guest registers for access violations. It initializes DbgHelp but
  does not walk/symbolize native frames or write a crash dump. The vectored
  handler sees first-chance exceptions too: a `[CRASH]` line alone does not
  establish that an exception went unhandled.
- The watchdog's `GS address value` lines are raw guest stack words. Some are
  return addresses; others are arguments, data or stale values. The 16 ICALL
  entries are recent indirect targets, not a call chain, and omit direct calls.
- `xbox_WatchdogStart` stores pointers to only the calling thread's registers.
  Its stack scan uses the main stack's bounds and reads while execution runs;
  it is neither an all-thread dump nor a consistent frozen snapshot.
- The ICALL ring, index and count are shared `volatile` globals updated with
  ordinary writes/increments. Concurrent writers can mix or lose history and
  introduce data races in diagnostics. `volatile` supplies no synchronization.
- Guest workers already receive thread-local registers and separate simulated
  stacks/TIB allocations. That is a useful foundation, not proof of correct
  scheduling or synchronization semantics. The first guest system-thread call
  runs inline by default; subsequent calls can spawn real Windows threads.
- The toolkit already reports some critical-section owners/waiters and provides
  `RECOMP_CS_TRACE_CRT=all` plus `RECOMP_CS_WATCH=<guest VA>` for focused traces
  (enable the former with the latter). Its guest backtraces are heuristic stack
  scans. Reuse these hooks, then validate ordering and concurrent access.
- The current runner archives a log and terminates a timed run. It does not
  preserve matching symbols/build metadata, capture worker contexts, or assert
  milestones. The present Release output has no game PDB or linker map.
- stdout and stderr independently open the same filename with different modes.
  Their file positions can overwrite output; a run needs one coherent logging
  path before concurrent multi-line reports are trustworthy.

### 01a — Reproducible run artifacts and native symbols

Keep optimized code but generate matching PDBs and a linker map. Give each run
an artifact directory containing its log, executable/symbol identity, XBE hash,
build configuration, toolkit revision/local-change identity, diagnostic settings,
duration and outcome (normal exit, unhandled exception, diagnostic deadline,
or collector failure). Add explicit boot checkpoints and expected-checkpoint
checks. Consolidate log output; tag diagnostic events with monotonic time and
thread identity, and keep multi-line records together or use structured events.

Acceptance: two runs remain independently interpretable after a rebuild;
a controlled fault resolves to the correct source/function; timeout is never
reported as game success; concurrent test messages do not overwrite each other.

### 01b — Crash and hang collection across all threads

Use an external collector/debugger for unhandled-exception and timeout dumps,
so collection need not acquire locks in a broken game process. Distinguish
handled first-chance exceptions from fatal ones. Capture all native thread
contexts/stacks with matching symbols and sufficient guest RAM to inspect
guest stacks, globals and objects. Register guest threads with stable guest/host
IDs, start routine, actual stack bounds, TIB/TLS address and lifecycle state;
preserve the association in dumps. Take the dump before watchdog termination.
Retain a bounded fallback if collection itself fails.

Acceptance: an intentional worker-thread fault identifies that worker and
its guest state. A two-thread deadlock yields both native stacks and both guest
stacks, while a pure CPU spin still produces a useful dump. Thread exit during
collection must not leave dangling register pointers or invalid stack reads.

### 01c — Thread-safe recent history and synchronization evidence

Replace shared unsynchronized call history with bounded per-thread buffers and
safe publication/snapshot rules; merely making writers thread-local does not
make live concurrent reads safe. Record guest call site/target and enough
context to distinguish a call from a tail jump; identify any dropped events.
Add selectable function/ABI tracing where needed rather than logging every
instruction. Trace thread create/start/exit and critical-section/event/semaphore
operations, including object identity, owner/waiter, signal/reset, timeout and
return status. Reuse the existing lock diagnostics after auditing their shared
state. Avoid introducing a global diagnostic lock into critical guest paths.

Acceptance: concurrent known call sequences stay attributed to their own
threads without corrupting buffers; a deliberate lock-order deadlock names
both owners/waiters and acquisition sites; a missed signal can be distinguished
from a signaled object whose waiter never resumes.

### 01d — Repeatable concurrency regression probes

Add small fixtures for worker register/TLS/stack isolation, create/exit/join,
event wakeups, critical-section exclusion, worker crash, deadlock and CPU spin.
Run bounded repetitions and archive failure artifacts automatically. On
suspected races, add narrowly targeted scheduling delays/yields with recorded
seeds/settings and rerun with lighter instrumentation to account for timing
changes. Repetition improves coverage; it does not prove absence of races or
give deterministic replay of the Windows scheduler.

The toolkit's `RECOMP_WORKERS=inline` can help narrow a hypothesis, but changes
execution semantics and can itself deadlock when a worker waits for its caller.
A passing inline run is neither a fix nor proof of a race. A static crash dump
also cannot reveal every earlier racing write; use targeted event/memory-write
tracing once the suspect object and code paths are known.

Acceptance: each deliberately broken fixture produces its expected failure
classification and actionable artifacts, and healthy fixtures pass repeated
runs with no state leakage. Run the existing memmove test and game checkpoint
smoke run after harness changes to ensure diagnostics preserve boot behavior.

## Historical harness implementation checkpoint (2026-09-12)

- Added optimized PDB/map builds and an external debugger/collector. Each run
  archives binaries/symbols, metadata/hashes/settings, patches, source snapshots,
  game log, native/guest thread report and a dump when capture is requested.
- Fixed shared stdout/stderr file positioning and null-log checkpoint handling.
  Checkpoint and ICALL diagnostics carry thread/time identity. All-thread event
  histories are bounded per-thread records inspected only at debugger stops.
- Canonical guest RAM, registry and live guest registers are verified present
  in dump memory streams; exited threads retain history without TLS dereferences.
- Legacy ICALL rings and kernel dispatch selection are now thread-local.
  A forced-interleaving regression proved the old shared kernel slot called the
  wrong ordinal; `dispatch-before` failed and `dispatch-after` passed.
- Eight collector probes passed: three healthy concurrency repetitions with
  8,000 protected increments each, event signal/reset/timeout through actual
  imported kernel bridges, forced dispatch interleaving, handled exception,
  worker crash, two-thread deadlock and CPU spin. Summary:
  `logs/harness-test-results.json`. Existing memmove CTest also passes.
- Ordinary game verification still reaches the prior guest-entry/memmove path,
  with first missing target `0x0018AFB0` and a timed dump. Latest:
  `logs/runs/20260912-214337-093-harness-ready/`.
- The named native stack locates the current hang in `sub_001497DC`, reached
  through guest `nh_malloc` and game initialization. Cause is not yet proven.

Run `python scripts/test-harness.py` for the collector suite. The run script now
uses the external collector, disables the in-process watchdog for collected
runs, and emits `result.json`. Script outcomes: 0 normal zero exit with expected
checkpoints; 1 nonzero game exit; 2 collector failure; 3 diagnostic capture or
unhandled exception; 4 missing expected checkpoint. Inspect JSON to distinguish
crash from deadline. Shell wrappers may report nonzero codes differently.

Remaining harness coverage: the fixture workers currently use native thread
creation plus runtime stacks/TIBs, rather than the complete guest
PsCreateSystemThreadEx bootstrap/termination path. Semaphore and multiple-wait
edge-case matrices, scheduled perturbation sweeps, and diagnostics outside the
instrumented synchronization boundaries remain follow-up work. Registry capacity
is 128 thread lifetimes per run; overflow is explicit and never reuses stale TLS.
Old optional tracing/profiling paths have not all been audited for concurrency.
Source ZIPs describe files at launch, not proof of a clean build of every source.

The proven crash/hang/history capabilities are sufficient to resume sequential
startup recovery now. Keep 01c/01d's remaining coverage open and return to it
before declaring guest worker scheduling or general concurrency correct.

## Milestones and acceptance checks

Statuses apply only to observed, verified behavior. Each row should produce a
small reviewable change and a saved log, test result, or scene capture.

Suggested Agent is a routing recommendation, not a record of who performed the
work. Labels follow the escalation gates in `AGENTS.md`: Luna is the default
for bounded implementation with a clear contract, Sol Medium is for concrete
ambiguity or a repeated failed Luna attempt, Astra Medium is for architecture
or task-graph revision, and Terra review is an independent correctness review.
Sol Light orchestrates a parent milestone when several Luna packets must be
sequenced; it does not replace the suggested implementation agent.

| ID | Status | Milestone | Acceptance check | Suggested Agent |
|---|---|---|---|---|
| 00 | Done | Reproduce launch and first failure | Correct working directory, fresh archived baseline, no lingering process. | Luna + Terra review |
| 01 | Done | Repair the first memmove failure | Copy/ABI tests pass, first failure gone, later startup path retained. | Luna + Terra review |
| 01a | Done | Preserve runs and native symbols | Matching artifacts, coherent thread-tagged logs, meaningful outcomes and checkpoint checks. | Luna + Terra review |
| 01b | Done | Capture crashes/hangs across all threads | Controlled worker crash, deadlock and spin produce inspectable dumps before termination. | Luna + Terra review |
| 01c | Core verified; coverage open | Make tracing safe and useful for concurrency | Per-thread call history and verified lock/wait/signal ownership evidence. | Luna + Terra review |
| 01d | Core verified; coverage open | Validate concurrency diagnostics | Positive/negative multithreaded fixtures produce expected outcomes across bounded repetitions. | Luna + Terra review |
| 01e | Done for current backing | Preserve actual GPU memory in captures | Capture separate contiguous/instance memory and readable GPU aperture; verify marker bytes in dumps across nine harness probes. Future trapped model state remains separate work. | Luna + Terra review |
| 01f | Done for current backing and native fixture | GPU snapshots, offline decoder and failure probes | Thirteen harness probes, twenty offline tests, five CTests, NVIDIA/WARP readback and verified RenderDoc capture. Snapshot storage does not prove GPU execution. | Luna + Terra review |
| 01g | Partial; optional tool follow-up | Nsight Systems trace completeness | Report and annotations captured; resolve the DX11 startup diagnostic and verify complete intended events before timing claims. Does not block 11a. | Luna + Terra review |
| 02 | Done | Recover startup target `0x0018AFB0` | Inspect its caller and original boundaries, lift real initialization, verify initialized globals and return stack. | Luna + Terra review |
| 03 | Done | Recover the adjacent startup routines | Investigate the `0x0018B1A0`–`0x0018C3C0` group one function at a time; compare initialized tables with XBE intent. | Luna + Terra review |
| 04 | Done | Recover remaining observed library targets | Classify and handle `0x001BDAA9`, `0x0018E410`, `0x001A5299`, `0x001A52A4`; verify individual contracts. | Luna + Terra review |
| 05 | Done | Locate the boot hang | Capture native call stack plus guest registers/stack; identify exact polling instruction or recursive path. | Luna + Terra review |
| 06 | Pending | Audit imported kernel gaps | Complete 06a and 06b; name ordinals 204/144/91/8, measure actual calls, implement required semantics and stack cleanup. | Luna + Terra review |
| 06a | Pending | Establish imported kernel contracts | Decode the four ordinal call sites, argument cleanup and required observable effects; save a contract table and a bounded probe for each reached import. | Luna |
| 06b | Pending | Implement reached imported kernel semantics | Implement only measured ordinal behavior, verify return/status and stack cleanup in probes, and show the prior startup checkpoint still holds. | Luna + Terra review |
| 07 | In progress | Establish reliable CRT/game initialization | Complete 07a and 07b; trace constructor completion and first game entry, then resolve reachable uninitialized-EBP warnings and ABI mismatches. | Luna + Terra review |
| 07a | Pending | Prove constructor and CRT completion | Capture constructor order, return registers/stack and first game-entry checkpoint across a bounded run. | Luna |
| 07b | Pending | Repair evidenced initialization ABI defects | Fix only reproduced EBP/register/stack contract defects and verify constructor outputs plus the next failure address. | Luna + Terra review |
| 08 | Pending | Validate paths and first real asset read | Complete 08a and 08b; resolve a requested file under `game/Media`, verify length/content and missing-file behavior, and inventory asset formats. | Luna + Terra review |
| 08a | Pending | Establish path and file error contracts | Exercise relative/rooted paths, existence, length and missing-file returns with guest/host pointer checks and saved evidence. | Luna |
| 08b | Pending | Read and classify one real asset | Read one requested `game/Media` asset byte-for-byte, verify its length/content against the retail file, and record the format needed by later rendering. | Luna + Terra review |
| 09 | Pending | Validate allocation and object ownership | Complete 09a and 09b; check guest pointers, alignment, virtual reserve/commit behavior, frees and repeated-load memory growth. | Luna + Terra review |
| 09a | Pending | Verify guest allocation invariants | Probe reserve/commit, alignment, pointer translation and boundary failures with canaries and captured guest addresses. | Luna |
| 09b | Pending | Verify ownership across repeated loads | Exercise one load/free cycle repeatedly, detect double-free/leak/aliasing, and archive bounded memory-growth evidence. | Luna + Terra review |
| 10 | Pending | Validate timers, threads and synchronization | Complete 10a and 10b; startup workers complete, waits unblock for a real reason, timing is monotonic and reproducible. | Luna + Terra review |
| 10a | Pending | Verify timer and wait contracts | Test monotonic time, timeout boundaries, signal/reset and return statuses with bounded repeated probes. | Luna |
| 10b | Pending | Verify startup worker lifecycle | Prove worker creation, wake, join/exit and synchronization isolation on the real startup path, with a captured next checkpoint. | Luna + Terra review |
| 11 | Investigating | Select and wire graphics interception | Trace JSRF's D3D initialization and complete 11a–11d; decide which XDK entry points use host D3D8/D3D11 versus NV2A handling. | Sol Light orchestration; Luna + Terra review |
| 12 | Pending | Create a window and present a clear | Host device and responsive window initialize, and an actual guest command repeatedly changes the presented framebuffer. | Luna + Terra review |
| 13 | Pending | Draw one game-owned UI primitive | Correct vertex/index format, viewport, blend and texture sampling; compare with reference. | Luna + Terra review |
| 14 | Pending | Load one actual menu texture | Decode the format JSRF requests, including swizzle/mips as needed; display with correct alpha/color. | Luna + Terra review |
| 15 | Pending | Reach the title screen | Render an identifiable stable screen; identify intro/FMV sequencing and any temporary skip explicitly. | Luna + Terra review |
| 16 | Pending | Connect controller input | Map movement and menu buttons; verify held versus pressed states and controller disconnect/reconnect. | Luna + Terra review |
| 17 | Pending | Navigate the main menu | Start/back/options work without missing dispatch or stack drift; labels and selection are visible. | Luna + Terra review |
| 18 | Pending | Start a new game and load the opening area | Actual game loader finishes, objects and collision data exist, transition completes. | Luna + Terra review |
| 19 | Pending | Render the opening scene and character | Geometry, transforms, camera, skinning and depth behave correctly for this scene. | Luna + Terra review |
| 20 | Pending | Match JSRF's distinctive rendering | Implement observed cel shading/outlines, materials, transparency and effects; compare the same reference scene. | Luna + Terra review |
| 21 | Pending | Make skating and camera playable | Movement, turning, jumping, collision and camera respond at a stable simulation rate. | Luna + Terra review |
| 22 | Pending | Complete one graffiti interaction | Prompt/input, animation, paint consumption and progression state agree. | Luna + Terra review |
| 23 | Pending | Add audible game feedback | Implement the actual sound path; movement/UI effects play once, at correct pitch and volume. | Luna + Terra review |
| 24 | Pending | Add music and streaming audio | Music starts, loops/transitions and streams without starvation or runaway memory. | Luna + Terra review |
| 25 | Pending | Save and resume the playable slice | New save in isolated host location, exit, restart and restore expected location/progression; handle missing save. | Luna + Terra review |
| 26 | Pending | Stabilize the minimum playable slice | Repeat new-game/play/save/load; sustain at least 15 minutes without crashes, stack drift or unbounded memory growth. | Luna + Terra review |
| 27 | Pending | Cross one area transition | Assets unload/reload correctly, state persists, return transition works. | Luna + Terra review |
| 28 | Pending | Support an additional character and challenge | Character-specific animation/movement and challenge completion/progression function. | Luna + Terra review |
| 29 | Pending | Validate cutscenes and FMV | Decode/play required sequences with synchronized sound and working skip/return behavior. | Luna + Terra review |
| 30 | Pending | Expand area coverage | Maintain an area checklist; load, traverse, interact, leave and return to every area individually. | Luna + Terra review |
| 31 | Pending | Expand mission and encounter coverage | Checklist each mission, enemy/encounter type, failure/retry and progression unlock. | Luna + Terra review |
| 32 | Pending | Complete a full campaign playthrough | New save through ending without debug skips; save checkpoints and capture remaining defects. | Luna + Terra review |
| 33 | Pending | Cover optional/unlockable content | Verify characters, collectibles, challenges and options against a content checklist. | Luna + Terra review |
| 34 | Pending | Harden Windows behavior | Focus loss, resize/fullscreen, controller reconnect, audio device changes and clean shutdown work. | Luna + Terra review |
| 35 | Pending | Profile and resolve bottlenecks | Measure representative scenes, fix demonstrated CPU/GPU/I/O stalls, preserve simulation speed. | Luna + Terra review |
| 36 | Pending | Package a reproducible port | Clean build and launch on another Windows setup with user-supplied assets, documented controls/configuration and known issues. | Luna + Terra review |

Graphics, input and audio may be reordered when the real boot path requires
them sooner. Do not call the minimum slice finished merely because a window
opens, and do not call the whole game finished after one playable scene.

## Historical initializer checkpoint (superseded below)

Artifact: `logs/runs/20260912-215131-391-all-initializers/`.
All 14 observed missing initializer entries now run real translated instructions.
`python scripts/verify-initializers.py` verified 616 initialized words against
an independent interpreter of original XBE instructions; wrappers checked ESP
and nonvolatile preservation at every return. Recovery boundaries/evidence are
in `config/recovered-functions.json`; regenerate only them with
`scripts/recover-functions.py`. The resulting merged function database is
`tools/disasm/output/functions.recovered.json`, and the manual manifest excludes
these external definitions during any complete regeneration.

Use `scripts/build-jsrf.ps1` before running. It rejects compiler failure and
source edits during a build; the runner verifies source/executable identity.
A failed older build/run artifact `20260912-214847-905-initializer-group` is not
evidence of a working initializer group. Subsequent verified builds supersede it.

The then-open milestone 05 captured loop `0x00149B28` in heap allocation `sub_001497DC`.
The free-list head `0x00F81180` points to `0x0105D968`, whose links point to
itself while its chunk is marked allocated. The scan seeks size `0x368`,
but the linked chunk is size 2. The investigation was to find the first incorrect list mutation and
compare its original instructions, operands, and callee ABI before fixing it.
Do not replace the heap or bypass the scan based on this snapshot alone.

## Translation and startup checkpoint (later on 2026-09-12)

Heap hang cause: LOCK XADD's result flags were lost, so COM Release methods
always took their destructor branches and eventually freed the same object
again. Fixed locked atomic flag tracking and snapshotting of the operation's
result (rather than re-reading shared memory). Relifted 82 affected functions.
Compiled regression: 288 cases pass, including intervening writes; original
translator fails (`logs/lifter-before/`). Milestone 05 is complete.

Repeated word/dword comparisons likewise used stale flags after CMPS, making
unequal GUIDs compare equal. Corrected tracking and zero-count ZF preservation;
relifted 140 functions. Additional 300 byte/word/dword comparison cases pass,
including mismatches, reverse direction and zero count. Both CTests pass.

All 541 generated direct stubs now report failure, and this project stops by
default at unresolved/invalid calls (exception E0424943). Explicit historical
fallback requires JSRF_ALLOW_UNRESOLVED=1; do not use it as acceptance evidence.
The first strict-stub stop was an omitted internal branch in 0x00154D70, now
restored. That investigation continued through D3D mode enumeration and missing tails;
see config/boundary-fixes.json and the live report for current artifacts.

Kernel gap names are now identified from the toolkit's ordinal contracts:
204 NtProtectVirtualMemory (16 stack bytes), 144 KeSetDisableBoostThread (8),
91 IoDismountVolumeByName (4), 8 DbgPrint (cdecl, 0 callee cleanup).
None has yet produced the runtime 'no bridge' warning in the verified path.
Keep their implementations pending until required semantics are established.

## GPU boundary checkpoint and decomposed work

Video queries now pass an actual-guest-code probe: 24 mode entries, 212-byte
caps copy with boundary canaries, expected error returns and tested ESP/shared
nonvolatile registers. The game requests 640x480, format 0x11, two back buffers.
Six reviewed boundary fixes preserve real internal branches; 20 recovered
functions include 14 startup initializers and six D3D routines. Device setup
at 0x00192090 runs as far as its hardware helper 0x00194ADD, after allocating
512 KiB for the pushbuffer. There is still no game-owned rendered image.

| ID | Status | Next bounded outcome | Acceptance | Suggested Agent |
|---|---|---|---|---|
| 11a | Complete as corrected documentation audit | GPU setup and port 0x80C0 contract | Every setup address and named register decoded; PCI 0x0C/0x30 meanings and masks independently rechecked; single state owner proposed. Open hypotheses (GPIO effect, vendor 0x4C bytes, PTIMER TIME-write encoding/effect, 0xFE502A, 7th KeInitializeInterrupt arg) are explicit 11b/11d inputs. See docs/jsrf-gpu-setup-contract.md. | Luna + Terra review |
| 11b | In progress (through 11b4b2 complete) | Single coherent NV2A register/command model | Complete 11b1–11b5; real progress drives acknowledgements and a blocked command remains blocked and diagnosable. | Sol Light orchestration; Luna + Terra review |
| 11b1 | Complete | Install one NV2A MMIO state owner | Permanent serialized MMIO trapping, quiesced legacy mutations, actual concurrent VEH probes, generation-validated frozen snapshots, explicit unreadable behavior and clean teardown verified. | Luna → Sol Medium; Terra review |
| 11b2 | Complete | Implement NV2A PCI configuration paths | Immutable identity/class, standard setup fields, exact PBUS range and deliberately shared vendor backing round-trip through the real HAL bridge; other buses/invalid accesses retain zero/ignored behavior. | Luna → Sol Medium; Terra review |
| 11b3 | Complete | Complete PTIMER register contracts | Pinned xemu-compatible split TIME writes, divisors, alarm scheduling, late enable, W1C/PMC aggregation and clean asynchronous service lifecycle are covered by deterministic core tests and the real trapped-MMIO runtime probe. | Luna → Sol Medium; Terra review |
| 11b4 | In progress (through 11b4b2 complete) | Add bounded PFIFO/USER submission | Complete 11b4a–11b4b; execute only observed packet/control and reviewed method forms, preserve guest pushbuffer addressing, and stop explicitly on unsupported work without advancing GET or completion. | Luna + Terra review |
| 11b4a | Complete | Validate packet intake and rollback | Physical-offset USER submission through the real trapped aperture reads only the contiguous window; bounded packet/control decoding atomically commits GET and a method-record stream, while unsupported work preserves all accepted state and reports an exact diagnostic. | Luna → Sol Medium; Terra review |
| 11b4b | In progress (11b4b1–11b4b2 complete) | Execute a reviewed method subset | Complete 11b4b1–11b4b3; apply only method effects whose class binding and host contract are proven; an unsupported method blocks before GET/completion and records its subchannel, method and parameter. | Luna + Terra review |
| 11b4b1 | Complete | Establish atomic unsupported-method boundary | With no class binding model, only NOP 0x0100 on subchannel 0 is accepted; every other method records exact fields and rolls back the entire stream without renderer or completion effects. | Luna + Terra review |
| 11b4b2 | Complete for fixture contract | Prove class binding and first state effects | Fixture-gated handle lookup plus transactional SET_OBJECT proves per-subchannel NV097 binding; only packed clip H/V are accepted, while format/pitch/offset methods roll back as unsupported. | Luna → Sol Medium; Terra review |
| 11b4b3 | Complete | Connect real RAMIN object lookup | Production SET_OBJECT walks claimed PRAMIN RAMHT with the original 0x001945D6 11-bit hash, valid bit, handle match, and class from RAMIN word0 low byte (including `0xB03D` → `0x3D`). Fixture binding stays opt-in for 11b4b2 tests. Focused NV2A PASS 304. Independent review accepted. Solo `logs/runs/20260921-103827-656-ramht-lookup/` still stops at `0x001918E0`. | Next: kick/GET contract |
| 11b4b4 | Complete | Write the kick/GET contract and own the kick latch | `docs/jsrf-kick-get-contract.md` maps the ring control block, `0x00191530` (RET 8, fast/slow exits), `0x001916B0`, `0x001916C0`, `0x00191390`, `0x00191440`, `0x001918E0`, `0x001917F0`, `0x00190240` and its two real callers. The kick is a write to `NV_PFIFO_CACHE1_DMA_PUT` bit 16 plus a poll until the engine clears it, so bit 16 is now a model-owned latch (toolkit `488286f`); the offset mask is `0x1FFEFFFF` because the generic `0x1FFFFFFF` contains bit 16. Kick requests/acks are counted and a blocked stream still acknowledges. 13 new contract cases, NV2A 319. Ordinary boot unchanged at `0x001918E0`, GET=PUT=`0x1000`. | Next: recover the kick chain |
| 11b4b5 | Complete | Recover the pushbuffer kick chain | All 16 chain entries added to `config/recovered-functions.json` (70 total) with `stack_args` read off real epilogues: `RET 8` on `0x00191530`/`0x00191440`/`0x001916C0`, `RET 4` on `0x00191390`/`0x00191730`/`0x001917B0`/`0x00190240`, `RET 0xC` on `0x00190FB0`, plain `ret` elsewhere. `0x001910C0` and `0x001910E0` are separate functions (fall-through); `0x001918E0` tail-jumps to `0x001917F0` and shares its frame. Confirmed the method blocks do not call `0x00191270`; the kick is reached from `0x00191440`. Three latent generator gaps fixed (see report). **The link blocker is cleared**; `jsrf_recomp.exe` now builds (19,245,568 bytes, identity-verified). | Clear the run blocker, then run the chain and expect the accepted-submission contract |
| 11b5 | Pending | Reconcile completion and legacy acknowledgements | Retire conflicting synthetic acknowledgements for modeled registers; implemented work advances completion, while the forced-stall probe remains blocked and yields a useful dump. | Luna + Terra review |
| 11c | Partial | Complete recovered GPU initialization | Complete 11c1 and 11c2; `0x00192090` returns with correct outputs/ABI, including reviewed helpers and port handling; no success stubs. | Sol Light orchestration; Luna + Terra review |
| 11c1 | In progress (chain recovered, the game links, **real-guest-run ICALL open** — see below) | Establish remaining GPU-helper contracts | Hardware setup through framebuffer publish is recovered. Focused 11c1 2197 checks. Ordinary stop `logs/runs/20260921-020149-996-gpu-setup-912a0/` is `0x001918E0`. WBINVD ran. Keep `0x00193F70` and `0x0018E120` fatal. The kick chain `0x001918E0`/`0x001917F0`/`0x00191530`/`0x00191440`/`0x00190240` is recovered per 11b4b5, and `jsrf_recomp.exe` now links with verified build identity. Both known correctness defects are **closed**: `tail_jump_alias` entries are declared and folded into their parent rather than emitted as second bodies, and silent deletion of live jumps went **1309 -> 309 -> 115 -> 40**, with same-function deletions 1032 -> 0. Six toolkit revisions: `58a9cf9` (alias fold), `a301962` (a real tail call is not an intra-body goto — 864 tail calls were being rewritten to `(void)0`), `5d68f27` (create an alias for a branch target mid-body in another function), `ff4d442` (**Cause A fixed**: fold an alias into the body that starts where it ends — deleted jumps **309 -> 115**, duplicate alias bodies **958 -> 0**, header declarations 6,072 -> 5,714), `db54746` (**Cause B fixed**: do not accept a bare `mov edi,edi` as a prologue — `8b ff` is the MSVC hot-patch pad *and* the 2 bytes before a switch table, so the pass was claiming table data as code; the rule now needs a run of >=4 dwords that are each a decoded instruction start at `+2`; `gap_prologue` 363 -> 303, deleted jumps **115 -> 40**, declarations 5,714 -> 5,652), and `9568f29` (two recomp tests that read another module's output dir). **Cause B was mostly not Cause B**: only a minority of the 115 were genuine cross-body branch targets; 71 were phantoms. **The `db54746` fix repairs 91 truncated functions and adds 0 entries.** Verification is **field-for-field identical on the fixture probe** (`--probe gpu-progress --expect-checkpoint probe_gpu`: `normal_exit`, exit 0, 4 snapshots, 36 named frames) — an earlier "guest run regressed" claim was a fixture probe compared against a real guest run and is retracted. Measured correctly, against real guest runs: pre-fix `c98dbdc6` and post-fix `9568f29` are **both** `unhandled_exception` `0xE0464643`, both reach `guest_entry` (log line 48), both issue 157 kernel calls; the difference is that the pre-fix run logged **48** `[RECOVERED] ABI verified` checks and died at `Failed to resolve VA 0x001918E0`, while the post-fix run logs **0** and dies at `[ICALL] invalid target 0x00700010 ... return=0017E627`. The failure moved **earlier**, into startup: `sub_0017DBBD` builds `ecx = MEM32(edi + 8) + index*8` and pushes `MEM32(ecx + 4)` as a callback into `sub_0017E600`, which dispatches via `call eax` at `0x17E625`; `edi` is `[ebp + 0x10]`, a runtime-built table descriptor, and `0x00700010` is in no image section. **`sub_0017DBBD` is the MSVC C++ exception unwinder, not game code** — `0x0017DBA4` is `cmp dword ptr [eax], 0xe06d7363`, the MSVC `_EH_EXCEPTION_NUMBER` magic; `sub_0017D1F8` is `__SEH_prolog`, `sub_0017E3F8` reads `fs:[0x24]`/`fs:[0x28]` (`_getptd`), `sub_0017DC6E` maintains `[eax+0x7c]` (`_catchlevel`), and `sub_0017E600` dispatches the catch handler via `call eax` at `0x17E625`. Crash-dump memory at `0x001EB744` holds `0x19930520` (`EH_MAGIC_NUMBER1`) and `0x001EB760` a valid table pointer, so the EH tables are laid out correctly and `0x00700010` is a **handler-table slot holding a small integer where a code address belongs**. A `JSRF_ALLOW_UNRESOLVED=1` diagnostic run (permitted as a diagnostic, not as acceptance) shows continuing past it gives `esp=0x6A0CC48F` — not a stack address — and a wild read at `0x10000FFFC`, so `0x00700010` is a **hard stop, not a missing callee**; implementing "one more address" cannot fix it. Also note `g_xbox_code_lo/hi` covers **`.text` only** (`0x00011000`..`0x0018CB30`, built from XBE sections flagged `0x04` EXECUTABLE), so every other section is outside it — **do not widen the range to mask this**, `0x00700010` is in no section anyway. **Not shippable; next packet is the SEH chain** — what the demo throws immediately after `guest_entry`, auditing `fs:[0]` and `g_seh_ebp`, since a garbage `esp` is what a desynchronised SEH unwind produces. Verification otherwise green: build identity, all eight compiled suites, 19/19 harness probes, toolkit 256 passed, and the fixture probe field-identical (callers of `sub_0017DBBD`: `0x0017DCE6`, `0x0017DD6B`, `0x0017DDAE`, `0x0017E004`, `0x0017E2EE`). Two regeneration traps, both of which produced wrong numbers I briefly published: **`recover-functions.py` is not the translation pass** (the `recomp_NNNN.c` chunks come only from the manual invocation in AGENTS.md), and **a clean regeneration must clear `tools/disasm/output/*.json`, not just `.disasm_cache.json`**, because the disassembly JSON is an input to the translation. Real figures are 5,652 declarations / 40 gotos, reproduced from a cold start. **`--split 1000` chunk count can change** (6 -> 5 -> 6 across regens), so re-run `cmake -S . -B build` after regenerating or the build keeps a reference to a chunk that no longer exists | Reach the `probe_gpu` checkpoint on `gpu-submit-supported` and satisfy its accepted-submission contract: `diag=ok sink=1 successes=1 atomic=accepted` with `USER_DMA_GET == USER_DMA_PUT`. **`unsupported_method` belongs to `gpu-submit-blocked` only and would be a defect here** — an earlier version of this row said the opposite and was corrected. All three submission modes pass through the harness. A probe run expects **its probe checkpoint only**: `--probe=` returns before `checkpoint("guest_entry")`, so the default `['memory_ready','guest_entry']` does not apply and passing it reports a false failure. Treat a fixture submission as necessary but not sufficient: it does not prove the guest ran a real command stream |
| 11c2 | Pending | Recover and integrate reviewed GPU helpers | Add only approved helper entries/internal tails, run behavior and ABI checks, then archive the next real stop or prove `0x00192090` returned. | Luna + Terra review |
| 11d | Pending | Guest ISR/DPC execution | Complete 11d1 and 11d2; correct worker stack/register/IRQL isolation, notification and reentrancy tests finish relevant 01c/01d gaps. | Sol Light orchestration; Luna + Terra review; Sol Medium if concurrency evidence conflicts |
| 11d1 | Pending | Prove guest callback execution context | Verify ISR/DPC entry ABI, guest stack/register ownership, IRQL and per-thread isolation with direct callback probes. | Luna + Terra review |
| 11d2 | Pending | Deliver GPU interrupts through ISR/DPC | Exercise signal, masked, missed-signal and reentrant cases; callbacks run for modeled causes and no unrelated worker state is corrupted. | Luna + Terra review; Sol Medium if ambiguity remains |
| 12 | Pending | Window and first game-owned clear/present | Responsive Windows window and an actual guest command changes its framebuffer. | Luna + Terra review |

Keep the guest device/resource layout and connect the NV2A/D3D11 path behind
it. The existing toolkit acknowledgement worker and optional software executor
are not a complete GPU; their state ownership and completion semantics must be
reconciled before claiming rendering. Detailed task packets are in AGENTS.md.

## GPU register and capture checkpoint

`docs/jsrf-gpu-setup-contract.md` records original setup instructions, pinned
xemu implementation references, and local integration gaps. The NV2A reset
clock disagreed with its PLL coefficient: writing the same value changed the
clock from 466666648 to 233333324 Hz. Reset now reuses the register-write path.
The new compiled register-only fixture failed before and passes all 11 checks
afterward; logs/gpu-registers-before.txt and gpu-registers-after.txt preserve
evidence. All three CTests pass. No renderer is exercised by this fixture.

GPU audit exposed separately allocated physical memory missing from the old
dumps. The collector now adds the entire committed/readable 64 MiB contiguous
window and 16 MiB GPU backing, including reserved instance memory. All nine
harness probes pass with known contiguous-allocation marker bytes verified in
the dumps. The latest ordinary run retains all 616 initializer words, captures
the same missing 0x00194ADD, and permits reading pushbuffer address 0x80001000.
When registers become trapped, a frozen model-state snapshot must replace the
readable-aperture capture. Neither these tests nor existing acknowledgements
prove real GPU command completion.


## GPU harness completion and next-session handoff

Milestone 01f is verified in `logs/harness-test-results.json`; all thirteen
collector probes and five CTests pass. The frozen GPU report distinguishes
unreadable storage, synthetic progress, acknowledgement mode and pre-submission
state. The ordinary checkpoint above retains all 616 initializer words and stops
at the same missing helper. Native NVIDIA/WARP clear/copy/readback passes with
4,096 checked pixels and no D3D11 debug errors. RenderDoc capture is verified.

Nsight Systems is installed and produced a report with annotations, but its
exported diagnostic warns DX11 profiling may not have started correctly. Treat
01g as partial; the wrapper only validates report creation. Detailed artifacts,
commands and limitations are in AGENTS.md and the current report.

The user requested a handoff focus due to remaining weekly usage. The corrected
11a contract audit is complete; the next bounded packet is 11b, not broad
renderer implementation.
Current GPU harness changes remain uncommitted in both repositories. Inspect and
preserve them before proceeding; earlier commit hashes represent the baseline.
