# Agent workflow: roles, authority, packet lifecycle, and escalation

This file is the **single authority** for JSRF agent staffing and workflow.
`AGENTS.md` points here but does not duplicate provider, model, or effort assignments.

**Design goal:** cheap models execute precisely; senior models exercise judgment;
evidence stays trustworthy; technical questions end at the Advisor; the project
keeps moving. When a literal reading of this file defeats that goal, the Advisor
decides what the rule is for (§2.3).

**Staffing rule:** the roster table in §1 is the only persisted staffing policy.
Other files and later sections refer to roles only. If staffing changes, edit §1
first and sweep the repository for stale copied assignments.

## 0. Startup checks

Complete these in every new top-level session before implementing a game packet.
A previous session's child IDs or PASS results do not establish readiness.

1. **Load current authority.** Read this file, the `CURRENT PACKET` block of
   `plan-jsrf-bare-minimum.md`, and `docs/jsrf-run-profiles.md`. Record both
   repository identities and dirty files. Nothing else selects work.
2. **Verify routes.** Compare the running Session with its §1 row from harness
   metadata (record `UNKNOWN` if unverifiable). Resolve every other role live (§1).
3. **Probe the Acceptance reviewer** — both stages.
   Invoke each at its listed effort. PASS requires a completed response containing a
   fresh session token and one reason an empty evidence set must fail acceptance.
   Dispatch alone is not PASS.
4. **Probe the Advisor (one combined probe).** Spawn it as a fresh continuable child.
   In the first turn give it a unique marker and ask it to read one named repository
   file and report a fact deliberately left out of the brief. In a second turn to the
   **same child**, ask for the marker without repeating it. PASS requires the correct
   fact (checked against the file) and the correct marker from the same child.
   This proves both continuity and that the Advisor can read project evidence.
5. **Reconcile packet state.** Inspect both working trees and preserve unrelated
   edits. Execute **only** the exact packet/revision named in the plan's
   `CURRENT PACKET` block. Never discover work by scanning for pending items. If no
   packet is current, implementation is `BLOCKED` until one is promoted (§5).
6. **Persist the receipt.** Fill `docs/session-start-template.md`, save it as
   `docs/reviews/startup-<date>-<session-id>.md`, and link it from the active packet
   or review record.

A required route, effort, spawn, or continuation that is unavailable makes startup
`BLOCKED` for accepted game work. Do not substitute an unlisted route. The Planner
needs no separate probe: every adequacy review it returns must cite the files and
line ranges it read, which is checked on first use.

## 1. Supported harnesses and roster

Exactly two harnesses are supported. Use only the assignments in the active column.

| Role | Codex | DeepSeek Harness (DSH) |
|---|---|---|
| **Session** | `gpt-6-luna` @ `max` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Worker subagents** | `gpt-6-luna` @ `max` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Planner** | `gpt-6-astra` @ `medium` | **Claude Opus 5.5** @ `high` (`route: LIVE_RESOLVE`) |
| **Persistent advisor** | `gpt-6-astra` @ `medium` | **Claude Opus 5.5** @ `high` (`route: LIVE_RESOLVE`) |
| **Acceptance reviewer** | `gpt-6-luna` @ `max` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Acceptance reviewer — second stage** | `gpt-6-astra` @ `low` | **Claude Opus 5.5** @ `medium` (`route: LIVE_RESOLVE`) |

The second stage runs **only** when the first-stage review does not return `ACCEPT`
(§2.2). A first-stage `ACCEPT` is final and is not passed on.

**Authority attaches to the role, not the model.** An Opus Acceptance reviewer is a
contract role and is bound exactly like a Luna reviewer. An Opus Advisor has Advisor
authority. One child holds one role per decision: a child that reviewed a packet's
acceptance does not also rule as Advisor on a dispute about that review.

### Live verification

- DSH: resolve roles with `list_subagent_models` and verify any listed effort. A row
  marked `LIVE_RESOLVE` requires **exactly one** advertised route whose canonical model
  identity is the named model and which supports the required effort. Record the
  returned provider/model string. Zero or multiple matches is `BLOCKED` until §1 or the
  route ambiguity is repaired. Never invent an identifier from a display name.
- Codex: verify route and effort through current harness metadata.
- If a row omits effort, omit `reasoning_effort`.
- An unavailable assignment is `BLOCKED`; never fall back silently.

## 2. Role authority

### 2.1 Two layers

