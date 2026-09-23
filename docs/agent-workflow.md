# Agent workflow: models, the session loop, and escalation

This file is the **single authority** for how a JSRF session is staffed and how it
works. It replaces `grok-role-map.md`, `deepseek-harness.md` (retired 2026-09-22) and
`.dsh/skills/advisor-escalation/` (folded in 2026-09-23). `AGENTS.md` carries a short
mandatory pointer to it, because that is the only file loaded automatically.

**Read this at session start, and again when its revision changes or when you resume
with uncertain context.** A policy can change during a long-lived session — it did,
twice, on 2026-09-22 — so "read once" is not enough.

---

## 0. Startup checks — do these before selecting work

Complete this checklist in every new top-level session before implementing game
packets. Revalidate affected checks after a route/policy change or loss of child
state. A catalog entry, old child ID or previous session's PASS is not readiness.
Read-only diagnosis and repairing this workflow may proceed while readiness is
blocked, but cannot be presented as accepted game work.

1. **Identify the harness and instructions.** Read this file, the active plan,
   `report-deepseek.md` CURRENT STATE and `docs/jsrf-run-profiles.md`. Record their
   Git revision plus working-tree diff identity. Identify the configured session
   model/effort from actual metadata, or record UNKNOWN; documentation cannot
   change an already running model. Use only the current column in §1.
2. **Actually invoke the reviewer.** DSH: discover with `list_subagent_models`,
   then create `workbuddy-ai/hy4-preview-f` at `high` for a read-only readiness
   challenge. Codex: create a separate `gpt-6-luna` at `max` for that challenge.
   Ask it to return a fresh session-specific token and identify one way an empty
   evidence set must fail acceptance. Record requested/returned route, child ID,
   response and error if any. PASS requires an actual completed response from the
   intended route, not just successful dispatch. HY4 is a **DSH** prerequisite;
   do not invent a Codex HY4 route. A smoke reply is readiness, not packet review.
3. **Create and verify the persistent advisor now.** Use §4 with a fresh
   self-contained brief and a unique marker. Record the child ID. After its first
   reply, send a continuation to the same child asking for that marker without
   supplying it again. PASS requires the correct reply on that same child. A
   replacement one-shot answer is FAIL. Reuse this advisor for subsequent delta
   briefs in this top-level session; a new session creates/revalidates its own.
4. **Reconcile state and repository ownership.** Inspect both Git working trees;
   preserve unrelated edits. Select the first unaccepted executable packet whose
   dependencies are accepted. The plan owns acceptance; CURRENT STATE owns the
   latest observed stop. Reconcile contradictions explicitly. Do not resume a
   historical "next packet" or repeat delivered work solely because review is
   missing. Assign one build/run owner and disjoint worker write scopes.
5. **Check packet readiness.** Exact criterion IDs/revision, real commands or an
   explicitly scheduled tooling prerequisite, admissible profile, controls and
   reviewer must exist. If they do not, refine the contract with the advisor before
   implementation. No executor may invent an oracle while claiming the packet met
   its old criteria. Record attempts and evidence in a packet record (§2.1).
6. **Persist the startup receipt.** Use `docs/session-start-template.md`, saving a
   filled receipt under `docs/reviews/startup-<date>-<session-id>.md` and linking it
   from the live report. Include reviewer probe, advisor continuation, selected
   packet, both repository identities, dirty files and remaining blockers. Missing
   or unverifiable evidence means BLOCKED/UNKNOWN, never an inferred PASS.

If a required spawn/continuation fails, record its exact error and consult the
available advisor; do not substitute Sol, Terra or another unlisted route. If the
advisor itself is unavailable, record the startup blocker and the manual route/auth
repair needed. Continue only independent diagnosis/documentation. Never put secrets
or full provider configuration into the receipt.

---

## 1. The two supported harnesses

