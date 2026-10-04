# Agent workflow

This file owns agent staffing and how agents work on the JSRF port. `AGENTS.md` owns
operating and build discipline, `plan-jsrf-bare-minimum.md` owns the current work and the
milestones, and `docs/jsrf-run-profiles.md` owns evidence profiles.

**The bare minimum is pragmatic (owner decision, 2026-09-30).** Take the cheapest honest
path to the title screen, then the rest of the slice. A shortcut is allowed when it is
recorded in `docs/jsrf-compatibility-ledger.md` and the run record lists its ledger ID;
strict evidence is kept for fidelity claims.

## 1. Roster

This table is the only place model assignments are written. One harness is supported:
the DeepSeek Harness (DSH).

| Role | Spawn parameters | Route @ effort |
|---|---|---|
| **Orchestrator** | not spawned; chosen at launch, verified from harness metadata | `claude/claude-sonnet-5-5` @ `medium` |
| **Workers** | `provider: workbuddy-ai`, `model: deepseek-v4.1-flash`, `reasoning_effort: max` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Persistent Advisor** | `provider: claude`, `model: claude-opus-5-5`, `reasoning_effort: xhigh` | `claude/claude-opus-5-5` @ `xhigh` (one continuable child per session) |
| **Turn reviewer** | `provider: claude`, `model: claude-opus-5-5`, `reasoning_effort: high` | `claude/claude-opus-5-5` @ `high` (a fresh child per review) |

Spawn with all three parameters exactly as the row gives them: DSH rejects a `model`
without its `provider`, and a model name alone does not select a route. The session
allow-list in the active DSH profile must contain every spawned route above.

**Fallback routes: NONE AUTHORISED.** Choosing a fallback is a staffing change, which is
owner-reserved (§6). An unavailable route is reported, never silently replaced.

## 2. Starting a session

1. Read `AGENTS.md`, this file, the plan's **Current work** section, and
   `docs/jsrf-run-profiles.md`.
2. Fetch both repositories and inspect their status. Preserve unrelated edits.
3. Confirm the Orchestrator's route and effort from harness metadata.
4. Spawn the Persistent Advisor as a continuable child on its §1 route and effort, and
   confirm a second message reaches the same child. A one-shot child is not the Advisor.
   If this fails, work that needs the Advisor waits and ordinary work continues.
5. Start working. The Turn reviewer's route is resolved when it is first needed.

## 3. Roles

### Orchestrator

The Orchestrator owns planning, implementation strategy, ordinary technical decisions,
evidence design, integration, building, testing, running, record keeping, commits and
pushes, and deciding what to do next. It revises its plan as evidence arrives and keeps
the plan's **Current work** section true. It needs no packet, preflight, or approval for
ordinary work.

Before investigating a blocker in depth, it searches prior art: upstream xboxrecomp, the
toolkit forks, Mercenaries-Recompiled
(`https://github.com/KraftMacAndChee/Mercenaries-Recompiled`) and halo-ce-universal
(`https://github.com/cybersecurity/halo-ce-universal`). The answer has repeatedly existed
elsewhere already.

### Workers

Spawn workers when parallelism or context isolation helps. Give each a bounded goal and,
where they write, disjoint files. Workers report measurements, changes, tests and blockers,
marking each claim as measured or inferred. The Orchestrator decides whether to continue,
redirect or replace a worker. A worker's summary is a lead: check a fact directly before a
decision rests on it.

### Persistent Advisor

The Advisor decides hard technical questions; its ruling ends a technical dispute. Consult
it when:

- two attempts at the same root cause have failed;
- credible measurements conflict;
- architecture or fidelity is genuinely in question;
- a proposed shortcut may hide behaviour a later milestone needs;
- the Orchestrator cannot confidently tell competing mechanisms apart;
- the Turn reviewer and the Orchestrator disagree on a material technical issue.

Ordinary debugging and implementation decisions do not need the Advisor.

Brief it with the smallest set of files and artifacts it should read itself, what has been
measured, and one question. It returns a ruling, its basis (observed or inferred), and
what would reverse it. Record a ruling that later work relies on in the plan or the
technical record. The Advisor never overrules a reproduced measurement by authority; it
orders a better measurement.

### Turn reviewer

Review once, when a turn has finished work: it changed code or records, ran the game, or
reports results. A turn that only answers a question or reports status is not reviewed.

Spawn a fresh Turn reviewer and brief it with the prompt, the draft reply, the turn's
commits and diff, test results, and the evidence the reply relies on. The reviewer checks
these itself and never receives the Advisor's conversation. The Persistent Advisor is
never the Turn reviewer: it would be reviewing its own rulings.

It looks for correctness problems rather than process compliance: real bugs, claims the
evidence does not support, requested work left undone, and unsafe repository state. For a
milestone claim, such as reaching the title screen, it reproduces the load-bearing
measurement rather than reading the reply. It returns:

```text
TURN_REVIEW: PASS | FIX
BLOCKING: <each: what is wrong, the evidence, the concrete consequence> | NONE
ADVISORY: <other findings> | NONE
```

On `FIX` the Orchestrator fixes the blocking items. Only fixes that change code or
behaviour get a re-review, and there is at most one. Wording, record cleanup and
commit-message corrections do not. Anything still disputed after that goes to the Advisor,
which decides, and the turn ends. If the reviewer route is unavailable, the reply says the
review did not run.

## 4. Hard rules

1. **Evidence beats opinion.** A failed, missing, stale, or `UNKNOWN` measurement never
   becomes a pass, and no ruling changes that.
2. **No claim of a fix without measuring it.** Name the command or artifact that shows it,
   or say `UNVERIFIED`.
3. **Say what was observed.** Label load-bearing claims as observed (read or run) or
   inferred.
4. **Absence needs coverage.** Zero hits prove absence only with a positive control showing
   the target would have been seen.
5. **The view is not the artifact.** Truncated or summarized output is not the file; read
   the underlying range before a count, negative, or provenance claim.
6. **A deciding test must be able to fail.** Where practical, break the behaviour and show
   the test fails for the intended reason.
7. **Validate the final tree.** Rerun load-bearing tests on the exact tree being committed;
   an earlier green run does not cover later edits.
8. **Shortcuts are recorded.** Every stub, patch, approximation, reimplementation or
   synthetic-completion switch a result relies on has a ledger entry, and the run record
   lists its ledger IDs. Exploratory evidence never supports a fidelity claim
   (`docs/jsrf-run-profiles.md`).
9. **Repository safety.** Original assets stay unchanged; commit and push as `AGENTS.md`
   describes; never force-push.

## 5. Records

- The plan's **Current work** section says what is being done, the current finding, and
  the next step. The Orchestrator updates it as evidence moves.
- Durable findings go in the plan or the technical record; shortcuts go in the ledger;
  strict-run stops go in `docs/reviews/strict-horizon-ledger.md`.
- No session reports in the repository; scratch notes stay outside it.

## 6. Owner decisions

Ask the owner only when a decision changes or requires:

- the project objective, scope, or what the bare minimum means;
- a fidelity trade-off that changes that objective;
- staffing or model assignments (§1);
- a destructive or hard-to-reverse repository or asset action;
- credentials, purchases, spending, legal or licensing choices;
- anything the owner marked `OWNER REQUIRED`.

Technical questions, policy gaps and disagreements between agents go to the Advisor, not
the owner. The owner may message any agent directly; that message is an owner instruction.
