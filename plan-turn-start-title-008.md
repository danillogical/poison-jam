# plan-turn-start-title-008

**Turn Planner, independent starting strategy.** The Orchestrator owns the turn and may change
this plan when measurements warrant. Every item marked OBSERVED was verified this session against
`logs\runs\20261007-085324-850-title007-blacktrace` ("blacktrace"),
`logs\runs\20261007-081204-797-title007-witness2` ("witness2"), and toolkit source at `0c1c7ad`
plus its uncommitted working tree.

## 0. Objective, baseline, corrections to the brief

**Objective.**
- **Primary:** a same-flip draw→present trace on the `NV097_FLIP_STALL` path, keyed on
  `(flip_stalls, present serial)`. It must only observe; it must not change selection or
  presentation.
- **Secondary:** admit the 29-method class (`0x0580-0x05AC`, `0x06C0-0x06FC`, `0x1964`) from a
  runtime witness.
- **Then** continue toward M15: the title screen itself, judged by content and by eye, with the
  image preserved.

**Corrections to the brief (all OBSERVED).**
- **The toolkit tree is not clean.** It holds an uncommitted `RECOMP_FLIP_TRACE` draft:
  - `src/kernel/nv2a_pb_exec.c` (+184 lines): `trace_note_surface`, `trace_hash_surface`,
    `trace_reason`, `trace_flip`.
  - `src/video/fb_present.c`: `xbox_FramebufferPresentSerial` and `xbox_FramebufferPresentHash`.
  - Two unrelated test files are also modified: `tests/dir_context_release_test.c` and
    `tests/kernel_file_apc_test.c`.

  Build on the draft and fix it (§1). Keep the two test files out of the trace commit.
- **Blacktrace's "~170 s black" is mostly a stalled walk, not black flips.**
  - Presents go from 2425 (t=519 s) to 2440 (t=528 s).
  - Then comes `[PFIFO] reject … 1964` (log line 108865), after which presents stay at **2442**
    from t=538 s to t=688 s.
  - That leaves only about 18 black flips to observe.
- **The long black with presents still advancing is witness2.** That run used `ADMIT_UNKNOWN=1` and
  the same method-table hash (`6a83be…`). Presents reached 3120, with 4533 commits,
  `admitted_unknown=0`, and no `admit-unknown` lines.
  - So in that run **the black does not depend on the 29** (OBSERVED, one run).
  - It also shows that reaching `0x1964` varies from run to run, so a witness run can come out
    NOT EXERCISED.
- **`[TEXUSE]` is confirmed.** `0x80084000 640x480 fmt 0x11 lin` appears in 52 report blocks in
  blacktrace. The first (line 15441) reads 84 batches and the last reads 2474. Witness2 reaches
  13035, roughly 15 batches per black flip.
- **The `[GPU]` report fields don't mean what they suggest.** "draw surface" moves between
  `0x84000`, `0x11C000` and `0x1B2000`. "clear surface" is just `s_gpu.color_offset`, the current
  target, not a record of clears.
- **What the 29 do in the executor.**
  - `0x1964` is `SET_VERTEX_DATA4UB` for attribute 9 (texcoord0), with param `FF000000`. The
    executor accepts it but ignores it, because only attribute 3 (diffuse) is used.
  - `0x0580` and `0x06C0` go only into `s_reg`. Apart from a debug print, nothing in rendering
    reads them.

**Leading hypothesis (INFERRED).**
1. The guest renders the scene off-screen into `0x84000`.
2. A full-screen pass samples that surface as a linear R5G6B5 texture and writes into a swap
   surface (`0x11C000` or `0x1B2000`).
3. That pass produces black.
4. `present_track_flip` then correctly picks the swap surface drawn this frame, which is black.

**Competing explanations to keep open.**
- **Selection:** the stale, targeted or fallback branch picks the wrong surface.
- **Copy:** the published copy is stale because `xbox_FramebufferWindowPresent` returned early.
- **Sampling:** the pass passes normalised UVs to a linear texture, so every pixel reads one texel.
  The top of `0x84000` is pure-black sky.
- **Blend:** `put_pixel` writes any pair other than SRC_ALPHA/ONE_MINUS_SRC_ALPHA as opaque.
- **Ordering:** the pass samples before the scene is drawn, or samples the surface it is writing.
- **Fade:** an unimplemented combiner or fade factor forces black.
- **Nothing composed:** no surface holds a finished frame at the flip.

## 1. Instrumentation: the minimum, building on the draft

**Hooks (already correct in the draft).**
- At `case NV097_FLIP_STALL`, read the flags before `present_track_flip`, which clears them. Emit
  the trace after `xbox_FramebufferWindowPresent`.
