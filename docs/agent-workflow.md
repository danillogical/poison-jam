# Agent workflow: decision sandwich, roles, authority, packet lifecycle, and escalation

This file is the **single authority** for JSRF agent staffing and workflow.
`AGENTS.md` points here but does not duplicate provider, model, or effort assignments.

**Design goal:** cheap models execute precisely; senior models exercise judgment;
evidence stays trustworthy; technical questions end at the Advisor; the project
keeps moving. The workflow uses a **decision sandwich**: senior roles freeze the
question and policy, the Session and workers execute the bounded work, Muse performs a routine
pre/post execution guardrail, and a fresh Packet reviewer decides acceptance. When a
literal reading of this file defeats that goal, the Advisor may issue a process
interpretation under §2.3, subject to the hard limits in §2.4 and §3.4.

**The bare minimum is pragmatic (owner decision, 2026-09-30).** The project takes the path of least
resistance to the title screen and then the rest of the slice. Shortcuts are allowed when recorded
in `docs/jsrf-compatibility-ledger.md` (§2.4.9); strict evidence is kept for fidelity claims
(`docs/jsrf-run-profiles.md` §"Pragmatic bare minimum").

**Staffing rule:** the roster table in §1 is the only persisted staffing policy.
Other files and later sections refer to roles only. If staffing changes, edit §1
first and sweep the repository for stale copied assignments.

## 0. Startup checks

Complete these in every new top-level session before implementing a game packet.
A previous session's child IDs, route resolutions, or PASS results do not establish
current readiness.

1. **Load current authority.** Read this file, the `CURRENT PACKET` block of
   `plan-jsrf-bare-minimum.md`, and `docs/jsrf-run-profiles.md`. Record both
   repository identities and dirty files. Nothing else selects packet-governed work.

2. **Verify routes.** Confirm the running Session matches its §1 row from harness
   metadata. Resolve the Planner, Packet reviewer and Turn reviewer routes live (§1).
   Record `UNKNOWN` if unverifiable; never infer a canonical route from a display name.

3. **Probe the Packet reviewer.** Resolve its §1 route and spawn a fresh child at the
   listed effort. PASS requires a completed response containing:
   - a fresh session token;
   - one reason an empty evidence set must fail acceptance; and
   - the output hash of one named read-only command it ran itself.

   Dispatch alone is not PASS. The probe child is discarded; each packet review spawns
   its own fresh Packet reviewer. The Turn reviewer needs no startup probe: its first
   turn-end review reports its route, its effort and the output hash of a read-only
   command it ran, and those are checked on that first use.

4. **Probe the Persistent Advisor as a real continuable child.** Resolve the exact
   §1 Advisor provider/model and spawn it through a delegation surface that creates a
   **continuable** child at the listed effort. The probe must establish together:
   - exact model/provider;
   - requested effort;
   - visibility in the continuable-agent listing;
   - a continuation call reaches the same child;
   - a first turn reads a named repository file and reports a fact deliberately omitted
     from the brief;
   - a second turn to the same child returns a unique marker supplied only in turn one.

   A one-shot child does **not** satisfy this requirement. If no route can prove
   **continuability + exact §1 route + listed effort**, Advisor-dependent work is
   `BLOCKED`.

5. **Probe the Muse decision guardrail.** Spawn one continuable child on its §1 route
   at the listed effort. PASS requires:
   - the child appears in the normal DSH child list;
   - the effective model route is the one §1 names;
   - its cwd is the current repository through session-scoped workspace resolution;
   - Muse reads one named repository fact using Muse-native tools; and
   - a second message reaches the same DSH child and uses a marker supplied only in the
     first turn.

   Reuse this child for routine packet preflight and post-execution guardrail checks
   during the top-level session. It is not the Packet reviewer and does not replace the
   Advisor.

6. **Reconcile packet state.** Inspect both working trees and preserve unrelated edits.
   Game-behavior implementation executes **only** the exact packet/revision named in
   the plan's `CURRENT PACKET` block. Never discover packet work by scanning for pending
   items. If no packet is current, game-behavior implementation is `BLOCKED` until one
   is promoted (§5).

   This does **not** block explicit owner-directed non-packet work that the authoritative
   plan names as such, including maintenance, tooling, rebaseline work, repository
   hygiene, documentation maintenance, or environment repair, provided that work does
   not silently change behavior governed by an unpromoted packet. When in doubt, route
   the boundary to the Planner or Advisor rather than treating maintenance as an
   implicit implementation packet.

7. **Persist a bounded startup receipt.** Fill `docs/session-start-template.md` and
   write the current receipt to:

   `docs/reviews/startup-current.md`

   replacing the previous session's rolling receipt rather than creating an unbounded
   family of `startup-<date>-<session-id>.md` files. Record the receipt SHA-256 plus
   the Session route, Advisor child ID, Muse guardrail child ID, and Packet reviewer
   probe route. Before a packet reaches final acceptance, copy the startup facts that
   materially establish that packet's staffing and route readiness into its durable
   acceptance/review record. Historical authority therefore lives in packet/review
   records; `startup-current.md` is only the current session's operational receipt.

A required Session, Planner, Advisor, Muse guardrail, reviewer, effort, spawn, or
continuation capability that is unavailable makes the affected workflow stage
`BLOCKED`. Do not substitute an unlisted route. The Persistent Advisor and Muse
guardrail are created and probed fresh in each top-level session. The Planner needs no
separate startup probe: its route is resolved live when its first child is spawned, and
every adequacy review it returns must cite the files and line ranges it read; both are
checked on first use.

## 1. Supported harness and roster

One harness is supported: the DeepSeek Harness (DSH). Use only these assignments.

| Role | Route @ effort (DSH) |
|---|---|
| **Session / orchestrator** | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Worker subagents** | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Planner** | `claude/claude-opus-5-5` @ `high` (Claude Opus 5.5; `route: LIVE_RESOLVE`, fresh child per packet) |
| **Persistent Advisor** | `claude/claude-opus-5-5` @ `xhigh` (Claude Opus 5.5; `route: CONTINUABLE_PINNED`, session-continuable child) |
| **Muse decision guardrail** | `subagent_muse` @ `max` (`muse-code` / Muse Spark 1.3; one session-continuable child) |
| **Packet reviewer** | `claude/claude-opus-5-5` @ `medium` (Claude Opus 5.5; `route: LIVE_RESOLVE`, fresh child per packet review) |
| **Turn reviewer** | `codex/gpt-6.1-sol` @ `high` (GPT-6.1 Sol; `route: LIVE_RESOLVE`, fresh child per turn-end review) |

`codex` here is the DSH **provider** name that serves GPT-6.1 Sol, not a harness.
`subagent_muse` is the current DSH-native continuable Muse child route backed by
`muse-code`; it is not the retired external Muse subagent provider that historically
used the same name.

**Fallback routes: NONE AUTHORISED.** A fallback is a staffing change and no agent may
choose one. There is no second technical-escalation model above the Persistent Advisor.
If a decision is outside the Advisor's authority, it is an owner decision under §3.4.

The session allow-list in the active DSH profile must contain the ordinary model routes
named above. `subagent_muse` is verified through its own child/model route rather than
treated as an ordinary DSH tool-calling model.

Completion after execution has two gates:

1. the **Muse decision guardrail** must return `CLEAR`; then
2. a fresh **Packet reviewer** performs the authoritative acceptance review.

A chore gets the Muse post-check only (§2.2, §5.8).

A disputed review, requested criteria change, or technical/policy question goes to the
Persistent Advisor. If the remaining decision is owner-reserved under §3.4, the Advisor
asks the owner.

**Decision sandwich.**

