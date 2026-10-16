# JSRF Windows recompilation: engineering guide

## Orchestration

- **Codex agent (Plan Orchestrator):** use `$codex-omp-orchestrator` and its Codex
  Plan Orchestrator reference.
- **Primary OMP agent (Turn Orchestrator):** use `$codex-omp-orchestrator` and its
  OMP Turn Orchestrator reference.
- **Other OMP roles:** remain subagents and follow the skill's subagent guidance
  within the Turn Orchestrator's assignment.

## Project

Port Jet Set Radio Future to Windows through Xbox static recompilation. First reach a
playable opening area with movement, graffiti, audio and save/resume; then expand to
the whole game. Work in small, verifiable milestones and explain defects with evidence.

## Start here

1. Read `plan-jsrf-gameplay.md`: current execution authority, measured state and next task.
2. Read `docs/jsrf-run-profiles.md`: strict, exploratory and fixture evidence rules.
3. Consult `docs/jsrf-technical-record.md`, `docs/jsrf-compatibility-ledger.md` and
   `config/stop-chain.json` for findings, shortcuts and runtime stops.
4. Fetch both repositories and inspect HEAD, remotes, ahead/behind and working trees
   before editing. Preserve unrelated edits.

Keep the current plan accurate as evidence moves. Title and keyboard plans are
historical records. Durable findings belong in the current plan, technical record,
strict-horizon ledger or compatibility ledger; session reports and scratch notes stay
outside the repository. OMP routing belongs in `.omp/config.yml`.

## Repositories and public safety

- Game: `C:\Users\logic\Repos\poison-jam`; public origin
  `https://github.com/danillogical/poison-jam`.
- Toolkit: `C:\Users\logic\Repos\xboxrecomp`; origin is the owner's fork
  `https://github.com/danillogical/xboxrecomp`; upstream is
  `https://github.com/sp00nznet/xboxrecomp`, fetch-only with push URL `DISABLED`.
  Verify names and URLs with `git remote -v`.
- Read toolkit `docs/GETTING_STARTED.md`, `docs/technical/indirect-calls.md` and
  `lessons-learned.md` before toolkit changes.
- Preserve original assets and existing saves. Never track or push `game/`, retail
  lifted code, Xbox BIOS/MCPX/HDD assets, secrets or blobs over 100 MB.

Commit durable verified work as it lands; never leave finished work uncommitted across
a session boundary. Push at useful checkpoints and closure. When both repositories
have outgoing work, push toolkit first, then game: game records cite toolkit commits.
An unchanged, synchronized repository needs no new commit or push.

Before every push, verify the tree is clean, the branch/commit is the intended durable
state, the destination is `origin`, and the update is a fast-forward. Code needs its
relevant tests green; never push failed, rolled-back or incomplete experiments or a
temporary conflict branch. Records may be pushed when committed and clean. For the
public game repository, audit every outgoing commit for forbidden paths, secrets
(`scripts/secret-audit.py`) and oversized blobs. Never use `--force` or
`--force-with-lease`; never push toolkit upstream without explicit owner authorization.
Record pushes with `PUSHED_TO: / BRANCH: / COMMIT: / REMOTE_URL: / RESULT:` in the
durable record for the work they close.

## Build, test and run on Windows

Prefer supported `just` recipes; `just --list` lists them:

```powershell
just shaders               # ahead-of-time NV2A vertex-program corpus (once per checkout)
just build                 # guarded Release build and identity checks
just test                  # build, then full CTest suite
just ctest                 # CTest against the current build
just check                 # repository checkers
just strict-run <label>    # explicitly sets RECOMP_GPU_ACK=0
just explore-run <label>   # exploratory; adds no override
just probe <name> <label>  # bounded fixture probe
just doctor                # environment report
just disk                  # space gate and retention plan
```

`just shaders` must run once in a fresh checkout, before the first `just build`:
the Vulkan back end's vertex-program corpus is retail-derived and untracked, so
CMake fails with a clear message until it exists. It compiles offline with the
Vulkan SDK's `dxc` and `spirv-val` (set `VULKAN_SDK`); nothing is compiled at run
time. It also records the SHA-256 of the tool that translated the programs, and
`build-jsrf.py` refuses a corpus it cannot compare against the tool the repository
owns (`build/vsh-tool/.../nv2a_vsh_hlsl.exe`): delete or relink that artifact, move
any of the toolkit's three translator sources (`nv2a_vsh_hlsl.c`, `.h`,
`nv2a_vsh_hlsl_tool.c`), or leave an environment or cached `XBOXRECOMP_DIR` naming a
different toolkit, and `just shaders` has to run again before `just build` accepts.
`JSRF_NV2A_VSH_HLSL` names an external tool instead: it is read-only to this build
(never rebuilt, replaced or overwritten) and must be newer than all three sources
with the corpus's recorded sources unchanged, or the refusal stands.

