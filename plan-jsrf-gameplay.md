# Jet Set Radio Future: playable opening area, then a native Vulkan build

**Status: the sole execution plan** (owner, 2026-10-14). Completed plans are retired to git history
(title screen and keyboard input: `git show 3f414ba:<file>`). `AGENTS.md` owns engineering
operations; `docs/jsrf-run-profiles.md` owns evidence classification;
`docs/jsrf-technical-record.md` (TR) owns established findings; `docs/jsrf-compatibility-ledger.md`
owns shortcuts; `config/stop-chain.json` owns stops.

## Objective

A playable opening-area slice on a **native Vulkan build with no code translated or compiled at run
time**: correct rendering, a visible player, movement, the camera, dialogue and the pause menu. Graffiti,
audio fidelity and save/resume come later. Hardware fidelity is not claimed by exploratory evidence.

## Owner direction (2026-10-13)

- **Compile-time code over live translation:** anything translated or compiled at run time won't carry
  over to native functions, so it needs an owner decision.
- **Vulkan,** for a future Android port.
- **No new D3D11 work.** D3D11 stays only as the parity reference until the owner accepts Vulkan.
- **Then:** the library lift (the statically linked XDK Direct3D 8, then DirectSound onto portable
  audio), Android portability, and decompilation of game code function by function.

## Done (details in the TR)

- **M15 and K16:** title and live keyboard input. Records are in the retired plans' history and TR §26
  onward.
- **The Garage renders:** fade, floor, stencil, culling, characters and shadows (TR §§31.1–31.4).
  The full skate path runs.
- **Gum's tutorial is reached** with closed-loop `goto` steering (TR §§31.8–31.9,
  `config/key-scripts/gum-tutorial.json`).
- **The pause menu works:** stops 50–52 and a 25-function omitted-function batch (TR §31.10).
  77 ambiguous fatal targets remain in `config/recovery-unresolved.json`; repair each when a run
  reaches it.
- **Controller:** an Xbox One controller works through XInput, with rumble (toolkit `e365e04`).
- **Ported from andeecollard/jsrf-recomp** (TR §31.11), both confirmed by the owner in play:
  - the 16-bit clear colour, i.e. the white Smilebit card (`e7c2efb`);
  - the dialogue fence (`5569b1d`, ledger L17): the "Corn" label went from 1/40 to 40/40.
- **Dropped:** the gamma ramp. The logo fades already render (TR §31.12).
- **All 128 vertex programs are static XBE data** (TR §31.12, `scripts/find-vertex-programs.py`).
- **The Vulkan backend** (`RECOMP_GPU_BACKEND=vulkan`, ledger L62, TR §§31.13–31.16):
  - **V0:** build-time SPIR-V (`xbr_add_spirv`) and lavapipe for tests (`9f39537`);
  - **V1:** the core (`4f8a880`);
  - **V2:** parity tests;
  - **V3:** the combiner uber-shader (`d23ca42`), with the review repairs (game `434dde1`);
  - **V4:** 512 randomized combiner states within 1 LSB (`434dde1`).
- **The key-script title and menu detectors are fixed** (game `2e8b84a`). The unchanged stock
  `config/key-scripts/e2e.json` reaches the Garage on both backends, with matching character shading.
  The validated Vulkan run is `20261013-211154-309-keyscript-e2e-vk-v3-handoff-val`.
- **V5: the vertex programs come from a static corpus** (TR §31.19, ledger L62). `just shaders`
  extracts the 128 programs (126 distinct) from the XBE, translates them with the toolkit's new
  `nv2a_vsh_hlsl` host tool and compiles them offline with `dxc`; the Vulkan back end looks a batch's
  program up by hash and re-checks its identity against the words. A miss returns 0 and the CPU
  interpreter draws it. Nothing is translated or compiled at run time. The stock e2e on Vulkan
  (`20261014-144815-512-keyscript-e2e-vk-v5`, exe `5e80272d`, superseded by the repair below) had
  **0 vertex-program misses with 1.24M corpus-shader draws** (329M triangles); the repaired build's
  run is `20261014-165814-809-keyscript-e2e-vk-v5r` (exe `23fbe93d`), **0 misses with 1.22M
  corpus-shader draws**. The independently authored gating test `jsrf_vsh_aot` passes with its two
  negative controls.
