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
- Workflow/plan/run-profile revisions and dirty diff identity: workflow `b0174ae9d4af7af7`, plan `f20c5d97cff65531`, run profiles `4bb50f539ba4b7e8`
- Game revision/status: `master` `dfc22ff56610e6627bf28d0da322e3c1a328983f` (clean)
- Toolkit revision/status: `main` `929856fcfc145036252510509abaa0782d910a22` (clean)
- Unrelated edits preserved: 0 game, 0 toolkit. Toolkit pulled first
  (`Already up to date.`), then game (`c333048..dfc22ff`, fast-forward,
  `docs/agent-workflow.md`, `docs/session-start-template.md`,
  `plan-jsrf-bare-minimum.md`, `scripts/check-route-allowlist.py`,
  `scripts/gen-startup-receipt.py`, `scripts/record-review.py`,
  `tests/test_reviewer_routes.py`, `tests/test_route_allowlist.py`).
- CURRENT PACKET copied from plan (packet + exact revision + SHA-256), or NONE:
  **NONE** — `## CURRENT PACKET — none` (`plan-jsrf-bare-minimum.md:56`)
  - plan hash `f20c5d97cff6553108a49047c3e09ee0624806f55d4c0b843886101705f70545`
  - block: "No packet is promoted. **The fail-fast observer attempt is PARKED by the
    owner — STOP UNKNOWN, no retry, no further correction, consultation or list
    extension.**" The plan also states "The first packet is C1 (§7); it is promoted
    here, by exact revision and hash, only after its adequacy review returns
    `ADEQUATE`" (`plan-jsrf-bare-minimum.md:68`).
- Dependencies and their recorded acceptance reviews: none (no packet promoted)
- Next exact authorized action: **owner-directed discovery under §0.6** — the
  bounded watch run recorded in plan §13 item 10, approved by the owner this turn.
  No game-behavior implementation: with no packet promoted that stays `BLOCKED`
  until one is promoted (§5).
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
- Decision guardrail: requested `grok` / `Grok 4.7` @ `xhigh`; **BLOCKED**. The
  active catalog registers **no `grok` provider**. Grok 4.7 is advertised only as
  `workbuddy-ai/grok-4.7` — "Grok-4.7 · x1.90", efforts low, medium, high, xhigh —
  which is a different provider id from the one §1 names, so the §1 route does not
  resolve. A spawn attempt on the §1 route exactly as written was rejected:
  `Error: child LLM route "grok/Grok 4.7" is not allowed for this Session`. No
  substitute id or route was used (owner instruction; §1 "Fallback routes: NONE
  AUTHORISED").
- Reviewer: requested `claude` / `claude-opus-5-5` @ `high`; returned route
  identity `claude` / `claude-opus-5-5` @ `high` (`maxTokens: 128000`) — **PASS**
  (`route: LIVE_RESOLVE`, a fresh child per packet review and one per turn for the
  turn-end review). Exactly one advertised route matches, and it offers `high`.
- Workers: `workbuddy-ai/deepseek-v4.1-flash` @ `max` (the §1 Worker row). No
  worker spawned this session.
- Exact error or ambiguity, if any: the Decision guardrail route above. §1 names
  `grok/Grok 4.7`; the catalog advertises `workbuddy-ai/grok-4.7`. Per the owner's
  instruction for this turn, the roster is not corrected here and no other id is
  substituted.

### Allow-list check

`just route-check` (`scripts/check-route-allowlist.py`, checker
`jsrf-route-allowlist/1`) — **exit 1**, 1 finding:

```text
checker jsrf-route-allowlist/1
  roster routes    : 3
    OK   claude/claude-opus-5-5
    MISS grok/Grok 4.7
    OK   workbuddy-ai/deepseek-v4.1-flash
  allow-list routes: 15
  fallbacks named  : none (owner-reserved; see the note)
  1 finding(s):
    [roster_route_not_allowed] the roster names grok/Grok 4.7, which the active allow-list does not contain; a route that is policy but not allowed cannot be selected when it is needed
```

Two of the three roster routes are covered. The active profile patch
(`C:\Users\logic\.dsh\profiles\web\cordis.patch.yml`) contains
`workbuddy-ai/grok-4.7`, not `grok/Grok 4.7`. This is consistent with the live
resolution above and is the same missing route, reported independently by a static
text check.

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

## Decision guardrail probe — BLOCKED

- Guardrail child ID: **none created**
- Route: `grok` / `Grok 4.7` @ `xhigh` (the §1 row) — not advertised by the catalog.
- Exact error or missing evidence: no `grok` provider is registered, so the §1
  route cannot be resolved and the child cannot be spawned. The spawn attempt
  returned `Error: child LLM route "grok/Grok 4.7" is not allowed for this Session`.
  Every §0.5 PASS requirement (child in the DSH child list, effective route equal to
  the §1 row, session-scoped workspace, a repository fact read with its own tool
  call, a second-turn marker) is therefore unmet, and the child-side
  `git rev-parse HEAD` comparison requested this turn could not be performed.
- Result: **BLOCKED**. Guardrail-dependent work is `BLOCKED` (§0). The advertised
  id was recorded and no substitute was used.

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
- State/plan disagreements and how they were escalated: none. Both trees clean and
  fast-forward after the pull.
- Overall disposition and next action: **BLOCKED**. The §0.5 Decision guardrail
  probe cannot run because the §1 route `grok/Grok 4.7` is not advertised, so
  guardrail-dependent work is `BLOCKED` and the guardrail side of the decision
  sandwich is unavailable this session. Advisor (§0.4) and Reviewer (§0.3) both
  PASS. No packet is promoted, so game-behavior implementation is `BLOCKED` until
  one is promoted (§5). The session's work this turn is the owner-approved §0.6
  discovery run recorded in `docs/reviews/owner-sega-600-observations.md` and plan
  §13 item 10.

Do not mark readiness PASS with a failed or unknown required item. A provider
catalog entry is not a completed invocation. A new advisor answering the
follow-up is not continuation. A readiness response is not acceptance of a
game packet.
