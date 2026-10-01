# Fresh-session startup receipt

Filled per `docs/agent-workflow.md` §0.6 by `scripts/gen-startup-receipt.py`; this is a receipt, not a second policy.
Fields marked **UNVERIFIED** cannot be measured by a generator and must be
filled from the probe that establishes them.

Generated: 2026-10-01T08:36:03.024342+00:00

## Identity and handoff

- Session ID/date/harness: 2026-10-01, DSH; session `eaacd84e-a3f6-4ee2-b4ac-c12d02fa6de6` started 00:42:52-07:00
  (harness: `DSH_PROFILE=web`, `DSH_WEB_URL=http://127.0.0.1:3080`)
- Actual main model/effort: `codex/gpt-6.1-sol` @ `medium` — the §1 Session row.
  Verified from harness metadata, not self-reported: `record.rows.modelSelection.val.lastUsed`
  = `{provider: codex, model: gpt-6.1-sol, reasoningEffort: medium}` in
  `C:\Users\logic\.dsh\storages\session_projcache\sessions\session-eaacd84e-a3f6-4ee2-b4ac-c12d02fa6de6.json`
  - **Caveat on this field.** `modelSelection.lastUsed` is the **last selection**, not a
    per-turn proof of the route that served any particular turn, and it carries a monotonic
    `seq` that drifts as the session runs (observed `seq` 143 at the time of the Session's
    own direct read). Cite the field *with its `seq`*.
  - A worker read of this file initially reported the parent as absent. The Advisor inferred a
    lookup-key error; the worker's re-read with the corrected filename confirms the file exists
    and carries the value above.
- Workflow/plan/run-profile revisions and dirty diff identity: workflow `f5cd19d831a4ecb2`, plan `f0b9b8f89efd807d`, run profiles `775757cdea36bb2f`
- Game revision/status: `master` `2a7324b9ce47b23ff5df57dd40dfa91fb5b5378a` (clean)
- Toolkit revision/status: `main` `1f9309a8d15679f1b42d90cb9f03a17fa242ef8f` (clean)
- Unrelated edits preserved: 0 game, 0 toolkit
- CURRENT PACKET: present
  - plan hash `f0b9b8f89efd807d9bf028fe2c9fe69e638cf7f245bc8cf392e0473390fcc846`
  - block: No packet is promoted. Phase 0 (§4) and the chores of §5–§6 run as owner-directed chores, which
need no packet. The first packet is C1 (§7); it is promoted here, by exact revision and hash, only
after its adequacy review returns `ADEQUATE` (`docs/agent-workflow.md` §5).
- Dependencies and their recorded acceptance reviews: none (no packet promoted)
- Next exact authorized action: F4 (plan §13) — take the `sink_capacity` design question to the
  Persistent Advisor, implement the ruling in the toolkit with a focused test, validate, then one
  bounded smoke run asking whether GET advances past `0x8EF0`
- Build/run owner and worker write ownership: Session owns integration, the build/run slot and the
  record. This session's worker ran read-only discovery (F4 sink brief, `logs/workers/`) with an
  extended write scope covering this receipt only. Implementation workers receive the serial
  build/run slot when one is assigned; no two workers hold it at once.

## Route resolution — PASS / BLOCKED

Resolved live in this session by the Session, not by this generator:

- Planner: route `claude/claude-opus-5-5` @ `high`, `route: LIVE_RESOLVE`, fresh child per packet
  (§1). Not probed at startup — §0 says the Planner needs no separate probe. No Planner call was
  made this session; no packet reached planning.
- Persistent advisor: requested `claude` / `claude-opus-5-5` @ `high`, `route: CONTINUABLE_PINNED`.
  Resolved live: exactly one advertised route, `claude/claude-opus-5-5`. **PASS** — see the probe
  section below.
- Reviewer: requested `claude` / `claude-opus-5-5` @ `medium`, `route: LIVE_RESOLVE`, fresh child per
  review. Resolved live to exactly one advertised route and probed **PASS** — see the probe section.
- Workers: `workbuddy-ai/deepseek-v4.1-flash` @ `max` (the §1 Worker row). One worker child spawned
  this session (F4 sink brief assembly, read-only).
- Exact error or ambiguity, if any:

## Reviewer probe — PASS / FAIL / UNKNOWN

- Child ID; fresh token; response reference; empty-evidence answer; command and output hash; effort; result:
  - Child ID: `92a1954b-baf4-4089-99b2-4e51caf1bd00` — route `claude/claude-opus-5-5`, provider
    `claude`, effort `medium`, spawned continuable (§0 step 3 requires a fresh child per review).
  - Fresh token: `f1aa16de-46ea-45ba-b7c9-cc14517c915c`.
  - Named read-only command and output hash: `git rev-parse HEAD` (trimmed output), sha256
    `ade1ab078a145da3ca471b6ce93f414d8a61618ee104f07351bcc08ab9217d6e`.
    **Independently reproduced** by this worker: `git rev-parse HEAD` in the game repository returns
    `2a7324b9ce47b23ff5df57dd40dfa91fb5b5378a`, whose UTF-8 sha256 (no trailing newline) is exactly
    that digest. The trimmed-vs-raw newline matters: the same string with a trailing LF hashes to
    `eb47682685229b88d935fa5dcb70fcd92f30d2e411030087c396926068987eea`, so the digest is reproducible
    only against the trimmed form — recorded because it is what makes the hash checkable.
  - Effort; result: `medium`; **PASS**.
  - Empty-evidence answer, verbatim: *"with no evidence, there’s no way to tell the claim holds
    apart from nothing was run or captured. Accepting it would let something never checked pass as
    verified. A pass has to come from evidence the Reviewer can reproduce, not absence of
    failures."*
