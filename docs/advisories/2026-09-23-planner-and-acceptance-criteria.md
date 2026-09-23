# Assessment

**MEASURED** below means direct inspection of the cited file or artifact, not a rerun of the historical experiment. **INFERRED** marks recommendations and interpretations. No files were changed.

## First: Planner and Advisor being the same model

**MEASURED:** The newly supplied instruction assigns both roles to Astra Medium. The workflow I read still has only session, worker, advisor and reviewer rows (`docs/agent-workflow.md:45-50`).

**INFERRED:** This is acceptable, but **the advisor is not model-diverse relative to the plan author**. Separate contexts reduce commitment to a particular argument; they do not remove correlated reasoning errors.

Use these safeguards without changing the authorized roster:

- **Separate Planner and Advisor children.** Do not continue the Planner under an “Advisor” label when its own plan is disputed.
- Give the Advisor the user’s objective, disputed criterion, both positions and evidence—not merely the Planner’s rationale.
- Have the designated reviewer inspect **plan adequacy before execution**: could an implementation satisfy these checks while failing the objective?
- Distinguish that plan review from the later review of implementation evidence.
- A criterion change receives a new revision and review. Neither the Planner nor Advisor may make a failed historical check pass by rewriting its meaning.
- If the same-model Advisor cannot resolve the dispute with a discriminating measurement, preserve the uncertainty. Another confident endorsement is not new evidence.

**INFERRED:** The Planner should author proposed packets; the main session still owns integration into the active plan and execution. Planning authority is not acceptance authority.

# A. Why plans fail to be followable

**INFERRED:** Your recurring structural defect is an incomplete chain:

> **Claim → admissible evidence → reproducible procedure → decision rule → recorded disposition**

The executor repeatedly has to invent one of those links.

Prose is not inherently defective. “The result must equal `0x1234` at this address after this stimulus” is prose and is checkable. Conversely, a command returning zero can be a poor acceptance check.

A fresh executor needs to know:

| Missing element | Typical accidental “pass” |
|---|---|
| Exact claim and its scope | A local ABI result becomes “guest progress.” |
| Evidence eligibility | An exploratory artifact satisfies a strict criterion. |
| Required exercise of the behavior | No error appears because the target was never reached. |
| Oracle independent of the implementation | A generated answer is compared with itself. |
| Input population and coverage | “All entries” means whichever entries the script happened to enumerate. |
| Expected failure behavior | Missing metadata becomes an empty set, hence success. |
| Stable criterion identity | A reviewer evaluates a shortened paraphrase instead of the original obligation. |
| Scope boundary | An unrelated next defect becomes a new acceptance requirement. |
| Tool readiness | The executor invents a parser and trusts it without testing its negative cases. |

**INFERRED:** A plan cannot eliminate implementation judgment. It should eliminate the need to invent **what success means**. Where the correct implementation is unknown, specify a bounded investigation and its decision outputs—not a fictional implementation recipe.

# B. Where your criteria went wrong

## Verification of the three claims

### 1. The four cited artifacts are not strict: confirmed; the historical criterion wording needs qualification

**MEASURED:** All four files contain the two overrides at lines 25–31:

- `logs/runs/20260922-181157-372-a2e-252b5-span/metadata.json`
- `logs/runs/20260922-190336-778-a2f-7e255-span/metadata.json`
- `logs/runs/20260922-220505-613-a2g-304f0-span/metadata.json`
- `logs/runs/20260922-224429-003-a2g-304f0-span/metadata.json`

**MEASURED:** The profile document classifies them as synthetic completion (`docs/jsrf-run-profiles.md:36-43`).

**MEASURED:** A2g explicitly requires:

> “a strict 30 s run is archived and compared against the A2f baseline; and the run’s next stop is named.”  
> — `plan-jsrf-bare-minimum.md:782-786`

