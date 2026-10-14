# Jet Set Radio Future: playable opening area

**Status: current execution authority** (owner, 2026-10-11).
`plan-jsrf-title-screen.md` and `plan-jsrf-keyboard-input.md` retain the completed
title/input history. `AGENTS.md` owns engineering operations;
`docs/jsrf-run-profiles.md` owns evidence classification;
`docs/jsrf-technical-record.md` (TR) owns established findings;
`docs/jsrf-compatibility-ledger.md` owns shortcuts; `config/stop-chain.json` owns stops.

## Objective

Reach a useful playable opening-area slice: correct enough Garage/world rendering,
a visible player, movement, turning, jumping and camera response. Later work expands
to graffiti, audio and save/resume; those are not prerequisites for the next rendering
checkpoint and are not claimed complete here.

## Current state

Verified at game `f842c6f` and toolkit `4f8a880` on 2026-10-13 (the toolkit commit for V3 and the game
commit for its tests and records are added below this section as they land). Check with `git status -sb`
in `C:\Users\logic\Repos\poison-jam` and `C:\Users\logic\Repos\xboxrecomp`.

- **Reachable:** M15, K16, the Garage, movement, Corn's talk, **Gum's tutorial** (TR §31.9,
  `config/key-scripts/gum-tutorial.json`) and the **pause menu** (stops 50–52 plus a 25-function
  batch, TR §31.10).
  - Owner-run acceptance: `config/key-scripts/e2e.json` (intro, skate, jump ×2, pause, resume,
    skate, jump ×2).
  - An Xbox One controller works through XInput (toolkit `e365e04`).
- **Fixed from andeecollard/jsrf-recomp:** the 16-bit clear colour, i.e. the Smilebit card (TR §31.11,
  toolkit `e7c2efb`), and **the dialogue fence** (toolkit `5569b1d`, ledger L17).
  - The dialogue fence fix: "Corn" label in 40/40 snapshots (was 1/40), with an A/B through
    `RECOMP_FENCE_MIRROR_LIVE=1`.
  - The owner confirmed both in play.
- **Dropped:** gamma. The logo fades already render (TR §31.12).
- **Shaders:** all 128 vertex programs are static XBE data, and the static words give HLSL identical
  to live and valid SPIR-V (TR §31.12, `scripts/find-vertex-programs.py`).
- **The Vulkan backend** (`RECOMP_GPU_BACKEND=vulkan`, toolkit `src/video/nv2a_vk_backend.c`, ledger
  L62, TR §31.13) has V0–V3 done:
  - build-time SPIR-V (`xbr_add_spirv`);
  - lavapipe for tests;
  - the backend core;
  - parity tests `jsrf_gpu_backend_parity_vk`;
  - **V3: the combiner uber-shader** (`src/video/shaders/nv2a_vk_rc.hlsl`, TR §31.14). One pixel shader
    compiled ahead of time interprets the `Nv2aCombiner` registers from a per-draw uniform buffer
    statement for statement with `nv2a_rc_eval` — general stages, final combiner, fog and all four
    texture stages. No per-configuration generation and no runtime compile.

  Combiner-shaded surfaces (characters, part of the city) now render through it. Vertex programs still
  run on the CPU (V5).
- **Owner direction (2026-10-13)** — read it before choosing work:
  - **compile-time code over live translation;**
  - **Vulkan, for a future Android port;**
  - **no new D3D11 work** (D3D11 stays only as the parity reference).

  Then the library lift (D3D8, then DirectSound) and decompilation, after Phase V.

## Current rendering and gameplay checkpoint

- **Face culling is implemented and tested.** Disabled, FRONT, BACK, ALL and
  CW/CCW selection pass controlled WARP pixel tests on fixed-function, GPU-program
  and CPU-transformed program paths. Inverting the production mapping fails both
  focused CTests. Same-target transitions and partial clears are covered.
- **The dark player silhouette is resolved in the measured Garage view.**
  `20261011-221941-413-keyscript-culling-skate` shows colored clothing/skates and
  a localized shadow against the verified stencil baseline. A complete frame in
  `20261011-222515-360-keyscript-culling-trace` confirms the character outline
  uses FRONT/CW while the colored body uses BACK/CW, with matching bound D3D11 states.