Exactly two harnesses are supported. Use only your column's routes.
This repository roster implements the user's latest instruction and supersedes
older personal-skill defaults naming Sol/Terra or Luna High. Keep Away cloud-only
behavior; do not restore retired roles when loading a personal workflow skill.

| Role | Codex | DeepSeek Harness (DSH) |
|---|---|---|
| **Session** | `gpt-6-luna` @ `max` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Worker subagents** | `gpt-6-luna` @ `max` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Planner** | `gpt-6-astra` @ `medium` | `workbuddy-ai/kimi-k3` |
| **Persistent advisor** | `gpt-6-astra` @ `medium` | `workbuddy-ai/kimi-k3` |
| **Acceptance reviewer** | `gpt-6-luna` @ `max` | `workbuddy-ai/hy4-preview-f` @ `high` |

**The DSH advisor changed on 2026-09-23 by direct user instruction.** Astra ran out
of tokens; the user replaced it with `workbuddy-ai/kimi-k3`. This supersedes the
`codex:gpt-6-astra` cell for DSH only — the Codex advisor is still
`gpt-6-astra` @ `medium`. The consequence is recorded honestly below: the DSH
advisor is now on the **same provider as the session and the reviewer**, so it no
longer supplies model diversity by itself.

**The Planner and the advisor are the same model.** That is acceptable, with three
safeguards, because the risk is real: **the advisor is not model-diverse relative to
the plan author**, so a bad plan can be endorsed by a same-model reviewer. In DSH
this is now doubly true — after the 2026-09-23 substitution, session, workers,
Planner, advisor and reviewer all sit on `workbuddy-ai`, so the advisor adds
**procedural** independence only.

- **Separate children.** When a plan is disputed, do not continue the Planner under
  an "Advisor" label. Spawn a fresh child with the user's objective, the disputed
  criterion, and *both* positions and their evidence — not just the Planner's
  rationale.
- **Never count their agreement as model-diverse confirmation.** Separate contexts
  reduce commitment to an argument; they do not remove correlated reasoning errors.
  If the advisor cannot resolve a dispute with a discriminating measurement, the
  uncertainty stays.
- **Planning authority is not acceptance authority.** The Planner authors proposed
  packets; the session owns integration into the active plan and execution; the
  reviewer owns the evidence. **Neither the Planner nor the advisor may make a
  failed check pass by rewriting its meaning** — a criterion change needs a new
  revision, a reason, and re-review.

**Plan adequacy is reviewed before execution**, separately from the later review of
implementation evidence. The question for that review is: *could an implementation
satisfy these checks while failing the objective?*


**Everything else is retired.** Do not select `gpt-5.6-sol`, `gpt-5.6-luna`,
`gpt-5.6-terra`, `gpt-5.5`, Grok, `grok-cli`, `hy3`, `glm-5.3`, or any
`workbuddy-ai/gpt-*` route. Those names fill the historical sections of the reports
and old commit messages; read them as history, never as a roster. **Historical review
identities and evidence provenance keep their original names** — do not retroactively
rename an old reviewer to a current one.

> **Correction (2026-09-23).** `kimi-k3` was listed as retired in the paragraph
> above. The user has since named it as the DSH advisor after Astra exhausted its
> tokens, so it is now **current** for that one role. The retirement list is not
> authority over a later direct user instruction. Do not extend this to any other
> role: `kimi-k3` is the DSH advisor and nothing else.

### Measured constraints on this table

Verified with `list_subagent_models` 2026-09-23. These are not preferences; they are
what the routes actually serve:

- **In DSH the advisor is `workbuddy-ai/kimi-k3`** (user instruction, 2026-09-23),
  replacing `codex:gpt-6-astra` after that route exhausted its tokens.
  `list_subagent_models` reports it as advertised with **no reasoning-effort
  levels**, so do not pass `reasoning_effort` for it — the session's own startup
  invocation omitted the field and the child reported
  `agentProvider: workbuddy-ai`, `agentModel: kimi-k3` with no effort key.
  **Independence caveat, measured not assumed:** the DSH session
  (`workbuddy-ai/deepseek-v4.1-flash`), the reviewer
  (`workbuddy-ai/hy4-preview-f`) and now the advisor are all on `workbuddy-ai`.
  The advisor therefore still gives **procedural independence** (separate child,
  no shared context, asked to adjudicate rather than confirm) but **not model
  diversity in the provider sense**. Treat its rulings as decision authority, not
  as third-family confirmation.