- At `SET_SURFACE_COLOR_OFFSET`, record the candidate surface.

**Add.**
1. **A per-frame batch ring:** the last 64 batches since the previous flip, emitted only on traced
   flips. Each entry holds:
   - target `color_offset`;
   - stage-0 texture: offset, format, width×height, pitch, valid, addr_u/v;
   - blend enable and factors;
   - draw path (imm, inline or arrays) and transform path (screen-space, fixed-function or vertex
     program);
   - `idx_count`, and the change in `tris_drawn` and `pixels`;
   - the first three vertices' UVs as the rasteriser uses them, and the texel each one samples.

   This ring is what connects the source surface to the pass and to the output. Also count
   `subch≠0` methods per frame.
2. **Proof that a frame was published.** Record the present serial before and after the `Present`
   call. If they are equal, the window copy was not updated (early return on `!s_fb_running` or
   `RECOMP_FB_VA`), and the "published" hash is stale. Also log the presenter's own `bpp`.
3. **Fix three draft defects.**
   - (a) It hashes every candidate with the *current* clip and pitch. Keep each surface's last
     pitch, format and clip instead.
   - (b) It hashes the bound texture with surface geometry, which is wrong for the 512² DXT
     textures. Use the texture's own geometry, or hash linear textures only.
   - (c) The `CHANGE` filter key has no content in it, so content changes are skipped. Add the
     candidate hashes and the published hash to the key.
4. **Content metrics per candidate:** non-black fraction and a coarse distinct-colour count. The
   hash only identifies a picture.

**Must not change.** The `present_track_flip` arguments and result, the Set/Present calls, guest
memory, `s_gpu`/`s_present`, the walk, PFIFO, and GET/PUT. Never use `RECOMP_FB_VA` and never force
a resolve.

**Perturbation risk.**
- Reading the surfaces has no side effects. The contiguous window is plain host memory; MMIO is not
  touched, and the draft bounds-checks the range.
- The real cost is time: roughly 3–5 surfaces × 307k pixels per traced flip, on the executor
  thread, inside the walk, under the PFIFO lock. That can shift guest timing, so:
  - with tracing off, leave only `trace_note_surface`;
  - gate tracing by a flip window and an event budget;
  - run one trace-off control on the same binary and confirm it still reaches 2424 and still goes
    black.
- Logs: about 8 lines per traced flip plus up to 64 ring lines, so about 5 MB extra for 300 traced
  flips on top of the 12 MB log.
- Visual dumps: raw RGB565 of every candidate plus the published copy, with a JSON sidecar, at most
  8 events (about 25 MB), gated by `RECOMP_FLIP_TRACE_DUMP`. Convert to PNG offline.

**Test.** In `nv2a_present_track_test.c`, check that the reason classifier covers all four branches
and that the selection is identical with tracing on and off. Validate the test by mutation.

## 2. Which events to capture

1. Every flip from `RECOMP_FLIP_TRACE_FROM=2400` until a 300-flip budget runs out. This covers
   disclaimer → `e886…` → black.
2. After that, only flips that match a diagnostic condition:
   - the published hash differs from the selected surface's hash;
   - the published frame is black while some candidate is not;
   - the decision key changes (reason, selected surface, or a candidate hash).
3. A final ring of the last 16 flip records, flushed at report time and at exit. A run that stalls,
   as blacktrace did, still records its last flips.
4. Visual dumps:
   - the 2 flips before 2424 (disclaimer and `e886…`);
   - the first 2 black flips;
   - the first and last flips that match the condition.

## 3. Candidate surfaces, and why the set is complete

- **What the presenter can publish:** every `SET_SURFACE_COLOR_OFFSET` value. The selected surface
  `done` is always one of `drawn_offset`, `targeted_offset` or `color_offset`, and each of those was
  set by that method. So this set is **PROVED complete for what the presenter can publish**, given
  that these runs have no `s_backend`.
- **What the pass reads:** every texture offset bound this frame (from the batch ring), plus
  `s_tex_use`.
- **Logged as context:** `xbox_GetDisplayFramebuffer()`, `PCRTC_START` (0 in blacktrace),
  `s_gpu.color_base`, and `flip_read`/`flip_write`/`flip_modulo`.
- **The one gap is a guest CPU write.** Hash each candidate at the frame's first batch as well as at
  the flip. If a surface changes and the ring shows no executor writes to it, the guest CPU wrote
  it.

## 4. Tracing the `present_track_flip` reason

- Use the flags as they were before the call: `drawn_this_frame`, `targeted_this_frame`
  (used_targeted), `stale_drawn_offset`, `fallback_color_offset`.
- Log `s_present.drawn_offset`, `s_present.targeted_offset` and `s_gpu.drawn_offset` separately;
  they can diverge.
