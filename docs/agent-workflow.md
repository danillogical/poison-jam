# Agent workflow: models, the session loop, and escalation

This file is the **single authority** for how a JSRF session is staffed and how it
works. It replaces `grok-role-map.md` and `deepseek-harness.md`, both retired
2026-09-22. `AGENTS.md` carries the one-screen version of this at its head,
because that is the only file loaded automatically; everything below is the detail
behind it.

Read this once at session start, then work. Do not re-read it per packet.

---

## 1. The two supported harnesses

Exactly two harnesses are supported. Pick the row that matches the harness you are
running in, and use only those models.

| Role | Codex | DeepSeek Harness (DSH) |
|---|---|---|
| **Session** | `gpt-6-luna` @ `high` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Worker subagents** | `gpt-6-luna` @ `high` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Persistent advisor** | `codex/gpt-6-astra` @ `medium` | `codex/gpt-6-astra` @ `medium` |
| **Acceptance reviewer** | `gpt-6-luna` @ `max` | `workbuddy-ai/hy4-preview-f` @ `high` |

**Everything else is retired.** Do not select `gpt-5.6-sol`, `gpt-5.6-luna`,
`gpt-5.6-terra`, `gpt-5.5`, Grok, `grok-cli`, `hy3`, `glm-5.3`, `kimi-k3`, or any
`workbuddy-ai/gpt-*` route. Those names appear throughout the historical sections
of the reports and in old commit messages; read them as history, never as a roster.

### Measured constraints on this table

Verified with `list_subagent_models` 2026-09-22. These are not preferences; they
are what the routes actually serve, and two of them change how you work:

- **In DSH the advisor is `codex:gpt-6-astra` — stated by the user directly, and
  it is the one cross-provider call in the table.** `workbuddy-ai` does **not**
  serve `gpt-6-astra` (measured: that provider advertises only `hy4-preview-f`,
  `deepseek-v4.1-flash` and `gpt-5.5`). So the DSH advisor comes from the `codex`
  provider even though the session itself is on `workbuddy-ai`. That is deliberate:
  reaching across providers is exactly what makes the advisor independent of the
  session. Do not "fix" this by looking for an Astra route on `workbuddy-ai`.
- **`workbuddy-ai/hy4-preview-f` advertises exactly one effort: `high`.** The
  reviewer's effort is not a choice on DSH. Do not "raise it to max"; there is no
  such setting, and the attempt fails.
- `workbuddy-ai/deepseek-v4.1-flash` serves up to `max`; `codex/gpt-6-luna` serves
  up to `max`; `codex/gpt-6-astra` serves up to `ultra`.

**Independence, stated honestly, because the two rows are not equivalent.** The
DSH reviewer (`hy4-preview-f`) is a **third model family** — it shares neither the
session's nor the advisor's blind spots, which is the strongest form of the check.
The **Codex reviewer is `gpt-6-luna`, the same model family as the Codex session**,
differing only in effort (`max` vs `high`). That is a weaker check: a higher effort
of the same model can still share a systematic misreading with the session. Treat a
Codex review as a careful second pass, not as independent confirmation, and when a
Codex packet's acceptance is genuinely load-bearing, send the disagreement to the
advisor rather than relying on the reviewer's agreement. Do not silently upgrade
the claim to "independently verified".

---

## 2. The session loop

A session does this, in order, without being asked:

1. **Confirm the roster.** Read this table and resolve your own route. On DSH,
   confirm the reviewer route exists with `list_subagent_models` **before**
   promising a review, and say plainly if it is missing rather than substituting a
   model this policy did not ask for. The route list is frozen at session
   composition: a route added to settings mid-session is not usable by that
   session.
2. **Read the plan.** `plan-jsrf-bare-minimum.md` owns acceptance criteria and
   statuses. Then read the `CURRENT STATE` block at the top of
   `report-deepseek.md`, which owns the current blocker, the evidence revision and
   the next packet. The plan overrides any "next packet" wording in the report's
   historical sections.
3. **Work the next packet in the main session.** The session does the work itself.
   It does not hand implementation to a worker and wait. Spawn a worker only for
   **context isolation** — reading a large log, dump or artifact and returning a
   bounded summary so the raw content never enters the session's window. A worker
   is a reader, not a decider: it returns `file:line` evidence marked *measured* or
   *inferred*, and the session adjudicates. **One owner performs build,
   regeneration and run — never a worker.**
4. **Work until the packet's acceptance criteria pass.** The plan defines them.
   When they pass, the packet is *delivered*, not *accepted*.
5. **Get the acceptance reviewer to confirm it.** Spawn the reviewer from the
   table above and require it to **verify or refute each criterion independently** —
   reproducing the load-bearing measurements itself — returning per-criterion
   AGREED / DISAGREED / CANNOT VERIFY with the command or `file:line` behind it.
   Ask for the falsification rather than the confirmation, and require a
   **positive control** wherever it checks that something is absent. Record the
   review in `report-deepseek.md`.
6. **Escalate a disagreement, do not out-vote it.** If session and reviewer
   disagree, both positions and their evidence go to the advisor, whose call is
   **final**. Do not out-vote the reviewer; do not let it out-vote the session. An
   unresolved disagreement usually means a *measurement* is broken, which is
   exactly the case the advisor exists for.

### Closing a packet — the transitions, including the awkward ones

"Delivered" and "accepted" are different states and the gap between them has more
than one exit. All of these are legitimate; only the first is success.