```text
Planner drafts; Advisor shape preflight
        ↓
Muse guardrail preflight
        ↓
Planner adequacy review; freeze and promote
        ↓
DeepSeek Session + workers execute
        ↓
Muse guardrail post-review
        ↓
Packet reviewer reproduces and ACCEPTs / NOT ACCEPTED
        ↓
Persistent Advisor resolves any remaining technical decision or dispute
        ↓
Owner only for §3.4 owner-reserved decisions
```

**Independence.**

- Session and workers share DeepSeek 4.1 Flash intentionally; they are execution
  capacity, not independent review.
- Planner, Persistent Advisor and Packet reviewer share Claude Opus 5.5, so their
  independence is procedural: separate children, separate authority, and a Packet
  reviewer never receives the Planner, Advisor or Muse conversation. Its evidence
  reproduction is what makes a packet review independent.
- Muse Spark is a different model family and supplies the routine pre/post execution
  guardrail, not acceptance.
- The Turn reviewer (GPT-6.1 Sol) is a different model family from both the DeepSeek
  execution it checks and the Opus roles.

**Authority attaches to the role, not the model.** A child has exactly the authority of
the role it was spawned for: Planner, Advisor, Muse guardrail, Packet reviewer or Turn
reviewer authority. One role does not inherit another role's powers merely because
it uses a capable model.

### Live verification

- **Session.** Verify the running Session's route and effort from harness metadata.

- **Ordinary routes.** Resolve the Planner, Packet reviewer and Turn reviewer with
  `list_subagent_models` (or the running harness equivalent) and verify effort. A
  `LIVE_RESOLVE` row requires exactly one advertised route whose canonical identity
  matches the named model/provider and supports the required effort. Zero or multiple
  matches is `BLOCKED`.

- **Persistent Advisor: `CONTINUABLE_PINNED`.** Prove all three together:
  1. exact Claude Opus 5.5 provider/model;
  2. a continuable child reachable again; and
  3. the listed effort.

  Use a continuable-capable DSH child path with model selection enabled. Record the
  tool surface, provider, model, effort, child ID, continuability probe, and
  continuation result. A one-shot Opus child or a continuable child on an unverified
  model does not satisfy the role.

- **Muse decision guardrail.** Use the §1 route. Verify it creates a real continuable
  DSH child on that route, runs at the listed effort, appears in the
  normal child list, and operates in the requesting DSH session's cwd. DSH-native tool
  calling is unsupported on the Muse model route; Muse-native tools operate inside the
  assigned workspace. Record the child ID and continuity probe.

- **Planner.** A fresh child per packet or adequacy review as §5 requires.

- **Reviewers.** A fresh Packet reviewer child for each packet review and a fresh Turn
  reviewer child for each turn-end review. Never reuse the Muse guardrail, the Advisor
  or a Planner child as either.

- If a row omits effort, omit `reasoning_effort`.

- An unavailable assignment is `BLOCKED`; never fall back silently.

### Escalation ceiling

The **Persistent Advisor is the final technical escalation point**.

Architecture disputes, methodology changes, reviewer disputes, contradictory evidence,
reverse-engineering questions, fidelity questions, repeated implementation failure, and
other technical uncertainty terminate at the Persistent Advisor.

There is no second model tier above the Advisor.

If the remaining decision is genuinely owner-reserved — for example it changes the
project objective, materially changes scope, changes staffing/model assignments,
requires a significant new cost or dependency, requires a destructive or hard-to-reverse
action, or would contradict an explicit owner instruction — the Advisor asks the owner.

A factual contradiction is not owner-reserved merely because it is difficult. The
Advisor orders the cheapest discriminating measurement and lets reproduced evidence
control.

## 2. Role authority

### 2.1 Four layers

| Layer | Roles | Produces | Bound by |
|---|---|---|---|
| **Judgment** | Persistent Advisor, Planner | rulings, packet design, adequacy verdicts, review-dispute and criteria-change rulings, deferrals, stops, exceptions | facts (§2.4), the owner's objective and reserved decisions (§3.4) |
| **Guardrail** | Muse decision guardrail | pre-execution packet-shape verdict; post-execution causal/test/integration verdict | frozen/current packet, recorded rulings, facts (§2.4); may not change policy/criteria itself |
| **Review** | Packet reviewer, Turn reviewer | criterion dispositions, review findings, criteria-change requests and the completion disposition (Packet reviewer); turn-end verdicts (Turn reviewer, §4.5) | facts (§2.4), packet claim, this file, recorded rulings |
| **Contract** | Session, Worker subagents | executed steps, edits, measurements, evidence | frozen packet, this file, recorded rulings |

**Rank settles judgment; evidence settles facts.** The Advisor may overrule the Planner
on method, materiality, or scope, and may resolve a disputed Muse or reviewer
interpretation. No role, including the Advisor, can overrule a reproduced measurement
by authority; it can only order a new or better measurement.

The Muse guardrail is deliberately **not** the Packet reviewer. It catches wrong packet
shape before execution and causal/test/integration drift immediately after execution;
the fresh Packet reviewer supplies independent acceptance.

### 2.2 Contract roles execute; Muse guards the sandwich; the reviewers review

These rules are hard. When the contract is silent, ambiguous, or appears wrong, a
contract role **stops that line of work and escalates** (§4); it never resolves the
question itself.

**All contract roles**

1. For packet-governed game-behavior work, work only on the exact packet/revision named
   in `CURRENT PACKET`; verify its hash. Explicit owner-directed non-packet work allowed
   by §0.6 is outside this packet restriction but may not silently alter behavior that
   should be governed by an unpromoted packet.
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
9. An explicit stop boundary — from the owner, the Advisor, the Planner, or a packet's
   `Stop if` — is hard: stop, record the state, report. Do not continue past it on your
   own judgment.
10. Write each review record to disk, from the reviewer's own response, **before**
    starting the next revision or promoting.

**Session / orchestrator** — owns integration, build, run, evidence collection, and
record keeping. It drafts the mechanical parts of a packet (commands, paths, hashes,
environment) and verifies that every command in a draft actually runs before submitting it for review.
It does not author criteria or decision rows on its own authority (§5.1), and while the
Planner works it keeps the brief frozen (§5.1).

The Session **may** record direct measurements and contract-prescribed mechanical
derivations, including deterministic transforms the frozen procedure explicitly asks it
to compute. It may not invent new causal or historical conclusions that a decision will
rely on. New causal interpretation belongs to the Planner or Advisor; the Session
supplies the underlying artifacts and measured/mechanical results.

**Pragmatic duties (bare-minimum work).** The Session:

- **searches prior art first** — upstream, the toolkit forks, Mercenaries-Recompiled (`https://github.com/KraftMacAndChee/Mercenaries-Recompiled`) and
  halo-ce-universal (`https://github.com/cybersecurity/halo-ce-universal`) — before investigating a blocker (the answer has repeatedly already
  existed elsewhere);
- **picks the cheapest honest class itself**, without a senior call: if emulating a blocker would
  take more than about a day, it approximates, stubs, patches or reimplements, and adds the ledger
  entry in the same commit;
- **lists the ledger IDs** each milestone run relied on in that run's record;
- **takes a shortcut to the Advisor** when it would re-gate or remove an admitted model
  (`docs/jsrf-run-profiles.md` "Listed models"), when two shortcuts in a row have failed on the same
  blocker, or when it cannot tell whether a shortcut would hide a real defect in a later milestone.

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

**Muse decision guardrail — routine pre/post execution check.** The same continuable
Muse child is reused through the top-level session while its context remains
reliable. It is not a Planner, Advisor, or Packet reviewer.

**Pre-execution**, after the packet has been drafted and mechanically verified but
before final adequacy/freeze, Muse reads the packet plus the smallest source/evidence
set needed to judge whether execution would be coherent and testable:

```text
MUSE_PREFLIGHT: PROCEED | REDIRECT | DISCOVERY_FIRST | ESCALATE
REASON:
ANTI_VACUITY_RISK: NONE | <risk>
REVERSED_BY:
```

- `PROCEED` lets the draft continue to adequacy review.
- `REDIRECT` returns it to the Planner for a bounded revision.
- `DISCOVERY_FIRST` means implementation would be guessing.
- `ESCALATE` sends architecture, policy, evidence-admissibility, fidelity, or other
  Advisor-class questions to the Persistent Advisor.

Muse may identify anti-vacuity, integration, or missing-control defects;
it may not rewrite policy or acceptance criteria on its own authority.

A routine discovery packet skips the preflight; its post-review still runs.

**Post-execution**, before the Packet reviewer is spawned, Muse reads the frozen
packet, actual diff, validation, and load-bearing evidence:

```text
MUSE_POST: CLEAR | REVISE | ESCALATE
BLOCKING:
ANTI_VACUITY:
INTEGRATION:
REVERSED_BY:
```

- `CLEAR` sends the exact candidate tree/evidence to the Packet reviewer.
- `REVISE` names a concrete repairable defect or evidence/test failure; fix it,
  revalidate, and rerun Muse post-review.
- `ESCALATE` means the next step requires an Advisor ruling before changing the frozen
  contract or opening a new causal branch.

Muse `CLEAR` is **not acceptance**. It is the second side of the decision sandwich
around execution.

**Chores.** A chore gets the same post-execution check on its diff and gate output
before it is recorded as done. `REVISE` is repaired and re-checked; `ESCALATE` goes to
the Advisor.

Every Muse re-check is bounded by the churn limits in §5.5.

**Packet reviewer** — a fresh child performs a **real review of a packet's delivered
work after Muse returns `CLEAR`, before the packet can complete**. It is not a checklist: its job is to find out whether the work is
actually right and whether the evidence actually shows it. It reads the frozen packet,
the diff in both repositories since the packet's baseline, the delivered evidence and
run records, and the ledger entries the work relies on, and tries to falsify the
packet's claim.

- **Criteria.** For each mandatory criterion return `AGREED`, `DISAGREED`, or
  `CANNOT VERIFY`, with the reproducing command or `file:line` evidence. Reproduce
  load-bearing measurements; absence checks need a positive control. An ambiguous
  criterion is `CANNOT VERIFY`, naming the ambiguity, never a private reinterpretation.
- **Work review.** Review the change itself: correctness bugs, guest ABI or
  guest-state errors, regressions to accepted behaviour, unrecorded shortcuts, tests or
  instrumentation that cannot fail, and evidence that meets a criterion's letter while
  missing its purpose. Each finding cites `file:line` or artifact evidence and is
  labelled observed or inferred (§2.4.1).
- **Shortcuts.** For a bare-minimum milestone, confirm every shortcut the run relied on
  (switches set, stubs, patches, approximations) has an entry in
  `docs/jsrf-compatibility-ledger.md` and is listed in the run record; a missing entry
  is `DISAGREED` for that criterion.
- **Blocking findings.** A work-review finding that states a concrete failure scenario
  under the §3.1 test, within the packet's claim, is **blocking**: it holds completion
  even when every criterion is `AGREED`. A finding without such a scenario is an
  advisory; it is recorded and does not change the disposition. A defect outside the
  packet's claim is a follow-up lead, not a blocking finding.
- **Criteria-change requests.** When the review shows that a criterion is wrong — it can
  pass on broken work, cannot fail, guards against nothing, cannot be decided as
  written, or the claim needs a criterion the packet lacks — the Packet reviewer may **ask the
  Advisor to change the acceptance criteria**. The request names the criterion (or the
  missing one), the failure scenario or reason, and proposed replacement text. It goes
  to the Advisor through the Session (§4.1); the Packet reviewer never applies it itself.
- **Escalation.** When deciding a criterion or a finding needs judgment the review
  cannot settle — device or emulator semantics, architecture, evidence admissibility,
  or whether a suspected defect is real — the Packet reviewer escalates it to the
  Advisor through the Session instead of guessing either way. It names the item, what
  it checked, and the one question that would decide it. An escalation is never a way
  to pass an item it could not verify: until the Advisor rules, the item is open.
- **Disposition.** `ACCEPT` only when every mandatory criterion is `AGREED`, no blocking
  finding is open, and no criteria-change request or escalation is pending; otherwise
  `NOT ACCEPTED`, naming the blocking criteria, findings, requests, and escalations.

The Packet reviewer and the Turn reviewer (§4.5) are bound by §2.4 and by contract
rules 1, 4, 5, 7 and 9 above. They never edit the work, the packet, or the evidence,
and never lower a criterion on their own reading. A criteria-change request cannot turn failed evidence into success: a granted
change is re-measured and re-reviewed (§2.3 hard ceiling).

**Review and disputes** — Muse guardrail plus one independent acceptance stage:

0. The delivered packet first passes Muse post-review. `REVISE` is repaired and sent
   back to Muse; `ESCALATE` goes to the Advisor. Only `CLEAR` proceeds.

1. After Muse `CLEAR`, the fresh Packet reviewer reviews the packet as above. `ACCEPT` is final
   for the exact reviewed tree and evidence.

2. On `NOT ACCEPTED` the Session records the review, then sorts each blocking item:

   - **Evidence failure** — the reproduction failed, or the evidence is missing, stale,
     or does not show the claim. The criterion stays failed; the work or evidence is
     fixed and the affected criteria are re-reviewed (`pending — post-review edits`).
     No role can turn it into PASS (§2.4).

   - **Blocking finding** — the work is fixed and the finding and affected criteria are
     re-reviewed (`pending — post-review edits`). If the Session disputes that the
     finding meets §3.1 or falls within the packet's claim, the Advisor classifies it
     as blocking or advisory (§2.3).

   - **Criteria-change request** — the Advisor rules `KEEP` or `REVISE` with the new
     text (§2.3). `REVISE` is a frozen-packet revision authorised under §5.4; the
     affected criteria are re-measured and re-reviewed against the new text. `KEEP`
     returns the criterion to review unchanged.

   - **Escalation** — the Advisor reads what the item turns on and rules on the
     escalated question (§2.3). Its ruling settles the item as `AGREED`, `DISAGREED`,
     a blocking finding or an advisory; a ruling that needs new evidence sends the item
     back for measurement and re-review.

   - **Contradicting measurements** — the reviewer's reproduction and the delivered
     evidence disagree and neither is shown wrong. That is a factual dispute: it goes
     to the Advisor (§4.2), which orders the discriminating measurement, and the
     reproduced result controls (§2.4).

   - **Interpretation dispute** — the evidence is not in question, but the Session and
     reviewer read an already-frozen criterion differently, or the reviewer returned
     `CANNOT VERIFY` because its wording is ambiguous. This goes to the Advisor as a
     review-dispute ruling (§2.3).

3. **Advisor adjudication.** Review disputes, finding classifications, criteria-change
   requests, and unresolved technical questions go to the session's Persistent Advisor.
   Do not spawn a second technical-authority model solely to create another opinion.

   The fresh Packet reviewer already supplies procedural independence from the
   planning/execution path. If the Persistent Advisor previously participated in the
   disputed policy or packet decision, it must explicitly re-evaluate that prior ruling
   against the Packet reviewer's challenge and the underlying evidence rather than relying on
   its earlier conclusion.

4. The Advisor handling the item reads the evidence it turns on itself rather than
   relying on either side's summary. Per escalated criterion it returns `AGREED`,
   `DISAGREED`, or `CANNOT VERIFY`, with the reading it applied and the evidence that
   would reverse it; per finding, `BLOCKING` or `ADVISORY`; per request, `KEEP` or
   `REVISE`. If what remains is which measurement is right, it returns `CANNOT VERIFY`
   naming the measurement that would decide — never a disposition by authority.

