# Fresh-session startup receipt

Filled per `docs/agent-workflow.md` §0 by `scripts/gen-startup-receipt.py` and then
completed from this session's own probes; this is a receipt, not a second policy.
Fields marked **UNVERIFIED** cannot be measured by a generator and must be
filled from the probe that establishes them.

Generated: 2026-10-04T00:39:23.739759+00:00

## Identity and handoff

- Session ID/date/harness: 2026-10-03, DSH; session
  `session-c6018192-e90e-4718-9616-ff2165ca495b`
  (harness: `DSH_PROFILE=web`, `DSH_WEB_URL=http://127.0.0.1:3080`)
- Actual main model/effort: `workbuddy-ai/deepseek-v4.1-flash` @ `max` — the §1
  Session row. Verified from this session's own `request/header` record in
  `C:\Users\logic\.dsh\sessions\--C-Users-logic-Repos-my_xbox_game--\session-c6018192-e90e-4718-9616-ff2165ca495b\`,
  not from a display name.
- Workflow/plan/run-profile revisions and dirty diff identity: workflow `1c9489e0c5573f8b`, plan `4bcc474277c42d3f`, run profiles `4bb50f539ba4b7e8`, recomputed from the current file bytes because this session's own staffing edit changed the workflow and the plan. The paths the staffing commit changes are `docs/agent-workflow.md`, `docs/reviews/startup-current.md` and `tests/test_reviewer_routes.py`.
- Game revision/status: `master` `dfc22ff56610e6627bf28d0da322e3c1a328983f` at the startup pull of the previous turn; `1b7bd3d000e5c499f9096babf9974a2cba3052e2` (clean) at the start of this staffing/discovery turn.
- Toolkit revision/status: `main` `929856fcfc145036252510509abaa0782d910a22` (clean, unchanged this session)
- Unrelated edits preserved: 0 game, 0 toolkit. Toolkit pulled first
  (`Already up to date.`), then game (`c333048..dfc22ff`, fast-forward,
  `docs/agent-workflow.md`, `docs/session-start-template.md`,
  `plan-jsrf-bare-minimum.md`, `scripts/check-route-allowlist.py`,
  `scripts/gen-startup-receipt.py`, `scripts/record-review.py`,
  `tests/test_reviewer_routes.py`, `tests/test_route_allowlist.py`).
- CURRENT PACKET copied from plan (packet + exact revision + SHA-256), or NONE:
  **NONE** — `## CURRENT PACKET — none` (`plan-jsrf-bare-minimum.md:56`)
  - plan hash `4bcc474277c42d3f` (current bytes; the packet block itself is unchanged)
  - block: "No packet is promoted. **The fail-fast observer attempt is PARKED by the
    owner — STOP UNKNOWN, no retry, no further correction, consultation or list
    extension.**" The plan also states "The first packet is C1 (§7); it is promoted
    here, by exact revision and hash, only after its adequacy review returns
    `ADEQUATE`" (`plan-jsrf-bare-minimum.md:68`).
- Dependencies and their recorded acceptance reviews: none (no packet promoted)
- Next exact authorized action: **owner-directed §0.6 maintenance and discovery** — the
  owner-authorized Decision guardrail staffing correction, and the fcmov discovery
  test recorded in plan §13 item 10. No game-behavior implementation: with no packet
  promoted that stays `BLOCKED` until one is promoted (§5).
- Build/run owner and worker write ownership: Session owns integration, the
  build/run, evidence collection and record keeping (§2.2). No worker was spawned
  this session.

## Route resolution — PASS / BLOCKED

