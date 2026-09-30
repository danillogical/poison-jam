# Fresh-session startup receipt

Filled per `docs/agent-workflow.md` §0.6 by `scripts/gen-startup-receipt.py`; this is a receipt, not a second policy.
Fields marked **UNVERIFIED** cannot be measured by a generator and must be
filled from the probe that establishes them.

Generated: 2026-09-30T11:33:50.230657+00:00

## Identity and handoff

- Session ID/date/harness: 2026-09-30, DSH; session started 02:49:47-07:00
  (harness: `DSH_PROFILE=web`, `DSH_WEB_URL=http://127.0.0.1:3080`)
- Actual main model/effort: `workbuddy-ai/deepseek-v4.1-flash` @ `max` (the §1
- Workflow/plan/run-profile revisions and dirty diff identity: workflow `c30a32ca7c94c3b9`, plan `b4a8e7665c34a54c`, run profiles `9e4838af2cd7746e`
- Game revision/status: `master` `d95ff82b16cff0f7e5692ffea10479b3f982174b` (DIRTY)
- Toolkit revision/status: `main` `4ec3eca0d24d4d3d4008ce13c024a266758603ad` (clean)
- Unrelated edits preserved: 3 game, 0 toolkit
- CURRENT PACKET: present
  - plan hash `b4a8e7665c34a54cb086d1fe82990f9e7ab37d1d4a6467efb6f7b9009c7bb15a`
  - block: No packet is promoted. Phase 0 (§4) and the chores of §5–§6 run as owner-directed chores, which
need no packet. The first packet is C1 (§7); it is promoted here, by exact revision and hash, only
after its adequacy review returns `ADEQUATE` (`docs/agent-workflow.md` §5).
- Dependencies and their recorded acceptance reviews: none (no packet promoted)
- Next exact authorized action: Phase 1 tooling chores in the plan's order — T14, T6, T7, T4, then T1, T2, T8–T13, T5, with T3 now that the owner's assets are present
- Build/run owner and worker write ownership: Session owns integration/build/run; bounded workers were given disjoint new-file scopes (T9, T10)

## Route resolution — PASS / BLOCKED

Resolved live in this session by the Session, not by this generator:

- Planner: not probed at startup (workflow §0 says the Planner needs no separate probe; each turn reports its own `reasoningEffort`). Expected `Muse Spark 1.3` @ `high` via `skill: muse-worker`, fresh handle. **No Planner call was made this session** — no packet reached planning. (Muse Spark 1.3 @ high, `skill: muse-worker`, fresh handle)
- Persistent advisor: requested `claude` / `claude-opus-5-5` @ `high`, `route: CONTINUABLE_PINNED`. Resolved live: exactly one advertised route, `claude/claude-opus-5-5`, supporting `low/medium/high/xhigh/max`. **PASS** — see the probe section. (Claude Opus 5.5 @ high, `route: CONTINUABLE_PINNED`)
- Acceptance reviewer: requested `codex` / `gpt-6.1-sol` @ `high`, `route: LIVE_RESOLVE`. Resolved live: exactly one advertised route, `codex/gpt-6.1-sol`. **PASS** — see the probe section. (GPT-6.1 Sol @ high, `provider: codex`, `route: LIVE_RESOLVE`)
- Workers: `workbuddy-ai/deepseek-v4.1-flash` @ `max` (the §1 Worker row). **PASS** — see the probe section.
- Exact error or ambiguity, if any:

## Acceptance reviewer probe — PASS / FAIL / UNKNOWN

- Child ID; fresh token; response reference; empty-evidence answer; command and output hash; effort; result: UNVERIFIED (not measurable by this generator; see the note)
- Exact error or missing evidence:

## Persistent advisor probe — PASS / FAIL / UNKNOWN

- Child ID: `251fe018-dfdc-4608-89b8-ca7a1e8d27ec`
- Turn 1 reference; unique marker given:
- Named file and the fact deliberately omitted from the brief:
- Advisor's answer; checked against the file:
- Turn 2 reference (same child, marker not repeated); returned marker:
- Result:
- Exact error or missing evidence:

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
  - free space: 63.65 GB (above 50.0 GB floor)
- State/plan disagreements and how they were escalated:
- Overall disposition and next action: all three senior routes **PASS**; no packet

Do not mark readiness PASS with a failed or unknown required item. A provider
catalog entry is not a completed invocation. A new advisor answering the
follow-up is not continuation. A readiness response is not acceptance of a
game packet.