- **V5 repair: a rejected identity was dereferenced while logging** (TR §31.20). An independent Codex
  review rejected V5 on this: `vb_program_lookup` rejected a corpus entry with NULL `words` or a wrong
  `len`, then its diagnostic digested that same rejected storage (`0xC0000005` for both, reproduced by
  a probe that compiles the two functions verbatim). The diagnostic now logs a reason from scalar
  fields only. A **different worker** authored the regression
  (`jsrf_vsh_aot_malformed`, `tests/test_vsh_aot_malformed.c`): it copies the corpus in memory,
  corrupts only the copy (`words` NULL, `len` `UINT32_MAX`/±1, missing shader), and requires a clean
  rejection with `identity_fail` +1, no hit, real CPU-fallback pixels and no crash. It **crashes on the
  shipped `32d83dd0` and passes on the repaired `84f987cd`**. The deciding pipeline-identity mutation
  was then **re-run against the final repaired source in a scratch tree**
  (`xbr-mac/v5mut`, production never mutated in place): baseline `84f987cd` PASS, mutant `d8a41429`
  FAIL (`gpu surfaces differ in 0 of 4096 pixels`), restored `84f987cd` PASS. Final gate: CTest
  **74/74** on lavapipe, `just check` passed, and the e2e `20261014-165814-809-keyscript-e2e-vk-v5r`
  (exe `23fbe93d`) has **0 misses with 1.22M corpus-shader draws**.
- **V6: the synthetic vertex-program conformance gate** (TR §31.21, `tests/test_vsh_conformance.c`).
  A worker distinct from every V5 author built the gate on the parity test's own V1–V7 programs
  (extracted at configure time, so the encoder cannot drift). It checks the offline host tool's bytes
  against `nv2a_vsh_hlsl` directly, the `--vk` decorations including each output varying's location by
  name, a table hit against the CPU interpreter's pixels, a miss and a corrupt identity falling back to
  the CPU, and V7 under `RECOMP_GPU_VSH=0`. An independent worker ran 18 mutants and found 7 false
  passes — one a real defect (output locations only counted, not value-checked), one a real fixture gap
  (no fixture used MAC `add`), and then six MAC ops (`dp3`/`dph`/`min`/`max`/`slt`/`sge`) with no
  coverage at all — all fixed and mutation-confirmed. Thirteen fixtures, eight CTest cases, full CTest
  82/82, `just check` passed. Test-only: no toolkit change.
- **V6 repair: the four-channel pixel predicate was wrong twice** (TR §31.21). An independent review
  rejected the gate as committed: `chan_diff_n` extracted the fourth channel with a right shift by -8
  (undefined behaviour, so the fourth channel was whatever the build made of it — one clang-cl build
  compared alpha only, another rejected an identical image; MSVC 19.51 masked the count and was
  accidentally correct), and the first repair indexed a shift table by a channel *count*, so the
  "RGB-only" set became alpha/red/green — blue was never compared and alpha leaked into the coverage
  test (wrong on every compiler). Both forms are replaced by named channel-index sets (`chan_diff` =
  `{A,R,G,B}` decides a match; `chan_diff_rgb` = `{R,G,B}` decides coverage), and a worker distinct from
  both the fix author and the original V6 author added `jsrf_vsh_conformance_channelgate`, which drives
  the real predicate: an alpha-only image is rejected, an in-tolerance one accepted, red/green/blue each
  discriminate, and coverage stays RGB-only. A scratch A/B/C run shows the new case **fails the partial
  state on MSVC and clang-cl and fails the committed original on clang-cl** (where its predicate rejects
  an identical image), and passes on the repaired source; nine V6 CTest cases, full CTest 83/83,
  `just check` passed. Test-only: no toolkit change.

