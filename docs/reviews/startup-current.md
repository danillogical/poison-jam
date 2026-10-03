# Fresh-session startup receipt

Filled per `docs/agent-workflow.md` §0 by `scripts/gen-startup-receipt.py`, then
completed from this session's own probe results. This is a receipt, not a second
policy: `docs/agent-workflow.md` owns the roster, startup procedure and failure
handling.

Generated: 2026-10-03T05:52:59.569286+00:00 (mechanical fields)
Probe fields filled: 2026-10-03 (this session)

## Identity and handoff

- Session ID/date/harness: 2026-10-03, DSH; session
  `session-162a2425-a048-4d82-995f-2934c653ec6b` (harness: `DSH_PROFILE=web`,
  `DSH_WEB_URL=http://127.0.0.1:3080`, `DSH_HOME=C:\Users\logic\.dsh`)
- Actual main model/effort: `workbuddy-ai/deepseek-v4.1-flash` @ `max` — the §1
  Session row. Read from harness metadata, not self-reported:
  `record.rows.modelSelection.val.lastUsed` =
  `{provider: workbuddy-ai, model: deepseek-v4.1-flash, reasoningEffort: max}` at
  `seq` 699 in
  `C:\Users\logic\.dsh\storages\session_projcache\sessions\session-162a2425-a048-4d82-995f-2934c653ec6b.json`.
  Caveat carried from prior receipts: `lastUsed` is the last selection, not a
  per-turn proof of the route that served any particular turn, and its `seq`
  drifts as the session runs; cite it with the `seq`.
- Workflow/plan/run-profile revisions and dirty diff identity: workflow
  `57bb7c13d5eb4525`, plan `697b3552bb12858d`, run profiles `4bb50f539ba4b7e8`;
  no dirty diff in either tree
- Game revision/status: `master` `371b06285c8d56d20f191af9a78c129bc1580948` (clean)
- Toolkit revision/status: `main` `929856fcfc145036252510509abaa0782d910a22` (clean)
- Unrelated edits preserved: 0 game, 0 toolkit
- CURRENT PACKET: **none**. Plan hash
  `697b3552bb12858d339e9b958d98e96bceab6567c6ef0d994f436d0920b58a30`; the block
  reads `## CURRENT PACKET — none`. No packet is promoted; the fail-fast observer
  stays PARKED by the owner (no retry). Packet-governed game-behavior
  implementation is therefore `BLOCKED` (§0.6).
- Dependencies and their recorded acceptance reviews: none (no packet promoted)
- Next exact authorized action: **owner-directed discovery work under §0.6**
  (records only, no fixes), as instructed in this session's prompt — find what
  should update the SEGA logo's armed fade and why it does not in the port; then
  record, commit and push (toolkit first) and stop. This is explicit
  owner-directed non-packet work and does not resume the paused broader goal.
- Build/run owner and worker write ownership: Session owns integration, the
  build/run slot and the record. This session ran one owner-directed read-only
  xemu oracle session (T3) and read-only archive analysis; no implementation
  worker, no rebuild, no port run.

## Route resolution — PASS / BLOCKED

Resolved live in this session by the Session with `list_subagent_models`, not
inferred from a display name.

- Session: `workbuddy-ai/deepseek-v4.1-flash` @ `max` — matches §1 (harness metadata above)
- Planner: `claude/claude-opus-5-5` @ `high` — exactly one advertised route
  (`claude/claude-opus-5-5`), effort `high` advertised. `LIVE_RESOLVE`. **Not
  spawned this session** (no packet, no planning call required).
- Persistent advisor: requested `claude` / `claude-opus-5-5` @ `xhigh`,
  `route: CONTINUABLE_PINNED`. Exactly one advertised route; `xhigh` advertised.
  Probed and **PASS** (below).
- Muse guardrail: `subagent_muse` @ `max` (`muse-code` / Muse Spark 1.3, one
  session-continuable child). Probed and **PASS** (below).
- Packet reviewer: `claude/claude-opus-5-5` @ `medium`. Exactly one advertised
  route; `medium` advertised. Probed and **PASS** (below). No packet review was
  required this session.
- Turn reviewer: `codex/gpt-6.1-sol` @ `high`. Exactly one advertised route
  matching the named provider/model; `high` advertised. No startup probe is
  required (§0.3); verified on first use at this turn's end.
- Workers: `workbuddy-ai/deepseek-v4.1-flash` @ `max` (the §1 Worker row). No
  worker child spawned this session.
- Exact error or ambiguity, if any: none. All five required routes resolved to
  exactly one advertised route each; no row was zero-match or multi-match.

## Packet reviewer probe — PASS

- Child ID: `e5082010-2749-4751-87fc-dcd67be92c85`; route
  `claude/claude-opus-5-5` @ `medium`
- Fresh token: `82807b2fb8b3420d`
- Empty-evidence answer: "Acceptance needs positive, checkable proof that a
  criterion holds. If there is no evidence, nothing has been measured, so the
  criterion is unverified (CANNOT VERIFY / UNKNOWN), not passed. Passing it by
  default would invent a result." Grounded by the child itself in
  `docs/agent-workflow.md:278-280` and
  `docs/reviews/review-record.schema.json:124`.
- Command run by the child itself:
  `git -C C:\Users\logic\Repos\my_xbox_game rev-parse HEAD`
