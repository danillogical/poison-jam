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
3. **Probe the Acceptance reviewer.** Resolve its route live (§1) and spawn a fresh
   child at the listed effort. PASS requires a completed response containing a fresh
   session token, one reason an empty evidence set must fail acceptance, and the output
   hash of one named read-only command it ran itself (reproduction is its job). Dispatch
   alone is not PASS. The probe child is discarded; each review spawns its own.
4. **Probe the Advisor (one combined probe).** Resolve its route live (§1) and spawn
   it as a fresh continuable child at the listed effort. In the first turn give it a
   unique marker and ask it to read one named repository file and report a fact
   deliberately left out of the brief. In a second turn to the **same child**, ask for
   the marker without repeating it. PASS requires the correct fact (checked against the
   file), the correct marker from the same child, and the listed effort in the child's
   metadata. This proves continuity and that the Advisor can read project evidence.
5. **Reconcile packet state.** Inspect both working trees and preserve unrelated
   edits. Execute **only** the exact packet/revision named in the plan's
   `CURRENT PACKET` block. Never discover work by scanning for pending items. If no
   packet is current, implementation is `BLOCKED` until one is promoted (§5).
6. **Persist the receipt.** Fill `docs/session-start-template.md`, save it as
   `docs/reviews/startup-<date>-<session-id>.md`, and link it from the active packet
   or review record.

A required route, effort, spawn, or continuation that is unavailable makes startup
`BLOCKED` for accepted game work. Do not substitute an unlisted route. The Advisor
child is created and probed fresh in each top-level session (§4.4). The Planner needs
no separate probe: every adequacy review it returns must cite the files and line ranges
it read, and each Planner turn must report the listed `reasoningEffort`; both are
checked on first use.

## 1. Supported harnesses and roster

Exactly two harnesses are supported. Use only the assignments in the active column.

| Role | Codex | DeepSeek Harness (DSH) |
|---|---|---|
| **Session** | `gpt-6-luna` @ `max` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Worker subagents** | `gpt-6-luna` @ `max` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Planner** | `gpt-6-astra` @ `medium` | **Muse Spark 1.3** @ `high` (`skill: muse-worker`, fresh handle) |
| **Persistent advisor** | `gpt-6-astra` @ `medium` | **Claude Opus 5.5** @ `high` (`route: LIVE_RESOLVE`, continuable child) |
| **Acceptance reviewer** | `gpt-6-luna` @ `max` | **GPT-6 Sol** @ `high` (`provider: codex`, `route: LIVE_RESOLVE`, fresh child per review) |

Acceptance has **one review stage**. An `ACCEPT` is final; a rejection disputed only on
how a frozen criterion reads goes to the Advisor (§2.2).

**DSH independence.** The Planner (Muse Spark), Advisor (Claude) and Acceptance reviewer
(GPT-6 Sol) are three model families, and each runs in its own handle or child: a
reviewer never sees the packet's planning or Advisor conversation.

**Authority attaches to the role, not the model.** A GPT-6 Sol Acceptance reviewer is a
contract role and is bound exactly like any other Acceptance reviewer. A Muse Spark
Planner handle has Planner authority; the Advisor child has Advisor authority. One child
or handle holds one role: a child that reviewed a packet's acceptance does not also rule
on a dispute about that review.

### Live verification

- DSH: resolve ordinary model roles with `list_subagent_models` and verify any listed
  effort. A row marked `LIVE_RESOLVE` requires **exactly one** advertised route whose
  canonical model identity is the named model, whose provider matches when specified,
  and which supports the required effort. Record the returned provider/model string.
  Zero or multiple matches is `BLOCKED` until §1 or the route ambiguity is repaired.
  Never invent an identifier from a display name.
- The DSH Advisor resolves like any `LIVE_RESOLVE` row. The `claude` route was
  unusable from 2026-09-26; never assume it is back — a failed resolution is `BLOCKED`.
- The DSH Muse Spark role (Planner) is **not** resolved through `list_subagent_models`;
  it is provided by the `muse-worker` skill. Read that skill before first use. Each
  handle is opened fresh for its packet and recorded in that packet's review record.
  Handles are workspace-bound. Every Muse turn must report the `reasoningEffort` listed
  in the table; a different reported tier is `BLOCKED` for that output.
  `.muse-workers.md` holds the former persistent Advisor handle, as history.