| state | meaning | what unblocks it |
|---|---|---|
| **delivered** | criteria met as measured by the session | a reviewer must verify |
| **accepted** | the reviewer reproduced the criteria and AGREED | nothing — record it |
| **pending — CANNOT VERIFY** | the reviewer could not reproduce a measurement | **new evidence, or an explicit advisor ruling on that criterion.** The session's own green test does *not* close it |
| **pending — reviewer unavailable** | the route is missing or the spawn failed | escalate; **do not substitute a model the policy did not name** |
| **pending — post-review edits** | the tree changed after the review | re-review the affected criteria; a review covers the revision it saw |
| **escalated** | session and reviewer disagree | the advisor's ruling, recorded with both positions |
| **exploratory evidence** | the run carried synthetic-completion or bypass overrides | the *profile* criterion is unmet. Re-label, and re-open only the claims that depended on it |

**Advisor finality is decision authority, not proof.** Its ruling settles *who
decides*; it does not make a failed measurement pass, and no verdict changes what a
measurement says. Record the ruling with the criterion it addresses.

**Two limits worth stating plainly, because the workflow above could imply
otherwise:**

- **Independence is a matter of degree.** A third model family reduces correlated
  error; it does not eliminate it, and it is not a guarantee. The Codex reviewer is
  the session's *own* family, so its agreement is weaker evidence than DSH's — treat
  it accordingly rather than calling both "independently verified".
- **A review covers the revision it saw.** If the tree changes afterwards, the
  affected criteria are unreviewed again. Say which revision was reviewed.

---

## 3. When to escalate to the advisor

Escalate on any of these. Do not wait to be asked, and do not wait until you have
exhausted the obvious ideas — the trigger is the *shape* of the problem, not your
frustration level.

- **You are going in circles.** The same failure has survived two attempts, or you
  are re-deriving something you already tried. This is the "looping" trigger and it
  is the most common one. Two failed attempts at the same root cause is the limit;
  a third attempt is a guess.
- **You are hitting a wall.** Progress has stopped: no new measurement is changing
  your mind, or every next step is another guess.
- **Two measurements contradict each other** and neither is obviously the artifact.
  Highest-value case — it almost always means one *measurement method* is broken,
  and an outside view finds it faster than more measurements do.
- **You are about to say "impossible", "unproven", "cannot", or "rules out"** — any
  universal claim, including a negative result. A check that finds nothing is a
  claim about the *check* as much as about the system.
- **Before an expensive investigation** — a long build, a large recovery pass, a
  full regeneration — where a wrong premise wastes hours.
- **A review disagreement** (step 6 above).
- **An acceptance criterion is unmet, ambiguous, or contradicted by evidence.**
  That gate is a technical review request to the advisor, not a request for user
  permission.

The advisor returns **ranked mechanisms plus the cheapest discriminating experiment
for each**. Adopt or reject each with a recorded reason; do not adopt a
recommendation you cannot test.

---

## 4. The advisor contract

**Brief it fresh, then keep it.** The first consult is a self-contained brief; every
consult after that goes to the same child with only the delta, because the earlier
exchange is preserved.

- **Never use `subagent_fork` for the advisor.** Seeding it with this conversation
  destroys the independence that makes it worth consulting.
- **Spawn it with `run_in_background: true`, always.** This is not a style choice
  and getting it wrong is silent: `false` produces a **one-shot** child that
  answers its first question perfectly and then rejects every continuation with
  *"has no supported continuation state and cannot be resumed"*, forcing a full
  re-brief. Measured, with source: `resolveDelegationRun` returns
  `{ runInBackground: request.run_in_background ?? options.continuable }`
  (`dsh-tool-subagent/lib/index.js:360`) and only the `true` branch consults
  `continuable` (`:521-526`). "The answer gates my next action" is **not** a reason
  to pass `false` — it means do not start other work until the notice arrives.
- **Keep the child id.** It is printed as `started subagent <id>`. Continue it with
  `send_message`. Confirm persistence with `list_agents`: a continuable child
  appears there, a one-shot child does not.
- **A foreground one-shot returns its answer inline; a continuable spawn returns
  `started subagent <id>`.** That is the tell, and it is the only one.

The briefing template lives in `.dsh/skills/advisor-escalation/SKILL.md`, which is
a **procedure** (how to brief) and not a policy (who to spawn, when). The policy is
this file.

---

## 5. Why this is one file and not three

Three documents previously described the delegation policy — `AGENTS.md`,
`grok-role-map.md` and `deepseek-harness.md` — and a fourth,
`.dsh/skills/advisor-escalation/SKILL.md`, described the advisor. On 2026-09-22 the
policy changed twice within an hour (routes added 17:12, the acceptance-review gate
added 18:16) and **five separate claims went stale**, including two that actively
contradicted the new gate: the skill said the advisor was "the only subagent this
session is allowed to spawn", which forbade the reviewer the gate requires, and the
plan's own delegation table listed two routes while its acceptance section demanded
a third.

The lesson is not "write more carefully". It is that **a fact copied into several
files is not corroboration** — three of those five claims were restatements of the
first, and none were found by reading the file being edited. They were found by
sweeping every document for the claim after the policy changed.

So: **one authority per fact.** This file owns the roster and the loop. `AGENTS.md`
owns operating knowledge and carries only a pointer plus the one-screen summary.
The skill owns the briefing procedure. `plan-jsrf-bare-minimum.md` owns acceptance
criteria and statuses. `report-deepseek.md`'s `CURRENT STATE` block owns the current
blocker and next packet. When a policy changes, **grep every document for the old
claim** before considering the change done.
