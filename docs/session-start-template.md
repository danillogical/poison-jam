# Fresh-session startup receipt

Copy this template to `docs/reviews/startup-<date>-<session-id>.md` and fill every
field. This is a receipt, not a second policy: `docs/agent-workflow.md` owns the
roster, startup procedure and failure handling. Never store credentials here.

For a fresh DSH session, open the repository root and use this handoff prompt:

> Read AGENTS.md and the current docs/agent-workflow.md. Complete and record the
> startup checklist, including a real HY4 response and same-child persistent-advisor
> continuation. Read CURRENT STATE and the active plan. Resume the first unaccepted
> executable packet whose dependencies are accepted; preserve existing edits and
> reviews. Work to the frozen criteria, then obtain the required acceptance review.
> Do not treat historical next-step notes or a readiness probe as packet acceptance.

The prompt does not set the model or provider credentials: configure the session
and routes in the DSH launcher using the current workflow roster. A route failure
must be reported precisely; it is not permission to switch to a retired model.

## Identity and handoff

- Session ID/date/harness:
- Actual main model/effort (metadata evidence, or UNKNOWN):
- Workflow/plan/CURRENT STATE revisions and dirty diff identity:
- Game revision/status; toolkit revision/status:
- Unrelated edits preserved:
- Selected packet and contract revision:
- Dependencies and their recorded acceptance reviews:
- Last attempt/result; next exact action:
- Build/run owner and worker write ownership:

## Reviewer invocation — PASS / FAIL / UNKNOWN

- Requested route/effort; tool and returned route identity:
- Child ID; fresh challenge token:
- Completed response/turn reference:
- Empty-evidence rejection answer:
- Exact error or missing evidence:

## Persistent advisor — PASS / FAIL / UNKNOWN

- Requested route/effort; tool and returned route identity:
- Child ID; initial brief/response reference:
- Initial unique marker:
- Follow-up invocation/turn reference (same child, marker not in prompt):
- Returned marker and comparison:
- Decision log/brief location for later sessions:
- Exact error or missing evidence:

## Packet readiness — PASS / BLOCKED / UNKNOWN

- Required criterion IDs and frozen contract revision:
- Existing procedure/tool checks and any TOOLING REQUIRED prerequisites:
- Evidence profile, source/build identity and positive/negative controls:
- State/plan disagreements and resolution:
- Reviewer/advisor availability limitations:
- Overall disposition and next action:

Do not mark readiness PASS with a failed or unknown required item. A provider
catalog entry is not a completed invocation. A new advisor answering the follow-up
is not continuation. A readiness response is not acceptance of a game packet.
