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