Resolved live in this session with `list_subagent_models`; every child route below
was read from that child's own `request/header` record under
`C:\Users\logic\.dsh\sessions\--C-Users-logic-Repos-my_xbox_game--\<child-id>\`,
which is harness metadata rather than a self-report.

- Planner: `claude/claude-opus-5-5` @ `high` — exactly one advertised route
  (`list_subagent_models claude` → `claude/claude-opus-5-5`; efforts low, medium,
  high, xhigh, max); `route: LIVE_RESOLVE`, fresh child per packet. Not spawned
  this session (no packet to plan).
- Persistent advisor: requested `claude` / `claude-opus-5-5` @ `xhigh`; returned
  route identity `claude` / `claude-opus-5-5` @ `xhigh` (`maxTokens: 128000`) —
  **PASS**, `route: CONTINUABLE_PINNED`.
- Decision guardrail: requested `workbuddy-ai` / `grok-4.7` @ `xhigh`; returned
  route identity `workbuddy-ai` / `grok-4.7` @ `xhigh` — **PASS**,
  `route: CONTINUABLE_PINNED`. §1 named `grok/Grok 4.7` until this session, which
  the active catalog does not advertise; the owner made the §3.4 staffing decision
  to move the row to the advertised route, and §1 now reads
  `provider: workbuddy-ai`, `model: grok-4.7`, `reasoning_effort: xhigh`. The
  advertised catalog entry is `workbuddy-ai/grok-4.7` — "Grok-4.7 · x1.90",
  efforts low, medium, high, xhigh.
- Reviewer: requested `claude` / `claude-opus-5-5` @ `high`; returned route
  identity `claude` / `claude-opus-5-5` @ `high` (`maxTokens: 128000`) — **PASS**
  (`route: LIVE_RESOLVE`, a fresh child per packet review and one per turn for the
  turn-end review). Exactly one advertised route matches, and it offers `high`.
- Workers: `workbuddy-ai/deepseek-v4.1-flash` @ `max` (the §1 Worker row). No
  worker spawned this session.
- Exact error or ambiguity, if any: none. All four §1 routes resolve to exactly one
  advertised entry.

**Allow-list.** `just route-check` (`scripts/check-route-allowlist.py`, checker
`jsrf-route-allowlist/1`) — **exit 0**, no findings, all three roster routes covered:

```text
checker jsrf-route-allowlist/1
  roster routes    : 3
    OK   claude/claude-opus-5-5
    OK   workbuddy-ai/deepseek-v4.1-flash
    OK   workbuddy-ai/grok-4.7
  allow-list routes: 15
  fallbacks named  : none (owner-reserved; see the note)
  no findings
