# JSRF Windows recompilation: agent working guide

## Purpose and operating intent

Port Jet Set Radio Future to Windows through Xbox static recompilation. First
reach a playable opening area with movement, graffiti, audio and save/resume;
then extend coverage to the whole game. Work in small, verifiable milestones.
The user is learning the process: explain each defect, its evidence and result.

Read this guide, the current milestone in `plan-jsrf-bare-minimum.md`, and the
`CURRENT STATE` block at the top of `report-deepseek.md` before selecting work.
The plan owns acceptance criteria and statuses; this guide owns operating
knowledge; that block owns the current blocker, evidence revision and next packet.

Two reports are live and neither supersedes the other:
`report-deepseek.md` (DeepSeek sessions and the hourly automation; the CURRENT
STATE block lives here) and `report-jsrf-bare-minimum.md` (audit lineage; the
A1-A5 sequence). Read the CURRENT STATE block first.

`docs/jsrf-run-profiles.md` defines **strict** versus **exploratory** runs, and
classifies the environment overrides. `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`,
`RECOMP_GPU_ACK` and `RECOMP_VBLANK` are synthetic completion and cannot satisfy
boot, audio, GPU or liveness acceptance. `diagnostic_deadline` means the capture
was bounded, not that the guest was live; `normal_exit` means the entry point
returned, not that the title was satisfied.
**`docs/agent-workflow.md` owns the model roster, the session loop and the
escalation triggers — read it at session start** (the next section summarises it).
Keep the report updated during work, with **bold Decision entries for choices made
on your own recommendation**. Proceed through ordinary implementation choices
without repeatedly asking permission; current user instructions take precedence.
Preserve unrelated changes.

**This file is loaded automatically with a 65,536-byte budget.** It currently sits
just under that, so **add sparingly**: short operating knowledge belongs here,
dated narrative belongs in `docs/jsrf-operating-history.md` or the reports. A
section appended past the budget is silently never read — which is exactly what
happened to this file's own tail before 2026-09-22.

## Models and the session loop — `docs/agent-workflow.md` is the authority

**`docs/agent-workflow.md` owns the roster, the session loop and the escalation
triggers. Read it at session start.** The summary below is the whole policy on one
screen; the file has the reasoning, the measured constraints and the briefing
contract. It replaces `grok-role-map.md` and `deepseek-harness.md`, both retired
2026-09-22.

**Two harnesses are supported. Pick your row and use only those models.**

| Role | Codex | DeepSeek Harness (DSH) |
|---|---|---|
| Session | `gpt-6-luna` @ `high` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| Worker subagents | `gpt-6-luna` @ `high` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| Persistent advisor | `codex/gpt-6-astra` @ `medium` | **`codex:gpt-6-astra`** @ `medium` |
| Acceptance reviewer | `gpt-6-luna` @ `max` | `workbuddy-ai/hy4-preview-f` @ `high` |

**Everything else is retired:** `gpt-5.6-sol`, `gpt-5.6-luna`, `gpt-5.6-terra`,
`gpt-5.5`, Grok, `grok-cli`, `hy3`, `glm-5.3`, `kimi-k3`, and every
`workbuddy-ai/gpt-*` route. Those names fill the historical sections of the reports
and old commit messages — read them as history, never as a roster to spawn.

Three measured constraints, because each one looks like a mistake otherwise:

- **In DSH the advisor is `codex:gpt-6-astra`.** `workbuddy-ai` does not serve that
  model, so the DSH advisor is the one deliberate cross-provider call — which is
  also what makes it independent of the session.
- **`workbuddy-ai/hy4-preview-f` advertises exactly one effort, `high`.** The DSH
  reviewer's effort is not a choice; do not try to raise it.
- **The Codex reviewer is the session's own model family** (`gpt-6-luna` at `max`
  rather than `high`). That is a weaker check than DSH's third-family reviewer —
  treat it as a careful second pass, not independent confirmation, and do not
  describe it as "independently verified".

