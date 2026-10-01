# JSRF Windows recompilation: agent working guide

## Purpose

Port Jet Set Radio Future to Windows through Xbox static recompilation. First reach a
playable opening area with movement, graffiti, audio, and save/resume; then expand to
the whole game. Work in small, verifiable milestones and explain defects with evidence.

At session start read, in this order:

1. `docs/agent-workflow.md` — **staffing, session loop, review, escalation**
2. `plan-jsrf-bare-minimum.md` — **current packet, blocker, next action, acceptance criteria/status**
3. `docs/jsrf-run-profiles.md` — **strict vs exploratory evidence rules**

Do not keep session reports in the repository. Durable findings go in the plan, a
packet, or a review record; scratch notes stay outside the repository.

`AGENTS.md` contains operating knowledge only. **Do not copy provider, model, or effort
assignments here.** If `docs/agent-workflow.md` is unavailable, staffing is BLOCKED;
do not reconstruct it from memory, reports, handoffs, or history.

Keep original assets and existing saves unchanged. Preserve unrelated edits.

## Workspace and key files

Game repo: `C:\Users\logic\Repos\my_xbox_game`  
Toolkit: `C:\Users\logic\Repos\xboxrecomp`

Toolkit remotes (**verify with `git remote -v`; do not assume names**): `origin` is the owner's fork
(`https://github.com/danillogical/xboxrecomp`); `upstream` is
`https://github.com/sp00nznet/xboxrecomp`, fetch-only (its push URL is `DISABLED`). The game repository's
remote is `origin` = `https://github.com/danillogical/poison-jam`, which is **public**: `game/` is
gitignored and original assets must never be tracked or pushed.

**Commit and push policy — both repositories (owner instruction, 2026-09-28; extends the toolkit
policy of 2026-09-25).** Regular commits and
pushes of **both** repositories are part of normal durable closure, not an end-of-project step.

- **Commit** durable work as it lands, in whichever repository it belongs to: an accepted or closed
  packet, a promoted packet, a review record or ruling, a plan update, an owner-directed change, a
  sync/merge. Never leave accepted work uncommitted across a session boundary.
- **Push both repositories at the same checkpoints:** packet closure, packet promotion, a completed
  sync/merge, a materially useful commit later work depends on, a milestone boundary, and the end of a
  session. Push the **toolkit first**, then the game, because game records cite toolkit commits.
- **Before every push**, in that repository: the tree is **clean**; the branch/commit is the **intended
  durable state**; the push is a **fast-forward**; the destination is **`origin`** (toolkit: the fork,
  never `upstream`); and, for **code**, the active packet's tests/acceptance passed. For the **game**
  repository, which is public, also confirm the outgoing commits add **no `game/` path, no secret, and
  no blob over 100 MB** (`scripts/secret-audit.py` covers secrets).
- **Records** (plan, packets, review records, rulings) may be pushed whenever committed and clean.
  **Code** may not be pushed in a failed/rolled-back state, on a temporary conflict branch, as an
  incomplete experiment, pending acceptance, or from a dirty tree — `R-CONFLICT`, rollback and
  `INADEQUATE` are **no-push** states for code.
- **Never** `--force`/`--force-with-lease`; **never** push to toolkit `upstream` without explicit owner
  authorization. Record each push as `PUSHED_TO: / BRANCH: / COMMIT: / REMOTE_URL: / RESULT:` in the
  record for the work it closes.

Inspect both working trees before editing. Toolkit instructions live in
`docs/GETTING_STARTED.md`, `docs/technical/indirect-calls.md`, and `lessons-learned.md`.

| Location | Role |
|---|---|
| `CMakeLists.txt` | game/test/collector targets |
| `src/main.c` | XBE load, runtime init, probes, guest entry |
| `src/recomp_manual.c` | manual dispatch and fatal unresolved-call diagnostics |
| `src/jsrf_crt.c` | verified CRT replacements such as memmove |
| `src/recomp/gen/` | generated translation units, dispatch, declarations, stubs |
| `src/recomp/recovered/recovered.c` | reviewed recovered bodies |
| `config/manual-functions.json` | generated/manual symbol exclusion set |
| `config/recovered-functions.json` | reviewed recovery entries and evidence |
| `config/boundary-fixes.json` | reviewed parent-span corrections |
| `config/recovery-unresolved.json` | fatal dependencies of recovered functions |
| `scripts/recover-functions.py` | recovery generation and validation |
| `scripts/relift-selected.py` | targeted relifts and boundary updates |
| `scripts/build-jsrf.py` | guarded Release build and identity checks |
| `scripts/run-jsrf.py` | bounded debugger launch and artifact archive |
| `scripts/check-run-profile.py` | strict/exploratory profile classification |
| `scripts/check-dump-mapping.py` | XBE-backed dump-content integrity gate |
| `scripts/check-merge-structure.py` | structural merge check: conflict markers, duplicate `case` labels, duplicate file-scope definitions |
| `scripts/inspect-jsrf.py` | original-XBE disassembly and guest-memory reads |
| `scripts/jsrf_dump.py`, `scripts/jsrf_gpu.py` | dump/GPU offline inspection |
| `tools/harness/collect.c` | external debugger, all-thread capture, minidumps |
| `docs/agent-workflow.md` | sole authority for agent roster/workflow |
| `docs/jsrf-operating-history.md` | dated narrative; read only when needed |
| `game/default.xbe`, `game/Media/` | original assets; never modify |
| `logs/runs/` | archived run evidence |

