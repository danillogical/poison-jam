# Full regeneration with the v0.12 lifter (owner instruction, 2026-09-28)

**Result:** the generated tree now comes from the toolkit `2925f0b` lifter. Build, `ctest` 23/23 and the
game Python suite pass; a strict run reaches the same stop by the same allocation. **One behaviour
difference is open** (below).

## Procedure

1. Full pass, from the game root with the toolkit on `PYTHONPATH`:
   `python -m tools.recomp game/default.xbe --all --split 1000 --gen-dir src/recomp/gen --game-name
   "Jet Set Radio Future" --manual-functions config/manual-functions.json --exclude-manual
   src/recomp_manual.c --trace-functions config/trace-functions.json`.
   **Analysis inputs are the toolkit's** gitignored `tools/{disasm,func_id,abi_analysis}/output`
   (dated 2026-09-21, the inputs of the previous pass): every previously generated function is in the
   toolkit's 8768-entry `functions.json`, 713 are absent from the game's 8437-entry one. The earlier
   attempt of 2026-09-22 (`5e756a7`) used the game's and was reverted as unattributable. The provenance
   manifest now records the toolkit files as `toolkit:` inputs.
2. `relift-selected.py boundaries`: all 7 reviewed boundary fixes verified (5 relifted in generated code,
   2 live in `recovered.c`).
3. `recomp_types.h`: the pass refreshes it from the template; the project's exact-delta ABI additions were
   carried over by a three-way merge (base: template at toolkit `484887b`), clean.
4. The six A4b2 `jsrf_watch_store` hooks re-applied at the same guest stores (anchored on label and store).
5. `recovered.c` untouched (recovery-owned; it had no dropped instructions).

## What the new lifter changed

| | Before | After |
|---|---|---|
| Generated functions | 5580 | 5740 (+161 switch-arm entries, −1 folded tail) |
| Dispatch entries | 8768 | 8928 |
| Dropped `rcl`/`rcr` | 8 | 0 (the CRT divide helpers stay hand-written) |
| Untranslated instructions | silent comments | `RECOMP_UNIMPL` report at runtime (`src/recomp_manual.c`) |

Two fixes the regeneration required:

- **`sub_00162B9D`** (`mov eax, 0x800401F0; ret 0xc`), a shared COM error tail of two recovered parents,
  is now folded by the translator and no longer emitted; `recovered.c` tail-calls it. Hand-written in
  `src/recomp_manual.c`, listed in `config/manual-functions.json` and the manual lookup.
- **`recomp_unimpl`** is defined per the toolkit template: the instruction remains a no-op, as before, and
  is reported; `RECOMP_UNIMPL_TRAP=1` aborts at the first one (run-profiles: observation, fail-closed).

## Behavioural comparison

Strict runs, settings `RECOMP_APU_TRAP=1 RECOMP_GPU_ACK=0 RECOMP_KERNEL_LOG_BUDGET=100000`, 8 s:
old `20260928-183145-568-crt-divide-fix-strict`, new `20260928-185612-449-regen-v012-strict`.

- Both STRICT; `[A3A] ac97 witness: gc=0x00000002 gs=0x00000100` in both; no `[UNIMPL]` reached; no ABI or
  delta failure.
- Both request 598,869,040 bytes from `0x00149E50`, fail, and die at `[ICALL] invalid target 0x00000000
  return=0014982E`. **Stop unchanged.**
- The 18 traced DirectSound functions have **identical entry counts** in both builds.

**Open difference — D3D resource release.** Both builds make the same 116 contiguous allocations
(ordinal 166). The old build frees 54 of them through `sub_00192830` (ordinal 171 from `0x001928BB`); the new
build frees none. A diagnostic trace of the new build (`20260928-190035-939-regen-trace-192830`, positive
control: 634 other trace entries) shows `sub_00192830` is **never entered** — its own translation is
equivalent in both trees, so a caller's decision changed. Main-thread `RtlEnter/LeaveCriticalSection` pairs
drop by ~160 accordingly. Which build is right is not established: the old tree carries translations the
upstream fixes call wrong (flags at joins, operand-width sign, `REPE CMPS`, `LOOP`), and either side could
be the faithful one. **This is a discovery question for the next Planner:** trace `sub_00192830`'s ten
callers in both builds and find the first divergent branch.

**Rollback:** revert the merge of this branch; the pre-regeneration tree is `0f7ef9c`.