5. If deciding needs a policy, architecture, scope, fidelity or exception decision, the
   Advisor makes it as a separate, recorded technical-policy ruling (§3.3) rather than
   folding it into the disposition. Missing evidence remains missing evidence (§2.4).
   If the required decision is owner-reserved under §3.4, the Advisor asks the owner.

6. `ACCEPT` then requires Muse `CLEAR`, every mandatory criterion `AGREED`, and no blocking finding
   open, by the review or by the Advisor's ruling. That disposition binds for that
   evidence revision and is not re-ruled without new evidence or `PREMISE_CHANGED`.

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
  bounded claim mechanically decidable or avoid one of the concrete wrong outcomes
  above.

**Persistent Advisor** — the project's senior technical decision-maker. "Persistent"
means **continuable throughout one top-level session**; it does not mean that unrecorded
child memory is trusted across sessions. Cross-session persistence comes from recorded
rulings, not from reusing an old child.

Everything the Planner may do, plus the Advisor may:

- resolve any technical question: architecture, device/emulator semantics,
  reverse-engineering method, evidence admissibility, contract interpretation;
- define missing technical or evidence policy (§3.3);
- resolve disagreements among Session, Planner, and reviewer — its ruling is final
  within the limits of §2.4 and §3.4; the Planner may record dissent, but dissent does
  not block;
- rule on review disputes about how a frozen criterion reads (§2.2), reading the
  disputed evidence itself; it interprets wording but never lowers the evidence
  requirement, and a contradiction between measurements is settled by a new
  measurement, not by authority;
- rule on a Packet reviewer's request to change acceptance criteria (§2.2): `KEEP`, or
  `REVISE` with the new text, which reopens the packet under §5.4 and is re-measured
  and re-reviewed; and classify a disputed review finding as blocking or advisory;
- override a literal reading of a **process** rule when that reading defeats the rule's
  purpose, stating the purpose and the override;
- decide whether an uncertainty is material and whether an advisory is worth acting
  on; reclassify any finding in either direction;
- stop an unproductive review or revision loop; tell the Planner or reviewer that a
  finding is non-blocking and deferred;
- approve reasonable technical exceptions to process;
- tell the Session to proceed when further process adds no meaningful confidence;
- change methodology when repeated failures show the current method is the wrong shape.

**Hard ceiling on Advisor overrides.** An Advisor process override cannot:
- override or weaken §2.4 evidence invariants;
- override an explicit owner instruction or a §3.4 owner-reserved decision;
- retroactively change a frozen criterion, threshold, profile, or decision row so that
  failed, missing, contradictory, `UNKNOWN`, or `BLOCKED` evidence becomes successful;
- waive evidence after it has failed;
- turn a factual contradiction into a judgment call.

If a frozen criterion is genuinely ill-formed, the Advisor may rule that it must be
revised; the affected work is then re-measured and re-reviewed. It does not pass on the
old evidence.

The Advisor does not need the owner for other technical decisions. It escalates only a
reserved owner decision (§3.4).

**Accountability instead of gates.** Every discretionary call — defer, waive,
simplify, stop, accept uncertainty, grant an exception, override a process rule — is
recorded in one line in the relevant review record:

`decision; reason; what would reverse it.`

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
   for revision and re-measurement, it does not pass the criterion.

4. **Profiles are fixed in advance.** Exploratory or fixture evidence never satisfies a
   strict criterion. A rule that admits a class of evidence is general, prospective,
   and recorded in its owning document before any evidence relies on it.
   `docs/jsrf-run-profiles.md` owns the profile rules. Bare-minimum milestone criteria
   are not strict criteria: that document admits exploratory runs for them, with ledger
   IDs (§2.4.9).

5. **Absence needs coverage.** Zero hits prove absence only with a positive witness
   that the target would have been observable.

6. **The view is not the artifact.** Truncated, paginated, or summarized tool output
   is not the file. Before a load-bearing count, negative, or provenance claim,
   account for truncation and blank-line handling, and read the underlying range when
   the view may be lossy. When in doubt, the result is UNKNOWN.

7. **Reviews bind to bytes.** A review covers only the exact revision and evidence it
   saw. A post-review edit reopens the affected criteria.

8. **Exceptions relax process, never evidence.**

9. **Shortcuts are recorded, never hidden.** Every path a result relies on that is not
   emulated or translated — a synthetic-completion switch, stub, patch, approximation or
   reimplementation — has an entry in `docs/jsrf-compatibility-ledger.md`, and the run's
   record lists its ledger IDs. A result that relies on an unrecorded shortcut is not
   evidence for anything until the entry exists.

10. **Anti-vacuity is load-bearing.** A checker or test that decides a packet must be
    capable of failing for the behavior it claims to protect. When practical, mutate or
    disable the relevant behavior and prove the expected test fails for the expected
    reason. A surviving mutation is explained, never waved through.

11. **Observe the claimed layer.** Evidence comes from the layer the claim is about. A
    higher layer rejecting the same input does not prove a lower-layer guard fired, and
    a fixture probe does not show what a real guest run does.

12. **Final-tree revalidation.** An earlier green run does not bind later edits. Before
    final packet acceptance, rerun load-bearing validation on the exact candidate tree
    that Muse clears and the Packet reviewer reviews.

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
Plausibility is a judgment call: the Planner or Advisor for packet design, the Packet
reviewer for its own review findings (§2.2), and the Advisor when a classification is disputed.

### 3.2 Advisory

Every other finding: wording, formatting, narrative or history accuracy, record
pointers, incomplete cross-references, stronger-than-needed controls, and hypothetical
attacks outside the packet's stated trust boundary. Advisories are listed in the review
record's **Deferred** section. They never reopen a frozen packet. Correcting a
non-packet record needs no re-review.

### 3.3 Technical-policy ruling

An Advisor decision that settles a technical question the project documents leave open
or ambiguous — evidence admissibility, methodology, device semantics, interpretation
of a criterion, a dispute, a loop stop, or an exception. It binds every role from the
moment it is recorded and states:

- the question and decision;
- observed / inferred / uncertain basis (§2.4.1);
- what would reverse it;
- where it is recorded.