| Layer | Roles | Produces | Bound by |
|---|---|---|---|
| **Judgment** | Persistent advisor (senior), Planner | rulings, packet design, adequacy verdicts, deferrals, stops, exceptions | facts (§2.4), the owner's objective and reserved decisions (§3.4) |
| **Contract** | Session, Worker subagents, Acceptance reviewer | executed steps, evidence, dispositions | the frozen packet, this file, and recorded rulings |

**Rank settles judgment; evidence settles facts.** The Advisor may overrule the Planner
on method, materiality, or scope, and may overrule any contract role's interpretation.
No role, including the Advisor, can overrule a reproduced measurement by authority; it
can only order a new or better measurement.

### 2.2 Contract roles — execute, do not reinterpret

These rules are hard. When the contract is silent, ambiguous, or appears wrong, a
contract role **stops that line of work and escalates** (§4); it never resolves the
question itself.

**All contract roles**

1. Work only on the exact packet/revision named in `CURRENT PACKET`; verify its hash.
2. Run commands exactly as written. A step that cannot be performed as written is a
   blocker to escalate, not something to improvise around.
3. Stay inside the declared write scope. Preserve unrelated edits in both trees.
4. Never edit a frozen packet, a pinned file, or an artifact under review while that
   revision is frozen or its review is running.
5. Never invent evidence. Every result is `PASS`, `FAIL`, or `UNKNOWN`/`BLOCKED` by
   the packet's own decision rule; missing, malformed, stale, or empty evidence is
   never PASS.
6. Never change policy, criteria, thresholds, or scope, and never classify a review
   finding as blocking or advisory. Those are judgment-layer decisions.
7. Never claim a fix without measuring it. A report that something was fixed names
   the command or artifact that shows it, or says `UNVERIFIED`.
8. Do not write narrative about your own repairs into operative documents.
9. An explicit stop boundary — from the owner, the Advisor, the Planner, or a
   packet's `Stop if` — is hard: stop, record the state, report. Do not continue past
   it on your own judgment.
10. Write each review record to disk, from the reviewer's own response, **before**
    starting the next revision or promoting.

**Session** — owns integration, build, run, evidence collection, and record keeping.
It drafts the mechanical parts of a packet (commands, paths, hashes, environment) and
verifies that every command in a draft actually runs before submitting it for review.
It does not author criteria or decision rows on its own authority (§5.1), and while
the Planner works it keeps the brief frozen (§5.1). It does not
write causal or historical claims that a decision will rely on; it supplies artifacts
and lets the Planner or Advisor draw the conclusion.

**Worker subagents** — used for context isolation (summarize a large log or dump) or
bounded implementation (explicit files, contract, and check). Workers keep disjoint
write scopes, never touch policy documents, stop at ambiguity or at the edge of their
scope, and return `file:line` or artifact evidence marked **MEASURED** or
**INFERRED**. A worker never owns final integration, build, or run.

**Acceptance reviewer** — checks delivered evidence against the **frozen contract
only**:

- For each mandatory criterion return `AGREED`, `DISAGREED`, or `CANNOT VERIFY`, with
  the reproducing command or `file:line` evidence. Reproduce load-bearing measurements;
  absence checks need a positive control. Try to falsify.
- Overall disposition: `ACCEPT` only when every mandatory criterion is `AGREED`;
  otherwise `NOT ACCEPTED`, naming the blocking criteria.
- An ambiguous criterion is `CANNOT VERIFY` plus an escalation to the Advisor, never a
  private reinterpretation.
- Concerns outside the contract are listed separately as advisories. They do not
  change the disposition and do not add criteria.

**Two-stage acceptance:**

1. The first-stage reviewer reviews every mandatory criterion as above.
2. If it returns `ACCEPT`, acceptance is final. Do **not** pass it to the second stage.
3. If it returns `NOT ACCEPTED`, the Session records that review, then sends the
   frozen contract, the evidence, and the first-stage record to a fresh
   second-stage reviewer. It re-reviews **only the criteria the first stage did not
   `AGREE`**. The first-stage findings are leads: for each one it reproduces the
   measurement and returns its own `AGREED`, `DISAGREED`, or `CANNOT VERIFY`.
4. The second stage is the final reviewer disposition. `ACCEPT` requires every
   mandatory criterion `AGREED`, whether in the first stage or the second.
5. The second stage is still a contract role, bound exactly like the first. If the
   Session disputes its result, that goes to the Advisor as usual.

A first-stage route failure is `pending — reviewer unavailable`, not a failed review;
it is repaired, not passed to the second stage.

### 2.3 Judgment roles — decide