## Evidence rules

### Run profiles

**Do not enumerate the overrides here.** `docs/jsrf-run-profiles.md` is the single
authority on override classification, and this file previously carried a summary
that named two removed overrides (`RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`) as
live — the stale-duplication failure the plan records as W15. `AGENTS.md` is loaded
automatically on a byte budget, so a second copy drifts silently.

What a session actually needs:

- **A strict run must set `RECOMP_GPU_ACK=0` explicitly.** That switch is synthetic
  completion when enabled, and it is enabled by default when absent and by every
  value except the exact string `0`. `just strict-run` and `just ttd-record` set it
  for you; the runner refuses a strict request without it and never inserts it.
- **Any synthetic-completion or bypass setting makes a run exploratory.** Which
  names those are, and what each does, is the table in `docs/jsrf-run-profiles.md`.
- **`scripts/check-override-drift.py`** fails when a document names an override the
  toolkit no longer reads, so this section cannot go stale again.

**The bare minimum is pragmatic (owner, 2026-09-30).** Take the path of least resistance to the
title screen; exploratory runs may satisfy bare-minimum milestones when every shortcut they rely on
is in `docs/jsrf-compatibility-ledger.md` and the run record lists those ledger IDs
(`docs/jsrf-run-profiles.md` §"Pragmatic bare minimum"). Exploratory/fixture evidence still cannot
support a fidelity claim, and a shortcut without a ledger entry is not allowed.

`diagnostic_deadline` means capture was bounded, not that the guest was live.
`normal_exit` means the entry point returned, not that the title objective succeeded.
Always inspect `result.json` and the run profile before making acceptance claims.

### Dump integrity

Before interpreting XBE-backed memory from an archived dump, run:

```powershell
python -X utf8 scripts\check-dump-mapping.py <run-dir>
```

The control read is guest VA `0x00011000`; correct `.text` begins:

```text
8b512c85d28b4130c70190431c00741c
```

A dump can be structurally valid while XBE-backed content is displaced. Read runtime
memory at its **actual guest VA**. A shifted comparison may establish byte provenance;
it must never be used as a correction that fabricates repaired guest state.

### Probe runs

A `--probe=` run may return before `guest_entry`. Pass the checkpoint the probe actually
emits, for example:

```powershell
python -X utf8 scripts\run-jsrf.py --seconds 3 --probe gpu-submit-supported \
  --expect-checkpoint probe_gpu --label gpu-submit-supported
```

Do not compare a fixture probe with a real guest run as though they exercised the same
path.

## Build and run

**Prefer the `just` recipes** (plan T6); they are the supported spelling of each step, so
a record can cite `just <recipe>` instead of a command line that drifts. `just --list`
shows all of them.

```powershell
just build          # guarded Release build, identity-checked
just test           # build, then the full CTest suite
just ctest          # CTest only, against the current build
just strict-run <label>   # strict-profile run; sets RECOMP_GPU_ACK=0 for you
just explore-run <label>  # exploratory-profile run
just probe <name> <label> # bounded fixture probe
just check          # every repository checker
just doctor         # host/run environment report
just disk           # free-space gate and retention plan
```

The underlying commands, if a recipe is not usable:

```powershell
python -X utf8 scripts\build-jsrf.py
ctest --test-dir build -C Release --output-on-failure
python -X utf8 scripts\run-jsrf.py --seconds 5 --label smoke
Get-Content .\jsrf_run.log -Tail 50
python -X utf8 scripts\test-harness.py
```

A strict launch needs `RECOMP_GPU_ACK=0` in the environment; the runner refuses a
strict request without it and never inserts it. `just strict-run` sets it for you.

