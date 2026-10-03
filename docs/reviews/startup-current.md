# Fresh-session startup receipt

Filled per `docs/agent-workflow.md` §0.6 by `scripts/gen-startup-receipt.py` and then
completed from this session's own probes; this is a receipt, not a second policy.
Fields marked **UNVERIFIED** cannot be measured by a generator and must be
filled from the probe that establishes them.

Generated: 2026-10-03T16:11:20.000343+00:00

## Identity and handoff

- Session ID/date/harness: 2026-10-03, DSH; session
  `session-d4f66947-86a8-42c5-9d69-a04353db2f64`
  (harness: `DSH_PROFILE=web`, `DSH_WEB_URL=http://127.0.0.1:3080`)
- Actual main model/effort: `workbuddy-ai/deepseek-v4.1-flash` @ `max` — the §1
  Session row. Verified from this session's own `request/header` record in
  `C:\Users\logic\.dsh\sessions\--C-Users-logic-Repos-my_xbox_game--\session-d4f66947-86a8-42c5-9d69-a04353db2f64\`,
  not from a display name.
- Workflow/plan/run-profile revisions and dirty diff identity: workflow `2de284987ce0fa6f`, plan `cd3c8706e0113fec`, run profiles `4bb50f539ba4b7e8`
- Game revision/status: `master` `ab85bb9ea84b9488c6949b6a4def75a118f2dbda` (clean)
- Toolkit revision/status: `main` `929856fcfc145036252510509abaa0782d910a22` (clean)
- Unrelated edits preserved: 0 game, 0 toolkit. Toolkit pulled first
  (`Already up to date.`), then game (`c0294db..ab85bb9`, fast-forward,
  `docs/agent-workflow.md` + `scripts/gen-startup-receipt.py`).
- CURRENT PACKET: **NONE** — `## CURRENT PACKET — none` (`plan-jsrf-bare-minimum.md:56`)
  - plan hash `cd3c8706e0113fececdf9d4108d0d826735908781b5c44da4c29f8bbf28011c6`
  - block: "No packet is promoted. **The fail-fast observer attempt is PARKED by the owner — STOP UNKNOWN, no
retry, no further correction, consultation or list extension.**" The plan also states
    "The first packet is C1 (§7); it is promoted here, by exact revision and hash, only
    after its adequacy review returns `ADEQUATE`" (`plan-jsrf-bare-minimum.md:68`).
- Dependencies and their recorded acceptance reviews: none (no packet promoted)
- Next exact authorized action: **owner-directed environment diagnosis under §0.6**
  (Muse guardrail shell-denial question). No game-behavior implementation: with no
  packet promoted, that remains `BLOCKED` until one is promoted (§5).
- Build/run owner and worker write ownership: Session owns integration, the
  build/run, evidence collection and record keeping (§2.2). No worker was spawned
  this session.

## Route resolution — PASS / BLOCKED

