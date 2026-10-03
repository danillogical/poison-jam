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

Toolkit receipt: **PUSHED_TO: origin / BRANCH: main / COMMIT: 929856fcfc145036252510509abaa0782d910a22 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: Everything up-to-date**. Clean unchanged toolkit and equal origin/main verified; pushed first. Game commit/push receipt pending.

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