Useful inspection:

```powershell
python -X utf8 scripts\inspect-jsrf.py disasm <start-va> <end-va>
python -X utf8 scripts\inspect-jsrf.py memory <run-dir> <guest-va> <length>
python -X utf8 scripts\verify-initializers.py <run-dir>
```

From the toolkit root:

```powershell
python -m unittest tools.recomp.test_lifter_atomics \
  tools.recomp.test_lifter_string_compare \
  tools.recomp.test_lifter_carry \
  tools.recomp.test_seh_frame_owner
```

The executable is `build\Release\jsrf_recomp.exe`; its working directory must be the
game root. Prefer the bounded runner. Inspect executable paths before stopping an
existing process; never kill unrelated processes.

A captured failure may make the runner return nonzero. Interpret `result.json`, not the
shell code alone. Handled first-chance exceptions are not crashes.

After runtime changes, run targeted behavior/ABI regressions plus a game smoke run.
After collector/thread-tracing changes, rerun harness probes. Documentation-only edits
do not require a rebuild.

## Windows host tooling

The primary Windows development host has these external tools available.

| Tool | Version / location |
|---|---|
| LLVM / clang-cl | LLVM 23.1.2; `C:\Program Files\LLVM\bin\clang-cl.exe`; on `PATH` |
| WinDbg TTD recorder | `ttd.exe`; on `PATH`; recording requires an elevated process |
| just | `just.exe`; on `PATH` |
| pre-commit | `pre-commit.exe`; on `PATH` |
| DuckDB | Python package 1.5.6 under `C:\Python313\python.exe` |
| XbSymbolDatabase CLI | `C:\Users\logic\Repos\XbSymbolDatabase\build\win_x64\bin\XbSymbolDatabaseCLI.exe` |
| xemu | `C:\Users\logic\Downloads\xemu\xemu.exe` |

xemu owner-provided asset directories:

- BIOS: `C:\Users\logic\Downloads\xemu\bios`
- MCPX: `C:\Users\logic\Downloads\xemu\mcpx`
- HDD: `C:\Users\logic\Downloads\xemu\hdd`

These Xbox assets are local, proprietary owner-provided inputs. Never copy,
archive, commit, or upload them into either repository or any generated artifact.

Prefer tools resolved from `PATH` where available. Use the explicit paths above
for tools not placed on `PATH`. Do not reinstall an available tool merely to
change its installation method.

## Host and sandbox constraints

- This shell may export both lower- and upper-case proxy variables; MSBuild can fail on
  duplicate `http[s]_proxy` names. `scripts/build-jsrf.py` is the guarded path. For
  direct CMake builds, unset lower-case proxy variables.
- Toolkit Python scripts may require
  `PYTHONPATH=C:\Users\logic\AppData\Roaming\Python\Python313\site-packages` and
  `C:\Python313\python.exe` for `capstone`.
- Under confined/workspace-write execution, parallel MSBuild workers may fail silently
  at `Checking File Globs`; retry with `--parallel 1` before diagnosing source.
- Confined execution may block `tempfile` writes used by GPU inspection and may deny
  `%LOCALAPPDATA%\xboxrecomp\Partition*.img`. Treat those as environment blockers, not
  code regressions. Real game runs need access to the emulated disk images.
- Heredocs may be rejected by the command validator; write commit messages to a file
  and use `git commit -F <file>`.
- `logs/` is gitignored; do not place durable helper source there.

## Translation and regeneration discipline

### Full translation pass

Routine builds do **not** regenerate `src/recomp/gen/recomp_NNNN.c`. When a full pass
is required, use exactly:

```powershell
python -m tools.recomp game/default.xbe --all --split 1000 \
    --gen-dir src/recomp/gen --game-name "Jet Set Radio Future" \
    --manual-functions config/manual-functions.json \
    --exclude-manual src/recomp_manual.c \
    --trace-functions config/trace-functions.json
```

`recomp_funcs.h`, `recomp_NNNN.c`, `recomp_dispatch.c`, and
`recomp_stubs_unresolved.c` must come from the same pass.

Before measuring a clean full regeneration, clear stale analysis JSON inputs as
required by the translation workflow; do not measure a new lifter against old generated
chunks.

### Generated-source rules

- `scripts/recover-functions.py` owns reviewed recovery output, not the full chunk tree.
- Files under `src/recomp/gen/*.c` are linked into production. Test fixtures belong in
  `src/recomp/gen/fixtures/`, never beside production chunks.