Senior discretion is **procedural, not factual**. Judgment roles decide what matters,
what is enough, and what to do next; they do not decide what happened (§2.4).

**Planner** — owns packet design and adequacy. It may, on its own authority:

- write or rewrite a packet's claim, criteria, and decision rows, and hand the Session
  a rewrite instead of a defect list;
- stop investigating as soon as the planning test in §5.1 is met, leave the remaining
  unknowns to the packet, and choose a discovery packet (§5.8) when the next
  implementation depends on facts nobody has observed;
- simplify an over-engineered packet and delete requirements that protect nothing;
- waive a drafting-checklist item (§6.1) with a one-line reason;
- classify every finding it raises as blocking or advisory (§3.1–3.2);
- accept bounded uncertainty that cannot change the decision;
- redesign a failing evidence strategy instead of patching it again (§5.5);
- stop revision churn and declare that enough evidence exists to proceed;
- recommend retiring a packet whose premise has failed.

**Persistent advisor** — the project's senior technical decision-maker. Everything
the Planner may do, plus:

- resolve any technical question: architecture, device/emulator semantics,
  reverse-engineering method, evidence admissibility, contract interpretation;
- define missing technical or evidence policy (§3.3);
- resolve disagreements among Session, Planner, and reviewer — its ruling is final
  within the limits of §2.4 and §3.4; the Planner may record dissent, but dissent does
  not block;
- override a literal reading of a workflow rule when that reading defeats the rule's
  purpose, stating the purpose and the override;
- decide whether an uncertainty is material and whether an advisory is worth acting
  on; reclassify any finding in either direction;
- stop an unproductive review or revision loop; tell the Planner or reviewer that a
  finding is non-blocking and deferred;
- approve reasonable technical exceptions (process only — never evidence, §2.4);
- tell the Session to proceed when further process adds no meaningful confidence;
- change methodology when repeated failures show the current method is the wrong shape.

The Advisor does not need the owner for any of this. It escalates only a reserved
owner decision (§3.4).

**Accountability instead of gates.** Every discretionary call — defer, waive,
simplify, stop, accept uncertainty, grant an exception, override a rule — is recorded
in one line in the relevant review record: *decision; reason; what would reverse it.*
The call is valid when made; the record makes it auditable afterwards.

### 2.4 Evidence invariants — hard for every role

1. **Observed, inferred, uncertain.** Every load-bearing claim in a ruling, verdict,
   or report is labelled as something the author observed itself (read or ran),
   something inferred, or something uncertain. Never present an inference or a brief's
   claim as observed.
2. **Briefs are leads.** Another agent's summary is not evidence. When a fact would
   decide the outcome and is disputed, surprising, or cheap to check, look at it
   directly. The senior role decides which facts are material and need not re-read
   sources that cannot change the decision.
3. **Failure stays failure.** No ruling turns a failed measurement into PASS, replaces
   missing evidence, converts UNKNOWN into success, or waives a criterion after the
   fact. A judgment role may rule a criterion ill-formed; that sends the packet back
   for revision and a re-measurement, it does not pass the criterion.
4. **Profiles are fixed in advance.** Exploratory or fixture evidence never satisfies a
   strict criterion. A rule that admits a class of evidence is general, prospective,
   and recorded in its owning document before any evidence relies on it.
   `docs/jsrf-run-profiles.md` owns the profile rules.
5. **Absence needs coverage.** Zero hits prove absence only with a positive witness
   that the target would have been observable.
6. **The view is not the artifact.** Truncated, paginated, or summarized tool output
   is not the file. Before a load-bearing count, negative, or provenance claim, account
   for truncation and blank-line handling, and read the underlying range when the view
   may be lossy. When in doubt, the result is UNKNOWN.
7. **Reviews bind to bytes.** A review covers only the exact revision and evidence it
   saw. A post-review edit reopens the affected criteria.
8. **Exceptions relax process, never evidence.**

## 3. Definitions

### 3.1 Blocking defect

A defect in the **operative contract** — criteria, procedure and commands, decision
rows, pins, write scope, stop conditions, or any text an executor acts on — such that
a faithful, literal execution can plausibly produce:

- a false PASS, false FAIL, or false UNKNOWN/BLOCKED;
- a wrong implementation;
- a wrong decision-row classification or misattribution;
- a wrong evidence binding; or
- unsafe or destructive execution.

A blocking finding must state a **concrete failure scenario**: these inputs or this
state lead to that wrong outcome. Without a scenario it is an advisory. The test is the
consequence, not where the text sits: a false sentence in a step note that would lead an
executor to break a step is blocking; a wrong count in a history note is not.
Plausibility is a judgment call for the Planner or Advisor (for example, an attack the
real toolchain cannot emit is not plausible).

