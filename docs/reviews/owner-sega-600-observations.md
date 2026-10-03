# Owner-directed 600-second SEGA observations

## Authority and boundary

Owner parks the fail-fast observer: no retry. Ordered chores only, no new fixtures/observer harnesses or bounded units, at most one quick Advisor consult per step; ambiguity is recorded then checkpointed, not investigated further. W14 bounds explicitly waived for these runs. History scrub untouched. These observations do not resume the paused broader goal.

## Checkpoint 1 receipt

Records metadata repaired using existing ruling content and one quick Advisor metadata consult; existing ruling checker wired into `just check` and CTest. Guarded build identity refreshed after CMake changed; no regeneration. Production executable SHA-256 remains `74377ac6130e812b7a830492d4995bdc58efb8899ff5c1d6c23b7ee6c0c0424e`. `just check` green; CTest 32/32. Fresh Reviewer `3084f0a2-73d3-4dbf-b449-761555bacc4a`, pinned `claude/claude-opus-5-5` medium, accepted checkpoint 1. Erroneous non-roster content-review dispatch is documented in the plan, not counted as acceptance.

- PUSHED_TO: origin / BRANCH: main / COMMIT: `348a0e38fdf3fd940d6e5f79ad24cf25fbcf2c41` / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: success, already up to date (toolkit first).
- PUSHED_TO: origin / BRANCH: master / COMMIT: `fd435ac262c546d1dfae09db3a501560df137bf5` / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: success, fast-forward `6121247..fd435ac`.

## Checkpoint 2 receipt

`just check` green; public staged-path/secret/blob-size pre-commit gates passed; both trees clean before push. Toolkit first:

- PUSHED_TO: origin / BRANCH: main / COMMIT: `348a0e38fdf3fd940d6e5f79ad24cf25fbcf2c41` / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: success, already up to date.
- PUSHED_TO: origin / BRANCH: master / COMMIT: `ded98fbdf9387ad87fe6aaeaeabb1bfbfe063cc6` / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: success, fast-forward `fd435ac..ded98fb`.

## Step 2 launch choice

One current-build exploratory run, requested 600 seconds. Same environment as D2 300-second smoke: `RECOMP_APU_TRAP=1`, `RECOMP_FB_WINDOW=1`, `RECOMP_FB_WINDOW_DUMP_EVERY=600`, `RECOMP_KERNEL_LOG_BUDGET=100000`, `RECOMP_NV2A_TRACE=1`, `RECOMP_PB_EXEC=1`, `RECOMP_PB_SCAN=1`; `RECOMP_GPU_ACK` absent/default. `RECOMP_FB_DUMP` points to a new observation directory, `logs/workers/owner-d2-600-framebuffers`.

Fresh disposable save root selected: unchanged runner offers no seed option and refuses nonempty roots. The optional prior-root copy is not used; prior D2 save root and original assets remain unchanged. The 600-second interval includes cache fill, not 600 seconds after the marker. No seed ledger entry is introduced.

D2 switch/device IDs: **L14–L18, L20–L25, L39, L40**; L16 is configured but legacy acknowledgement/register mutation body is inert under the aperture owner; L18 uses the active NV097 consumer. L19 dormant. L21–L24 depend on APU_TRAP, while L25 remains always active. This does not invent a ledger ID for accepted directory-context cleanup, which corrects wrapped file I/O (L08), not a new shortcut. Always-active non-Emulated/non-Translated support paths include L02–L12 (L06 limitations), L25–L26, L29–L31, L33, L35–L38; optional keyboard/USB/serial/tripwire paths are not selected. Counter increase and diagnostic deadline do not establish a changed image or liveness.

Run ID **`20261002-014152-190-owner-d2-600`**; metadata `started_utc=2026-10-02T08:41:52.830618+00:00`. Archived metadata confirms exact D2 settings above and production/collector identities. The record was created after launch; run source archive identity is checkpoint-1 commit `fd435ac`, not a silently edited runtime.

## Step 2 result — still SEGA

Archived `result.json`: `diagnostic_deadline`, `exit_code=3`, `duration_seconds=603.032515`, `dump_ok=true`, `save_root_verified=true`, checkpoints passed, `gpu_report_ok=true`. Runner/shell exit 3 is the documented deadline classification, not a test crash or success claim. Stop UTC by ledger convention: **2026-10-02T08:51:55.863133Z**.

67 BMP files produced, **two distinct full-file SHA-256 values**:

| Captures | UTC mtime interval | SHA-256 | Meaning |
|---|---|---|---|
| 000–007 (8) | 08:41:56.139858–08:41:57.350799 | `9dc5cdcf2b4abd98a266b99496fa0062bec2c56e30b979a7cb5d033275eed1b3` | early executor-surface dumps, not the display-window sequence |
| 008–066 (59) | 08:42:06.979210–08:51:50.165120 | `905e9204f3dbb0accbd266c16357ac850cd490f5f5a0e6e2c45f2dbcf315cd9f` | timestamp-distinct window captures, identical SEGA image |

Session inspected the final window capture losslessly converted to PNG: **Presented by SEGA**, no title. All 59 window captures are byte-identical, spanning **583.185910 s**. The existence of two hashes does not mean SEGA was exited: they are different surfaces/early phase, not a late display transition.

The unnumbered marker exists at `save-root/Cache/Media/Cache/JSRF_CACHE_COMPLETE.CMP`, UTC mtime **08:45:14.621172**; log line 232735 names it. Marker-to-stop interval **401.241961 s**; final window sample is about 395.54 s after the marker. These timings are filesystem observations, not guest simulation-time measurements. No sampled window evidence of exiting SEGA; transient changes between samples cannot be ruled out. Deadline alone proves no liveness, causal fade class or strict-horizon move. **Checkpoint 2 closes as still-SEGA observation; conditional step 3 is now applicable.** No step-2 Advisor consult needed, no new fixture or fade-dump investigation.

Local reproduction: hash all `logs/workers/owner-d2-600-framebuffers*.bmp`; read this run's metadata/result; compare filesystem UTC mtimes; inspect window BMP 066 (PNG conversion only for display). Proprietary run artifacts/images stay ignored and are not committed.

## Step 3 launch — conditional no-APU observation

Still-SEGA condition observed in step 2; one 600-second run requested. Same binary, collector and D2 profile with **`RECOMP_APU_TRAP` absent** and new dump prefix `logs/workers/owner-d2-noapu-600-framebuffers`. Fresh disposable root again; no prior-root copy or runtime edit. Step-3 launch source identity is checkpoint-2 commit `ded98fb`.

**Ledger-ID difference:** remove active **L21–L24** (GP DSP, voice processor and two APU stubs depend on APU_TRAP); **L25 stereo-fold approximation remains always active**. Remaining D2 IDs **L14–L18, L20, L25, L39, L40**, with L16 configured/inert under owner and L19 dormant. Always-active support ledger IDs remain as listed above. Setting removal does not create a new unrecorded shim or imply fidelity. No step-3 Advisor consult needed; no extension beyond the single requested observation.

## Step 3 result — visual outcome UNKNOWN, no framebuffer evidence

Run ID **`20261002-015434-370-owner-d2-noapu-600`**. Archived metadata confirms APU_TRAP absent, the other profile settings unchanged apart from new dump/log/root paths, and the same executable/collector hashes. Start UTC **2026-10-02T08:54:35.049849Z**. Archived result: `diagnostic_deadline`, `exit_code=3`, `duration_seconds=602.440777`, `dump_ok=true`, `native_threads=14`, `named_frames=118`, root verified, checkpoints passed, `gpu_report_ok=true`. Stop UTC by ledger convention **2026-10-02T09:04:37.490626Z**. Exit 3 is the documented deadline classification.

**No BMP files match this run's dump prefix; no FBWIN/framebuffer or CMP-marker line appears in the archived log; no `JSRF_CACHE_COMPLETE*.CMP` exists in this disposable root.** Therefore no timed frame hashes can establish either still-SEGA or SEGA exit. **Visual outcome UNKNOWN**, not evidence that APU removal fixes or explains the stop. `named_frames` means symbolized native stack frames, not rendered images. Deadline and GPU analysis success do not establish liveness or a strict-horizon move.

The initial offline summary command incorrectly assumed at least one BMP and exited 1 with IndexError after printing COUNT 0 / no markers. Corrected timestamp extraction did not assume images. This was an analysis-command error, not another guest attempt. Ambiguity recorded; **no rerun, new probe, consultation or further investigation**. Both authorised observation runs are spent; owner-directed chores stop here after checked commit/push.

Final checkpoint-3 push receipt is emitted in the closure response (avoids a self-referential commit hash); checkpoint 1 and 2 receipts above are durable.

## Owner-authorized archived classification (2026-10-02; no new runs)

This later owner instruction authorizes read-only analysis of the two existing runs and documentation only; it supersedes the preceding analysis stop for this chore, not the observer park or broader-goal pause. No runtime implementation, fixture, dump-memory interpretation or guest launch. One quick Advisor consult: `e96296aa-510e-453b-bc58-ed2ed04bc7e3`, live-resolved `claude/claude-opus-5-5` high. Advisor verified that the current generated/recovered source hashes match the D2 archived build-source identity; both runs have the same executable identity recorded above.

### Exhaustive post-marker file-open/read list: empty

Scanned the entire D2 archived [log](../../logs/runs/20261002-014152-190-owner-d2-600/jsrf_run.log) for `[PATH]` and `[READ]`. **Every match is at or before line 232735. Lines 232736–393951 contain zero `[PATH]` and zero `[READ]` records.** Thus the requested list of new file opens/reads after the unnumbered CMP marker has **no entries**, including no intro movie and no audio stream. The marker itself is followed by successful `[FILE]` status and close at lines 232736–232740; it is not a later media open. No `.sfd`, `.sfa`, `.adx`, Sofdec or movie-name match exists anywhere in this log. Earlier sound/cache asset reads are not post-marker stream playback. An opened-and-repeating movie read is therefore **not observed**; F5's conditional intro-skip instruction is not activated by this evidence. This is a logging observation, not proof about every possible packed/in-memory media source.

### APU and DirectSound after the marker

The whole-log APU/voice/stream/position/notification search finds only initialization (lines 48–54) and title-start (3203), all before CMP. **Voices playing, advancing stream positions and delivered audio notifications are UNKNOWN**, not measured zero: the archived environment has no APU trace switch, there is no audio capture, and the function trace is the DSOUND critical-section helper `sub_0019E438`, not a status/position/notification API trace. No new tracing was added.

