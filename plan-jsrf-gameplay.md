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

This session fetched and verified clean, synchronized game
`2e1d6323c274b098184cb26cb065ece6551073e6` and toolkit
`4e302def8472fe17425bf28de117675a91a550b0`. Culling is now pushed as toolkit
`8e200100949ffb2b253a4ac7884c06a9d71d8713`; production-path tests are game
`5bb8c839a468968aaf0d8785bd6935e2fb694a23` (TR §31.3).

- **M15 complete and reproduced:** the real title with `PLEASE PRESS START TO BEGIN`
  (TR §26.6; title plan's two accepting runs).
- **K16 complete:** live keyboard Enter reaches guest Start and opens the real main
  menu (`keyscript-k16-r3b`, `keyscript-newgame-2`; TR §31). Keyboard Z / Xbox A
  selects NEW GAME and advances dialogue. Gameplay and the HUD are reachable
  (`keyscript-intro-1`). Garage movement is now observed below; K17's broader
  menu-direction/back acceptance remains separate.
- **The Garage black-world defect is fixed:** stale unordered SSE compares repeatedly
  rearmed the warp fade; APU DSP physical-address translation overwrote stage vertex
  buffers. Targeted relifts and toolkit `293a97b` restored floor, walls, fences and
  skyline (`20261011-180827-975-keyscript-groundfix1`; TR §31.1).
- **Stencil is implemented in the NV2A executor and D3D11 backend:** toolkit
  `4e302de` carries stencil state, D24S8 storage, operations/masks/ref and clears.
  The exploratory run `20261011-194754-768-keyscript-stencil1` shows a substantially
  brighter Garage after stencil gates the fullscreen shadow darkening. Its result
  records script completion and zero ICALL/fatal/walk-rejection counts; its source
  identity is toolkit `d37f47d` plus the archived patch later committed as `4e302de`.
  This is measured rendering improvement, not a fidelity or fully correct shadow claim.
- **Gameplay automation is committed:** `config/key-scripts/skate.json` enters the
  game and exercises forward/backward movement, turns and jump; `menu-nav.json`
  exercises menu directions, accept and back. Script contents establish available
  actions, not acceptance of every movement, camera or menu behavior.

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

## Immediate next task

The rendering/movement checkpoint is reached on the exploratory path. Extend the
opening-area automation to exercise graffiti, audio and save/resume, then record
the earliest actual failure. Those behaviors remain unvalidated, rather than
known renderer defects. No separate lighting/combiner feature is justified by
the measured Garage view. Keep the historical off-screen ref-204 quad and
eye-plane-crossing/hardware shadow fidelity as explicit coverage limits.

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

Culling has deciding production-path tests and gameplay evidence. Player visibility,
movement, turning, jumping and camera response are supported by observations;
the adjacent ABI stop is repaired and runtime-confirmed. This does not establish
strict fidelity or complete graffiti/audio/save acceptance. Start the next session
from the pushed baseline and measure those remaining opening-area behaviors.
