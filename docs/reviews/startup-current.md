# Fresh-session startup receipt

Owner instruction: startup + light role smoke test + Phase 0 rebaseline (2026-09-29).
Filled per `docs/agent-workflow.md` §0.6; this is a receipt, not a second policy.

## Identity and handoff

- Session ID/date/harness: `session-f5018f2e-ee33-4185-89cb-34c4cf570598`, 2026-09-29, DSH (`DSH_PROFILE=web`, `DSH_WEB_URL=http://127.0.0.1:3080`)
- Actual main model/effort (metadata evidence, or UNKNOWN): `workbuddy-ai/deepseek-v4.1-flash` @ `max` (harness metadata: `agent-default-model` in the active profile patch; the Session row of workflow §1). Not independently verifiable from inside the session — recorded from configuration, not from a probe.
- Workflow/plan/run-profile revisions and dirty diff identity: all three read at the revisions below; no dirty diff (`git status --porcelain` empty in both trees)
- Game revision/status; toolkit revision/status: game `master` `5776aab` clean; toolkit `main` `2a349c8` clean
- Unrelated edits preserved: none present at startup (both trees clean); no unrelated edit was created
- CURRENT PACKET copied from plan (packet + exact revision + SHA-256), or NONE: **NONE** (`plan-jsrf-bare-minimum.md` §"CURRENT PACKET — none": "No packet is promoted. Phase 0 (§4) and the chores of §5–§6 run as owner-directed chores, which need no packet.")
- Dependencies and their recorded acceptance reviews: none (no packet promoted)
- Next exact authorized action: Phase 0 §4 V1→V5 in order, then stop
- Build/run owner and worker write ownership: Session executes V1–V4; one DeepSeek worker performs the V5 read-only audit (scratch output under gitignored `logs/`)

## Route resolution — PASS / BLOCKED

- Planner: requested Muse Spark 1.3 @ high (`skill: muse-worker`, fresh handle); returned `modelId: muse-spark-1.3-contributor`, `reasoningEffort: high` — **PASS**
- Persistent advisor: requested `claude/claude-opus-5-5` @ high via a continuable path; **BLOCKED** — see below
- Acceptance reviewer: requested `codex/gpt-6.1-sol` @ high; route and command reproduced, effort not exposable on the only available path — **PARTIAL**
- Workers: requested `workbuddy-ai/deepseek-v4.1-flash` @ max (session default, not per-child pinned); command executed and correct result returned — **PASS**
- Exact error or ambiguity, if any: see the Advisor section

### Persistent advisor — BLOCKED (root cause found and repaired for the next session)

Every available spawn surface refused the pinned route:

- `subagent` with `provider: claude`, `model: claude-opus-5-5`, `reasoning_effort: high`
  → `Error: child model selection is disabled for this tool instance`
- `subagent_fork` with the same pin → identical error
- `workflow` → `agent()` is **forbidden** for the Advisor by workflow §1 while it delegates through
  one-shot `subagents.start()` (`dsh-workflow-ptc/lib/index.js:328`), so it was not used for the Advisor.

`dsh-tool-subagent/lib/index.js:64` raises that error when
`requestedAgentOptions(..., enabled)` receives `enabled === false`. `enabled` is fixed when the
tool is installed for a session (`selectForSession`, `lib/index.js:588-605`) from
`subagentModelSelectionPolicy`; a session with no recorded policy stays disabled, and a live
profile-patch reload does **not** re-sample it (verified: the error persisted after the edit below).

**Root cause of the missing policy.** There is no live `~/.dsh/settings.yaml`; DSH migrated it
(`dsh-settings/lib/index.js:346-363`, `importLegacyDocument`) and renamed it
`settings.yaml.imported`. The migration **silently dropped** the `subagent-model-selection`
section: its `allowedModels` listed `codex/gpt-6-sol` **twice**, and
`assertAllowedModelRoutes` rejects duplicate routes (`model-selection-settings.js:34`), so the
whole section was rejected, logged as a warning, and never imported. Reproduced mechanically:

```
OLD settings.yaml allowedModels -> REJECT duplicate route: codex/gpt-6-sol
NEW cordis.patch.yml allowedModels -> OK 14 unique routes | enabled = True
```