Vulkan, and therefore `just shaders`, is optional. The toolkit publishes one
capability decision (`XBR_VULKAN_FOUND`, cache BOOL, recomputed every configure: the
loader, its headers, `dxc` and `spirv-val` must all exist), and the game's configure
branches on it: with the capability, the retail corpus is required and its
provenance is validated inside that configure; without it, the configure generates an
empty vertex-program corpus under `build/empty-corpus`
(`nv2a_vsh_program_count = 0`), prints that status, and program batches run on the
executor's CPU interpreter. A capability-off build therefore needs no corpus, no
`dxc` and no `VULKAN_SDK`; `just shaders` itself still does.

Underlying commands when a recipe is unavailable:

```powershell
python -X utf8 scripts\build-jsrf.py
ctest --test-dir build -C Release --output-on-failure
python -X utf8 scripts\run-jsrf.py --seconds 5 --label smoke
python -X utf8 scripts\test-harness.py
```

The executable is `build\Release\jsrf_recomp.exe`, with the game root as its working
directory. Prefer bounded launches. `scripts/key-script-run.py` drives gameplay with
isolated run saves (see the current plan); the D3D11 collector stall is documented in
TR §29. Inspect executable paths before stopping processes; never kill unrelated ones.
After runtime changes, run targeted behavior/ABI regressions and a game smoke run.
After collector/thread-tracing changes, rerun harness probes. Documentation-only edits
need no rebuild. Run checks against the final durable tree.

Host tools already available: LLVM 23.1.2 / `clang-cl`, `just`, `pre-commit` and WinDbg
`ttd.exe` on PATH (TTD recording needs elevation); Python at `C:\Python313\python.exe`
with DuckDB 1.5.6. XbSymbolDatabase CLI:
`C:\Users\logic\Repos\XbSymbolDatabase\build\win_x64\bin\XbSymbolDatabaseCLI.exe`.
xemu: `C:\Users\logic\Downloads\xemu\xemu.exe`, with owner assets in sibling
`bios`, `mcpx`, `hdd` directories. Never copy/archive/upload those assets.
Use installed tools; do not reinstall to change installation method.

- Guard builds with `scripts/build-jsrf.py`: case-duplicate PATH/proxy variables can
  break MSBuild. For direct CMake builds, unset lower-case proxy variables.
- Toolkit Python may need `C:\Python313\python.exe` and
  `PYTHONPATH=C:\Users\logic\AppData\Roaming\Python\Python313\site-packages` for capstone.
- Confined MSBuild can stall at `Checking File Globs`; retry `--parallel 1` first.
  Denied temporary writes or `%LOCALAPPDATA%\xboxrecomp\Partition*.img` access are
  environment blockers. Real runs need access to emulated disks.
- Write multiline commit messages to a file and use `git commit -F <file>` if shell
  heredocs are rejected. `logs/` is ignored; durable helper source belongs elsewhere.

## Evidence

`docs/jsrf-run-profiles.md` alone owns override classification; do not duplicate its
table here. Strict runs require exact `RECOMP_GPU_ACK=0`; the runner refuses a strict
request without it and never inserts it. Synthetic completion/bypass makes a run
exploratory. Exploratory runs may satisfy pragmatic milestones only with every relied-on
shortcut ledgered and its IDs listed in the run record; they cannot prove fidelity.
`JSRF_ALLOW_UNRESOLVED=1` cannot satisfy acceptance. Check override drift with
`scripts/check-override-drift.py`.

Inspect `result.json`, profile, logs and source identity before making claims.
`diagnostic_deadline` only means bounded capture; `normal_exit` only means guest entry
returned; `gpu_report_ok` only means analysis succeeded. Handled first-chance exceptions
are not crashes. Fixture probes may return before `guest_entry`; use their actual
checkpoint, e.g. `--probe gpu-submit-supported --expect-checkpoint probe_gpu`.

Before interpreting XBE-backed dump memory:

```powershell
python -X utf8 scripts\check-dump-mapping.py <run-dir>
python -X utf8 scripts\inspect-jsrf.py memory <run-dir> <guest-va> <length>
python -X utf8 scripts\inspect-jsrf.py disasm <start-va> <end-va>
python -X utf8 scripts\verify-initializers.py <run-dir>
python -X utf8 scripts\jsrf_gpu.py <run-dir> --write
```

The mapping control at guest VA `0x00011000` begins
`8b512c85d28b4130c70190431c00741c`. Structurally valid dumps can have displaced XBE
content. Read actual guest VAs; a shifted comparison may prove byte provenance, never
fabricate repaired state. Read GPU reports/snapshots alongside `result.json`,
`stacks.txt` and `process.dmp`. Snapshots and the final-dump decoder are bounded
observations, not complete MMIO history. GET movement with synthetic acknowledgement
does not prove command execution.