General rules are recorded in the document that owns the topic (§8); case rulings go in
the review record. The Session records the ruling text verbatim with the Advisor child
ID, model, provider, effort, and whether the child satisfied the continuability probe.
A paraphrase cites that record. An attribution that cannot be traced to a recorded
Advisor response is `UNKNOWN`.

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
Worker           -> Session
Session          -> Planner        (packet design, adequacy, whether to revise)
Planner          -> Advisor        (policy gap, methodology, architecture)
Session          -> Muse guardrail (routine preflight and post-execution checks)
Muse guardrail   -> Advisor        (policy/criteria/architecture/evidence decision)
Packet reviewer  -> Advisor        (via Session; escalation, dispute, finding classification, criteria-change request)
Turn reviewer    -> Advisor        (via Session; a disputed turn-end item, §4.5)
Advisor          -> Owner         (§3.4 only; all technical questions end at Advisor)
```

If the Advisor is unavailable, that is a staffing blocker; the Planner still decides
within its own authority. If Muse is unavailable, work waits at the applicable side of
the sandwich rather than silently skipping the guardrail.

**The owner may message any agent directly**, including a child. Such a message is an
owner instruction, not an injection: follow it, record it, and tell the Session if it
changes scope. It outranks every role's ruling.

### 4.2 When to consult the Advisor

Each trigger below is a consult, not an option.

A contract role consults the Persistent Advisor when:

- the same root-cause failure survives **two attempts**, or each next step is a guess;
- two credible measurements contradict each other;
- about to rely on a universal or negative claim (`impossible`, `rules out`, `nothing found`);
- an expensive build, regeneration, or investigation rests on an unverified premise;
- a criterion or step is ambiguous, contradicted, or cannot be executed as written;
- work would need a policy, criterion, scope, or threshold change;
- the Session and a reviewer disagree;
- Muse returns `ESCALATE`;
- a new blocker appears before investigation and its mechanism is not already established;
- choosing between technical approaches that differ in architecture, fidelity, or which
  code they touch;
- a worker returns a surprising result that contradicts a recorded finding, ruling, or
  expectation;
- the Session is about to tell the owner a causal conclusion it inferred rather than
  measured.

The **Persistent Advisor is the final technical escalation point**. Difficulty,
uncertainty, disagreement, or repeated failure does not create another model tier above
it.

If contradictory observations remain, the Advisor orders the cheapest discriminating
measurement and lets reproduced evidence control. If the current method is failing, the
Advisor changes methodology, creates discovery work, narrows the claim, or stops the
line of work.

Only when the remaining decision is genuinely owner-reserved under §3.4 does the
Advisor ask the owner.

No third blind attempt follows two failures of the same root-cause mechanism.

**Not an Advisor question:** a fact a worker can read or measure; a question already
answered by project authority or a recorded ruling; the same question again without new
evidence; mechanical choices with no effect on the claim; the routine Muse
pre/post-execution guardrail itself.

A required consult is never skipped to stay inside a packet's senior-call budget.

### 4.3 Briefing the Advisor

Give the smallest `READ YOURSELF` set that could change the decision, and one bounded
question. Use the right consult:

- **Fault diagnosis:** rank plausible mechanisms; give the cheapest discriminating
  experiment for each.
- **Architecture/policy:** options, trade-offs, decision, what would reverse it, and
  the document that owns the rule.

```text
You are the Persistent Advisor (docs/agent-workflow.md §2.3) on a static-recompilation
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

**Quick consult.** For a bounded judgment check — which of two approaches, whether a
result is surprising, which mechanism to test first — the Session sends at most ten lines
to the session's continuable Advisor child: the question, the files to read, and what it
has measured. The Advisor answers in at most ten lines, ending with `REVERSED BY:`. A
quick consult that turns out to need a ruling becomes one under the full template. Quick
consults keep consulting cheap enough that cost is never the reason to skip one; the
Session records each answer it acts on in one line in the relevant record.

Follow-ups may be delta briefs. A delta does not freeze earlier premises: if a
correction changes a load-bearing premise, mark it `PREMISE_CHANGED` and ask the
Advisor to reconsider every ruling that depended on it. Never instruct the Advisor not
to revisit a premise.

### 4.4 Advisor and Muse continuity

The Persistent Advisor and Muse guardrail are both session-continuable children, but
they are separate roles and separate conversations.

**Persistent Advisor**

1. resolve the exact §1 Advisor provider/model;
2. use a continuable-capable spawn surface at the listed effort;
3. verify the child appears in the continuable-agent listing;
4. verify a second message reaches the same child;
5. retain the child ID for later rulings.

**Muse decision guardrail**

1. spawn on the §1 Muse route;
2. verify the child is a real DSH child on that route;
3. verify the listed effort and session-scoped workspace;
4. verify listing visibility and second-turn continuation;
5. retain the child ID for routine preflight/post-review.

Do not seed a new Advisor or Muse guardrail from the current chat transcript. Brief from
files and durable records. Recorded rulings and accepted review records, not child
memory, carry authority across top-level sessions.

Every Packet reviewer and Turn reviewer is a fresh child, separate from both persistent
children.

If a required continuation mechanism is unavailable, work needing that role is
`BLOCKED`; do not invent an invocation.

### 4.5 Turn-end review

**Before the Session replies to a prompt and ends its turn, the Turn reviewer performs a
final review.** Its purpose is to stop the Session ending a turn prematurely: stopping with
work the prompt asked for still undone, with a required step skipped, or with a reply
that claims more than was done.

The Session spawns a fresh Turn reviewer child on its §1 route and briefs it with:
the prompt verbatim; the Session's draft reply; both repositories' identities, status
and the commits/diff made during the turn; and the packet or chore state it touched. The
brief is a lead; the Turn reviewer checks the repositories and artifacts itself
(§2.4.2).

The Turn reviewer checks:

1. **Done.** Everything the prompt asked for was done, or each undone item names a
   legitimate stop: an explicit stop boundary (§2.2 rule 9), a `BLOCKED` result with
   evidence, a pending review or ruling, or a §3.4 owner decision.
2. **Required closure.** What this file and `AGENTS.md` require at this point happened:
   review records written (§2.2 rule 10), durable work committed, and both repositories
   pushed at a push checkpoint, with each push's preconditions met.
3. **Honest reply.** Every claim in the draft reply is supported by an artifact or
   command, and nothing that failed, was skipped, or is `UNVERIFIED` is reported as done.
4. **Advisor use.** A §4.2 trigger that fired during the turn without a consult. Where
   the harness log of the Session's turn is readable, the Turn reviewer reads it itself rather
   than asking the Session to summarise. If the draft reply relies on the conclusion the
   consult should have checked, that is `CONTINUE`: consult before the reply goes out.
   Otherwise it is recorded under `ADVISOR`.

It returns:

```text
TURN_END: END | CONTINUE
REMAINING: <each item: what is undone; evidence; why no legitimate stop covers it> | NONE
REPLY_CORRECTIONS: <each unsupported or overstated claim> | NONE
ADVISOR: <each §4.2 trigger that fired without a consult> | NONE
```

On `CONTINUE` the Session does the remaining work, corrects the reply, and requests a
new turn-end review on a fresh child. If the Session disputes a `REMAINING` item (out of
the prompt's scope, needs the owner, or covered by a stop), the Advisor rules on it. If
two consecutive turn-end reviews return `CONTINUE` on the same item, the Session takes
that item to the Advisor rather than looping.

A turn-end review is not acceptance: it never `ACCEPT`s a packet, adds criteria, or
substitutes for §2.2's review. If the Turn reviewer route is unavailable, the Session may end
the turn but must say in its reply that the turn-end review did not run.

## 5. Packet lifecycle

### 5.1 Planning and authorship

Planning decides **what to find out or build next**. It does not find it out. The
Planner is not required to solve a technical problem before designing the packet that
investigates it.

**1. The brief is frozen.** The Session opens planning with one evidence brief: the
blocker, the runs and artifacts that show it, and anything already measured. It may
gather that evidence first, including bounded diagnostic runs. Once the brief is sent,
the Session sends the Planner nothing new unless it **refutes the brief's premise**;
other findings wait for the packet. Policy edits also wait unless the Planner asks for
one. A Planner working against a brief that changes every few minutes cannot finish.

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
and multi-step analysis of dumps or binaries are execution: they belong inside a
packet. A question that needs outside knowledge goes to the Advisor as one bounded
question. The one file the Planner writes is its own draft packet.

**4. Sketch first, with an early Advisor shape preflight and escalating checkpoints.**

The Planner's goal is a sketch of at most about 15 lines: claim, class (discovery or
change), unknowns, experiment, and outcome rows. It writes the sketch at the top of its
draft packet file under a `Sketch` heading, where it survives compaction and the owner
can read it.

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
the decision needs; the checkpoints limit investigation.

At every checkpoint the Planner writes its **current sketch** into the draft, however
rough, with unknowns stated as unknowns. It also writes a **forecast** if it continues:
the specific reads it will make next, and what it expects them to change in the sketch.
From 40 calls on it writes a **yield** as well: what the last 20 calls actually changed,
compared with the forecast.