- For `drawn_this_frame`, record which batch in the ring set the flag last. That shows whether the
  deciding batch was the full-screen pass or an overlay.

## 5. The 29 methods, from a runtime witness

**Getting the witness.**
- A run of at least 700 s with `RECOMP_NV2A_ADMIT_UNKNOWN=1` can double as a trace run.
- Only `[PFIFO] admit-unknown` lines count as the witness. Record run, line number and log SHA-256
  for each in `config/nv2a-runtime-witnessed-methods.json`.
- No lines, as in witness2, means NOT EXERCISED: repeat the run. Never fall back to the decode.
- Check the witnessed set against blacktrace's no-admit `missing_methods` (29). That list
  corroborates; it does not replace the witness.

**Admission.**
- Run `scripts/gen-nv2a-method-inventory.py --witness=…` and verify the delta is exactly **+29 on
  class 0x97**, with zero removals and with `0x1810` and the 39 still present.
- Expected effect:
  - `0x0580`/`0x06C0` are captured into state and nothing reads them.
  - `0x1964` is accepted and has no effect.
  - Record attribute-9 texcoord sent as 4UB as a render gap; it may matter if the composite pass
    carries its UVs that way.

**Order.**
- Trace first, on binary B1: an admit run (also the witness attempt), a no-admit control, and a
  trace-off control.
- Then admit the 29 and make one no-admit trace run on B2. Admission mainly changes whether the walk
  keeps flipping, not the pixels (INFERRED).

## 6. Parallel workers (disjoint files)

| Worker | Owns | Task |
|---|---|---|
| W1 | toolkit `nv2a_pb_exec.c`, `nv2a_present_track.h`, `fb_present.c`, `tests/nv2a_present_track_test.c` | finish the trace (§1–§4) and its tests; build B1 |
| W2 | game `scripts/fliptrace-report.py` (new) and its test | parse `[FLIPTRACE]` into a per-flip table, raw→PNG, content metrics, an xemu comparison sheet |
| W3 (read-only) | `logs/workers/title008/w3/` | `jsrf_gpu.py` decode of blacktrace's last frames before GET: the full-screen pass's target, texture, blend, UV source/range and path, as a static Case B prediction |
| W4 (after a witness) | game witness JSON; toolkit generated `nv2a_method_table.c` | admit the 29 and verify the delta |

- Builds and runs go one at a time; each run takes about 12 minutes.
- Before comparing runs, check `exe_sha256` and the `build-source.json` hashes of the method table
  and `nv2a_pb_exec.c`.

## 7. Branching after the first trace run

**Case A: selection or copy is wrong.** Any of:
- some candidate holds a composed non-black frame, but stale/targeted/fallback picks a black
  surface;
- serial after == serial before;
- published hash ≠ selected surface hash.

Action: fix `present_track` or the presenter, with a unit test, then rerun.

**Case B: the pass produces black.** The reason is `drawn_this_frame`, the deciding batch is a
full-screen pass into the selected surface, and that surface is black while the bound `0x84000` is
non-black at the same flip. The ring separates the causes:
- normalised UVs on a linear texture → constant texel;
- an unmodelled blend pair;
- ordering or sampling its own target (compare texture hashes at the first batch and at the flip);
- a combiner or fade.

Action: fix with a fixture, then rerun.

**Case C: nothing composed at the flip.** Any of:
- every candidate is black at every traced flip;
- the scene exists only between flips;
- a guest CPU write is detected.

Action: look upstream (guest fade/timing, combiner, a guest wait tied to the walk stall) and
consult the Advisor.

**In every case:** if the presented frame turns non-black, compare it **by eye** with the xemu
deliverable, never by hash.

## 8. Completion criteria

1. The trace committed (toolkit first, then game): off by default, with a mutation-validated test
   and a stated perturbation analysis.
2. At least one run past presents 2424 with keyed trace lines across the transition and into the
   black, and its visual dumps kept and inspected.
3. Every conclusion labelled OBSERVED / PROVED / INFERRED / UNQUALIFIED / NOT EXERCISED, and the
   result classified as Case A, B or C.
4. The 29 admitted from a witness (+29, zero removals), or recorded as NOT EXERCISED with the runs
   that tried.
5. Records updated: plan Current work, a new TR §23.6, and the ledger, including the §0
   corrections.
6. If the cause is found: the fix landed and a run made to look for the title. M15 is claimed only
   on title content confirmed by eye, with the image preserved.

**Advisor consult points:** the trace design and its perturbation before the first run; the
semantics of a Case B fix (linear-texture coordinate convention, combiner); and whether admission
should come before or after a Case B fix.