### 3.2 Advisory

Every other finding: wording, formatting, narrative or history accuracy, record
pointers, incomplete cross-references, stronger-than-needed controls, and hypothetical
attacks outside the packet's stated trust boundary. Advisories are listed in the review
record's **Deferred** section. They never reopen a frozen packet. Correcting a
non-packet record (history log, review pointer) needs no re-review.

### 3.3 Technical-policy ruling

An Advisor decision that settles a technical question the project documents leave open
or ambiguous — evidence admissibility, methodology, device semantics, interpretation of
a criterion, a dispute, a loop stop, an exception. It binds every role from the moment
it is recorded and states:

- the question and the decision;
- observed / inferred / uncertain basis (§2.4.1);
- what would reverse it;
- where it is recorded.

General rules are recorded in the document that owns the topic (§8); case rulings go in
the review record. The Session records the ruling text verbatim with the Advisor child
ID and route; a paraphrase cites that record. An attribution that cannot be traced to a
recorded Advisor response is `UNKNOWN`.

### 3.4 Owner decision

Reserved for the human owner. Ask only when a decision changes or requires:

- the project objective, product scope, or what "bare minimum" / the playable slice means;
- an intentional fidelity/functionality tradeoff that changes that objective;
- staffing or model assignments in §1;
- destructive or difficult-to-reverse repository or asset actions;
- credentials, purchases, external spending, legal or licensing choices;
- anything the owner explicitly marked `OWNER REQUIRED`;
- an Advisor ruling that would contradict an explicit owner instruction.

Do **not** ask the owner because policy is incomplete, a technical threshold must be
chosen, several technically valid approaches exist, agents disagree, or this file has no
rule for the situation. That is what the Advisor is for.

## 4. Escalation

### 4.1 Ladder

```text
Worker   -> Session
Session  -> Planner   (packet design, adequacy, whether to revise)
Session  -> Advisor   (any technical question, including how to read a frozen step)
Reviewer -> Advisor   (ambiguous criterion; Session/reviewer disagreement)
Planner  -> Advisor   (policy gap, methodology change, anything beyond Planner authority)
Advisor  -> Owner     (§3.4 only; everything else ends at the Advisor)
```

If the Advisor is unavailable, that is a staffing blocker; the Planner still decides
within its own authority.

**The owner may message any agent directly**, including a child. Such a message is an
owner instruction, not an injection: follow it, record it, and tell the Session if it
changes scope. It outranks every role's ruling.

### 4.2 When a contract role escalates

- the same root-cause failure survives **two attempts**, or each next step is a guess;
- two credible measurements contradict each other;
- about to rely on a universal or negative claim (`impossible`, `rules out`, `nothing found`);
- an expensive build, regeneration, or investigation rests on an unverified premise;
- a criterion or step is ambiguous, contradicted, or cannot be executed as written;
- the work would need a policy, criterion, scope, or threshold change;
- Session and reviewer disagree.

### 4.3 Briefing the Advisor

Give the smallest `READ YOURSELF` set that could change the decision, and one bounded
question. Use the right consult:

- **Fault diagnosis:** rank plausible mechanisms; give the cheapest discriminating
  experiment for each.
- **Architecture/policy:** options, trade-offs, decision, what would reverse it, and
  the document that owns the rule.

```text
You are the Persistent advisor (docs/agent-workflow.md §2.3) on a static-recompilation
project. Repository: <path>. Toolkit: <path>.

READ YOURSELF (sources that could change the decision):
- <file/artifact + range + why it matters>

SESSION CLAIMS (leads, not evidence):
- <claim> [pointer]

TRIED / MEASURED:
- <hypothesis> -> <prediction> vs <measurement> [artifact]

QUESTION:
- <one bounded question>

Return:
RULING: <decision>
BASIS: observed / inferred / uncertain, for load-bearing claims only
REVERSED BY: <evidence that would change it>
RECORD IN: <owning document or review record>
```

Follow-ups may be delta briefs. A delta does not freeze earlier premises: if a
correction changes a load-bearing premise, mark it `PREMISE_CHANGED` and ask the Advisor
to reconsider every ruling that depended on it. Never instruct the Advisor not to
revisit a premise.

### 4.4 Advisor continuity