- **R1 repair is independently accepted** (TR §§31.22–31.24). The rejected first
  implementation `9078b47` recorded provenance but left three gaps: a changed or missing host-tool
  hash was accepted, a stale explicit tool override was accepted with a warning, and the guard could
  validate a toolkit different from the one CMake builds. The repair makes `tool.sha256` recorded and
  DECISIVE (schema 2), bound to the one generating tool the repository owns
  (`build/vsh-tool/.../nv2a_vsh_hlsl.exe`), refuses a stale `JSRF_NV2A_VSH_HLSL` override instead of
  warning, resolves ONE toolkit for the guard, the generator and the CMake link (refusing a
  cache/environment disagreement), and checks the owned tool tree's own `CMAKE_HOME_DIRECTORY`
  against it. A different worker's regression fails eight weakening mutants of the new gates with a
  comment-only control green, and the real scratch gate shows a whitespace content edit refused
  before configure and cleared by regeneration. V5/V6 acceptance is unchanged.

- **R2 is done: a build without the Vulkan toolchain compiles an honest empty corpus** (TR §31.25).
  The toolkit publishes ONE bound capability decision (loader, headers, `dxc` and `spirv-val` must
  all EXIST; `XBR_VULKAN_FOUND` is a FORCE-refreshed cache BOOL with
  `XBR_VULKAN_UNAVAILABLE_REASON`), the game's configure branches once on it — capability on: the
  real corpus is required, named `just shaders` when absent, and validated inside that configure;
  capability off: a generated empty table (`nv2a_vsh_program_count = 0`) under `<build>/empty-corpus`
  is compiled even when a retail corpus is present — and the build guard decides the capability
  fresh, so no cached verdict can pick a mode. A different worker's regression fails four mutants of
  the branch logic; the no-SDK configure succeeds and links the real game, and the SDK-present
  no-corpus configure still refuses naming `just shaders`. Codex independently accepted R2 at
  game `b28b7cb` / toolkit `b20ddba`: 176 tests passed, all four branch mutants detected, and the
  final-source capability-OFF configure/link passed.

- **R3 is independently accepted** at game `5842b38` / toolkit `a052de5` (TR §31.26). The corpus lookup moved ahead
  of the cull-all early return in `vb_draw_program`, so a fully culled batch still records whether the
  corpus carries its program (`misses` +1 exactly for an absent one) while the cull-all branch keeps
  returning 1 even on a miss — nothing visible is drawn by either path, so the executor does not
  transform it on the CPU. Counter meanings are unchanged, a culled batch does no setup, allocation or
  submission, and the deciding fixture proves the ordering with a per-batch delta plus a mutation that
  cannot distinguish it any other way. Codex independently reproduced the passing gate, the old-ordering
  mutation failure and the restored pass. **The owner's pause was lifted on 2026-10-16** ("V7 is done
  now, continue the plan"); the plan resumed with the approved sequence, as the owner's own commits
  then recorded: N2 (`1a1563e`), V7 under N3 and the Vulkan play option (`550342f`), and the launcher
  default (`b1df807`). R3's acceptance above is unchanged and none of that work was reopened; N4's
  first subitem is recorded below.

**Owner review accepted V5 and V6** at game `2a350ec` / toolkit `5668278` (2026-10-15).
R1–R5 below are approved follow-ups, not reasons to reopen those milestones. The review was a code
review; it did not rebuild the Windows checkout or re-read its run logs.

## Remaining work, pre-approved by the owner (2026-10-15)

**N1, the execution rule: these steps are the approved plan.** Execute them in order, directly. Don't
open a separate planning phase or start a headless planner for them; the previous operation stalled
on exactly that (TR §31.17). Plan only for work this list doesn't cover. Stop and ask the owner only
for:

