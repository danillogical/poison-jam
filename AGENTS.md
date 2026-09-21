# JSRF Windows recompilation: agent working guide

## Purpose and operating intent

Port Jet Set Radio Future to Windows through Xbox static recompilation. First
reach a playable opening area with movement, graffiti, audio and save/resume;
then extend coverage to the whole game. Work in small, verifiable milestones.
The user is learning the process: explain each defect, its evidence and result.

Read this guide, the current milestone in `plan-jsrf-bare-minimum.md`, and the
current status in `report-jsrf-bare-minimum.md` before selecting work. The plan
owns acceptance criteria and statuses; this guide owns operating knowledge.
When the session is Grok, also read `grok-role-map.md` for model, effort,
and spawn mechanics. Keep the report updated during work, with **bold Decision
entries for choices made on your own recommendation**. The user reads it live
in Grok Build (historical Codex sessions used this same guide). Proceed
through ordinary implementation choices without repeatedly asking permission;
current user instructions take precedence. Preserve unrelated changes.

This guide must support a fresh Sol, Terra, Luna, or Grok agent without the
previous conversation. Current Grok sessions use `grok-role-map.md`. The
parent orchestrator is Grok 4.7 xhigh, and that same budget reviews.
The user has authorized price-conscious delegation as the standing
workflow for suitable JSRF implementation packets; no fresh delegation request
is needed for each packet. Sol at low effort is the orchestrator for 11b packet
decomposition and evidence review. Luna at medium effort is the default worker
for narrow original-XBE disassembly contract tables, well-understood recovery
entries within approved manifests, bounded ABI/behavior tests, offline
decoder/harness scripts, documentation and localized fixes with clear expected
behavior. Routine GPU/register/mapping implementation belongs to Luna once its
contract is clear; GPU or kernel work is not automatically Expert work.

Home Qwen is optional parallel capacity for logs, symbols, reconstruction and
test drafts. It is never a critical prerequisite. Prefer sequential small
packets; use one optional second Luna only for an independent useful packet.
Sol at medium effort is reserved for a concrete ambiguity or the same failure
after two Luna attempts, especially ABI/translation, weak-memory,
multithreading, subsystem or contradictory-evidence problems. Astra at medium
effort is reserved for an incorrect architecture, interface or task graph and
returns implementation to Luna. After two Expert attempts on the same root
problem, collect missing evidence or request an architectural replan when
supported by that evidence. Terra at low effort independently reviews meaningful
changes. It may batch a few closely related Luna packets before accepting their
combined result or starting a dependent milestone. Workers still run their own
isolated acceptance tests. Trivial mechanical edits fully verified by objective
checks do not need a separate reviewer.

| Role | Actual model / effort | JSRF selection rule |
|---|---|---|
| Orchestrator | `gpt-5.6-sol` / `low` | Plan the next small packet, adjudicate evidence and integrate. |
| Standard Worker | `gpt-5.6-luna` / `medium` | First choice for implementation once the contract is clear, including GPU work. |
| Expert Worker | `gpt-5.6-sol` / `medium` | Two failed Luna attempts or concrete unresolved root-cause ambiguity. |
| Architect | `gpt-6-astra` / `medium` | Evidence shows the design, interfaces or task graph must change. |
| Reviewer | `gpt-5.6-terra` / `low` | Independent functional review of meaningful changes. |

Grok sessions follow `grok-role-map.md` instead of the Codex model/effort
column above. Packet roles, escalation gates, and ownership rules stay the
same: the parent orchestrates and reviews at Grok 4.7 xhigh, sequential
leaves stay on that parent or a fresh subagent, and plan mode is
architecture. Codex spawn flags (`collaboration.spawn_agent`,
`fork_turns`) do not apply; use `spawn_subagent` with a self-contained
prompt. Children inherit the parent model.

Default mode is Away, with no local-provider calls. Select Home explicitly for
optional Qwen capacity. Each worker receives the mode and stays in its assigned
role; it does not recursively launch this pipeline.

Every delegated packet names its milestone, expected outcome, source range and
files, facts versus hypotheses, ABI/interface constraints, baseline artifact,
exact acceptance commands and file ownership. One owner performs build,
regeneration and run. Codex sessions use `collaboration.spawn_agent` with
`fork_turns="none"` and explicit `model`/`reasoning_effort` so the worker does
not inherit an expensive parent model or the full investigation history. Grok
sessions use `spawn_subagent` with a self-contained prompt as described in
`grok-role-map.md`; children inherit the parent model and cannot spawn further
workers. Role names in custom agents are optional and do not imply a mandatory
first hop or recurring fresh Sol/Astra involvement. Keep original assets and
existing saves unchanged.

## Workspace and source map

PowerShell, project `C:\Users\logic\Repos\my_xbox_game`.
Toolkit `C:\Users\logic\Repos\xboxrecomp`. Inspect applicable toolkit instructions
before editing that sibling; this file does not establish scope over it.
Read toolkit `docs/GETTING_STARTED.md` Step 8, `docs/technical/indirect-calls.md`
and `lessons-learned.md` as needed. Other-title examples are not JSRF evidence.

| Location | Role |
|---|---|
| `CMakeLists.txt` | Game, collector, regression targets; optimized PDB/map build. |
| `src/main.c` | XBE loading, runtime/log initialization, probes, guest entry. |
| `src/recomp_manual.c` | Manual dispatch and fatal unresolved-call diagnostics. |
| `src/jsrf_crt.c` | Verified memmove replacement and guest cdecl contract. |
| `src/recomp/gen/` | Generated functions, dispatch, declarations, runtime macros. |
| `src/recomp/recovered/recovered.c` | Reviewed recovery bodies and return checks. |
| `config/manual-functions.json` | External definitions protected during regeneration. |
| `config/recovered-functions.json` | Reviewed real entries, spans, cleanup, evidence. |
| `config/boundary-fixes.json` | Reviewed parent spans; internal labels are not entries. |
| `config/recovery-unresolved.json` | Trapped dependencies of recovered functions. |
| `scripts/recover-functions.py` | Recovery generation, TODO rejection, merged database. |
| `scripts/relift-selected.py` | Targeted atomics, wide comparisons and boundary relifts. |
| `scripts/build-jsrf.ps1` | Recovery/test generation and guarded Release build. |
| `scripts/build-identity.py` | Source, executable, collector and PDB/map identity verified. |
| `scripts/run-jsrf.py` | Bounded debugger launch and full artifact archive. |
| `scripts/run-jsrf.ps1` | Thin forwarder to `run-jsrf.py` for existing PowerShell commands. |
| `src/diagnostics.c`, `src/diagnostics.h` | Guest thread registry and event histories. |
| `tools/harness/collect.c` | External debugger, all-thread stacks and minidumps. |
| `tests/harness_probes.c`, `tests/video_probes.c` | Concurrency/failure/video contracts. |
| `scripts/test-harness.py` | Thirteen positive/negative run and capture checks. |
| `tests/test_memmove.c`, `tests/lifter-regressions.c` | Compiled behavior/ABI regressions. |
| `scripts/inspect-jsrf.py` | Original-XBE disassembly, exact guest memory export and GPU report. |
| `tools/harness/gpu_capture.h` | Bounded frozen GPU register/device/pushbuffer snapshots. |
| `scripts/jsrf_dump.py`, `scripts/jsrf_gpu.py` | Validated minidump reader and offline NV2A packet inspection. |
| `tests/gpu_probes.c`, `tests/test_gpu_inspection.py` | Four GPU capture probes and 20 offline inspection tests. |
| `tests/native_gpu_smoke.c`, `scripts/test-native-gpu.ps1` | Separate D3D11 clear/copy/readback fixture and capture tools. |
| `scripts/verify-initializers.py` | Independent XBE interpreter versus captured writes. |
| `tests/test_nv2a_contract.c` | Eleven real-core register/clock contracts; no renderer. |
| `docs/jsrf-gpu-setup-contract.md` | Reviewed setup addresses, primary references and integration gaps. |
| `docs/jsrf-callback-reentry-contract.md` | Original-XBE `0x00193D90`/`0x00194210`/`0x00197AAC` ABI, KeSetEvent, reentry. |
| `tests/test_callback_reentry.c` | Bounded 5,440-check callback/signal/reentry contract fixture. |
| `grok-role-map.md` | Grok 4.7 xhigh overlay for Codex role names, review, and spawn. |
| `report-grok.md` | Session decisions and issues while executing without stopping to ask. |
| `game/default.xbe`, `game/Media/` | Original executable/assets; do not modify. |
| `tools/disasm/output/`, `tools/func_id/output/` | Original analysis/classification. |
| `jsrf_run.log` | Latest log, overwritten by a launch. |
| `logs/runs/` | Archived logs, binaries, symbols, source, thread reports and dumps. |