Reuse the startup-probed Advisor child for the whole top-level session unless its
state becomes unreliable; a new session creates and probes its own. Do not seed it with
the current conversation. DSH: spawn with `run_in_background: true`, keep the child ID,
continue with `send_message`. Codex: resolve from §1, start from a self-contained brief,
and resume the same task through the harness continuation mechanism. If the route or
schema is unavailable, startup is `BLOCKED`; do not invent an invocation.

## 5. Packet lifecycle

### 5.1 Planning and authorship

Planning decides **what to find out or build next**. It does not find it out. The
Planner is not required to solve a technical problem before designing the packet that
investigates it.

**1. The brief is frozen.** The Session opens planning with one evidence brief: the
blocker, the runs and artifacts that show it, and anything already measured. It may
gather that evidence first, including bounded diagnostic runs. Once the brief is sent,
the Session sends the Planner nothing new unless it **refutes the brief's premise**;
other findings wait for the packet. Policy edits also wait, unless the Planner asks for
one. A planner working against a brief that changes every few minutes cannot finish.

**2. Planning is done when three things can be stated:**

- the blocker, from existing evidence;
- the unknowns that would change what to build;
- the cheapest experiment that tells those unknowns apart.

At that point the Planner writes the packet, and the remaining unknowns become its
subject. If the evidence cannot yet specify an implementation safely, that is not a
planning failure: the packet is a discovery packet (§5.8) that obtains the missing
facts.

**3. The Planner only reads.** It reads existing source, docs, disassembly and archived
runs. Running the guest, building, writing tools or scripts, fetching external source,
and multi-step analysis of dumps or binaries are execution: they belong inside a packet.
A question that needs outside knowledge goes to the Advisor as one bounded question.
The one file the Planner writes is its own draft packet.

**4. Sketch first, with escalating checkpoints.** The Planner's goal is a sketch of at
most about 15 lines: claim, class (discovery or change), unknowns, the experiment, and
the outcome rows. It writes the sketch at the top of its draft packet file, under a
`Sketch` heading, where it survives compaction and the owner can read it. The full
packet then replaces the sketch without further investigation. The sketch goes to the
Advisor at the checkpoints below, never to the Session, which does not judge or answer
it.

Checkpoints count **tool calls, not reasoning**. The Planner should think as long as
the decision needs; the checkpoints limit investigation. Each one is heavier than the
last:

At every checkpoint the Planner writes its **current sketch** into the draft, however
rough, with unknowns stated as unknowns. It also writes a **forecast** if it continues:
the specific reads it will make next, and what it expects them to change in the sketch
(class, claim, an unknown resolved, an outcome row). From 40 calls on it writes a
**yield** as well: what the last 20 calls actually changed, compared with the forecast.

| At | The Planner | The Advisor |
|---|---|---|
| **20 calls** | writes sketch 1; either writes the packet, or writes a forecast and continues | not involved |
| **40 calls** | writes sketch 2, the yield against the 20-call forecast, and a new forecast; sends all three to the Advisor, then continues unless redirected | may answer, redirect, or stay silent |
| **60 calls** | stops investigating; writes sketch 3 and the yield against the 40-call forecast; sends both, plus what is still missing, to the Advisor | must decide: extend planning, naming the reads allowed, or have the Planner write the packet with the gaps as its subject |

The yield is what tells the Advisor whether investigation is still paying. If a
20-call stretch changed nothing that matters in the sketch, that is the signal to stop,
and the Planner should say so rather than wait to be told. The Planner messages the
Advisor directly when the harness allows; otherwise the Session relays the message
unchanged.

Every checkpoint decision is written into the draft, so the trail shows why planning
ran long. A Planner that reaches 40 or 60 is a signal to the owner and the Advisor, not
a failure by itself. The budget counts tool calls because models do not see wall-clock
time.

**5. Authorship.**

1. The **Planner** states the bounded claim and non-goals and owns the criteria and
   decision rows. It may write them itself or accept a Session draft.
2. The **Session** fills the mechanical parts (commands, paths, hashes, environment)
   and verifies that every command runs before submitting the revision.
3. **Change packets:** adequacy review is by a Planner child. If that child wrote or
   materially rewrote the criteria or rows of the revision, the review goes to a
   **fresh** Planner child. **Discovery packets:** the writing Planner reviews its own
   packet against §5.3's two questions; no second Planner is spawned. Either way,
   independence in the end comes from the Acceptance reviewer reproducing the evidence.

### 5.2 States

