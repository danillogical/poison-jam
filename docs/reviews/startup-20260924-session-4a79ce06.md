# Fresh-session startup receipt

Harness: DSH (DeepSeek Harness), Web GUI `http://127.0.0.1:3080`.
This is a receipt, not a second policy: `docs/agent-workflow.md` owns the roster,
startup procedure and failure handling.

## Identity and handoff

- **Session ID/date/harness:** `session-4a79ce06-0bd5-46f1-a760-c800ce8b62f9`,
  2026-09-24, DSH. Session ID read from `$env:DSH_SESSION_ID`.
- **Actual main model/effort (metadata evidence):** `workbuddy-ai/deepseek-v4.1-flash`
  @ `max`. Source: `C:\Users\logic\.dsh\settings.yaml` `agent-default-model`
  (`provider: workbuddy-ai`, `model: deepseek-v4.1-flash`, `reasoningEffort: max`).
  This matches the DSH `Session` row of workflow §1. Observed (file read), not inferred
  from the workflow table.
- **Workflow/plan/run-profile revisions and dirty diff identity** (SHA-256, read this
  session):
  - `AGENTS.md` — `B97C81FBF02FB87A94B9F29B3EDF58DCF2CF692C71C4A43991178C91600A1497` (12124 bytes)
  - `docs/agent-workflow.md` — `B17BFDEE7B997126FFD01442E12DA0D141D3D8DE542E31411518E3470EA93B95`
  - `plan-jsrf-bare-minimum.md` — `C02939CE642078BCA23D403D353D8D7BD3D99B85FB32607ED2D0F7472625A3EF`
  - `docs/jsrf-run-profiles.md` — was `5928D309512B2BCE37BA040B997FC3731897EF0CB219BE023297545879C4BCD4`
    (292 lines) **at session start**; the Session later edited it under the Advisor ruling
    recorded below, and it is now `4DE22A11ACE356C0EACD95E57EC33DF403A3763AEDAC764BED7A91B94F97A4DB`
    (19542 bytes, 313 lines). Both hashes are given because the packet cites this document.
  - `docs/session-start-template.md` — `A021645E3766D95C3A9E314B228C03238507B45906067EA1C4928599CCC11FFA`
  - Dirty diff identity: **none at session start** — `git status --porcelain` was empty in
    both trees when this receipt was begun. As of this writing the game tree carries
    exactly two session-owned changes and nothing else: ` M docs/jsrf-run-profiles.md`
    (the Advisor-mandated policy edit recorded below) and the untracked receipt file
    itself. The toolkit tree remains clean. No unrelated edit exists in either tree.
- **Game revision/status:** `98edf113d14246f09144ac4c6bf491bcf35ce5a4`, clean.
- **Toolkit revision/status:** `c97ce2c3c3f268d849ee330e346ff91721f415e5`, clean.
  Both match the plan's recorded baseline (`toolkit c97ce2c`, `game 98edf11`) exactly.
- **Unrelated edits preserved:** none existed. No dirty file was found in either
  repository at session start, so nothing had to be preserved or reverted. The only
  working-tree changes this session are its own: the Advisor-mandated edit to
  `docs/jsrf-run-profiles.md` and this receipt. (See "Baseline conditions" below for one
  non-dirty baseline anomaly that is recorded rather than reverted.)
- **CURRENT PACKET copied from plan (packet + exact revision + SHA-256), or NONE:**
  **NONE.** `plan-jsrf-bare-minimum.md:9` reads `## CURRENT PACKET — none`, and `:11`
  states implementation is `BLOCKED` until one is promoted.
- **Dependencies and their recorded acceptance reviews:** the last closed packet is
  `A3a-r25` (`docs/packets/a3a-ac97-codec-model.md`, revision `A3a-r25`, SHA-256
  `6F907A42EAC6FD02392E11EADEF127AE840D97A245E771CD15B63A387F0E5BDC`), `ACCEPTED`
  2026-09-24 — record `docs/reviews/a3a-r25-acceptance-review.md` (`ACCEPT`, no
  disagreements). Its code is committed at toolkit `c97ce2c` / game `a16350f`, both
  ancestors of the current HEADs (verified with `git merge-base --is-ancestor`). `P0`
  (`P0.S`, `P0.1`–`P0.7`) is accepted and is a prerequisite, not active work.
- **Next exact authorized action:** the Planner designs the next packet against the
  measured blocker (plan `:13-16`). Nothing is executable until an adequacy review
  returns `ADEQUATE` and that frozen revision is promoted into `CURRENT PACKET`.
- **Build/run owner and worker write ownership:** Session owns integration, build, run
  and evidence collection (workflow §2.2). No worker scopes are assigned, because no
  packet exists yet to declare them.

## Route resolution — PASS

Resolved live with `list_subagent_models` (DSH). Every §1 DSH row was resolved; no
substitution was made and no route was assumed from a display name.

| Role | Requested route/effort | Returned route identity | Result |
|---|---|---|---|
| Session | `workbuddy-ai/deepseek-v4.1-flash` @ `max` | `workbuddy-ai/deepseek-v4.1-flash`, efforts `low`–`max` | PASS |
| Worker subagents | `workbuddy-ai/deepseek-v4.1-flash` @ `max` | `workbuddy-ai/deepseek-v4.1-flash`, efforts `low`–`max` | PASS |
| Planner | Claude Opus 5.5 @ `high` (`LIVE_RESOLVE`) | `claude/claude-opus-5-5`, efforts `low`–`max` | PASS |
| Persistent advisor | Claude Opus 5.5 @ `high` (`LIVE_RESOLVE`) | `claude/claude-opus-5-5`, efforts `low`–`max` | PASS |
| Acceptance reviewer (first stage) | `workbuddy-ai/deepseek-v4.1-flash` @ `max` | `workbuddy-ai/deepseek-v4.1-flash`, efforts `low`–`max` | PASS |
| Acceptance reviewer (second stage) | Claude Opus 5.5 @ `medium` (`LIVE_RESOLVE`) | `claude/claude-opus-5-5`, efforts `low`–`max` | PASS |

- **`LIVE_RESOLVE` check (workflow §1):** provider `claude` advertises **exactly one**
  model, `claude/claude-opus-5-5` ("Claude Opus 5.5"), and it supports the required
  efforts `high` and `medium`. Exactly one match — no ambiguity, not `BLOCKED`.
- **Workers requested route/effort; returned route identity:** as tabled;
  `workbuddy-ai/deepseek-v4.1-flash` @ `max`.
- **Exact error or ambiguity, if any:** none.

## Acceptance reviewer probes — PASS (both stages)

