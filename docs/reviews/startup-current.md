# Fresh-session startup receipt

Filled per `docs/agent-workflow.md` §0.6 by `scripts/gen-startup-receipt.py`; this is a receipt, not a second policy.
Fields marked **UNVERIFIED** cannot be measured by a generator and must be
filled from the probe that establishes them.

Generated: 2026-10-01T05:26:56.446531+00:00

## Identity and handoff

- Session ID/date/harness: 2026-09-30, DSH; session started 02:49:47-07:00
  (harness: `DSH_PROFILE=web`, `DSH_WEB_URL=http://127.0.0.1:3080`)
- Actual main model/effort: `workbuddy-ai/deepseek-v4.1-flash` @ `max` (the §1
- Workflow/plan/run-profile revisions and dirty diff identity: workflow `4ec760339ea33f4d`, plan `7bd50133a0d13e3b`, run profiles `775757cdea36bb2f`
- Game revision/status: `master` `0597d94a72dfeb5c39a4ef11173f255af1a858d7` (DIRTY)
- Toolkit revision/status: `main` `86113c730fe452508ce3f02f9fa1982260846599` (clean)
- Unrelated edits preserved: 1 game, 0 toolkit
- CURRENT PACKET: present
  - plan hash `7bd50133a0d13e3b78ce964f5fe7d1ecb4ce9581024d73487707ca8eadcec0a5`
  - block: No packet is promoted. Phase 0 (§4) and the chores of §5–§6 run as owner-directed chores, which
need no packet. The first packet is C1 (§7); it is promoted here, by exact revision and hash, only
after its adequacy review returns `ADEQUATE` (`docs/agent-workflow.md` §5).
- Dependencies and their recorded acceptance reviews: none (no packet promoted)
- Next exact authorized action: Phase 1 tooling chores in the plan's order — T14, T6, T7, T4, then T1, T2, T8–T13, T5, with T3 now that the owner's assets are present
- Build/run owner and worker write ownership: Session owns integration/build/run; bounded workers were given disjoint new-file scopes (T9, T10)

## Route resolution — PASS / BLOCKED

Resolved live in this session by the Session, not by this generator:

- Planner: not probed at startup (workflow §0 says the Planner needs no separate probe; each turn reports its own `reasoningEffort`). Expected `Muse Spark 1.3` @ `high` via `skill: muse-worker`, fresh handle. **No Planner call was made this session** — no packet reached planning.
- Persistent advisor: requested `claude` / `claude-opus-5-5` @ `high`, `route: CONTINUABLE_PINNED`. Resolved live: exactly one advertised route, `claude/claude-opus-5-5`. **PASS** — see the probe section.
- Acceptance reviewer: requested `codex` / `gpt-6.1-sol` @ `high`, `route: LIVE_RESOLVE`. **NOT PROBED this session.** The route resolves (`codex/gpt-6.1-sol` is advertised, and a pinned `codex`/`gpt-6.1-sol` @ `high` control child answered a probe), but §0 step 3's acceptance probe — fresh token, empty-evidence reason, and the hash of a named read-only command — was **not** run, because this session made no acceptance claim that needs a reviewer. This is **UNVERIFIED**, not PASS.
- Workers: `workbuddy-ai/deepseek-v4.1-flash` @ `max` (the §1 Worker row). No worker child was spawned this session.
- Exact error or ambiguity, if any: the Claude route was initially **BLOCKED** — `list_subagent_models --provider claude` advertised nothing and a spawn returned `No eligible account for claude/claude-opus-5-5` (the CLI reported `OAuth session expired and could not be refreshed`). The owner logged in, after which the route resolved to exactly one entry and the probe passed. Recorded because a route that resolves is not a route that can serve.

## Acceptance reviewer probe — PASS / FAIL / UNKNOWN

- Child ID; fresh token; response reference; empty-evidence answer; command and output hash; effort; result: **NOT RUN** (UNVERIFIED). No acceptance claim was made this session; the work was owner-directed chores (F0a, F0b, F1) whose closure is the gate script's own output (`docs/agent-workflow.md` §5.8).
- Exact error or missing evidence: the probe itself was not performed.

## Persistent advisor probe — PASS / FAIL / UNKNOWN

- Child ID: `c8c75ebb-9a4e-45e1-b052-f6777a5b2f94` — route `claude/claude-opus-5-5`, provider `claude`, effort `high`, spawned continuable.
- Turn 1 reference; unique marker given: marker `JSRF-ADV-MARKER-4X7Q2N`, supplied **only** in turn 1.
- Named file and the fact deliberately omitted from the brief: `docs/jsrf-compatibility-ledger.md`; L25's Class and Where, and D4's Status.
- Advisor's answer; checked against the file: `L25 CLASS: Approximated`; `L25 WHERE: src/apu/apu_mixdown.c:101`; `D4 STATUS: Mitigation, opt-in: L34 (RECOMP_GUEST_SERIAL=1), not yet run on the title`. All three checked against the file and correct.
- Turn 2 reference (same child, marker not repeated); returned marker: `CONTINUATION-OK marker=JSRF-ADV-MARKER-4X7Q2N`.
- Result: **PASS** — all six §0 step 4 predicates hold together: exact §1 model/provider, listed effort, present in the continuable-agent listing, reachable by `send_message`, turn 1 read the named file and reported the omitted facts, turn 2 returned the turn-1 marker to the same child.
- Exact error or missing evidence: none after the owner's login. Before it, the route was BLOCKED as recorded above.

## Packet readiness — PASS / BLOCKED / UNKNOWN

- Frozen revision/hash matches `CURRENT PACKET`: N/A — no packet
- Adequacy review record and verdict: N/A — no packet promoted
- Deferred advisories (recorded, not acted on):
- Prerequisites / tooling checks:
  - just: just 1.58.0 (C:\Users\logic\AppData\Local\Microsoft\WinGet\Packages\Casey.Just_Microsoft.Winget.Source_8wekyb3d8bbwe\just.EXE)
  - pre-commit: pre-commit 4.6.2 (C:\Users\logic\AppData\Roaming\Python\Python313\Scripts\pre-commit.EXE)
  - clang-cl: clang version 23.1.2 (https://github.com/llvm/llvm-project 85ac560262434c9ccfc0c183ec22d4138ed647fb) (C:\Program Files\LLVM\bin\clang-cl.EXE)
  - ttd: Microsoft (R) TTD 1.01.11 x64 (C:\Users\logic\AppData\Local\Microsoft\WindowsApps\ttd.EXE)
  - duckdb: 1.5.6 (not on PATH)
  - python: 3.13.2 (C:\Python313\python.exe)
  - free space: 232.29 GB (above 15.0 GB floor)
- State/plan disagreements and how they were escalated: none. `just check` was red on generation provenance; the repair was taken to the Persistent Advisor under §2.3 and its ruling is recorded in `docs/reviews/f0b-provenance-and-record-array-writer.md`.
- Overall disposition and next action: the Persistent Advisor route **PASS**; the Acceptance reviewer probe **NOT RUN (UNVERIFIED)** and not required, no acceptance claim being made. No packet is promoted, so packet implementation is **BLOCKED** and the plan's §13 owner-directed chores continue. Next: F3 (iterate the stop now that F1 has named the writer).

Do not mark readiness PASS with a failed or unknown required item. A provider
catalog entry is not a completed invocation. A new advisor answering the
follow-up is not continuation. A readiness response is not acceptance of a
game packet.