- Exact error or missing evidence: none — every §0 step 3 element is present above (fresh child,
  fresh token, the empty-evidence answer, a named read-only command and its output hash, effort),
  and the hash was independently reproduced here.

## Persistent advisor probe — PASS / FAIL / UNKNOWN

- Child ID: `4e6d87e1-f748-48b3-a0a4-a6e5728bfeee` — route `claude/claude-opus-5-5`, provider
  `claude`, effort `high`, spawned continuable.
  Verified from harness metadata, not self-reported:
  `record.rows.modelSelection.val.lastUsed` = `{provider: claude, model: claude-opus-5-5,
  reasoningEffort: high}` in
  `C:\Users\logic\.dsh\storages\session_projcache\sessions\4e6d87e1-f748-48b3-a0a4-a6e5728bfeee.json`;
  mode `continuable` per the Session's `subagentCatalog`.
- Turn 1 reference; unique marker given: startup turn 1; marker `ADVISOR-eaacd84e-7391`, supplied
  **only** in turn 1.
- Named file and the fact deliberately omitted from the brief: `docs/session-start-template.md`
  **line 3** — it instructs the reader to "Copy this template to
  `docs/reviews/startup-<date>-<session-id>.md`", which conflicts with `docs/agent-workflow.md`
  §0.6 (`:82-93`), which requires the **rolling** `docs/reviews/startup-current.md`.
- Advisor's answer; checked against the file: the Advisor reported that conflict and ruled that the
  **rolling** receipt controls. Checked directly here: both files were re-read and the conflict is
  exactly as reported — `docs/session-start-template.md:3` does specify the dated form, and
  `docs/agent-workflow.md:82-93` does require the rolling one. **Correct.**
- Turn 2 reference (same child, marker not repeated); returned marker: `ADVISOR-eaacd84e-7391`,
  returned by the same child with the marker not repeated in turn 2.
- Result: **PASS** — all six §0 step 4 predicates hold together: exact §1 model/provider (`claude` /
  `claude-opus-5-5`), listed effort (`high`), present in the continuable-agent listing, reachable by
  continuation, turn 1 read the named file and reported the omitted fact, and turn 2 returned the
  turn-1 marker to the **same** child.
  **Advisor metadata ruling (recorded 2026-10-01).** The Advisor (child
  `4e6d87e1-f748-48b3-a0a4-a6e5728bfeee`, `claude/claude-opus-5-5` @ `high`, continuity verified)
  ruled that no further route check is needed and the current handling stands: observed — the parent
  session file exists and carries `modelSelection.lastUsed` = `codex`/`gpt-6.1-sol` @ `medium`;
  observed — a child's `DSH_SESSION_ID` is the child's own id. Inferred — the worker's "parent
  absent" report was a lookup-key error. **Reversed by:** the parent's `lastUsed` disagreeing with
  the requested Session route (`codex`/`gpt-6.1-sol` @ `medium`), or evidence that a child's
  environment carries the actual parent session id.
- Exact error or missing evidence: none — route, effort, continuability, continuation, the named
  file and the omitted fact are all recorded above and checked.

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
  - free space: 229.36 GB (above 15.0 GB floor)
- State/plan disagreements and how they were escalated: none. No packet is promoted, so no
  frozen-revision mismatch is possible. F4 runs as an owner-directed chore under §0.5.
- Overall disposition and next action: **PASS** for every required startup item.
  Session route verified from harness metadata (`codex/gpt-6.1-sol` @ `medium`, §1 row); the
  Persistent Advisor route **PASS** (exact §1 model/provider, `high`, continuable, continued);
  the Reviewer probe **PASS** (fresh token, effort `medium`, and its `git rev-parse HEAD` output
  hash independently reproduced here). Packet implementation remains **BLOCKED** in the §0.5 sense
  only — no packet is promoted — while the plan's §13 owner-directed chores (F4) continue.
  Next: F4 — the `sink_capacity` design question goes to the Persistent Advisor, then the ruling is
  implemented in the toolkit with a focused test, validated, and measured with one bounded smoke run.

**Receipt form (Advisor fact template vs workflow §0.6).** The Advisor's fact template line 3
describes a *dated* receipt (`startup-<date>-<session-id>.md`); `docs/agent-workflow.md` §0.6
requires a **rolling** `docs/reviews/startup-current.md` that replaces the previous session's
receipt. **Workflow §0.6 controls** — this file is that rolling receipt, and the generator's
default output is the same path. Recorded because the two spellings disagree and a future session
would otherwise create an unbounded family of dated receipts.

Do not mark readiness PASS with a failed or unknown required item. A provider
catalog entry is not a completed invocation. A new advisor answering the
follow-up is not continuation. A readiness response is not acceptance of a
game packet.
