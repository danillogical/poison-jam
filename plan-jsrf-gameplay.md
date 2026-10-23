# Jet Set Radio Future: playable opening area, then a native Vulkan build

**Status: the sole execution plan** (owner, 2026-10-14). Completed plans are retired to git history
(title screen and keyboard input: `git show 3f414ba:<file>`). `AGENTS.md` owns engineering
operations; `docs/jsrf-run-profiles.md` owns evidence classification;
`docs/jsrf-technical-record.md` (TR) owns established findings; `docs/jsrf-compatibility-ledger.md`
owns shortcuts; `config/stop-chain.json` owns stops.

**Owner resume and portability preflight (2026-10-22):** the owner resumed after the D4
step-3 pause and ordered Windows validation of the macOS changes first. The independent
read-only review accepted the portability subset with conditions (TR §31.63–31.64). The
bounded repair is now in: the M-A anchor, `RECOMP_VK_DRIVER` classification, grouped honest
exclusion messages, the suballoc debt relabel, the POSIX cache documentation, and a
different-author `JSRF_FIXTURES_ONLY=ON` gate. Codex rejected the first candidate for a
G1 corpus-binding hole and a fixtures-only disclosure gap; the repair now derives the
compiled corpus from the real configured conformance project, adds the cleared-corpus
mutant, and announces the exclusions in the branch fixtures-only actually executes
(TR §31.65). Codex independently verified the repaired binding gate and accepted the
preflight test/disclosure repair (TR §31.66). Full CTest **162/162, zero skipped,
677.54 s**; the final announcement-only correction also passed the guarded build and
the complete G1 gate. The bounded Windows test-debt repair (TR §31.67) now corrects the
allocator padding/dedicated oracles, applies the shared-scratch RESOURCE_LOCK on Windows and
removes the two parity warnings; a controlled 1792-byte allocator case decides the old forms
while the real llvmpipe/NVIDIA workloads report zero padding/dedicated. Codex rejected the
first candidate because the controlled gate held copies rather than the workload predicates;
the repaired test-only `tests/vk_suballoc_oracles.h` is now called by both the real workload
and the controlled gate. Private header mutations recompile both consumers and fail the
controlled gate; the zero-padding/dedicated llvmpipe workload still passes those mutations. Codex
independently accepted the bound test-debt repair (TR §31.69). The
MoltenVK suballoc skip remains explicit debt (historical 1792 is still an inference), and CTest
locks do not coordinate separate ctest processes. **POSIX handle safety repaired (TR §31.71):**
the interrupted preflight's defect is closed — a live-handle registry validates provenance before any
dereference, pins are atomic with the liveness decision, and every successful public close drops
exactly one lifetime reference (the draft's release-only-on-final-close leak is corrected). A
different author's native gate decides it: plain 175/0 and ASAN 22/0 on that shim, with bypass,
no-withdraw and early-release mutants red (the last only under LeakSanitizer) and the original
committed source red on both required rows. **Codex then rejected that candidate for a measured
regression (TR §31.72):** `WaitForMultipleObjects(Ex)` rejected the supported `(HANDLE)-2`/
`(HANDLE)-1` sentinels with `WAIT_FAILED`/error 0 (the `NULL` controls passed) and did not resolve the calling thread's own value
after its public close, because the gates never called a multi-object wait at all. The bounded repair
classifies sentinels explicitly, routes ordinary values through the same self-aware `thread_pin` the
single wait uses, and releases every pin on every exit; the extended gate is plain **251/0** and ASAN
**44/0** on the repaired shim against **21 FAIL** on the pre-repair source, with sentinel
fall-through, `obj_pin`-for-self and skipped-release controls each decisive. `jsrf_inplace_event_bridge`
stays unregistered off Windows as explicit **unvalidated POSIX integration**: the measured blocker is
a dependency-limited WSL host (35 undefined references against the SDL2/XAudio2 POSIX providers), and
a Mac is untested rather than claimed to fail; the stale crash reason is corrected in
`CMakeLists.txt` and the macOS doc, and the fixture's own header-validation case has still not been
executed on any POSIX host. Windows: guarded build, six targeted event/DPC/callback/save/fence runs each a
single `-R` pattern (no alternation), full limited CTest **162/162 in 524.32 s** with the full log
retained, and `just check` green; the bounded strict 8 s Vulkan smoke carries because the Windows
executable and every Windows-relevant input are byte-unchanged (the repaired file is POSIX-only). The MoltenVK suballoc skip remains explicit
debt (historical 1792 is still an inference), and CTest locks do not coordinate separate ctest
processes. Codex independently accepted the repaired POSIX checkpoint (TR §31.73), including actual
final-source native gates and private negative controls. The provider block that stopped the first
step-4 attempt (TR §31.74) was cleared by the owner's explicit selection of
`workbuddy-ai/deepseek-v4.1-flash-sg` for every global role. **D4 step 4 now has an implementation
candidate (TR §31.75):** combiner texture modes 6 BUMPENVMAP and 7 BUMPENVMAP_LUM are implemented
independently in the CPU executor (`rc_stage_fetch`) and in the ahead-of-time Vulkan uber-shader
(`fetch`), with the bump matrix/scale/offset and `SET_SHADER_OTHER_STAGE_INPUT` admitted, shadowed
and transported per draw, stage 0's discarded words left at their D3D default, and the shared
refusal rule moved from `> 5` to `> 7` so modes 8..31 and stage-3 cube still fall back. Windows:
guarded build, full limited CTest **162/162 in 518.59 s** with the full log retained, `just check`
green including the real corpus re-derivation, 22/22 mutation mutants decided by their named
assertions, and **three** bounded 8 s strict Vulkan smokes with pipeline cache off and an isolated
save root — two with the executor registered, the third on the committed tree with empty patch
hashes — plus one discarded first attempt whose save-root override was a self-inflicted invocation
error. No validation layer was inserted, so no validation claim is made.

**Codex rejected that candidate** (`soleSTOPdec_0mvk3pb0h4c42082ca8a9d576`) on two demonstrated
defects, and a bounded repair landed (TR §31.76, report
`logs/reports/d4-step4-q-binding-repair-2026-10-22.md`). (1) The bump modes divided the stage
coordinate by texture q before adding the delta. The selected reference initialises `pT` from `vtxT`
with no prior divide, projects only in PROJECT2D, and has the bump modes sample `pT.xy + dsdt`; that
is a per-mode rule, and the earlier reading of it as an internal inconsistency was wrong. Measured
against the actual CPU translation unit, q=2 selected a different texel from q=1. The delta is now
added to the incoming xy in both the CPU executor and the HLSL, with modes 1/2/3/4/5 unchanged. (2)
The stage-0 texture bind had widened from `m > 3` to `m > 7`, doing image and sampler work for
PASSTHRU and CLIPPLANE; it is now the exact sampled-mode set `{1,2,3,6,7}`. A different-author
focused gate (`tests/test_bumpenv_q_cpu.c`, `tests/test_bumpenv_q_vk.c`,
`scripts/bumpenv-q-controls.py`) decides both with hand-derived texel expectations: 8/8 GREEN,
bump selection independent of q in the CPU seam and in the actual AOT Vulkan path, PROJECT2D still
projecting so a global divide deletion cannot pass, a production binding witness for modes 4/5 versus
1/2/3/6/7, and two private controls that each go RED with named assertions and return GREEN. Primary
review also caught a stale-object hazard in that gate: the `#include`d production files were not
declared dependencies of the test objects, so a restored private build could run a mutated object;
`OBJECT_DEPENDS` now states them, the harness forces a clean rebuild before every build and refuses a
verdict when consecutive builds are byte-identical, and its scratch directory moved out of `%TEMP%`
where MSBuild's `MSB8029` warns incremental builds are unreliable.
Windows: guarded build, full limited CTest **170/170 in 550.19 s**, `just check` green including the
real corpus re-derivation, and one 8 s strict smoke with the executor registered that reaches no bump
batch. Two of those checks first failed for reasons outside the production change and are recorded:
G1 stages tracked files only, so the new tests had to be staged; `xbox_guest_meter` flaked once on its
own lock timing and is green in isolation and in the clean run.

**Step 5 is still a separate, differently-authored gate**, and is not started: an independent formula
oracle over the committed stream, the full bump matrix/source/luminance/format coverage, CPU/Vulkan
pixel agreement where the perturbed texel is visible in the readback, and old-red/final-green. Signed
(snorm) bump inputs, cube/volume bump stages and rect-texture remap remain open, and modes 8..31 and
stage-3 cube still fall back. No in-game bump correction is claimed: the smokes say nothing about the
title's own bump state. This repair admission consumed one carried normal slot and the single granted
repair allowance, so the D4 allowance was then **one normal operation and zero repair cycles**, with no
slot refunded or added. No D4 or D completion. *Correction (TR §31.78): that grant came from Codex's
skill-confirmed review of an independently reproduced defect, not from a new human repair-packet
instruction; the earlier wording here said otherwise.*

**Codex review of that repair (TR §31.77):** the production q/binding fixes pass
Codex's freshly rebuilt actual-CPU probe and all eight focused Windows tests,
including llvmpipe pixels and binding-state witnesses. The candidate is still
rejected for two new test-tool defects: the CPU fixture is registered off Windows
but explicitly emits a non-Windows `#error` (reproduced with WSL GCC), and the
manual control helper deletes a pre-existing scratch child without ownership
(reproduced using a harmless Codex-created sentinel). Sole STOP decision
`dec_0mvkahpy93eec0dd14c7fb9a8` closes operation `op_0mvk3sc0ufb39663d99cd2f3a`;
the concrete bounded repair advice is in
`logs/reports/d4-step4-fixture-safety-codex-review-2026-10-22.md`. The next packet
repairs only these fixture/tool issues, preserving production and local history.
The skill-confirmed NEW review renews one exhausted repair allowance and carries
the one remaining normal slot, adding no operation slot. Its admission spends
both: cumulative D4 **10 operations / 5 historical repair allowances**, and
**zero normal slots / zero repair cycles** remain. Full step 5 remains pending and
cannot silently take a fresh normal allowance. No feature publication or
completion.

**Fixture/tool repair landed (TR §31.78, report
`logs/reports/d4-step4-fixture-safety-repair-2026-10-22.md`):** the CPU fixture's
artificial non-Windows `#error` is gone and all five cases now compile, link and
pass from a real Linux binary against the same production `rc_stage_fetch`; the
only real host dependency was the toolkit's `platform` shim, linked by one
`if(NOT WIN32)` line. The control harness now refuses an existing scratch root,
validates resolved containment before creating anything, records an ownership
marker and directory identity, and removes only a root it created — the earlier
run's own `toolkit-<control>` child can no longer be deleted. Both new regressions
fail against the pre-repair code and pass now; the focused Windows 8/8, the full
limited CTest **172/172**, `just check` and two fresh-scratch controls (each RED on
named assertions then GREEN) all pass on the settled tree. Full step 5 is still a
separate, differently-authored gate and is not started; the strict smoke was
carried on verified identical runtime identity and says nothing about bump
sampling.

**Codex review of the fixture/tool repair (TR §31.79):** actual native CPU
compilation and all ten focused CTests pass (10/10 in10.66s), and the original
existing-root deletion is fixed. The candidate is still rejected: after the
owned root is replaced, final release refuses cleanup but `force_rebuild` checks
only lexical containment and deletes a replacement directory's sentinel. Codex
reproduced this sequentially with disposable data; a private early ownership
guard preserves it. Sole STOP `dec_0mvkd0r9590a81f646c06f28d` closes
`op_0mvkao1vnf743041d9de6007a`. One further narrowly bounded cleanup-check repair
is permitted by the skill's confirmed-review rule, renewing only one exhausted
operation slot and one repair allowance; its admission yields cumulative D4
**11 operations / 6 historical repair allowances, zero normal slots / zero repair
cycles**. Full step 5 remains pending; its normal allowance is not renewed.

**Forced-cleanup ownership repaired (TR §31.80, report
`logs/reports/d4-step4-rebuild-ownership-repair-2026-10-22.md`):** the deciding path
from §31.79 is closed in the existing helper, with no production change. Before the
first removal, `force_rebuild` now requires a supplied ownership proof (`owned=None`
is a named refusal, not a bypass), requires `_still_owned()` to hold now — the
replaced-identity/changed-marker case root release already refused — and validates
every exact deletion target, so a `*.dir` that is a symlink or junction is refused
rather than followed and each target must resolve inside the valid owned root. All
targets are planned and validated before any `rmtree`/`unlink`, a removal `OSError`
becomes a `HarnessError` that keeps its path and message, and a genuinely owned fresh
root still gets the same recursive `*.dir` removals, exe/SPIR-V unlinks and identical
printout. The sequential invariant is stated, not race freedom. The existing
ownership module now has fifteen cases: the replaced-root/foreign-marker/sentinel,
missing-proof and redirected-junction cases fail against the pre-repair helper on
outcome (the replacement's sentinel, exe and `.dir` were all gone) and pass now,
the junction case is not skipped on this host, and the eleven earlier cases and a
normal fresh-owned control are retained. Windows: focused 10/10 in 11.16 s with the
real llvmpipe device and WSL compile/link witnesses, both private controls RED on
named assertions then GREEN from their own fresh owned roots with distinct
baseline/mutated/restored hashes and byte-identical shared toolkit hashes, guarded
build 59 targets / 172 tests, full limited CTest **172/172 in 531.64 s** on the
final tree (564.95 s on the same source before the last prose edit), `just check`
green with `entries=126 recompiled_and_matched=126`. The strict smoke is carried on
matching runtime identity (`jsrf_recomp.exe ca550e78…`, `jsrf_collect.exe cb90b7e3…`,
`game/default.xbe fd190557…`); it says nothing about bump sampling. The portability
test's own prose now states what it actually does — it compiles parsed source lists
with a supplied `platform` source and does not execute a non-Windows CMake graph, so
that branch remains source-inspected. Full step 5 is still a separate,
differently-authored gate and is not started, and its normal allowance is not renewed.

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
  72 ambiguous fatal targets remain after C3 regeneration in `config/recovery-unresolved.json`; repair each when a run
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

### Current execution amendment: upstream and reJSRF cleanup (owner, 2026-10-18)

N4 is finished and pushed (toolkit `6c3cd84`, game `699bc46`). The owner resumed the
approved A1–D5 cleanup directly on this Windows laptop; this supersedes the earlier
stop after N4 and the cleanup report's Mac/scp workflow. The reports in ignored
`logs/reports/` are session evidence, not additional files to publish.

Execute the remaining cleanup in this order, without a new planning phase for the
already approved steps:

1. **C3 — DONE (TR §31.54):** finish the v0.14.0 regeneration checkpoint on toolkit
   `merge-v0.14.0` (`ee2e0cd`): guarded build and warning review, refreshed generation
   provenance and baseline, `just check`, full software-Vulkan CTest through the
   limited-rights wrapper, then the bounded cache-off Vulkan e2e smoke. Classify
   any earliest fatal/ICALL against original XBE instructions; never suppress it.
   Static gates now pass: build, `just check`, full limited-rights CTest 99/99 and
   independent mutation controls (TR §31.45). **The smoke failed before the first
   frame**, in original AC'97 reset loop `0x1A6F88`; no ICALL/fatal is not a pass.
   **C3-RR diagnosis is the next bounded operation:** compare original XBE and
   old/current native code, establish the reset model/ownership contract, and
   return a minimal source-backed repair proposal and independently authored gate
   design. No production edits or C4 work in that diagnostic operation. The old
   compiler omitted this loop; the regenerated backedge preserves it. Preserve
   TR §1's AC'97 model ruling and L20; do not bypass the loop or enable a synthetic
   readiness/field pin. Authorize any subsequent model repair as its own packet.
   The first diagnostic report confirms the cause but its repair proposal is
   **not accepted** (TR §31.46): handler ordering is reversed, its reset-bit test
   contradicts the cited contract, and its writable-page window can miss a
   concurrent reset. The corrected report removes that writable window, but
   remains **rejected** (TR §31.47): it invents four additional channel controls,
   omits observable reset state, and locks individual callbacks rather than a
   whole decoded transaction. Codex authorized one bounded C3-RR implementation
   packet under its own corrected contract: four source-backed channels,
   register-visible reset effects, inaccessible Windows mapping with private
   state, whole-transaction serialization, preserved L20 and POSIX behavior,
   and a different worker's behavioral/mutation gate. Preserve existing A2H
   regression coverage. A newly exposed DMA/audio stop is a new horizon to
   classify, never permission to manufacture progress. No C4 or push yet.
   **C3-RR: accepted widths repaired and independently reviewed (TR §§31.50–31.51);
   C3 still unaccepted.** Codex's probe showed an accepted eight-byte store was
   corrupted: `1122334455667788` read back `5566778855667788` while the 89-check
   gate passed, because the write callback narrowed the value to 32 bits before
   the byte extraction (shifts ≥ 32 were undefined). The 2026-10-19 qword repair
   carries `uint64_t` through every accepted width (1/2/4/8) with defined shifts,
   keeps all eight bytes in ordinary storage, extends the BDBAR dword rule to a
   wider write's first dword, and moves the `[A3A]` witness snapshot/once-flag
   under the leaf lock with the print/flush after release. A different author's
   gate grew to 132 checks (ONE CTest case): real faulting qword store/load with
   nonmatching words, separate byte/dword/word readback, an independent 64-bit
   read assembled from two 32-bit stores, edge/zero/ones values, neighbour
   isolation, narrower-width positives and qword GC/GS overlap/L20. The historical
   `c395db3` source fails the deciding row as a value assertion
   (`expected 0x1122334455667788, got 0x5566778855667788`); final and restored pass
   132/0/0/0; truncation, repeated-low-word, zero-high-word and read-narrow mutants
   each fail a named predicate with 0 harness errors and skips, and the refreshed
   prior controls still decide their own rows (M11 remains a crash, not a value
   predicate). The configured reviewer was invoked through the supported per-task
   `agent: reviewer` selection; its native session records `modelRole: slow`,
   `workbuddy-ai/deepseek-v4.1-flash:max`, `resolvedModelIsFallback: false`,
   thinking `max`, and it found no patch defect. An earlier mis-dispatched generic
   task review was cancelled and not counted. Final guarded build (0 compile/link
   errors, 0 C4310, 18 known MSB8028), `just check`, and the full limited-rights
   CTest **100/100 in 574 s** pass. The toolkit's shared decoder keeps its factored
   probe (effective start/width/bounds validated before any service; the
   `mod == 0` SIB-base-5 disp32 fixed; register-only, segment/LOCK and unsupported
   forms refused without side effects) and the Windows NABM owner keeps the 4 KB
   page permanently inaccessible with private state under one leaf lock, models
   the register-visible channel reset for PI/PO/MC/SO with the pinned source's SR
   read-only/W1C, LVI five-bit and BDBAR `~3` rules, and keeps `GLOB_CNT`/
   `GLOB_STA` as plain storage except the L20 derived bit. POSIX: the changed unit
   adds no POSIX error and the guards remove exactly the two implicit-declaration
   errors; the file retains 41 pre-existing Windows-shim errors on both HEAD and
   final (unrelated, recorded). The stock Vulkan smoke
   (`20261019-032447-223-keyscript-c3-rr-qword-e2e-vk`, exe `83823e5c…`, 11.8 s)
   stops on the unchanged separately classified horizon: `[ICALL] invalid target
   0x00700010 ... return=00193EB7` with `[AC97] serviced=40 refused=0` and the
   L20 witness live, 0 frames, exit `0xE0424943`. The qword repair did not cause
   and does not fix that ICALL. Original-XBE bytes place the site at
   `0x193EB5 call eax` (`eax = [esi+0x1c4]` at `0x193E62`) in the code block at
   `0x193E40`, reached by `jmp` from the body entered at `0x193D90` in the XBE's
   D3D section; the target is in the runtime's fake guest RW-data area
   (`FAKE_RWDATA_VA` 0x00700000), not code. What installed that pointer, and
   whether the halted SR state changes this D3D8 path, are UNPROVEN; no SR=0
   ablation was built. Register/reset/width repair is not whole-C3 acceptance.
   C3/C4 remain unaccepted; the next decision is Codex's.
   **C3-ICALL diagnosis is done (TR §31.52), read-only.** The called field is static
   BSS in the D3D section (`esi=0x0019D468` is past the section's file-backed size),
   the containing `0x193D90` is a recovered function whose body is byte-identical
   before and after regeneration, and the call site discards the ordinal-145 return
   value. The qword repair cannot be the cause; the stop is co-extensive with the
   AC'97 `ac97m_` toolkit patch but that correlation spans archives differing in more
   than one variable. **What wrote `0x00700010` into `0x0019D62C` is UNPROVEN** (no
   ICALL archive ships a dump). Next instrument: a hardware write breakpoint on the
   host mapping of guest `0x0019D62C` against the archived exe/pdb, or a `peek` chain
   (`["0x0019D468","0x1C4"]`) to localise the write to a step interval. No repair, no
   C4 and no push until that writer is named.
   **The writer of the field is now named (TR §31.53).** One bounded exploratory run
   (`20261019-213447-432-a2h-slotw-writer`, the same executable `83823e5c…` as the failing
   smoke) used the toolkit's shipped `JSRF_TRACE_A2H_SLOTW` slot-write watch, which armed on
   `slot=0019D62C` across all 29 views and recorded a single slot hit on tid 13564:
   `pre=00000000 -> post=00700010`, `range=GAME_MODULE`, native RIP `0x7FF6BE1CB68D` =
   `sub_0018CE30 + 0x5D`, which is `0x0018CE3A mov dword ptr [ecx+0x242c], eax` — the only
   store to that field in the XBE and in the generated tree, executed with `eax=0x00700010`,
   through the canonical alias, with a coherent terminal read. **UNPROVEN and now the next
   question: what supplied `0x00700010`.** The setter's only static caller (`sub_0015F9E0`,
   via `sub_00012210`) pushes 0, the indirect dispatch route was not observed, and the value's
   origin remains open. Absence claims are invalid in that archive (`concurrent_overlap=15`),
   and `[AC97] serviced=7` here vs `40` earlier means it is not comparable on the AC'97
   variable. The store is a legitimate setter, so this is a bad value, not yet a bad store;
   C3 and C4 remain UNACCEPTED and no repair is authorized.
   **C3 accepted (owner-directed takeover, 2026-10-20; TR §31.54).** The value came from a lift
   defect, not the game: the regenerated adjustor thunk `sub_00169000` lost its `jmp 0x168230`, so the
   guest stack stayed 8 bytes low and `sub_0005F350` popped the caller's `edi` (`0x00700010`) into the
   `ebx` that `sub_00012210` hands the setter. Toolkit `35c6537` makes a jump to a target with its own
   symbol a tail call; `7544264` makes every other untranslatable jump a fatal named trap and refuses
   to fold an alias onto a later owner. Game `141c23a` applies the independent C4 review's repairs,
   `1211173` keeps automated runs' saves in their private root, and `3638f80`/`ba47e8a` gate it all.
2. **C4 — DONE (TR §31.54):** independent merge/recovery review, Vulkan e2e with both N3 loader insertion
   witnesses, D3D11 reference e2e and viewed logos/Corn/Garage/pause frames, N2 with
   a same-session archived-executable baseline, then TR/L02/L62/AGENTS records.
   Fast-forward toolkit main only after acceptance and audited pushes: toolkit
   origin first, then game origin.
3. **D1 — DONE (TR §31.55):** port the MIT reJSRF toolkit discovery fixes `58f7166`, `0c32a78`,
   `982cd28`, `7c58ec6`, `6a38c01`, explaining any part already covered by upstream
   `34f5e1c`; retain attribution in commit bodies. Skip the performance-only ports.
4. **D2 — DONE, no delta (TR §31.55):** regenerate and classify only D1's deltas, with C3 and C4 gates.
5. **D3–D4:** triage reJSRF's game-side fixes against our LLE executor and Vulkan,
   then gather the required scene evidence on both backends and view the frames.
   **Coverage prerequisite (2026-10-21, TR §§31.56–31.57):** the first local
   instrumentation candidate was rejected: stage-1-only texgen/matrix use gave
   false zeros and a first-batch delivery mask changed after publication. A local,
   unpushed repair candidate now covers all four stages, freezes per-word
   availability with each first record and keeps live delivery state separate; it
   closes the reproduced stage-1 and retention cases. A further local, unpushed
   repair now ties vertex-program word availability to the accepted upload slot and
   component and freezes the selected start with the claim (TR §31.58). Codex
   independently reproduced the repaired cases and source-matched smoke identity.
   A different-author committed-stream and real-Vulkan gate now exists
   (TR §31.59): game `576fa7af39b13c27c134128f07233297416f5016`, test-only, 56
   gate CTests and a 21-mutant private harness against the frozen toolkit. The
   rejected boundary (TR §31.60) is repaired in TR §31.61: all modes 0..5 at all
   four stages are drawn and decided per draw, the mode-5 mutant is in the
   private table (22 mutants), and the helper refuses existing work paths with
   owned-only cleanup plus a focused regression. Codex independently accepted the
   repaired gate (TR §31.61): focused 57/57, escaping mode-5 mutant now detected,
   old-helper negative control confirmed, full-suite 161/161 receipt verified.
   The accepted gate is pushed; step 3's four cache-off Vulkan coverage scripts
   have now run (TR §31.62): texgen/texture-matrix use is positive in all four,
   specular lighting is positive, and **no stage-mode>=6 batch or Vulkan rc
   fallback was observed**, so the handoff's replanning trigger applies. The
   conditional step-6 criterion is met but step 6 still waits for bump and its
   gate. Next is fixture-driven BUMPENVMAP (steps 4/5); in-game confirmation
   waits for the optional owner-assisted step 8. No BUMPENVMAP or texgen
   implementation is accepted yet.
6. **D5:** plan applicable items as separate gated steps, with a different test
   author, then records and audited pushes. New design is planned only here when
   the preceding evidence establishes what is applicable.

One step per OMP operation, one subagent and one game run at a time. N1's stop list,
unsigned commits without trailers, compile-time translation, preservation of assets
and saves, and no new D3D11 work remain in force. N6 and R5 follow this owner-directed
cleanup rather than interrupting it.

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

### N4: Vulkan readback and synchronisation — DONE (TR §§31.42–31.44)

Readbacks take 202–211 s of a 560 s run, and V1 submits and fence-waits simply. Work V1's list in the
`nv2a_vk_backend.c` header:

- frames in flight and per-frame streams;
- one batched readback per flip;
- narrower barriers;
- sub-allocated memory;
- a persisted pipeline cache.

**N4 is complete** (2026-10-18, TR §31.44). The last item, the persisted pipeline cache plus R4,
landed at toolkit `6c3cd84` / game gate `2253927`. The driver's builds of the ahead-of-time SPIR-V
persist in a per-device file and a corrupt or foreign file starts cold. R4 measured no pressure:
the e2e workload creates 41-42 pipelines against `VB_MAX_PIPES` 256, so `pipe_evictions` stays 0.
A warm cache gives no measurable gain in the Garage benchmark, because every pipeline is created
before the window.

**N4.4 independently accepted at toolkit `03aaae8` / game gate `e4cb724`** (2026-10-17,
TR §§31.42–31.43). The repair releases a never-referenced allocation at once, destroys every
object before its range goes back, and refuses (keeping ownership and stopping the back end)
instead of dropping when the park list is full. Its pre-repair source fails the gate (35 of 336
checks) and the repaired source passes (10 properties / 335 checks); full CTest 96/96. A review
by a different model than the implementer found no correctness defect; its three Low findings are
recorded as limits in §31.43. Next separate item: the persisted pipeline cache/R4, which completes N4.

**History: N4.4's acceptance was first blocked** (2026-10-17, TR §31.41).
Codex reproduced a rollback leak in toolkit `ecbaca4`: 65 failed image binds during an open
recording overflow the 64-entry parked-release list. After the real garbage drain all images
are destroyed, but 65,536 bytes remain owned by nobody; the dedicated variant frees only 64 of
65 allocations. Idle controls pass, and a private never-bound rollback control closes both
failures. The existing gate explicitly permits this leak, so its green result does not satisfy
the required leak-free rollback contract. The next operation must repair N4.4, with a different
worker's capacity/ownership gate and private negative controls, before any pipeline cache/R4,
N6 or R5 work. Last independently accepted checkpoint remains N4.3 (toolkit `5af1c48`, game
records `9fd8e05`). N2 also needs three matched baseline/final windows: observed timing drift
does not establish its cause, and the late comparison has only one baseline and two final
samples. Preserve the startup AVs as causally unresolved. See TR §31.41 for independent proof,
repair advice and the review-confirmed allowance accounting.

**N4.4 implementation report, superseded as acceptance by the review above** (TR §31.40; toolkit
`ecbaca4`, game gate `3f4e82f`). The back end no longer makes one `VkDeviceMemory` per resource:
a resource takes a RANGE out of a pool block, pooled by `(memoryTypeIndex, linear|non-linear)` so an
image and a buffer never share a block and `bufferImageGranularity` can never apply between
neighbours. Pooled classes: render targets, self-sample snapshots, depth images, textures (the churn
path), the 1x1 white image, the ring's slot streams, the standalone slice buffers and the readback
staging pair. Dedicated allocations remain with a counted reason each (`requires`/`prefers`
dedicated, a range over 64 MiB, a pool at its block or reserve budget, a full pool table, a
host-visible type without coherence) -- and note that the three 16 MiB ring streams land in their own
exact-size blocks, so the reduction comes from the image classes and the small slice buffers, not
from them. A range is reused only when nothing that could still read it is recording or in flight
(releases park in a bounded list and fold back after the drain); at most one empty block ≤ 4 MiB per
pool is retained; allocate/create/bind/map failures release exactly what they took;
flush/invalidate clip to the sub-allocation and widen to whole `nonCoherentAtomSize` atoms. Decided
by `jsrf_vk_suballoc_gate` (9 properties / 265 checks / 70 anchors, no skip, ten private mutants red
plus an orchestrator overlay) and the real-driver `jsrf_vk_suballoc_device` (byte-exact targets,
textures, depth and snapshot through image churn), with the retained ring/readback/caller/scope gates
migrated by a worker distinct from the production change. Independent code review found one real
defect (a double-counted free-range head made `padding_bytes` dishonest) plus a retention policy
that never fired and gate gaps -- all repaired; the security/native-lifetime review found no
High/Medium defect. Full CTest **96/96** and `just check` green on the frozen source; N2 shows no
established gain or regression (the change makes 10 `vkAllocateMemory` calls for 232 ranges on
the game's own 560 s workload; the same baseline exe measured 41.37 and 23.13 presents/s within
one session, but the cause of the drift is unresolved); the N3 bounded stock e2e ran with
`validation_errors=0` and the loader's own insertion line quoted. **The next separate operation is
the N4.4 ownership/capacity repair and matched measurement, then persisted pipeline cache/R4,
N6 and R5 in the approved order.** Nothing later than N4.4 was
started.

**N4.3 independently accepted at toolkit `5af1c48` / game gate `4e09019`, records `1b0e291`**
(2026-10-17, TR §§31.38–31.39). Every normal-path barrier in the Vulkan back end now carries the stage and access scopes its
producer and consumers actually need (`vb_dep`/`vb_image_dep`; `vb_image_barrier` is gone and all seven
layout transitions are scoped, with no layout VALUE changed), while two sites stay deliberately broad:
the per-submission barrier in `vb_slot_submit` and N4.2's abandonment barrier in
`vb_targets_flush`. The changed sites, their producer/consumer argument, the retained broad sites and
what is deliberately NOT claimed are in the packet's dependency argument, kept outside git with the
N4.3 receipts. Decided by two new gates authored by workers distinct from the production change
(`jsrf_vk_sync_scope_gate`: 115 hazard pairs decided, 0 unordered, 5 properties / 77 checks, 9 private
mutants; `jsrf_vk_sync_paths_gate`: real device, per-site mask census, reachability counters and exact
published bytes, 6 properties / 119 checks, 11 private mutants, plus a synchronization-validation run
with a mutant that makes the layer report hazards and a safe-but-broad mutant that does not), with the
retained ring and readback-ordering gates migrated by a different worker to the new helper and to the
abandonment barrier as the decider for the abandoned-group classes. Codex independently passed all
eight Vulkan gates, re-ran the private scope mutants, and verified a real synchronization-validation
negative control (four hazards on the unsafe source, none on final source). The full suite had one
documented `xbox_guest_meter` timing failure that passed its allowed rerun; it was not 94/94 on
the first run. Matched N2 medians 44.00 → 43.95 show no speed claim. One excluded pre-window
startup crash remains causally unresolved; a similar pre-change exit/phase does not prove its cause
or exclude this change. The next separate operation is **N4.4 sub-allocated memory**, then the
persisted pipeline cache/R4, N6 and R5 in the approved order. Nothing later than N4.3 was started.

**Latest independent acceptance (2026-10-17): N4.2 is accepted at toolkit `cd638d4` / game
`acf20ff` with records `801df8b`** (TR §31.37). The two confirmed failure-path defects are
closed; historical rejected candidates and their repair evidence remain below. Codex's original
ordering reproduction now passes, six targeted Vulkan CTests pass independently, private baseline
and both weakening mutants fail on actual ordering semantics and the restored source passes
(14 properties / 275 checks). Full-suite receipt 92/92, final checks and the loader-witnessed
bounded N3 archive are verified. No frame-rate gain is claimed. **Next separate operation:
N4.3 narrower barriers and layouts**, then sub-allocated memory, persisted pipeline cache/R4,
N6 and R5 in the approved order. No later item is accepted by this review.

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

**N4.2 at toolkit `4832f62` / game `70ecf0f` + records `fe09e5e` remains rejected pending
abandoned-batch ordering repair** (independent review, TR §31.35). The caller repair works:
publication outcome explicit at every coherence boundary, one bounded
retry, then a coherence stop propagated to the flip, target replacement, texture aliasing and the
executor's CPU consumers (`Nv2aBackend.stopped`). A different worker's caller gate
(`jsrf_vk_readback_callers`, 15 properties/297 checks; red on the pre-fix source) drives the actual
callers and an executor case in `jsrf_nv2a_hal` decides the CPU-consumer half; eleven private
final-source mutants plus the real-device rounding mutant are in the receipts, and the bounded
multi-batch split is now covered instead of being called unreachable. `jsrf_vk_readback_fault_gate`'s
expectations are unchanged. Full CTest 91/91, `just check` passed, one bounded N3 e2e with
`rb_incomplete=0`, and three N2 Garage windows whose median (43.87) sits below the archived candidate
set's (44.72) with no attributable cause — no performance claim in either direction. Still open in N4: narrower
barriers and layouts, sub-allocated memory, the persisted pipeline cache and R4. **History, for the
rejection this repair answers:**

**N4.2 at toolkit `891bca2` / game `505f798` was rejected pending a coherence repair**
(independent Codex review, 2026-10-17; TR §31.33). The implementation and normal-path
measurements below are retained as candidate evidence, not acceptance. The flip's readback stages every dirty target
through ONE bounded pair of buffers — a host-visible slice buffer and a device-local pack buffer,
grown on demand up to a 96 MiB cap with 4-byte-aligned tight rows — and packs a 16-bit target with
the ahead-of-time compute kernel `cs_pack` (new tracked `src/video/shaders/nv2a_vk_pack.hlsl`,
built and `spirv-val`'d by the existing `xbr_add_spirv` rule as `-T cs_6_0`, nothing compiled at
run time), so the host only memcpy's rows into guest memory. The per-target `w*h*4` readback
buffers and the per-pixel 5:6:5 truncation loop are gone, and a 16-bit target's PCIe readback
traffic halves. The bytes did not change: the kernel computes
`(r8>>3)<<11 | (g8>>2)<<5 | (b8>>3)`, the same truncation the deleted loop computed from the
guest's little-endian `B|G<<8|R<<16|A<<24` word, and 4-byte targets keep the byte-for-byte row
copy. The queue family the back end picks must now have `VK_QUEUE_COMPUTE_BIT` as well as
graphics: a device without one is passed over and the executor's CPU rasteriser remains the
fallback, because there is ONE 16-bit publication path and not a second, untestable
CPU-truncation branch. A pack descriptor-set allocation that fails no longer publishes a
half-recorded batch — the batch is skipped whole and its targets stay dirty and are retried.
Measured on the RTX 4070 over three quiet 60 s Garage windows each: host publication **0.564 →
0.132 ms per flip** and fence wait **5.333 → 5.10–5.45 ms per flip** (the pack's added GPU work
did not grow the drain), whole flush 5.91 → 5.43/5.26/5.62 ms — but presents/s **did not move**
(45.20 → 44.72 median, inside the before runs' own 44.03–46.35 spread), so this makes **no
frame-rate claim**: the removed host work was absorbed by idle time in the frame loop, and the
drain (which this item does not target) still dominates the flush. Gates, both authored by workers
other than the production change: `jsrf_vk_readback_pack` (the real back end on lavapipe over two
frames and a mid-frame alias flush, seven targets including a pitch-padded 16-bit one, an odd
21×13 dirty rectangle and both formats in one flip, every published byte compared with the CPU
executor and the truncation boundary constants recomputed in the fixture; four mutants — rounding
pack, skipped publish, slice-0 publish, over-wide rows — each fail it) and
`jsrf_vk_readback_fault_gate` (a deterministic pack-set allocation failure: 7 properties, 138
checks, with the pre-fix plain-`break` mutant failing 5 of them while the success-path controls
stay green). Full CTest 90/90 and `just check` pass, and the N3 bounded e2e under the Khronos
layer is clean (0 validation errors, the known 10 warnings, 0 program misses). Still open in N4:
narrower barriers and layouts, sub-allocated memory, the persisted pipeline cache and R4.

Measure each with N2. Parity tests must stay green. Each code subitem gets a different worker's
gate and deciding mutation evidence.

**N4.2's coherence repair is done at toolkit `4832f62` / game `70ecf0f`** (TR §31.34), before any
narrower barrier. The defect it answers: an injected pack descriptor-allocation failure let the actual
`vb_flip` advance the frame with 496 guest bytes stale, and the next `vb_target_load` reloaded those
old bytes over the unpublished target. Retaining dirty protected nothing. The repair reports the
publication outcome from the flush, retries a boundary failure exactly once after refreshing the ring,
and otherwise stops the back end before any stale reload, release, guest consumption or frame
acknowledgement; the stop is propagated to the executor's CPU paths through `Nv2aBackend.stopped`.
Staging allocation failures are covered with the same path. The deciding tests come from workers
distinct from the production change, run against the final source in private overlays: the
caller-level gate, the executor case, eleven mutants (the candidate's four success-path mutants were
built in the shared checkout against a pre-final backend, contrary to the packet; those four mutations
were re-run privately against the final source, and only the re-runs are relied on), and a bounded
split/over-cap property that replaces the old "unreachable" claim. The owner-authorized review-confirmed reset's one repair cycle is consumed by this packet; no
later N4 item starts before independent acceptance. This changes no milestone or owner stop condition.

**N4.2's ordering repair is done at toolkit `cd638d4` (on `4832f62`) / game gate `acf20ff`**
(TR §31.36), before any narrower barrier. The defect it answers (§31.35): a group whose pack
descriptor set could not be allocated left its already-recorded copies in the open recording, and the
next group's slices start at offset 0 again, so its 4-byte copy — or a 16-bit target's pack-output
copy — rewrote the same staging bytes with no memory dependency between the two writes. ONE added
`vb_barrier()` at the abandonment point orders every write the abandoned group had recorded before
anything recorded after it, on both staging buffers, and covers the pack buffer's
read-before-a-later-write class; no counter, retry, stop, publication, slot or ownership semantic
changed and no success path was touched. A different worker's gate `jsrf_vk_readback_ordering_gate`
(14 verdicts / 275 checks) decides it from the recorded command stream — stage and access masks,
destination ranges, command buffers and slot/fence state, with write-after-write and write-after-read
classes — red on the pre-fix source (10 properties, 23 of 275 checks) and on two private weakening
mutants of the final source, green restored at the identical sha256; the retained caller, fault, ring,
executor, real-device pack, parity and AOT/V6 gates stay green, full CTest is 92/92 in 441.05 s,
`just check` passes with the real corpus at 126/126, and a bounded N3 e2e on the pushed, clean-tree
executable is loader-witnessed and clean. The guest-memory guarantee is now stated per published target
rectangle: an earlier batch can publish before a later one fails, so the stop preserves the unpublished
targets and is not whole-frame atomicity. The owner-authorized review-confirmed reset's one renewed
repair cycle is consumed by this packet; no later N4 item starts before independent acceptance.

#### R4: measure pipeline-cache pressure (part of N4) — DONE (TR §31.44)

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