Probe each stage listed in workflow §1 separately. Each was invoked at its listed
effort and asked for a fresh session token plus one reason an empty evidence set must
fail acceptance.

- **First stage** — child ID `f72e9d99-3642-4c98-bf2e-3f20257315f0`, route
  `workbuddy-ai/deepseek-v4.1-flash` @ `max`. Fresh token `AR1-7q3mz8vk24`, returned in
  a completed response containing `PROBE: acceptance-reviewer-stage-1`. Empty-evidence
  answer: `ACCEPT` requires every mandatory criterion `AGREED`; with no evidence each is
  `CANNOT VERIFY`, so the only contract-consistent disposition is `NOT ACCEPTED`, and a
  default `PASS` would make the gate unfalsifiable. **Result: PASS.**
- **Second stage** — child ID `39afeb9d-9eba-4c19-846c-0807b0874b44`, route
  `claude/claude-opus-5-5` @ `medium`. Fresh token `AR2-q7Vx3mKp9Lz2Rw`, returned in a
  completed response containing `PROBE: acceptance-reviewer-stage-2`. Empty-evidence
  answer: a verdict asserts a measurement was reproduced and met the criterion; with no
  evidence there is nothing to reproduce, so a build that never ran is
  indistinguishable from a success, making it `CANNOT VERIFY` and not `PASS`.
  **Result: PASS.**
- **Exact error or missing evidence:** none. Neither token appeared anywhere else in
  this session; each was invented by its own child.

## Persistent advisor probe — PASS

One combined probe, both parts on the **same** child, per workflow §0.4.

- **Child ID:** `407c54a3-6ca4-4a65-835b-faf5355195cd` — spawned
  `run_in_background: true` as a fresh continuable child, route
  `claude/claude-opus-5-5` @ `high`.
- **Turn 1 reference; unique marker given:** marker `DSH-JSRF-ADVISOR-7Q4M2XK9`, given
  only in turn 1.
- **Named file and the fact deliberately omitted from the brief:** `AGENTS.md`, §"Dump
  integrity". The brief named the file but not the value; the advisor was asked for the
  exact 16-byte hex string the dump-integrity control read of guest VA `0x00011000`
  must begin with, plus its line number.