- **Shadow volumes produce GPU samples.** The paired FRONT/BACK passes bind
  matching INCR/DECR stencil state, followed by a stencil-EQUAL darkening pass
  (TR §31.3). Shadows improved; hardware fidelity and stencil-value readback are
  unproven. The old zero CPU-pixel counter did not establish absent GPU output.
- **The alpha-tested quad family produces output.** A later complete frame after
  skating records 1,139 passing samples from its texture/primitive family. That
  draw uses alpha ref 0; the historical ref-204 draw was off screen and remains
  unclosed. There is no measured alpha-test defect to fix (TR §31.4).
- **The full skate path completes:** `20261011-223618-932-keyscript-culling-skate-fixed`
  executes all 71 steps in 302 s and continues presenting through the 480 s bound.
  Forward motion, both turns, jump/grind behavior, backward-input movement and
  camera following are visible in pre/post frames. The next measured blocker,
  truncated recovery owner `0x000FFFD0`, was repaired in `9b0af4e` and returned
  with its ABI verified. No further fatal/ABI blocker was observed in this run.

- **The HUD spray-can counter read zero on every observed Garage frame (observation).**
  Packet 001 measured the counter band at exactly 140 white pixels ("00") on all 1954
  gameplay frames it captured, with 296 further frames at 0 that are menus and fades; no
  non-zero counter value was observed. Those frames all precede Gum's lesson, which no run
  obtained, so the zero counter says nothing about later tutorial steps, can pickups or
  spray targets. The talk-script text and `e000.dat` naming Shibuya Terminal are hypotheses
  about where graffiti starts, not measurements (TR §§31.5, 31.6).

- **Gum's assets are resident; her NPC instance and activation state remain unidentified (Packet
  001-FIX2).** The run loads Stg00 assets; `Gum.bin`/`Gum.dat` are opened by the game and her model is
  resident in guest memory, and all four tutorial talk scripts are loaded. Corn's live talk slot
  is `0` outside his talk and holds his script during it, while a slot holds the TE002
  pointer constantly — a resident table entry, not an activation state. A ten-burst forward walk
  moved for seven frames and then stopped against a wall in an alley, and no second talkable NPC
  was located on the sampled paths. That is "not located on sampled paths", not proven absence:
  the paths were open-loop wall-clock movement with no camera steering and no guest coordinate
  read (TR §31.7).

## Immediate next task: an 8-hour plan (handoff, 2026-10-13)

The goal of this block is to finish **Phase V**: JSRF playable on Vulkan with **no shader compiled at
run time**, matching D3D11, then the owner plays it. Work in this order. Each code step gets a test
written by a **different** worker, and every step is gated before the next.

**Rules that apply to every step:**

- One game run at a time; never kill a process you didn't start.
- Commit and push the toolkit first, then the game.
- Generated shader output is **never** committed (`src/recomp/gen/` style, gitignored).

### 1. V3: the combiner uber-shader (about 2.5 h, needs the strongest model)

One pixel shader, compiled ahead of time with `xbr_add_spirv`. It interprets the `Nv2aCombiner`
registers from a uniform buffer, statement for statement with `nv2a_rc_eval`
(`src/kernel/nv2a_combiner.c:97`): general stages, final combiner, fog, and all four stages'
textures.

- **No per-configuration shader generation and no run-time compile.** If it's too slow, specialise
  it through SPIR-V specialization constants at pipeline creation.
- **What V1's author says V3 needs** (`src/video/nv2a_vk_backend.c`):
  - `VbDraw` grows to 4 textures and samplers (`vb_texture()` already returns the view, layout and
    texel scale);
  - a combiner vertex format equal to D3D11's 92-byte `GbRcVertex`, built from `b->extra`;
  - the about-400-byte combiner registers in a stream-backed UBO with a dynamic offset (add UNIFORM
    usage, count it in `vb_reserve`, enlarge the descriptor pool);
  - a pipeline-kind field in the key;
  - `rc_unsupported` → `rc_draws`.