- Codex: verify route and effort through current harness metadata.
- If a row omits effort, omit `reasoning_effort`.
- An unavailable assignment is `BLOCKED`; never fall back silently.

## 2. Role authority

### 2.1 Two layers

| Layer | Roles | Produces | Bound by |
|---|---|---|---|
| **Judgment** | Persistent advisor (senior), Planner | rulings, packet design, adequacy verdicts, acceptance-dispute rulings, deferrals, stops, exceptions | facts (§2.4), the owner's objective and reserved decisions (§3.4) |
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

When spawning a bounded implementation worker, the Session includes the
implementation-worker progress gate below in the brief. A worker's no-progress stop is
not permission to spawn an identical replacement automatically; the Session first
routes the returned blocker through the normal escalation ladder.

**Worker subagents** — used for context isolation (summarize a large log or dump) or
bounded implementation (explicit files, contract, and check). Workers keep disjoint
write scopes, never touch policy documents, stop at ambiguity or at the edge of their
scope, and return `file:line` or artifact evidence marked **MEASURED** or
**INFERRED**. A worker never owns final integration, build, or run.

**Implementation-worker progress gate.** This gate applies to bounded implementation
workers, not read-only/log-analysis workers.

- By **15 tool calls**, the worker must have produced at least one concrete execution
  artifact: an edit, compile/build attempt, test run, generated fixture, or a bounded
  blocker report.
- If it has not, it stops broad investigation and either performs the smallest safe
  implementation/compile step immediately or returns `BLOCKED`.
- After the first implementation attempt, further source reads must be tied to a
  **specific observed compiler, linker, test, or runtime failure** and state what that
  read is expected to resolve.
- A worker may not restart architecture/source exploration from first principles after
  an implementation failure unless new contradictory evidence invalidates the prior
  design premise.
- If **two consecutive 10-call stretches** produce no new artifact, measurement, or
  narrowed blocker, the worker stops and returns control to the Session.
- Re-reading the same files or reconsidering already-settled design alternatives
  without new contradictory evidence counts as **no progress**.
- On genuine ambiguity, required scope/policy change, or inability to proceed inside
  the assigned write scope, the worker returns:

```text
STATUS: BLOCKED
EXACT REQUIREMENT:
EXACT BLOCKER:
EVIDENCE:
WHY CURRENT WRITE SCOPE CANNOT SATISFY IT:
SMALLEST DECISION/CHANGE NEEDED:
```

A no-progress or blocker return is an escalation signal. The Session does **not**
automatically spawn an identical replacement worker on the same brief; it first
decides whether the blocker belongs to the Planner, Advisor, or a revised bounded
worker brief.

**Acceptance reviewer** — checks delivered evidence against the **frozen contract
only**:

- For each mandatory criterion return `AGREED`, `DISAGREED`, or `CANNOT VERIFY`, with
  the reproducing command or `file:line` evidence. Reproduce load-bearing measurements;
  absence checks need a positive control. Try to falsify.
- Overall disposition: `ACCEPT` only when every mandatory criterion is `AGREED`;
  otherwise `NOT ACCEPTED`, naming the blocking criteria.
- An ambiguous criterion is `CANNOT VERIFY`, naming the ambiguity, never a private
  reinterpretation; the Session takes it to the Advisor (below).
- Concerns outside the contract are listed separately as advisories. They do not
  change the disposition and do not add criteria.

**Acceptance and disputes** — one review stage:

1. The Acceptance reviewer reviews every mandatory criterion as above. `ACCEPT` is
   final.
2. On `NOT ACCEPTED` the Session records the review, then sorts each criterion that is
   not `AGREED`:
   - **Evidence failure** — the reproduction failed, or the evidence is missing, stale,
     or does not show the claim. The criterion stays failed; the work or evidence is
     fixed and the affected criteria are re-reviewed (`pending — post-review edits`).
     No role can turn it into PASS (§2.4).
   - **Contradicting measurements** — the reviewer's reproduction and the delivered
     evidence disagree and neither is shown wrong. That is a factual dispute: it goes to
     the Advisor (§4.2), which orders the discriminating measurement, and the reproduced
     result controls (§2.4).
   - **Interpretation dispute** — the evidence is not in question, but the Session and
     the reviewer read an already-frozen criterion differently, or the reviewer returned
     `CANNOT VERIFY` because its wording is ambiguous. Only this goes to the Advisor, as
     an acceptance-dispute ruling (§2.3).
