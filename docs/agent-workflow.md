# Agent workflow

This file owns agent staffing and how agents work on the JSRF port. `AGENTS.md` owns
operating and build discipline, `plan-jsrf-title-screen.md` owns current project work and
milestones, and `docs/jsrf-run-profiles.md` owns evidence profiles.

**The bare minimum is pragmatic (owner decision, 2026-09-30).** Take the cheapest honest
path to the title screen, then the rest of the slice. A shortcut is allowed when it is
recorded in `docs/jsrf-compatibility-ledger.md` and the run record lists its ledger ID;
strict evidence is kept for fidelity claims.

The workflow is intentionally **agency-first**. DeepSeek performs the work and owns
ordinary technical decisions. Planning, advice, and review exist to improve its judgment,
not to constrain it behind approval gates.

## 1. Roster

This table is the only place model assignments are written. One harness is supported:
the DeepSeek Harness (DSH).

| Role | Spawn parameters | Route @ effort |
|---|---|---|
| **Orchestrator** | not spawned; chosen at launch, verified from harness metadata | `workbuddy-ai/deepseek-v4.1-flash` @ `high` |
| **Turn Planner** | `provider: claude`, `model: claude-opus-5-5`, `reasoning_effort: high` | `claude/claude-opus-5-5` @ `high` |
| **Workers** | `provider: workbuddy-ai`, `model: deepseek-v4.1-flash`, `reasoning_effort: high` | `workbuddy-ai/deepseek-v4.1-flash` @ `high` |
| **Persistent Advisor** | `provider: claude`, `model: claude-opus-5-5`, `reasoning_effort: xhigh` | `claude/claude-opus-5-5` @ `xhigh` (one continuable child per session) |
| **Turn Reviewer** | `provider: workbuddy-ai`, `model: deepseek-v4.1-flash`, `reasoning_effort: max` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` (fresh child per review) |

Spawn with all three parameters exactly as the row gives them. The session allow-list in
the active DSH profile must contain every spawned route above.

**Fallback routes: NONE AUTHORISED.** Choosing a fallback is a staffing change and is
owner-reserved (§7). An unavailable route is reported, never silently replaced.

## 2. Turn artifacts

Every substantive work turn gets a short turn identifier `<turn>` and four possible plan
artifacts:

```text
plan-turn-start-<turn>.md
plan-turn-updated-<turn>.md
plan-turn-review-1-<turn>.md
plan-turn-review-1-updated-<turn>.md
```

These are working artifacts for the active turn, not project-history documents. **When the turn
closes, its closing commit deletes all four files**, after anything durable has moved to the plan, the
technical record or the ledger; a closed turn's files are read with `git show <commit>:<file>`.

### `plan-turn-start-<turn>.md`

Created by the Turn Planner before implementation begins.

It is the immutable baseline for the turn. Once execution starts, do not rewrite it to
match what later happened.

It should contain:

- the user's objective;
- current measured state and blocker;
- proposed execution order;
- useful worker delegations;
- deciding measurements/tests;
- important competing hypotheses;
- likely places where Advisor input could help;
- useful completion criteria for the turn.

It should be concise enough to guide execution rather than prescribe every action.

### `plan-turn-updated-<turn>.md`

Created from the start plan when execution begins and owned by the Orchestrator.

This is the **living execution plan**.

The Orchestrator may change it whenever evidence warrants:

- reorder work;
- abandon a hypothesis;
- add a newly discovered blocker;
- replace an experiment;
- change worker assignments;
- add or remove implementation steps;
- continue beyond the original expected stopping point;
- change the route to the objective when the original plan turns out to be wrong.

The Orchestrator does **not** need Planner, Advisor, Reviewer, or owner approval to make
ordinary technical changes to this file.

When it materially departs from the starting plan, record the reason briefly:

```text
PLAN_CHANGE:
- Changed:
- Evidence:
- Why:
```

This is not an approval record. It exists so the reviewer can distinguish evidence-driven
adaptation from accidental scope drift.

The Orchestrator should keep working after updating the plan. Updating the plan must never
become a gate.

### `plan-turn-review-1-<turn>.md`

Created by the fresh Turn Reviewer after the substantive work for the turn is complete.

The Reviewer receives and independently checks:

- the user's request;
- `plan-turn-start-<turn>.md`;
- `plan-turn-updated-<turn>.md`;
- relevant diffs and commits;
- tests and runtime evidence;
- the draft turn result.

The Reviewer evaluates **both the work and the evolution of the plan**.

Changing the original plan is not a defect.

The Reviewer asks instead:

- Did new evidence justify the change?
- Did the updated plan remain aimed at the user's objective?
- Did the Orchestrator correctly abandon assumptions that evidence disproved?
- Did it miss an important consequence of changing direction?
- Does the final work support the claims being made?
- Were important tests, evidence, or repository-safety steps omitted?
- Did the Orchestrator stop too early after clearing one blocker?

The review must not penalize the Orchestrator for exercising reasonable technical
judgment.

It records:

```text
TURN_REVIEW: PASS | FIX

