# Toolkit sync to upstream v0.12.0+ (owner instruction, 2026-09-28)

**Result:** toolkit `main` = merge `2925f0b` (parents: local `d6e8f0b`, upstream `ea60cfa`). Fork caught
up: `upstream/main...main` = 0 behind / 60 ahead. Performed directly on the owner's instruction, not as a
packet.

## Merge

121 upstream commits since `v0.11.0`; 12 files changed on both sides; 3 conflicted (7 hunks):

| File | Hunk | Resolution |
|---|---|---|
| `src/kernel/kernel_hal.c` | includes | both sides |
| `src/kernel/xbox_memory_layout.c` | worker tick | upstream's removal of the early `DMA_GET = DMA_PUT` (advance now follows the scan); local's mirror ticks outside the GPU-ack gate; upstream's `dsp_ack_tick()`/`poke_tick()` placed beside them |
| `src/kernel/xbox_memory_layout.c` | worker start | local `RECOMP_GPU_ACK` read + upstream `dsp_ack_init()`/`poke_init()` |
| `tools/recomp/lifter.py` | ×2 | independent additions → both sides |
| `tools/recomp/lifter.py` | ×2 | same `rep cmps/scas` width fix on both sides → upstream's form |

A4s AC'97 invariants hold: A3a model region byte-unchanged; zero `getenv("RECOMP_AC97_READY")`, zero
set-only codec writes, zero `ac97_arm_write_trap(` call sites. `check-merge-structure.py`: files=120
switches=143 findings=0 unknown=0.

## Verification

| Check | Result |
|---|---|
| `build-jsrf.py` (no regeneration) | success |
| `ctest` | 22/22 |
| game Python tests | 420/423; the 3 failures are `test_generation_provenance` and pre-date the merge (see below) |
| toolkit `unittest` modules | 169 run, all pass (12 skipped) |
| toolkit pytest-style modules (pytest not installed; run through a local stand-in) | identical counts on the merge and on pure `upstream/main` |
| strict run `20260928-165936-788-sync-v012-strict-inert` (same settings as baseline `20260928-120818-632-a2h-attrib-inert-off`) | STRICT; `[A3A] ac97 witness: gc=0x00000002 gs=0x00000100`; main thread follows the baseline's kernel sequence to the same failed 598,869,040-byte allocation; death is the `0xFFFFFFFF` ICALL variant already seen in pre-merge runs (`20260928-030751-407`, `20260928-110806-304`). **Stop unchanged.** |

## Push

```text
PUSHED_TO: origin (owner's fork)
BRANCH: main
COMMIT: 2925f0bc63fa6337139c02c34a7fedefefda5be9
REMOTE_URL: https://github.com/danillogical/xboxrecomp.git
RESULT: d6e8f0b..2925f0b fast-forward; origin/main verified = 2925f0b
```

## Findings for the plan

1. **The committed generated code drops `rcr` in the CRT's 64-bit divide helpers.** Eight `/* TODO: rcr … */`
   in `recomp_0004.c`, in `sub_0017C9C0` (`__alldiv`, disassembly checked), `sub_0017D2C0`, `sub_0017D4D0`,
   `sub_001816B0`. None has a manual replacement. Any 64-bit divide or remainder whose divisor needs more
   than 32 bits returns a wrong result. Upstream's lifter now lifts `rcl`/`rcr` (`4dd267a`), but the tree is
   not regenerated. Other silently dropped instructions in the same tree: `popfd`, `popal`, `sldt`, `str`,
   `arpl`, `stmxcsr`, `int 0xcc`.
2. **Other upstream lifter fixes are also latent in the generated tree** until regeneration or a targeted
   relift: `LOOP/LOOPE/LOOPNE`, result sign at operand width, `REPE CMPS/SCAS` CF, `frndint` control word.
3. **New synthetic-completion switches are unknown to `jsrf_run_profile.py`**, which treats unknown names as
   strict: `RECOMP_DSP_ACK` (zeroes guest words — aimed at a DSP pending word), `RECOMP_POKE`,
   `RECOMP_FORCE_RETURN`, `RECOMP_PAD_PRESS`; behaviour-changing: `RECOMP_ASYNC_IO`, `RECOMP_UNIMPL_TRAP`,
   `RECOMP_USB_HC`, `RECOMP_USB_NDP`, `RECOMP_KEYBOARD`. Classification is a `jsrf-run-profiles.md` policy
   question for the Advisor.
4. **The generation-provenance guard fails at game `HEAD`, independent of the merge.** `3d0dc08` (A4b2-NR)
   added six `jsrf_watch_store` hooks to `recomp_0000.c`/`recomp_0005.c` without re-recording the manifest.
5. **Leads for the open A2h line, not claims:** the death follows a 598,869,040-byte allocation. A wrong 64-bit
   divide (finding 1) or read semantics (`991ff12`, `517682e`) could produce such a size. Test before
   relying on either.