3. The Session sends the Advisor only the disputed criterion(s), the frozen contract,
   the review record, the delivered evidence, and each side's reading in one or two
   sentences. With no second reviewer, the Advisor is the only other look at a rejected
   criterion, so it reads the evidence the dispute turns on itself (§2.4.2) instead of
   relying on either side's summary.
4. It returns `AGREED`, `DISAGREED`, or `CANNOT VERIFY` per escalated criterion, with the
   reading it applied and the evidence that would reverse it. If, once the readings are
   stated, they agree and what remains is which measurement is right, it returns
   `CANNOT VERIFY` naming the measurement that would decide — never a disposition by
   authority.
5. If deciding needs a policy, architecture, scope, fidelity or exception decision, the
   Advisor makes it as a separate, recorded technical-policy ruling (§3.3) rather than
   folding it into the criterion's disposition. Missing evidence remains missing
   evidence (§2.4).
6. `ACCEPT` then requires every mandatory criterion `AGREED`, by the review or by the
   dispute ruling. That disposition binds the criterion for that evidence revision and is
   not re-ruled without new evidence or `PREMISE_CHANGED`.

A reviewer route failure is `pending — reviewer unavailable`, not a failed review; it
is repaired, not sent to the Advisor.

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
- recommend retiring a packet whose premise has failed;
- keep initiative bounded by the packet decision: adjacent improvements, cleanups,
  architecture opportunities, and newly noticed defects stay follow-up leads unless
  omitting them can plausibly cause this packet's bounded claim to false-PASS,
  false-FAIL, choose a wrong implementation, bind the wrong evidence, or execute
  unsafely;
- not improve architecture merely because a better design is visible while planning;
  an architectural change enters the packet only when it is necessary to make the
  bounded claim mechanically decidable or to avoid one of the concrete wrong outcomes
  above.

**Persistent advisor** — the project's senior technical decision-maker. Everything
the Planner may do, plus:

- resolve any technical question: architecture, device/emulator semantics,
  reverse-engineering method, evidence admissibility, contract interpretation;
- define missing technical or evidence policy (§3.3);
- resolve disagreements among Session, Planner, and reviewer — its ruling is final
  within the limits of §2.4 and §3.4; the Planner may record dissent, but dissent does
  not block;
- rule on acceptance disputes about how a frozen criterion reads (§2.2), reading the
  disputed evidence itself; it interprets the wording but never lowers the evidence
  requirement, and a contradiction between measurements is settled by a new
  measurement, not by the ruling;
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
the review record. The Session records the ruling text verbatim with the Advisor's
child ID, model, and effort; a paraphrase cites that record.
An attribution that cannot be traced to a recorded Advisor response is `UNKNOWN`.

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
Worker      -> Session
Session     -> Planner       (packet design, adequacy, whether to revise)
Session     -> Advisor       (technical/policy question, including how to read a frozen step)
Reviewer    -> Advisor       (via the Session; a frozen-contract interpretation dispute, §2.2)
Planner     -> Advisor       (policy gap, methodology change, anything beyond Planner authority)
Advisor     -> Owner         (§3.4 only; everything else ends at the Advisor)
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

The DSH Advisor is a **continuable child** on the §1 route, reused for the whole
top-level session unless its state becomes unreliable; a new top-level session creates
and probes its own (§0 step 4). Do not seed it with the current conversation: brief it
from the files (§4.3). Spawn it with `run_in_background: true`, keep the child ID, and
continue it with `send_message`. Rulings, not the child's memory, carry decisions across
sessions: each is recorded, with the child ID, where §3.3 and §8 say.

For the direct Codex harness, use the continuable Advisor route from the Codex column
of §1 and resume it through that harness's continuation mechanism. If the required
continuation mechanism is unavailable, work needing the Advisor is `BLOCKED`; do not
invent an invocation.

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

**4. Sketch first, with an early Advisor shape preflight and escalating checkpoints.**
The Planner's goal is a sketch of at most about 15 lines: claim, class (discovery or
change), unknowns, the experiment, and the outcome rows. It writes the sketch at the
top of its draft packet file, under a `Sketch` heading, where it survives compaction and
the owner can read it.

For every **new change packet or material redesign**, the first viable sketch goes to
the Persistent Advisor **as soon as it exists and no later than 20 tool calls**, before
the Planner expands it into the full packet. This is a mandatory **shape/policy
preflight**, not a second adequacy review. The Advisor reads only the smallest source
set that could change these four judgments:

1. Is this the right packet class: change vs discovery?
2. Is the bounded claim the right size, or is it combining independently decidable work?
3. Are the stated unknowns actually the ones that can change what should be built?
4. Can the proposed experiment/evidence decide the claim without inventing new policy?

The Advisor returns only:

```text
SHAPE: PROCEED | REDIRECT | DISCOVERY_FIRST
REASON: <short>
POLICY_ISSUE: NONE | <bounded issue>
REVERSED_BY: <evidence that would change this>
```

`PROCEED` authorizes the Planner to expand the sketch into the full packet without
another general Advisor review. `REDIRECT` or `DISCOVERY_FIRST` is binding technical
direction under §2.3. This preflight does **not** replace the fresh-Planner adequacy
review required by §5.1.5 for a change packet.

A routine discovery packet does not require the mandatory preflight unless it creates
or changes technical/evidence policy, the Planner cannot state a discriminating
experiment, or an existing Advisor ruling requires consultation.

After the shape preflight, the full packet replaces the sketch **without further
investigation unless the forecast below names a specific unresolved fact that can still
change the sketch**. The sketch goes to the Advisor at the checkpoints below, never to
the Session, which does not judge or answer it.

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
| **first viable sketch / ≤20 calls** | writes sketch 1 and, for a change packet/material redesign, sends it for shape preflight before expanding the packet; after `PROCEED`, either writes the packet or writes a specific forecast and continues | returns `PROCEED`, `REDIRECT`, or `DISCOVERY_FIRST`; expands into a technical ruling only if a policy/architecture question is actually present |
| **40 calls** | writes sketch 2, the yield against the prior forecast, and a new forecast; sends all three to the Advisor, then continues unless redirected | decides whether the investigation is still paying; may answer, redirect, or stop it |
| **60 calls** | stops investigating; writes sketch 3 and the yield against the 40-call forecast; sends both, plus what is still missing, to the Advisor | must decide: extend planning with an explicit bounded list of allowed reads, or have the Planner write the packet with the gaps as its subject |

The yield is what tells the Advisor whether investigation is still paying. If a
20-call stretch changed nothing that matters in the sketch, that is the signal to stop,
and the Planner should say so rather than wait to be told. The Planner messages the
Advisor directly when the harness allows; otherwise the Session relays the message
unchanged.

The Advisor does **not** perform a full second review of every completed Planner packet.
After `PROCEED`, it becomes involved again only at the checkpoints above or when the
Planner encounters a genuine Advisor-class question: architecture/device semantics,
evidence admissibility, workflow methodology, deletion/waiver of a previously
protective requirement, acceptance of material uncertainty, fidelity tradeoff,
conflict with an existing ruling, or repeated failure requiring a methodology change.

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
   **fresh** Planner child. In DSH this means a fresh Muse Spark Planner handle under the
   §1 roster, never the handle that wrote the revision;
   the Advisor's shape preflight is not the adequacy review and does not replace it.
   **Discovery packets:** the writing Planner reviews its own packet against §5.3's two
   questions; no second Planner is spawned. Either way, independence in the end comes
   from the Acceptance reviewer reproducing the evidence.

### 5.2 States

| State | Meaning | Exit |
|---|---|---|
| **draft** | being written | submitted for adequacy review |
| **INADEQUATE** | review found ≥1 blocking defect | revision (§5.4) or retirement |
| **ADEQUATE** | review found zero blocking defects | **frozen and promoted in the same step** |
| **promoted** | frozen hash in `CURRENT PACKET` | Session executes |
| **delivered** | Session says criteria pass with evidence | Acceptance review |
| **disputed** | the Acceptance review returned `NOT ACCEPTED` and a frozen-contract interpretation dispute remains | Advisor dispute ruling (§2.2) |
| **accepted** | all mandatory criteria are `AGREED` after review or dispute ruling | record; plan names next work |
| **pending — CANNOT VERIFY** | reviewer cannot reproduce a criterion | new evidence; if evidence exists but wording remains disputed, Advisor dispute ruling (§2.2) |
| **pending — reviewer unavailable** | reviewer route failed | repair route; no substitution |
| **pending — post-review edits** | reviewed tree/evidence changed | re-review affected criteria |
| **escalated** | policy/architecture/scope/fidelity/exception question | Advisor ruling, then rerun/re-review as needed |
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
(`docs/jsrf-technical-record.md §4`).

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

