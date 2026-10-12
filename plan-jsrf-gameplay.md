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
  (`keyscript-intro-1`). K17's full directional/back acceptance is still unproven.
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
- **The historical alpha-tested quad remains unclosed, not a proven defect.**
  Its old transformed vertices were off screen; it was absent from the new complete
  frame. Do not choose an alpha-test fix from this evidence.
- Forward movement, left/right turns, an airborne jump pose and camera following
  are visible. Backward movement was skipped after an ABI abort at `0x000FFFD0`.

## Immediate next task

Run the full unchanged `skate.json` on game `9b0af4e`, which restores the truncated
`0x000FFFD0` effect method through its original `RET 4` epilogues and removes eight
false internal entries. Focused recovery checks/build passed; runtime return and
backward movement still need confirmation. Record the earliest remaining blocker
from that run; do not reopen stencil or choose a lighting/combiner feature from
appearance alone.

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

## Natural checkpoint for the next session

Culling implemented, focused test green, gameplay runtime confirming its effect,
the next blocker measured, and both repositories pushed and clean. Stop there if the
next defect is a separate substantial problem. Do not claim the opening-area slice
complete until player visibility, movement, turning, jumping and camera response are
each supported by runtime observations.