| At | The Planner | The Advisor |
|---|---|---|
| **first viable sketch / ≤20 calls** | writes sketch 1 and, for a change packet/material redesign, sends it for shape preflight before expanding the packet; after `PROCEED`, either writes the packet or writes a specific forecast and continues | returns `PROCEED`, `REDIRECT`, or `DISCOVERY_FIRST`; expands into a technical ruling only if a policy/architecture question is actually present |
| **40 calls** | writes sketch 2, yield against the prior forecast, and a new forecast; sends all three to the Advisor, then continues unless redirected | decides whether the investigation is still paying; may answer, redirect, or stop it |
| **60 calls** | stops investigating; writes sketch 3 and the yield against the 40-call forecast; sends both, plus what is still missing, to the Advisor | must decide: extend planning with an explicit bounded list of allowed reads, or have the Planner write the packet with the gaps as its subject |

The yield tells the Advisor whether investigation is still paying. If a 20-call
stretch changed nothing that matters in the sketch, that is the signal to stop, and the
Planner should say so rather than wait to be told.

The Advisor does **not** perform a full second review of every completed Planner packet.
After `PROCEED`, it becomes involved again only at the checkpoints above or when the
Planner encounters a genuine Advisor-class question: architecture/device semantics,
evidence admissibility, workflow methodology, deletion/waiver of a previously
protective requirement, acceptance of material uncertainty, fidelity tradeoff,
conflict with an existing ruling, or repeated failure requiring methodology change.

Every checkpoint decision is written into the draft, so the trail shows why planning
ran long. A Planner that reaches 40 or 60 is a signal to the owner and Advisor, not a
failure by itself.

**4b. Muse execution preflight before adequacy/freeze.**

After the Planner expands the packet and the Session fills/verifies its mechanical
commands, but **before** the fresh-Planner adequacy review, the session's Muse guardrail
reads the candidate packet and the smallest current source/evidence set needed to judge
execution shape. It runs **once per packet**, on the first full draft; a routine
discovery packet skips it (§2.2).

It returns the `MUSE_PREFLIGHT` contract from §2.2.

- `PROCEED` sends the same draft to adequacy review.
- `REDIRECT` sends it back to the Planner for bounded revision, and Muse re-checks only
  the redirected points.
- `DISCOVERY_FIRST` requires a discovery packet for the missing fact.
- `ESCALATE` goes to the Persistent Advisor.

Because this happens before adequacy/freeze, resolving a Muse preflight finding does not
reopen a frozen packet.

**5. Authorship.**

1. The **Planner** states the bounded claim and non-goals and owns the criteria and
   decision rows. It may write them itself or accept a Session draft.
2. The **Session** fills the mechanical parts (commands, paths, hashes, environment)
   and verifies that every command runs before submitting the revision.
3. **Change packets:** adequacy review is by a Planner child. If that child wrote or
   materially rewrote the criteria or rows of the revision, the review goes to a
   **fresh** Planner child on the §1 Planner route, never the child that wrote the
   revision. The Advisor's shape
   preflight is not the adequacy review and does not replace it.
4. **Discovery packets:** the writing Planner reviews its own packet against §5.3's two
   blocking questions; no second Planner is spawned. Either way, final independence
   comes from the Packet reviewer reviewing the work and reproducing the evidence.

### 5.2 States

| State | Meaning | Exit |
|---|---|---|
| **draft** | being written | Muse preflight, then adequacy review |
| **MUSE REDIRECT** | preflight found a concrete packet-shape/test issue | Planner revision |
| **MUSE DISCOVERY_FIRST** | preflight found implementation would be guessing | discovery packet |
| **MUSE ESCALATE** | guardrail found Advisor-class decision | Advisor ruling |
| **INADEQUATE** | adequacy review found ≥1 blocking defect | revision (§5.4) or retirement |
| **ADEQUATE** | review found zero blocking defects (a change packet also has `MUSE_PREFLIGHT: PROCEED`) | **frozen and promoted in the same step** |
| **promoted** | frozen hash in `CURRENT PACKET` | Session and workers execute |
| **delivered** | Session says criteria pass with evidence | Muse post-execution review |
| **MUSE REVISE** | post-review found repairable defect without policy change | repair + revalidate + Muse post-review |
| **MUSE CLEAR** | post-review found no blocking guardrail issue | fresh Packet reviewer |
| **disputed** | Packet reviewer returned `NOT ACCEPTED` and a dispute/classification remains | Advisor ruling (§2.2) |
| **pending — criteria change** | Packet reviewer asked Advisor to change criteria | Advisor `KEEP` or `REVISE` (§5.4) |
| **pending — reviewer escalation** | Packet reviewer escalated an item it could not settle | Advisor ruling, then measurement or re-review as the ruling requires |
| **accepted** | Muse `CLEAR`, all mandatory criteria `AGREED`, and no blocking review finding open | record; plan names next work |
| **pending — CANNOT VERIFY** | reviewer cannot reproduce a criterion | new evidence; rerun affected sandwich stages |
| **pending — reviewer unavailable** | reviewer route failed | repair route; no substitution |
| **pending — post-review edits** | reviewed tree/evidence changed | Muse post-review + fresh Packet reviewer for affected criteria |
| **escalated** | policy/architecture/scope/fidelity/exception question | Advisor ruling, then rerun/re-review as needed |
| **retired** | premise failed or objective changed | recorded in plan |
| **exploratory evidence** | artifact used bypass/synthetic completion | cannot satisfy strict criteria |

### 5.3 Adequacy review

The Planner reviews the packet itself, not the Session's description of it, and returns:

```text
REVISION:          <packet revision + SHA-256 it read>
READ:              <files/commits/runs and ranges read directly>
PREMISE_FRESHNESS: PASS | BOUNDED | FAIL   (§5.6)
BLOCKING:          <each: location; failure scenario; required outcome> | NONE
DEFERRED:          <advisories> | NONE
DECISIONS:         <one line each: decision; reason; what would reverse it>
VERDICT:           ADEQUATE | INADEQUATE
```

`VERDICT` is `ADEQUATE` exactly when `BLOCKING` is `NONE`,
`PREMISE_FRESHNESS` is not `FAIL`, and, for a change packet, Muse preflight has
returned `PROCEED` (§5.1.4b; once per packet, not per revision). There are no other counts or hidden conditions.

For a **discovery packet** (§5.8), the writing Planner reviews its own packet. The two
special discovery questions determine **BLOCKING**:

1. could an outcome be misread into the wrong row?
2. is the packet safe and reversible?

Those two questions do **not** waive premise freshness. `PREMISE_FRESHNESS` remains
mandatory for discovery packets exactly as above; `FAIL` still forces `INADEQUATE`.
The Session confirms every command runs, records the review block, and freezes and
promotes on `ADEQUATE` as for any packet. A misread outcome row is caught again at
acceptance, where the reviewer checks row selection.

**ADEQUATE ends plan iteration.** The Session freezes that exact revision, records the
review, and promotes it into `CURRENT PACKET` in the same step, with no discretion to
revise first. Deferred advisories stay deferred.

### 5.4 Revising

**After INADEQUATE:** the revision repairs the blocking defects. It may also fix cheap
operative advisories, but adds no narrative about the repairs. Re-review covers the
blocking defects, changed regions, and a contract-completeness check; advisory hunting
in unchanged text is not requested.

**After ADEQUATE**, a frozen packet may be revised **only** when a Planner or Advisor
authorizes it, recording one of:

1. a finding from any source, including execution, meets the blocking test (§3.1) with
   a concrete scenario;
2. a load-bearing premise is invalidated by new evidence (`PREMISE_CHANGED`);
3. an Advisor policy ruling changes a policy the packet depends on;
4. the Advisor grants a Packet reviewer's criteria-change request (`REVISE`, §2.2);
5. the owner changes the objective or scope.

Nothing else reopens a frozen packet: not advisories, wording, history, record pointers,
narrative accuracy, formatting, a reviewer's idea for another control, or a wish to
clear findings. A step that is ambiguous but not wrong is settled by an Advisor
interpretation ruling that execution follows, not by a revision.