- force pushes or rewritten history;
- original assets or saves;
- an environment failing twice;
- a shortcut that changes what a milestone means;
- anything compiled or translated at run time.

**N5: one step per operation.** Each step below is sized to finish and report on its own, so a
bridge timeout loses one step at most. Every code step gets its gating test from a **different**
worker. Commit and push each gated step (toolkit first, then game); don't carry finished work
uncommitted. Quote the deciding mutation results, with final-source identities and baseline/restored
positive controls. No commit signing, co-author or signature trailers. Session reports stay outside git.

**Execution order:** R1 → R2 → R3 → N2 → V7 under N3 → V8 → N4 (including R4) → N6.
N2 remains a prerequisite of V7. R5 is low priority and follows V7; schedule it after N6 so it does
not interrupt the required sequence. Each step or listed N4/N6 subitem is a separate operation.

### R1: detect a stale shader corpus — DONE (TR §§31.23–31.24)

The runtime identity check proves the guest words, not which translator or compiler produced their
SPIR-V. Synthetic table CTests do not validate the real generated corpus.

- Record provenance in generated `nv2a_vsh_programs.h`/`.c` (schema 2): SHA-256 of toolkit
  `nv2a_vsh_hlsl.c`, `nv2a_vsh_hlsl.h`, `nv2a_vsh_hlsl_tool.c`, the generating tool itself, the
  `dxc --version` string and dxc arguments, and the corpus's own counts. Every field decides,
  `tool.sha256` included, so a corpus that omits one is refused rather than passed.
- The recorded executable is the ONE the repository owns, `build/vsh-tool/.../nv2a_vsh_hlsl.exe`; the
  game build tree's copy is not a candidate, because a relink moves its hash without an input moving.
  Only `just shaders` rebuilds the owned artifact, so deleting or relinking it, moving a translator
  source, or pointing the build at another toolkit requires `just shaders` before `just build`.
- Have `scripts/build-jsrf.py` recompute that provenance before configure and refuse a mismatch,
  naming `just shaders`. Never silently link stale shaders.
- ONE toolkit binds the guard, the generator and the CMake link: the build tree's cached
  `XBOXRECOMP_DIR` wins, the environment and the sibling are fallbacks, and a disagreement between
  the cache and the environment refuses naming both paths. The owned tool tree's own
  `CMAKE_HOME_DIRECTORY` is checked against that same resolution, so a tool built from another
  checkout cannot stand for these sources. A `JSRF_NV2A_VSH_HLSL` override is freshness-verified
  (content first, mtime second) and refused when stale; it is never rebuilt, replaced or overwritten,
  and it cannot record a source change the corpus does not already record.
- Run `check-vsh-aot-table.py` against the **real** generated table in `just check`, with its real
  XBE/compiler inputs and the same resolved tool; retain device-free synthetic tests in CTest.

**Gate (independently accepted at `c6eaf6b`).** Matching provenance and a normal fresh bootstrap pass, and
the full CTest run is green. A different worker's regression (126 tests in the provenance file, 166
in the focused suite) fails eight weakening mutants of the new gates — the tool hash made advisory,
dropped from the required expectations, `expected(tool_path=None)`, the guard's no-tool and
stale-override refusals, the guard call moved after configure, and the toolkit-conflict check
neutered — with a comment-only control green, and it drives the real `build-jsrf.py` in a scratch
tree to show a corpus mismatch refused before configure (no configure marker, no configure log). The
real scratch gate: the baseline supported build accepts; a whitespace content edit to
`nv2a_vsh_hlsl.c` makes it refuse before configure, naming the field and `just shaders`; offline
regeneration rebuilds the owned tool from the current sources and re-records the identity; the build
accepts again. A tool-only replacement and a cache-versus-environment toolkit mismatch both refuse.