DSOUND work does continue and return after CMP: helper enter/exit pairs appear immediately at log lines 232741–232797 and still at 393902–393906. Advisor counted 8070 entries from each of `001A0383`, `001A0413`, `001A0489`, and 2690 from each of `0019E802`, `0019F266`, `0019F2AD` after CMP. The frozen main-thread [stack](../../logs/runs/20261002-014152-190-owner-d2-600/stacks.txt#L51899-L51929) is in push-buffer submission from `13A80 → 14D090 → 198F10 → 198ED0 → 191390 → 1912A0`; `198F10` increments a frame counter and calls submission. The retained event-ring cycles include matching DSOUND lock releases; they are partial history, not full playback telemetry. The APU frame thread is at throttle (stack lines 51988–51995), which does not establish voice state. **Presentation and per-frame DSOUND servicing continue while the sampled image remains SEGA; the submission stack is not itself the scene-exit gate.** No fidelity or whole-game liveness acceptance follows.

### Without APU_TRAP: earlier DSP initialization gate confirmed

The no-APU frozen main-thread [stack](../../logs/runs/20261002-015434-370-owner-d2-noapu-600/stacks.txt#L198352-L198372) is `sub_001A1769 → sub_001A19D6 → sub_0019E6D7 → sub_0019F05C → 00168130 → 00118C00 → 00012AE0 → 00012C10 → 0006F9E0`. Source line 21935 is the `loc_001A18D0` **DSP pending-word spin**, `cmp [ebx],0 / jne`, following the pending-word store. Archived registers report `ebx=803BC810`, a DSP block +0x810; this run's address is not silently replaced by the older TR example `803C0810`. This is the known [TR §4 pending-word gate](../jsrf-technical-record.md#the-spin-a4a-r2-discovery-accepted-2026-09-24), within DSOUND initialization, **not** a playing-voice status poll. The last retained lock acquisition has no release; the spin holds the DSOUND critical section. Initialization precedes entry to the main loop.

The last path is `Media\\Sounds\\dsstdfx.bin` (log line 4887). The [log tail](../../logs/runs/20261002-015434-370-owner-d2-noapu-600/jsrf_run.log#L465610-L465654) shows only other threads' repeated kernel wait/timer calls, not completion of the main-thread spin. No CMP/framebuffer exists, and the archived result remains a diagnostic deadline. **APU_TRAP, specifically its L21 GP DSP path, is required to reach the logo on this observed build/path.** Removing L21–L24 moved the stop earlier; it does not explain the later SEGA exit predicate or prove those paths universally necessary.

### Most likely later gate and cheapest honest shortcut (recommendation only)

**Leading hypothesis: an unmet audio-completion/progress predicate for the SEGA scene (buffer/stream status, position or notification); confidence low, exact predicate/VA UNKNOWN.** A movie-player gate is unsupported by the absence of movie opens; a GPU submission location is a presentation vehicle, not causal proof. A timer/fade or other scene-state predicate remains possible. Crucially, absence of audio telemetry is **not evidence that audio never advances**. The two archives cannot distinguish these predicates conclusively.

Cheapest honest shortcut, **if the audio predicate is subsequently identified**, is to bypass only that one logo sound-completion condition, not decode movies or repair the entire audio model. Before any implementation, identify its exact site and contract and create a *Patched* or *Intentionally ignored* compatibility-ledger entry with an explicit exploratory switch, hazards, and removal gate (real playback/notification completion); future run records must cite that ID. No new ledger ID is allocated for an unimplemented proposal. No blanket DirectSound success, guessed pending-word clear, GPU acknowledgement change, cache seed, or unlocated whole-scene skip. F5's movie skip remains conditional on an actual intro blocker. Nothing implemented, no observer retry, no new runs; classification chore stops after checked commit/push.

Closure push receipts for this records-only commit are reported with the final commit identity (avoids a self-referential hash).

## Owner-authorized named no-op kernel observation (2026-10-02)

Toolkit pulled first, fast-forward `348a0e3 → 929856fcfc145036252510509abaa0782d910a22`. The change only names each do-nothing bridge on its process-wide first call; it does not implement completion. Owner authorizes one rebuilt 600-second D2 exploratory run and records, no fix. Broader goal stays paused; observer stays parked.

### Read-only first: last three archived summary blocks per thread

D2 [archive log](../../logs/runs/20261002-014152-190-owner-d2-600/jsrf_run.log) after marker line 232735. Headers lack a native tid: attribution uses guest stack region matched to explicit kernel-call `tid=` records. Rankings are **cumulative since thread start**, not post-marker interval counts. Summary output interleaves: adjacency alone is not attribution. Listed active ordinals are independently observed in post-marker call records; main-thread detailed calls stop at its 100000-call budget, while summaries continue. No claim that threads without a final summary perform no kernel work.

| Native tid / guest stack region | Last three summary header lines / total calls | Final ranked ordinals and cumulative counts | Ordinals explicitly called after CMP |
|---|---|---|---|
| 42264 / `00F7` (main) | 392722 / 138806; 393224 / 139198; 393691 / 139618 | 277 ×55294; 294 ×55294; 161 ×8372; 160 ×4468; 129 ×3905; 289 ×2303 | 129, 160, 161, 187, 199, 277, 294 |
| 65924 / `007B` | 392785 / 3899; 393421 / 3905; 393869 / 3909 | 119 ×1955; 145 ×1954 (only two occupied ranks) | 119, 145 |
| 5160 / `0123` | 392812 / 24189; 393436 / 24228; 393896 / 24254 | Last unambiguously contiguous six at 393437–393442: 246 ×5585; 250 ×5584; 224 ×3729; 143 ×3722; 159 ×1868; 124 ×1862. Final interleaved block: 246 ×5591, 250 ×5590, 143 ×3726, 124 ×1864; remaining ranks not safely attributable | 124, 143, 159, 224, 231, 246, 250 |
| 59404 / `012B` | 392816 / 3844; 393433 / 3850; 393900 / 3854 | 159 ×1926; 224 ×1926; 277 ×1; 294 ×1, interleaved with 5160. Shared static `shown_ord` scratch can omit/duplicate ranks under concurrent summaries; do not reconstruct six invented entries | 159, 224 |
| 51468 / `0133` | 392829 / 3625; 393457 / 3631; 393928 / 3635 | 231 ×3633; 277 ×1; 294 ×1 (only three occupied ranks) | 231 |

Ordinals: 277/294 enter/leave critical section; 160/161 raise/lower IRQL (fastcall); 129 raise IRQL to DPC; 289 initialize ANSI string (unchanged main total, not evidence of post-CMP calls); 187 close; 199 free virtual memory; 119 insert DPC; 145 set event; 159 wait for single object; 224/231 resume/suspend thread; 246/250 reference/dereference object; 143/124 set/query base priority. These establish repeated service/synchronization/clock activity, not the SEGA exit predicate.

Build receipt: guarded `just build` succeeded without regeneration. First full CTest 31/32: `xbox_guest_meter` failed a kernel-return concurrency assertion. Correction from T3 audit: the recovered 31 ms DPC overrun belongs to an older Oct 1 **passing** log, not proven to the failed run; do not attribute it to that failure. Isolated rerun passed; second **full CTest 32/32 passed**. No code altered to hide the failure. Initial unittest invocation ran zero tests (wrong runner); direct `python -X utf8 ../xboxrecomp/tools/kernel_audit/test_noop_bridges_are_named.py` passed, naming all 67 bridges.

New run uses the same D2 environment/ledger IDs as Step 2, a fresh disposable save root and fresh framebuffer directory; only toolkit observation build differs. Because the kernel lines have no intrinsic timestamps, an external host tail reader records line-numbered UTC receipt times at 50 ms polling in `logs/workers/owner-kernel-noop-receipts.jsonl`. Times are **host observations**, subject to scheduling/buffering latency, not exact guest invocation times. No runtime trace switch or collector changed.

### Sole new run: complete first-call inventory and outcome

Archive `20261002-132843-938-owner-d2-kernel-noop-600`; game source `a5e06d9`, toolkit `929856f`; EXE SHA-256 `f1a7a7a445f4f658adfab756b8c05fb7e7634453550bbb97c84872933c69dbe7`. Start `2026-10-02T20:28:44.712643Z`, derived stop `20:38:49.541471Z`, duration 604.828828 s. `diagnostic_deadline`, result code 3, fresh root verified; 21 native threads, checkpoints passed, dump and offline GPU report successful. Wrapper shell exit 1 reflects the runner's nonzero deadline return, not a guest crash; result governs. Exactly one guest launch, no rerun.

All first-call lines in the **complete final log** (two total):

| Log line | UTC host receipt | Receipt seconds from metadata start | Bridge / caller | CMP relationship / suspicion |
|---|---|---|---|---|
| [824](../../logs/runs/20261002-132843-938-owner-d2-kernel-noop-600/jsrf_run.log#L824) | 20:28:48.795267Z | 4.082624 | `HalRegisterShutdownNotification` / `0x00194BAF` | Before CMP; shutdown registration side effect, not an identified logo gate |
| [4247](../../logs/runs/20261002-132843-938-owner-d2-kernel-noop-600/jsrf_run.log#L4247) | 20:28:49.608284Z | 4.895641 | `KeSetDisableBoostThread` / `0x00147D92` | Before CMP; thread scheduler boost policy, not an identified logo gate |

CMP PATH line [247970](../../logs/runs/20261002-132843-938-owner-d2-kernel-noop-600/jsrf_run.log#L247970) received at `20:32:19.342266Z` (+214.629623 s). **No first-call no-op appeared after CMP**, so this experiment yields **no new prime suspect** under the owner's late-first-call criterion. First-call suppression is process-wide: later reuse of an already named bridge is invisible; this does not rule out partial/non-noop bridges, guest audio code, or missing asynchronous completion. The trace is not voice/notification telemetry.

Final framebuffer remains **Presented by SEGA**. Capture-path qualification: the supplied fresh `RECOMP_FB_DUMP` basename had no trailing directory separator; runtime overwrote that one file at each of 60 logged FBWIN writes, plus an initial suffixed `000.bmp`. Therefore this run proves the **final image**, not 60 retained identical images or a continuous still-SEGA interval. No rerun to repair that limitation. Initial fresh path and settings are in metadata; original assets/saves untouched. This observation is exploratory and moves no strict horizon.

### Bounded judgement / stop

One quick Advisor consult only, `claude/claude-opus-5-5`, high, child `759e9303-9388-4b2a-ae3f-335e2008667b`. Caller inspection: `sub_00194ADD` registers shutdown callback `001941E0` in DSOUND initialization and continues without consuming a result; the `00147D92` thread-creation wrapper similarly discards the boost-control return before releasing the reference. Neither provides a plausible scene-completion dependency. No repair/shortcut for these two is justified.

Most likely remaining gate is still **logo scene completion, possibly logo-audio completion/progress**, low confidence, exact predicate/address **UNKNOWN**. New naming evidence neither measures audio nor elevates that hypothesis. Cheapest honest future change remains: identify the one scene-exit condition first, then either implement its actual missing completion bridge if proven, or bypass only that predicate with a new compatibility-ledger entry, explicit exploratory switch, hazards and removal gate. No ledger ID allocated for an unlocated shortcut; no implementation, blanket DSOUND success, forced DSP word, cache seeding, GPU_ACK change, movie skip, or observer work. Broader goal remains paused; stop after records/checks/commit/push.

Push receipts for this records-only closure: `PUSHED_TO: origin / BRANCH: main / COMMIT: 929856fcfc145036252510509abaa0782d910a22 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: up-to-date (toolkit first)`; game `PUSHED_TO: origin / BRANCH: master / COMMIT: this records-only commit (identity in final response) / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward`. No runtime/helper source or local artifacts included.

## T3 — read-only xemu SEGA exit oracle (2026-10-02)

**Supersedes the audio-leading / gate-UNKNOWN judgement above. No port fixes or new port runs.** xemu 0.8.136 (`fc24584ce88f0915ad7f04775bb7712c2e3f49ee`) ran with a disposable `-snapshot` HDD overlay and read-only i386 gdbstub observations. Verified desktop images show **Presented by SEGA** before and **Created by Smilebit** after. Earlier heuristic captures were still SEGA fades, not successors. QMP quit succeeded; subsequent process inspection verified xemu absent. The original EEPROM was used; its protection/unchanged status is not established.

Evidence stays outside git in `%TEMP%/owner-sega-oracle`: `sega-ram.bin`, `departed-ram.bin`, register packets, control reads, screenshots, and per-image holes manifests. These are **guest-VA images**, not physical RAM dumps. Unreadable pages are placeholder zeros, excluded from comparison (about 80–83% holes). Control at `0x11000` matches XBE/port bytes `8b512c85d28b4130c70190431c00741c`. The 344-byte i386 register packet includes 16 decoded dwords and 280 undecoded x87/SSE bytes. Changed readable state, not hole zeros, supplies the comparison.

### Actual presentation predicate — high confidence

Logo object vtable `0x1CCFB8`, update slot +4 = **`0x7E360`**. Actual pointers: oracle `0x515030`, port `0x143EE60`; no guessed delta correction. `0x7E360` switches on **logo+0x98** (16 phases). Normal SEGA segment:

1. Phase 0 waits on `0x24650()` (fade complete), then advances to phase 1 and clears logo+0x9C.
2. Phase 1 increments **logo+0x9C each update**. At **counter > 0x78 (120)** (`0x7E412–0x7E424`) it advances to phase 2 and calls `0x24620(0xFF000000, 1/120)` (`0x7E4AF`). Optional controller input sets logo+0xA0 and can shorten the hold; sampled input latch is zero.
3. **Phase 2 waits for `0x24650() != 0` at `0x7E444–0x7E44B`.** This is the exact blocked SEGA exit predicate in the archived port. Nonzero advances the phase and calls `0x24620(0, 1/120)` to fade the successor in. Thus the ordinary producer is **per-frame fade integration**, not audio completion, a kernel event, wall-clock duration, or the render scene-ready gate.

`0x24650` fetches app subsystem 6 via `0x128C0` and returns **fade+0xC0** (returns 1 only if no object). `0x24620` forwards to `0x24540`, which writes target ARGB channels fade+0xA8..0xB4, step+0xB8 and clears done+0xC0. Fade update vtable+4 **`0x24700`** moves current channels +0x98..0xA4 toward targets, clamps on convergence, and sets **+0xC0=1 once all four channels match** (`0x24943` region). Draw `0x24400` consumes packed colour+0xBC; completion is not produced by draw.

| Actual field | Oracle SEGA | Oracle Smilebit | Archived port |
|---|---:|---:|---:|
| Logo phase +0x98 | 0 | 5 | **2** |
| Logo hold counter +0x9C | 0 | 121 | **121** |
| Logo input latch +0xA0 | 0 | 0 | 0 |
| Logo auxiliary latch +0xA4 | 0 | 1 | 1 |
| Fade current alpha +0x98 | ~0.25833 | ~0.66667 | **0** |
| Fade target alpha +0xA8 | 0 | 1 | **1** |
| Fade step +0xB8 | 1/120 | 1/120 | 1/120 |
| Fade done +0xC0 | 0 | 0 | **0** |

Fade pointers oracle `0x600E60`, port `0x15F0E60`, vtable `0x1C4D10`. The two oracle snapshots catch different fades **in progress**; done need not be 1 in the successor snapshot. The phase change plus verified screenshot proves that an intervening fade completed; snapshots are not a full per-call trace.

### Frame/render path and rejected cache-machine inference

Port app `0x1063A70`, oracle `0x363A70`. app+0x18/+0x24/+0x94 are zero in both snapshots and port. Oracle tick `0x265174` 100→700; app frame counters 101→697. Port tick 2567, counters 3766. `0x13A80 → scene vtable+0xB8 → 0x14D090` performs presentation/timer work; `0x14D080` scene slots +0xB0/+0xB4 return zero, not a negative ready condition. Neither this gate nor increasing frames explains fade convergence by itself.

`0x123E0 → 0x11070` walks app+0x87DC, calling node vtable+4 then child+0x28/sibling+0x30 unless node flags+4 are negative. In the archived port tree the fade is reachable: root `0x108FF40 → 0x1340060 → 0x108FFA0`, child `0x15F0E60`; fade flags `0x10003` are nonnegative. Logo sibling `0x143EE60` has flags 9. Reachability in a frozen tree does **not** prove the armed fade instance was updated on every prior frame.

Worker initially promoted cache object `0x430060` state25→26, substate3→10, latch1→0 as the logo gate. **Rejected by Session comparison:** port counterpart `0x1340060` already has state **30**, latch0, substate10, pending fields +0x50/+0x54/+0x64 all0. Its `0x25310/0x25390`, `0x2DBE0` and `PRESS/DEFAULT` asset walk are separate cache/state machinery, not the actual presentation exit. The `.data` comparison found 16 changed dwords (282 across readable compared memory), including the cache tables/overlay, but heap logo/fade state is decisive. State changes, successful asset opens and `D:`/`Z:` rewrites alone cannot name the presentation predicate.

### One quick Advisor consult; future action only

Advisor `c11bbc0a-6897-4d25-8315-801b3e8ab313`, `claude/claude-opus-5-5`, high: original `0x24700` and inspected recovered alpha arithmetic agree; FCMP status bits and parity masks are correct. Advisor also checked the archived source bundle: the fade body comparison chain and FPU/parity helpers match the working tree; the map links `sub_00024700` from recovered.obj, and both archived patch files are empty. **Do not diagnose a parity defect from this capture.** Alpha0/target1/done0 after arming strongly suggests missing updates of the armed object, but invocation/instance mismatch, resets or runtime execution faults remain unproven. First-call `[RECOVERED]` logs do not prove post-arm execution.

Cheapest honest future repair: diagnose the armed subsystem-6 instance's `0x24700` invocations and actual stores, then correct only the demonstrated traversal/instance/runtime defect. No completion bridge or bypass is presently justified. For a pragmatic shortcut, only this located phase-2 predicate could be considered, with a new explicit exploratory switch, ledger entry, hazards and removal gate; **none allocated or implemented**. Audio is not implicated by this exit path; no `RECOMP_APU_TRACE=1` run was made or recommended as the next diagnostic. No GPU_ACK changes, cache seed, new observer, movie skip or title acceptance. Broader goal stays paused.

### CTest evidence correction and remaining limit

Raw failed full-suite output has not yet been recovered from the available surviving artifacts. The earlier reported assertion text was `FAIL: the kernel return re-entered while the worker was inside`; this is retained context, **not newly recovered full raw output**. The surviving toolkit `build/Testing/Temporary/LastTest.log` is an older Oct 1 passing run: its raw line `[GSERIAL] overrun: dpc on tid 43312 waited 31 ms; holder tid 3976 -- running without the lock` precedes `Test Passed.` It cannot substantiate the failed run's overrun. Isolated and second full-suite passes are unchanged historical observations, not evidence that the original timing-sensitive failure was fixed.

Records validation: `just check` all checkers passed; `git diff --check` clean; two staged record blobs scanned by `scripts/secret-audit.py`, zero secret hits, sizes 30,242/109,688 bytes before this validation sentence (far below 100 MB), no `game/` path. Documentation-only work required no rebuild. Fresh turn-end Reviewer `04d83ae2-8589-4f11-a12a-008b68d70027`, `claude/claude-opus-5-5` medium, **ACCEPT**; independently confirmed records-only diff, clean toolkit and no xemu process. Unrelated local session marker preserved/excluded, not staged.

T3 push: `PUSHED_TO: origin / BRANCH: main / COMMIT: 929856fcfc145036252510509abaa0782d910a22 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: up-to-date (toolkit first)`; game receipt uses the resulting records-only commit identity in final response. Proprietary RAM, generated source, screenshots and scratch scripts excluded.

## Folded-alias follow-up — archived evidence only (2026-10-02)

Owner boundary: **read-only, no new runs or fixes**. Reused `20261002-132843-938-owner-d2-kernel-noop-600`; no xemu restart, guest launch, build, recovery generation or manifest edit. This follow-up proposes an L02 recovery, not a scene-exit bypass.

### Log census and capture limitation

The complete archived 30,973,165-byte log contains **1 `[ALIAS-ICALL]` line**, at [line 2652](../../logs/runs/20261002-132843-938-owner-d2-kernel-noop-600/jsrf_run.log#L2652):

```text
[ALIAS-ICALL] target=0x0014FEF0 owner=0x00150231
```

The run's own archived dispatch source has 134 map pairs. `recomp_alias_observe` atomically increments the total and per-entry hits before testing first sighting and the <=8 distinct-print cap. One line is **not cap saturation**; there is no evidence of eight logged aliases hiding later ones. It proves this alias was invoked at least once, **not its exact count or an exhaustive nonzero-counter census**. Log completeness/loss is not interchangeable with a frozen counter array. The wrapper uses zero-based map/hits index **111** and calls `sub_00150231` from its start.

The requested dump counter census is **BLOCKED by uncaptured host memory**, not zero hits. `check-dump-mapping.py` passes: matches1, mismatch/unreadable/missing0 at guest control `0x11000`. This establishes guest-image mapping only; it cannot make host globals readable. Worker parsed matching PDB MSF info and dump CodeView records: GUID **135BA5C2-6096-4517-8E6D-3D541722BD3F**, age **84**, exact match. No `cdb.exe` was available; symbol locations below come from the matching archived linker map plus dump module ASLR base, and missing-page proof from all110 memory descriptors, **not a successful debugger counter read**. ModuleListStream gives actual EXE base `0x7FF789340000` (size `0x361E000`); archived map preferred base is `0x140000000`.

| Symbol | Image RVA | Actual host VA | Frozen value |
|---|---:|---:|---|
| `g_recomp_alias_icall_entries` | `0xD6BC00` | `0x7FF78A0ABC00` | not captured; archived source constant **134** |
| `g_recomp_alias_icall_map[][2]` | `0xD6BC10` | `0x7FF78A0ABC10` | not captured; all134 static pairs available in archived source |
| `g_recomp_alias_icall_count` | `0xEC90D0` | `0x7FF78A2090D0` | **UNKNOWN** |
| `g_recomp_alias_icall_hits[]` | `0x25C3120` | `0x7FF78B903120` | **UNKNOWN for all134 entries** |

Parsing the minidump's captured memory descriptors finds only two EXE-image slices: `0x7FF78971B11D+0x100` and `0x7FF78A163000+0x85240` (image RVA `0xE23000..0xEA8240`). None covers these globals. Host arrays cannot be replaced by guest RAM or initialized EXE/BSS zeros. A debugger may reconstruct read-only constants from the matching EXE, but cannot recover dynamic hits from pages that were never captured. Scratch parser/source extracts remain external in `%TEMP%/aliasjob`.

Consequently the only **observed hit** that can be listed is:

| Alias VA | Owner VA | Count evidence | Table method? | Fade/presentation path? |
|---|---|---|---|---|
| `0x0014FEF0` | `0x00150231` | **>=1** from first-sight log; exact frozen count unavailable | **Yes**, render-scene vtable `0x1E0F00`, slot `+0x148`, dword at `0x1E1048` | **Yes**, per-frame scene-state calls in `0x13A80`; not the fade producer or done reader |

No other alias is claimed to have zero hits; the requested complete nonzero list is unavailable within the no-new-run boundary.

### Own-byte / table evidence and proposed L02 repair

The original pointer table contains distinct `.text` entries: `0x14FDE0` at +0x144, **`0x14FEF0` at +0x148**, `0x14FE30` at +0x14C, followed by `0x150130`, `0x150170`, `0x1501F0`, `0x150240`, `0x150280`. This is a function-pointer table, not a switch-label table. The current recovery manifest has entries for the six siblings `0x14FE30`, `0x14FE50`, `0x14FE60`, `0x150130`, `0x150170`, `0x1501F0`, but **no start entry at `0x14FEF0`**. The analysis database labels it `tail_jump_alias`, end `0x150231`; that inferred span crosses other real functions and is not an acceptable recovery boundary.

Own original bytes instead establish:

- Body **`[0x14FEF0, 0x150104)`**. `cmp edx,9; ja 0x150075` handles the pointer-argument path; otherwise `jmp [edx*4+0x150104]` selects ten cases. Every path returns with **`ret 0xC`** (this plus two arguments), makes no calls and contains no loops.
- Own jump table **`[0x150104, 0x15012C)`**, ten dwords: `14FF10 14FF3C 14FF68 14FF72 14FF81 14FF8B 14FFC3 14FFD5 15001A 15002C`. All targets lie inside the body. Four NOPs precede the next function at `0x150130`. Stop recovery at **`0x150104`**, excluding the table data, as with prior L02 switch recoveries; do not use the alias owner as the end.
- Original writes per-stage texture/combiner state in `0x19DF10..0x19DF2C + (stage<<7)` and dirty flags at `0x19DED8`, returning S_OK. The alias owner **`0x150231` is only the shared E_INVALIDARG tail** of neighbouring `0x1501F0`: `eax=0x80070057; ret 0xC`. It omits these stores while keeping the stack balanced.
- Per-frame scene calls at `0x13DDC` and `0x13F03` use `[scene-vtable+0x148]`; subsequent instructions do not branch on the returned HRESULT. This locates the observed alias on the presentation path, but does not show it bypasses `0x123E0/0x11070` or the armed subsystem-6 update.

**Proposed fix — not applied:** add one reviewed routine recovery to `config/recovered-functions.json`: start `0x0014FEF0`, end `0x00150104`, section `.text`, kind `routine`, stack_args **12**, with the above table/own-byte evidence under existing **L02**. Generate its own body/dispatch using the existing recovery process, not a stub or synthetic completion. This is the same repair class as `0x37550`, `0x26780`, `0x7E360`, `0x24700`.

Future acceptance must verify ten in-span switch targets plus the `ja` path, no unresolved indirect jump, return cleanup (`ESP+16` including return address), recovered lookup priority over the alias, and expected per-stage stores for known inputs. The alias line disappearing alone is insufficient to establish repair or SEGA departure. No manifest, source, L02 count or runtime changed here.

### Causal judgement and stop

One quick Advisor consult for this follow-up: `c11bbc0a-6897-4d25-8315-801b3e8ab313`, `claude/claude-opus-5-5` high, original bytes/source read offline. **Observed defect: real render-state method replaced by an error tail. Inference: likely rendering-fidelity damage, not direct fade-update starvation.** The original leaf has no task/scene-list writes, loops or calls; its alias has balanced cleanup. Sampled callers ignore HRESULT. Worker also found 50 raw byte-pattern candidate +0x148 call sites (broader than Advisor's 45 lifted sites); immediate next-instruction classification found no test/cmp of EAX, but included three branches and one ambiguous case. **This does not prove whole-path HRESULT non-use**: immediate-instruction scans are not dataflow or decoded-CFG analysis. Other callers and indirect effects remain unverified, so no global impossibility claim is made. Parent independently disassembled the original switch: register **EDX**, default **0x150075**; rejected worker's raw-scan misdecodes EAX/0x150071. Independent address addition also corrected the initial host-VA arithmetic for count/hits to the values in the table above.

The earlier missing-update hypothesis is still unproven: no post-arm `0x24700` call trace was recovered. This alias is a concrete byte/table-backed repair candidate on the presentation path, **not a demonstrated fix for fade done0 / SEGA hold**. No APU trace, new dump, bypass, title claim or strict-horizon movement. Complete counter census is blocked by the existing dump's missing pages; stop rather than violate no-new-runs. Broader goal remains paused.

Push receipts for this records-only follow-up: `PUSHED_TO: origin / BRANCH: main / COMMIT: 929856fcfc145036252510509abaa0782d910a22 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: up-to-date (toolkit first)`; game `PUSHED_TO: origin / BRANCH: master / COMMIT: resulting records commit (final response) / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: records validated; fast-forward push receipt in final response`. Validation: `just check` all checkers passed; `git diff --check` clean; two records-only staged blobs, secret audit0 hits; no `game/` paths or large blobs. Toolkit clean/up-to-date first; no xemu process. Fresh turn-end review requested after game push; its result is reported in final response.

## Owner L02 recovery and one-run check (2026-10-02)

New explicit owner authorization supersedes the preceding read-only boundary for this bounded chore: pull game `d385e37` first, recover `0x0014FEF0..0x00150104` with stack_args12, regenerate recovery outputs, `just build`, `just test`, `just check`; xemu fade-producer breakpoint; exactly one600 s D2 exploratory run with APU_TRAP1; records and toolkit-first push, then STOP. No observer retry, APU-trace edit, fade bypass or broader-goal resumption. Pull fast-forwarded `889b695` to `d385e37`; toolkit remains `929856fcfc145036252510509abaa0782d910a22`. New collector includes executable data segments; symbol reader locates globals via archived map and actual module base, not stale host addresses.

### Recovery generation and validation

Reviewed worker `e56a8fcd-b0e9-48dc-a0e3-684cbfa16f2a`, `workbuddy-ai/deepseek-v4.1-flash` max, adds the one recovery entry and normal manual-exclusion entry. Parent independently read the generated body: ten in-span switch destinations; unsigned `ja` default `150075`; normal original-table paths use local gotos. A defensive `RECOMP_ITAIL` remains for a corrupted/unexpected table dword, so **no unresolved indirect jump on the normal original-table path**, not literally no fallback. All exits restore ESI and advance entryESP16; wrapper checks ESP/EBX/ESI/EDI. For RECOMP_ICALL-family dispatch, manual lookup returns recovered before generated alias lookup (not a claim about every toolkit thread/APC dispatch path). Existing134-entry alias map may still contain the pair, but dispatch priority should make its hitcounter zero.

First ordered validation: `just build` exit0, `just test` **31/32, exit1**, `just check` exit1, both failing only `jsrf_generation_provenance`: manifest inputs and the preserved generated-tree baseline naturally mismatch after the authorized recovery generation. This is a failed gate, not accepted PASS or a missing raw log: scratch output [test failure](../../logs/workers/owner-l02-recovery/first-test-failure.txt#L114-L176), [check failure](../../logs/workers/owner-l02-recovery/first-check-failure.txt). Session authorizes narrowly updating the two existing provenance records to measured recovery input/output hashes with an amendment; original full-generation identity/history must remain preserved. No full regeneration or checker weakening is authorized. No port launch until fresh ordered validation passes. Pre-launch `just disk`: PASS, 217.41 GB free,15 GB floor,13.42 GB allocated runs; no deletion. Read-only analyzer control on the prior mapping-verified dump reproduces SEGAphase2/hold121 and fadealpha0/target1/done0; old host globals are explicitly unavailable. The new capture will be mapping-checked before any XBE-backed interpretation. An existing ignored passive BMP copier (offline self-test PASS) preserves stable window-dump frames from the sole runner launch without changing the guest environment or adding a guest run.

External scratch behavior fixture extracts the exact generated body/wrapper, uses Xbox register/stack memory, and checks independently expected stores: modes0..9 × stages0/1, existing dirty-bit OR preservation for all10, two pointer-default variants and callee-saved registers/returnaddress. [Raw behavior log](../../logs/workers/owner-l02-recovery/behavior-harness.txt#L8-L46): **ALL CASES PASS**, S_OK/ESP+16; first-return ABI message appears as PowerShell NativeCommandError stderr formatting, not an ABI failure. Fixture is not a guest port launch and is not tracked retail code.

Fresh ordered gate logs: [build success](../../logs/workers/owner-l02-recovery/final-build.txt#L8-L16), [test32/32](../../logs/workers/owner-l02-recovery/final-test.txt#L84), [all checkers passed](../../logs/workers/owner-l02-recovery/final-check.txt#L8-L46). Parent independently read all three and ran provenance `--check` (True). Current executable SHA-256 `e29986c406fa6129f411d5f16ac783867b117db1b5409f1689da9c85175c0fb3`; map resolves `sub_0014FEF0` to recovered.obj. Worker explicit receipt confirms build→test→check each exit0 on frozen recovered.c `a469141f607252bf3da4f8b1b3c9c4790a550524e902f23ae4c4efdab6f30c6c`. The trailing garbled exit line in scratch logs is UTF16-appended `exit=0`, not a guest/checker error. No checker was weakened. Reviewer independently accepts the code while requiring provenance-prose correction: one neighboring `14FE60` body gains EBP context and an unreachable fallthrough after both unconditional returns. Measured new-entry marker deltas are ABI_CALL0, CONTINUE+3, g_seh_ebp+4; prior absolute ABI/CONTINUE counts were already stale. Baseline control reproduces the old tree exactly; this is not a generator-version change or full translation. All linked `logs/` artifacts here are ignored **local-only evidence**, intentionally unavailable in the public repository.

Sole authorized runner started after that green receipt. A `.ps1` invocation was rejected by host execution policy **before any script, runner or guest executed**; its output was collected. The same inline setup then launches the sole600 s guest. This is not a second experiment or runtime retry. No rebuild while the run is active; records-only checker confirmation is separate from the already ordered code gates. Worker corrected both provenance narratives and input byte sizes; asserts prose correction did not move any hash field or generated output. [Post-prose `just check`](../../logs/workers/owner-l02-recovery/postprose-check.txt) exit0, standalone generation-provenance check True. Fresh Reviewer `a641817e-1319-4406-9418-72a3daf51d37`, `claude/claude-opus-5-5` medium, independently disassembled the whole original body, read table/vtable, checked wrapper/lookup and old baseline replication; preliminary code acceptable, final closure remains CONTINUE pending capture/results/records/push receipts.

An in-progress profile check before runner finalization returned UNKNOWN solely because save-root verification is written at completion. Final reclassification is **EXPLORATORY** (GPU_ACK absent/default enabled); checker exit1 is intentional for every non-strict classification, not a failed archive-identity result.

### Sole 600-second capture: result and limitations

Run `20261002-174731-263-owner-l02-d2-600`, requested600s, started `2026-10-03T00:47:31.990861Z`, duration602.768215s, stop `2026-10-03T00:57:34.759076Z`. [Result](../../logs/runs/20261002-174731-263-owner-l02-d2-600/result.json): diagnostic_deadline/exit3, dump_ok true,21 native threads,191 named frames,1 GPU snapshot,0 dropped,save-root verified,checkpoints passed,no missing checkpoints,gpu_report_ok true. This is bounded capture, not proof of liveness. Exactly one matching run directory exists; the script-policy rejection created none. Archived exeSHA `e29986c406fa6129f411d5f16ac783867b117db1b5409f1689da9c85175c0fb3` matches green build; collectorSHA `2d8c62a441df1638d16a103fd20e06c2d09604221dca95107629e4a8531d688b`. Same effective settings as prior D2 run except new log/dump destination; no GPU_ACK override, fresh disposable root/no seed. Mapping gate **1 match/0 mismatch/0 unreadable/0 missing**, control11000 exact.

**Recovered execution observed:** [log2644](../../logs/runs/20261002-174731-263-owner-l02-d2-600/jsrf_run.log#L2644) `[RECOVERED] 0x0014FEF0 returned; ABI verified (ESP/EBX/ESI/EDI)`; no ABI FAILURE or ALIAS-ICALL log lines found. This first-return line plus static lookup/store fixture supports recovered dispatch, not a cumulative alias-hit measurement.

**Required tool failed; equivalent cumulative census succeeds:** `python -X utf8 scripts/read-host-symbol.py <run> --alias-hits` exit1: `host memory 0x00007FF76248CCD0 is absent from this dump`. [Raw failure](../../logs/workers/owner-l02-recovery/alias-census-failure.txt). Reviewer traced the failure to **const entry count/map in .rdata**, not mutable counters. MiniDumpWithDataSegs captures writable .data, not .rdata. Modulebase7FF761720000; countRVA D6CCD0/mapD6CCE0 missing, while .data `[E24000,E24000+27BE330)` is fully present.

Reviewer and parent independently read all134 mutable64-bit hits from frozen .data using archived linker-map relocation: **all0, cumulative count0, printed0, no seen flag**. Static134-entry map/count read from **archived SHA-identical executable** (a stated provenance substitution, not invented runtime state): **index111 retains14FEF0→150231, hits[111]=0**. Thus14FEF0 absent from positive hits, expected result met. [Equivalent census](../../logs/workers/owner-l02-recovery/equivalent-alias-census.txt), [archived PE map](../../logs/workers/owner-l02-recovery/archived-pe-map.txt), [live .data control](../../logs/workers/owner-l02-recovery/host-live-control.txt): frameflip3974, memorybase/offset10000, so not a zero-filled capture. These counters cover process-lifetime RECOMP_ICALL-family alias dispatch at capture, not kernel_bridge direct thread/APC/DPC paths. First-return recovered log2644 agrees but is not substituted for the census. TLS ICALL ring remains unreadable/UNKNOWN separately. Known tool defect: fixed .rdata tables must be read from the verified archived executable or explicitly captured; **follow-up only**, no collector fix/new guest authorized in this chore.

**Mapping-verified final guest state:** app1063A70,root108FF40,modes0/0/0/0; logo143EE60 vt1CCFB8 phase2 hold121; subsystem6 fade15F0E60 vt1C4D10 alpha0,target1,step0.008333333767950535,done0. Same unresolved phase2 state as prior D2 control. [State/history analysis](../../logs/workers/owner-l02-recovery/final-analysis.json#L96-L153).

**Frozen histories:** five registered guest threads;128-event retained capacity each. Overwritten counts262092/0/3301/3399/0; incomplete0. No retained match for kind1 target24700/site1108A, the listed oracle caller targets, or14FEF0. This says nothing about their absence across600s: bounded tails have overwritten older events; direct recursion is not ICALL; no event ECX or distinct recovered-entry kind. Oracle object identity therefore not established in the port.

**Screen did not observably leave SEGA:** passive copier retained59 valid window BMPs,00:47:45.227145→00:57:28.732297Z,583.505152s span, all identicalSHA `9e7541a8abc68b31faa3f4553867dcd70a7364eb5fc35b60b7055344e5ee7401`. Parent inspected first/intermediate/final lossless PNG views: Presented by SEGA. [Frame census](../../logs/workers/owner-l02-recovery/frame-summary.json), [final image](../../logs/workers/owner-l02-d2-600-window/w0058.png). This is a claim about retained samples, not proof of every frame; no title or strict-horizon advancement.

Ledger IDs **L14–L18,L20–L25,L39,L40**: L16 configured/inert under owner,L19 dormant,L21–L24 enabled byAPU_TRAP1,L25 alwaysactive. Supporting compatibility IDs **L02–L12** (L06 limits), **L25–L26,L29–L31,L33,L35–L38** retain their existing qualifications. L02 genuine render-state method restored, **not a fade-cause repair or fidelity claim**. Broader goal paused; no bypass, observer retry, APU trace or extra port experiment. Final records `just check` and `git diff --check` PASS; exactly one owner-l02-d2-600 run directory verified. Fresh Reviewer independently accepts **code and final records for commit**, confirms counters/state/history/oracle/profile/mapping/encoding and scope. Closure conditioned only on clean committed checks/outgoing audit and real push receipts; no remaining code/run acceptance blocker.

Toolkit receipt: **PUSHED_TO: origin / BRANCH: main / COMMIT: 929856fcfc145036252510509abaa0782d910a22 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: Everything up-to-date**. Clean unchanged toolkit and equal origin/main verified; pushed first. Game receipt: **PUSHED_TO: origin / BRANCH: master / COMMIT: 5c1d7d6664c5aab16c6e19425285cb0543428a3b / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward d385e37→5c1d7d6, success**. Committed-tree `just check` PASS, clean intended tree, FF ancestry confirmed;8 outgoing blobs,0 secret hits,max2,803,546bytes,no game/,logs/,src/recomp/ or unrelated MUSE path. Both repositories pushed toolkit first. Records-only receipt closure follows this code commit; no further runtime/code work. **Owner chore COMPLETE; STOP**, broader goal remains paused.

### Xemu armed fade caller evidence

Three bounded disposable snapshot oracle sessions: first6hits proved idle (`alpha=target=0, done1`) and were rejected as armed evidence; second filtered6armedhits; third recorded logo identity/phase to verify SEGA. No port600run was consumed by these oracle sessions. Xemu exited0 each time; no replacement server or original asset upload. HDD protected by `-snapshot`; EEPROM preservation was not independently verified, so no stronger all-asset-preservation claim is made.

Final six entry-breakpoint hits16–21 at `0x24700`: fade `this=0x600E60`, vt `0x1C4D10`; SEGA logo `0x515030`, vt `0x1CCFB8`, **phase0/hold0**. Alpha `1.0, .991666675, .983333349, .975000024, .966666698, .958333373`, target0, step1/120, done0. These prove the **opening fade-in producer**, not the phase2 fade-out that was stuck in the prior port capture. Raw local evidence: [six oracle hits](../../logs/workers/owner-l02-recovery/oracle-armed-hits.json), [launch record](../../logs/workers/owner-l02-recovery/oracle-launch.json).

Manual guest unwind validated against original call instructions and pushes, not an EBP-chain or arbitrary stack-word census (entry ESP `D0044D24`; EBP1 is `13A80`'s constant):

| Entry ESP offset | Return | Calling instruction / frame |
|---|---|---|
| +00 | `1108A` | `11087 call [eax+4]` in inner `11070`, fade node |
| +08 | `11096` | `11091 call11070` in middle walker; saved ESI at+04 `38FFA0` |
| +10 | `11096` | outer recursive walker; saved ESI at+0C `38FF40` |
| +18 | `124C3` | `124BE call11070` in `123E0`; saved appESI at+14 `363A70` |
| +30 | `13B24` | `13B1F call123E0` in `13A80`; `123E0` savedESI+1C and16-byte locals+20..2C |
| +40 | `13F9E` | `13F99 call13A80` in frame loop `13F80`; saved ESI/EBP/EBX+34..3C |
| +48 | `6FA41` | `6FA3C call13F80`; savedESI+44 |

Only walker `11087` is indirect; preceding chain/recursion is direct and need not appear in ICALL histories. Node+4 negative skips its update and subtree; xemu fade+4 `00010003` is nonnegative. Saved ESI identifies descent context, not necessarily first-child ordering. One quick Advisor `b4b18588-1659-4b43-ad3f-d25bc9935928`, `claude/claude-opus-5-5` high, independently read sixhits/helper and original bytes; agreed with manualchain and phase0 limits.

For frozen port histories the matching key is **kind1 target `00024700`, site `0001108A`**: the macro records the pushed return address, not instruction `11087`. Per-event ECX is absent, so a positive match cannot identify a particular fade object; a negative128-entry tail cannot establish never-ran across600 s. Report overwritten/incomplete counts. Game registry owns128events/thread; separate TLS ICALL ring is16targets. Frozen fade state is a point-in-time observation, not a retrospective object-specific call trace. There is **no distinct recovered-entry event kind**: generated/recovered callers using `RECOMP_ICALL` publish kind1 before recovered/manual lookup, while recovered wrappers only print first-return ABI confirmation. Thus inspect the frozen shared histories for recovered oracle-chain targets, but do not invent a separate recovered trace or expect the direct recursion chain to be present.

## Owner-directed discovery: what should update the armed fade (2026-10-03)

Owner-directed discovery under `docs/agent-workflow.md` §0.6. **Records only, no fixes, no port run.** Both repositories pulled first (toolkit then game); both clean at `main` `929856f` / `master` `371b062`. Broader goal stays paused; the F5 observer stays parked. One quick Advisor consult (instrument choice only), recorded below.

### 1. xemu T3 — the phase-2 chain is the phase-0 chain

Three read-only gdbstub sessions were **not** needed; one bounded session answered it. Launch: `-snapshot -S -qmp tcp:127.0.0.1:4444`, assets used in place from the owner's configuration, **no write packet ever sent** (`?`, `g`, `m`, `Z0`/`z0`, `s`, `c`, `D` only). `xemu exit 0`; no replacement server; no port run consumed.

Breakpoint `0x24700`, filtered on the SEGA logo object `0x515030` (vtable `0x1CCFB8`) and its phase at `+0x98`. 273 iterations, 22.073 s: **126 phase-0 hits** and **10 phase-2 hits** (budget reached).

| Phase-2 hit | ECX (fade) | vtable | flags | alpha `+0x98` | target `+0xA8` | done `+0xC0` | logo phase / hold |
|---|---|---|---|---|---|---|---|
| 263 (first) | `0x00600E60` | `0x1C4D10` | `0x00010003` | **0.0** | **1.0** | **0** | **2 / 121** |
| 264 | `0x00600E60` | `0x1C4D10` | `0x00010003` | 0.008333 | 1.0 | 0 | 2 / 121 |
| 265–272 | `0x00600E60` | `0x1C4D10` | `0x00010003` | +1/120 each | 1.0 | 0 | 2 / 121 |

**The first phase-2 hit reproduces the port's stuck state exactly** — alpha 0, target 1, done 0, logo phase 2, hold 121 — and the very next sampled hit has alpha `1/120`. **Scoped to the sample:** this shows the updater was *entered* in phase 2 and that alpha advanced by `1/120` by the next sampled entry. It does **not** show the update ran on the phase-transition activation itself (the first sampled entry is already phase 2 / done 0, so the transition is not in the sample), and it does not show every subsequent frame was updated — the sample is ten breakpoint entries, not a contiguous frame history.

**The return-address chain is byte-for-byte the recorded phase-0 chain.** Every one of the 10 phase-2 hits and the 6 recorded phase-0 hits carries the same stack head:

```text
+00 0x0001108A  +08 0x00011096  +10 0x00011096  +18 0x000124C3
+30 0x00013B24  +40 0x00013F9E  +48 0x0006FA41
```

ECX is `0x00600E60` in **both** phases, and the fade's links are identical (`child+0x28 = 0`, `sibling+0x30 = 0x00600F40`). Within the sampled interval — the logo's phase-0 fade-in and the 10 phase-2 hits at the budget — **every observed hit uses the same node and the same call chain**, so no *second* producer or fade object was **observed**; the sample cannot exclude one that was never hit before the budget ran out, and it is not claimed to. The plan's open question — "the caller chain of `0x24700` during the phase-2 fade" — is answered for what was sampled: it is the chain already recorded.

Artifacts (local, outside git): `%TEMP%/owner-sega-phase2/{phase2-hits.json, phase0-hits.json, all-hits-summary.json, launch.json, oracle-phase2.py}`. `phase2-hits.json` sha256 `fb8e760d1ebd0b16d9997b78bcb9162eab0bb94d5f5b9a2406c22c97739517b2`; `phase0-hits.json` sha256 `5ae852534ca66079470e98c4dc3594c194a63419f4feb432066dc3c35df2c11a`.

### 2. Port, read-only on `20261002-174731-263-owner-l02-d2-600` — the capture cannot answer the object-specific half

Mapping gate first: `check-dump-mapping.py` → `matches 1, content-mismatch 0, unreadable 0, missing 0`, control `0x11000` exact. Every port value below is read at its actual guest VA from that dump.

**The fade instance is present in the list the update walks.** Walking `[0x22FCE0]` (= app `0x1063A70`) `+0x87DC` (= root `0x108FF40`), then `+0x28` (child) then `+0x30` (sibling):

```text
0x0108FF40 vtable 0x001C4480  child 0x01340060
  0x01340060 vtable 0x001C4F68  sibling 0x0108FFA0
  0x0108FFA0 vtable 0x001CCEB8  child 0x015F0E60  sibling 0x01341990
    0x015F0E60 vtable 0x001C4D10  flags 0x00010003  update[+4] = 0x00024700   <-- the armed fade
    0x015F0F40 vtable 0x001C4580  child 0x0161B5E0
    0x015F2D40 vtable 0x001CB0A8
    0x0143EE60 vtable 0x001CCFB8  flags 0x00000009  update[+4] = 0x0007E360   <-- the SEGA logo
```

The port tree corresponds to the oracle's node for node over the same span — `0x108FF40 → 0x1340060 → 0x108FFA0 → {0x15F0E60, 0x15F0F40 → 0x161B5E0, 0x15F2D40, 0x143EE60}` against the oracle's `0x38FF40 → 0x430060 → 0x38FFA0 → {0x600E60, 0x600F40 → 0x62B5E0, 0x602D40, 0x515030}`, with the same vtables, flags and update slots at every position. The armed fade is `0x15F0E60` (`alpha 0.0`, `target 1.0`, `step 0.008333333767950535`, `done 0`, flags `0x00010003` — the same flags xemu shows), and `app+0xB0` — the subsystem-6 slot that `0x24650` reads — is **the same object**. Instance identity is therefore established, not assumed. The node's `+4` flags are non-negative, so `0x11077`'s `js` does not skip it.

One difference, recorded rather than smoothed over: the oracle's `sega-ram.bin` snapshot has **one extra sibling** after the logo (`0x515030`'s `+0x30` = `0x52EE60`, vtable `0x001CCF78`), which the port's logo node does not have (`+0x30` = `0`). That node is a later-scene object: in the oracle's two successor snapshots its vtable has become `0x001C4390` with flags `0x80000000` (negative, so the walker skips it), and the logo's own `+0x30` is then `0`. It is appended after the SEGA segment, not before it, and it is **not** on the path that updates the fade — the fade is reached through `0x38FFA0`'s child, not through the logo's sibling. It does not affect the reachability finding above, and it is not claimed as a defect.

**Where the fade sits in the walk's order** — replaying `0x11070`'s preorder (update node → recurse child `+0x28` → sibling `+0x30`) over the frozen port tree gives:

| # | Node | flags | update `[[+0]]+4` | |
|---|---|---|---|---|
| 1 | `0x0108FF40` | `0x00000000` | `0x00011C90` | |
| 2 | `0x01340060` | `0x00000000` | `0x00025310` | |
| 3 | `0x0108FFA0` | `0x00000000` | `0x0007BDD0` | |
| 4 | `0x015F0E60` | `0x00010003` | `0x00024700` | **← the armed fade** |
| 5 | `0x015F0F40` | `0x00010003` | `0x00042880` | |
| 6 | `0x0161B5E0` | `0x40000100` | `0x00011C90` | |
| 7 | `0x015F2D40` | `0x00010003` | `0x00066440` | |
| 8 | `0x0143EE60` | `0x00000009` | `0x0007E360` | **← the SEGA logo** |
| 9 | `0x01341990` | `0x00000000` | `0x00118610` | |
| 10 | `0x01345CC0` | `0x00000000` | `0x00116E30` | |

The fade is visited **4th** and the logo **8th**, in the same traversal — which is what makes the arming order below load-bearing.

**A second structural finding: `0x123E0` selects exactly one of five traversals per activation, and all five can reach the fade update.** The routing is a **priority chain**, not five calls: each test that succeeds calls its traversal and then `jmp 0x124C3`, so exactly one traversal runs per activation — precedence `app+0x40` → `0x114D0`, else `+0x44` → `0x112A0`, else `+0x48` → `0x11700`, else `+0x4C` → `0x11930`, else the default `0x11070`. Each traversal invokes a different slot on every node it visits:

| Traversal | Slot invoked | Selected when (in this order) |
|---|---|---|
| `0x114D0` | `[[node]+0x1C]` | `app+0x40 != 0` |
| `0x112A0` | `[[node]+0x10]` | else `app+0x44 != 0` |
| `0x11700` | `[[node]+0x28]` | else `app+0x48 != 0` |
| `0x11930` | `[[node]+0x34]` | else `app+0x4C != 0` |
| `0x11070` | `[[node]+0x04]` | else — all four are zero |

All five share the same shape — null check, `test [node+4]` / `js` skip, an optional `0x1BA8A0` call, the indirect call, then child `+0x28` and sibling `+0x30` recursion — and the four alternatives reach the fade update too, because the fade's `+0x10`, `+0x1C`, `+0x28` and `+0x34` slots all point at `0x000AECC0`, which is `mov eax, [ecx]; jmp [eax+4]` — a **thunk back to the very same `+4` update** (`0x24700`). The logo's four alternative slots instead point at `0x00011C90`, which is a bare `ret` (its real body is the separate `0x11CA0` entry).

So `0x24700` is reachable through **five** routes, one per activation, and on the fade all five land on the same body. The oracle agrees byte for byte: in `sega-ram.bin` the fade `0x00600E60` (vtable `0x001C4D10`) has `+0x10`/`+0x1C`/`+0x28`/`+0x34` = `0x000AECC0` with bytes `8B 01 FF 60 04 90` (`mov eax,[ecx]; jmp [eax+4]`), the logo `0x00515030` has all four = `0x00011C90` with bytes `C3 90 90 90 90 90` (`ret`), and the routing in `0x123E0` is the same priority chain. So this is guest structure, not a port artifact.

**What this does and does not buy.** It means a run that fails to show the `0x11070` chain has **not** shown the fade was unreached: a mode flag routes the update through one of the other four routes and still calls `0x24700`. It does **not** explain the observed state: mode selection is mutually exclusive, all five reach the fade, and nothing in the tree or flags changed — so "a different mode ran" is not an account of why alpha stayed 0. It is **not** claimed as the defect; nothing here shows which traversal ran, and the `0x1108A` chain xemu recorded is the `0x11070` one.

**But nothing in this capture records whether `0x24700` ran during the hold, and the reason is structural, not statistical:**

- `src/recomp/recovered/recovered.c:15365` logs on **first call only** (`if (!logged++)`). The single `[RECOVERED] 0x00024700 returned` line is at [log 6660](../../logs/runs/20261002-174731-263-owner-l02-d2-600/jsrf_run.log#L6660) — early in the run, ~136,000 lines **before** the cache marker at [log 141999](../../logs/runs/20261002-174731-263-owner-l02-d2-600/jsrf_run.log#L141999). A later call cannot print.
- The walker's only indirect call, at guest `0x11087`, is lifted to `RECOMP_ICALL_SAFE_AT` ([recomp_0000.c:121](src/recomp/gen/recomp_0000.c#L121)). That macro does **not** contain `RECOMP_DIAG_CALL` — only `RECOMP_ICALL`, `RECOMP_ICALL_SAFE` and `RECOMP_ITAIL` do. So this call site publishes **no** guest event, in any run, by construction.
- The frozen 128-event tails contain only kernel-bridge events (targets `001BA050`, `001916C0`, …). The absence the earlier session reported was therefore **guaranteed by the instrument**, not evidence about the guest.
- The TLS ICALL ring is 16 targets and is not in the dump.

So **step 2's answer is: the capture answers the reachability half and cannot answer the object-specific half.** Positively captured: `0x13A80`, `0x13F80` and `0x6F9E0` are on the main thread's chain, and the armed fade is present in the list the walk traverses. Not answerable from this archive: whether `0x24700` was invoked **on this object** during the hold — the two records that would show it are structurally incapable of it (below), and no amount of re-reading changes that.

**What the capture does say, and it sharpens the question.** The frozen main thread's native stack ([stacks.txt:50977–50979](../../logs/runs/20261002-174731-263-owner-l02-d2-600/stacks.txt#L50977-L50979)) is

```text
sub_0014D090+0x117  ->  sub_00013A80+0x38D2 (recomp_0000.c:8175)  ->  sub_00013F80+0xB5  ->  sub_0006F9E0+0x1CD
```

and `recomp_0000.c:8175` is the `0x13F2A` indirect call to `0x14D090`. Two static facts make that frame load-bearing:

- `0x13A80` reaches `0x13B1F` (`call 0x123E0`) **unconditionally**: between `0x13A80` and `0x13B1F` there is no `ret`, and the only `jmp` is `0x13A9B jmp 0x13AA2`; every other branch target in that range is inside it.
- `0x123E0` reaches the **default** traversal (`0x124B8 → 0x11070`) only when all four of `app+0x40`, `+0x44`, `+0x48`, `+0x4C` are zero; otherwise it takes one of the four mode routes above. All four read **0** in the frozen dump. (An earlier draft said the other branches "skip the walk" — they do not; they call a different traversal that also reaches the fade update. Corrected above.)

**One caveat on that second point, and how far it goes.** Those four fields are **recomputed at the top of every activation** of `0x13A80` — `0x13A85`–`0x13AFA` derives each from `app+0x50..+0x6C`, and `0x13AFD` then clears `+0x50`. So a frozen reading describes the capture frame's own decision and is not a standing property of the run. For **this** frame, however, the reading does decide: in the lifted body, the `0x123E0` call is at `recomp_0000.c:7652` and the frozen `0x13F2A` Present call is at `:8175`, and there is **no** write to `app+0x40`, `+0x44`, `+0x48` or `+0x4C` anywhere between them. So the frame that is on the stack at capture is one whose `0x123E0` saw all four flags zero and therefore took the `0x124B8 → 0x11070` walk path. What the frozen zeros do **not** license is any claim about *other* activations in the run — an earlier draft used them that way, and that step is withdrawn.

So that frame **does** establish that the default traversal ran in that activation, and it establishes the chain `0x13A80 → 0x13F80 → 0x6F9E0` positively. What it does **not** establish is that this frame's traversal updated the armed fade — and the reason is ordering, not luck:

- The traversal is **preorder**: `0x11087` calls the node's update, `0x1108A` takes the child `+0x28`, `0x11096` the sibling `+0x30`. The fade `0x15F0E60` is a **child of `0x108FFA0`**, while the logo `0x143EE60` is a **later sibling of `0x15F0F40`** — so the traversal reaches the fade *before* the logo, every time.
- On the arming tick the fade's `done` is still `1`. `0x24700` then returns at `0x24703` (`jne 0x24962`, and `0x24962` is `pop esi; ret`) **without touching `+0x98`**. (`done = 1` has **three** possible sources — the constructor at `0x246C1`, the completion store at `0x2494A` and the immediate-set path at `0x24493` — so this step does not claim which one set it; see the `step` lead below.)
- The logo arms it further along the same traversal: `0x7E409`–`0x7E424` (hold → 121, phase++) then `0x7E4AF` → `0x24620` → `0x24540`, which writes `done = 0` at `0x24553` and the target at `0x24578`, and **never writes `+0x98`**.

The frozen state — alpha 0, target 1.0, done 0, logo phase 2, hold 121 — is **exactly what the arming frame leaves behind**, so the arming-frame account is *compatible* with everything observed and needs no extra mechanism. Quick Advisor consult 2 (`c548070d-9cc1-4340-b3b0-d56d88e2f0cc`) ruled the earlier contradiction reading unsound on this basis; the Session reproduced each step above from the original bytes.

**How far that identification goes — and it does not go all the way.** It is *not* established that the captured activation **is** the arming tick. Phase 2 does not increment the hold (`0x7E444` with `done == 0` falls to `0x7E46D`, which only touches `+0xA0`), so `phase 2 / hold 121 / alpha 0 / done 0` is the state of the arming frame **and of every later frame that also failed to update the fade**. The endpoint snapshot is shared by all of them and cannot tell them apart. What the ordering shows is that the frozen frame is *consistent with* being the arming frame — not that it is one. An earlier draft asserted the identification outright; that is withdrawn.

**One new lead from the phase-1 dump, recorded with its alternative.** The fade carries a `step` field (`+0xB8`) written by two different paths: the constructor writes `1/60` (`0x3C888889`, `0x246CB`), while both arm paths write `1/120` (`0x3C088889`, pushed at `0x7E4AF`/`0x7E460` and stored at `0x245E4`). In `20260930-225440-580-f3-alias-fix-strict` the field reads **`0x3C088889` — the arm value** — so an arm ran after construction in that run, overwriting the constructor's `1/60`. That is evidence that the **arm path executes in the port**, which is what makes the "`0x24700` returns at `0x24703` while `done` is still 1" account applicable rather than hypothetical.

The same dump reads `done = 1`. That is **consistent with** the completion store at `0x2494A` — which requires `ecx == 4`, i.e. all four fade components reaching their targets, and would mean `0x24700` ran to completion in that run. But it is **not** decisive, and it is not claimed: `0x24480` (reached via `0x24600`) sets `done = 1` at `0x24493` and writes alpha **directly, without touching `step`**, so `done = 1` with an arm-value `step` is equally consistent with an immediate-set call having run instead of the completion store. The constructor sets `done = 1` too (`0x246C1`), so a bare `done = 1` never identifies its writer. Separating the candidate writers needs the alpha watch with its raw frame (below) or TTD. Recorded as a lead, not a finding.

**This lead also touches consult 3's control ruling, and has now been ruled on.** Consult 3's own `REVERSED BY` was "+0xB8 = `0x3C088889` (1/120) in the phase-1 dumps, which would mean the phase-0 arm did run and the alpha control stands" — and the dump reads exactly that value. The evidence was returned under §4.3 `PREMISE_CHANGED`, and **consult 4 ruled that the phase-0 arm is *not* the source**, on two grounds the Session then verified from the original bytes:

- **Target mismatch.** `0x24540` computes the target as `(arg >> 24) / 255` — `0x1C4CC8` is `0x3B808081` = `0.003921568859…` = 1/255. The `0x7E4AF` arm pushes `0xFF000000`, so it would set target **1.0**; the dump reads **0.0**.
- **Phase mismatch.** Phase 0 reaches `0x7E4AF` only through the skip path at `0x7E3E9`, which does `phase += 2` — leaving phase 2, not the phase 1 the dump shows.

What fits `target 0` with `step 1/120` is a fade **down** from 1.0 to 0, and the **logo constructor** at `0x7E752`–`0x7E762` is one instance: it calls `0x24600(0xFF000000)` (alpha 1.0, `done` 1) and then `0x24620(0, 1/120)`. It is not the only site with that shape — `0x7E460` reaches the same `(edi = 0, 1/120)` pair. (**Corrected:** an earlier draft also cited `0x6663C`–`0x66652` and `0x6683C`–`0x6684D` as pushing "the same pair". They do not: those sites push `0x3C888889` = **1/60**, not 1/120 — `0x6664B`/`0x66650` and `0x66846`/`0x6684B` — so they are not arm sites for the observed step.) And consult 4 **withdrew** the endpoint discriminator it had suggested here: the endpoint fits a completed `0x24700` fade-in exactly as well as an immediate set, so it separates nothing (see below).

**How strong the source argument is — narrowed on review, and the Advisor agreed.** The two mismatches above are **conditional**, not absolute. Consult 4's §4.5 narrowing: *"Target 0.0 and step 1/120 constrain only the most recent `0x24540` arm (target byte 0, step 1/120). They support an arm-compatible overwrite after construction, not its caller."* Another target-writing arm or a phase change could have intervened between a given call and the capture. The Session verified the candidate set directly:

- Exactly **three** direct call sites push `0x3C088889` before calling `0x24620`: `0x7E460` (ret `0x7E465`), `0x7E4B9` (ret `0x7E4BE`) and `0x7E762` (ret `0x7E767`). The **first and last** pair it with target 0 — `0x7E460` pushes `edi` and `edi = 0` from the function's own `xor` at `body_0007E360` line 82901, and the constructor's `0x7E762` pushes `edi = 0` (`xor edi, edi` at `0x7E6D5`). The **middle** one, `0x7E4B9`, pushes `0xFF000000` — target 1.0, not 0.
- `0x24540` has exactly **one** direct caller (`0x2463D`, ret `0x24642`) and no aligned pointer to it in the image.
- `recomp_0002.c:20963` is **not** a third arm site: it stores `1/120` into stack slots at `0xF14A7` without calling `0x24620` there.
- **Not excluded:** calls through computed pointers.

So the correct statement is the weaker one: **the `step` field shows a post-construction arm-compatible overwrite; it does not identify which caller performed it.** And consult 4 withdrew its implied discriminator outright: the endpoint (alpha 0, `done` 1, target 0, step 1/120) *"fits a completed `0x24700` fade-in exactly as well as an immediate `0x24600(0)`/`0x24480` set. My 'matches exactly' separated nothing."* `done = 1` with `alpha = 0` therefore remains open.

Consult 4's `REVERSED BY` conditions are recorded with the same care, and they are **compatible-with**, not proofs of, what they were offered to prove:

- A phase-1 dump with target 1.0 **and** step 1/120 *"would fit `0x7E4B9` as the last arm; it would not prove it."*
- More than one update per activation *"is shown only by counted `1/120` alpha steps from `0x24700` (`raw[esp+4] = 0x1108A`), e.g. a full 120-step 1→0 fade, outnumbering the `app+0x87E0` activations over the same interval."* Unspecified hits alone do not show it.

Consult 4's own `REVERSED BY` for these narrowings: a direct or indirect `0x24620`/`0x24540` caller outside the logo pushing target 0 (or `edi`) with step `0x3C088889` — which would widen the candidate set; or `app+0x87E0` being reset during the logo, which would void the activation arithmetic above.

**An earlier reading of this record is retracted.** It argued the frozen frame proved a live contradiction — that reaching `0x13F2A` implies the walk ran and must therefore have written alpha. That inference silently assumed the fade was *already armed* when the walk reached it in that frame; on the arming frame it is not. No contradiction is claimed.

**What the archive does narrow — and one thing an earlier draft claimed from it that it does not support.** The activation counter at `app+0x87E0` is loaded at `0x13F5C`, incremented at `0x13F68` and stored at `0x13F6A`, in `0x13A80`'s epilogue — after both the `0x13B1F` traversal call and the `0x13F2A` Present call, with no `ret` between entry and that store — so it counts completed activations of the frame function. It is **not** a build-specific constant: for build `7027fafad9cd7069` it reads 321, 1016 and 3532 at 62.6 s, 182.7 s and 603.2 s, and each of those runs *ends* at phase 2 / hold 121 / alpha 0 / done 0. So those runs completed **many more activations in total** than the 60 s one while sitting in the same terminal logo state.

What that does **not** license, and an earlier draft asserted: that thousands of activations happened *after* arming, or that the logo sat in phase 2 *continuously*. The snapshot is an endpoint, not an interval history — it cannot say when in each run the arming tick occurred, nor whether the counter advanced smoothly or the run spent its time elsewhere. The honest statement is the weaker one: **the completed-activation totals differ across runs of different lengths while the terminal state is identical**, so a "the loop stopped at the arming frame" account is not supported by these runs — but nothing here establishes what the loop was doing between arming and capture. No cause is claimed here.

### 3. Proposed bounded run — for owner approval, not executed

One exploratory run, **no code change and no rebuild**, using an instrument already listed observation-only in `docs/jsrf-run-profiles.md` (`RECOMP_WATCH`). Point it at the fade's **alpha** field by **literal VA**, with the raw frame enabled so the writer can be attributed:

```text
RECOMP_WATCH=0x15F0EF8
RECOMP_WATCH_RAW=1
```

Keep the D2 profile and ledger IDs of the latest run otherwise unchanged, with a fresh disposable save root. **Bound: `--seconds 150`.** That is a concrete exploratory cap chosen to contain the arming sequence — the logo reaches phase 1 / hold 61 by 14.6 s and phase 2 by ~47 s in the archived runs — but it is **not** established as the *smallest* sufficient bound, and it does **not** guarantee that the arm or a completion is observed inside it: the arming tick's position varies by run, and a value change that happens after the window closes is simply not seen. If the arm is seen but no completion follows, the honest next step is a longer bound, not a conclusion. The run is a diagnostic, not a horizon: `RECOMP_WATCHDOG_SECS` is out (`run-jsrf.py:326` strips it and it ends the process with `_exit(3)`), so the bound is the runner's own `--seconds`.

**Why alpha and not the `done` flag** (this reverses the previous draft's choice, on quick Advisor consult 4, narrowed in §4.5). **Alpha is preferred because `done` carries no per-step history.** `done` is a flag: `0x246C1` (constructor), `0x24493` (immediate set) and `0x2494A` (completion) set it, and `0x24553` (arm) clears it — so its value cannot distinguish alpha **stepping** by `1/120` from `0x24700` against alpha **jumping** to 0 through the immediate-set path at `0x24480`. Alpha records each `0x24700` step. The earlier `done` proposal is superseded.

**Why the literal VA and not a pointer chain.** `xbox_WatchInit` ([xbox_memory_layout.c:1699–1705](../../../xboxrecomp/src/kernel/xbox_memory_layout.c#L1699-L1705)) treats **every** leading `[` as a dereference of the root and then reads **one** trailing `+offset`; `[[0x22FCE0]+0xB0+0x98]` would therefore resolve to `mem[mem[0x22FCE0]]+0xB0` — the app's vtable in `.rdata`, a readable page nothing writes — and the watch would arm and stay silent. That is a false negative, not a measurement. `RECOMP_PEEK` has the same grammar. The literal VA is safe here because `app+0xB0 == 0x15F0E60` (so alpha is `0x15F0EF8`) in **all three** archived port dumps checked: `20261002-014152-190`, `20261002-132843-938`, `20261002-174731-263`. A future run must re-check that before reading the output.

**Writer attribution — this is what makes the run decisive, and it needs `RECOMP_WATCH_RAW=1`.** `watch_report` prints the raw frame as `raw[esp+0]`, `raw[esp+4]`, … ([xbox_memory_layout.c:1593–1601](../../../xboxrecomp/src/kernel/xbox_memory_layout.c#L1593-L1601)), and the two candidate writers have **disjoint** innermost slots:

| Store | Where it runs | `raw[esp+0]` | `raw[esp+4]` |
|---|---|---|---|
| `0x244B8` — sets alpha from the argument (constructor/`0x24600` path) | inside `0x24480`, **before any push** | `0x2461D` ([recomp_0000.c:41101](src/recomp/gen/recomp_0000.c#L41101)) | — |
| `0x24748` — the `1/120` add | inside `0x24700`, **after `push esi`** | saved `esi` = the node `0x15F0E60` | `0x1108A` ([recomp_0000.c:121](src/recomp/gen/recomp_0000.c#L121)) |

So a step of `+1/120` carrying a traversal return in `raw[esp+4]` is the traversal's update; a jump to 0 carrying `raw[esp+0] = 0x2461D` is the immediate setter. **Read those raw slots, not the filtered chain** — the filtered list prints code-looking words and can include stale values. A write through an alias view is still not trapped, which stays an open case.

**The traversal return is not always `0x1108A` — all five routes have their own, and the table lists them.** The node's update is reached through a *different* vtable slot and a different return address depending on which mode flag routed the traversal (`0x123E0`'s priority chain), so requiring `0x1108A` alone would misread a genuine hit on another route. Each route's innermost return, verified in the lifted source:

| Route (dispatch at `0x123E0`) | vtable slot | Innermost return |
|---|---|---|
| `app+0x40` → `0x114D0` | `+0x1C` | `0x114EA` ([recomp_0000.c:1037](src/recomp/gen/recomp_0000.c#L1037)) |
| `app+0x44` → `0x112A0` | `+0x10` | `0x112BA` ([recomp_0000.c:579](src/recomp/gen/recomp_0000.c#L579)) |
| `app+0x48` → `0x11700` | `+0x28` | `0x1171A` ([recomp_0000.c:1495](src/recomp/gen/recomp_0000.c#L1495)) |
| `app+0x4C` → `0x11930` | `+0x34` | `0x1194A` ([recomp_0000.c:1953](src/recomp/gen/recomp_0000.c#L1953)) |
| default → `0x11070` | `+0x04` | `0x1108A` ([recomp_0000.c:121](src/recomp/gen/recomp_0000.c#L121)) |

The five route *calls* themselves return to `0x12431`, `0x12448`, `0x1245C`, `0x12470` and `0x124C3` respectively (all `jmp 0x124C3`), which appear further out in the raw frame. All five end at the same `0x24700` update, so **any** of the five innermost returns with a `1/120` step counts as the traversal updating this object.

**Why this instrument and not the alternatives.** `RECOMP_WATCH` traps the page, single-steps the writer and prints the guest frame, so a hit is decisive about *whether and by whom* alpha was written. `RECOMP_WATCHDOG_SECS` is out: `run-jsrf.py:326` strips it and it ends the process with `_exit(3)`. `RECOMP_PEEK` only snapshots. `RECOMP_ICALL_FEEDBACK` would need a rebuild (`#ifdef`-gated), so it is not the smallest instrument for a no-rebuild run.

**A stronger no-rebuild alternative exists, at a cost — recorded so the owner can choose.** Quick Advisor consult 3 observed that `just ttd-record` (strict profile plus `RECOMP_APU_TRAP=1`) runs on the **current build** and would show every `sub_00024700` entry with its `ECX`, `[esi+0xC0]` and the branch taken — which separates the remaining cases directly, and strict runs do reach phase 2 / hold 121 (`20260930-225739-446-f3-alias-fix-2-strict`). The Session has **not** run it and is not proposing it as the smallest step: it needs an **elevated** process, the trace is large (the recipe caps at `--max-file-mb 20480`), and it consumes a strict-profile run. The alpha watch is the cheaper first probe; TTD is the escalation if the watch comes back ambiguous. `docs/jsrf-run-profiles.md` is the authority on what each of those settings means for the profile.

**Named positive control — an *expected* control, not a guaranteed one.** `check-instrument-controls.py` classes `RECOMP_WATCH` as an instrument, so a control is required. The control is the **constructor's store of alpha `0 → 1.0`** at `0x244B8`, reached from the logo constructor's `0x24600(0xFF000000)` call at `0x7E752`–`0x7E757`: it runs early in every run, and it carries `raw[esp+0] = 0x2461D` with the caller return `0x7E75C`. Consult 4's §4.5 narrowing: call it **expected**, not guaranteed — *it is silent if alpha is already `1.0` when the store runs*, and the watch reports only changed values.

This control replaces two earlier proposals, and **both earlier forms were wrong**:

- The original **phase-0 fade-in** control is unreliable. Consult 4's §4.5 narrowing of its own timing claim: *"In the phase-1 dump, frames 72 − hold 61 = 11 activations lie outside the captured phase-1 hold. This doesn't measure phase 0's length and doesn't exclude a completed or partial phase-0 fade."* The fade is unarmed (`done = 1`, `target = 0.0`) with alpha 0 at capture, so the control could plausibly stay silent for reasons unrelated to the question.
- The **`done`-flag** control is superseded for the reason above — it cannot separate stepping from a jump.

**What the outcomes decide — and what they do not.** `watch_report` suppresses unchanged values (`xbox_memory_layout.c:1567–1568`: `if (now == g_watch_last) return;`), so this is a **changed-value** watch, not an entry trace or a write log.

| Outcome | Means | Does **not** mean |
|---|---|---|
| Alpha steps by `1/120` repeatedly with a traversal return in `raw[esp+4]` after the arm (`0x1108A`, or `0x112BA`/`0x114EA`/`0x1171A`/`0x1194A` on a mode route) | The traversal's update ran on this object and the value advanced — the fade is being driven. | Not that the fade is visible on screen, and not that the value survived. If alpha later returns to 0, a later write moved it down: either an **immediate set** (`0x24480` via `0x24600`; `raw[esp+0] = 0x2461D`) or a **re-arm** (`0x24540`) to a lower target, after which the *same* updater `0x24700` decrements. Which one it is has to be read from the raw slots, not assumed. |
| Alpha jumps to 0 with `raw[esp+0] = 0x2461D` | The **immediate-set** path ran, not the update — a different defect from "the update never runs". | Not that the update is fine elsewhere. |
| No alpha change after the arm | The alpha dword was never reported changing after the arm. | Not that `0x24700` never ran: a run in which the FPU branch skipped the add, or in which the watch failed (protection lost, alias write), or in which the change fell outside the 150 s window, looks identical. Guaranteed guest value changes are not guaranteed observations. |
| No alpha change at all, including the constructor | **UNCONTROLLED** — the control did not fire. | Not a proven wrong address: check the runtime's `WATCH: … not armed` line first. `UNCONTROLLED` is a verdict on the run, not a diagnosis. |

**Limits.** This is exploratory and moves no strict horizon; it establishes only whether the fade's alpha changed, by how much, and through which of the two writers. It does not establish a cause, and it is not a fidelity claim. No ledger ID is allocated for it: `RECOMP_WATCH`/`RECOMP_WATCH_RAW` are already listed observation-only with no semantic effect, so the run adds no shortcut.

**Stopped here for owner approval** of that one run. Nothing was built, launched or changed in the port; no bypass, stub or ledger entry was created.

### Quick Advisor consults

**Consult 1 — instrument choice.** Advisor child `c548070d-9cc1-4340-b3b0-d56d88e2f0cc`, `claude/claude-opus-5-5` @ `xhigh`, continuable (startup probe PASS, `docs/reviews/startup-current.md`). Question: is `RECOMP_WATCH` on the fade's alpha the right smallest no-rebuild instrument, and is there a cheaper decisive switch? Answer, reproduced and independently confirmed by the Session: the proposed pointer spec **misparses** under the real grammar (confirmed at `xbox_memory_layout.c:1699–1705`) and would arm on a silently-wrong readable address; use the literal VA after confirming `app+0xB0` in another archived dump (confirmed across three dumps); `RECOMP_WATCHDOG_SECS` is stripped and `RECOMP_PEEK` cannot separate the two cases; a watch hit is decisive but silence is not, so the run needs a positive control; and the zero-cost checks (walker gate, node fields, kernel-bridge tail) should run first. Those zero-cost checks were run and are recorded above. Its proposed control (the phase-0 fade-in) was later found defective — see consult 3. `REVERSED BY:` walker disassembly showing no per-node gate before `0x11087` — the walker **does** gate at `0x11077` (`js` on `node+4`) and again at the `0x123E0` flag branches; or archived port dumps disagreeing on `app+0xB0` — they agree.

**Consult 2 — is the frozen frame a contradiction?** Same child. The Session was about to record that the frozen `0x13A80` frame proved a live contradiction (walk ran ⇒ alpha must have been written ⇒ yet alpha is 0). The Advisor ruled the inference **unsound**, and named the failing premise: the fade was not yet **armed** when that frame's walk reached it. The Session then reproduced each step from the original bytes and retracted the reading (§2 above): `0x24703` skips when `done != 0`, `0x24962` is `pop esi; ret`, the walk is preorder with the fade ahead of the logo, and the arm at `0x24540` sets `done = 0` and the target but never `+0x98`. **On the hypothesized arming tick the fade's `done` is still 1 *before* the logo arms it** — a conditional pre-arm state, not the captured state: the phase-2 captures read `done = 0`, and `done = 1` at capture is **not** claimed. Which of the three setters (`0x246C1` constructor, `0x2494A` completion, `0x24493` immediate set) left it at 1 in that pre-arm state is not claimed either. The Advisor further concluded "the walk stopped — the main loop has not finished a frame since the arming tick". The Session checked the archive against that and reported it unsupported: the activation counter at `app+0x87E0` (loaded `0x13F5C`, stored `0x13F6A`) reads 321 / 1016 / 3532 at 62.6 / 182.7 / 603.2 s for the same executable `7027fafad9cd7069`, each run ending at phase 2 / hold 121 / alpha 0. That shows the runs completed **different totals** of frame activations while ending in the same terminal logo state — it does **not** show thousands of activations after arming, and it cannot, because an endpoint snapshot is not an interval history. The record therefore states only that a "loop stopped at the arming frame" account is unsupported, and that the archive cannot separate "no route reached this node" from "the update ran but did not change the value". `REVERSED BY:` a per-run interval trace (or a `[WATCH]`/counter series) showing activations continuing after arming with alpha still 0, which would settle what the endpoint cannot.

**Consult 3 — the repaired proposal, and a retraction.** The turn-end reviewer rejected the first proposal's outcome table as non-discriminating and required either a fixed bound and a genuinely discriminating instrument, or the Advisor's agreement that WATCH-only silence is inconclusive. The Session sent that request with the counter evidence. The Advisor answered, and its reply changed three things:

1. **It retracted its own consult-2 conclusion.** Its `REVERSED BY` condition there was evidence that the loop keeps running; the counter data meets it (the hold advances one per activation in phase 1, and the frame count rises 321 → 3532 → 3974 with run length). It withdrew "the walk stopped" and stated that the walk runs, the fade is visited before the logo, and alpha never moves.
2. **It narrowed the negative case** from four possibilities to three, ruling out "`done` stayed 1" (every phase-2 dump reads `done = 0`) and "a same-value store hides the add" (`0 + 1/120 ≠ 0`). What remains: never invoked on this object; invoked but the translated FPU branch skipped the add; or the watch failed.
3. **It raised the control question** that led to the corrected proposal. Its categorical form ("the phase-0 fade-in never runs") is **not** adopted, and consult 4 subsequently **retracted that specific causal claim**. Consult 4 kept a timing point — that phase 0 ended within ~11 activations, too short for 120 steps of 1/120 — but **this record does not adopt that either**: `72 − 61 = 11` counts activations outside the captured phase-1 hold, which is not a measured whole-phase-0 history and does not exclude a completed or partial fade. What survives from both rounds is the practical point that the **phase-0-fade-in control is unreliable**, which is why the control was replaced (see the proposal).

**Consult 4 — the contested premise, and the final instrument choice.** Sent under §4.3 `PREMISE_CHANGED` with the `step = 1/120` evidence. The Advisor:

- **Retracted "slot 6 empty / the phase-0 fade never ran"**, and argued the phase-0 arm is not the source of the observed step (target and phase mismatches, above — both verified by the Session, and both **conditional** as narrowed in the lead section: they exclude `0x7E4AF` as the unreplaced last arm, not every historical invocation).
- **Reversed the watch target back to alpha** (`0x15F0EF8`), because `done` carries no per-step history and so cannot separate alpha *stepping* from alpha *jumping to 0*.
- **Supplied the attribution mechanism**: with `RECOMP_WATCH_RAW=1`, the two candidate writers have disjoint innermost raw slots (`raw[esp+0] = 0x2461D` for `0x244B8`; `raw[esp+0]` = saved node / `raw[esp+4] = 0x1108A` for `0x24748`). The Session verified the disjointness directly: `0x24480` performs **no push** before its alpha store, while `0x24700` does `push esi` at entry.
- **Supplied the new control**, later narrowed at the Session's request to an **expected** rather than guaranteed one: the constructor's alpha `0 → 1.0` store at `0x244B8` via `0x24600(0xFF000000)` at `0x7E752`–`0x7E757` — silent if alpha is already 1.0 when it runs.
- **Named its own reversal conditions**, recorded as *compatible-with* rather than proofs, and narrowed under §4.5: target 1.0 with step 1/120 *"would fit `0x7E4B9` as the last arm; it would not prove it"*; and several updates per activation are shown *"only by counted `1/120` alpha steps from `0x24700` (`raw[esp+4] = 0x1108A`), e.g. a full 120-step 1→0 fade, outnumbering the `app+0x87E0` activations over the same interval."*

**Consult 4 §4.5 — the escalation, and the Advisor's own further narrowings.** A reconciliation item survived two consecutive reviews, so it went to the Advisor rather than another edit/review loop (§4.5). The Advisor **agreed to all five** requested narrowings (timing, source, both reversal conditions, target choice, and the "second writer" phrasing), and volunteered two more of its own: it **withdrew the discriminator** it had implied — *"the endpoint fits a completed `0x24700` fade-in exactly as well as an immediate `0x24600(0)`/`0x24480` set. My 'matches exactly' separated nothing"* — and it narrowed its candidate-set census, which the Session then verified directly (three `1/120` arm sites; one direct caller of `0x24540`; `recomp_0002.c:20963` not an arm site; computed-pointer calls not excluded). Its `REVERSED BY`: a direct or indirect `0x24620`/`0x24540` caller outside the logo pushing target 0 (or `edi`) with step `0x3C088889`; or `app+0x87E0` being reset during the logo, which would void the activation arithmetic.

**Provenance for the Advisor's rulings.** The rulings above are not paraphrases of a summary. The child's full transcript is persisted at
`%USERPROFILE%\.dsh\sessions\--C-Users-logic-Repos-my_xbox_game--\c548070d-9cc1-4340-b3b0-d56d88e2f0cc\session.v4.jsonl.zstd` (zstd-compressed JSONL; the consult-4 §4.5 ruling is `seq 481`). It decompresses with `zstandard` and carries the verbatim text, including the two self-narrowings quoted here.

It also named a stronger no-rebuild instrument (`just ttd-record`), recorded above rather than adopted, because it needs an elevated run and a large trace.

**Status of the "walk stopped" question.** Both the Session's earlier reading and the Advisor's consult-2 ruling on it are now withdrawn. What the record claims is the narrower, supported statement: the walk runs and reaches the fade **before** the logo in the same traversal; the fade's `done` is 0 and its alpha is exactly 0 at capture in every phase-2 run; and the archive cannot say whether `0x24700` was invoked on this object during the hold. That is a live, well-posed question — not a proven contradiction, and not a proven halt.

**Records-only closure.** No port fix, no runtime edit, no new run, no ledger ID, no observer retry, no APU trace. The single xemu session was read-only and is spent.