- **One address can be spelled two ways.** The lifter emits an `A1` moffs load (always into
  `eax`) as hex — `MEM32(0xFE820010u)` — but a ModRM `disp32` operand as signed decimal —
  `MEM32(-25034736)` — for the same guest VA. Every address at or above `0x80000000` is
  exposed, which covers all MMIO and the contiguous window. **Never enumerate guest
  accesses by grepping one spelling**; derive them from the original XBE and normalise to
  `uint32`. Measured cost of getting this wrong: a `PIO_FREE` site list built by spelling
  covered 10 of 28 sites (`docs/jsrf-technical-record.md §4`).
- Boundary changes require both manifest editing **and**:

```powershell
python -X utf8 scripts\relift-selected.py boundaries
```

- Review newly exposed external calls after widening a boundary; add declarations only
  where the generated declaration set truly does not cover them.
- `config/manual-functions.json` is also written by generation. Avoid leaving one symbol
  defined both in recovered code and an unreplaced generated chunk.
- Avoid full regeneration for small fixes. After regeneration, review definitions,
  direct calls, stubs, dispatch, and runtime-header synchronization; then rebuild/test/run.

## Guest-code discipline

Guest pointers are 32-bit Xbox virtual addresses; host pointers are 64-bit. Use runtime
mapping (`XBOX_PTR`, `MEM32`, etc.). Never store truncated native pointers in guest
fields.

Generated `void(void)` functions communicate through guest registers and the simulated
stack. Derive ABI from original instructions, including argument positions, return
register, saved registers, and `RET` cleanup. Host C calling convention is not the guest
ABI.

Classify missing targets before patching: omitted function, internal label, kernel
thunk, or invalid pointer. Alignment/section membership alone is insufficient.

Do not silence failures with blanket success, arbitrary stack resets, guessed pointer
guards, or undocumented stubs. Temporary stubs require a contract and removal gate.
Keep direct symbols, dispatch, definitions, and manifests consistent.

Unresolved/invalid calls are fatal by default (`0xE0424943`).
`JSRF_ALLOW_UNRESOLVED=1` is exploratory only and cannot satisfy acceptance.
Record the earliest actual failure and caller.

Recovered wrappers check key ABI state, but existing checks are not a complete
architectural-EBP audit. `JSRF_TRACE_HEAP=1` is diagnostic only; normally leave it off.

## Harness and concurrency boundaries

The collector freezes native threads and archives stacks, guest stack words, canonical
guest RAM, registers, thread histories, contiguous/pushbuffer/instance memory, readable
GPU aperture, binaries/symbols, and source identity.

Interpretation limits:

- ICALL history is partial; it is not a full guest call stack.
- Raw guest stack words are not unwound frames.
- Canonical RAM and separately allocated contiguous/GPU memory are distinct.
- Read thread histories only from a frozen capture; publication loss/overwrite is
  explicitly reported.
- Fixture concurrency does not prove arbitrary race freedom or every kernel scheduling
  path.
- `RECOMP_WORKERS=inline` changes semantics and may deadlock; use only as a controlled
  experiment, never as a correctness fix or acceptance evidence.

## GPU inspection

For the current guest stop and next executable packet, use
`plan-jsrf-bare-minimum.md`; do not store a current stop in this file.

```powershell
python -X utf8 scripts\run-jsrf.py --seconds 5 --label gpu-setup
python -X utf8 scripts\jsrf_gpu.py <run-dir> --write
python -X utf8 scripts\jsrf_gpu.py <run-dir> --compare <other-run>
python -X utf8 scripts\inspect-jsrf.py memory <run-dir> 0x80001000 256 --out logs\pushbuffer.bin
python -X utf8 scripts\test-harness.py
```

Read `gpu-snapshots.jsonl`, `gpu-report.json`, and `gpu-report.md` with `result.json`,
`stacks.txt`, and `process.dmp`.

`gpu_report_ok` means analysis succeeded, not that boot or GPU execution succeeded.
Snapshots are bounded frozen-state observations, not a complete MMIO history or semantic
hardware model. The offline decoder reads the final dump only.

`RECOMP_GPU_ACK=0` disables synthetic GPU acknowledgement mutations for diagnostics.
Enabled GET movement does not prove command execution. Synthetic fixture progress is
never acceptance evidence for real guest GPU behavior.

## History

Dated checkpoints, superseded defects, old run IDs, old commit IDs, previous model
assignments, and rationale narratives belong in `docs/jsrf-operating-history.md`.
Reports are transient scratch logs, not a durable history sink. Read history only when
investigating provenance or why a current rule exists.