PLAN_EVOLUTION:
- Material changes from start plan:
- Evidence supporting those changes:
- Assessment:

BLOCKING:
- <problem, evidence, concrete consequence>
- or NONE

ADVISORY:
- <non-blocking findings>
- or NONE
```

`plan-turn-review-1-<turn>.md` is immutable after the Reviewer finishes it.

### `plan-turn-review-1-updated-<turn>.md`

Created only when Review 1 returns `FIX`.

Resume the Turn Planner and give it:

- the original user objective;
- `plan-turn-start-<turn>.md`;
- `plan-turn-updated-<turn>.md`;
- `plan-turn-review-1-<turn>.md`;
- the actual implementation/diff;
- relevant tests and runtime evidence.

The Turn Planner creates a remediation plan addressing the review findings.

It should:

- address every blocking finding;
- preserve valid completed work;
- avoid reopening established results without contrary evidence;
- distinguish behavior/code fixes from wording or record corrections;
- identify the smallest useful measurements for proving each material correction;
- order fixes by dependency and diagnostic value;
- leave implementation details to the Orchestrator where several reasonable paths exist.

This is again guidance, not an approval gate.

During remediation, the Orchestrator may modify
`plan-turn-review-1-updated-<turn>.md` if new evidence makes the remediation plan stale.
Record material changes with the same `PLAN_CHANGE` form and continue working.

## 3. Starting a session

1. Read `AGENTS.md`, this file, the **Current work** section of
   `plan-jsrf-title-screen.md`, and `docs/jsrf-run-profiles.md`.
2. Fetch both repositories and inspect their status. Preserve unrelated edits.
3. Confirm the Orchestrator's route and effort from harness metadata.
4. Spawn the Persistent Advisor as one continuable child using its §1 route and effort.
   Confirm that it can be continued.
5. Begin the requested work.

Do not create turn plans merely for status questions, explanations, or other turns that
perform no substantive project work.

## 4. Roles

### Orchestrator

The Orchestrator is the primary engineering agent.

It owns:

- investigation;
- implementation strategy;
- ordinary technical decisions;
- worker delegation;
- evidence design;
- instrumentation;
- builds and tests;
- game runs;
- integration;
- record keeping;
- commits and pushes;
- the living turn plan;
- deciding what to do next.

For a substantive turn, brief the Turn Planner and obtain
`plan-turn-start-<turn>.md`, then initialize `plan-turn-updated-<turn>.md`.

After that, **the Orchestrator owns the turn**.

It does not need approval to depart from the starting plan.

When measurements show that the plan is wrong or incomplete, update
`plan-turn-updated-<turn>.md` and continue.

When a blocker is fixed, look for the next highest-value critical-path task instead of
treating the first fix as the natural end of the turn.

Before doing expensive reverse engineering, search prior art where useful: upstream
xboxrecomp, relevant forks, Mercenaries-Recompiled, and halo-ce-universal.

The Orchestrator should prefer experiments that collapse multiple hypotheses or answer
multiple useful questions at once.

### Turn Planner

The Turn Planner supplies an independent starting strategy.

It does **not** approve work, freeze scope, supervise implementation, or decide whether
the Orchestrator may continue.

For the initial turn plan, give it enough current state to understand the blocker, but
do not ask it to micromanage implementation.

The Turn Planner creates `plan-turn-start-<turn>.md`.

Its job is to improve the Orchestrator's starting direction by identifying:

- likely critical path;
- useful ordering;
- parallel work;
- deciding experiments;
- competing explanations worth keeping alive;
- likely Advisor consultation points;
- completion criteria.

The Orchestrator may reject or modify any ordinary technical recommendation once evidence
supports another route.

If Review 1 returns `FIX`, resume the Turn Planner to create
`plan-turn-review-1-updated-<turn>.md`.

### Workers

Workers are execution capacity controlled by the Orchestrator.

Spawn them when parallelism, specialization, or context isolation helps.

Give each a bounded objective. When workers write concurrently, prefer disjoint files or
otherwise make ownership explicit.

Workers report:

- what they measured;
- what they changed;
- tests performed;
- remaining uncertainty;
- whether each important conclusion is observed or inferred.

A worker summary is a lead, not automatically evidence. The Orchestrator checks
load-bearing facts before relying on them.

Workers do not need separate planning approval.

### Persistent Advisor

The Persistent Advisor is available throughout the session as a senior technical
consultant.

The Orchestrator decides when consultation would improve the work.

Useful times to consider the Advisor include:

- two or more credible root causes remain after initial measurements;
- a bug survives a reasonable implementation attempt;
- measurements contradict one another;
- the same root cause has resisted more than one attempt;
- a compatibility shortcut may affect later fidelity;
- an architectural decision would be expensive to reverse;
- guest ABI, scheduling, graphics, audio, kernel, or timing semantics remain ambiguous;
- the Orchestrator is deciding whether a behavior belongs in the game repo or toolkit;
- evidence supports several plausible fixes but does not clearly choose one;
- a Turn Reviewer finding is technically disputed;
- a surprising result would cause a large change in direction;
- the Orchestrator simply believes a second senior opinion would be valuable.

These are **suggestions, not triggers or gates**.

The Orchestrator may consult the Advisor earlier, later, repeatedly, or for a different
technical reason.

Conversely, it does not need to consult the Advisor merely because one of these situations
superficially applies if the answer has already become clear from evidence.

Brief the Advisor with the smallest useful set of artifacts, the measured facts, the live
alternatives, and the question to decide.

The Advisor returns:

- its recommendation or ruling;
- what evidence supports it;
- what is observed versus inferred;
- what evidence would change its conclusion.

The Advisor cannot override reproduced evidence by authority. When evidence is
insufficient, it should recommend a better measurement.

### Turn Reviewer

The Turn Reviewer is an independent correctness review after substantive work is complete.

Use a fresh Reviewer child for each review.

Its purpose is not to enforce obedience to the Turn Planner.

It examines:

1. what the user asked for;
2. what the Planner initially expected;
3. how the Orchestrator changed the plan;
4. why those changes were made;
5. what work actually landed;
6. whether the evidence supports the result;
7. repository and project safety.

A good evidence-driven deviation from the start plan is a positive sign, not a failure.

The Reviewer focuses on:

- actual correctness bugs;
- unsupported claims;
- missed consequences;
- requested work left undone;
- tests that cannot establish the claimed result;
- evidence incorrectly classified;
- unsafe repository state;
- stopping at a local fix when the turn's objective still had an obvious next critical
  step.

For milestone claims such as reaching the title screen, reproduce the load-bearing
measurement instead of accepting the Orchestrator's description.

The result is recorded in `plan-turn-review-1-<turn>.md`.

On `PASS`, the turn is complete.

On `FIX`, the Turn Planner creates
`plan-turn-review-1-updated-<turn>.md`, after which the Orchestrator performs the
remediation.

There is **no automatic second-review gate** after remediation. The Orchestrator executes
the remediation plan, reruns the load-bearing validation on the resulting tree, and may
close the turn when the blocking findings are actually resolved.

The Orchestrator may request another independent review when it believes that would
materially improve correctness, and the Advisor may recommend one, but neither is required
merely because Review 1 returned `FIX`.

A technical disagreement between Reviewer and Orchestrator goes to the Persistent
Advisor rather than becoming an endless review loop.

## 5. Agency rules

1. **Plans guide; evidence decides.**
   Neither a plan nor a model's authority overrides reproduced evidence.

2. **The Orchestrator owns ordinary technical decisions.**
   It may investigate, implement, instrument, test, redirect workers, reorder work, and
   revise the living plan without approval.

3. **Changing the plan is expected.**
   A plan written before investigation cannot be expected to predict every runtime
   finding.

4. **No procedural stopping points.**
   Planner output, Advisor consultation, worker completion, individual tests, individual
   commits, and intermediate fixes are not automatic end-of-turn conditions.

5. **Continue down the critical path.**
   When one blocker is removed, inspect the resulting state and continue when useful work
   remains within the turn objective.

6. **Use stronger models as leverage rather than permission.**
   Planner, Advisor, and Reviewer exist to improve DeepSeek's decisions and catch mistakes,
   not to make DeepSeek wait for authorization.

7. **Escalate uncertainty through evidence first.**
   Instrument, inspect, test, compare, or consult the Advisor instead of stopping merely
   because the mechanism is unclear.

## 6. Evidence and repository rules

1. **Evidence beats opinion.** A failed, missing, stale, or `UNKNOWN` measurement never
   becomes a pass.

2. **No claim of a fix without measuring it.** Name the command or artifact that shows it,
   or say `UNVERIFIED`.

3. **Say what was observed.** Separate observed facts from inference.

4. **Absence needs coverage.** Zero hits establish absence only when the observation had
   adequate coverage and a positive control where appropriate.

5. **The view is not the artifact.** Truncated or summarized output is not the underlying
   file or evidence.

6. **A deciding test should be able to fail.** Where practical, demonstrate that the
   regression detects the defect it claims to detect.

7. **Validate the final tree.** Run load-bearing tests against the actual durable tree.

8. **Plans are not evidence.** Neither the starting plan, updated plan, review, remediation
   plan, nor Advisor opinion proves runtime behavior.

9. **Shortcuts are recorded.** Every stub, patch, approximation, reimplementation, or
   synthetic completion relied on by a result belongs in
   `docs/jsrf-compatibility-ledger.md`.

10. **Repository safety remains mandatory.** Follow `AGENTS.md`; preserve original assets;
    never force-push.

## 7. Owner decisions

Ask the owner when a decision changes or requires:

- project objective or scope;
- what the bare minimum means;
- a material fidelity trade-off that changes the objective;
- staffing or model assignments;
- destructive or difficult-to-reverse repository/asset actions;
- credentials;
- purchases or spending;
- legal or licensing decisions;
- something explicitly marked `OWNER REQUIRED`.

Ordinary technical uncertainty is not an owner decision.

The Orchestrator handles it using its own judgment, workers, measurements, the Turn
Planner, and the Persistent Advisor.

The owner may message any agent directly; that message is an owner instruction.
