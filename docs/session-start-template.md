# Fresh-session startup receipt

Copy this template to `docs/reviews/startup-<date>-<session-id>.md` and fill every
field. This is a receipt, not a second policy: `docs/agent-workflow.md` owns the
roster, startup procedure and failure handling. Never store credentials here.

For a fresh DSH session, open the repository root and use this handoff prompt:

> Read `AGENTS.md`, `docs/agent-workflow.md`, `plan-jsrf-bare-minimum.md`, and
> `docs/jsrf-run-profiles.md`. Complete the workflow startup checklist (§0) using
> the roles designated in workflow §1, including live route resolution, the
> packet-reviewer probe, the Decision guardrail probe, and the combined persistent-advisor
> probe.
>
> You are a contract role (§2.2): execute the frozen contract exactly, stop at
> ambiguity or any stop boundary, and escalate technical questions to the Advisor.
> Treat other agents' summaries as leads, never as evidence.
>
> Execute only the exact packet/revision explicitly promoted by the plan's
> `CURRENT PACKET` block.

The prompt does not set the model or provider credentials: configure the session
and routes in the DSH launcher using the current workflow roster. A route failure
must be reported precisely; it is not permission to switch to a retired model.

## Identity and handoff

- Session ID/date/harness:
- Actual main model/effort (metadata evidence, or UNKNOWN):
- Workflow/plan/run-profile revisions and dirty diff identity:
- Game revision/status; toolkit revision/status:
- Unrelated edits preserved:
- CURRENT PACKET copied from plan (packet + exact revision + SHA-256), or NONE:
- Dependencies and their recorded acceptance reviews:
- Next exact authorized action:
- Build/run owner and worker write ownership:

## Route resolution — PASS / BLOCKED

- Planner: requested route/effort; returned route identity:
- Persistent advisor: requested route/effort; returned route identity:
- Decision guardrail: requested route/effort; returned route identity:
- Packet reviewer: requested route/effort; returned route identity:
- Turn reviewer: requested route/effort; returned route identity (verified on first use):
- Workers: requested route/effort; returned route identity:
- Exact error or ambiguity, if any:

## Packet reviewer probe — PASS / FAIL / UNKNOWN

Probe it on a fresh child that runs one read-only command (workflow §0 step 3).

- Child ID; fresh token; response reference; empty-evidence answer; command and output hash; effort; result:
- Exact error or missing evidence:

## Decision guardrail probe — PASS / FAIL / UNKNOWN

- Guardrail child ID; route; effort; workspace; named fact read; turn-2 marker; result:
- Exact error or missing evidence:

## Persistent advisor probe — PASS / FAIL / UNKNOWN

- Child ID:
- Turn 1 reference; unique marker given:
- Named file and the fact deliberately omitted from the brief:
- Advisor's answer; checked against the file:
- Turn 2 reference (same child, marker not repeated); returned marker:
- Result:
- Exact error or missing evidence:

## Packet readiness — PASS / BLOCKED / UNKNOWN

- Frozen revision/hash matches `CURRENT PACKET`:
- Adequacy review record and verdict:
- Deferred advisories (recorded, not acted on):
- Prerequisites / tooling checks:
- State/plan disagreements and how they were escalated:
- Overall disposition and next action:

Do not mark readiness PASS with a failed or unknown required item. A provider
catalog entry is not a completed invocation. A new advisor answering the follow-up
is not continuation. A readiness response is not acceptance of a game packet.