- **The reference for semantics:** the D3D11 generator `gb_rc_*` in
  `src/video/nv2a_d3d11_backend.c` (around line 1558 onward). Port its meaning, not its generation.
- **Gate:** build, CTest, then `e2e.json` with `RECOMP_GPU_BACKEND=vulkan` on the RTX 4070. Garage
  characters must be shaded like the D3D11 run `logs/runs/20261013-010916-545-keyscript-e2e-f2`, and
  the run must reach the Garage with the stock script; the city colours fooled the menu detector
  before.
- **DONE 2026-10-13** (TR §31.14, ledger L62): the shader, the UBO, the four stages, the pipeline-kind
  field and `rc_draws`/`rc_fallback` are in; the deciding tests were written by a different worker.
  Build, full CTest (68/68) and `just check` pass. E2E `logs/runs/20261013-185536-693-keyscript-e2e-vk-v3`
  on the RTX 4070: 0 ICALL, 0 fatal, 0 validation errors, 12,473 presents against D3D11's 11,042,
  `rc_draws=1692697 rc_fallback=0`, and the real main menu renders correctly with the title characters
  cel-shaded. **The Garage gate is NOT met**: the stock script stops at step 1 because
  `key-script-run.py`'s menu detector accepted the attract city frame at 601 orange pixels against a
  600 threshold — the pre-existing defect this plan's step 3 owns (the pre-V3 Vulkan run failed the
  same step at 665, so V3 widened the margin rather than causing it). Step 3 must land before this gate
  can be re-run.

### 2. V4: combiner tests (about 1 h, routine, different worker)

Randomised combiner states and inputs on lavapipe: uber-shader pixels against the `nv2a_rc_eval` CPU
reference within ±1 LSB. Extend `tests/test_gpu_backend_parity_vk.c`'s XFAIL combiner case into a
real gate. Mutations: a wrong combiner op or a swapped input mapping must fail.

### 3. Key-script menu detector (about 0.5 h, routine)

`scripts/key-script-run.py` `is_menu` (orange-band test near line 699) false-fires on a wrong-colour
city frame. Make it robust: for example, require the menu's text/logo structure as well as the
orange. Add a test with a city frame that has orange and no menu. Its own test is fine here; it's
tooling. Gate: stock `e2e.json` reaches the Garage on both backends.

### 4. V5: vertex programs from the static corpus (about 1.5 h, routine; the design is decided)

1. A new generator script in `scripts/` (to create, e.g. `gen-shaders`) reuses `find-vertex-programs.py`'s extraction (128 programs, 126
   distinct).
2. It runs `nv2a_vsh_hlsl` (built as a host tool from `src/video/nv2a_vsh_hlsl.c`), then
   `dxc -spirv -T vs_6_0`.
3. It writes a C table keyed by the FNV hash, checked with length plus a digest of the words.
4. `just shaders` → gitignored `src/recomp/gen/shaders/`, linked into the exe.
5. `draw_program` on Vulkan looks up the table; a **miss returns 0**, so the executor's CPU
   interpreter draws it, plus one logged hash. **No live compile.**

**Vulkan notes from TR §31.12:** y via `-fvk-invert-y` (keep V1's y/winding convention: surface
pixel = framebuffer pixel); `SV_ClipDistance0` needs `shaderClipDistance`; emit `[[vk::location(n)]]`
so the location equals the attribute index.

**Also from V1's author:** INDEX usage on the stream; `input_mask` in the pipeline key; the 192×float4
constants (3 KB) per draw in a stream-backed UBO.

**Gate:** the e2e stats line shows 0 misses.

### 5. V6: vertex-program tests (about 1 h, routine, different worker)

- Offline HLSL is byte-identical to the live `nv2a_vsh_hlsl` output for synthetic V1–V7-style
  programs.
- A table hit matches the CPU interpreter's pixels on lavapipe.
- A miss falls back.
- A corrupt digest is rejected.
- The mutations fail.

### 6. V7: acceptance (about 0.5 h, judgement)

`e2e.json` on Vulkan (RTX 4070). It must complete with no ICALL, ABI failure or validation error.
Check:

- frame comparison against D3D11 at the same steps (logos, Corn's talk, Garage, pause);
- the dialogue dwell (`C:\Users\logic\xbr-mac\talk-dwell.json` with `namecheck.py`: label 40/40);
- presents per second against D3D11;
- the vertex-program miss count.

Record it in TR §31.13 and ledger L62.

### 7. V8: the owner plays on Vulkan (about 0.25 h)

Add a Vulkan option to `play-jsrf.bat` (e.g. `play-jsrf.bat vulkan`, or a `RECOMP_GPU_BACKEND`
passthrough) and ask the owner to play. The default switches only after the owner accepts.

### 8. If time remains (about 0.75 h)

Apply the jsrf-recomp audit items to the **Vulkan** backend only (no D3D11 changes), each with a
test:

- the window clip as a scissor, not a refusal (character select);
- one depth surface per zeta (already true; verify);
- the alpha of a render target sampled as LIN_X8R8G8B8 must read 1 (their G54);
- the fixed-function infinite-light sign in the executor (`max(0, dot(N, L))`).

### Facts, tools and traps

- **Windows box:** Vulkan SDK `C:\VulkanSDK\1.4.357.0`; GPUs RTX 4070 (Vulkan's default) and Intel
  Iris Xe (D3D11's default adapter).
- **lavapipe:** `C:\Users\logic\tools\vk-sw\x64\lvp_icd.x86_64.json`, CMake cache
  `XBR_VK_SW_ICD`. The Vulkan loader ignores `VK_DRIVER_FILES` in an **elevated** process, so
  software-ICD tests skip there. Run them non-elevated, or with
  `python C:\Users\logic\xbr-mac\ctest-limited.py [ctest args]`.
- **Known XFAIL:** a 16-bit target first cleared in a frame reads destination alpha 0 (it mirrors
  D3D11, `nv2a_vk_backend.c:1096`). Fix it only with evidence of a visible effect.
- **`xbox_guest_meter`** is a known timing flake; rerun it once before treating it as a failure.
- **Provenance and line endings:** after editing a provenance input (`config/recovered-functions.json`
  etc.), make sure the working file is CRLF (`del` + `git checkout --`) **before**
  `check-generation-provenance.py --write`, or a clean checkout fails `--check`.
- **Ahead-of-time direction:** prefer ahead-of-time designs; anything compiled or translated at run
  time needs an owner decision.
- **Stop and ask the owner** for force pushes, assets or saves, an environment failing twice, or a
  shortcut that changes what a milestone means.

## Working loop

1. Plan before substantive engineering. Read current source and evidence, choose one
   measured problem and its acceptance checks, then adapt when evidence changes.
2. Make the smallest useful change in the owning repository. Build with `just build`;
   run focused tests and applicable repository checks against the final tree.
3. Use the direct Windows key driver for D3D11 gameplay (collector limitation, TR §29):

   ```powershell
   python -X utf8 scripts/key-script-run.py culling-skate 480 config/key-scripts/skate.json
   python -X utf8 scripts/key-script-run.py menu-check 300 config/key-scripts/menu-nav.json
   ```

   These runs are exploratory and use disposable per-run saves. Inspect archived
   `result.json`, `metadata.json`, source identity, logs and before/after frames in
   `logs/runs/`. A completed script alone does not prove the requested visual effect.
4. List relied-on compatibility IDs in the run record (title/input/GPU path includes
   L16, L18, L47, L56–L60; consult the ledger for the actual run). Apply the run-profile
   policy; exploratory results do not prove fidelity. If reading a dump, first run
   `scripts/check-dump-mapping.py` and use actual guest VAs.
5. Record the measured effect and earliest remaining blocker here and in the appropriate
   technical record/ledger. Preserve original assets/saves. Commit and push verified
   work, toolkit first when both repos changed; finish with clean synchronized trees.

## Completed checkpoint and next session

The opening area is playable on D3D11, with correct text and the pause menu, through the Gum tutorial.
Phase V has a working Vulkan core with parity tests and, since V3, ahead-of-time register combiners.
The next session continues the 8-hour plan at step 3 (the key-script menu detector), which blocks the
V3 Garage gate; V4 follows. V3 started from game `f842c6f` and toolkit `4f8a880`.
