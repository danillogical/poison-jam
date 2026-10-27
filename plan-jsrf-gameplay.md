# Jet Set Radio Future: playable opening area, then a native Vulkan build

**Status: the sole execution plan** (owner, 2026-10-14). Completed plans are retired to git history
(title screen and keyboard input: `git show 3f414ba:<file>`). `AGENTS.md` owns engineering
operations; `docs/jsrf-run-profiles.md` owns evidence classification;
`docs/jsrf-technical-record.md` (TR) owns established findings; `docs/jsrf-compatibility-ledger.md`
owns shortcuts; `config/stop-chain.json` owns stops.

## Current state (as of 2026-10-26)

- Toolkit `cf185d0` and the game (the 2026-10-26 handoff commits, from `a3eecf6` to the commit that
  introduced this plan layout) are published; the last `PUSHED_TO` receipts are in TR §31.84.
- D4 (BUMPENVMAP modes 6/7, then texgen and texture matrices, then records) is done (TR §31.84).
- The xboxrecomp v0.16.0 catch-up is done (TR §31.83); serial guest mode (L34) stays because the
  upstream guest-CPU gate fails to reach the title.
- Strict smoke, Vulkan e2e and D3D11 e2e pass on the v0.16.0 tree (icall 0, fatal 0).
- 2026-10-26, Q1: no upstream change (`upstream/main` is still v0.16.0 `f0f8a46`; 10 open upstream
  PRs, #265-#274, to recheck next week; #267 and #268/#269 touch owner-kept designs). The nworkers
  `rejsrf` toolkit branch is unchanged; reJSRF v0.0.53-54 and rexbox v0.0.14-16 are build/locale/Linux
  only.
- 2026-10-26, Q2 (N6a): the window clip is a scissor (toolkit `88fedc4`, game gate `c0c4d61`, TR
  §31.85, L84); local, not pushed.
- The next item is Q3, LIN_X8R8G8B8 alpha.
- Older dated status paragraphs and the pre-approved R1-R5/N-series text moved to
  `docs/jsrf-operating-history.md`, section "Plan history moved on 2026-10-26".

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

**Decisions of 2026-10-24 (weekly upstream merges):**

- Prefer upstream's design in merge conflicts; re-express ours as a small separable delta.
- Keep the fork's model vblank, the kmem arena, the APU IRQ path, APC deferral default-on (NT ordering)
  and the keyboard packet moving only on change.
- Keep serial mode (L34), because upstream's guest-CPU lock plus gate fails to reach the title.
- The full list and the divergence inventory are in `docs/upstream-merge-playbook.md`.

## Done

Milestone names with their TR sections; details are in the TR.

- **M15 and K16:** title and live keyboard input (TR §26 onward).
- **The Garage renders:** fade, floor, stencil, culling, characters, shadows (TR §§31.1-31.4).
- **Gum's tutorial reached** by closed-loop `goto` steering (TR §§31.8-31.9).
- **The pause menu works** (stops 50-52, omitted-function batch; TR §31.10).
- **Ports from andeecollard/jsrf-recomp:** 16-bit clear colour and dialogue fence (TR §31.11, L17).
- **Gamma ramp dropped; all 128 vertex programs are static XBE data** (TR §31.12).
- **Vulkan backend V0-V4:** build-time SPIR-V, core, parity tests, combiner uber-shader (TR §§31.13-31.16, L62).
- **V5:** static vertex-program corpus (TR §31.19); **V5 repair** (TR §31.20).
- **V6:** vertex-program conformance gate (TR §31.21), with its pixel-predicate repair.
- **R1:** stale shader corpus detection (TR §§31.22-31.24).
- **R2:** empty corpus without the Vulkan toolchain (TR §31.25).
- **R3:** culled batches in miss accounting (TR §31.26).
- **N2:** controlled Garage benchmark (TR §31.27).
- **V7:** Vulkan acceptance under the validation layer (TR §31.28).
- **V8:** Vulkan is the `play-jsrf.bat` default (owner, 2026-10-16).
- **N4.1:** frames in flight (TR §§31.29-31.31).
- **N4.2:** batched readback with a compute pack (TR §§31.33-31.37).
- **N4.3:** narrower barriers and layouts (TR §§31.38-31.39).
- **N4.4:** sub-allocated memory (TR §§31.40-31.43).
- **N4 and R4:** persisted pipeline cache and pressure measurement (TR §§31.42-31.44).
- **C3 and C4:** v0.14.0 regeneration checkpoint and merge/recovery acceptance (TR §§31.45-31.54).
- **D1 and D2:** reJSRF toolkit fixes ported, no regeneration delta (TR §31.55).
- **D3:** coverage instrumentation and gate (TR §§31.56-31.62).
- **Portability preflight and POSIX handle safety** (TR §§31.63-31.73).
- **D4 steps 4-5:** BUMPENVMAP modes 6/7 and their committed-stream gate (TR §§31.75-31.82).
- **xboxrecomp v0.16.0 catch-up** (TR §31.83).
- **D4 steps 6-7:** texgen and texture matrices, closing records (TR §31.84, L75-L83).
- **Q2 (N6a):** the window clip is a scissor (TR §31.85, L84).

## Work queue (after the 2026-10-26 handoff)

Take the first unblocked item; an item marked **[stronger model or owner]** or **[owner-assisted]**
blocks the queue (report it and stop, don't skip past it). Each item fits one session and ends in a
local commit. Push only with
the owner's confirmation, after `just audit`.

**Gate discipline for every code item.**
1. Write the gate before reading the implementation's reasoning. Working alone, that means: write
   and commit the test first (it must fail on the current code), then implement in a later session
   without editing the test.
2. Use literal expected values with their derivation written in a comment. Two renderers agreeing
   decides nothing.
3. Old-red: build the new test against the previous toolkit commit in a private clone, and require
   failures on NAMED assertions. A compile failure or a skip is not evidence. Recipe (scratch outside
   both repos and outside `%TEMP%`, e.g. `C:\Users\logic\xbr-mac\<name>`):
   - `git clone --no-hardlinks C:\Users\logic\Repos\xboxrecomp <scratch>\tk`, then
     `git -C <scratch>\tk checkout <previous-commit>`;
   - `cmake -S . -B <scratch>\build -DCMAKE_BUILD_TYPE=Release -DXBOXRECOMP_DIR=<scratch>\tk
     -DBUILD_TESTING=ON -DJSRF_FIXTURES_ONLY=ON -DXBR_VK_SW_ICD=<XBR_VK_SW_ICD from build\CMakeCache.txt>`
     from the game root, then `cmake --build <scratch>\build --config Release --target <test targets>`;
   - CPU cases: `ctest --test-dir <scratch>\build -C Release -R ^<prefix> --output-on-failure`;
     Vulkan cases: `python -X utf8 scripts\ctest-limited.py --build-dir <scratch>\build -R ^<prefix>`;
   - if the test needs a struct field the old toolkit lacks, keep the new header and check out only
     the old behaviour files (`git -C <scratch>\tk checkout <previous-commit> -- <files>`), as TR
     §31.82 did; then delete `<scratch>`.
4. Add at least one private mutation control: an entry in the `CONTROLS` table of
   `scripts/bumpenv-q-controls.py` (exact text edits, the cases that must go RED and their named
   assertions; `--control` choices come from the table). `just controls <name>` must go RED, then
   GREEN.
5. Run `just build`, `just check`, the new tests and the full `just ctest-limited
   --output-on-failure`, all green with none skipped (the test count grows as tests are added; any
   failure or skip is the gate, not the count).
6. A genuine production defect found by a gate: commit the failing test, record the defect in the
   plan's Current state and the session summary, and stop for the owner.

Items marked **[stronger model or owner]** need hardware-rule derivation or a design decision. A
cheap agent should not start them; it should report that they are next and stop.

| # | Item | Size | Gate | Stop and ask if |
|---|---|---|---|---|
| Q1 | **Weekly upstream merge**, following `docs/upstream-merge-playbook.md`. If `HEAD..upstream/main` is empty, add a dated "no upstream change" line to Current state and take the next item. | 1–2 sessions | the playbook's gates | any playbook stop condition |
| Q3 | **N6b: a render target sampled as LIN_X8R8G8B8 reads alpha 1** (jsrf-recomp G54). Toolkit decoder / Vulkan sampler format. | 1 | CPU and Vulkan fixture literals with alpha 0xFF; old-red | the CPU decoder disagrees with Vulkan before the change |
| Q4 | **N6c: one depth surface per zeta.** Add a test proving the Vulkan back end keys depth by zeta address (L62). | 1 | a fixture with two zetas; a mutation control that shares them | production is wrong (report it, don't fix it) |
| Q5 | **N6d / lighting: the fixed-function infinite-light sign** (`max(0, dot(N, L))`) **and the `lit_color` normal** (TR §31.84: it uses the model-view, not the inverse, always normalizes and back-face flips, L82). **[stronger model or owner]** | 1–2 | xemu `vsh-ff.c` lighting rules, a literal gate like the texgen gate | always: the rules need derivation |
| Q6 | **The upstream guest-CPU gate hangs at boot** (TR §31.83: `KeInsertQueueDpc`/`KeSetEvent` spin, no title in 300 s). Diagnose it before serial mode (L34) can retire. **[stronger model or owner]** | 1–2 | an exploratory e2e reaching the title with `RECOMP_DISPATCH_GATE=1 RECOMP_GUEST_LOCK=1` | always |
| Q7 | **The last fatal alias `0x1688F3`** (L65): a reviewed `config/recovered-functions.json` entry with disassembly evidence, or translator clipping. | 1 | regen inventory shows it direct; full CTest; strict smoke | it is reached at runtime before the fix (then it's urgent) |
| Q8 | **Is the texgen effect visible?** Capture the JSRF NORMAL_MAP stage-0 batches (`RECOMP_PB_COVERAGE=1`) and compare one scene with xemu (`scripts/xemu-oracle.py`). No fidelity claim without xemu. **[owner-assisted]** | 1 | viewed frames | xemu needs owner input |
| Q9 | **Optional D4 step 8: an in-game boost/attract-demo comparison against xemu.** Boost needs spray cans, so it needs a route script or the owner playing. **[owner-assisted]** | 1–2 | viewed frames | always |
| Q10 | **D5: plan the reJSRF-derived items** that the D3/D4 evidence shows apply (TR §31.55–31.56 table): ADXM exclusion, lighting gaps. Each becomes its own gated item. | planning | owner approval of the item list | always (it's a planning step) |
| Q11 | **R5: reduce duplicated vertex-program decode logic** (`decode_program`, `derived_mask`, `find-vertex-programs.py` `program_info`). Low priority. | 1 | `gen-shaders.py --check` byte-identical corpus, V6 conformance green, a decode mutation rejected | the corpus bytes change |
| Q12 | **Divergence clean-up after the next regeneration**: delete `RECOMP_BACKEDGE`, `xbox_inb/outb`, `usb_gamepad_output` and the `xbox_VblankAckHeld` stub (see the playbook's divergence inventory). | 1 | build, toolkit tests, full CTest, strict smoke | anything still references them at runtime |
| Q13 | **Small tooling debts**: `scripts/pbcov-mutants.py:66` hard-codes the ICD path (use the cache's `XBR_VK_SW_ICD`); the 3 pre-existing `RunnerPreflightTests` failures in `tests/test_run_profiles.py` (fake runner log lacks `[SAVE] udata_root`); the pre-commit hook's `GIT_INDEX_FILE` leaks into `check-merge-structure.py` when `git commit -- <paths>` is used (until fixed, stage with `git add`, then `git commit -F`); toolkit `xbox_guest_meter` (`tests/kernel_guest_cpu`) fails intermittently right after a full `just tk-tests` run ("the yield came back before the worker had run", TR §31.85): make it load-tolerant or list it in `tests/FORK-STATUS.md`. | 1 | the affected tests green | — |

## Facts, tools and traps

- **Windows box:**
  - Vulkan SDK `C:\VulkanSDK\1.4.357.0` (`dxc`, `spirv-val`);
  - GPUs: RTX 4070 (Vulkan's default) and Intel Iris Xe (D3D11's default adapter);
  - lavapipe: `C:\Users\logic\tools\vk-sw\x64\lvp_icd.x86_64.json`, CMake cache `XBR_VK_SW_ICD`.
- **Elevated sessions:** the Vulkan loader ignores `VK_DRIVER_FILES` in an elevated process, so
  software-Vulkan tests skip there. Run CTest with `just ctest-limited`; run other commands
  un-elevated with `just limited -- <cmd>` (`AGENTS.md` lists the host-tool recipes).
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
- **Commits:** no commit signing, and no co-author or signature trailers.
  Stage with `git add <paths>`, then `git commit -F <msgfile>`.

## Working loop

1. Take the first unblocked item of the Work queue; read its source and evidence; keep to its gate.
2. Make the smallest useful change in the owning repository. Run `just build`, the focused tests,
   `just ctest-limited --output-on-failure`, and `just check`.
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
5. Record findings in the TR and ledger, update this plan's Current state, Done and Work queue, then
   commit. Push only with the owner's confirmation, after `just audit` (toolkit first). Finish with
   clean trees.
