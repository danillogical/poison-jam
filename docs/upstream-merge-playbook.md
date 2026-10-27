# Upstream merge playbook: `sp00nznet/xboxrecomp` into the fork, then re-accept the game

This is the weekly procedure. The model run is TR §31.83 (the v0.16.0 catch-up); the
earlier C3/C4 acceptance is TR §31.54. Everything runs locally on the Windows laptop.
Follow the steps in order, one run at a time, and record what you measured, not what you
expect.

Tooling named here (`just tk-tests`, `just ctest-limited`, `just audit`,
`scripts/toolkit_path.py`, `scripts/tk-tests.py`, `scripts/ctest-limited.py`) landed on 2026-10-26
(game `a3eecf6`, `a0b7d1b`, `382f798`).

## Repositories

| Name | Path | Remotes |
|---|---|---|
| Toolkit | `C:\Users\logic\Repos\xboxrecomp` | `origin` = danillogical fork (push); `upstream` = sp00nznet (fetch only, push URL `DISABLED`); `nworkers` = reJSRF's fork (fetch only, `DISABLED`; branch `rejsrf` = reJSRF-only commits on top of upstream) |
| Game | `C:\Users\logic\Repos\poison-jam` | `origin` |

Verify the `DISABLED` push URLs with `git remote -v` before anything else.

## Owner policy (do not reopen without the owner)