**INFERRED:** This artifact fails that profile requirement. However, “nothing made it checkable” is too strong: the metadata already made it manually checkable. What was missing was an explicit artifact-field check, expected outcome and enforced gate.

**MEASURED:** A2f preserves “All 7 criteria AGREED” but does not preserve the seven criteria in its section (`plan:720-743`). A2e’s review table is in the report rather than a stable packet contract (`report-deepseek.md:1153` onward).

**INFERRED:** I can confirm false strict labels for all four artifacts. I cannot reconstruct each packet’s exact original seven/six-criterion obligation solely from the present plan. Recover those original contracts rather than silently supplying a retrospective one. **Losing the criterion set while retaining ACCEPTED is itself a major defect.**

### 2. A2 acceptance scope drifted: confirmed

**MEASURED — actual criterion:**

> “Archive an identity-verified bounded guest run with profile, event/interrupt evidence and next stop.”  
> — `plan:552-553`

**MEASURED — added requirement in status:**

> “the packet also requires the archived run’s next stop to be this packet’s own”  
> — `plan:525-527`

**INFERRED:** The status invented an obligation absent from that criterion. The criterion needed a disposition rule: an unrelated later stop is recorded as a follow-up, not automatically a failure of this packet. Other A2 criteria could still fail; that requires identifying the particular criterion, not adding one during narration.

### 3. An exploratory result can support a narrower claim: confirmed, with limits

**MEASURED:** A2g reports ABI failures dropping to zero and other execution counts (`plan:761-765`), while the profile policy prohibits exploratory evidence from establishing boot/audio/GPU/liveness acceptance (`docs/jsrf-run-profiles.md:27-32`).

**INFERRED:** A trustworthy “returned; ABI verified” observation can establish that **this exercised invocation satisfied the instrumented ABI checks under this recorded configuration**. It does not establish all paths, correct function semantics, absence of instrumentation exemptions, or strict integration reachability.

Split the obligations:

1. **Structural correction:** the intended branch targets and bodies are emitted.
2. **Exercised ABI behavior:** the target actually executes and satisfies specified register/stack checks.
3. **Strict integration reachability:** the target is reached under validated strict conditions.
4. **Downstream semantics:** the intended modeled effect occurs.

These are different claims with different admissible evidence.

## Additional examples from the actual plan

| MEASURED wording | INFERRED missing contract |
|---|---|
| P0.1: “Tests include…” (`plan:61-63`) | Case IDs, expected classifications, assertion requirements and failure-path coverage. Merely adding named tests can satisfy the wording. |
| P0.3: “Set a conservative budget below 65,536 bytes…” (`plan`, P0.3 acceptance) | Exact byte limit, counted files, encoding and checker invocation. |
| P0.7: “explicitly enumerated permitted differences” (`plan`, P0.7 acceptance) | The enumeration itself. The executor can create the allowlist after seeing unwanted drift. |
| A2: “the intended waiter makes observable progress” (`plan:549-550`) | Which waiter, which state transition, causal linkage, time/step bound and evidence fields. |
| A3b: “an independently specified fixture verifies the observable effect … including negative cases” (`plan:882-885`) | Selected action, input vectors, reference oracle, exact expected effects and named negative cases. |
| A3c: “completion cannot outrun modeled execution” (`plan:887-889`) | Observable ordering relation and controlled schedule. Finite tests cannot establish an unqualified universal claim. |
| A4b: “each remaining edge is represented correctly, proven unreachable … or produces a deterministic diagnostic” (`plan:920-925`) | A versioned edge inventory and per-edge disposition. A detector omitting an edge must not make the claim vacuously true. |
| A4c: “a same-profile bounded run is compared” (`plan:928-931`) | What must remain invariant, what differences are allowed and which differences fail acceptance. Performing a comparison is not an outcome. |
| A5: “resolve high-confidence correctness findings” (`plan:944-946`) | The finding inventory, confidence definition and permitted closure dispositions. |