- **`workbuddy-ai` does not serve `gpt-6-astra`** (it advertises only
  `hy4-preview-f`, `deepseek-v4.1-flash`, `gpt-5.5` and `kimi-k3`). If an Astra
  advisor is ever wanted in DSH again, it must come from `codex`.
- **`workbuddy-ai/hy4-preview-f` advertises exactly one effort: `high`.** Do not try
  to raise it; there is no such setting and the attempt fails.
- `workbuddy-ai/deepseek-v4.1-flash` serves up to `max`; `codex/gpt-6-luna` up to
  `max`; `codex/gpt-6-astra` up to `ultra`.

### Independence is three different things, and only one is model diversity

Conflating them has already caused two opposite errors here — overclaiming a Codex
review, and under-crediting a same-family reviewer that genuinely re-ran a test.

| kind | what it means | who has it |
|---|---|---|
| **model diversity** | a different model family, so systematic misreadings are less likely to correlate | DSH reviewer (`hy4-preview-f`, third family); the advisor in both harnesses |
| **procedural independence** | the reviewer did not write the change and is asked to falsify, not confirm | any reviewer, including the Codex one |
| **evidence reproduction** | the reviewer re-ran the load-bearing measurement itself | any reviewer that actually re-runs it |

**The Codex reviewer is the session's own model family** (both use `gpt-6-luna`
at `max`), so it has procedural independence and can reproduce evidence, but **not**
model diversity. Say *"the reviewer reproduced the measurement"* when that is what
happened, and reserve *"independently verified"* for a different-family check. If a
Codex packet's acceptance is genuinely load-bearing, send it to the advisor rather
than leaning on the reviewer's agreement.

---

## 2. The session loop

### 2.1 Open a packet record first

Before implementing, record: the packet, its **exact acceptance criteria** (quoted,
not paraphrased), the evidence revision, the reviewer you will use, and an attempt
log. The attempt log is what makes "I am looping" a *checkable state* rather than a
feeling — see §3. Keep it in the plan or the report; keep it durable.

### 2.2 Then work the packet, with the session owning integration

The session owns the plan, the primary implementation, and **integration, build and
run** — a worker never performs those, and the session adjudicates every result.
Beyond that, **delegate what is useful**:

- **Context isolation** — read a large log, dump or artifact and return a bounded
  summary so the raw content never enters the session's window. Highest-value use;
  reach for it first.
- **Bounded scoped implementation** — a well-specified piece of work with a clear
  contract, expected behavior and its own acceptance check. Give it exact files,
  facts-vs-hypotheses, and what "done" means.

A worker returns `file:line` evidence marked *measured* or *inferred*. **The session
integrates and runs the acceptance test; it does not hand a whole packet away and
wait.** Shared-tree write ownership stays explicit: one writing owner at a time.

> **Correction (2026-09-23).** This step used to read *"spawn a worker **only** for
> context isolation"* and *"a worker is a reader, not a decider"*, with "never to
> implement" in `AGENTS.md`. **That restriction was never authorized.** The user's
> note says *"spawning worker subagents **as needed**"*, and the instruction it
> descends from authorized *unlimited workers*, not readers-only. The narrowing was
> introduced by the session and then treated as policy — the same failure mode as the
> stale claims below, with the session as the author of the stale claim. What is
> genuinely required is that **one owner holds integration, build and run**; that is
> a different rule, and conflating them removed useful capacity for no stated reason.

### 2.3 Work until the criteria pass