Separate observation from inference. Missing, stale or UNKNOWN evidence is not a pass.
Zero hits need adequate coverage and a positive control where appropriate. A deciding
regression should detect its defect; demonstrate that when practical. Plans and review
opinions do not prove runtime behavior.
Instrumentation used to decide a result must retain the deciding event, e.g. with a
write-once latch or lossless event buffer; a droppable log line alone is insufficient.

Collector limits: ICALL history is partial; raw stack words are not unwound frames;
canonical RAM and contiguous/GPU allocations differ. Read thread history only from
frozen captures and account for publication loss/overwrite. Fixture concurrency does
not prove arbitrary race freedom. `RECOMP_WORKERS=inline` may deadlock and changes
semantics; use only for controlled experiments, never acceptance or a correctness fix.

## Generated code and guest ABI

`src/recomp/gen/` and `src/recomp/recovered/` are ignored retail-derived output, linked
into production but never published. CMake requires local generation from
`game/default.xbe`. Preserve a copy of local `src/recomp/` before pulling a commit
that untracks it, then restore the ignored output. Routine builds do not regenerate it.

Full translation spelling (from the game root, with toolkit on PYTHONPATH):

```powershell
python -m tools.recomp game/default.xbe --all --split 1000 --gen-dir src/recomp/gen --game-name "Jet Set Radio Future" --manual-functions config/manual-functions.json --exclude-manual src/recomp_manual.c --trace-functions config/trace-functions.json --backedge-yield
```

`just regen` runs this pass and `scripts/patch-generated.py` to reapply
`config/generated-patches.json`. Rebuild from nothing in order:

1. `just regen`.
2. `python -X utf8 scripts\relift-selected.py boundaries`.
3. `python -X utf8 scripts\recover-functions.py` (also needs local ignored
   `tools/disasm/output/functions.json`).
4. `python -X utf8 scripts\gen-abi-deltas.py`.
5. `just check` and `just test`.

A rebuild from nothing has not yet been measured end to end; record its requirements
when it is. Clear stale analysis JSON inputs before measuring a clean full regeneration.
Full-pass functions, declarations, dispatch and unresolved stubs must come from the
same pass. `--backedge-yield` emits serial guest preemption points (L34); targeted
relifts omit it because older chunk headers may lack the macro.

- Recovery output is owned by `scripts/recover-functions.py` and
  `config/recovered-functions.json`; review unresolved dependencies in
  `config/recovery-unresolved.json`. Keep definitions, direct symbols, dispatch,
  declarations and `config/manual-functions.json` consistent; generation writes the
  latter too. Never leave one symbol defined in both recovered and generated code.
- Boundary changes need `config/boundary-fixes.json` edits **and**
  `scripts/relift-selected.py boundaries`. Review newly exposed calls and add only
  genuinely missing declarations. Prefer targeted relifts for small fixes.
- Generated `.c` beside chunks is production; test fixtures go in
  `src/recomp/gen/fixtures/`. After regeneration review runtime-header synchronization,
  definitions, calls, stubs and dispatch; then build/test/run.
- Derive guest-access inventories from original XBE instructions and normalize to
  uint32. Hex `MEM32(0xFE820010u)` and signed decimal `MEM32(-25034736)` can name the
  same VA; grepping one spelling misses accesses (TR §4).
- Guest pointers are 32-bit, host pointers 64-bit. Use `XBOX_PTR`/`MEM32` mapping;
  never truncate native pointers into guest fields. Generated `void(void)` functions
  communicate through guest registers/stack: derive arguments, return/saved registers
  and RET cleanup from original instructions, not host C ABI.
- Classify missing targets (omitted function, internal label, kernel thunk, invalid
  pointer) before patching. Alignment/section membership alone is insufficient.
  Never silence failures with blanket success, arbitrary stack resets, guessed pointer
  guards or undocumented stubs; temporary stubs need a contract and removal gate.
- Unresolved/invalid calls are fatal by default (`0xE0424943`); record the earliest
  actual failure and caller. Recovery ABI checks are not a complete EBP audit.
  `JSRF_TRACE_HEAP=1` is diagnostic; normally leave it off.

Useful toolkit lifter regressions: `python -m unittest tools.recomp.test_lifter_atomics
tools.recomp.test_lifter_string_compare tools.recomp.test_lifter_carry
tools.recomp.test_seh_frame_owner` (one command from the toolkit root).

Dated operating history lives in `docs/jsrf-operating-history.md`; read it for
provenance, never as current execution instructions.