### 5.5 Churn and redesign

A change packet passes three checks before execution — Advisor shape preflight, Muse
preflight, adequacy review — and two after it — Muse post-review and the Packet
reviewer. These limits keep them from looping:

1. **Muse preflight runs once.** After `PROCEED`, revisions go back to adequacy review
   only. Muse preflights again only after `PREMISE_CHANGED` or a redesign under rule 5.
2. **Re-checks cover what changed.** Any check after a revision or repair reads the
   changed regions and the points it raised, not the whole packet again.
3. **No re-litigating.** A check does not reopen what an earlier check, an adequacy
   verdict or a ruling already decided, unless it states a §3.1 failure scenario that
   the earlier decision did not consider.
4. **Two on the same mechanism go up.** Two consecutive send-backs on the same
   criterion or mechanism — `REDIRECT`, `INADEQUATE`, or `REVISE` — go to the Advisor,
   not to a third round.
5. **Three send-backs in total go up.** A packet sent back three times before execution,
   by any mix of checks, goes to the Advisor, which decides: proceed as is, redesign,
   or stop. On redesign the Planner rewrites that criterion or method instead of
   patching it again.

The Planner or Advisor may also stop a loop at any time by recording the decision (§2.3).

**A criterion whose evaluation is itself an analysis belongs in a discovery packet.**
If deciding a criterion requires multi-step static or dynamic analysis of a binary or a
run — work the Planner cannot complete by reading — then the criterion is asking a
contract to *produce* evidence rather than check it. Put that analysis in a discovery
packet (§5.8) that runs first, and have the later change packet cite the accepted
outcome as a pinned precondition or evidence source.

### 5.6 Premise freshness

Before an adequacy verdict the Planner judges whether the evidence that motivates the
packet still holds for the target revision. The Session may prepare the facts; the
Planner decides whether they are enough. Consider: whether the cited artifact exists
and shows the event; its evidence profile; the source/build it represents; later
commits that may have fixed or superseded it; whether the failure is current or only
historical; and a coverage witness for absence claims.

- `PASS` — premise established for the target revision.
- `BOUNDED` — uncertainty remains, is stated, and the packet fails closed if the
  premise is wrong.
- `FAIL` — premise refuted or unsupported in a way that could yield a false result;
  packet is `INADEQUATE` or retired.

Do this once per packet, and again only on `PREMISE_CHANGED`, not every revision.

### 5.7 What a frozen packet contains

The operative contract only: claim and limits, identity and profile, steps, criteria,
decision rows, pins, stop conditions, and closure. No revision history, `"Fixed:"`
notes, or rationale about earlier revisions. Those go in one log:

`docs/reviews/<packet>-revision-history.md`

which is non-authoritative, is not reviewed for accuracy, and loses to the contract on
any conflict. The packet carries a one-line pointer to it.

### 5.8 Packet classes

Three classes. A **chore** is owner-directed mechanical work; a **discovery packet**
obtains knowledge; a **change packet** changes behaviour or makes an acceptance
claim.

| | **Chore** | **Discovery packet** | **Change packet** |
|---|---|---|---|
| Output | a mechanical result with its own gate output | knowledge: what was observed | a behaviour change, or an acceptance claim |
| Authorized by | the owner, in the authoritative plan, **by name** | the Planner | the Planner |
| May do | build, test, sync, regenerate, tooling, record fixes, environment repair | read anything; add diagnostic-only instrumentation; run exploratory or fixture profiles | anything its contract authorizes |
| Instrumentation | none it introduces silently | reversible, trace-only or behind an environment variable, off by default at closure | production code under full review |
| Contract | none; the gate script is the contract | about one page (§6.3) | full contract (§6.1–6.2) |
| Adequacy review | **none** — the gate script replaces it | writing Planner, two blocking questions + premise freshness (§5.3) | a Planner that did not write it, full review (§5.3) |
| Review | Muse post-check (§2.2); otherwise the gate's own output is the record | Muse post-review, then the Packet reviewer confirms artifacts exist, match commands, and select the recorded outcome row, and reviews any instrumentation | Muse post-review `CLEAR`, then full independent review (§2.2): work reviewed, every criterion reproduced |
| Can claim | that the mechanical steps ran and what they produced | `"observed X under profile Y"` | what its criteria establish |