| State | Meaning | Exit |
|---|---|---|
| **draft** | being written | submitted for adequacy review |
| **INADEQUATE** | review found ≥1 blocking defect | revision (§5.4) or retirement |
| **ADEQUATE** | review found zero blocking defects | **frozen and promoted in the same step** |
| **promoted** | frozen hash in `CURRENT PACKET` | Session executes |
| **delivered** | Session says criteria pass with evidence | Acceptance review |
| **second-stage review** | first-stage reviewer returned `NOT ACCEPTED` | second-stage disposition (§2.2) |
| **accepted** | final reviewer stage returned `ACCEPT` | record; plan names next work |
| **pending — CANNOT VERIFY** | reviewer cannot reproduce a criterion | new evidence, or Advisor ruling on interpretation |
| **pending — reviewer unavailable** | reviewer route failed | repair route; no substitution |
| **pending — post-review edits** | reviewed tree/evidence changed | re-review affected criteria |
| **escalated** | disagreement or ambiguity | Advisor ruling, then rerun/re-review as needed |
| **retired** | premise failed or objective changed | recorded in the plan |
| **exploratory evidence** | artifact used bypass/synthetic completion | cannot satisfy strict criteria |

### 5.3 Adequacy review

The Planner reviews the packet itself, not the Session's description of it, and returns:

```text
REVISION:          <packet revision + SHA-256 it read>
READ:              <files/commits/runs and ranges read directly>
PREMISE_FRESHNESS: PASS | BOUNDED | FAIL   (§5.6)
BLOCKING:          <each: location; failure scenario; required outcome>  | NONE
DEFERRED:          <advisories>  | NONE
DECISIONS:         <one line each: decision; reason; what would reverse it>
VERDICT:           ADEQUATE | INADEQUATE
```

`VERDICT` is `ADEQUATE` exactly when `BLOCKING` is `NONE` and `PREMISE_FRESHNESS` is not
`FAIL`. There are no other counts or conditions: the number of advisories or prose
defects has no effect on the verdict.

For a **discovery packet** (§5.8) the writing Planner reviews its own packet, and the
review asks only two questions: could an outcome be misread into the wrong row, and is
the packet safe and reversible? A blocking defect is one that answers either question
badly. The Planner returns the same block, with `READ` naming what it read and
`VERDICT` its answer; the Session confirms every command runs, records the block, and
freezes and promotes on ADEQUATE as for any packet. A misread outcome row is caught
again at acceptance, where the reviewer checks the row selection.

**ADEQUATE ends plan iteration.** The Session freezes that exact revision, records the
review, and promotes it into `CURRENT PACKET` in the same step, with no discretion to
revise first. Deferred advisories stay deferred.

### 5.4 Revising

**After INADEQUATE:** the revision repairs the blocking defects. It may also fix cheap
operative advisories, but adds no narrative about the repairs. Re-review covers the
blocking defects, the changed regions, and a contract-completeness check; the blocking
test still applies anywhere, but advisory hunting in unchanged text is not requested.

**After ADEQUATE**, a frozen packet may be revised **only** when a Planner or Advisor
authorizes it, recording one of:

1. a finding from any source, including execution, meets the blocking test (§3.1) with
   a concrete scenario;
2. a load-bearing premise is invalidated by new evidence (`PREMISE_CHANGED`);
3. an Advisor policy ruling changes a policy the packet depends on;
4. the owner changes the objective or scope.

Nothing else reopens a frozen packet: not advisories, wording, history, record pointers,
narrative accuracy, formatting, a reviewer's idea for another control, or a wish to
clear findings. A step that is ambiguous but not wrong is settled by an Advisor
interpretation ruling that execution follows, not by a revision.

### 5.5 Churn and redesign

When two consecutive INADEQUATE verdicts have blocking defects in the same criterion or
mechanism, the method is probably the wrong shape. The Planner must then **redesign**
that criterion (change what it measures, split it, or delete it) or take the
methodology to the Advisor. A third patch of the same shape is not allowed. The Planner
or Advisor may also stop a loop at any time by recording the decision (§2.3).

