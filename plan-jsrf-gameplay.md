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

- **R1: a stale shader corpus is refused at build time** (TR §31.22). The runtime identity check
  proves the guest words, not which translator or compiler produced their SPIR-V, so a translator or
  dxc change after `just shaders` used to link wrong pixels silently. The generator now records a
  provenance block (schema, the three translator source hashes, the tool hash and args, the
  `dxc --version` string and dxc args, the corpus counts) into both generated files, and
  `scripts/vsh_aot_provenance.py`'s single `check_corpus()` sequence is what both the build guard and
  the fixtures call. `scripts/build-jsrf.py` recomputes and refuses a mismatch before configure,
  naming `just shaders`; there is no bypass and no auto-regeneration. `tool.sha256` is recorded but
  advisory — measured, the MSVC tool executable is not reproducible (three clean rebuilds of
  identical sources gave three hashes), so a decisive tool hash would refuse a corpus `just shaders`
  had just produced; tool freshness is enforced at generation time by mtime instead. `just check` now
  runs the REAL `check-vsh-aot-table.py` (`entries=126 recompiled_and_matched=126 xbe_distinct=126
  problems=0`). Gate: a whitespace content edit to `nv2a_vsh_hlsl.c` in a scratch tree makes the build
  refuse naming the field and `just shaders`; offline regeneration clears it. A different worker wrote
  the 68-test regression, mutation-controlled (15 weakening edits each detected, one per guard step).
  Full CTest 84/84, `just check` passed. Test-only: toolkit untouched at `5668278`.

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

### R1: detect a stale shader corpus — DONE (TR §31.22; see Done above)

The runtime identity check proves the guest words, not which translator or compiler produced their
SPIR-V. Synthetic table CTests do not validate the real generated corpus.

- Record provenance in generated `nv2a_vsh_programs.h`/`.c`: SHA-256 of toolkit
  `nv2a_vsh_hlsl.c`, `nv2a_vsh_hlsl.h`, `nv2a_vsh_hlsl_tool.c` and the host tool itself, plus the
  `dxc --version` string and dxc arguments.
- Have `scripts/build-jsrf.py` or CMake configure recompute that provenance and refuse a mismatch,
  naming `just shaders`. Never silently link stale shaders.
- Run `check-vsh-aot-table.py` against the **real** generated table in `just check`, with its real
  XBE/compiler inputs; retain device-free synthetic tests in CTest. Correct CMake's comment that
  currently implies those synthetic tests alone guard the real corpus.
- Before generation, rebuild the host tool or prove it is newer than all three source inputs,
  including a tool selected through an override or an existing build directory.

**Gate:** a whitespace content edit to `nv2a_vsh_hlsl.c` in a scratch copy makes the build refuse
with `just shaders`; regeneration clears the refusal. A different worker tests matching provenance
(accepted) and mismatched provenance (rejected). Quote the stale-input mutation, refusal and restored
pass; preserve original assets and the ignored retail corpus.

### R2: an empty corpus when the Vulkan SDK is absent

Vulkan is optional. If Vulkan or dxc is unavailable, generate an empty table at configure time
(`nv2a_vsh_program_count = 0`) and print that status; program batches use CPU interpretation.
With Vulkan and dxc available, a missing corpus remains a fatal error naming `just shaders`.

**Gate:** a scripted configure with `VULKAN_SDK` unset, Vulkan/dxc unavailable and no corpus succeeds
and reports the empty corpus; a configure with Vulkan available and no corpus still refuses with
`just shaders`. A different worker writes both configure tests. Quote the deciding branch mutations
and positive controls. Run these in scratch configurations; do not remove the owner's local corpus.

### R3: include culled program batches in miss accounting

Move `vb_program_lookup` above the cull-all early return in `vb_draw_program`, so a program used only
in fully culled draws is still included in corpus coverage. Count the lookup outcome, then return 1
for cull-all even on a miss: both GPU and CPU paths draw nothing. Document that return-value choice.

**Gate:** a different worker tests a cull-all batch whose program is absent from the corpus:
`misses` advances by one and nothing draws. Quote a mutation that restores the old ordering and
makes the new gate fail, then the restored pass. R1–R3 must land before V7.

### N2: a controlled benchmark (about 0.5 h)

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

### V7: acceptance (about 0.5 h)

`e2e.json` on Vulkan (RTX 4070), under N3. It must complete with no ICALL, ABI failure or validation
error. Also check:

- frames against D3D11 at the same steps (logos, Corn's talk, Garage, pause);
- the dialogue dwell (`C:\Users\logic\xbr-mac\talk-dwell.json` with `namecheck.py`: label 40/40);
- the N2 benchmark on both backends;
- the vertex-program miss count (0).

Record it in TR §31 and ledger L62.

### V8: the owner plays on Vulkan (about 0.25 h)

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