**A chore needs no packet.** It is named as a chore in the authoritative plan (the
plan's task tables and its §3 task classes), and its closure is
`scripts/chore-gate.py`'s output plus the Muse post-check rather than a review. That is the whole point: the
measured failure is the v0.11 sync taking **19.6 h and six revisions as packet A4s**,
while the owner's direct v0.12 sync (121 upstream commits) landed as one merge commit
and one record commit seven minutes apart (`2925f0b`, `32680d7`). The packet
machinery added nothing to a mechanical sync and cost a day.

**A chore may add a bare-minimum shortcut** — a patch, stub, approximation,
reimplementation or switch — when it adds the ledger entry in the same commit
(§2.4.9); that is how the plan's title-screen fast path runs.
**It still may not change admitted evidence semantics:** it may not re-gate or remove an
admitted model (`docs/jsrf-run-profiles.md` "Listed models") or change what counts as
strict evidence. That is a change packet, or a classification in the run-profile
document — that document's decision, not the chore's.

**A chore that fails its gate is a finding, not a failure to hide.** The gate's
output is recorded as-is, including a changed stop site. "The stop changed" is a
result; a chore that reports only success is not reporting.

A discovery packet never itself satisfies a strict criterion and never claims that
something works. **An accepted discovery outcome may, however, be pinned as a
precondition or cited as an evidence source by a later change packet.** The later
change packet still owns its strict acceptance; the discovery result does not become
strict evidence merely by being cited.

A discovery packet's outcome table names the next packet for each result, so closing it
hands the Planner its next brief directly. Prefer discovery whenever the next
implementation depends on facts nobody has observed. Keep it small enough to execute
in one session. Leaving diagnostic instrumentation enabled after closure requires a
change packet.

## 6. Packet construction

### 6.1 Drafting checklist

This list is for change packets; a discovery packet uses §6.3. Whoever drafts works
through it; the Planner may waive an item with a one-line reason.

**Enumerating guest accesses to an address.** A criterion that claims something about
*every* access to an address must not build its population by searching text. One
address can be spelled more than one way in generated code, so a text search can
silently cover a subset. Derive the population from the original XBE instruction
stream with operands normalized to `uint32`, reconcile it against generated code by
normalized **value** rather than spelling, freeze the count and the command that
produced it inside the criterion, and make PASS conditional on
`count found == frozen count` with every listed site evaluated. Any difference is
`UNKNOWN`, never PASS. State what the method cannot see: register-indirect, computed,
and table-driven accesses. A text search is a lead, never a completeness witness.

**Stating the enumeration method.** Completeness and uniqueness claims state their
enumeration method. Linear-sweep decode is inadmissible for completeness without a
drift control. Use recursive descent or an equivalent control-flow-following
enumeration. Byte-pattern scans are alignment-independent for existence; for uniqueness
(`"exactly one"`) state the encoding coverage over all instruction forms that could
carry the pattern. Raw-byte scans are a sound fallback for existence and displacement
uniqueness when instruction boundaries are not mechanically established.

**Merge packets.** A packet that merges upstream must inventory device- and
profile-relevant hunks **by content, not conflict status**, because a clean hunk can
restore a deleted override or arm new device behavior as silently as a conflict can
hide it. The local admitted form wins for anything touching an admitted model or a
classifier-listed/deleted variable, and the merged tree is checked for those names and
arming call sites. See `docs/jsrf-run-profiles.md` for merge/evidence-profile policy.

1. **Objective first.** State bounded claim and explicit non-goals.
2. **Stable IDs.** One criterion = one independently decidable obligation.
3. **Guards against.** Each criterion names the wrong outcome it prevents. Delete a
   criterion that prevents none.
4. **Profile before measurement.** Fix strict/exploratory/fixture/manual scope,
   identities, instrumentation, and permitted overrides first.
5. **Reproducible procedure.** Working directory, tools, inputs, command, environment,
   bounds, artifact destination. Missing tooling is a prerequisite, not an invented
   command. Every command has been run before freezing.
6. **Decision rule.** PASS, FAIL, and UNKNOWN/BLOCKED are each defined; missing,
   malformed, stale, unexercised, or empty evidence never defaults to PASS.

   **Decision inputs are lossless by construction.** A criterion may select a row only
   from a record that cannot drop the deciding event: a write-once latch or an uncapped
   counter updated at the event by the code that performs it. Capped, sampled,
   rate-limited, or first-N logs are **observation only**. Absence of a witness is never
   positive attribution: it selects `UNKNOWN` or an explicit unattributed row, never a
   row that blames a specific agent. The packet that owns the code producing a decision
   input also owns and fixture-tests that input's semantics; a consuming packet only
   reads it.

6b. **Decision inputs are bounded by construction.** A record a row decides from must
   have a size fixed by a **finite universe that is stated and derived from source**,
   independent of run length and input volume. Key it by the **property the decision
   classifies**, not by individual event identity. A table whose key universe is not
   shown finite is **observation only**. Overflow or out-of-universe counters are bug
   detectors; if a record can overflow simply because the run was long or busy, the
   key is wrong. Completeness of instrumentation is established structurally by
   enumerated hook sites and fixture coverage, not by counting distinct keys at run
   time. A criterion whose only role is to qualify a PASS is evaluated only when that
   PASS holds.

7. **Exercise and controls.** Absence claims need a coverage witness; a new checker
   needs known-good and known-bad controls; the oracle is not derived solely from the
   implementation under test.
8. **Separate claim classes.** Structural correctness, exercised ABI behavior, strict
   reachability, device semantics, and liveness are separate obligations.
9. **Scope and stops.** Population, window, coverage, write scope, and `Stop if`.
10. **Mechanical closure.** Criterion -> artifact/hash -> result -> reviewer
    disposition on one declared evidence revision.
11. **Observability first.** If success and failure cannot be distinguished, build and
    validate tooling in an earlier packet.

12. **Anti-vacuity.** For each load-bearing checker/test, state the known-bad control or
    focused mutation that would make it fail for the intended reason when practical.

13. **Real guest path.** When a claim is about guest or runtime behaviour, include at
    least one validation on a real guest run under the declared profile, not only a
    fixture probe (`AGENTS.md` "Probe runs").

14. **Final-tree binding.** State which load-bearing commands are rerun after the final
    accepted edit set so Muse and the Packet reviewer see evidence from the exact candidate
    tree.

Before release ask both questions:

> Could broken behavior, an unexercised path, an exploratory run, stale evidence, or
> an empty input satisfy these checks? If yes, tighten.

> Does every requirement protect the objective against a named wrong outcome? If not,
> delete it.

### 6.2 Packet template

```markdown
## <Packet ID> — <bounded outcome>

**Contract revision:** <revision>   **Status:** draft
**Governing requirement:** <requirement or link>
**Depends on:** <accepted packet IDs/revisions>
**Baseline:** <game/toolkit revisions + dirty-state identity>
**Senior-call budget:** <N Planner, N Advisor, N Muse guardrail, N review> (W13)
**Revision log:** docs/reviews/<packet>-revision-history.md (non-authoritative)

### Motivating evidence
- <artifact/run/commit> — profile <...> — source/build <...> — supports <claim>

### Load-bearing premises (W10)
Each premise this packet depends on, with the **byte-level command** that
establishes it, so the reviewer re-runs them first:
- <premise> — `<command>` — <expected output or hash>

### Dry-run transcript (W3)
Every command in `### Execution`, executed on the target host and tree before
freezing, with its output hashed. A frozen command that has never run is a command
that may not run at all.
- `<command>` — exit <N> — output sha256 `<hash>`

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

**W13's budget is a field, not a hope.** "Per-task senior-call budget (§3), recorded
in the packet; exceeding it is an Advisor continue/stop decision", answering "change
packets consumed 15–27 senior calls; discovery 2–4". A packet whose budget is
exceeded stops for an Advisor continue/stop rather than continuing to spend.

**W3's transcript is a prerequisite to freezing, not paperwork.** The measured
failures are "frozen commands that never ran (A4s-r4 anchors, A4s-r5 PowerShell 5.1
grep; 7 of 12 A4s Advisor rulings)". `scripts/check-packet-transcript.py` verifies
the section exists and that every command in `### Execution` appears in it.

**W10's premises are re-run first.** The measured failure is "false ACCEPTs on false
premises (OOM slice, named-producer-frame)". A premise with a byte-level command is
one the reviewer can falsify; a premise stated in prose is one they must trust.
`scripts/check-record-hygiene.py` rejects a value cited from a `CONTENT_MISMATCH`
dump or a run with tracing off.

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
| **decision sandwich** | senior roles freeze the bounded question/policy; Muse guards immediately before and after execution; a fresh Packet reviewer decides acceptance |
| **model diversity** | a different model family from the work or decision layer being checked |
| **procedural independence** | reviewer/adjudicating role did not author the work or decision it is judging and is asked to falsify, not confirm |
| **evidence reproduction** | reviewer re-ran the load-bearing measurement |
| **continuability** | the same child can receive a later message through the harness continuation API; a one-shot response does not qualify |
| **route identity** | the canonical provider/model actually used, resolved live rather than inferred from a display label |
| **guardrail clear** | Muse found no blocking pre/post execution issue; it is not final packet acceptance |

Model diversity helps; it never replaces reproduced evidence. Muse and the Packet
reviewer provide different checks: Muse has current-session context and catches
decision/execution drift; the fresh Packet reviewer supplies independent acceptance.
Continuability and route identity are separate properties and must be verified
separately when §1 requires both.

## 8. Document ownership

| Fact | Owner |
|---|---|
| roster, role authority, decision-sandwich lifecycle, packet lifecycle, escalation, adequacy verdict | **this file** |
| operating/build/runtime discipline | `AGENTS.md` |
| evidence-profile semantics, strictness, admitted model classes | `docs/jsrf-run-profiles.md` |
| current packet, blocker, next action, milestone acceptance | `plan-jsrf-bare-minimum.md` |
| a packet's operative contract | `docs/packets/<packet>.md` |
| Muse preflight/post-review results, Packet reviewer verdicts, deferred advisories, discretionary decisions, case rulings | `docs/reviews/<packet>-r<N>-*.md` |
| a packet's revision narrative | `docs/reviews/<packet>-revision-history.md` (non-authoritative) |
| current top-level-session readiness receipt | `docs/reviews/startup-current.md` (rolling, non-historical) |
| durable staffing/readiness facts for accepted work | the packet's durable acceptance/review record |
| current-session Advisor/Muse child IDs | `docs/reviews/startup-current.md` only; never cross-session authority |
| historical handles from retired Muse routes | `.muse-workers.md` (history only; never readiness authority) |
| transient session notes | outside the repository |
| dated narrative | `docs/jsrf-operating-history.md` |

Do not copy the roster outside §1. The Advisor decides general technical rules; the
Planner applies them to packet design; Muse checks decision/execution alignment; the
Packet reviewer decides acceptance; the Session records the durable result.

`startup-current.md` is intentionally bounded operational state. Before it is replaced,
any staffing or route facts that materially support accepted packet work must already
exist in the packet's durable review/acceptance record.