**A criterion whose evaluation is itself an analysis belongs in a discovery packet.** If
deciding a criterion requires multi-step static or dynamic analysis of a binary or a run —
work the Planner cannot complete by reading — then the criterion is asking a contract to
*produce* evidence rather than to check it, and its verdict cannot be known at planning
time. Put that analysis in a discovery packet (§5.8) that runs first, and have the change
packet cite the accepted outcome as a precondition. Two symptoms identify it: consecutive
adequacy reviewers must effectively execute the criterion to review it, and the criterion's
own procedure contains an unbounded enumeration ("apply this rule to every site and show
each one terminates"). Measured cost of not doing this: two consecutive `INADEQUATE`
verdicts and a §5.5 redesign on one criterion
(`docs/reviews/a4b-pio-methodology-ruling.md`).

### 5.6 Premise freshness

Before an adequacy verdict the Planner judges whether the evidence that motivates the
packet still holds for the target revision. The Session may prepare the facts; the
Planner decides whether they are enough. Consider: the cited artifact exists and shows
the event; its evidence profile; the source/build it represents; later commits that may
have fixed or superseded it; whether the failure is current or only historical; and a
coverage witness for absence claims.

- `PASS` — the premise is established for the target revision.
- `BOUNDED` — some uncertainty remains, it is stated, and the packet fails closed if
  the premise is wrong (a stale premise yields FAIL or UNKNOWN, never a false PASS).
- `FAIL` — the premise is refuted or unsupported in a way that could yield a false
  result; the packet is INADEQUATE or retired.

Do this once per packet, and again only on `PREMISE_CHANGED`, not on every revision.

### 5.7 What a frozen packet contains

The operative contract only: claim and limits, identity and profile, steps, criteria,
decision rows, pins, stop conditions, and closure. No revision history, "Fixed:" notes,
or rationale about earlier revisions. Those go in one log,
`docs/reviews/<packet>-revision-history.md`, which is non-authoritative, is not reviewed
for accuracy, and loses to the contract on any conflict. The packet carries a one-line
pointer to it.

### 5.8 Packet classes

| | **Discovery packet** | **Change packet** |
|---|---|---|
| Output | knowledge: what was observed | a behaviour change, or an acceptance claim |
| May do | read anything; add diagnostic-only instrumentation; run exploratory or fixture profiles | anything its contract authorizes |
| Instrumentation | reversible, trace-only or behind an environment variable, off by default at closure | production code under full review |
| Contract | about one page (§6.3) | full contract (§6.1–6.2) |
| Adequacy review | the writing Planner, two questions (§5.3) | a Planner that did not write it, full review (§5.3) |
| Acceptance | reviewer confirms the artifacts exist, match the commands, and select the recorded outcome row | every criterion reproduced |
| Can claim | "observed X under profile Y" | what its criteria establish |

A discovery packet never satisfies a strict criterion and never claims anything works.
Its outcome table names the next packet for each result, so closing it hands the
Planner its next brief directly. Prefer a discovery packet whenever the next
implementation depends on facts nobody has observed. Keep it small enough to execute
in one session. Leaving its instrumentation enabled after closure requires a change
packet.

## 6. Packet construction

### 6.1 Drafting checklist

This list is for change packets; a discovery packet uses §6.3. Whoever drafts works
through it; the Planner may waive an item with a one-line reason (for example, "no
controls: the oracle is an existing tested tool").

**Enumerating guest accesses to an address.** A criterion that claims something about
*every* access to an address must not build its population by searching text. One address
can be spelled more than one way in the generated code, so a text search silently covers a
subset and a literal executor returns PASS over it. Derive the population from the original
XBE instruction stream with operands normalised to `uint32`, reconcile it against the
generated code by normalised **value** rather than spelling, freeze the count and the
command that produced it inside the criterion, and make the PASS predicate conditional on
`count found == frozen count` with every listed site evaluated — any difference is
`UNKNOWN`, never PASS. State in the same sentence what the method cannot see:
register-indirect, computed, and table-driven accesses. A text search is admissible as a
lead, never as a completeness witness. This was measured: a `PIO_FREE` enumeration built by
grepping one spelling found 10 of 28 sites (`docs/reviews/a4b-q1-advisor-ruling.md`,
PREMISE_CHANGED addendum).

1. **Objective first.** State the bounded claim and explicit non-goals.
2. **Stable IDs.** One criterion = one independently decidable obligation.
3. **Guards against.** Each criterion names the wrong outcome it prevents. A criterion
   that prevents none is deleted.
4. **Profile before measurement.** Strict/exploratory/fixture/manual scope, identities,
   instrumentation, and permitted overrides are fixed first.
5. **Reproducible procedure.** Working directory, tools, inputs, command, environment,
   bounds, artifact destination. Missing tooling is a prerequisite, not an invented
   command. Every command has been run before freezing.
6. **Decision rule.** PASS, FAIL, and UNKNOWN/BLOCKED are each defined; missing,
   malformed, stale, unexercised, or empty evidence never defaults to PASS.
7. **Exercise and controls.** Absence claims need a coverage witness; a new checker
   needs known-good and known-bad controls; the oracle is not derived solely from the
   implementation under test.
8. **Separate claim classes.** Structural correctness, exercised ABI behavior, strict
   reachability, device semantics, and liveness are separate obligations.
9. **Scope and stops.** Population, window, coverage, write scope, and `Stop if`.
10. **Mechanical closure.** Criterion -> artifact/hash -> result -> reviewer disposition
    on one declared evidence revision.
11. **Observability first.** If success and failure cannot be distinguished, build and
    validate the tooling in an earlier packet.

Before release ask both questions:

> Could broken behavior, an unexercised path, an exploratory run, stale evidence, or an
> empty input satisfy these checks? — if yes, tighten.
> Does every requirement protect the objective against a named wrong outcome? — if not,
> delete it.

### 6.2 Packet template

```markdown
## <Packet ID> — <bounded outcome>

**Contract revision:** <revision>   **Status:** draft
**Governing requirement:** <requirement or link>
**Depends on:** <accepted packet IDs/revisions>
**Baseline:** <game/toolkit revisions + dirty-state identity>
**Revision log:** docs/reviews/<packet>-revision-history.md (non-authoritative)

### Motivating evidence
- <artifact/run/commit> — profile <...> — source/build <...> — supports <claim>

### Claim and boundaries
- Establishes: <precise claim>
- Does not establish: <limits>
- Non-goals: <out of scope>

### Readiness
- Tools/tests (verified to run): <invocations>
- Prerequisites / access: <...>
- Stop if: <conditions>

### Execution
- Steps: <bounded steps>
- Write scope: <paths>   Worker scopes: <optional, disjoint>
- Build/run owner: <one owner>

### AC-<ID> — <single claim>
- Mandatory: yes/no
- Guards against: <wrong outcome>
- Evidence profile: <fixture/exploratory/strict/manual>
- Procedure: <cwd + command + environment>
- Artifact: <path/schema>
- Oracle: <independent basis>
- PASS / FAIL / UNKNOWN: <predicates>
- Controls: <known good + known bad, or waiver reason>
- Claim limits: <what PASS does not establish>

### Decision rows
- R-<ID>: <condition> -> <outcome> -> <next action>

### Closure
- Evidence index: <criterion -> artifact/hash -> result>
- Post-review edits reopen affected criteria.
- Unrelated next stop: record as follow-up; do not expand scope.
```

### 6.3 Discovery packet template

```markdown
## <Packet ID> — discover <what>

**Class:** discovery   **Contract revision:** <revision>   **Status:** draft
**Starts from:** <blocker + run/artifact that shows it>
**Baseline:** <game/toolkit revisions>

### Question
<the one decision this packet unblocks, and the unknowns behind it>

### Experiments
1. <instrumentation, if any: file, what it records, how it is enabled; off by default>
2. <exact command(s), profile, bounds, artifact destination>

### Outcomes
| ID | Observed | Means | Next packet |
|---|---|---|---|
| O-1 | <observable pattern> | <interpretation> | <next packet> |
| O-UNKNOWN | none of the above, or evidence missing/unreadable | not decided | <re-plan> |

### Limits and stops
- Does not establish: <limits; never a strict or "it works" claim>
- Stop if: <conditions>
- Closure: instrumentation off by default or removed; artifacts listed.
```

## 7. Independence vocabulary

| Term | Meaning |
|---|---|
| **model diversity** | a different model family from the Session |
| **procedural independence** | the reviewer did not author the work and is asked to falsify, not confirm |
| **evidence reproduction** | the reviewer re-ran the load-bearing measurement |

Model diversity helps; it never replaces reproduced evidence.

## 8. Document ownership

| Fact | Owner |
|---|---|
| roster, role authority, packet lifecycle, escalation, adequacy verdict | **this file** |
| operating/build/runtime discipline | `AGENTS.md` |
| evidence-profile semantics, strictness, admitted model classes | `docs/jsrf-run-profiles.md` |
| current packet, blocker, next action, milestone acceptance | `plan-jsrf-bare-minimum.md` |
| a packet's operative contract | `docs/packets/<packet>.md` |
| verdicts, deferred advisories, discretionary decisions, case rulings | `docs/reviews/<packet>-r<N>-*.md` |
| a packet's revision narrative | `docs/reviews/<packet>-revision-history.md` (non-authoritative) |
| transient session notes | outside the repository (no report files) |
| dated narrative | `docs/jsrf-operating-history.md` |

Do not copy the roster outside §1. The Advisor decides a general technical rule (§3.3);
this table says where it is recorded; the Session records it; the Planner and reviewer
apply it to later work.