Resolved live in this session by the Session, not by this generator. Every child
route below was read from that child's own `request/header` record under
`C:\Users\logic\.dsh\sessions\--C-Users-logic-Repos-my_xbox_game--\<child-id>\`, which
is harness metadata rather than a self-report.

- Planner: `claude/claude-opus-5-5` @ `high` — exactly one advertised route
  (`list_subagent_models claude` → `claude/claude-opus-5-5`); `route: LIVE_RESOLVE`,
  fresh child per packet. Not spawned this session (no packet to plan).
- Persistent advisor: requested `claude` / `claude-opus-5-5` @ `xhigh`; returned
  route identity `claude` / `claude-opus-5-5` @ `xhigh` (`maxTokens: 128000`) —
  **PASS**, `route: CONTINUABLE_PINNED`.
- Muse guardrail: `subagent_muse` @ `max`; returned route identity
  `muse-code` / `muse-spark-1.3-contributor` @ `max` — **PASS**
  (`subagent_muse` @ max, one session-continuable child).
- Packet reviewer: `claude/claude-opus-5-5` @ `medium`. Exactly one advertised
  route; returned route identity `claude` / `claude-opus-5-5` @ `medium` —
  **PASS** (`route: LIVE_RESOLVE`, fresh child per packet review).
- Turn reviewer: `codex/gpt-6.1-sol` @ `high`. Exactly one advertised route
  (`list_subagent_models codex` → `codex/gpt-6.1-sol`) — resolved, but **not yet
  spawned**; verified on first use per §1.
- Workers: `workbuddy-ai/deepseek-v4.1-flash` @ `max` (the §1 Worker row). No
  worker spawned this session.
- Exact error or ambiguity, if any: none. All four §1 routes resolved to exactly
  one advertised entry.

## Packet reviewer probe — PASS

- Child ID: `961cefb8-65e4-4e17-b491-a400a01b26a7`
- Fresh token: `PKTREV-7c3e91a4`
- Empty-evidence answer: cited §2.4 invariant 3 ("Failure stays failure") and
  invariant 5 ("Absence needs coverage"), reading `docs/agent-workflow.md`
  lines 619–673 directly.
- Command and output hash: `git rev-parse HEAD` →
  `c7bc15869160e27f7548a92094bd3f5f0bc8a31c7cda619b95a4ed46a966661c`
- Effort/route: `claude/claude-opus-5-5` @ `medium` (from its `request/header`).
- Result: **PASS**. Its independently computed hash matches the Session's own
  positive control for the same command exactly (see "Positive control" below).
- Exact error or missing evidence: none. The probe child is discarded.

## Muse guardrail probe — PASS (with a recorded shell limitation)

- Muse child ID: `284f6218-1859-4de2-b440-2fdd16472fa7`
- Route/effort: `muse-code` / `muse-spark-1.3-contributor` @ `max` (from its
  `request/header`)
- Workspace: `C:\Users\logic\Repos\my_xbox_game` (session-scoped resolution;
  `workspaceMode: session`)
- Named fact read: `docs/jsrf-run-profiles.md:50` — the `RECOMP_GPU_ACK` row. Its
  quote is byte-exact against the file.
- Turn-2 marker: `MUSE-GUARDRAIL-PROBE-7F3A91C4`, returned verbatim by the same
  child on continuation.
- Result: **PASS** for the §0.5 requirements.
- **Recorded limitation (owner-directed diagnosis, §0.6).** The Muse child can read
  files with `muse.read_file` / `muse.search`, but every `muse.powershell` call is
  refused with `tool denied: deny_unmatched: no policy rule allows this action`.
  This is **not** a DSH restriction: the child's DSH record carries
  `sandbox/mode danger-full-access` and `approval/policy never`, both
  `source: delegation`, and the plugin hardcodes Muse's own
  `approvalMode: 'denyUnmatched'` at session creation. Until that changes, Muse
  cannot run `git diff` / `git show` for a guardrail check and must read the
  working tree and any written artifact instead.
- Exact error or missing evidence: none for §0.5.

## Persistent advisor probe — PASS

- Child ID: `f60b72c9-f3b7-4aa0-a36b-5787ea9ea12c`
- Turn 1 reference; unique marker given: `ADVISOR-PROBE-4B2E77D0`
- Named file and the fact deliberately omitted from the brief: read
  `docs/jsrf-run-profiles.md` and reported the strict-run variable
  (`RECOMP_GPU_ACK`, must be present as exact string `0`) from `:18`, corroborated
  at `:50`. It additionally reported `plan-jsrf-bare-minimum.md:58` ("No packet is
  promoted.") and `:68` (first packet `C1`), neither of which was in the brief.
- Advisor's answer; checked against the file: both quoted lines verified
  byte-exact against the files by the Session.
- Turn 2 reference (same child, marker not repeated); returned marker:
  `ADVISOR-PROBE-4B2E77D0`, plus `C1` recalled from turn 1.
- Route/effort: `claude` / `claude-opus-5-5` @ `xhigh` (from its `request/header`).
- Result: **PASS** — continuable, exact §1 route, listed effort, listing
  visibility, and a continuation that reaches the same child.
- Exact error or missing evidence: none. The Advisor self-reported that it could
  not verify its own effort from inside the child; the Session confirmed `xhigh`
  from harness metadata, as §1 requires.

## Positive control — Session's own measurements

Run by the Session with `pwsh` from `C:\Users\logic\Repos\my_xbox_game`. SHA-256 is
over stdout with CRLF normalized to LF and trailing newline(s) stripped, UTF-8
encoded; an added exit-code line is **not** part of any hash.

| # | Command | Bytes | SHA-256 |
|---|---|---|---|
| a | `git rev-parse HEAD` | 40 | `c7bc15869160e27f7548a92094bd3f5f0bc8a31c7cda619b95a4ed46a966661c` |
| b | `git show --stat HEAD` | 2240 | `7c4a5b87e714ad274a7df41152518a3bb5ff70d62f8ee24d77713756ccbad216` |
| c | `python -X utf8 scripts\check-agent-docs.py --check` | 98 | `5de425e5d160bcfbf36da1fbb966d7a070f8d52b483243151e6ba0e2cff50d37` |
| d | `Get-Location` | 102 | `bf43c6335762965b7bffebef51f061b5fe230e768dd3e62da301287d08a9a1dc` |

`check-agent-docs.py --check` exited 0 (`checker jsrf-agent-docs/3`; `AGENTS.md`
20312 bytes, budget 65536; `no findings`).

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
  - free space: 214.07 GB (above 15.0 GB floor)
- State/plan disagreements and how they were escalated: none. Both trees clean and
  fast-forward after the pull.
- Overall disposition and next action: **all required startup items PASS**. §0.5
  Muse guardrail and §0.4 Advisor both PASS with recorded evidence; §0.3 Packet
  reviewer PASS. No packet is promoted, so game-behavior implementation is
  `BLOCKED` until one is promoted (§5). The session's work this turn is the
  owner-directed §0.6 environment diagnosis recorded above and in the turn reply.

Do not mark readiness PASS with a failed or unknown required item. A provider
catalog entry is not a completed invocation. A new advisor answering the
follow-up is not continuation. A readiness response is not acceptance of a
game packet.