Toolkit edits currently span `src/kernel/kernel_bridge.c`, `kernel_rtl.c`,
`xbox_memory_layout.c`, new `recomp_diagnostics.h`, `templates/runtime/recomp_types.h`,
`src/nv2a/nv2a_core.c`, and `tools/recomp/lifter.py`, `translator.py`. Inspect both
working trees before editing. The user requested local commits of this checkpoint.
Game Git now records the source, generated code, harness and documentation;
assets/builds/logs/saves are ignored. The accepted GPU harness/submission
checkpoint is game `1e62b6f` with toolkit `3db44d7`. The 11c1 recovery and
bounded service-contract checkpoint is game `9f0e4e0`; its thread-safe timestamp
runtime companion is toolkit `3359892`.

The PRAMIN-instance-backing and RAMHT-lookup checkpoint is game `8641320`
(toolkit `c98dbdc`). It records recovery of the GPU setup path from
`0x00194ADD` through the first pushbuffer kick, the production RAMHT
handle-to-class walk, the completed in-place event bridge, and the
`run-jsrf.py` run wrapper. The first fatal target is `0x001918E0`; GET=PUT at
the stop. Toolkit unit suite 27/27, game Release CTest 11/11 at both revisions.
`0x001918E0` and its kick chain (`0x001917F0`, `0x001916B0`, `0x00191530`,
`0x00190240`) are deliberately still fatal until the kick/GET contract exists.

The kick/GET contract checkpoint is game `3c6fd81` (toolkit `488286f`). It adds
`docs/jsrf-kick-get-contract.md`, which maps the ring control block at
`MEM32[0x0019DCE0]` and the whole kick chain, and implements the model-owned
`NV_PFIFO_CACHE1_DMA_PUT` bit-16 kick latch. The kick is a register write plus a
poll until the engine clears bit 16 (original `0x00191270`, inlined at
`0x001912A0`), so the model must clear it or the guest spins forever. The
offset mask is `0x1FFEFFFF`, NOT the generic `0x1FFFFFFF`, which contains bit
16. `NV_USER_DMA_PUT` writes route through the canonical PFIFO register slots.
NV2A contracts 319, Release CTest 11/11, ordinary stop
`logs/runs/20260921-111607-936-kick-ack/` unchanged at `0x001918E0`. The chain
is now unblocked for recovery; expect the next packet to stop on
`unsupported_method` and treat that stop as the intended result.

The pre-harness source snapshot remains `logs/snapshots/before-harness.zip`.
Game baseline commit: `185a564`. The historical GPU section below predates the
committed checkpoints just listed. Preserve the unrelated untracked
`xboxrecomp/recomp_run.log`. Inspect both working trees before starting work.

**Build note for this machine.** This shell exports both `http_proxy` and
`HTTP_PROXY` (and the https pair). MSBuild's `CL.exe` task tracking adds those
names case-sensitively and dies with `MSB6001 ... Item has already been added.
Key in dictionary: 'https_proxy'`. Run builds as
`env -u http_proxy -u https_proxy cmake --build ...`. The toolkit Python
scripts additionally need `capstone`, which lives in
`C:\Users\logic\AppData\Roaming\Python\Python313\site-packages`; set
`PYTHONPATH` to that path (Windows form) and use `C:\Python313\python.exe`.
Neither workaround changes any project source.
The guarded chain is `scripts/build-jsrf.ps1`, but PowerShell here refuses it
twice over: the execution policy blocks the script, and `cmake` is not on the
PowerShell PATH. Run the same steps directly with the Bash tool — `cmake -S . -B
build`, `recover-functions.py`, `generate-lifter-tests.py`, `build-identity.py
before`, the six-target `cmake --build` with the `env -u` prefix, then
`build-identity.py after` — which is what the chain does and produces the same
artifacts.

**A probe run expects its probe checkpoint only.** `--probe=` returns early in
`src/main.c:148`, before `checkpoint("guest_entry")` at `main.c:162`, so a probe
run never emits `guest_entry`. `run-jsrf.py` defaults the expectation to
`['memory_ready', 'guest_entry']` when no `--expect-checkpoint` is given, which
is right for a plain run and reports a false `checkpoints_passed: false /
missing_checkpoints: ["guest_entry"]` for a probe run. Pass
`--expect-checkpoint probe_gpu` for `--probe gpu-submit-supported`. The guest did
reach its entry point; only the expectation was wrong.

**Six findings as of 2026-09-21.** Items 1 and 6 are open. Items 2, 3, 4 and 5
are fixed and kept here as operating knowledge, because each one was invisible
until it was diagnosed and each could return.

1. **This repository's `.git` has no object store.** It holds `HEAD`, `config`,
   `index` and `logs/` but no `objects/`, no loose refs and no `packed-refs`,
   and no remote is configured, so `git status`, `git log` and `git cat-file`
   all fail with `fatal: not a git repository`. The working tree is intact and
   `.git/logs/HEAD` lists ten commits up to tip `7e2c4f43`. Do not "fix" this
   with `git init`; restoring `objects/` from a backup or snapshot preserves the
   history, and only the user can choose. The toolkit sibling
   `C:\Users\logic\Repos\xboxrecomp` is healthy.
2. **The generated chunk tree did not link, for a reason that was not the
   detector.** FIXED 2026-09-21: `jsrf_recomp.exe` builds, identity-verified.
   The header declared 541 unresolved stubs while
   `recomp_stubs_unresolved.c` defined 3, because
   `scripts/recover-functions.py` rewrote that file from its own 70-entry view
   of `referenced_calls` and deleted the bodies the full pass had written. The
   two halves are now produced separately and consistently: the full
   `translate_batch_split` owns `recomp_funcs.h`, the chunks, the dispatch
   table and `recomp_stubs_unresolved.c`; `recover-functions.py` owns only
   `recomp_stubs_recovery.c`. Never let the recovery script write a file the
   full pass generates.