**The loop.** (1) Confirm the roster and, on DSH, confirm the reviewer route with
`list_subagent_models` before promising a review. (2) Read the plan, then the
`CURRENT STATE` block in `report-deepseek.md`. (3) **Work the next packet in the
main session**; spawn workers only for context isolation — reading a big artifact
and returning a bounded summary — never to implement. One owner performs build,
regeneration and run. (4) Work until the plan's acceptance criteria pass: that is
*delivered*, not *accepted*. (5) Spawn the acceptance reviewer and require it to
**verify or refute each criterion independently**, reproducing the load-bearing
measurements itself, returning per-criterion AGREED / DISAGREED / CANNOT VERIFY
with the command or `file:line` behind it, and a **positive control** wherever it
checks that something is absent. (6) **On disagreement, escalate — do not
out-vote.** Both positions and their evidence go to the advisor, whose call is
final.

**Escalate to the advisor when you are looping or hit a wall** — the same failure
after two attempts, no new measurement changing your mind, two measurements
contradicting, an impending universal claim ("impossible", "unproven", "rules
out", or a negative result), an expensive investigation resting on an unverified
premise, a review disagreement, or an acceptance criterion that is unmet or
ambiguous. The trigger is the *shape* of the problem, not frustration. The advisor
returns ranked mechanisms plus the cheapest discriminating experiment for each.

**Spawn the advisor with `run_in_background: true`, always.** `false` yields a
**one-shot** child that answers once and then rejects every continuation with *"has
no supported continuation state"*, forcing a full re-brief. "The answer gates my
next action" is not a reason to pass `false` — it means do not start other work
until the notice arrives. Keep the child id and continue it with `send_message`.
Never use `subagent_fork` for the advisor: seeding it with this conversation
destroys the independence that makes it worth consulting.

Keep original assets and existing saves unchanged.

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
| `scripts/build-jsrf.py` | The guarded Release build; `--parallel 1` if a confined build dies silently. |
| `scripts/build-identity.py` | Source, executable, collector and PDB/map identity verified. |
| `scripts/run-jsrf.py` | Bounded debugger launch and full artifact archive. |
| `scripts/resolution_starts.py` | What the runtime resolves; `genuine_starts()` is own-symbol only. |
| `scripts/check-span-exits.py` | Spans that cut a branch target; `--selfcheck` is its positive **and** negative control. |
| `scripts/check-dump-mapping.py` | Gate: does each archived dump map guest VAs to the right bytes? |
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
| `docs/agent-workflow.md` | **Authority for the model roster, the session loop and escalation.** |
| `docs/jsrf-operating-history.md` | Dated checkpoints and handoffs, moved out of this file (read on demand). |
| `docs/archive/` | Retired docs: `grok-role-map.md`, `deepseek-harness.md`, `report-grok.md`. History only. |
| `.dsh/skills/` | Tracked local skills; `advisor-escalation` is the advisor **briefing procedure** only. |
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
before`, the full `cmake --build` target list with the `env -u` prefix, then
`build-identity.py after` — which is what the chain does and produces the same
artifacts. `scripts/build-jsrf.py` is that sequence in one command and is the
easier entry point; **if a build dies silently at `Checking File Globs`, retry
with `--parallel 1`** — see "Sandbox limits" below. Build **every** target CTest runs, not a subset: MSBuild
deletes a target's output when its link fails, so a later successful build that
does not name that target leaves the test permanently "Not Run" with nothing in
the build log to explain it. That is what hid a stale `-DRECOMP_ABI_CHECK` in
`build/CMakeCache.txt` behind a failing `jsrf_recovery_11c1` for a session.

Two host quirks worth knowing before they cost time:

- **Heredocs are blocked** by the command validator on this host, so
  `git commit -F -` with a `<<'EOF'` body is rejected. Write the message to a file
  and use `git commit -F <file>`.
- **`logs/` is gitignored**, so a probe script written there does not survive.
  Guest memory from an archived run is already reachable through
  `scripts/inspect-jsrf.py memory <run-dir> <va> <length>`, which reads a run's
  `process.dmp` and keeps guest VAs as VAs — prefer it over a new helper.

**Control every dump read with one read of `.text` (added 2026-09-22, A2h).** A
dump can carry a well-formed `guest_ram=<base>+<size>` identity in `stacks.txt`
and still map guest VAs to the wrong bytes. Measured: one run in 546 has its whole
canonical window **displaced by 0x37608 bytes** — 460 of 545 XBE-backed pages read
as `original[VA + 0x37608]`, 0 as `original[VA]`, and `XBEH` occurs 0 times in the
file — while its identity line is byte-identical to a good run's. `jsrf_dump.py`
validates that an identity *exists*; it cannot tell whether it matches the payload.

The control is one read: the loader never patches `.text`, so a correct dump must
return the XBE's own bytes at guest VA `0x00011000`.

```powershell
C:\Python313\python.exe -X utf8 scripts\inspect-jsrf.py memory <run-dir> 0x00011000 16
# .text[0] must be 8b512c85d28b4130c70190431c00741c
```

`scripts/check-dump-mapping.py` runs it across every archived run (or named ones)
and exits nonzero on any failure. Run it before any dump-based claim; a `DIFFERS`
dump is not evidence, and its addresses are **not** guest addresses.

**Sandbox limits — policy-dependent, and they look exactly like code failures.**
Measured 2026-09-22 under a **workspace-write** DSH file policy, and **all three
disappear under full access**. Recorded because a session that runs confined will
hit them, and because each one was briefly misread here as a project defect:

1. **`cmake --build … --parallel N` fails for every N > 1** — exit 1 with **no
   error text**, the log stopping at `Checking File Globs`; `-- /verbosity:diagnostic`
   shows `Done building target "ResolveProjectReferences" … -- FAILED` with
   `0 Error(s)`. MSBuild's multi-node workers use named pipes, which the confined
   sandbox blocks. `--parallel 1` builds cleanly; under full access `--parallel 4`
   also builds cleanly. **If a build dies silently at `Checking File Globs`, try
   `--parallel 1` before investigating the tree.**
2. **`tempfile.mkdtemp` directories are not writable** (though `Path.mkdir` in the
   same location is), so CTest `jsrf_gpu_inspection` fails with `PermissionError`
   writing `process.dmp`. Under full access it passes: **CTest 11/11**. Under
   workspace-write, report the real count as 10/10 with the eleventh blocked by
   the environment, and do not call it a regression.
3. **The title needs write access to `%LOCALAPPDATA%\xboxrecomp\`**, where its
   emulated hard-disk images live (`Partition0.img` … `Partition5.img`). Without
   it `NtOpenFile` returns `ACCESS_DENIED`, the guest calls
   `HalReturnToFirmware(2)`, and the run ends after **1.8 s with ~515 log lines** —
   which reads exactly like a catastrophic regression. The tell is
   `[KERNEL] → returned 0xC0000022` immediately after
   `[PATH] \Device\Harddisk0\partition0 -> partition image`, plus the absence of
   any `[KERNEL] #5xxx` call. A real run reaches ~6,700 kernel calls in ~5 s.

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

1. **This repository's `.git` object store is present and working again, as of
   2026-09-22.** Earlier notes said it held `HEAD`, `config`, `index` and `logs/`
   but no `objects/`, no loose refs and no `packed-refs`, so `git status`,
   `git log` and `git cat-file` all failed with `fatal: not a git repository`.
   That is no longer true: `.git/objects` has entries, `git cat-file -t HEAD`
   returns `commit`, and commits are being created normally (tip `638dd9e` at
   that date). There is still **no remote configured**, so a commit is local
   only — record the toolkit revision in each game commit message, as the
   project does, because that is the only link between the two histories.
   The toolkit sibling `C:\Users\logic\Repos\xboxrecomp` is healthy.
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

## Operating history — moved out of this file 2026-09-22

The dated checkpoints and handoff narratives that used to end this file now
live in `docs/jsrf-operating-history.md`. They were moved because this file is
loaded automatically with a **65,536-byte budget** and had grown to 70,101
bytes, so its final section was being **truncated away entirely** — no session
was reading it. The history is unchanged and still authoritative for what
happened; it is simply read on demand instead of always.

Read it when you need the *reason* behind an operating rule, or the narrative
of a defect this file only summarises. The reports
(`report-deepseek.md`, `report-jsrf-bare-minimum.md`) carry the same events in
more detail and remain the primary record.