The plan defines them. When they pass the packet is **delivered**, not **accepted**.
Attach criterion-specific evidence to the packet record — a criterion with no
evidence behind it is not met, however green the suite looks.

### 2.4 Get the reviewer to confirm

Spawn the reviewer from §1 and require it to **verify or refute each criterion
independently** — reproducing the load-bearing measurements itself — returning
per-criterion AGREED / DISAGREED / CANNOT VERIFY with the command or `file:line`
behind it. Ask for the falsification rather than the confirmation, and require a
**positive control** wherever it checks that something is absent. Record the review
in `report-deepseek.md` **before** advancing the packet's status.

### 2.5 Escalate a disagreement — do not out-vote it

If session and reviewer disagree, both positions and their evidence go to the
advisor, whose call is **final**. Do not out-vote the reviewer; do not let it
out-vote the session. An unresolved disagreement usually means a *measurement* is
broken, which is the case the advisor exists for.

### 2.6 Close the packet, then continue

After a review or an advisor ruling: **apply the required changes, re-run the
affected criteria, obtain re-review of those criteria, persist the disposition, then
select the next packet.** A ruling that is not applied and re-verified has closed
nothing.

### 2.7 Closing a packet — the transitions, including the awkward ones

"Delivered" and "accepted" are different states, and the gap has more than one exit.
All of these are legitimate; only the second is success.

| state | meaning | what unblocks it |
|---|---|---|
| **delivered** | criteria met as measured by the session, with evidence attached | a reviewer must verify |
| **accepted** | the reviewer reproduced the criteria and AGREED | nothing — record it |
| **pending — CANNOT VERIFY** | the reviewer could not reproduce a measurement | **new evidence that the reviewer then evaluates, or an explicit advisor ruling on that criterion.** The session's own green test does *not* close it, and merely supplying evidence does not either — it must be assessed |
| **pending — reviewer unavailable** | the route is missing or the spawn failed | escalate; **do not substitute a model the policy did not name** |
| **pending — post-review edits** | the tree changed after the review | re-review the affected criteria; a review covers the revision it saw |
| **escalated** | session and reviewer disagree | the advisor's ruling, recorded with both positions, then §2.6 |
| **exploratory evidence** | the run carried synthetic-completion or bypass overrides | the *profile* criterion is unmet. Re-label, and re-open only the claims that depended on it |

**Advisor finality is decision authority, not proof.** Its ruling settles *who
decides*; it does not make a failed measurement pass, and no verdict changes what a
measurement says. Record the ruling against the criterion it addresses.

**A review covers the revision it saw.** If the tree changes afterwards, the affected
criteria are unreviewed again. Always say which revision was reviewed.

---

## 3. When to escalate to the advisor

Escalate on **material unresolved uncertainty that blocks a decision** — not on every
expected failing test. An ordinary red result during implementation is the work, not
a trigger; escalating it stalls the loop.

- **You are going in circles.** The same failure has survived two attempts, or you
  are re-deriving something you already tried. **Check your attempt log (§2.1): two
  entries for the same root cause means escalate before a third.** A third attempt
  from memory is a guess.
- **You are hitting a wall.** Progress has stopped: no new measurement is changing
  your mind, or every next step is another guess.
- **Two measurements contradict each other** and neither is obviously the artifact.
  Highest-value case — it almost always means one *measurement method* is broken, and
  an outside view finds it faster than more measurements do.