3. **Mid-body tail-jump targets were emitted as tail calls.** A
   `tail_jump_alias` entry begins inside its parent and shares the parent's end,
   so a jump back into the parent's earlier blocks looked external and was
   emitted as `g_seh_ebp = ebp; sub_X(); return;` for an address that is not a
   function. `tools/recomp/lifter.py` now consults the batch's spans
   (`set_batch_spans`, installed by `translate_batch_split`) and emits
   `goto loc_X` for a target inside any translated body. That removed 267
   unresolved targets (541 -> 274). Committed with item 4 as toolkit
   `58a9cf9` (`488286f` was the base).
4. **Alias entries are fragments, not functions, and the live jumps into them
   were silently deleted.** FIXED 2026-09-21. `translator.py` validates every
   `goto` against the labels defined in the same function and rewrites a missing
   one to `(void)0; /* ... dead code ... */`. Because a `tail_jump_alias` body
   contained `goto loc_X` for a label owned by its *parent*, those jumps were
   rewritten away: 2623 rewrites, **1032 of them inside a live `if`**. The proof
   case: `0x00011133` `mov esi,[esi+0x34]` / `test esi,esi` / `jne 0x000110D0` is
   a linked-list traversal loop, and the emitted alias `sub_00011105` replaced
   the backward branch with `(void)0`, so the loop ran once.
   `translate_batch_split` now collects aliases into `alias_parent`, folding each
   into the **nearest** earlier entry sharing its `end`, resolved through
   `_resolve_owner` so an alias-to-alias chain ends at a real body. Each one's
   dispatch entry names that owner while **keeping its own VA**, and aliases are
   kept out of the header entirely. Same-function live-jump deletions went
   **1032 -> 0**; the exe went 19,245,568 -> 10,892,800 bytes.
   Two traps here, both learned by breaking the link: a declared-but-undefined
   alias has no address, so the dispatch tuple cannot name it (2537 unresolved);
   and declaring it in the header makes every including unit reference it (1666
   unresolved). The alias must be *dispatched*, never *defined or declared*.
   Do not "fix" a remaining rewrite by weakening the label validator: a `goto`
   into another function is not valid C, and the validator is right to reject it.
5. **A real tail call was classified as an intra-body goto.** FIXED 2026-09-21.
   Item 3's span test asked "is the target inside a batch span", but a function
   start is inside its own span by construction, so **every** real tail call
   answered yes and was emitted as `goto loc_X`. C has no cross-function `goto`,
   so the validator then deleted it: `0x00011C0E jmp 0x12890` targets
   `sub_00012890`, a `call_target` function also reached by a direct call at
   `0x00011C5A`, and it was rewritten to `(void)0`. `_is_external_target` now
   discriminates on `detection_method`, not on span membership: a
   `tail_jump_alias` is a fragment and stays a `goto`; any other entry is a real
   entry point and stays a tail call even when its span overlaps. Deleted gotos
   **1309 -> 309** (cross-function 628 -> 301, absent-label 679 -> 8,
   same-function 2 -> 0); live `if (…) (void)0` 503 -> 273; **864** real tail
   calls now emitted. The lesson generalises: "is it inside a span" is true of
   every entry, so it can only ever subtract, and what it subtracted was genuine
   tail calls. Classify on what the entry *is*, never on where its span falls.