### R2: an empty corpus when the Vulkan SDK is absent — DONE (TR §31.25)

Vulkan is optional. If Vulkan or its shader toolchain is unavailable, an empty table is generated at
configure time (`nv2a_vsh_program_count = 0`) with a clear status; program batches use the CPU
interpreter. With the toolchain available, a missing corpus remains a fatal error naming
`just shaders`.

- ONE bound capability decides the mode. The toolkit's `cmake/XbrVulkan.cmake` requires the loader,
  its headers, `dxc` and `spirv-val` to EXIST (a cached path that no longer exists is not a
  toolchain) and publishes `XBR_VULKAN_FOUND` as a FORCE-refreshed cache BOOL plus
  `XBR_VULKAN_UNAVAILABLE_REASON` naming the failed check — recomputed on every configure, so the
  game, the build guard and any parent project read one value and no earlier configure's verdict
  survives.
- The game's configure branches once on it. Capability ON: the retail corpus is required (absent =
  FATAL naming `just shaders`), its provenance is validated INSIDE that configure
  (`scripts/build-jsrf.py --check-corpus-only --expect-capability-on`), and the corpus-dependent test
  targets are registered as before. Capability OFF: a generated empty table is written under
  `<build>/empty-corpus` and compiled instead, even when a retail corpus is present, and the
  corpus/Vulkan-dependent tests are not registered. One `JSRF_CORPUS_SOURCES` /
  `JSRF_CORPUS_INCLUDE_DIR` pair feeds every target, so the branches cannot drift.
- The build guard decides the capability FRESH: it configures the shipped probe project
  `scripts/cmake/vulkan-capability` under `build/`, forwarding the controls the configure will see,
  and reads the verdict from that probe's cache; an undetermined probe is fail-safe (the retail
  corpus stays required). Capability OFF prints the empty-corpus status and needs no corpus, no owned
  tool and no `VULKAN_SDK`.
- Offline `just shaders` still needs `dxc`/`spirv-val`. The empty table is an honest absence of
  compiled programs, never a claim that a retail table passed R1/V5/V6, and nothing is translated or
  compiled at run time.

**Gate (met).** Capability OFF, with and without a corpus: the supported configure exits 0, reports
`Vulkan/unavailable: …`, generates a table with `nv2a_vsh_program_count = 0`, compiles only that
file, and links the real game (`jsrf_recomp.exe`, exit 0); the retail corpus is byte-untouched.
Capability ON with no corpus: the configure fails naming `just shaders`, and the guard refuses before
configure without creating a build tree. Capability ON with the corpus: the configure validates it,
and a corpus whose `tool.sha256` moved makes the configure — and a later rebuild with no reconfigure —
fail rather than compile. OFF→ON→OFF in one build tree flips both the cached value and the compiled
corpus source. A different worker's regression (10 scripted cases, 176 tests with the R1 suites)
detects four mutants of the branch logic, and a direct `cmake -S . -B build` cannot bypass the guard.

### R3: include culled program batches in miss accounting — DONE (TR §31.26)

`vb_program_lookup` now runs in `vb_draw_program` before the cull FRONT_AND_BACK early return, so a
batch whose program the corpus does not carry is counted even when nothing is visible: **coverage is a
property of the batch's identity, not of whether the batch was drawn**. The cull-all branch still
returns 1 regardless of what the lookup found (nothing visible means nothing drawn by either path, so
the executor must not transform the batch on the CPU), and the non-culled miss still returns 0 for the
existing CPU-interpreter fallback. `misses`/`identity_fail` and their write-once latches keep the
meanings `nv2a_vk_backend.h` documents: a batch whose program is absent is a miss and returns 0
whatever its input mask, and `declines` stays reserved for a program the corpus carries that this draw
cannot set up. No counter meaning changed; a culled batch still performs no setup, pipeline,
descriptor, stream or draw work, so a match without a draw fabricates no hit, triangle or upload.