**INFERRED:** My earlier P0 proposal shares this weakness. It is a useful decomposition, **not yet a fully executable acceptance contract**. It needs the same hardening as A1–A5; do not exempt it because an advisor wrote it.

# C. Pasteable Planner rulebook

The following is **INFERRED proposed policy**, not a claim about already implemented behavior.

```markdown
## Planner rules: acceptance criteria are evidence contracts

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
```

# D. Planner output contract

**INFERRED:** Use a compact packet with expandable criterion records, not an enormous table whose cells omit essential detail.

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

# Hardening the current plan

## Method

**INFERRED:**

1. **Snapshot the existing contract and statuses.** Do not rewrite historical obligations in place and leave ACCEPTED unchanged.
2. **Inventory every active acceptance sentence.** Give it an ID and separate bundled obligations.
3. **Recover missing original criteria**, especially A2f’s seven-item contract. Unrecoverable means provenance incomplete—not permission to invent the old requirement.
4. For each criterion, fill: claim, admissible evidence, procedure, oracle, exposure witness, failure/unknown handling, artifact identity and disposition.
5. Assign a readiness class:
   - **READY:** existing reproducible check;
   - **MANUAL AUDIT:** bounded rubric and finite evidence inventory;
   - **TOOLING REQUIRED:** missing observation/checker;
   - **DISCOVERY REQUIRED:** target behavior or oracle not yet known.
6. **Run a counterexample review before implementation.** Test the contract against stale files, empty inventories, unreached targets, synthetic overrides and known-bad examples.
7. **Validate new checkers before relying on them.**
8. Migrate packet status **criterion by criterion**. Preserve local accepted evidence; mark the unsupported strict-integration obligations pending. Do not globally bless or globally discard a packet.
9. Release only the next executable packet. A3b, for example, needs action selection and contract discovery before an implementation packet can contain exact test vectors.

## Criteria requiring tooling or new observability

**INFERRED:** These are gaps in the inspected plan/tooling, not a claim that every repository file has been exhaustively searched.

| Criterion family | Required addition |
|---|---|
| **P0.1 strict runtime eligibility** | Effective-setting classifier and launch gate, including defaults and malformed-input handling. |
| **P0.2 durable review closure** | Exact packet/criterion/revision-to-review records and validation. |
| **P0.3 budget and policy consistency** | Explicit byte threshold and bounded document checker; semantic contradictions still need review. |
| **P0.4 capture vs content integrity** | Distinct validation outcomes and adversarial dump fixtures. |
| **P0.7/A4c regeneration preservation** | Versioned input/output manifest, protected-instrumentation checks and predeclared drift rules. |
| **A2 intended waiter progress** | Correlated producer/signal/waiter evidence with identified thread/object and observable post-wait behavior; generic counts alone are insufficient. |
| **A3b action semantics** | Action-specific fixture and independently specified oracle after selecting the action. |
| **A3c completion ordering** | Controlled execution/completion observations, such as a paused execution test proving completion remains pending. |
| **A4b every missing edge** | Complete versioned edge inventory and per-edge disposition checker, plus taken-edge fixtures. |
| **A5 reconciliation** | Criterion-to-evidence ledger; suite totals cannot perform this reconciliation. |

**MEASURED:** The current profile checker still classifies by variable **presence**, not enabled value (`scripts/check-run-profile.py:74-77`); it drops malformed entries and collapses duplicate names (`:67-71`), and skips named missing metadata without failure (`:104-106,135`).

**INFERRED:** Therefore it is not yet a strictness oracle. The four cited artifacts remain clearly exploratory because their values explicitly enable the overrides. But the archive-wide **247 should presently be described as flagged runs**, not automatically as a validated count of effective exploratory configurations.

**Bottom line — INFERRED:** Add the Planner, but do not make the project depend on the Planner writing more authoritative prose. Require it to deliver **versioned, falsifiable evidence contracts**, with tooling gaps exposed before the executor begins.