6. **The residual deleted jumps: three independent causes, all in alias folding.
   Causes A and B FIXED. A distinct real-guest-run ICALL is open — the fix is
   correct but has an unresolved interaction; do not ship until it is chased.**
   Measured
   against the **live** database (`tools/disasm/output/functions.json`, the one
   the generator loads at `__main__.py:268`).
   *Cause A (FIXED 2026-09-21, toolkit `ff4d442`).* `translate_batch_split`
   folded an alias into a parent chosen by "same `end`, earlier `start`". **2,902
   of the 3,151 aliases end exactly where a real entry *starts*** — a shape that
   rule cannot match, because the only entries sharing that `end` are other
   aliases. So `_resolve_owner` found no body, and the alias was emitted as its
   own `void sub_*(void)`: a second definition of code the function below
   already emits, and a carrier for `goto loc_X` labels owned by that function.
   A second rule now adopts a parent whose start equals the alias's end, gated
   on the same-end rule running first, on the adopter being a **real** entry
   (never another alias) and on the section matching. Deleted jumps
   **309 -> 115**, duplicate alias bodies **958 -> 0**, header declarations
   6,072 -> 5,714, exe 10,900,480 -> 9,532,416 bytes.
   *Cause B (FIXED 2026-09-21, toolkit `db54746`; supersedes the `5d68f27`
   diagnosis below).* The 115 were **mostly not** branch targets in another
   function's body. They were 71 false functions created by
   `probes_as_prologue` accepting a bare `mov edi,edi` (`8b ff`) as a prologue.
   `8b ff` is also the 2 bytes MSVC leaves in front of a **switch table**, so
   the pass claimed table data as code. The rule now requires a run of >= 4
   consecutive dwords that are each the start of a *decoded instruction*,
   beginning at `+2` — 60 of 71 suspect entries fail it, 0 of the 292 genuine
   `gap_prologue` entries do. Two weaker tests were tried and discarded because
   they **saturated** (a `.text`-dword run passed 71/71 and 292/292; an
   undecoded-instruction count was 0 for both). Effect: `gap_prologue` 363 ->
   303, deleted gotos 115 -> 40, declarations 5,714 -> 5,652.
   **The fix repairs 91 truncated functions** (e.g. `0x000208DD` end `0x000208E2`
   -> `0x00020900`) and **adds 0** entries.
   *Verification after the fix.* The fixture probe is **byte-identical** to the
   pre-fix build: run `20260921-144001-732-deepeek-fixture-gpu-progress` matches
   `20260921-132312-868-deepeek-parent-fix` field for field (`normal_exit`,
   `exit_code 0`, `checkpoints_passed true`, 4 snapshots, 36 named frames).
   **Do not compare a fixture probe against a real guest run.** A `gpu-*`
   `--probe` never executes guest code (zero `[KERNEL]`/`[RECOVERED]` lines) and
   `test-harness.py` gives it `--expect-checkpoint probe_gpu`, whereas a real run
   (`probe=""`) uses the default `["memory_ready","guest_entry"]`. I confused the
   two and published a regression that was not one.
   *The open defect, measured against a real guest run* (`probe=""`,
   `20260921-111607-936-kick-ack` on `c98dbdc6` vs `20260921-143443-445-run` on
   `9568f29`): both are `unhandled_exception` with `exit_code 0xE0464643`, both
   reach `guest_entry` at log line 48, both issue 157 kernel calls. The real
   difference is that the pre-fix run logged **48** `[RECOVERED] ... ABI verified`
   checks and died at `[ICALL] Failed to resolve VA 0x001918E0` after 379 thread
   calls, while the post-fix run logs **0** and dies at
   `[ICALL] invalid target 0x00700010 ... return=0017E627`. So the failure moved
   *earlier*, into the demo's startup path. The site is `sub_0017DBBD`: it forms
   `ecx = MEM32(edi + 8) + index*8` and pushes `MEM32(ecx + 4)` as a callback to
   `sub_0017E600`, which dispatches it via `call eax` at `0x17E625`. `edi` is
   `[ebp + 0x10]`, a runtime-built table descriptor; `0x00700010` is in no
   image section. Callers of `sub_0017DBBD`: `0x0017DCE6`, `0x0017DD6B`,
   `0x0017DDAE`, `0x0017E004`, `0x0017E2EE`. **Next packet: find who supplies
   `[ebp + 0x10]` and what fills callback slot `[base + index*8 + 4]`.**
   Do not revert: the removed entries really are jump tables (every
   dword in `0x000FFFAA`'s "body" is a `.text` address, some runs terminated by
   `90909090`), and `0x001005B5` — owned by nothing before — is now correctly
   owned by the alias `0x000FFFD0`.
   *Superseded `5d68f27` note, kept because the mechanism is still real:*
   a branch target that genuinely lives in another function's body. The alias
   pass added for it took entries 8,742 -> 8,848 and aliases 3,045 -> 3,151,
   but did not move the 309. **Do not measure this against
   `tools/func_id/output/identified_functions.json`
   — it is stale, uses `method`/`confidence` rather than `detection_method`, and
   nothing in the current pipeline reads it.** I made that mistake and reported
   161 "provably phantom" entries and a `_pass_phantom_entries` fix; both were
   artifacts of the stale file. Re-run against the live DB the count is 0, and
   the pass was reverted. That stale file also gives `0x74004` as
   `method=none, confidence=0.0` where the live file has it as
   `imm_ref_target, confidence=0.86`.
   **`recover-functions.py` is NOT the translation pass.** `src/recomp/gen/`'s
   `recomp_NNNN.c` chunks are regenerated only by the manual invocation
   documented below, which no build step runs. I measured a fix as having "zero
   effect" because I had regenerated only via `recover-functions.py` and was
   counting the old tree's numbers. **Regenerate the chunks explicitly before
   measuring any translation metric.** The tell was an alias still declared in
   `recomp_funcs.h` that the new rule should have folded.
   **A "clean" regeneration must also clear `tools/disasm/output/*.json`**, not
   just `.disasm_cache.json`. The disassembly JSON is an *input* to the
   translation, so clearing only the cache leaves the translation reading a
   stale partial database — that produced a phantom "5,074 declarations / 36
   gotos" result that I briefly published as the outcome. The real figures are
   5,652 / 40, reproduced exactly from a cold start.
   **Toolkit revision for items 3, 4 and 5: `a301962` (`488286f` was the base,
   `58a9cf9` was items 3 and 4). Item 6: `5d68f27` (branch-target alias pass),
   `ff4d442` (Cause A fix), `db54746` (Cause B fix, `ff4d442` was the base) and
   `9568f29` (two recomp tests that read another module's output dir).**

**The full translation pass has one correct invocation.** It regenerates
`recomp_funcs.h`, the `recomp_NNNN.c` chunks, `recomp_dispatch.c` and
`recomp_stubs_unresolved.c`, and those four must come from the same run or the
tree will not link. Nothing in `scripts/` runs it, so it is easy to invoke by
hand and get it subtly wrong — which is what happened on 2026-09-21, when
omitting the two flags left `sub_0017CEC0` and `sub_00190240` double-defined:

```
python -m tools.recomp game/default.xbe --all --split 1000 \
    --gen-dir src/recomp/gen --game-name "Jet Set Radio Future" \
    --manual-functions config/manual-functions.json \
    --exclude-manual src/recomp_manual.c
```

`--exclude-manual` reads the hand-written C source directly so the manual set
cannot drift from the file that defines it. `scripts/build-jsrf.ps1` does NOT
run this pass; it runs `recover-functions.py`, which owns only
`src/recomp/recovered/recovered.c`, the focused fixtures, the trap file and the
run inputs. Regenerate the chunks deliberately, not as part of a routine build.

**Files in `src/recomp/gen/` are linked into the game.** `CMakeLists.txt` globs
`src/recomp/gen/*.c` into `jsrf_recomp`, so anything left in that directory
becomes part of the game. A fixture source placed there is compiled into
production as well as into its test target, and because a fixture defines a
deliberate subset of the same symbols production defines, that always fails the
link with a wall of `LNK2005` then `LNK1169`. Fixtures belong in
`src/recomp/gen/fixtures/`; the glob additionally filters `recomp_.*_test\.c$`
as a standing guard.

**Boundary fixes need two steps.** A `config/boundary-fixes.json` entry widens
the span used by `recover-functions.py`, but only
`python -X utf8 scripts/relift-selected.py boundaries` rewrites the body already
generated into `src/recomp/gen/recomp_*.c`. Run both, then check whether the
widened span exposes calls to symbols the declarations file does not cover:
`src/recomp/gen/recomp_*.c` needs a local `extern` for any such symbol, because
`recomp_funcs.h` is generated from the raw function database and will not
declare something the raw split missed.

**`config/manual-functions.json` is written by the generator.** It appends every
recovered address, and `relift-selected.py` reads it back as pre-existing symbol
names. It is not purely a hand-maintained external-definition list, so an
address that is both recovered and still defined in an unreviewed chunk will
appear in both places and fail to link. Remove such an address from the file
once it is recovered.

## Verified checkpoint — 2026-09-12

- XBE title `0x5345000A`, XDK 4134, entry `0x00148023`; 8,437 analyzed functions,
  120 imports. SHA256:
  `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C`.
- Latest ordinary run: `logs/runs/20260912-231756-543-gpu-harness-final/`.
  Expected fatal missing target `0x00194ADD`; six native threads, 52 named
  frames and successful dump. This is a failure checkpoint, not a completed boot.
- The first failure `0x0017D15C` was an internal backward-copy block of truncated
  memmove `0x0017CEC0`. Host memmove replaces the complete guest routine.
- Fourteen startup initializers run real translated instructions; all 616 words
  independently match the original XBE. Twenty recovered functions now include
  these initializers and six D3D routines. Six parent boundaries are corrected.
- The heap hang came from lost LOCK XADD flags causing premature/double frees.
  Atomic flags now use the operation's result snapshot; 82 functions relifted.
  Repeated word/dword comparison flags also used stale prior flags (unequal
  GUIDs appeared equal); corrected tracking and zero-count ZF, 140 relifts.
- Release CTests 5/5 pass: 36,900 memmove cases, 288 atomic cases, 300 string
  comparison cases, WBINVD helper/flags, 11 GPU register/clock contracts,
  20 offline GPU inspection tests and a 4,096-pixel native D3D11 WARP readback.
  Fixed a standalone NV2A reset-frequency mismatch; its compiled before/after
  evidence is in `logs/gpu-registers-before.txt` / `gpu-registers-after.txt`.
  This backend is not yet driving guest GPU execution. Toolkit unit suite: 27 pass.
  The original atomic lifter fails a compiled regression in `logs/lifter-before/`.
- All thirteen harness checks pass, including video and four GPU probes. See `logs/harness-test-results.json`.
  A forced kernel dispatch race failed before and passes after TLS isolation.
  Dumps now include the separately allocated contiguous window and current GPU
  register backing. Marker contents are verified in the concurrency probes.
- Video queries verify 24 modes, 212-byte caps copy, invalid inputs, bounds and
  return contracts. Game requests 640x480, format 0x11, two back buffers.
  Device at `0x0019B200`; `0x00191020` allocates its 512 KiB pushbuffer.
- GPU initializer `0x00192090` reaches missing hardware helper `0x00194ADD`.
  The initializer has NOT returned successfully. No game frame/window yet.
- Milestones 00–05 (including core harness) are verified as detailed in the plan.
  Broader 01c/01d coverage remains open. Next priority is GPU packets 11a–11d,
  alongside kernel/thread coverage required by those packets.

## Build, test and investigate

Run from the game root:

```powershell
.\scripts\build-jsrf.ps1
ctest --test-dir build -C Release --output-on-failure
python -X utf8 scripts/run-jsrf.py --seconds 5 --label gpu-setup
Get-Content .\jsrf_run.log -Tail 50
python -X utf8 scripts/verify-initializers.py logs/runs/<run-directory>
python -X utf8 scripts/test-harness.py
python -X utf8 scripts/inspect-jsrf.py disasm 0x00194ADD 0x00194C3F
python -X utf8 scripts/inspect-jsrf.py memory logs/runs/<run-directory> 0x0019B200 128
```

Replace angle-bracket placeholders before execution. From the toolkit root:

```powershell
python -m unittest tools.recomp.test_lifter_atomics tools.recomp.test_lifter_string_compare tools.recomp.test_lifter_carry tools.recomp.test_seh_frame_owner
```

The build wrapper regenerates reviewed recovery code and compiled regression
fixtures, then checks source identity before/after compilation. Never edit
compiled sources during a build. Runner identity guards hash the game EXE/PDB/map, collector EXE/PDB and native
GPU fixture EXE/PDB as well as compiled source. They reject stale artifacts;
a previous failed build/run is documented in the report and is not evidence.
Build logs: `logs/configure-current.log`, `logs/build-current.log`.
Generated/toolkit warnings remain; review new warnings instead of ignoring all.

The executable is `build\Release\jsrf_recomp.exe`; its working directory must
be the game root. Prefer the bounded runner for automated work. It refuses a
concurrent game copy, uses an external debugger, disables the legacy watchdog,
archives artifacts and restores the environment. Inspect executable paths
before stopping any existing process; never kill unrelated processes.

Read archived `result.json`, `jsrf_run.log`, `stacks.txt` and `process.dmp`.
Outcomes distinguish normal exit, unhandled exception, diagnostic deadline and
collector failure. Script nonzero results are expected for captured failures;
PowerShell wrappers may present nonzero codes differently, so inspect JSON.
Handled first-chance exceptions are not crashes. Required checkpoints must
match exact names; additional output belongs on separate lines.

After runtime changes, use meaningful behavior/ABI regressions and a game smoke
run. Re-run collector probes after harness/thread-tracing changes. Documentation
changes alone do not require a rebuild. Stop repeating checks after they pass
unless new edits or unresolved evidence justify it.

## Guest code and regeneration discipline

Guest pointers are 32-bit Xbox VAs; host pointers are 64-bit. Translate with
runtime mapping (`XBOX_PTR`, `MEM32`, etc.), never truncate/store native pointers
in guest fields. Generated functions use `void(void)` but communicate through
guest registers and a simulated stack. Host C calling conventions do not supply
the guest ABI. Establish argument positions, return value, saved registers and
cleanup from original instructions: RET pops four bytes, RET N pops four plus N.
Memmove uses `[esp+4/+8/+12]`, returns guest destination in EAX, pops four only.

Classify missing targets before patching: real omitted function, internal label,
kernel thunk, invalid pointer. Alignment or section membership is insufficient.
Preserve real initialization/output state. Do not silence failures with blanket
success, arbitrary stack resets or guessed pointer guards. Any temporary stub
needs a documented contract and removal gate. Manual lookup may not intercept
direct calls: maintain one definition, direct symbols, dispatch and manifest.
Never resurrect generated memmove or seed its internal `0x0017D15C` as an entry.

Recovery boundaries and evidence live in the manifests. The build wrapper runs
`scripts/recover-functions.py`; it rejects unhandled-instruction TODOs, emits
reviewed definitions, and creates `tools/disasm/output/functions.recovered.json`.
For boundary changes also run `python -X utf8 scripts/relift-selected.py boundaries`.
Atomic and comparison relifts use the corresponding script modes. Review the
affected symbols and preserve installed names before regenerating more code.

Avoid complete regeneration for small changes. Full regeneration with all new
manifests is NOT verified yet. If necessary, snapshot generated/analysis files,
use this game's paths and merged database in a separate analysis directory,
and preserve the manual manifest. The original functions database is retained.
Review definitions, stubs, direct calls and dispatch; rebuild/test/run afterward.
The generated runtime header is not necessarily refreshed by regeneration;
synchronize it deliberately with the toolkit template without losing edits.

Unresolved and invalid calls stop by default with `0xE0424943`, including all
541 formerly silent direct stubs. `JSRF_ALLOW_UNRESOLVED=1` enables some legacy
fallback only; never use it as acceptance evidence. Newly generated recovery
dependency traps remain fatal. Record the earliest actual failure and caller.

ABI caveat: generated architectural EBP is often a C local; global `g_ebp` is
frame metadata not consistently restored by existing callees. Recovered routine
wrappers check ESP/EBX/ESI/EDI. Straight-line initializer wrappers additionally
check unchanged frame metadata. This is not a full architectural-EBP audit.
`JSRF_TRACE_HEAP=1` enables bounded temporary tracing and catches freeing a chunk
whose busy bit is clear; normally leave it off.

## Harness and concurrency boundaries

The external collector freezes all native threads at a debug event, preserves
named native stacks, raw guest stack words, canonical guest RAM, live registers,
thread lifecycle/identity and bounded per-thread call/tail/lock/wait histories.
It also captures the separate 64 MiB contiguous/pushbuffer/instance window and
16 MiB readable GPU aperture. These are distinct from canonical RAM today.
Unreadable/trapped ranges are explicitly skipped; once a GPU model owns trapped
registers it needs a frozen model-state snapshot. The prior dumps lacked these
regions; use the current checkpoint for GPU-memory investigation.
ICALL history is partial, not a full guest call stack; raw stack words are not
unwound frames. Dumps preserve matching binaries/PDBs/maps and source archives.

- Registry slots are never reused in a run; 128 lifetime capacity and overflow
  are reported. Exited TLS pointers are never dereferenced during collection.
- Read histories only when all threads are frozen. Incomplete odd publication,
  ring overwrites and capacity loss are explicit; this is not a live-read API.
- Legacy ICALL and kernel dispatch selection are thread-local. Other optional
  profiler/trace paths still need audits before heavy concurrency use.
- Fixtures use native workers with actual runtime stacks/TIB/TLS and kernel
  event bridges. They do not prove full PsCreateSystemThreadEx bootstrap/exit,
  semaphore/multiple-wait matrices, ISR/DPC execution or arbitrary race freedom.
- `RECOMP_WORKERS=inline` changes semantics and can deadlock. Use only as a
  controlled experiment, not a correctness fix or acceptance evidence.
- Dumps cannot explain every earlier racing write. Add focused event/write
  histories and forced interleavings; record perturbations and timing effects.

## GPU harness: commands, interpretation and installed tools

The completed harness pass is milestone 01f. It improves diagnosis, not guest
GPU emulation. Ordinary startup now stops at `0x001918E0` inside `0x00192090`
after WBINVD and framebuffer publish. The latest GPU report has USER GET/PUT
both `0x1000` and an empty pending queue.
Do not classify that pre-submission state as a corrupt packet or successful work.

```powershell
# Ordinary failure capture and offline inspection; replace RUN with its directory.
python -X utf8 scripts/run-jsrf.py --seconds 5 --label gpu-setup
python -X utf8 scripts/jsrf_gpu.py RUN --write
python -X utf8 scripts/jsrf_gpu.py RUN --compare OTHER_RUN
python -X utf8 scripts/inspect-jsrf.py memory RUN 0x80001000 256 --out logs/pushbuffer.bin
# Runner disables GPU acknowledgements automatically for each GPU probe.
python -X utf8 scripts/run-jsrf.py --seconds 3 --probe gpu-stall --label gpu-stall
python -X utf8 scripts/test-harness.py
# Standalone native graphics fixture, independent of guest startup/collector.
.\scripts\test-native-gpu.ps1
.\scripts\test-native-gpu.ps1 -Warp
.\scripts\test-native-gpu.ps1 -Capture renderdoc
.\scripts\test-native-gpu.ps1 -Capture nsight
```

Read `gpu-snapshots.jsonl`, `gpu-report.json` and `gpu-report.md` alongside
`result.json`, `stacks.txt` and `process.dmp`. The runner automatically analyzes
GPU data and records `gpu_report_ok`. Successful analysis is not successful boot.
Snapshots occur while the debugger freezes all threads: cooperative exception
`0xE0424750` or a full failure/deadline capture. Maximum 64 snapshots and 64 preview
words each; dropped snapshots are reported. Values are raw register storage,
not a semantic hardware model. Unreadable values are null, never substituted zero.
There is no periodic sampling or complete MMIO write history. A future trapped
MMIO backend must expose a frozen model-state snapshot.

The offline decoder reads the FINAL dump only, with exact guest-to-host mapping;
canonical RAM and separately allocated contiguous memory are not interchangeable.
Earlier previews cannot reconstruct an overwritten ring. It recognizes increment,
non-increment, wrap, old jump, jump and single-slot call/return; defaults bound
inspection to 1,024 words and 256 packets. Loops, truncated/reserved packets,
unreadable memory and invalid addresses are diagnosed without executing commands.
Its primary reference is the pinned xemu PFIFO source linked in `scripts/jsrf_gpu.py`.

`RECOMP_GPU_ACK=0` disables the legacy worker's GPU acknowledgement mutations;
it leaves the kernel/APU clocks running. The default remains enabled for ordinary
game runs. The collector reads actual `g_nv2a_ack_enabled`, not just environment
intent. Enabled GET movement does not prove execution. GPU fixtures require this
switch and set `g_jsrf_gpu_fixture`; synthetic progress is explicitly labeled.

| Probe | Required diagnostic result |
|---|---|
| `gpu-progress` | Fixture advances GET through two packets; snapshots observe changes and normal exit. No GPU execution claim. |
| `gpu-stall` | GET stays behind PUT; deadline dump identifies waiting worker and unchanged pending queue. |
| `gpu-corrupt` | Truncated packet is reported offline; fixture exits normally. |
| `gpu-unreadable` | GPU aperture becomes PAGE_NOACCESS; null registers and explicit skipped dump region, without collector failure. |

All thirteen probes pass in `logs/harness-test-results.json`. Five CTests pass.
Latest ordinary capture: `logs/runs/20260912-231756-543-gpu-harness-final/`:
expected `0xE0424943`, six native threads, 52 named frames, one GPU snapshot,
no drops, successful dump/analysis and 616 independently verified initializer words.

Installed and used on this machine:

- NVIDIA GeForce RTX 4070 Laptop GPU, driver 616.92; D3D11 debug layer available.
- RenderDoc 1.46.0: `C:\Program Files\RenderDoc\renderdoccmd.exe` and `qrenderdoc.exe`.
  Installed through winget ID `BaldurKarlsson.RenderDoc`. CMake optionally locates
  `renderdoc_app.h` there; reconfigure/build if installed after the fixture build.
- Nsight Systems 2026.5.1:
  `C:\Program Files\NVIDIA Corporation\Nsight Systems 2026.5.1\target-windows-x64\nsys.exe`.
  This is Systems, not Graphics. No additional installation is needed for this pass.

Native fixture selects the NVIDIA adapter explicitly on this hybrid laptop;
`-Warp` explicitly selects software. It requires the D3D11 debug layer, clears a
64x64 texture, copies it, waits on a real event query with a five-second deadline,
and checks all 4,096 pixels plus the debug error queue. Both adapters pass with
zero debug errors. It does not test shaders, draws, swapchain, presentation or
any game command. Capture it separately from the debugger collector; simultaneous
RenderDoc/Nsight/collector instrumentation on one process has not been tested.
The capture wrapper has no general external deadline beyond the native query
limit, so investigate a stuck tool instead of leaving repeated collectors running.

Verified native evidence:

- `logs/native-gpu/20260912-231530-540/renderdoc_capture.rdc` (7,589 bytes).
  Target confirmed capture; converted `renderdoc.xml` contains the named resource,
  annotation, ClearRenderTargetView and CopyResource. That folder's Nsight attempt
  FAILED from an argument bug; the wrapper was fixed afterward.
- `logs/native-gpu/20260912-231601-345/nsight.nsys-rep` (35,994 bytes).
  Exported `nsight.sqlite` contains two D3D11 annotation entries, but its diagnostic
  table warns: 'DX11 profiling might have not been started correctly.' It reports
  11 DX11 events; complete API timing is NOT verified. The wrapper currently
  checks report creation, not trace completeness. Do not call this full profiling
  success. A future tool-only packet can investigate injection/startup timing and
  expose diagnostic warnings in the wrapper before relying on timings.

Nsight profile command uses `--trace=dx11,dx11-annotations --sample=none
--cpuctxsw=none --wait=primary` with a separately constructed output argument.
Export a report with `nsys export --type=sqlite --output=DEST REPORT.nsys-rep`;
inspect DIAGNOSTIC_EVENT and D3D11_PIX_DEBUG_API before interpreting it. Native
capture artifacts live under `logs/native-gpu/` and are intentionally ignored by
Git. The fixture source, symbols, source identity and tool versions are archived.

## Immediate handoff after the GPU harness pass

The user asked to prioritize this guide because weekly usage is nearly exhausted.
Finish this checkpoint without broadening into renderer implementation.
**11b1 through 11b3 are complete (2026-09-19):** the NV2A register aperture has one serialized
owner, conflicting acknowledgement mutations are quiesced, actual concurrent
VEH accesses and decoder flags are tested, frozen model publication is
generation-validated, and teardown is verified. PBUS and the real HAL PCI path
now share validated NV2A configuration backing while identity fields remain
immutable. PTIMER now has pinned split-TIME semantics, deterministic contracts
and a wakeable service owned by the same serialized lifecycle; the 16-probe
harness proves expiry without another guest PTIMER read and clean shutdown.
**11b4a is also complete:** the real trapped USER path consumes physical-offset
pushbuffers only from the contiguous window, atomically records bounded packet
streams, and rolls GET/sink state back on unsupported work. **11b4b1 and
fixture-scoped 11b4b2 are complete:** fixture lookup plus transactional
SET_OBJECT proves per-subchannel binding, and only NV097 NOP and packed clip H/V
are accepted. Format, pitch and offsets remain unsupported and roll the entire
stream back. The full harness has 19 probes. 11b4b3 now has validated PRAMIN
backing: `0x00194913`/`0x001949E2` write through `device+0x700000` into the
claimed instance range shared with `nv_dma_load`. Production SET_OBJECT now
walks that RAMHT; the fixture seam remains for isolated 11b4b2 tests. The
packet is **11c1 leaf `0x00196800`**, the self-contained PLL coefficient decoder
inside `0x00194676`; leaves `0x0019460A` and `0x00194635` are recovered and
directly tested while the parent remains fatal. Follow it with `0x00196967`,
which claims and clears the first real instance-memory range needed by 11b4b3.
Keep recording facts versus
hypotheses, and do not stub helpers merely to reach another missing call.

11c1 has now accepted nine direct leaves: `0x0019460A`, `0x00194635`, `0x00196800`,
`0x00196967`, `0x00196A65`, identity ramp `0x00194533`, and register table
`0x00196B34`, instance clear `0x00196B6A`, and defaults `0x00196B9A`. The allocation leaf establishes the original 0x5000-byte instance
allocation and address derivation, but production intentionally still traps its
unrecovered `0x00194520` dependency and the parent `0x00194ADD`. Its focused
fixture passes 81 checks; combined Release CTest passes 7/7. The next narrow leaf
The real `0x00194520..0x00194533` dependency is now accepted and no longer fatal;
it returns the prior context+0x19C value in EAX while storing the incremented
value. Integrated helper `0x00194676..0x00194780` is also accepted with 1,752
focused checks and the standard routine frame-metadata caveat. The next packet is
`0x00194780..0x001948B9` is decomposed and tiny leaf `0x00196E80` is accepted.
`0x00196C83` is accepted with 1,889 focused checks and an evidence-backed
`restore_frame_metadata` manifest opt-in limited to that balanced prologued
helper. `0x00197BCF` is accepted after a fresh focused rebuild with 1,935 checks.
Timestamp leaf `0x00193C40` and the shared QPC publication contract are accepted:
the toolkit uses `INIT_ONCE`, the focused fixture proves exact EDX:EAX halves,
Release CTest passes 8/8, toolkit tests 27/27, and final run
`logs/runs/20260920-014707-773-timestamp-publication-final/` retains the expected
`0x00194ADD` boundary. The `0x00197C50` chain contains a polling/callback cycle among
`0x00196C0B`, `0x00194210`, `0x00193D90` and `0x00197AAC`; use Sol Medium for
that concurrency contract. The bounded test-only `0x00196C0B` harness is accepted:
1,984 checks over 64 repetitions and Release CTest 9/9. It does not prove the
generic event seam is the real `0x00193D90` signal contract. Luna may now recover
only `0x00196C0B` with test callback seams while production callees remain fatal.
That helper is now accepted with 1,956 focused checks and clean Terra review.
Queue helper `0x00193D10..0x00193D85` is now accepted with 1,988 focused checks;
both `0x0018E500` and `0x0018E584` remain fatal. The real `0x00193D90`
callback/signal and `0x00194210`/`0x00197AAC` reentry contract is accepted
with 5,440 bounded checks; those production bodies and `0x00196C4A` are now
recovered. `0x00194ADD`, `0x00194780`, `0x00197C50`, RAMIN object helpers,
and `0x001912A0` are recovered. Ordinary startup stops at unresolved
`0x001918E0`. WBINVD ran. Keep `0x00193F70`, `0x0018E120`, and `0x001918E0`
fatal. Production SET_OBJECT walks claimed PRAMIN RAMHT. The kick chain
stays fatal until its wait/wrap/kick contract is written.

## Fresh-context handoff — 2026-09-21

**The repository history was rebuilt on 2026-09-21.** `.git/objects` had gone
missing, so no git command worked and the plan/reports had been uncommitted. The
old directory is kept as `.git.broken-backup/` (gitignored) and history now
starts at `e336a1c`; SHAs before that do not resolve. When committing, put the
**toolkit revision in the commit message** — that is the only record of which
`xboxrecomp` revision a build came from now that history is truncated.

Preserve the unrelated untracked `C:\Users\logic\Repos\xboxrecomp\recomp_run.log`.
The current game binary stops at expected fatal `0x001918E0`. Latest bounded
evidence: `logs/runs/20260921-100901-149-pramin-backing/`. `0x00192090` has
not returned; it reached the first pushbuffer kick after WBINVD.

Current acceptance baseline:

```powershell
cmake --build build --config Release --target jsrf_recovery_11c1_test jsrf_nv2a_test
.\build\Release\jsrf_recovery_11c1_test.exe  # PASS: 2197
.\build\Release\jsrf_nv2a_test.exe  # PASS: 304
ctest --test-dir build -C Release --output-on-failure  # 11/11
Push-Location ..\xboxrecomp
python -m unittest tools.recomp.test_lifter_atomics tools.recomp.test_lifter_string_compare tools.recomp.test_lifter_carry tools.recomp.test_seh_frame_owner  # toolkit: 27/27
Pop-Location
```

Hardware setup through framebuffer publish is recovered. Toolkit `KeSetEvent`
uses per-waiter host events. PGRAPH_INTR writes are W1C. `OUT` lifts to
`xbox_outb`. PFB WBC reads idle after a flush write. Production `0x00193F70`,
`0x0014B794`, `0x0018E120`, and `0x001918E0` stay fatal.

Ordinary startup now stops at unresolved `0x001918E0`
(`logs/runs/20260921-103827-656-ramht-lookup/`). Production SET_OBJECT uses
the claimed RAMHT. The next packet, after independent acceptance of that
lookup, is
**kick/GET contract, on the Grok 4.7 xhigh parent, for `0x001918E0` → `0x001917F0` → `0x001916B0` /
`0x00191530` plus `0x00190240`**. Do not stub it.

Focused 11c1: PASS 2197. NV2A: PASS 304. CTest 11/11.

For a small future Sol/Terra/Luna task, select one helper or one register contract,
name its files and original address range, provide the baseline capture and exact
acceptance commands, and require updates to report/plan/this guide. A port/MMIO
implementation must have behavior tests plus a bounded game run; a documentation
contract audit alone needs no rebuild. Keep Nsight completeness as a separate
optional tool task: it does not block original-XBE analysis or RenderDoc inspection.

## Guest startup stop — the alias fold deleted a static initializer (2026-09-21)

**This supersedes the "next packet is the SEH chain" note above. There is no
exception, no unwinder and no EH-table defect on the path.**

The first real guest stop after `guest_entry` was
`[ICALL] invalid target 0x00700010 return=0017E627`. The previous session read
`esi=0x001EB764` / `edi=0x001EB76C` as EH-table pointers and `0x00700010` as an
EH handler slot. They are not. `sub_0014B50D` is **`_initterm`** — it walks the
function-pointer array `0x001EB760`..`0x001EB76C`, so `esi`/`edi` are its own
loop pointers — and slot `0x001EB764` holds **`0x0017E58F`**, a C++ static
initializer. `0x00700010` is stale stack read as `[ebp+8]` because the
three-argument `sub_0017E600` was called with zero arguments.

`0x0017E58F` was classified `tail_jump_alias` and folded by the `ff4d442`
"abutting alias" rule into the next entry `0x0017E600`: the dispatch tuple became
`{ 0x0017E58F, sub_0017E600 }` and the body was deleted. Pre-fix evidence that
this is a regression: `logs/runs/20260921-111607-936-kick-ack/source.zip` has
`{ 0x0017E58F, sub_0017E58F }` and a clean body in `recomp_0007.c`.

Fixed by recovering `0x0017E58F` in `config/recovered-functions.json`
(`kind: "routine"`, entry 71). An entry there becomes
`detection_method: reviewed_runtime_target`, gets its real translated body, and
overrides the wrong generated dispatch because `RECOMP_ICALL_SAFE` consults
`recomp_lookup_manual` **before** `recomp_lookup` — **no chunk regeneration was
needed.** Use `kind: "routine"` for anything that calls other framed functions:
a generated epilogue emits `POP32(esp, ebp)` without restoring `g_ebp`, so
`g_ebp` is a last-published-frame hint and the initializer default's
`g_ebp != before_bp` check fails spuriously.

Before/after: kernel calls **157 -> 177**, initializer now logs
`[RECOVERED] 0x0017E58F returned; ABI verified`, and the stop moves later to
`[ICALL] Failed to resolve VA 0x00148005`. Runs:
`logs/runs/20260921-154535-674-resume-check/` (before) and
`logs/runs/20260921-155635-882-alias-fix-verified/` (after). CTest 11/11.

**CLOSED for the CRT-initializer class.** The same defect was then enumerated
rather than chased. The CRT calls these function pointers with **no arguments**:

| table | range | walked by |
|---|---|---|
| A | `0x001EB760`..`0x001EB76C` | `_initterm` `0x0014B50D` |
| B | `0x001EB770`..`0x001EB83C` | `_initterm` `0x0014B4B5` |
| C | `0x001EB840`..`0x001EB854` | `_initterm` `0x0014B4B5` |
| D | `0x001EB854`..`0x001EB86C` | `_initterm` `0x0014B4B5` |
| hook | `[0x0022ED2C] = 0x0017BF79` | `sub_0014B4B5` |

**Table B is exactly the 14 initializers milestones 02/03/04 recovered** — the
rest of the same tables were never recovered because the fold had redirected
them to something that resolved. 60 distinct targets, **44 folded**; all 44 are
entries by definition (only the tables call them, with no arguments) and all 44
are now in `config/recovered-functions.json` (`kind: "routine"`, 114 entries).

**Range trap:** an entry's `end` must be **the alias's own DB end**, not the
first terminator from linear disassembly. `0x00181212` ends in `ret` at
`0x00181224` but branches to `0x00181225`, so a linear bound truncated the body
and made the branch an unresolved call. 40 of 43 bounds were widened.

Result: kernel calls 177 -> **200**, **56** `ABI verified` lines, no unresolved
ICALL, **CRT initialization completes** and the guest reaches heap allocation
(`[HEAP] #4..#7`, 4.2 MB of 50 MB). CTest 11/11. Run
`logs/runs/20260921-160209-538-crt-initializers-bounds/`.

**Next packet — a different class:** `0xC00000FD` **host stack overflow** inside
the toolkit's per-allocation `fprintf` in `xbox_HeapAlloc`
(`xbox_memory_layout.c:2228`). `esp=0x00F27EC8` against a guest stack of
`0x00780000`..`0x00F80000` means ~360 KB of 512 KB of guest stack is in use: deep
guest recursion whose recompiled frames cost far more host stack than guest
stack. Decide first whether the recursion is guest-legitimate; if it is, the port
needs a much larger host stack for the guest thread. **Do not fix it by
silencing the log line.**

**Alias-fold defect, still open for the general rule.** 3123 dispatch tuples
redirect a VA to a different symbol; only **87** have a `loc_<VA>` label in the
parent, so **3036** redirect to a body that does not contain the address; of the
971 aliases adopted by the abutting rule, **759 end in `ret`**. Three
discriminators were measured and **all three discarded because they saturate** —
"last instruction ends exactly at the alias end" (`int3` padding decodes as
instructions), "ends in `ret`" (fires 759/971), and "entered from data" (fires
**2651/3123**, because a switch table is also a run of `.text` addresses).
**Do not re-propose these without new evidence.** The remaining sound signal is
structural: fold an alias only when its body actually needs the parent's labels.
Until the rule is repaired, **an address entered from data must be recovered
explicitly.**

## GPU direction and bounded next packets

Read `docs/jsrf-gpu-setup-contract.md` before this work. Milestone 11a is
complete as a corrected documentation audit, and 11b1–11b3 implement the single
register-aperture owner, PCI paths and PTIMER. The open items (GPIO[0] electrical effect, vendor-specific PCI
0x4C..0x4F, constant 0xFE502A,
KeInitializeInterrupt 7th argument) are recorded
as hypotheses. The existing MMIO hook does not install a complete interception
path; PFIFO/USER are stubs and its VRAM is detached from guest allocations. See
the audit before using its initialization API.

Preserve the guest device/resource layout and command stream; connect the
existing NV2A/D3D11 components behind it. This is a direction, not a working
renderer. The memory-layout acknowledgement worker advances DMA_GET and busy
states without executing commands. That does not establish GPU completion.
Do not enable competing GPU/MMIO owners without reconciling their state.

1. **11a: GPU setup contract.** Inspect original `0x00194ADD..0x00194C3F` and
   callees `0x0019460A`, `0x00194635`, `0x00194676`, `0x00194780`. Record register
   reset values, OUT port `0x80C0`, device/queue fields and interrupt/DPC contracts.
   Source scope: original XBE, toolkit `src/nv2a`, memory mapping and kernel
   bridge. Use primary hardware references when local evidence is insufficient.
   Acceptance: exact behavior/address evidence and a proposed single state owner;
   no guessed successful port write or invented GPU status.
2. **11b: Port/MMIO and completion.** 11b1 connected one register model to the
   existing aperture and retired conflicting register acknowledgements. 11b2
   implements PCI configuration paths; 11b3 completes PTIMER semantics; next,
   11b4a adds bounded PFIFO/USER packet intake; 11b4b1 establishes the atomic
   unsupported-method boundary; 11b4b2 proves fixture-scoped SET_OBJECT and clip
   state; 11b4b3 production SET_OBJECT walks claimed PRAMIN RAMHT; 11b5
   reconciles command completion.
   A deliberately stalled command must remain blocked and diagnosable.
3. **11c: Complete GPU setup recovery.** Add reviewed entries and internal tails
   to the appropriate manifests; retain fatal dependency traps. Require verified
   outputs/return from `0x00192090`, then archive the next stop. WBINVD at
   `0x0019243A` ran in `logs/runs/20260921-020149-996-gpu-setup-912a0/`. The
   initializer now dies at pushbuffer kick `0x001918E0`.
4. **11d: Guest ISR/DPC execution.** Prove worker stack/register/IRQL isolation
   before dispatching callbacks. Recover ISR `0x00193C50` and DPC `0x00194480`
   when needed. Test missed signals and reentrancy; close relevant 01c/01d gaps.
5. **12: First presentation.** Connect the game's framebuffer to a responsive
   Windows window. Require a game-generated clear/frame, not a canned toolkit
   screen or a synthetic triangle (those are fixtures only).

WBINVD invokes `recomp_guest_cache_flush`, a full host memory barrier for coherent
emulated memory; it is not native cache invalidation or GPU completion. Existing
SFENCE/LFENCE/MFENCE handling and optional runtime concurrency paths need review
before relying on them for GPU workers. The video probe verifies behavior and
return contracts, not weak-memory ordering or actual GPU execution.

Each future packet must name its milestone, expected outcome, source scope,
baseline artifact, confirmed facts versus hypotheses, reproduction/acceptance
commands and required documentation. If evidence expands scope, record the new
boundary rather than adding guessed fixes. Update this guide's checkpoint and
next task, the plan's status and the live report after substantive progress.
No commit, publication or scheduling is implied by maintaining these
documents. Delegated JSRF packets use the user's standing authorization and
follow the ownership rules above.