- Lean toward upstream's design in conflicts: we merge weekly.
- Where both sides solved the same thing, take upstream's structure, names and APIs. Re-express ours as a small separable delta (an alias, a hook, a separate function). Example: `--backedge-yield` is an alias of upstream's `--back-edge-yield`.
- Keep genuinely unique fork fixes, placed to minimise future conflict.
- Never silently drop fork behaviour. List each such change for the owner.
- Opt-in upstream behaviour stays default-off. List every default that changed.
- Decisions already made:
  - Keep the fork's model vblank (`RECOMP_VBLANK` is retired).
  - Keep the kmem arena.
  - Keep the APU IRQ path.
  - Keep APC deferral default-on with NT ordering (JSRF's ADX poller needs it; L74).
  - Keep the keyboard packet moving only on change.
  - Keep serial mode (L34): upstream's guest-CPU lock plus gate (`RECOMP_DISPATCH_GATE=1 RECOMP_GUEST_LOCK=1`) was tried and does not reach the title.

## Stop and ask the owner

- A conflict forces choosing one side's semantics.
- An owner-decided divergence would change.
- Regeneration produces a startup hang or fatal, or a jump in fatal aliases you cannot classify.
- A runtime run regresses: title not reached, a new ICALL or fatal, a visible frame difference beyond timing.
- An environment step fails twice: ssh/socket `Operation not permitted`, an MSBuild stall at "Checking File Globs" (retry `--parallel 1` once), lavapipe.
- Anything would push.

## 1. Fetch and survey

1. `git fetch upstream nworkers origin` in the toolkit.
2. Count `HEAD..upstream/main`.
3. List it with dates and subjects: `git log --format='%h %ad %s' --date=short HEAD..upstream/main`.
4. Note the upstream releases (tags) in the range.
5. Check the game and toolkit trees are clean and note HEAD, ahead/behind for both.

## 2. Trial merge in a scratch clone

The real toolkit stays read-only in this step.

1. `git clone --no-hardlinks <toolkit> <scratch>` with `<scratch>` outside both repos.
2. In the scratch: `git merge --no-commit --no-ff upstream/main`.
3. Count conflict hunks per file (`git diff --name-only --diff-filter=U`, then `grep -c '^<<<<<<<'`).
4. For modify/delete conflicts, find why the fork deleted the file (`git log --diff-filter=D -- <file>`) before deciding.

## 3. Merge on a local branch

1. In the real toolkit: `git switch -c merge-<version>`.
2. `git merge --no-ff upstream/main`. A true merge commit: never rebase or squash a merge.
3. Resolve each hunk from both sides' intent (`git log` and `git show` on each side).
4. Write a per-file resolution table: file, ours / upstream / kept, one-line reason.
5. List every changed default and every fork behaviour that moved or went away.

## 4. Toolkit verification

1. `just tk-tests`: toolkit CTest plus the standalone projects. It classifies expected failures from `tests/FORK-STATUS.md` (b), (c), (d).
2. `pytest -q tools/recomp tools/disasm` with `C:\Python313\python.exe` and `PYTHONPATH=C:\Users\logic\AppData\Roaming\Python\Python313\site-packages`. The known failure is the `test_lifter_rep_compare_flags` negative control.
3. The lifter unittests, from the toolkit root: `python -m unittest tools.recomp.test_lifter_atomics tools.recomp.test_lifter_string_compare tools.recomp.test_lifter_carry tools.recomp.test_seh_frame_owner`.
4. Classify every new failing standalone project:
   - (a) a real regression: fix it with new commits on the merge branch, never amend the merge;
   - (b) an upstream test of a design the fork did not take;
   - (c) platform-only;
   - (d) pre-existing on `main`.
5. Update `tests/FORK-STATUS.md`.
6. The v0.16 merge's six modify/delete conflicts were files in three upstream test projects the fork deleted on purpose: `tests/memory_regressions/` (`CMakeLists.txt`, `README.md`, `test_main.c`), `tests/nv2a_ffp_backend/` (`CMakeLists.txt`, `nv2a_ffp_backend_test.c`) and `tests/nv2a_pb_walk/nv2a_pb_walk_test.c`. They test upstream designs the fork does not carry (segment scan, upstream FFP, page allocator); keep them deleted.

## 5. Make local `main` the merge

1. Fast-forward LOCAL toolkit `main` to `merge-<version>`. Nothing is pushed.
2. Record the old `main` commit first; `git branch -f main <old>` reverses it.
3. Every game script resolves the toolkit through `scripts/toolkit_path.py`: build cache, then `XBOXRECOMP_DIR`, then the sibling. It refuses on disagreement, so with local `main` moved everything sees the merged toolkit.
4. Do not point `XBOXRECOMP_DIR` at a worktree unless the build cache agrees.

## 6. Back up ignored generated output

1. Copy `src/recomp/` and `tools/disasm/output/` (game), and `tools/{disasm,func_id,abi_analysis,recomp}/output` (toolkit), to a dated folder outside both repos.
2. Verify with file counts and hashes: `python -X utf8 scripts\tree-compare.py <live> <backup>`.
3. Record the baseline inventory: `python -X utf8 scripts\recomp-inventory.py src\recomp <backup>\inventory.json` (full inventory to the file, summary to stdout).
4. Keep the backup until the owner accepts the merge; it is the baseline for step 8.

## 7. Rebuild from nothing

Clear stale analysis JSON first. Keep the disasm settings identical to last time.

1. Toolkit disasm, funcid and ABI stages, in the shared toolkit (about 431 s).
2. Game disasm `--text-only` (about 447 s).
3. `just regen` (about 220 s).
4. `python -X utf8 scripts\relift-selected.py boundaries`.
5. `python -X utf8 scripts\recover-functions.py` (about 37 s).
6. `python -X utf8 scripts\gen-abi-deltas.py`.

## 8. Classify the deltas against the backup

1. Re-run `scripts\recomp-inventory.py src\recomp <new>.json` on the new tree and compare it with the backup inventory:
   - function count;
   - dispatch entries;
   - direct dispatch;
   - fatal late-owner aliases;
   - owner-aliases;
   - recovery traps.
2. Recovered/generated duplicates must be 0.
3. Classify every added or removed function from the original XBE: `python -X utf8 scripts\inspect-jsrf.py disasm <a> <b>`. The class is genuine, internal label, kernel thunk or invalid.
4. Watch item: v0.16 relabelled genuine routines as `tail_jump_alias`, which the late-owner rule (`7544264`) made fatal (124 functions, including the CRT thread-notify routine 0x181009). Toolkit `af78da4` fixed it: a late-owner alias that no real span covers and that ends in ret, jmp or trap keeps its body.
5. A jump in fatal aliases after a merge is a stop-and-diagnose signal.
6. Known residue: 0x1688F3 stays fatal (L65).

## 9. Game-side follow-ups

1. Retire generated patches whose "before" text no longer exists.
2. Widen reviewed recovered spans only with disassembly evidence.
3. Classify every new env switch in `scripts/jsrf_run_profile.py` and `docs/jsrf-run-profiles.md` as strict-safe, exploratory or observation.
4. Re-record provenance: `python -X utf8 scripts\check-generation-provenance.py --write`.
5. Re-record the baseline: `python -X utf8 scripts\update-generated-baseline.py`.
6. Commit.

## 10. Gates

1. Guarded `just build`. Compare the warning set with the previous build; 18 MSB8028 is the known set.
2. `just check`.
3. `just ctest-limited --output-on-failure`: the full limited CTest, 241 tests as of 2026-10-26. Never use `|` in `-R`.
4. All D4 gates green with none skipped: `jsrf_bumpenv_*`, `jsrf_pbcov_*`, `jsrf_texgen_stream_*`.

## 11. Runtime acceptance

One run at a time. The title takes about 125-145 s to appear, so never use a budget under 620 s for the full script.

1. Strict: `just strict-run <label>`. Expect `diagnostic_deadline` with all checkpoints passed and no fatal, ICALL, alias or stub lines. Inspect `result.json`, profile and logs (`docs/jsrf-run-profiles.md`).
2. Vulkan e2e: `python -X utf8 scripts\key-script-run.py <label> 620 config\key-scripts\e2e.json RECOMP_GPU_BACKEND=vulkan RECOMP_VK_PIPELINE_CACHE=off`.
3. D3D11 e2e: the same command with `RECOMP_GPU_BACKEND=d3d11`.
4. Require `icall 0, fatal 0`, a complete script, and changing presents near the end.
5. View the snapshot PNGs side by side with the previous accepted run (§31.83: `20261025-162427-307-keyscript-v016-e2e-vk`, or the latest accepted):
   - 03 main-menu
   - 07 intro-00 (Corn)
   - 25 intro-06
   - 47 start
   - 59 paused
   - 62 resumed
   - 70 b-after-jump2
6. Any visible difference beyond timing is a stop.

## 12. reJSRF check

1. `git log upstream/main..nworkers/rejsrf` in the toolkit.
2. Check the reJSRF game repo and `reexec/rexbox` with `gh api` for MIT fixes that apply to our NV2A LLE path.
3. Perf-only and HLE-D3D8 changes usually do not apply.
4. Port with `git cherry-pick -x` and keep the attribution.

## 13. Records

1. A TR section: range, resolution summary, owner decisions, delta counts, runtime runs, divergences.
2. A plan status line in `plan-jsrf-gameplay.md`.
3. Ledger rows for new upstream default-on behaviour and for kept divergences.
4. `just check`.

## 14. Publish (only with the owner's confirmation)

1. `just audit` (`scripts/outgoing-audit.py --repo both`): fast-forward, clean tree, blob sizes, secrets, forbidden paths, `DISABLED` push URLs.
2. Push toolkit `main` first, then the game: `git -c credential.helper= -c "credential.helper=!gh auth git-credential" push origin <branch>`.
3. Never `--force`, never push to `upstream`.
4. Record `PUSHED_TO / BRANCH / COMMIT / REMOTE_URL / RESULT` receipts in the TR.
5. Delete the merge branch and any scratch clones.

## Divergence inventory

Where our deltas live. Each is a candidate to propose upstream or to delete.

**To propose upstream**
- NT-ordered APC deferral, default-on (L74).
- The `recomp_icall_fail_log` stub report.
- The 16-bit verbatim clear in swizzled-surface clears.
- The tiny-w screen-space clip path (L67).
- The model-driven vblank clock.
- `af78da4`: late-owner aliases that are complete routines keep a body.
- Not proposed: the texgen helper `cf185d0` is deliberately self-contained.

**Delete after the next regeneration**
- `RECOMP_BACKEDGE` (now routed through `RECOMP_BACK_EDGE`).
- `xbox_inb` / `xbox_outb`.
- `usb_gamepad_output`.
- The `xbox_VblankAckHeld` stub.

**Pick-one decisions already made**
- Serial mode over the gate (the gate diagnosis awaits a stronger model).
- Model vblank over bridge vblank.
- kmem arena over the page allocator (worth a dedicated experiment).
- The fork's APU IRQ path.
