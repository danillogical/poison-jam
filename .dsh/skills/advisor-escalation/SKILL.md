---
name: advisor-escalation
description: PROCEDURE for briefing the independent Astra advisor (codex/gpt-6-astra at medium effort) — how to write the brief, what to ask for, and how to adopt or reject the answer. This skill is a procedure, NOT the policy. Who to spawn, which models, the session loop and the escalation triggers all live in docs/agent-workflow.md. Load this when you have decided to escalate and need the briefing template.
whenToUse: You have decided to consult the advisor (see docs/agent-workflow.md section 3 for the triggers — looping, a wall, contradicting measurements, an impending universal claim, an expensive unverified premise, a review disagreement, or an unmet acceptance criterion) and need the briefing procedure. Do not use this skill to decide WHETHER to escalate; that is the policy's job.
---

# Briefing the advisor (procedure)

**This skill is the *how*. The *who* and *when* are policy and live in
`docs/agent-workflow.md`** — read §1 for the roster, §2 for the session loop and
§3 for the escalation triggers. If those disagree with this file, **they win**;
this file only describes how to write a good brief once you have decided to send
one.

Why the split: this file used to carry policy too, and on 2026-09-22 that went
wrong in a way worth remembering. It asserted *"The advisor is the only subagent
this session is allowed to spawn. No worker, reviewer, or architect subagents."*
That was true when written at 17:12 and was superseded at 18:16 by the standing
acceptance-review gate, which **requires** spawning a reviewer — so for five hours
this skill told fresh sessions they could not spawn a subagent the policy demands.
A procedure file cannot go stale against a policy it does not contain.

## The route

- **`codex:gpt-6-astra` at `medium` effort.** In DSH the advisor is reached through
  the **`codex`** provider — `workbuddy-ai` does not serve `gpt-6-astra`. This is
  the one deliberate cross-provider call, and it is what makes the advisor
  independent of the session.
- **Spawn with `run_in_background: true`, always.** `false` yields a **one-shot**
  child: it answers the first question perfectly and then rejects every
  continuation with *"has no supported continuation state and cannot be resumed"*,
  forcing a full re-brief. "The answer gates my next action" is **not** a reason to
  pass `false` — it means do not start other work until the notice arrives.
  Measured, with source: `resolveDelegationRun` returns
  `{ runInBackground: request.run_in_background ?? options.continuable }`
  (`dsh-tool-subagent/lib/index.js:360`) and only the `true` branch consults
  `continuable` (`:521-526`).
- **Never `subagent_fork`.** Seeding it with this conversation destroys the
  independence that is the entire point of consulting it.

## First consult versus follow-ups

**First consult — a full self-contained brief.** It has no context:

```
You are the independent advisor on a static-recompilation project. You have NOT
seen this conversation; everything you need is below. Answer in ranked
mechanisms, cheapest discriminating experiment first.

Repository: <path>   Toolkit (read-only for you): <path>
Python: <path>  (Windows; PowerShell. PYTHONPATH=<site-packages> for capstone.)

FACTS (measured, with the command or file:line that produced each):
  - <fact>  [evidence]
  - <fact>  [evidence]

HYPOTHESES I have already tried and how each failed:
  - <hypothesis> -> <what it predicted> vs <what was measured>

WHAT CONTRADICTS WHAT:
  - <measurement A> says <X>; <measurement B> says <Y>.

THE QUESTION: <one sentence>

WHAT I WANT: rank the mechanisms that could produce this, and for each give the
cheapest experiment that would DISCRIMINATE it from the others. Say explicitly
which of my facts you are treating as unreliable and why.
```

**Follow-ups — the delta only.** Send to the same child id with `send_message`.
It retains the earlier exchange, so re-briefing it is waste. State only what is
new: the measurement you ran, its result, and the question that result raises.

## What to ask for, and what to do with it

Ask for **ranked mechanisms plus the cheapest discriminating experiment for
each** — not for a conclusion. The value is in the *ranking and the test*, because
the test is what you can verify locally.

Then **adopt or reject each recommendation with a recorded reason.** Do not adopt
a recommendation you cannot test, and do not treat the advisor's confidence as
evidence. It is reasoning about your facts, not measuring them; if a fact you gave
it was wrong, its conclusion will be confidently wrong too.

Record the consult and its outcome in `report-deepseek.md`: the question, the
ranking, which mechanism you tested, and what the test measured. When the advisor
predicts a specific value or address, **record the prediction before testing it** —
a confirmed specific prediction is much stronger evidence than a plausible
explanation, and an unrecorded one cannot be distinguished from hindsight.

## Escalating a review disagreement

Per `docs/agent-workflow.md` §2 step 6: if the session and the acceptance reviewer
disagree, **both positions and their evidence go to the advisor, whose call is
final.** Do not out-vote the reviewer; do not let it out-vote the session. Brief
it with both claims, both sets of measurements, and the specific point of
disagreement — an unresolved disagreement usually means a *measurement* is broken,
which is the case this route exists for.
