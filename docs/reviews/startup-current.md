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
  (`list_subagent_models codex` → `codex/gpt-6.1-sol`); child
  `4bfc8a87-a3c0-496c-b8f7-af2471eae6e4` spawned for this turn's review, route and
  effort verified from its delegation descriptor; continued through its re-reviews
  per §4.5.
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
- **Recorded limitation (owner-directed diagnosis, §0.6).** Muse reports native-shell
  denials. On four control commands (`git rev-parse HEAD`, `git show --stat HEAD`,
  `python -X utf8 scripts\check-agent-docs.py --check`, `Get-Location`) it reported
  `muse.powershell` as the tool and the identical text
  `tool denied: deny_unmatched: no policy rule allows this action`. It supplied no
  shell output for any of them, so no Muse output hash exists to compare and no
  output mismatch was demonstrated. Its `docs/jsrf-run-profiles.md:50` quote matches
  the repository byte-for-byte.
  Rejecting layer (**inferred**, per the Advisor ruling below — not observed or
  established): most likely Muse Code's own approval enforcement. Observed basis:
  plugin code explicitly sets `approvalMode: 'denyUnmatched'`
  (`src/muse/sdk-adapter.ts:305`); the denial token `deny_unmatched` is that same mode
  name in snake_case; the child's DSH record carries `sandbox/mode danger-full-access`
  and `approval/policy never`, both `source: delegation`; and the child returned a
  file quote matching the repository. Its README says Muse's native tools "are NOT
  mediated by DSH's tool registry" and that DSH-side policies "do not constrain what
  Muse itself can do". Uncertain: whether `muse.powershell` was invoked at all.
  The child's DSH transcript records no `tool/call` or `tool/result` events, but
  that absence is **not** cited as evidence either way: the plugin README states
  Muse tools "never appear as DSH tool calls, tool-role messages, or tool events",
  so the absence is expected by design.
  Consequence for the guardrail, bounded to what was tested: the four commands above
  did not return output, so a guardrail check that needs a command result must read
  the working tree or a written artifact instead.
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

Positive-control revision binding: all four commands were run at game revision
`ab85bb9ea84b9488c6949b6a4def75a118f2dbda` (the pre-turn `HEAD`). The receipt commit
below moved `HEAD`, so re-running `git rev-parse HEAD` / `git show --stat HEAD` now
yields different hashes for the same commands; that is the revision change, not a
changed measurement. The four hashes above reproduce at `ab85bb9`.

## Advisor rulings recorded this session

Advisor child `f60b72c9-f3b7-4aa0-a36b-5787ea9ea12c`, route
`claude/claude-opus-5-5` @ `xhigh` (from its `request/header`), continuability
verified this session. Consulted under §4.2 after the Turn reviewer's `CONTINUE`.
Per §3.3 the ruling text is recorded **verbatim** below; the Session's paraphrase
follows it and cites this block.

### Verbatim ruling text (Advisor response, 2026-10-03)

> Q1 RULING: You may report it only as INFERRED: "most likely Muse Code's own approval enforcement". Do not call it observed or established. The zero DSH tool/call events must not be cited either way: README.md:206-209 says Muse tools never appear as DSH tool events, so that absence is expected by design and is not evidence. Your existing wording at startup-current.md:103-111 is correctly labelled; keep it.
> Q1 BASIS: observed: sdk-adapter.ts:305 passes `denyUnmatched`, the denial token `deny_unmatched` is that same mode name in snake_case, the DSH record shows approval never / danger-full-access, and muse.read_file succeeded in the same child. inferred: that the Muse runtime, not the model, produced the denial text, because only the child's own reply carries it. uncertain: whether powershell was invoked at all.
> Q1 REVERSED BY: a Muse-side record of that session (host session log or turn items) showing a powershell tool call with a typed denial upgrades this to observed. A Muse-side record showing no call at all means the denial was confabulated, and the attribution must be withdrawn.
> Q2 RULING: The strongest claim you may make is: "the plugin hardcodes approvalMode (sdk-adapter.ts:305) and its Config schema (config.ts:206-261) has no approval key, so no plugin *configuration* setting can change the mode a session starts in". That is observed, for this checkout only. "The smallest sufficient change is a plugin code change" is NOT admissible and must stay unestablished. (a) Necessity is not shown. The denial names a missing *policy rule*, and muse-sdk.md:480 says the approval mode is "Select, never create — the value names a mode the host's configuration already defines". So a host-side allow rule under denyUnmatched has not been ruled out, and one rejected key (`settings.default_profile`) in one plane rules out nothing beyond that key. (b) Sufficiency and minimality are not shown. The closed union offers only allowAll (all shell and writes, not read-only), onRequest (prompts that nobody answers in a non-interactive run) and promptUnmatched, and none of them has been tested. M1-03:313-314 also records shell as never probed.
> Q2 BASIS: observed: the hardcoded value, the schema without an approval key, and the closed four-member union. The "no profile-selection field" negative counts as observed only for the schema version or fingerprint you read; state that scope when you report it. inferred: that a host-side rule layer exists, from the denial wording and from select-never-create. uncertain: whether any host config surface admits a per-tool or per-command allow rule.
> Q2 REVERSED BY: (1) Muse host docs or schema that list how rules are configured, plus a live test where adding a read-only powershell allow rule returns shell output, would make the answer a host config change, not code. (2) A full list of host config planes showing no rule surface, plus a live test with a patched plugin that returns shell output, would establish that a code change is both necessary and sufficient. Minimality would still need the narrowest mode that passes.