**Gate (evidence in TR §31.26).** The real-backend fixture `jsrf_vsh_aot` now measures, around single
executor batches on lavapipe: a corpus-carried program drawn visibly (hits/tris/const_uploads +1); a
cull-all batch with an **absent** program (`misses` +1 exactly, `cull_all` +1, every other counter
unchanged, the surface byte-untouched sentinel — nothing drawn on either path); the same absent
program with culling not-all (`misses` +1 and the CPU interpreter draws it, pixel-parity with the CPU
reference); and a corpus-carried program fully culled (`cull_all` +1 only). Restoring the old ordering
in a private toolkit copy makes exactly the missing-corpus assertion fail (`misses advanced by 0,
expected exactly 1`), with baseline and restored runs green on the final source. R1–R3 land before V7.

### N2: a controlled benchmark — DONE (TR §31.27)

Medians of three 60 s Garage windows: D3D11 on the Iris Xe **15.17** presents/s; Vulkan on the same
Iris Xe **12.77**; Vulkan on the RTX 4070 **11.07**. Vulkan's cost is host-side sync and readback (N4).
Run `config/key-scripts/garage-bench.json` and `scripts/bench-summary.py` for every performance claim.

Original step text:

Whole-run present totals aren't comparable: Vulkan runs varied from 10.8 to 23.3 presents/s on the
same build, because the totals include boot and differ in progress. Add a key script that:

1. enters the Garage;
2. holds still for a fixed 60 s;
3. records presents/s and the frame-time distribution from the `[FBPRESENT]` and stats lines, over
   that window only.

Run it identically on both backends, three times each, and quote the median. Use it for every
performance claim from here on.

### N3: validation discipline (applies to V7 and every later Vulkan acceptance run)

- Set `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`.
- Quote the loader's line showing the layer was inserted. A run without it can't claim a validation
  pass.
- Report validation errors and warnings from the backend's counters.

### V7: acceptance — DONE (TR §31.28)

Vulkan `e2e.json` under the validation layer: complete, 0 errors (10 benign "attribute not consumed"
warnings), 0 misses; frames match D3D11; dialogue 40/40.

Original step text:

`e2e.json` on Vulkan (RTX 4070), under N3. It must complete with no ICALL, ABI failure or validation
error. Also check:

