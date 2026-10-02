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