**Repair applied** (owner-authorized: "use the configured DSH model-selection mechanism/allowedModels
needed to expose the exact Claude route") — `C:\Users\logic\.dsh\profiles\web\cordis.patch.yml`,
outside both repositories and therefore not committed to either. It now declares
`subagent-model-selection-settings` with `enabled: true` and 14 unique routes including
`claude/claude-opus-5-5`, `codex/gpt-6.1-sol` and `workbuddy-ai/deepseek-v4.1-flash`. The edit
validates as YAML and passes the same duplicate check. It takes effect for a session composed
after the edit; it cannot retroactively enable selection in this one.

Per workflow §0.4 this makes the Advisor route **BLOCKED** for accepted game work in this session:
no available tool simultaneously proves continuability + the exact Claude route + high effort, and
the instruction is explicit that a one-shot Claude response is FAIL, not "close enough". No
substitute model was used and no property was silently traded away.

Consequence, per the owner instruction: **accepted-packet work is BLOCKED**; this session's
owner-directed Phase 0 chore work requires zero senior technical calls and may continue. C1,
planning, adequacy and acceptance work must not start.

## Acceptance reviewer probe — PASS / FAIL / UNKNOWN

One review stage (workflow §1); probed on a fresh child that ran one read-only command.

- Child ID; fresh token; response reference; empty-evidence answer; command and output hash; effort; result:
  `workflow` run `reviewer-smoke`, one agent, `provider: codex`, `model: gpt-6.1-sol`. It ran
  `git rev-parse --short HEAD` in the game repo and returned `5776aab`, which matches the
  independently measured HEAD. Empty-evidence answer: "An empty evidence set cannot satisfy a
  mandatory acceptance criterion because it provides no verifiable support that the required
  condition was met." **Effort: UNKNOWN — not exposable.** `workflow`'s `agent()` rejects the
  option outright: `agent() option "reasoningEffort" is not recognized (supported: label, phase,
  schema, provider, model)`. So `high` could be neither requested nor verified.
- Route evidence (not the child's self-report): a three-way control on the same path resolved
  `codex/gpt-6.1-sol` → `"OK"`, while a bogus provider and a valid provider with a bogus model
  each returned `null` (failed). The route is therefore genuinely honoured, not silently defaulted.
- Exact error or missing evidence: effort clause unsatisfied; same root cause as the Advisor
  (model selection disabled at session composition). The reviewer role is not continuable by
  design, so this is an effort-pinning gap, not a continuability gap.

## Persistent advisor probe — PASS / FAIL / UNKNOWN

- Child ID: none created (route BLOCKED before spawn)
- Turn 1 reference; unique marker given: not reached; marker `ADVISOR-SMOKE-629` was never delivered
- Named file and the fact deliberately omitted from the brief: not reached
- Advisor's answer; checked against the file: not reached
- Turn 2 reference (same child, marker not repeated); returned marker: not reached
- Result: **FAIL** (no child; continuation untested — `ADVISOR CONTINUATION: FAIL`)
- Exact error or missing evidence: `child model selection is disabled for this tool instance`

## Packet readiness — PASS / BLOCKED / UNKNOWN

- Frozen revision/hash matches `CURRENT PACKET`: N/A — `CURRENT PACKET` is `none`
- Adequacy review record and verdict: N/A — no packet
- Deferred advisories (recorded, not acted on): none
- Prerequisites / tooling checks: free-space gate PASS (69.4 GB free on `C:`, floor 50 GB)
- State/plan disagreements and how they were escalated: **Phase 0 V1–V4 were already executed and
  committed** at this exact revision pair by the preceding session (game `44becd4`, `56372dc`), and
  are recorded in `docs/jsrf-technical-record.md` §2/§5/§7 and
  `docs/reviews/strict-horizon-ledger.md`. The owner instruction directs Phase 0 "exactly in
  order"; re-running V2/V3 would rewrite a committed generated tree and spend ~10 min of runs to
  re-derive values already on disk. This session therefore **re-verified** the recorded V1–V4
  results mechanically against the archived artifacts and the current tree (see the report), and
  executed the genuinely outstanding step V5. No value was accepted from the record without
  independent re-measurement. Escalation: this is a reading of an owner instruction, not a
  technical ruling; it is reported explicitly as such rather than resolved silently.
- Overall disposition and next action: accepted-packet work BLOCKED (Advisor route); owner-directed
  Phase 0 chore work continues with 0 senior calls; stop after V5.

Do not mark readiness PASS with a failed or unknown required item. A provider catalog entry is not
a completed invocation. A new advisor answering the follow-up is not continuation. A readiness
response is not acceptance of a game packet.