- **You are about to make a universal claim** — "impossible", "cannot", "rules out" —
  or to treat a **negative result** as settled. A check that finds nothing is a claim
  about the *check* as much as about the system. Honest uncertainty ("I have not
  proven X") is not itself a trigger; *relying* on the unproven thing is.
- **Before an expensive investigation** — a long build, a large recovery pass, a full
  regeneration — where a wrong premise wastes hours.
- **A review disagreement** (§2.5).
- **An acceptance criterion is unmet and you cannot close it**, ambiguous, or
  contradicted by evidence. Unfinished work is not a trigger; a criterion you cannot
  resolve is.

### Two kinds of consult — ask for the right one

The default is fault diagnosis. Architecture and policy questions need a different
ask, and using the fault template for them gets a fault-shaped answer.

**(a) Fault diagnosis** — *"rank the mechanisms that could produce this, and give the
cheapest experiment that discriminates each."* Use for a defect, a contradiction, a
stall.

**(b) Architecture / policy** — *"give me the options, their trade-offs, your
recommendation, and how I would verify the choice."* Use for "where should this live",
"is this structure right", "should we keep X". Do not ask a fault template where the
question is design.

Adopt or reject each recommendation with a recorded reason. **Do not adopt one you
cannot test**, and do not treat the advisor's confidence as evidence — it is reasoning
about the facts you gave it, so a wrong input yields a confidently wrong answer.

---

## 4. The advisor contract

### 4.1 Shared policy (both harnesses)

**Brief it fresh at startup, verify continuation, then keep it.** The first consult
is a self-contained brief; later consults go to the same child with only the delta.
The user's 2026-09-23 instruction requires an existing, tested advisor at startup;
it supersedes the earlier lazy-creation policy. A new top-level session must not
assume an old session's child is reachable. Durable briefs/decisions transfer
knowledge between sessions; child persistence is only promised within the session
where continuation was actually tested.

**Never use `subagent_fork` for the advisor.** Seeding it with this conversation
destroys the independence that makes it worth consulting.

**Record the prediction before testing it.** When the advisor predicts a specific
value, address or mechanism, write it down first. A confirmed specific prediction is
far stronger evidence than a plausible explanation, and an unrecorded one cannot be
distinguished from hindsight.

### 4.2 Invocation — DeepSeek Harness (DSH)

- Discover the installed subagent creation tool and read its current schema; do
  not copy Codex's `collaboration.spawn_agent` arguments into DSH. Select the route
  from §1 using that tool's advertised provider/model/effort fields. If the schema
  or required route is unavailable, mark startup BLOCKED rather than inventing an
  invocation. The supplied 2026-09-23 transcript is an Astra **child** log and does
  not contain its parent's creation call, so it cannot establish the spawn schema.
- **Spawn with `run_in_background: true`, always.** Getting this wrong is silent:
  `false` produces a **one-shot** child that answers its first question perfectly and
  then rejects every continuation with *"has no supported continuation state and
  cannot be resumed"*, forcing a full re-brief. Measured, with source:
  `resolveDelegationRun` returns
  `{ runInBackground: request.run_in_background ?? options.continuable }`
  (`dsh-tool-subagent/lib/index.js:360`) and only the `true` branch consults
  `continuable` (`:521-526`). "The answer gates my next action" is **not** a reason to
  pass `false` — it means do not start other work until the notice arrives.
- **Keep the child id** (`started subagent <id>`) and continue it with `send_message`.
  The supplied child transcript demonstrates `send_message` arguments
  `{ "agent_id": "<existing child id>", "message": "<delta or challenge>" }`;
  replace placeholders with the current session's returned identity and message.
  The log demonstrates repeated child continuation, not live HY4 availability.
- **The tell:** a foreground one-shot returns its answer inline; a continuable spawn
  returns `started subagent <id>`. Confirm child registration with `list_agents`;
  confirm continuation with the same-child marker exchange required by §0.
  A listed child alone does not prove that its next turn works.

### 4.3 Invocation — Codex

Use `collaboration.spawn_agent` with `fork_turns="none"`,
`model="gpt-6-astra"`, `reasoning_effort="medium"` and a self-contained brief.
Keep the returned canonical task name. Use `collaboration.followup_task` to resume
the same idle child; `send_message` alone does not start an idle Codex turn. For a
running child use `send_message`; collect its reply before relying on it. Run the
marker continuation check in §0 and record the result. Do not project DSH's
`run_in_background` argument onto Codex or claim cross-session persistence.

### 4.4 The briefing template

**First consult — self-contained.** The advisor has no context:

```
You are the independent advisor on a static-recompilation project. You have NOT
seen this conversation; everything you need is below.

Repository: <path>   Toolkit (read-only for you): <path>
Python: <path>  (Windows; PowerShell. PYTHONPATH=<site-packages> for capstone.)

FACTS (measured, each with the command or file:line that produced it):
  - <fact>  [evidence]
  - <fact>  [evidence]

HYPOTHESES already tried, and how each failed:
  - <hypothesis> -> <what it predicted> vs <what was measured>

WHAT CONTRADICTS WHAT:
  - <measurement A> says <X>; <measurement B> says <Y>.

THE QUESTION: <one sentence>

WHAT I WANT: rank the mechanisms that could produce this, and for each give the
cheapest experiment that would DISCRIMINATE it from the others. Say explicitly
which of my facts you are treating as unreliable, and why.
```

For a **policy or architecture** question, replace the last paragraph with: *"give me
the options, their trade-offs, your recommendation, and how I would verify the
choice."*

**Follow-ups — the delta only.** Send to the same child id. It retains the earlier
exchange, so re-briefing is waste: state what is new, the result, and the question it
raises.

**Mark every claim MEASURED (with its evidence) or INFERRED.** Ask the advisor to do
the same — it is what lets you tell its reasoning from its reading, and it caught a
wrong fact of mine more than once.

---

## 4b. The Planner, and what a criterion must contain

The **Planner** authors proposed packets. The session integrates and executes them;
the reviewer checks the evidence. See §1 for the same-model safeguards.

**The recurring structural defect is an incomplete chain:**

> **Claim -> admissible evidence -> reproducible procedure -> decision rule ->
> recorded disposition**

The executor ends up inventing one of those links. Prose is not the problem —
*"the result must equal `0x1234` at this address after this stimulus"* is prose and
is checkable. What is missing when a criterion fails is usually **inputs, domain,
or oracle**.

| missing element | the accidental pass it allows |
|---|---|
| exact claim and scope | a local ABI result becomes "guest progress" |
| evidence eligibility | an exploratory artifact satisfies a strict criterion |
| required exercise of the behavior | no error appears because the target never ran |
| oracle independent of the implementation | a generated answer compared with itself |
| input population and coverage | "all entries" means whichever ones were enumerated |
| expected failure behavior | missing metadata becomes an empty set, hence success |
| stable criterion identity | a reviewer evaluates a paraphrase, not the obligation |
| scope boundary | an unrelated next defect becomes a new requirement |
| tool readiness | the executor invents a parser and trusts it untested |

**Split these four obligations — they are different claims with different admissible
evidence, and conflating them is what let A2 be "accepted" on a run that could not
carry the claim:**

1. **Structural correction** — the intended branch targets and bodies are emitted.
2. **Exercised ABI behavior** — the target actually executes and satisfies the
   specified register/stack checks. An exploratory run *can* establish this.
3. **Strict integration reachability** — the target is reached under validated
   strict conditions. An exploratory run **cannot**.
4. **Downstream semantics** — the intended modeled effect occurs.

### The rulebook

### Planner rules

The Planner authors proposed packets. The session executes and integrates them;
the designated reviewer checks the evidence. Planner and Advisor may use the
same model, but must be separate children when the plan itself is disputed.
Agreement between them is not model-diverse confirmation.

1. **Start from the user objective.**
   Quote or link the governing requirement. State the packet's bounded claim
   and explicit non-goals. Do not replace an open user question with an assumed
   answer.

2. **Give every criterion a stable ID and revision.**
   One criterion should express one independently decidable obligation.
   Reviewers and status records refer to IDs, not paraphrased summaries.

3. **Specify evidence eligibility before the measurement.**
   Name the allowed run profile, fixture/live scope, source/build identity,
   required instrumentation and permitted overrides. A valid measurement in an
   ineligible artifact does not pass the criterion.

4. **Specify the procedure completely enough to reproduce.**
   Give working directory, tools, inputs, command/test identifiers, environment,
   bounds and artifact destination. Define how any run ID is obtained.
   Commands that do not exist are labeled TOOLING REQUIRED, not presented as ready.

5. **Define the decision rule.**
   Name exact fields/observations and expected values, relations or tolerances.
   Define PASS, FAIL and BLOCKED/UNKNOWN. Missing, malformed, stale, truncated or
   unexercised evidence never becomes PASS through an empty set or default value.

6. **Prove the target was exercised.**
   An absence claim requires an exposure/coverage witness and an observation
   completeness check. "No failures" is insufficient when the path never ran.

7. **Validate the checker and the oracle.**
   Include a known-good control and a known-bad control that the checker rejects.
   For regression checks, demonstrate rejection of the pre-fix defect or a
   representative controlled mutation. Expected answers must not be derived
   solely from the implementation under test.

8. **Separate local behavior from integration claims.**
   Structural correctness, exercised ABI behavior, strict reachability, device
   semantics and liveness are separate obligations. State what each artifact
   does and does not establish.

9. **Bound universal and causal claims.**
   Name the input population, schedules, time/step window and coverage limits.
   To claim causation, isolate the change or use a discriminating intervention.
   Otherwise report association or bounded evidence, not a universal proof.

10. **Freeze scope before implementation.**
    Status writing cannot add, remove or reinterpret criteria. Unrelated downstream
    defects become follow-up findings unless they violate an existing criterion.
    A changed criterion gets a new revision, reason, authorization and affected
    re-review; retain the old failed or unverified disposition.

11. **Make closure mechanical where feasible.**
    Bind each criterion to artifact paths/hashes, checker version and reviewer
    disposition. Packet acceptance requires all mandatory criteria on the same
    declared evidence revision. UNKNOWN and CANNOT VERIFY remain pending.
    Advisor judgment cannot turn a failed measurement into PASS.

12. **Do not hide missing observability inside an implementation packet.**
    If no existing observation can distinguish success from failure, create a
    tooling/fixture prerequisite first. Validate it before using it to accept
    the behavioral change.

Before releasing a plan, perform a counterexample review:
"Could broken behavior, an unexercised path, an exploratory run, stale evidence,
or an empty/malformed input satisfy these checks?"
Revise the contract until each realistic accidental-pass path is addressed.

Prescriptive implementation detail should be proportional to certainty.
When the mechanism is unknown, write an investigation packet with hypotheses,
discriminating experiments and decision branches—not invented implementation steps.

### The packet contract

Every proposed packet uses this shape, so the executor does not have to invent
anything:

```markdown
## <Packet ID> — <bounded outcome>

**Contract revision:** <revision/hash>
**Status:** Proposed
**Owner:** main session
**Governing requirement:** <verbatim requirement or durable link>
**Depends on:** <accepted packet IDs/revisions>
**Baseline:** <game revision + dirty-state identity; toolkit identity; artifact>

### Claim and boundaries
- Establishes: <precise capability or fact>
- Does not establish: <explicit exclusions>
- Non-goals: <work not authorized by this packet>
- Known facts: <MEASURED, citations>
- Assumptions/open questions: <INFERRED; how each is tested>

### Readiness
- Existing tools/tests: <paths, verified invocation>
- Missing tooling: <prerequisite packet, or NONE>
- Required environment/access: <policy-dependent requirements>
- Stop before implementation if: <unmet prerequisites>

### Execution
1. <bounded action, files/interfaces, expected intermediate result>
2. <next action>
- Allowed implementation choices: <executor discretion>
- Escalate/replan if: <specific ambiguity, contradiction or repeated failure>
- Worker scopes, if useful: <bounded tasks and file ownership>
- Build/run owner and serialization: <explicit>

### AC-<ID> — <single claim>
- Mandatory: yes/no
- Evidence class/profile: <fixture/exploratory/strict/manual audit>
- Identity prerequisites: <source/build/checker/input identities>
- Stimulus and coverage: <inputs, target-exercised witness, bounds>
- Procedure:
  - Working directory: <path>
  - Command/test: <exact invocation>
  - Environment: <explicit values/default policy>
- Artifact: <path rule; schema/version; required fields>
- Oracle: <reference basis independent of implementation>
- PASS: <Boolean predicate or bounded manual audit rubric>
- FAIL: <observations contradicting claim>
- BLOCKED/UNKNOWN: <missing access, malformed evidence, insufficient coverage>
- Controls: <good case; bad case; expected rejection>
- Claim limits: <what passing cannot establish>

<Repeat criterion record as needed.>

### Closure
- Required test inventory: <versioned list, not an unexplained suite total>
- Evidence index: <criterion ID -> artifact/hash -> result>
- Reviewer: <role from workflow, never copied model roster>
- Criterion change procedure: <revision and re-review>
- Unrelated next stop: record follow-up; do not expand this packet
- Next packet selection: <decision rule>

### Planner self-check
- Counterexamples attempted: <ways broken behavior might pass>
- Unresolved design decisions: <none, or explicit investigation branches>
- Plan-review disposition: <review record; pending until obtained>
```

**Before releasing a plan, run the counterexample review:** *could broken behavior,
an unexercised path, an exploratory run, stale evidence, or an empty/malformed input
satisfy these checks?* Revise until each realistic accidental-pass path is closed.

**Prescriptive detail must be proportional to certainty.** When the mechanism is
unknown, write an investigation packet with hypotheses, discriminating experiments
and decision branches — not invented implementation steps.

---

## 5. Why this is one file and not four

Four documents previously described this policy — `AGENTS.md`, `grok-role-map.md`,
`deepseek-harness.md` and `.dsh/skills/advisor-escalation/SKILL.md`. On 2026-09-22 the
policy changed twice within an hour (routes added 17:12, the acceptance-review gate
added 18:16) and **five separate claims went stale**, including two that actively
contradicted the new gate: the skill said the advisor was "the only subagent this
session is allowed to spawn", which forbade the reviewer the gate requires, and the
plan's own delegation table listed two routes while its acceptance section demanded a
third. A sixth was authored by the session itself — the readers-only worker rule in
§2.2, which no user instruction ever asked for.

The lesson is not "write more carefully". It is that **a fact copied into several
files is not corroboration** — most of those claims were restatements of the first,
and none were found by reading the file being edited. They were found by sweeping
every document for the claim after the policy changed.

So: **one authority per fact.**

| fact | owner |
|---|---|
| roster, session loop, escalation triggers, advisor contract | **this file** |
| operating knowledge, build/test commands, guest-code discipline | `AGENTS.md` |
| acceptance criteria and statuses | `plan-jsrf-bare-minimum.md` |
| current blocker, evidence revision, next packet | `report-deepseek.md` `CURRENT STATE` |
| what happened and why | `docs/jsrf-operating-history.md`, the reports |

**The skill was retired 2026-09-23 and folded into §4.** It did not earn its place:
its briefing template is short, it is used only by this workflow, and it provided no
independent discovery path across both harnesses — a session had to already know to
load it. **Reintroduce a skill only if it later provides a real capability** (a
validated evidence-brief assembler, required-field checking, multi-project reuse),
and even then the *obligation to escalate* must stay discoverable without loading it.

`AGENTS.md` carries a **short startup pointer**, not a replica of this file. A
duplicated roster is the exact failure mechanism described above, even when labelled
"summary". If a quick-reference table is ever wanted there, **generate it from this
file and test that it stays in sync**.

**When a policy changes, grep every document for the old claim** before considering
the change done — and check the plan and the reports, not just the file you edited.