```

The checker and its matching were not changed: the roster row was corrected to the
route the active profile patch (`C:\Users\logic\.dsh\profiles\web\cordis.patch.yml`)
already contains.

## Reviewer probe — PASS

- Child ID: `fccd1e17-0e2d-4865-a8cb-abe5b9a1d228`
- Fresh token: `9862714373a971a175cd6e398fcb8e86`
- Empty-evidence answer: an empty evidence set exercises nothing, so the criterion
  is `CANNOT VERIFY`; missing, malformed, stale, unexercised or empty evidence never
  defaults to PASS (`docs/agent-workflow.md:279`, `:1358`;
  `docs/reviews/review-record.schema.json:124`).
- Command and output hash: `git -C C:\Users\logic\Repos\my_xbox_game rev-parse HEAD`
  → `dfc22ff56610e6627bf28d0da322e3c1a328983f`, SHA-256
  `46f93d69cab42c4deda410de90a8f8ce89d980e42baf49103d0de509afaa270e`
- Effort/route: `claude` / `claude-opus-5-5` @ `high` (from its `request/header`).
- Result: **PASS**. Its independently computed hash matches the Session's own
  measurement of the same command at the same revision (`dfc22ff`, SHA-256
  `46f93d69cab42c4deda410de90a8f8ce89d980e42baf49103d0de509afaa270e`), computed
  by the Session with `pwsh` over stdout with trailing newlines stripped, UTF-8
  encoded. The receipt commit moves `HEAD`, so re-running the command afterwards
  yields a different hash; that is the revision change, not a changed measurement.
- Exact error or missing evidence: none. The probe child is discarded.

## Decision guardrail probe — PASS

- Guardrail child ID: `e01fab41-5669-47b1-b96a-ebf99be20f69` (`mode: continuable`,
  from its `subagent/catalog` record)
- Route: `workbuddy-ai` / `grok-4.7` @ `xhigh` (the corrected §1 row), spawned with
  all three explicit parameters and read back from its own `request/header`.
- Workspace: `C:\Users\logic\Repos\my_xbox_game` (session-scoped resolution).
- Named fact read with its own tool call: `docs/agent-workflow.md`, the §1 Decision
  guardrail row, which it quoted verbatim; it also ran
  `git -C C:\Users\logic\Repos\my_xbox_game rev-parse HEAD` itself and returned
  `1b7bd3d000e5c499f9096babf9974a2cba3052e2`, matching the Session's own reading.
- Turn-2 marker: `GUARDRAIL-PROBE-4T8N-20261004`, returned verbatim by the same
  child on continuation.
- Result: **PASS** for the §0.5 requirements.
- Exact error or missing evidence: none. The earlier §1 route (`grok` / `Grok 4.7`)
  was BLOCKED because the catalog registers no `grok` provider; that route is
  superseded by the owner's staffing decision and is no longer the §1 row.

## Persistent advisor probe — PASS

- Child ID: `79766644-778c-469c-ada5-8716057a0f39` (`mode: continuable`, from its
  `subagent/catalog` record)
- Turn 1 reference; unique marker given: `ADVISOR-PROBE-7Q4M-20261003`
- Named file and the fact deliberately omitted from the brief: read
  `scripts/check-route-allowlist.py` and reported the checker identifier
  `jsrf-route-allowlist/1` (`CHECKER_VERSION` at `:39`, printed at `:173`). The
  brief named the file but never the string.
- Advisor's answer; checked against the file: `jsrf-route-allowlist/1` verified by
  the Session against the file and against the checker's own output line.
- Turn 2 reference (same child, marker not repeated); returned marker:
  `ADVISOR-PROBE-7Q4M-20261003`, returned verbatim by the same child.
- Route/effort: `claude` / `claude-opus-5-5` @ `xhigh` (from its `request/header`).
- Result: **PASS** — continuable, exact §1 route, listed effort, listing
  visibility, and a continuation that reaches the same child.
- Exact error or missing evidence: none.

## Packet readiness — PASS / BLOCKED / UNKNOWN

- Frozen revision/hash matches `CURRENT PACKET`: N/A — no packet promoted
- Adequacy review record and verdict: N/A — no packet promoted
- Deferred advisories (recorded, not acted on): none raised this session
- Prerequisites / tooling checks:
  - just: just 1.58.0 (C:\Users\logic\AppData\Local\Microsoft\WinGet\Packages\Casey.Just_Microsoft.Winget.Source_8wekyb3d8bbwe\just.EXE)
  - pre-commit: pre-commit 4.6.2 (C:\Users\logic\AppData\Roaming\Python\Python313\Scripts\pre-commit.EXE)
  - clang-cl: clang version 23.1.2 (https://github.com/llvm/llvm-project 85ac560262434c9ccfc0c183ec22d4138ed647fb) (C:\Program Files\LLVM\bin\clang-cl.EXE)
  - ttd: Microsoft (R) TTD 1.01.11 x64 (C:\Users\logic\AppData\Local\Microsoft\WindowsApps\ttd.EXE)
  - duckdb: 1.5.6 (not on PATH)
  - python: 3.13.2 (C:\Python313\python.exe)
  - free space: 217.81 GB (above 15.0 GB floor; `just disk` gate PASS)
- State/plan disagreements and how they were escalated: none. The §1 Decision
  guardrail route was BLOCKED under the roster as written; the owner made the §3.4
  staffing decision and §1 was corrected to the advertised route.
- Overall disposition and next action: **PASS** for the required startup items.
  Decision guardrail (§0.5), Advisor (§0.4) and Reviewer (§0.3) all PASS, and
  `just route-check` is green with all three roster routes covered. No packet is
  promoted, so game-behavior implementation is `BLOCKED` until one is promoted
  (§5). Owner-directed §0.6 work — the staffing correction, the alpha watch run and
  the fcmov discovery test — is recorded in
  `docs/reviews/owner-sega-600-observations.md` and plan §13 item 10.

Do not mark readiness PASS with a failed or unknown required item. A provider
catalog entry is not a completed invocation. A new advisor answering the
follow-up is not continuation. A readiness response is not acceptance of a
game packet.