- Output: `371b06285c8d56d20f191af9a78c129bc1580948`
- Output sha256 (child's own computation):
  `2624fd0be03b5e3c509bf47ef5fdfc75e7176a1a0e50afccaea46e0191cb2e05`
- **Independently reproduced by the Session** on the same command and tree: the
  same hash `2624fd0b…2e05`. This is a reproduced measurement, not a brief.
- Effort: the child reported `EFFORT_CONFIRMED: UNKNOWN` (it cannot read its own
  effort). Effort is instead established by the Session's spawn at `medium`,
  which is the §1 row, and by the advertised effort list for the single resolved
  route.
- Result: **PASS** (fresh token + reason + self-run read-only command with a
  hash the Session reproduced).
- Exact error or missing evidence: none. The probe child is discarded and was not
  reused.

## Muse guardrail probe — PASS

- Muse child ID: `bce6d52d-1dbc-42ab-a8e1-72a69cfd620e`; route `subagent_muse`
  @ `max` (`muse-code` / Muse Spark 1.3)
- Workspace: `C:\Users\logic\Repos\my_xbox_game` — the requesting session's cwd,
  reported by the child from its own environment
- Named fact read with Muse-native tools: `plan-jsrf-bare-minimum.md:56-57` —
  `## CURRENT PACKET — none` (and the following line is blank)
- Turn-1 marker: `MUSE-PKT7Q2X9K`
- Turn 2 (same child, marker not repeated in the request): returned
  `MARKER: MUSE-PKT7Q2X9K` and `CONTINUATION: OK`
- Result: **PASS** (real DSH child, listed, correct workspace, named fact read,
  second message reached the same child and used the turn-1-only marker)
- Exact error or missing evidence: none. This child is retained for routine
  packet preflight/post-execution guardrail checks during this top-level session.

## Persistent advisor probe — PASS

- Child ID: `c548070d-9cc1-4340-b3b0-d56d88e2f0cc`; route
  `claude/claude-opus-5-5`, provider `claude`, effort `xhigh`
- Tool surface: `subagent` with `provider`/`model`/`reasoning_effort` selection
  (a continuable DSH child path)
- Continuability probe: the child appears in the continuable-agent listing, and a
  later `send_message` reached **the same child**
- Turn 1 reference; unique marker given: `MARKER: QV7-ottermandrel-4K19`
- Named file and the fact deliberately omitted from the brief: the child read
  `plan-jsrf-bare-minimum.md` §13 "Current state", lines 595-622, and reported
  (a) the SEGA logo update VA `0x7E360` and (b) the fade done-flag reader
  `0x24650` (which reads `+0xC0` of the subsystem-6 fade object). Neither value
  was given in the brief.
- Advisor's answer checked against the file by the Session: both values match the
  plan text the child cited. **Confirmed.**
- Turn 2 reference (same child, marker not repeated): returned
  `MARKER: QV7-ottermandrel-4K19` and `CONTINUATION: OK`
- Result: **PASS** (exact provider/model, listed effort, listing visibility,
  continuation, first-turn file read with an omitted fact, second-turn marker)
- Exact error or missing evidence: none. Retained for later rulings; it answered
  one quick consult this session (recorded in
  `docs/reviews/owner-sega-600-observations.md`).

## Packet readiness — BLOCKED (no packet promoted)

- Frozen revision/hash matches `CURRENT PACKET`: N/A — `CURRENT PACKET — none`
- Adequacy review record and verdict: N/A — no packet promoted
- Deferred advisories (recorded, not acted on): none new this session
- Prerequisites / tooling checks:
  - just: just 1.58.0 (C:\Users\logic\AppData\Local\Microsoft\WinGet\Packages\Casey.Just_Microsoft.Winget.Source_8wekyb3d8bbwe\just.EXE)
  - pre-commit: pre-commit 4.6.2 (C:\Users\logic\AppData\Roaming\Python\Python313\Scripts\pre-commit.EXE)
  - clang-cl: clang version 23.1.2 (https://github.com/llvm/llvm-project 85ac560262434c9ccfc0c183ec22d4138ed647fb) (C:\Program Files\LLVM\bin\clang-cl.EXE)
  - ttd: Microsoft (R) TTD 1.01.11 x64 (C:\Users\logic\AppData\Local\Microsoft\WindowsApps\ttd.EXE)
  - duckdb: 1.5.6 (not on PATH)
  - python: 3.13.2 (C:\Python313\python.exe)
  - free space: 217.29 GB (above 15.0 GB floor)
  - xemu: `C:\Users\logic\Downloads\xemu\xemu.exe`, configured assets resolved in
    place and present (bootrom, flashrom, eeprom, hdd, dvd)
- State/plan disagreements and how they were escalated: none. The plan's
  `CURRENT PACKET — none` matches the observed tree.
- Overall disposition and next action: **all required startup items PASS**, but
  packet-governed game-behavior implementation is **BLOCKED** because no packet is
  promoted. The work executed this session is explicit **owner-directed
  non-packet discovery under §0.6** (records only, no fixes), and does not resume
  the paused broader goal.

Do not mark readiness PASS with a failed or unknown required item. A provider
catalog entry is not a completed invocation. A new advisor answering the
follow-up is not continuation. A readiness response is not acceptance of a
game packet.