- **Advisor's answer; checked against the file:** `8b512c85d28b4130c70190431c00741c`
  at line 84, inside the ```` ```text ```` fence introduced by line 81. **Verified
  correct**: I read `AGENTS.md` myself this session and lines 81–85 contain exactly that
  value. The advisor also stated it read all 272 lines itself rather than reconstructing
  the value. The fact was **not** present in the brief.
- **Turn 2 reference (same child, marker not repeated); returned marker:** second
  `send_message` to the same child asked for the marker without repeating it; the child
  returned `DSH-JSRF-ADVISOR-7Q4M2XK9` exactly.
- **Result: PASS.** Correct fact (checked against the file) *and* correct marker, from
  the same child — continuity and evidence-reading both proved. This child is retained
  as the session's Persistent advisor (workflow §4.4) and was **not** seeded with this
  conversation.
- **Exact error or missing evidence:** none.

## Owner instruction recorded (workflow §4.1)

**The message at record `seq 399` in the Planner transcript
(`5673a592-dc14-4059-bd41-d3bfa233e7b8`) came from the owner.** It read "Stop further
investigation now…" and directed the Planner to design an observability/discovery packet
rather than continue reverse-engineering the GP DSP.

An earlier note in this receipt described that message as "a user message injected
directly into the Planner's session … rather than from me" and flagged it as worth the
owner's attention. **That characterisation is withdrawn.** Under `docs/agent-workflow.md`
§4.1, "The owner may message any agent directly, including a child. Such a message is an
owner instruction, not an injection: follow it, record it, and tell the Session if it
changes scope. It outranks every role's ruling." The owner has confirmed authorship of
that message. It was therefore a legitimate owner instruction, it correctly changed the
packet's scope from an implementation packet to a discovery packet, and nothing about it
was anomalous.

**Baseline note corrected.** The condition recorded below as "item 1" (the
`check-agent-docs.py` failure) was repaired by the owner while this session was paused,
in game commits `1c9b3f2` … `13b6df3`. Re-verified after resuming:
`python -X utf8 scripts\check-agent-docs.py --check` now prints `checker
jsrf-agent-docs/2` … `no findings` and exits `0`, and the full Python suite reports
`Ran 397 tests … OK` with **zero** failures. The historical description below is retained
only as the record of what was true at the session-start baseline.

## Baseline conditions recorded (not reverted)

Neither repository has a dirty file, so the baseline is clean as the plan records.
Two baseline conditions were observed and are recorded here rather than acted on:

1. **Pre-existing failing test at the baseline commit.** `scripts/check-agent-docs.py
   --check` exits `1` with two findings — `authority_links/stale_authority_link`
   ("AGENTS.md defers to report-deepseek.md, which does not exist") and
   `next_packet/missing_input` ("report-deepseek.md is missing") — and therefore
   `tests/test_agent_docs.py::PositiveControlTests::test_real_repository_is_clean`
   fails. Cause, observed: `scripts/check-agent-docs.py:30,77` still registers
   `REPORT = 'report-deepseek.md'` in `AUTHORITY_LINKS`, while baseline commit `98edf11`
   deleted that file (9280 deletions) without updating the checker or its test. The
   other 396 tests in that module pass; `ctest` is 12/12 and `scripts/test-harness.py`
   is 19/19 PASS. This is a defect in the baseline commit, **not** a dirty file, and it
   was left untouched. It is a candidate for a follow-up, not for this session to fix.
2. **The blocker run was produced from a dirty tree.** The archived strict run
   `logs/runs/20260924-100502-623-a3a-codec-model` records project revision `73eee970`
   and toolkit revision `484887b8` **plus** patches (`project.patch` 1010259 bytes,
   `toolkit.patch` 13156 bytes) — i.e. the A3a work in progress. That work is now
   committed as `a16350f`/`c97ce2c`. To establish whether the blocker is current for the
   **target** revision I compared all 144 entries of the run's `build-source.json`
   against the present working tree: **144 same, 0 differ, 0 missing**. The built
   `build\Release\jsrf_recomp.exe` also hashes to `879716046b99cecfb61a8373237df24fcd
   efa4d00317f50c726aeae7a35187ef`, identical to the run's recorded `exe_sha256`. The
   measured blocker therefore belongs to the current revision, not to a superseded tree.
3. **The blocker was re-measured on the clean baseline, not merely inherited.** I ran a
   fresh strict run on the clean tree at the baseline revisions:

   ```powershell
   $env:RECOMP_GPU_ACK='0'
   python -X utf8 scripts\run-jsrf.py --seconds 30 --profile strict --label session-startup-baseline
   ```

   → `logs/runs/20260924-143745-909-session-startup-baseline`, a **new** archive.
   Result: `outcome = diagnostic_deadline`, `exit_code = 3`, `dump_ok = true`,
   `checkpoints_passed = true`; `check-run-profile.py` reclassifies it **STRICT**;
   `check-dump-mapping.py` `matches: 1`; `stacks.txt` shows the same live frame
   `sub_001A1769+0xB20` at `recomp_0005.c:6749`; and guest VA `0x803C0810` in the frozen
   dump reads `00000003` again, with `0x1BA858` = `803C0000`. The blocker therefore
   reproduces on the target revision from a clean tree, independently of the archived
   A3a-era run. The guest did **not** proceed past the spin within the 30-second bound.
   This is one strict run's observation; it establishes the blocker is current, not that
   the stop is deterministic or that no other path exists.

## Packet readiness — BLOCKED (no packet current)

### Blocker characterisation measured this session (for the Planner's packet design)

All of the following are **observed** by the Session on the current clean baseline,
except where marked inferred. They are recorded here because the packet that follows
will rest on them; each names its artifact.

| # | Finding | Artifact |
|---|---|---|
| B1 | A fresh strict run on the clean baseline reproduces the spin: `diagnostic_deadline`, live frame `sub_001A1769+0xB20` at `recomp_0005.c:6749`, pending word `0x803C0810` = `3`. | `logs/runs/20260924-143745-909-session-startup-baseline` |
| B2 | `RECOMP_APU_TRAP=1` with `RECOMP_GPU_ACK=0` under `--profile strict` classifies **STRICT**, not exploratory: `RECOMP_APU_TRAP` is absent from the classifier's five-variable enumeration (`scripts/jsrf_run_profile.py:28-32,380-424`). | `logs/runs/20260924-143856-017-apu-trap-classification-probe` |
| B3 | Under trap the title genuinely starts the APU: `[APU] started by the title (SECTL=0000000F FECTL=0000100F)` (toolkit `src/apu/apu_core.c:149`). The non-trap strict baseline does **not** log that line. | same as B2 vs B1 |
| B4 | Trap alone does **not** clear the word or move the stop: both trap runs still end `diagnostic_deadline` with the same live frame `sub_001A1769+0xB20` at `recomp_0005.c:6749`, and `0x803C0810` still reads `3`. **Basis: the live frame and the frozen dump word only.** An earlier version of this row also claimed "max `[KERNEL] #` ordinal is 200 in all three runs — identical kernel progress". **That claim was wrong and is withdrawn** (see B13). | B1, B2, `logs/runs/20260924-143932-031-apu-trap-register-trace` |
| B5 | Register traffic on the current revision (ordered): `0x02000` SECTL, `0x3FFFC`, `0x0202C=80388000`, `0x02038=803AC000`, `0x0203C=803B0000`, `0x02030=80398000`, `0x02034=8039C000`, `0x0115C=80390000`, `0x02044=803B4000`, `0x020D8=1`, `0x02040=803CC000`, `0x02000=0x0F`. | `logs/runs/20260924-143932-031-apu-trap-register-trace` |
| B6 | The guest polls `0x20010` and receives a constant `00000080` (58 reads; trace cap 400 not reached). | same as B5 |
| B7 | **Gating hazard — one register, two models, and the trap switches the wrong one off.** `0x020010` (from `MCPX_COUNTERS`) and `0x20010` (from the APU trace) are the **same guest VA**, `0xFE820010`: `XBOX_MCPX_BASE` = `0xFE800000` = `APU_MMIO_BASE`, so `0xFE800000 + 0x020010 == 0xFE800000 + 0x20010` (verified by arithmetic and by both constants' definitions). Untrapped, that dword is plain MCPX RAM incremented by the tick loop at `xbox_memory_layout.c:664-670` (the "APU GP sample counter"). Trapped, the window is unmapped and the read faults into `apu_hook_handle_mmio`, whose offset routes `0x20000..0x30000` to `mcpx_apu_vp_read`; VP offset `0x10` is `NV1BA0_PIO_FREE`, which returns a **hardcoded `0x80`** ("Always pretend queue is empty", toolkit `src/apu/apu_vp.c`) and ignores the counter entirely. So with `RECOMP_APU_TRAP` the guest's `0xFE820010` poll is answered by a constant, not by the counter — the tick loop is skipped because it is gated `!g_apu_mmio_trapped`. The observed constant `00000080` in B6 is that hardcoded value. This is the discrepancy already recorded at `docs/jsrf-run-profiles.md:268-272`. **Consequence for the packet: a criterion must not treat the trapped `0xFE820010` value as evidence that the counter model ran, and must not treat the untrapped counter as if the guest read it.** | toolkit `src/apu/apu_core.c`, `src/apu/apu_vp.c`, `src/kernel/xbox_memory_layout.c:81-82,258,664-670` |
| B8 | The GP (`0x30000`) and EP (`0x50000`) DSP blocks still have **no model**; reads there return 0 with a named-block diagnostic, and `apu_dsp.c` is a passthrough stub. "Make the GP clear `+0x810` itself" is therefore not a small change. | toolkit `src/apu/apu_core.c`, `src/apu/apu_dsp.c:1-130` |
| B9 | **Both guest sites that clear the pending word were verified in the generated code, and they are the DSP stop path.** `recomp_0005.c:6524` (`loc_001A1747`) is `MEM32(edi + 0x810) = MEM32(edi + 0x810) & 0`; `recomp_0005.c:8086-8089` (`loc_001A1F9B`) is `edi = MEM32(0x1BA858); edi += 0x800; MEM32(edi + 0x10) = ebp; MEM32(edi) = ebp`. The original instructions are `0x001A1751 and dword ptr [edi+0x810],0` and `0x001A1F9B mov edi,[0x1ba858]` / `0x001A1FA7 mov [edi+0x10],ebp` — both reach the same dword through the published base `0x1BA858` = `803C0000`, plus `0x800` + `0x10` = `0x803C0810`. So `src/main.c:236-238`'s statement that only the DSP stop path writes 0 there is **confirmed** for the generated tree. | `src/recomp/gen/recomp_0005.c:6524,8086-8089`; XBE disasm `0x001A1751`, `0x001A1F9B-0x001A1FA7` |
| B10 | **Offset alone does not scope the claim.** A grep for `+ 0x810` across `src/recomp/gen/recomp_*.c` returns 5 hits; the four in `recomp_0000.c` (`:135283,135406,135868,135871`) are unrelated linked-list fields on other structures, not the DSP block. Any criterion phrased as "nothing else writes `+0x810`" is therefore ambiguous unless it names the block base (`0x1BA858` / `0x803C0810`). | `src/recomp/gen/recomp_0000.c:135283,135406,135868,135871` |
| B11 | **There is no DSP instruction executor in the toolkit — the GP cannot run anything today.** `src/apu/apu_dsp.c` is 166 lines whose entire body is `mcpx_apu_dsp_init` (allocates two zeroed `DSPState`, prints `STUBBED - passthrough mode`), a no-op preference setter, and `mcpx_apu_dsp_frame` (copies mixbin 0/1 to the monitor buffer, calls the forbidden `dsp_ack_frame` at `:145`, zeroes `g_dbg.*.cycles` at `:164-165`). A repo-wide search for any executor or opcode decode (`dsp_core_run`, `dsp_exec`, `dsp_step`, `dsp_opcode`, `DSP_REG_PC`) across all `*.c`/`*.h` returns **nothing**. `apu_state.h:40` says its struct is "from dsp_cpu.h", but `git grep dsp_cpu` matches that string in exactly one place — that comment; no `dsp_cpu.c` exists tracked or untracked. `src/apu/CMakeLists.txt` builds only `apu_core.c apu_vp.c apu_dsp.c apu_mmio_hook.c apu_xaudio2.c` and its header says GP/EP are stubs; `src/apu/README.md` agrees ("Currently **stubbed**"). So "make the GP clear `+0x810` itself" means implementing/porting a DSP56300 core first. | toolkit `src/apu/apu_dsp.c:1-166`, `src/apu/apu_state.h:40`, `src/apu/CMakeLists.txt:5-14`, `src/apu/README.md` |
| B12 | **No DSP test coverage exists** to serve as an oracle. A search of game `tests/` and toolkit `tools/`, `src/`, `tests/` for `apu_dsp` / `mcpx_apu_dsp` / `DSPState` finds only `src/apu/apu_core.c` and `src/apu/apu_dsp.c` themselves. Any packet changing DSP behaviour must take its oracle from a strict run. (Measured absence; the search is its coverage witness.) | as above |
| B13 | **WITHDRAWN CLAIM — the `#200` kernel ordinal is not a progress measurement.** The Planner caught this in the Session's own B4. `kernel_bridge.c` computes `budget = env ? strtol(env,NULL,0) : 200`, and logging is gated on `g_kernel_call_count <= budget`. None of the Session's three runs set `RECOMP_KERNEL_LOG_BUDGET` (their `metadata.json` settings are, respectively, `JSRF_COLLECTED,JSRF_LOG_PATH,RECOMP_GPU_ACK`; `…,RECOMP_APU_TRAP,…`; `…,RECOMP_APU_TRACE,RECOMP_APU_TRAP,…`), so all three hit the **default cap of 200**. Equal `#200` therefore says only that each run made *at least* 200 kernel calls — it cannot show identical progress. This is precisely the trap `docs/jsrf-run-profiles.md:154-155` warns about ("the default truncates the log, and a truncated log reads as a hang. Use 100000"). For contrast, the archived A3a run *did* set `RECOMP_KERNEL_LOG_BUDGET=100000`, so its `#13473` ordinal is real. **The Session's `#200` comparison was invalid and must not be reused.** B4's conclusion still stands on the live frame and the dump word, which are independent of the log budget. | toolkit `src/kernel/kernel_bridge.c` (`kernel_log_budget`, `KERNEL_LOG_ON`); the three runs' `metadata.json`; `docs/jsrf-run-profiles.md:154-155` |
| B14 | **`RECOMP_APU_TRACE` prints a read-back value, not the written value.** `apu_hook_handle_mmio` (`apu_mmio_hook.c:273-277`) obtains `v = mcpx_apu_mmio_read_quiet(...)` and prints it for both reads and writes. `mcpx_apu_mmio_read_quiet` returns **0 for any offset ≥ `0x30000`**, so every GP/EP-region trace line shows `00000000` regardless of what the guest wrote. Confirmed against the original code: the title writes immediates `mov dword ptr [0xfe83ff14], 0xff` at `0x001A582A` and `mov dword ptr [0xfe83fffc], 3` at `0x001A585E` (`0xFE83FF14`/`0xFE83FFFC` are GP-region offsets `0x3FF14`/`0x3FFFC`), yet the trace shows `write 0x3FFFC = 00000000`. **So the trace cannot be used to infer what the guest wrote to GP/EP, nor that it wrote zero.** | toolkit `src/apu/apu_mmio_hook.c:273-277`, `src/apu/apu_core.c` (`mcpx_apu_mmio_read_quiet`); XBE disasm `0x001A582A`, `0x001A585E`; `logs/runs/20260924-143932-031` |
| B15 | **GP/EP writes are silently dropped.** `mcpx_apu_dispatch_mmio` handles only `addr < 0x20000` (main registers) and `0x20000..0x30000` (VP); its own comment reads "GP (0x30000) and EP (0x50000) regions ignored for now". So under `RECOMP_APU_TRAP` the guest's GP writes are accepted and discarded — the same class of loss `src/main.c:82-86` describes for the untrapped aperture. Combined with B14, the current trace cannot observe GP/EP traffic in either direction. | toolkit `src/apu/apu_core.c` (`mcpx_apu_dispatch_mmio`) |
| B16 | **The APU trace cap is hardcoded at 400 lines with no environment override.** `apu_mmio_hook.c:268-269` is `static unsigned n; if (n++ < 400)`, a file-local counter, not a configurable budget. The trace run logged **344** lines, so it did not reach the cap — the observed traffic is complete for that run. But a longer run, or one that enables `RECOMP_APU_TRACE` alongside other traffic, would silently truncate with no indication. Contrast `RECOMP_KERNEL_LOG_BUDGET` (B13), which *is* configurable. A packet relying on trace completeness must either bound the run so 400 suffices or make the cap configurable first. | toolkit `src/apu/apu_mmio_hook.c:268-269`; `logs/runs/20260924-143932-031` (344 lines) |

B5's `0x02040` value (`803CC000`) differs from the older archived exploratory trace
(`803B4000`); that archive is a different revision and its exact values are not current.

- **Frozen revision/hash matches `CURRENT PACKET`:** N/A — `CURRENT PACKET` is `none`.
- **Adequacy review record and verdict:** N/A for the next packet. The last adequacy
  verdict belongs to the closed `A3a-r25`
  (`docs/reviews/a3a-r25-adequacy-review.md`).
- **Deferred advisories (recorded, not acted on):** carried in the plan's follow-up list
  (`plan-jsrf-bare-minimum.md:52-60`) — the `P0.1-AC1` re-review, moving
  `RECOMP_AC97_READY` into the classifier's `RETIRED_OVERRIDES` registry,
  `RECOMP_APU_TRAP` moving with the DSP packet, and the unmodelled AC'97 registers
  `0xFEC0017C` / `0xFEC00100`. None is authorized work.
- **Prerequisites / tooling checks (all verified to run this session):**
  - `python -X utf8 scripts\check-dump-mapping.py logs/runs/20260924-100502-623-a3a-codec-model`
    → control read `8b512c85d28b4130c70190431c00741c` at VA `0x00011000`, `matches: 1`,
    `content-mismatch: 0`, exit `0`. Re-run on the fresh baseline run
    `20260924-143745-909` and on the trap runs with the same result.
  - `python -X utf8 scripts\check-run-profile.py <run>` → `STRICT` for
    `20260924-100502-623-a3a-codec-model`, `20260924-143745-909-session-startup-baseline`,
    `20260924-143856-017-apu-trap-classification-probe` and
    `20260924-143932-031-apu-trap-register-trace` (reclassified from each archive, not
    from metadata alone).
  - `ctest --test-dir build -C Release --output-on-failure` → 12/12 passed.
  - `python -X utf8 scripts\test-harness.py` → 19/19 PASS.
  - `python -X utf8 scripts\inspect-jsrf.py disasm|memory` → both run (used for every
    measurement below).
  - `python -X utf8 scripts\verify-initializers.py logs/runs/20260924-100502-623-a3a-codec-model`
    → `PASS: 616 initialized words match captured game memory`, exit `0`.
  - `python -X utf8 scripts\run-jsrf.py` → ran four times (one 30 s strict baseline, two
    trap runs, one 12 s classification probe); each archived under `logs/runs/`.
    **Correction:** none of these four set `RECOMP_KERNEL_LOG_BUDGET`, so all four logs
    are truncated at the default cap of 200 kernel calls. Their `[KERNEL] #` ordinals are
    therefore **not** usable as progress measurements (B13). A run intended to measure
    progress must set `RECOMP_KERNEL_LOG_BUDGET=100000`, as `docs/jsrf-run-profiles.md:154-155`
    instructs and as the archived A3a run did.
  - `python -X utf8 scripts\build-jsrf.py` → `Build succeeded (success); source/executable
    identity recorded`, exit `0`. The rebuilt `build\Release\jsrf_recomp.exe` hashes to
    `879716046B99CECFB61A8373237DF24FCDEFA4D00317F50C726AEAE7A35187EF`, **identical** to
    the `exe_sha256` recorded by the archived blocker run `20260924-100502-623-a3a-codec-model`
    and to the exe inside the fresh baseline run's archive. The tree is therefore
    build-deterministic and unchanged from the revision the blocker was measured on. The
    build left tracked files untouched (`git status --porcelain` unchanged). Note the
    build script reports `regeneration: disabled (pass --allow-regeneration to permit it)`,
    so routine builds do not regenerate `src/recomp/gen/` — consistent with `AGENTS.md`.
  - Toolkit: `C:\Python313\python.exe -m unittest tools.recomp.test_lifter_atomics
    tools.recomp.test_lifter_string_compare tools.recomp.test_lifter_carry
    tools.recomp.test_seh_frame_owner` (with `PYTHONPATH` per `AGENTS.md`) → `Ran 27
    tests … OK`. The nonzero shell code is unittest writing to stderr, not a failure.
  - Game Python tests: `python -X utf8 -m unittest tests.test_run_profiles
    tests.test_agent_docs tests.test_ac2_provenance` → `Ran 397 tests`, `FAILED
    (failures=1)`, the single failure being the baseline `test_agent_docs` defect in
    "Baseline conditions" item 1. Note `python -m unittest discover -s tests` does
    **not** work in this tree (`Start directory is not importable`); name the modules.
  - **No DSP/APU test coverage exists** to serve as a local oracle: a search of game
    `tests/` and toolkit `tools/`, `src/`, `tests/` for `apu_dsp` / `mcpx_apu_dsp` /
    `DSPState` finds only `src/apu/apu_core.c` and `src/apu/apu_dsp.c` themselves. Any
    packet that changes DSP behaviour must therefore take its oracle from a strict run,
    not from a unit test. (Recorded as a measured absence, with the search as its
    coverage witness — see B12.)
- **Blocker re-measured independently (observed, not taken from the plan):**
  - `result.json`: `outcome = diagnostic_deadline`, `exit_code = 3`, `dump_ok = true`.
  - `stacks.txt` THREAD 39332: live frame `sub_001A1769+0xB20` at
    `src/recomp/gen/recomp_0005.c:6749` — the spin.
  - Original XBE disassembly `0x001A18C6`–`0x001A18D3`: `add ebx,0x810` /
    `mov dword ptr [ebx],eax` (with `eax = 3` from `push 3` / `pop eax` at
    `0x001A18C1`/`0x001A18C3`) / `cmp dword ptr [ebx],0` / `jne 0x1a18d0`. The pending
    word is written **3** and the loop exits only on **0**.
  - Frozen dump at guest VA `0x803C0810` reads `00000003` — still 3 at capture, so
    nothing cleared it. Published command-block base at guest VA `0x1BA858` reads
    `803C0000`, consistent with the 48K contiguous allocation at `0x803C0000`.
  - `src/main.c:228-254` records that the emulated APU is instantiated **only** under
    `RECOMP_APU_TRAP`, and toolkit `src/apu/apu_dsp.c:2,116` describes the GP/EP as a
    **stub** in passthrough mode. `RECOMP_APU_DSP_ACK` is the forbidden synthetic
    completion (`docs/jsrf-run-profiles.md:89`).
- **State/plan disagreements and how they were escalated:** none. Plan state, both
  working trees and the measured blocker agree. The two baseline conditions above are
  recorded, not escalated, because neither changes the plan's next action.
- **Overall disposition and next action:** startup is **PASS** — every required route,
  effort, spawn and continuation resolved, both probes passed, and the receipt is saved.
  **Implementation is `BLOCKED`** because no packet is current (workflow §0.5, §5.2).
  The next authorized action is the plan's: have the Planner design the next packet
  against the DSP pending-word spin, with the Session supplying evidence and drafting
  only the mechanical parts.

**Session-owned extra measurement (not required by §0, recorded for the packet).** The
plan's next action needs the blocker to hold for the *target* revision. Rather than
leave that as an inference from an A3a-era dirty-tree archive, the Session ran three
bounded strict runs on the clean baseline (B1–B4 above). This is evidence supplied to
the Planner; the Planner decides whether it is enough, and the adequacy review judges
the resulting packet.

## Technical-policy ruling recorded verbatim (workflow §3.3)

During packet design the Session consulted the retained Persistent advisor (child
`407c54a3-6ca4-4a65-835b-faf5355195cd`, route `claude/claude-opus-5-5` @ `high`, spawned
and probed in §"Persistent advisor probe" above) on one bounded question: whether a run
launched with `RECOMP_APU_TRAP=1` and `RECOMP_GPU_ACK=0` — which the classifier reports
as `STRICT` — is admissible as strict evidence for a criterion about the APU/DSP path,
given that the trapped window is served by stubs returning constants.

The ruling binds every role from the moment it is recorded. It is reproduced here so the
packet and its review record can cite a durable copy. **The general rule it states
belongs in `docs/jsrf-run-profiles.md` §"Feature enablement — real capability, not a
bypass" as a separate policy edit, not in the packet** (`docs/jsrf-run-profiles.md:280-282`).

> **RULING: Admissible only in part.** The STRICT label is necessary but not sufficient.
> Whether a claim is strict depends on where each value the claim relies on came from,
> not on the run's label alone.
>
> **(A) ADMISSIBLE as strict evidence** (observational claims about the title in this
> configuration): what the guest writes to the APU window, and in what order (e.g. "the
> title started the APU, SECTL=0000000F FECTL=0000100F"); which registers it reads and
> how often (e.g. "polled 0xFE820010 58 times"); where it stops: live frame, pending word
> at 0x803C0810 == 3, max kernel ordinal. The negative result "enabling the trap does not
> move the blocker" is admissible only for exactly this configuration. It does not show
> that a correct APU would fail to move the blocker.
>
> **(B) NOT ADMISSIBLE as strict evidence** (claims that depend on a value served by a
> stub): that the APU/DSP device path works; that any wait was satisfied by a modelled
> cause; that a GP/EP command had a semantic effect. Reads from GP (+0x30000) and EP
> (+0x50000) return 0 with no model behind them. NV1BA0_PIO_FREE (+0x20010) returns the
> hardcoded 0x80 ("pretend queue is empty"). If the guest makes progress because of one
> of those answers, that progress is at most EXPLORATORY-grade, even though the run file
> says STRICT. The packet must say so in the same sentence as the claim.
>
> **(C) Trap vs. non-trap runs are NOT a single-variable comparison.** The trap changes
> three things together: it instantiates the APU, it replaces the 0xFE820010 tick-loop
> counter (grandfathered row) with the constant 0x80, and it answers GP/EP reads with 0.
> The packet must not attribute a difference between the two to any one of these.
>
> **(D) No APU behaviour that exists only under the trap can be an "unconditional modeled
> cause", because it fails admission criterion 1 (it sits behind a switch).**
> - Clearing the pending word at 0x803C0810 is a result of DSP work. Under criterion 4 it
>   can never be supplied by a model or a stub; it requires the GP to actually execute.
> - The only strict route to "the DSP wait was satisfied" is real GP DSP56300 execution of
>   the title's command, observed in a run the classifier reports as STRICT.
> - RECOMP_APU_DSP_ACK remains synthetic completion.
>
> **What the packet should use:** the strict trap profile for traffic, ordering and
> stop-location measurement, with claim limits that name GP/EP (no model) and PIO_FREE
> (constant) as unmodelled. Any claim of forward progress past a stub answer is labelled
> exploratory. Any claim that the audio/DSP path works waits for a real GP engine.
>
> **BASIS** (advisor's own words; load-bearing items only):
> - Observed: classifier `jsrf_run_profile.py:380-424` enumerates only GPU_ACK,
>   AC97_READY, ALLOW_UNRESOLVED, ABI_CONTINUE and APU_DSP_ACK, so `RECOMP_APU_TRAP`
>   cannot make a run exploratory.
> - Observed: `jsrf-run-profiles.md:62-70` says strict allows no answer the emulated
>   hardware did not produce, and only strict can establish "a device path works" or "a
>   wait was satisfied by a modelled cause".
> - Observed: `:141` lists APU_TRAP as feature enablement that "still needs a GP SGE
>   engine"; `:222-230` are admission criteria 1 and 4; `:267-272` record the
>   grandfathered-counter discrepancy.
> - Observed: `apu_core.c:629-634` and `:637-665` route VP reads (+0x20000..+0x30000) to
>   the VP handler and return 0 for GP/EP with "no model".
> - Observed: `apu_vp.c:556-557` returns 0x80 for PIO_FREE; `apu_regs.h:128` defines
>   NV1BA0_PIO_FREE = 0x10, so VP +0x10 = APU +0x20010.
> - Observed: `apu_dsp.c:1-14` and `:102-117` show the GP/EP DSP is a "STUBBED -
>   passthrough".
> - Observed: `xbox_memory_layout.c:81` gives XBOX_MCPX_BASE = 0xFE800000; `:258` lists
>   0x020010 as "APU GP sample counter"; `:664` gates the counter tick on
>   `!g_apu_mmio_trapped`.
> - Observed: `main.c:243-254` instantiates the APU only under `RECOMP_APU_TRAP`.
> - Observed: in `20260924-143932-031`'s `jsrf_run.log` the advisor counted 58 lines
>   `[APUMMIO] read  0x20010 = 00000080`, all equal to 0x80, and one `[APU] started by the
>   title (SECTL=0000000F FECTL=0000100F)`. **There were zero "unimplemented GP/EP DSP
>   block" lines, so in that run the guest did not read GP/EP.**
> - Inferred: the traced value is a second quiet read taken after the access, not the
>   value captured from the guest's own read. For a stateless constant the two are
>   identical.
> - Uncertain: whether PIO_FREE = 0x80 is actually faithful. If VP methods complete
>   synchronously when written, an always-empty queue could be truthful device state
>   (allowed class 1). Not established with sources under the evidence rule, so treated
>   as a stub for now.
> - Uncertain, flagged and **not ruled on**: the same offset has two labels. The
>   grandfathered row calls `+0x20010` a "GP sample counter"; the VP register map calls it
>   PIO_FREE. The grandfathered row may have misidentified the register. Re-adjudicating
>   grandfathered rows is reserved to the owner (`jsrf-run-profiles.md:264-266`).
> - Not re-verified by the advisor: runs `20260924-143856-017` and `20260924-143745-909`,
>   the classifier output for either trap run, and the blocker-unchanged figures. Those
>   remain the Session's leads (B1–B4 above).
>
> **REVERSED BY:** (i) a real GP (DSP56300) or EP model landing behind the trap — values
> it produces would then be modelled, and part B narrows to whatever is still stubbed;
> (ii) sourced evidence, meeting the `jsrf-run-profiles.md:191-216` rule, that
> `PIO_FREE = 0x80` is truthful device state for a synchronously drained VP queue;
> (iii) evidence that the guest's progress in a given run never depended on a stubbed
> read; (iv) an owner decision re-adjudicating the grandfathered `0x020010` row;
> (v) a change to the classifier's semantics or the profile doc.
>
> **RECORD IN:** the case ruling goes verbatim in the review record for the packet being
> designed, with this child ID and route. The general rule — *"feature enablement does
> not make stub answers modelled; a claim's strictness depends on where each value it
> relies on came from"* — belongs in `docs/jsrf-run-profiles.md` §"Feature enablement —
> real capability, not a bypass", added as a separate policy edit, never by the packet
> itself (`:280-282`). The `0x020010` / `PIO_FREE` naming conflict goes to the owner as a
> re-adjudication item; it is **not** a blocker for this packet.

**Session note (not part of the ruling).** The two Advisor observations the Session had
*not* measured itself are now confirmed by the Session: `20260924-143932-031` contains no
"unimplemented GP/EP DSP block" line, and `apu_vp.c` returns the constant `0x80` for
`NV1BA0_PIO_FREE`. The Session did not verify the advisor's `apu_core.c` line numbers
`629-634` / `637-665` / `apu_vp.c:556-557`, which differ from the ranges the Session read
(`apu_core.c` `mcpx_apu_mmio_read` and `apu_vp.c` `mcpx_apu_vp_read`); the *substance* is
confirmed, the exact line numbers are the advisor's citation and are `UNKNOWN` to the
Session. The owner item (iv) above is **not** escalated by this session, because the
Advisor explicitly ruled it is not a blocker and §3.4 reserves owner contact for
decisions that change the objective — this one does not.

**One clarification about the ruling's part (A), so it is not misread as endorsing a bad
measurement.** Part (A) lists "max kernel ordinal" among the admissible observational
facts. That is a statement about a *validly obtained* ordinal. The Session's own use of
it was **not** valid: it compared `#200` across three runs that had all hit the default
log cap, which is a truncation artifact, not a measurement (B13). The ruling is not
wrong; the Session's application of that item was. Nothing in the ruling depends on the
ordinal — part (A)'s admissible set also names the live frame and the pending word, and
those are what B4's conclusion actually rests on.

**Also confirmed by the Session from the Planner's independent check:** `RECOMP_APU_TRACE`
prints a read-back value rather than the written value (B14), and GP/EP writes are dropped
by `mcpx_apu_dispatch_mmio` (B15). Both were verified against the toolkit source and the
original XBE instructions. They matter because the Advisor's part (A) admits "what the
guest writes to the APU window, and in what order" as strict evidence — and the current
trace **cannot** supply that for GP/EP. Any packet relying on that class of claim for
GP/EP must first fix the observability gap (§6.1.11).

### General rule recorded in its owning document (per the ruling's RECORD IN)

The ruling directed that the **general** rule be added to `docs/jsrf-run-profiles.md`
§"Feature enablement — real capability, not a bypass" as a separate policy edit, and that
the packet must not add it itself (`:280-282`, now `:301-303`). The Session made that
edit: a new subsection **"Enabling a feature does not turn stub answers into modelled
ones."** at `docs/jsrf-run-profiles.md:146-165`. It states that a claim is only as strict
as the source of each value it relies on; that a capability can put a stub in the path of
a read the guest depends on, making progress through it exploratory however the run
classifies; and that a capability changing several things at once is not a
single-variable comparison, naming the trap's three simultaneous effects.

Consequences the Session records rather than leaves implicit:

- The edit grew the file from **292 to 313 lines**. Anchors at or before line 145 are
  unchanged; everything after shifted **+23**. The Advisor's citations `:191-216`,
  `:222-230`, `:267-272`, `:280-282` now read `:214-239`, `:241-249`, `:288-292`,
  `:301-303`. The Planner was told, and told to prefer section names over line numbers —
  the plan itself already warns that line-number citations shift
  (`plan-jsrf-bare-minimum.md:96-101`).
- **This is a documentation-only edit**: it changes no classifier behaviour and no code.
  `python -X utf8 -m unittest` over all ten game test modules still reports `Ran 397
  tests … FAILED (failures=1)`, the single failure being the pre-existing baseline
  `test_agent_docs` defect at "Baseline conditions" item 1 — unchanged by this edit.
- The edit is **prospective and general**; it is not a waiver of any criterion after the
  fact and does not convert any existing measurement into a different profile. It is
  recorded here so the packet's citation of it has a durable, dated provenance.

## Mechanical pre-check of the `A4a-r1` draft (Session, workflow §5.1)

The Planner delivered `docs/packets/a4a-dsp-pending-word.md`, revision `A4a-r1`, status
`draft`, SHA-256 `B493B3FA0D7D0138B85EFF185ABDEE952495AB175B7DF3F3A814138EA2EFF816`
(26002 bytes). Before submitting it for adequacy review the Session must verify that the
mechanical parts run. The checks below were performed **read-only, against archived
artifacts**; no packet step was executed and nothing was committed, because the owner
paused the session to update the workflow.

**Verified to reproduce the packet's stated oracles and controls:**

| Packet element | Session check | Result |
|---|---|---|
| AC2 known-bad control | `Select-String -Path <143932-031>\jsrf_run.log -Pattern '^\s*\[APUMMIO\] write 0x3FF(14\|FC) = '` | 4 lines: `L794/L908/L919` `write 0x3FFFC = 00000000`, `L910` `write 0x3FF14 = 00000000`. The control **does** evaluate FAIL as the packet requires. |
| AC2 known-good control | `write 0x02000 = 0000000F` | present, `L955`. |
| AC4 oracle derivation | DSOUND section header (`VA 0x19E340`, `raw 0x18C000`) → delta `0x12340`; `0x1BA0A0 - 0x12340 = 0x1A7D60` | **exact.** The packet's stated file offset is correct and derivable as claimed. |
| AC4 main compare | exported `[0x803C0000, +0x800)` from `143932-031` vs XBE `0x1A7D60`, 0x800 bytes | **2048/2048 equal, no differing byte.** |
| AC4 known-bad control 1 | same compare at XBE `0x1A7D64` (shifted +4) | **1046/2048**, first difference at offset `0x0`. Control behaves as designed. |
| AC6 control strings | `trapped for MMIO` / `started by the title` | `143932-031` has `APU: 0xFE800000..0xFE880000 trapped for MMIO` at `L26`; the untrapped baseline `143745-909` has **neither** line. Emitted at toolkit `xbox_memory_layout.c:1734`. |
| AC3 oracle mechanism | `apu_note_unimplemented_block_read` fires from `mcpx_apu_mmio_read` whenever `addr >= 0x30000`, once per 64 KiB block (`block = addr >> 16`, `reported[8]`) | The read note is genuinely **unbounded by the 400-line trace cap**, so the packet's "two independent emitters" consistency oracle is structurally sound. |

**Two observations that are the Planner's to judge, recorded here rather than acted on:**

1. **The trace window is narrower than "the whole run".** In `143932-031` the log is 3484
   lines; the 344 `[APUMMIO]` lines run from `L773` to `L1840` and **none** appear after
   75 % of the log. So the trace records an early window, and "complete in-run inventory"
   means complete *within that window*, not across the entire run. The packet's AC3
   claim limits say it "covers the whole run up to freeze, all threads" — that phrasing
   and the observed window are in tension. This is a claim-limit wording question, not a
   step that fails, so it is for the Planner (or the adequacy review) to settle, not for
   the Session to reinterpret.
2. **The T1 mechanism is feasible as described.** `apu_decode_and_handle` computes the
   written value locally in each write branch (`val` from `*ctx_reg64(...)` for `89/88`,
   `imm` for `C7`/`C6`, and the OR/AND read-modify-write results) before calling
   `mcpx_apu_mmio_write`. So an out-parameter carrying that value to
   `apu_hook_handle_mmio` — the packet's suggested mechanism — is available without
   changing any argument passed to `mcpx_apu_mmio_write`/`_read`, which keeps T1 inside
   the packet's own Stop-if-2 boundary. Observed by reading the file.

Both observations are input to the adequacy review. The Session has not edited the packet,
which remains the Planner's revision `A4a-r1` at the hash above.

### Command verification results (§5.1: every command runs before submission)

The Session executed the packet's own R1 and R0 invocations verbatim (only the `--label`
changed, to `a4a-commandcheck-r1` / `-r0`, so they are not confused with the real evidence
runs). Both run:

| Run | Profile | outcome | APUMMIO | started | trapped | cap | decode-fail | budget |
|---|---|---|---|---|---|---|---|---|
| `logs/runs/20260924-155151-292-a4a-commandcheck-r1` | **STRICT** | `diagnostic_deadline` | 344 | 1 | 1 | 0 | 0 | 100000 |
| `logs/runs/20260924-155223-143-a4a-commandcheck-r0` | **STRICT** | `diagnostic_deadline` | 0 | 0 | 0 | 0 | 0 | 100000 |

So the packet's `Remove-Item` preamble, its environment block, its runner invocation, and
the AC6 control expectations all behave as written. The AC3 inventory commands were also
prototyped and work; on the R1 check they return 15 GP/EP `[APUMMIO]` lines (all
`= 00000000`, i.e. the T1 defect), **zero** unimplemented-block notes, zero cap lines,
zero decode failures, and 58 `read 0x20010` lines. Zero GP/EP read notes is the coverage
witness that `R-READY` depends on, and it reproduces on the current revision.

### BLOCKING DEFECT FOUND IN `A4a-r1` AC5 — reported, not repaired (Session is a contract role)

**Location:** `docs/packets/a4a-dsp-pending-word.md`, `A4a-AC5` procedure (the
`Select-String -Path <R1>\stacks.txt -Pattern 'sub_001A1769\+0xB20 .*recomp_0005\.c:6749'`
step) and its reuse inside `A4a-AC6`.

**Defect:** the pattern pins the frame offset to exactly `+0xB20`, but the offset the
collector records **varies between runs of the byte-identical executable**. Measured on
four runs whose `exe_sha256` is in every case
`879716046b99cecfb61a8373237df24fcdefa4d00317f50c726aeae7a35187ef`:

| Run | frame offset | `recomp_0005.c` line | outcome |
|---|---|---|---|
| `20260924-143745-909-session-startup-baseline` | `sub_001A1769+0xB20` | 6749 | `diagnostic_deadline` |
| `20260924-143932-031-apu-trap-register-trace` | `sub_001A1769+0xB20` | 6749 | `diagnostic_deadline` |
| `20260924-155151-292-a4a-commandcheck-r1` | `sub_001A1769+**0xB24**` | 6749 | `diagnostic_deadline` |
| `20260924-155223-143-a4a-commandcheck-r0` | `sub_001A1769+**0xB2B**` | 6749 | `diagnostic_deadline` |

The literal pattern therefore matches **0 times** on the last two runs while the guest is
demonstrably still spinning at the same source line.

**Concrete failure scenario (workflow §3.1):** the Session executes R1 exactly as written;
the collector samples the spin at `+0xB24` instead of `+0xB20`; AC5's `PASS` predicate
"≥1 matching frame" is false and its `FAIL` predicate "no matching frame" is true; the
packet's decision rows are evaluated in order and **`R-MOVED`** matches before any other
row, whose action is "the instrumentation is not observation-only, or the stop is
nondeterministic → stop and escalate to the Advisor. **Do not** read this as progress."
So a faithful literal execution reports a false FAIL, discards a good run, and sends the
project to the Advisor over a sampling artifact. The same pattern is reused in AC6, so the
false FAIL would be reported twice. This is a false-FAIL defect in the operative contract,
which is the blocking test, and it is reachable on the packet's own two runs — not
hypothetical.

**Not repaired by the Session, deliberately.** Repairing a criterion is the Planner's
authority (`docs/agent-workflow.md` §2.3, §5.4); a contract role must not reinterpret one
(§2.2.6). The Session's mechanical pass exists to surface exactly this before submission.
Recorded here so the finding is durable if the session is interrupted.

**Observation offered to the Planner, not a proposed criterion.** `recomp_0005.c:6749` is
stable across all four runs and in every run examined this session; the `+0xB2x` suffix is
the varying part. A pattern anchored on the source line rather than the byte offset would
have matched all four. The Session does not decide this.

**Also noted for the Planner (not blocking, claim-limit wording):** as recorded above, the
`[APUMMIO]` window in `143932-031` runs from log line 773 to 1840 of 3484, with nothing
after 75 % of the log, so AC3's "covers the whole run up to freeze, all threads" is in
tension with the observed window. The verified R1 check behaves the same way (344 lines,
none late in the log).