The Advisor also read `docs/recon/muse-sdk.md:480–500` and
`packets/M1-03-dsh-preset-smoke.md:307–314` in the plugin repository, and named its
rejecting-layer wording "most likely Muse Code's own approval enforcement".

### Session paraphrase (cites the block above)

- **Q1 — rejecting layer.** Only **inferred**: most likely Muse Code's own approval
  enforcement. The zero DSH `tool/call` events must **not** be cited either way.
- **Q2 — configuration change.** Strongest admissible claim: the plugin hardcodes
  `approvalMode` and its `Config` schema has no approval key, so no plugin
  *configuration* setting can change the mode a session starts in — observed, for this
  checkout only. "The smallest sufficient change is a plugin code change" is **not
  admissible** and stays unestablished.
- **BASIS:** observed — the hardcoded value, the schema without an approval key, the
  closed four-member mode union, and the child returning a file quote matching the
  repository. inferred — that the Muse runtime rather than the model produced the
  denial text, and that a host-side rule layer exists. uncertain — whether
  `muse.powershell` was invoked at all, and whether any host config surface admits a
  per-tool or per-command allow rule.

## Push record

- PUSHED_TO: `origin`
- BRANCH: `master`
- COMMIT: `abb854a3d648a7e8a6d9e20d1a4f83f80bf729a9`
- REMOTE_URL: `https://github.com/danillogical/poison-jam.git`
- RESULT: success (`ab85bb9..abb854a`, fast-forward; outgoing path list is
  `docs/reviews/startup-current.md` only; pre-commit passed including the
  no-`game/`-path and secret audit).
- Toolkit: **no push and no commit** — `main` at
  `929856fcfc145036252510509abaa0782d910a22`, clean, 0 ahead / 0 behind `origin/main`;
  `git pull --ff-only` reported `Already up to date.` The toolkit was pulled first,
  as required, and had nothing to send.

Corrections committed after the Turn reviewer's `CONTINUE` are recorded with their
own push tuple in the commit that carries them.

- PUSHED_TO: `origin` (correction commit)
- BRANCH: `master`
- COMMIT: `dc276392c14d6f564bceeea70e292951a816cbfd`
- REMOTE_URL: `https://github.com/danillogical/poison-jam.git`
- RESULT: success (`abb854a..dc27639`, fast-forward; outgoing path list is
  `docs/reviews/startup-current.md` only; pre-commit passed).

- PUSHED_TO: `origin` (review-outcome record commit)
- BRANCH: `master`
- COMMIT: `4fe3ab1b01754c4ccf4d139045f5db1bb24024ff`
- REMOTE_URL: `https://github.com/danillogical/poison-jam.git`
- RESULT: success (`dc27639..4fe3ab1`, fast-forward; outgoing path list is
  `docs/reviews/startup-current.md` only; pre-commit passed).

Turn-end review record for this turn (not a packet acceptance):

- First review: `TURN_END: CONTINUE` — four items: missing substantive Advisor
  consult, missing Muse post-check, missing push tuple, and a **freeze-first
  violation** (the Session kept investigating after spawning the reviewer, which
  §4.5 makes void). The first review is recorded as void on that ground.
- Repairs applied: Advisor consult obtained and recorded verbatim above; Muse chore
  post-check obtained (`MUSE_POST: CLEAR`, `BLOCKING: NONE`, `ANTI_VACUITY: NONE`,
  `INTEGRATION: NONE`); push tuple added; overstated claims narrowed.
- Re-review 1: `TURN_END: CONTINUE` on three items — no corrected frozen owner reply
  was supplied (the originally frozen draft was unchanged and still carried the
  defects); the third push tuple was missing; and the Advisor ruling was paraphrased
  rather than recorded verbatim (§3.3). It also corrected two receipt phrases that
  still asserted successful `muse.read_file` invocation, and one stale startup-status
  line as a non-blocking follow-up.
- Repairs applied for re-review 2: the corrected owner reply is
  `C:\Users\logic\AppData\Local\Temp\jsrf-turn-reply-corrected-2026-10-03.md`, which
  supersedes `jsrf-turn-draft-2026-10-03.md` (now void); the Advisor's complete ruling
  is recorded verbatim above; the third push tuple is recorded; the two invocation
  phrases and the stale Turn-reviewer line are corrected.
- Re-review 2: the same Turn reviewer child (`4bfc8a87-a3c0-496c-b8f7-af2471eae6e4`,
  `codex/gpt-6.1-sol` @ `high`) is continued with the corrected reply and this
  revision's diff, per §4.5 re-review rules.

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