- frames against D3D11 at the same steps (logos, Corn's talk, Garage, pause);
- the dialogue dwell (`C:\Users\logic\xbr-mac\talk-dwell.json` with `namecheck.py`: label 40/40);
- the N2 benchmark on both backends;
- the vertex-program miss count (0).

Record it in TR §31 and ledger L62.

### V8: the owner plays on Vulkan — DEFAULT SWITCHED (owner, 2026-10-16)

`play-jsrf.bat` now starts the Vulkan back end; `play-jsrf-d3d11.bat` (or `play-jsrf.bat d3d11`) keeps
D3D11 available as the reference. D3D11 is removed later. The key-script runner's default is unchanged.

Original step text:

Add a Vulkan option to `play-jsrf.bat` (`play-jsrf.bat vulkan`) and ask the owner to play. The default
backend switches only after the owner accepts; D3D11 is removed later.

### N4: Vulkan readback and synchronisation (after V7)

Readbacks take 202–211 s of a 560 s run, and V1 submits and fence-waits simply. Work V1's list in the
`nv2a_vk_backend.c` header:

- frames in flight and per-frame streams;
- one batched readback per flip;
- narrower barriers;
- sub-allocated memory;
- a persisted pipeline cache.

**N4.1 is independently accepted at game `389a03a` / toolkit `f41c0d2`, after repairing the
failed-wait lifetime defect** (TR §§31.29–31.31). A ring of
three submission slots, each owning its command recording, fence,
descriptor pool and 16 MiB upload stream; a submission is issued without waiting for the ones before
it; a draw entry splits a frame once the recording slot holds 2 MiB of stream or 32 descriptor sets
and the next slot is free; the host reaches a fence only to reuse an occupied slot, to publish guest
memory (a flip's readback, a target flush or release) or on a device error. A different worker's gate
`jsrf_vk_frames_in_flight` drives the real back end over 15 frames (241 981 batches, 1:1 with the
back end's own draw counter), checks every frame's guest bytes against the CPU executor, and fails if
the ring did not really overlap; it fails on both deciding mutants (a restored unconditional wait,
and reuse without observing the fence, which segfaults). N2 on the same RTX 4070: **43.40 presents/s
median against 33.48 before** (frame p50 22.4 ms against 29.9 ms). Full CTest 87/87 and `just check`
are green. Still open in N4: one batched readback per flip without the CPU convert on the critical
path, narrower barriers and layouts, sub-allocated memory, the persisted pipeline cache and R4.

**Review history, 2026-10-16 (TR §31.30):** a controlled failure-path reproduction showed
`vb_target_load` writes into a still-pending slot's stream after `vb_reserve` encounters a fence
timeout; its `g.ok` guard is after the write. The analogous texture-upload path needs the same
early-stop audit. The normal lifetime gate passes, but it does not inject that failed wait.
That candidate was rejected. A private early-guard control passed; the repair below closes the
confirmed defect. **Owner policy update (2026-10-16):** the Plan Orchestrator
may renew an exhausted repair allowance after independently confirming the review and recording its
own concrete repair advice. Those conditions are met for this finding (TR §31.30); one bounded
N4.1 repair packet is authorized. Keep prior decisions and spending, the 9 remaining operation slots,
all other limits and the existing stop conditions; no next N4 item before acceptance.

**Repair, 2026-10-16 (toolkit `f41c0d2`; TR §31.31).** The two upload paths now freeze before any
slot-resource access (`if (!g.ok) return;` immediately after their `vb_reserve`); `vb_stream` refuses a
slice outright once the failure latch is set, and its oversized-slice branch frees its own dedicated
buffer and returns NULL instead of handing back a pointer into it when the begin it performs failed
(found by independent review); a failed `vkBeginCommandBuffer` no longer marks the slot recording and a
dead recording buffer takes its place; a render pass is ended in the buffer that began it; and the
flush's barrier goes through `vb_cmd()`. A different worker's deciding regression
`jsrf_vk_ring_fault_gate` compiles the production bodies verbatim (no copy, build-time probe, no skip
path, no GPU needed) and injects the timeout and begin failures through the back end's own function
pointers: green on the repaired source (9 properties, 105 checks), red on `c5f3696` (properties 1, 2,
4, 5a, 5b, 6, 7), and red on three weakening mutants with the restored source green. Full CTest
**88/88**, `just check` passed, and a bounded e2e under N3 completed with `validation_errors=0`, the
known 10 unused-attribute warnings, `vsh_misses=0` and the loader's insertion line quoted. The 43.40
presents/s N2 figure above remains the `c5f3696` measurement: this repair changes failure paths only
and makes no new performance claim. Codex independently passed the actual fault/lifetime gates,
reproduced M1/M2/M3 failures and baseline/restored passes, and verified the final N3 archive's source
identity and loader witness. The next separate item is batched readback; no other N4 item is accepted.

Measure each with N2. Parity tests must stay green. Each code subitem gets a different worker's
gate and deciding mutation evidence.

#### R4: measure pipeline-cache pressure (part of N4)

Program pipelines are keyed by program hash and render state; `VB_MAX_PIPES` is 256. Read the
`pipelines=` values near the start, middle and end of the archived
`20261014-165814-809-keyscript-e2e-vk-v5r` run, and add an eviction counter to the stats line.
If e2e evicts pipelines, increase capacity and/or persist a `VkPipelineCache` as warranted by that
evidence; coordinate with N4's persisted-cache subitem. Measure changes with N2. The counter and any
cache change need a different worker's meaningful gate and quoted deciding mutation results.

### N6: Vulkan-only audit items carried over from jsrf-recomp (each with a test)

- The window clip as a scissor, not a refusal (character select).
- The alpha of a render target sampled as LIN_X8R8G8B8 must read 1 (their G54).
- The fixed-function infinite-light sign in the executor (`max(0, dot(N, L))`).
- Verify one depth surface per zeta.

### R5: reduce duplicated decode logic (low priority, after V7)

The input-mask rule is duplicated by the interpreter's `decode_program`, the host tool's
`derived_mask`, and `find-vertex-programs.py`'s `program_info`; the tool also applies Vulkan bindings
and locations through guarded text replacement. Prefer a Vulkan option in `nv2a_vsh_hlsl.c` that
emits decorations directly and shared production decode through `nv2a_vsh_program_info` or a shared
header for the host tool. Preserve semantics and avoid expanding into unrelated translation work.

**Gate:** a different worker verifies byte-identical corpus output before and after with
`gen-shaders.py --check`, plus the V6 conformance suite green. Quote a deciding decoration/decode
mutation that the gate rejects and a restored pass. This follow-up is pre-approved but low priority;
it is not a V7 acceptance prerequisite.

## Facts, tools and traps

- **Windows box:**
  - Vulkan SDK `C:\VulkanSDK\1.4.357.0` (`dxc`, `spirv-val`);
  - GPUs: RTX 4070 (Vulkan's default) and Intel Iris Xe (D3D11's default adapter);
  - lavapipe: `C:\Users\logic\tools\vk-sw\x64\lvp_icd.x86_64.json`, CMake cache `XBR_VK_SW_ICD`.
- **Elevated sessions:** the Vulkan loader ignores `VK_DRIVER_FILES` in an elevated process, so
  software-Vulkan tests skip there. Run CTest with `python C:\Users\logic\xbr-mac\ctest-limited.py`.
- **Known XFAIL:** a 16-bit target first cleared in a frame reads destination alpha 0, mirroring
  D3D11 (`nv2a_vk_backend.c:1096`). Fix it only on evidence of a visible effect.
- **Known flake:** `xbox_guest_meter` is timing-sensitive; rerun it once before treating it as a
  failure.
- **Provenance:** after editing a provenance input (e.g. `config/recovered-functions.json`), make the
  working file CRLF (`del` + `git checkout --`) **before**
  `check-generation-provenance.py --write`.
- **Screen detectors are backend-sensitive:** check any new detector against archived frames from
  both backends. The game mode (`obj(9)+0x5C`) can't tell the title, attract demo and menu apart; all
  read `0xF`.
- **Bridge and plan mode:** the OMP bridge can't enter interactive plan mode, and a separate planner
  is not allowed without an owner exception. Hence N1.
- **Commits:** no commit signing, and no co-author or signature trailers.

## Working loop

1. Take the next step above; read its source and evidence; keep to its gate.
2. Make the smallest useful change in the owning repository. Run `just build`, the focused tests,
   CTest via `ctest-limited.py`, and `just check`.
3. Run the game with the direct key driver:

   ```powershell
   python -X utf8 scripts/key-script-run.py e2e-vk 560 config/key-scripts/e2e.json RECOMP_GPU_BACKEND=vulkan
   python -X utf8 scripts/key-script-run.py e2e-d3d 560 config/key-scripts/e2e.json
   ```

   Runs are exploratory and use disposable saves. Inspect `result.json`, `metadata.json`, the logs and
   the frames in `logs/runs/`; a completed script alone proves nothing visual. One game run at a time;
   never kill a process you didn't start.
4. List the relied-on ledger IDs in the run record. If reading a dump, run
   `scripts/check-dump-mapping.py` first.
5. Record findings in the TR and ledger, update this plan's Done/Remaining lists, then commit and push
   (toolkit first). Finish with clean, synchronized trees.