**Enumerating guest accesses to an address.** A criterion that claims something about*every* access to an address must not build its population by searching text. One address
can be spelled more than one way in the generated code, so a text search silently covers a
subset and a literal executor returns PASS over it. Derive the population from the original
XBE instruction stream with operands normalised to `uint32`, reconcile it against the
generated code by normalised **value** rather than spelling, freeze the count and the
command that produced it inside the criterion, and make the PASS predicate conditional on
`count found == frozen count` with every listed site evaluated — any difference is
`UNKNOWN`, never PASS. State in the same sentence what the method cannot see:
register-indirect, computed, and table-driven accesses. A text search is admissible as a
lead, never as a completeness witness. This was measured: a `PIO_FREE` enumeration built by
grepping one spelling found 10 of 28 sites (`docs/jsrf-technical-record.md §4`,
PREMISE_CHANGED addendum).

**Stating the enumeration method.** Completeness and uniqueness claims state their enumeration
method. Linear-sweep decode is inadmissible for completeness without a drift control (known
instruction addresses demonstrably reached); use recursive descent or equivalent
control-flow-following enumeration. Byte-pattern scans are alignment-independent for existence;
for uniqueness ("exactly one") state the encoding coverage over all instruction forms that could
carry the pattern. This was measured twice in one session: a linear decode of `.text` from its
own start produced 29 548 plausible instructions and reached **neither** of two load-bearing
addresses, so every later "instruction boundary" in that sweep was wrong
(`docs/jsrf-technical-record.md §6`); and an `--aligned` dword scan
reported **zero** references to a vtable base where the correct count is **three**, because the
installs are `C7 06 70 12 1E 00` and the immediate sits off a 4-byte boundary
(`docs/jsrf-technical-record.md §6`). Raw-byte scans are the sound fallback for
existence and for displacement uniqueness, and they are why this line's surviving findings
withstood the defect.

**Merge packets.** A packet that merges upstream must inventory device- and
profile-relevant hunks **by content, not by conflict status**, because a clean hunk can
restore a deleted override or arm new device behaviour as silently as a conflict can hide it.
The local admitted form wins for anything touching an admitted model or a classifier-listed
or deleted variable, and the merged tree is grepped for those names and for arming call
sites. See `docs/jsrf-run-profiles.md` §"Upstream merges never silently change admitted
evidence semantics".

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
   **Decision inputs are lossless by construction.** A criterion may select a row only from
   a record that cannot drop the deciding event: a write-once latch or an uncapped counter,
   updated at the event by the code that performs it. Capped, sampled, rate-limited or
   first-N logs are **observation only**, and no row may depend on the presence or absence
   of such a line. Absence of a witness is never a positive attribution: it selects
   `UNKNOWN` or an explicit unattributed row, never a row that blames a specific agent. The
   packet that owns the code producing a decision input also owns and fixture-tests that
   input's semantics; a consuming packet only reads it. Measured cost of not doing this:
   two consecutive `INADEQUATE` verdicts on one mechanism
   (`xboxrecomp/src/apu/GP-INTEGRATION.md`).
6b. **Decision inputs are bounded by construction.** A record a row decides from must have a
   size fixed by a **finite universe that is stated and derived from source** (a register
   file, a FIFO count, a fixed set of classes, the enumerated instrumentation sites),
   **independent of run length and input volume**. Key it by the **property the decision
   classifies** (provenance class, bin, region), not by the identity of individual events
   (address, page, value). A table whose key universe is not shown finite is **observation
   only**. An overflow or out-of-universe counter is a **bug detector**; if a record can
   overflow because the run was long or busy, **the key is wrong**. Completeness of
   instrumentation is established **structurally** (enumerated hook sites, each with a
   fixture case), not by counting distinct keys at run time. A criterion whose only role is
   to qualify a PASS is **evaluated only when that PASS holds**. Measured cost of not doing
   this: a third consecutive `INADEQUATE` verdict on one mechanism, where a 256-entry table
   faced a 1024-word key universe (`xboxrecomp/src/apu/GP-INTEGRATION.md`).
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
| the former persistent Muse Advisor handle (history) | `.muse-workers.md` |
| transient session notes | outside the repository (no report files) |
| dated narrative | `docs/jsrf-operating-history.md` |

Do not copy the roster outside §1. The Advisor decides a general technical rule (§3.3);
this table says where it is recorded; the Session records it; the Planner and reviewer
apply it to later work.
