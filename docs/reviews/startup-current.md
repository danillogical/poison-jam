# Fresh-session startup receipt

Filled per `docs/agent-workflow.md` §0.6 by `scripts/gen-startup-receipt.py`; this is a receipt, not a second policy.
Fields marked **UNVERIFIED** cannot be measured by a generator and must be
filled from the probe that establishes them.

Generated: 2026-09-30T11:03:07.679574+00:00

## Identity and handoff

- Session ID/date/harness: 2026-09-30, DSH; session started 02:49:47-07:00
  (harness: `DSH_PROFILE=web`, `DSH_WEB_URL=http://127.0.0.1:3080`), launched from a
  **new elevated** command prompt so `ttd.exe` recording works
- Actual main model/effort: `workbuddy-ai/deepseek-v4.1-flash` @ `max` (the §1
  Session row; recorded from configuration, not independently verifiable from
  inside the session)
- Workflow/plan/run-profile revisions and dirty diff identity: workflow `c30a32ca7c94c3b9`, plan `b4a8e7665c34a54c`, run profiles `9e4838af2cd7746e`
- Game revision/status: `master` `5fc6348128780493177cc6dc213e06a2606038d4` (clean at startup; the working tree carries this session's later edits)
- Toolkit revision/status: `main` `4ec3eca0d24d4d3d4008ce13c024a266758603ad` (clean)
- Unrelated edits preserved: none were present at startup (both trees clean); no unrelated edit was created or reverted
- CURRENT PACKET: present in the plan, and it names **none** — "No packet is promoted. Phase 0 (§4) and the chores of §5–§6 run as owner-directed chores, which need no packet."
  - plan hash `b4a8e7665c34a54cb086d1fe82990f9e7ab37d1d4a6467efb6f7b9009c7bb15a`
- Dependencies and their recorded acceptance reviews: none (no packet promoted)
- Next exact authorized action: Phase 1 tooling chores in the plan's order — T14, T6, T7, T4, then T1, T2, T8–T13, T5, with T3 now that the owner's assets are present
- Build/run owner and worker write ownership: Session owns integration/build/run; bounded workers were given disjoint new-file scopes (T9, T10)

## Route resolution — PASS / BLOCKED

Resolved **live** in this session with `list_subagent_models` (§1). The
model-selection policy repaired on disk at the end of the previous session was
re-tested here and **works**.

- Planner: not probed at startup (workflow §0 says the Planner needs no separate probe; each turn reports its own `reasoningEffort`). Expected `Muse Spark 1.3` @ `high` via `skill: muse-worker`, fresh handle. **No Planner call was made this session** — no packet reached planning.
- Persistent advisor: requested `claude` / `claude-opus-5-5` @ `high`, `route: CONTINUABLE_PINNED`. Resolved live: exactly one advertised route, `claude/claude-opus-5-5`, supporting `low/medium/high/xhigh/max`. **PASS** — see the probe section.
- Acceptance reviewer: requested `codex` / `gpt-6.1-sol` @ `high`, `route: LIVE_RESOLVE`. Resolved live: exactly one advertised route, `codex/gpt-6.1-sol`. **PASS** — see the probe section.
- Workers: `workbuddy-ai/deepseek-v4.1-flash` @ `max` (the §1 Worker row). **PASS** — see the probe section.
- Exact error or ambiguity, if any: none. The previous session's Advisor block (`child model selection is disabled for this tool instance`) did **not** recur; the `subagent-model-selection-settings` patch in `C:\Users\logic\.dsh\profiles\web\cordis.patch.yml` (14 unique routes, `enabled: true`) is now sampled at session composition.

## Acceptance reviewer probe — PASS / FAIL / UNKNOWN

One review stage (workflow §1); probed on a fresh child that ran one read-only command.

- Child ID: `6a045ab8-44fc-4f13-93f3-df2f3afc1666`; provider/model `codex` / `gpt-6.1-sol`; effort `high` (requested and accepted by the spawn surface)
- Fresh token: `ACC-SMOKE-c9208be7`
- Command and output hash: `git -C C:\Users\logic\Repos\my_xbox_game rev-parse HEAD` -> `f156aa91178b18c387c2c834a5fa6de680bb717a`; SHA-256 of the 41-byte UTF-8 output with a single trailing LF = `dc3c1c3f7ae22cb6d4efb9dc724e47aba3c82a03e1027f787ea0a06308fc7243`. The value matches the independently measured HEAD.
- Empty-evidence answer: "An empty evidence set must fail a mandatory acceptance criterion because it contains no verifiable observations establishing that the required predicate was satisfied."
- Effort: `high`, **exposed and pinned** on this path — the gap the previous session recorded as PARTIAL, now closed.
- Result: **PASS**. Child discarded after the probe.

## Persistent advisor probe — PASS / FAIL / UNKNOWN

- Child ID: `251fe018-dfdc-4608-89b8-ca7a1e8d27ec`
- Route: `claude` / `claude-opus-5-5` @ `high`, spawned through the continuable `subagent` path with model selection enabled
- Turn 1: read `docs/reviews/strict-horizon-ledger.md` and reported the `Run ID` (`20260930-001405-390-v1-verified-strict`) and `Horizon event` of the last data row — a fact deliberately omitted from the brief. It generated marker `ADVISOR-PROBE-7c3e9a`.
- Turn 2 (same child, marker **not** repeated in the prompt): returned `ADVISOR-PROBE-7c3e9a` and the same Run ID, and stated it had read no file and run no command that turn.
- Continuability: the child appears in the model-facing continuable-agent listing and `send_message` reached the same child. **Same-child state proved.**
- Result: **PASS**. The child was retained for the session and issued the W11 ruling (§2.3) — see `docs/reviews/rulings/ttd-query-decision-input.md`.

## Packet readiness — PASS / BLOCKED / UNKNOWN

- Frozen revision/hash matches `CURRENT PACKET`: N/A — no packet
- Adequacy review record and verdict: N/A — no packet promoted
- Deferred advisories (recorded, not acted on): the W11 ruling's T1 reopen list — F3 (per-alias limit), F4 (`ttd-query.py` does not evaluate the known positive and returns 0 when controls fail), F5 (no mirror positive; a MISSING control, not waived), F6 (`XBOX_NUM_MIRRORS` never checked against the run's own `expected`)
- Prerequisites / tooling checks:
  - just: just 1.58.0 (C:\Users\logic\AppData\Local\Microsoft\WinGet\Packages\Casey.Just_Microsoft.Winget.Source_8wekyb3d8bbwe\just.EXE)
  - pre-commit: pre-commit 4.6.2 (C:\Users\logic\AppData\Roaming\Python\Python313\Scripts\pre-commit.EXE)
  - clang-cl: clang version 23.1.2 (https://github.com/llvm/llvm-project 85ac560262434c9ccfc0c183ec22d4138ed647fb) (C:\Program Files\LLVM\bin\clang-cl.EXE)
  - ttd: Microsoft (R) TTD 1.01.11 x64 (C:\Users\logic\AppData\Local\Microsoft\WindowsApps\ttd.EXE)
  - duckdb: 1.5.6 (not on PATH)
  - python: 3.13.2 (C:\Python313\python.exe)
  - free space: 63.66 GB (above 50.0 GB floor)
  - XbSymbolDatabase CLI: present; `v4.0.166-39-g20eced5`
  - xemu: present, `0.8.136`; its five configured assets all resolve
- State/plan disagreements and how they were escalated: **one, reported not
  resolved.** Plan §13's older "Next action" text still says "T14's free-space
  gate, then V1 → V2 → V3 → V4", but Phase 0 V1–V5 is already complete and
  committed at this revision pair (V5 landed in `bd887f0`, the plan's own §4 V5 row
  is marked DONE, and `docs/reviews/strict-horizon-ledger.md` records V3's result).
  Re-running V2/V3 would rewrite a committed generated tree to re-derive values
  already on disk, so this session did **not** repeat Phase 0 and instead executed
  the genuinely outstanding Phase 1 chores. This is a reading of a stale plan
  pointer, reported explicitly rather than resolved silently.
- Overall disposition and next action: all three senior routes **PASS**; no packet
  is promoted, so accepted-packet work is BLOCKED by §0.5 (not by a route), and
  the owner-directed Phase 1 chores proceed. Next action: the discovery packet the
  W11 ruling requires on the `0xC0000409` exit under TTD, because C1 is blocked
  until a traced run reaches the horizon.

Do not mark readiness PASS with a failed or unknown required item. A provider
catalog entry is not a completed invocation. A new advisor answering the
follow-up is not continuation. A readiness response is not acceptance of a
game packet.
