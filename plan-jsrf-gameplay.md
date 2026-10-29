# Jet Set Radio Future: playable opening area, then a native Vulkan build

**Status: the sole execution plan** (owner, 2026-10-14). Completed plans are retired to git history
(title screen and keyboard input: `git show 3f414ba:<file>`). `AGENTS.md` owns engineering
operations; `docs/jsrf-run-profiles.md` owns evidence classification;
`docs/jsrf-technical-record.md` (TR) owns established findings; `docs/jsrf-compatibility-ledger.md`
owns shortcuts; `config/stop-chain.json` owns stops.

## Current state (as of 2026-10-28)

- D4 (BUMPENVMAP modes 6/7, then texgen and texture matrices, then records) is done (TR §31.84).
- The xboxrecomp v0.16.0 catch-up is done (TR §31.83); serial guest mode (L34) stays as the default
  until Q6b retires it (the upstream guest-CPU gate now reaches the title; see the 2026-10-28 Q6 line).
- Strict smoke, Vulkan e2e and D3D11 e2e pass on the v0.16.0 tree (icall 0, fatal 0).
- 2026-10-26, Q1: no upstream change (`upstream/main` is still v0.16.0 `f0f8a46`; 10 open upstream
  PRs, #265-#274, to recheck next week; #267 and #268/#269 touch owner-kept designs). The nworkers
  `rejsrf` toolkit branch is unchanged; reJSRF v0.0.53-54 and rexbox v0.0.14-16 are build/locale/Linux
  only.
- 2026-10-26, Q2 (N6a): the window clip is a scissor (toolkit `88fedc4`, game gate `c0c4d61`, TR
  §31.85, L84).
- 2026-10-27, Q3 (N6b): an aliased LIN_X8R8G8B8 render target reads alpha 1 (toolkit `1df8bb7`,
  game gate `0d606cf`, TR §31.86, L85).
- 2026-10-27, Q4 (N6c): one depth surface per zeta is pinned by a test; production was right; the
  CPU fixed-function rasteriser has no depth test (TR §31.87, L86).
- 2026-10-27, Q5a: fixed-function lighting takes the normal from the inverse model-view rows,
  normalizes it only when enabled, and normalizes an infinite light's direction (toolkit `e10087e`,
  game gate `f8caed0`, controls `a3bf182`, TR §31.88, L82).
- 2026-10-28, Q5b stages 1 and 2 are done (gate `636a808`, toolkit `d02ed03`, controls `cd3dd38`; TR
  §§31.89-31.90, L87). Stage 2 routes fixed-function oD1 (lit gives (0,0,0,
  attribute-4 alpha), SPECULAR_ENABLE off gives (0,0,0,1), non-separate specular passes attribute 4)
  and the two stage-1 reds are green. Correction (TR §31.90): the stage-1 pf derivation is pinned to
  xemu `478b4f49`, which xemu `5c516ff0` (2026-10-17) supersedes with a ratio form and a 10000 clamp;
  no committed literal depends on it. Full CTest is all green again (293/293, none skipped).
- 2026-10-28, Q5b stage 1b: the admit-unknown e2e `20261028-122913-801-keyscript-e2e-vk-admit` met no
  unadmitted method, so JSRF never sent `SET_SPECULAR_PARAMS` on that route; Q5b is closed with the pf
  term ledgered (TR §31.90, L87).
- 2026-10-28, Q13: tooling debts closed (game `58ea0ca`, toolkit `1c2096d`): `pbcov-mutants.py` takes
  the ICD from the cache, the runner-preflight fakes carry the save roots, `check-merge-structure.py`
  ignores an inherited `GIT_INDEX_FILE` (a real `git commit -- <paths>` passed the hook), and the
  guest-meter serial tests wait for their condition (100/100 under load).
- Toolkit `9e10a2e` and game `939e362` are published (receipts at the end of TR §31.93).
- 2026-10-28, Q7: `0x1688F3` is an internal label named by a coincidental data word; the fatal trap
  stays and no code changes (TR §31.91, L65).
- 2026-10-28, Q11 (R5): one vertex-program input-mask rule shared by the interpreter and the host tool,
  `find-vertex-programs.py` pinned to it by a test (toolkit `ebf62bb`, game `8a6cf39`, TR §31.92). The
  corpus program bytes are unchanged.
- 2026-10-28, Q6: the upstream guest-CPU gate reaches JSRF's title. Two defects fixed: recovered and
  relifted code had no back-edge hooks (gate `7c626e2`, fix `f239648`), and upstream's lock deadlocked
  on `NtSuspendThread` (gate `36b1122`, fix toolkit `9e10a2e`). Exploratory Vulkan e2e with
  `RECOMP_DISPATCH_GATE=1 RECOMP_GUEST_LOCK=1` completes all 71 steps (TR §31.93, L34, L88). Serial
  mode is NOT retired.
- The next item is Q6b: retire serial mode (L34).
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
- Try upstream's guest-CPU lock plus gate, then retire serial mode (L34). Serial mode stays the default
  until the acceptance runs in Q6b pass.
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
- **Q3 (N6b):** an aliased LIN_X8R8G8B8 render target reads alpha 1 (TR §31.86, L85).
- **Q4 (N6c):** one depth surface per zeta, pinned (TR §31.87, L86).
- **Q5a:** fixed-function lighting normal and light direction (TR §31.88, L82).
- **Q5b stages 1-2:** fixed-function specular gate and oD1 routing; the pf term is ledgered (TR §§31.89-31.90, L87).
- **Q11 (R5):** one vertex-program input-mask rule (TR §31.92).
- **Q6:** the upstream guest-CPU gate reaches the title; back-edge hooks in recovered code and the
  `NtSuspendThread` lock deadlock fixed (TR §31.93, L34, L88).

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
| Q6b | **Retire serial mode (L34).** Owner direction already given ("try upstream, then retire ours"). Run a D3D11 e2e and a strict run with `RECOMP_DISPATCH_GATE=1 RECOMP_GUEST_LOCK=1`, compare frames with the serial-mode runs, then make the gate and lock the default and delete serial mode (`RECOMP_GUEST_SERIAL`) together with Q12's deletions (including `RECOMP_BACKEDGE`). | 1–2 | D3D11 e2e and strict smoke with the gate: icall 0, fatal 0, frames match the serial runs; full CTest and `just tk-tests` green | any acceptance run fails (stop; do not delete serial mode) |
| Q8 | **Is the texgen effect visible?** Capture the JSRF NORMAL_MAP stage-0 batches (`RECOMP_PB_COVERAGE=1`) and compare one scene with xemu (`scripts/xemu-oracle.py`). No fidelity claim without xemu. **[owner-assisted]** | 1 | viewed frames | xemu needs owner input |
| Q9 | **Optional D4 step 8: an in-game boost/attract-demo comparison against xemu.** Boost needs spray cans, so it needs a route script or the owner playing. **[owner-assisted]** | 1–2 | viewed frames | always |
| Q10 | **D5: plan the reJSRF-derived items** that the D3/D4 evidence shows apply (TR §31.55–31.56 table): ADXM exclusion, lighting gaps. Each becomes its own gated item. | planning | owner approval of the item list | always (it's a planning step) |
| Q12 | **Divergence clean-up after the next regeneration**: delete `RECOMP_BACKEDGE`, `xbox_inb/outb`, `usb_gamepad_output` and the `xbox_VblankAckHeld` stub (see the playbook's divergence inventory). `RECOMP_BACKEDGE` deletion now goes with Q6b, which removes serial mode. | 1 | build, toolkit tests, full CTest, strict smoke | anything still references them at runtime |

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
