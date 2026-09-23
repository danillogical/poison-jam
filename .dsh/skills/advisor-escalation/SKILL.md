---
name: advisor-escalation
description: Escalate a stuck or high-stakes JSRF problem to the independent Astra advisor (codex/gpt-6-astra at medium effort) — the only subagent this session is allowed to spawn. Use when two measurements contradict each other, when two or more hypotheses have failed, before an expensive investigation, when about to claim something is impossible or unproven, or when a negative result would change the plan. Ask for ranked mechanisms plus the cheapest discriminating experiment for each.
whenToUse: A JSRF packet is stuck, two measurements contradict, a universal claim is about to be made, or a packet's acceptance criterion is unmet or ambiguous and the advisor gate in plan-jsrf-bare-minimum.md applies.
---

# Advisor escalation (DeepSeek session)

Consult the independent second-model advisor when the current line of reasoning
has stopped producing progress, or when the cost of being wrong is high.

This is the **DSH-native port** of `~/.workbuddy-ai/skills/advisor-escalation/SKILL.md`.
That copy documents the WorkBuddy/Codex harness and its `Agent(resume=...)`
mechanism; this copy documents the DSH harness. Keep both — they are not
interchangeable, and the invocation differs.

## Who the advisor is, and the one rule about subagents

**The advisor is `codex` / `gpt-6-astra` at `medium` reasoning effort.**

Standing user instruction for this project (2026-09-22): **this session runs
everything itself on `workbuddy-ai/deepseek-v4.1-flash`. The advisor is the only
subagent it spawns. No worker, reviewer, or architect subagents.**

That retires the Sol / Luna / Terra packet roles *as delegation targets* in this
session. The role vocabulary still appears in `AGENTS.md`, the plan and older
reports; read it as describing **what kind of work** a packet is, not as a roster
to spawn. `grok-role-map.md` maps those names onto Grok and does not apply here
either.

Independence is the point. Astra is a different model family from DeepSeek, and
it is briefed fresh rather than handed this conversation — a same-model agent
given the same context tends to reproduce the same blind spot. That is exactly
what you are trying to escape when you are stuck.

## When to escalate

Escalate on any of these — do not wait to be asked:

- **Two or more hypotheses have failed** and the third is a guess.
- **Two measurements contradict each other** and neither is obviously the
  artifact. This is the highest-value case: it almost always means one
  *measurement method* is broken, and an outside view finds it faster than more
  measurements do.
- **You are about to say "impossible", "unproven", or "rules out"** — any
  universal claim. These are the claims that are wrong most often.
- **Before an expensive investigation** (a long build, a large recovery pass, a
  full regeneration) where a wrong premise wastes hours.
- **A negative result** that would change the plan ("the check found nothing",
  "no violations occur"). A check that finds nothing is a claim about the
  *check* as much as about the system.
- **The acceptance gate in `plan-jsrf-bare-minimum.md` fires**: a packet's
  criterion is unmet, ambiguous, or contradicted by evidence. That gate is
  mandatory and is a technical review request to the advisor, not a request for
  user permission.

`plan-jsrf-bare-minimum.md` records four triggers that were each met in a
twelve-hour window with no consult made, and every one of them turned out to be
a measurement artifact found locally only after costing a run or a false alarm:
a rebuilt tree that "looked like a regression" and differed only by an
environment variable; a kernel-log budget that made a live run look frozen;
`named_frames`/`native_threads` sampled at the deadline and read as before/after
metrics; and a symbol+offset in a stack line that was a mislabel past a body's
end. The gate applies **during** a packet, not only at its acceptance.

Do **not** escalate for routine work, or to avoid doing the measurement
yourself. The advisor proposes experiments; it does not run them, and it has no
tools pointed at this machine.

## How to invoke

Use the `subagent` tool:

```
subagent(
  description: "Advisor consult: <packet>",
  provider: "codex",
  model: "gpt-6-astra",
  reasoning_effort: "medium",
  run_in_background: <see below>,
  prompt: <the briefing>
)
```

- **Use `subagent`, never `subagent_fork`.** `subagent_fork` seeds the child with
  this conversation, which destroys the independence that makes the advisor
  worth consulting. The advisor gets a self-contained briefing or nothing.
- `provider` and `model` must be supplied **together**; both are required for the
  route to resolve. `reasoning_effort: "medium"` is the configured level for this
  role — `gpt-6-astra` also advertises low/high/xhigh/max/ultra, and medium is
  the deliberate choice, so do not silently raise it.
- **Omit `run_in_background`, or pass `true`, to get a durable child.** Under this
  session's preset the tool is configured `backgroundMode: continuable`, so the
  call returns `started subagent <childId>` immediately and the child stays
  addressable. **`run_in_background: false` waits in the foreground but the call
  still yields a continuable child**, so use it when the answer gates your next
  action.
- **`run_in_background: true` does NOT create a background *job*.** Under
  `continuable` policy there is no `job_output` to collect: the runtime sends one
  settlement notice when the child's turn ends, and the answer arrives as that
  notice. Do not go looking for a job id.
- **The child id is the handle. Keep it.** It is printed as
  `started subagent <id>`, and `list_agents` lists it.

### Confirming you have a continuable child, not a one-shot

Check `list_agents`. A continuable child appears there with its status
(`running`, `idle`, or `ready`); a one-shot child does **not** appear at all.
This distinction is worth one call, because it is silent otherwise: a one-shot
child still answers the first question correctly and only fails later, at the
first attempt to continue it. In this session the tool is `continuable`, so a
child that is missing from `list_agents` means the spawn did not take the
durable path.

### Continuing an advisor: `send_message`, not a re-brief

DSH exposes `send_message`. To continue the same advisor, send to its durable
agent id:

```
send_message(agent_id: "<id from list_agents>", message: "<short follow-up>")
```

A working target receives the message at its nearest step; an idle or ready
target starts a new turn with its earlier exchange intact.

Because the earlier exchange survives, a follow-up can be two sentences instead
of a fresh briefing. **Prefer this when continuing the same investigation.**
Re-brief only when the new question is unrelated: carrying stale, unrelated
context into a fresh problem makes the advisor worse, not better.

### Reach: the advisor belongs to the session that spawned it

`send_message` requires **exact adjacency**: you may target a *direct continuable
child*, or your direct parent if you are a resident child. `list_agents` shows
your own children only. So an advisor spawned in session A is addressable for the
rest of session A, but **a later session cannot message it** — that session is not
its parent, and the id will not appear in its `list_agents`.

Practical consequence: **spawn the advisor when you first need it, and keep using
that one child for the rest of the session.** Do not expect to inherit an advisor
from an earlier session, and do not spend a turn hunting for one in `list_agents`
— it will not be there. A fresh session starts with a fresh brief.

**A user-created session cannot be used as the advisor.** It is tempting to have
the human create the advisor session themselves so it shows up in their sidebar,
but that session is a *separate top-level session* (its own `delegationDepth: 0`),
not a child of yours, so sending to it is rejected exactly like another session's
child. Verified 2026-09-22: sending to a top-level session id and to another
session's child both return `belongs to another parent session`. There is no
model-facing tool that adopts or messages a session you do not parent.

**To watch the advisor in the Web UI, use the session header, not a new session.**
When a session has subagent descendants, the header appends a `/` count trigger
next to the title; it opens the descendant catalog, and selecting a row opens that
child's conversation. A continuable child with a live parent keeps the ordinary
composer, so the human can read the exchange **and type into it directly** — while
it runs. The ordinary sidebar deliberately omits subagent conversations, which is
why the header is the entry point. This is the supported way to make advisor
traffic visible; it needs no setup and costs no capability.

**A failed send says which of the two problems you have**, so read the error
rather than guessing:

| Target | Result |
|---|---|
| your direct **continuable** child | delivered |
| your own **one-shot** child | `has no supported continuation state and cannot be resumed; choose a different target` |
| **another session's** child, or a **top-level session** | `belongs to another parent session` |

So `has no supported continuation state` means *you spawned it wrong* (re-spawn
continuable); `belongs to another parent session` means *you are the wrong
session* and no amount of retrying will help — brief a fresh advisor instead.

## How to brief

On a **fresh spawn** the advisor has seen nothing — not this conversation, not
the codebase, not the earlier attempts. A briefing that assumes shared context
returns generic advice. Include:

1. **The system.** What is being built, and the one or two structural facts that
   make it unusual. State non-obvious conventions explicitly. For JSRF the
   essentials are: an Xbox title statically recompiled to Windows; guest code is
   32-bit Xbox virtual addresses reached through runtime macros (`MEM32`,
   `XBOX_PTR`), never host pointers; generated functions are `void(void)` and
   communicate through guest registers and a simulated stack, so the guest ABI
   is enforced purely by save/restore; and a generated `end` boundary that runs
   too long silently swallows the next function.
2. **The bug.** The exact observed wrong value, and the exact expected value.
3. **What is already established**, each item marked *measured* or *inferred*,
   with the method. "Exactly one writer, confirmed by matching the machine-code
   encoding" is worth much more than "only one place writes it".
4. **What has been ruled out, and by what.** Negative results are the most
   valuable input, because they let the advisor skip your dead ends.
5. **The question, as a request for ranked mechanisms.** Ask for hypotheses plus
   the cheapest discriminating experiment per hypothesis. Do not ask "what's
   wrong?" — that invites a restatement of what you already said.

Also send, because the project's acceptance gate requires it: the packet ID, the
failed criterion, both repository revisions, the exact commands and environment,
the artifact paths, expected versus actual behavior, the attempts already made,
and the proposed next action.

**Ask explicitly whether any of your measurements is more likely an artifact
than a fact, and which one.** That single question produced the most valuable
reply in this project's history: it identified that an ABI check validated
register *deltas* but never their absolute values, and that a value which
"looked like garbage" was in fact a plausible **offset** — which reframed the
whole bug.

Cap the answer ("under 500 words") and say plainly that **no code or file changes
are wanted**. Otherwise the reply drifts into implementation the advisor cannot
test.

On a **resume**, skip all of the above and send only the delta.

## How to use the reply

Treat it as a strong hypothesis, not a verdict. It is a different model with no
stakes in your prior reasoning — which makes it good at spotting a broken
measurement and bad at knowing this system's specifics.

- **Run the cheapest discriminating experiment first.** Not the most likely
  hypothesis — the one that splits the space fastest.
- **Expect the reply to attack your reasoning, not your conclusion.** The
  valuable part is usually a blind spot it names ("your check says nothing about
  caller-saved registers"), not its proposed fix.
- **Record which parts you adopted and which you rejected, and why**, in
  `report-deepseek.md`. A rejected hypothesis with a stated reason is a result;
  an unrecorded one gets re-suggested next session. A rejected hypothesis that
  was *later* confirmed is the most valuable entry of all — one advisor's
  top-ranked hypothesis was eliminated by measurement while its lowest-ranked
  one was the answer.
- **If the advisor contradicts a measurement you trust, re-verify the
  *measurement*, not the advisor.** Instrumentation error is more common than a
  wrong model.
- **An advisor-requested change to acceptance criteria must be recorded
  explicitly in the plan with its rationale.** It does not silently rewrite a
  criterion.

## Cost note

A consult costs a real round-trip on a frontier model, so it is not free — but a
wasted run, a wrong recovery pass, or a packet accepted on a broken measurement
costs more. The discipline that keeps this cheap is the **briefing**: a delta on
a resumed advisor is two sentences, and the cheapest-discriminating-experiment
framing is what stops the reply from being generic.

Do **not** schedule the advisor. It is on-demand by explicit choice: a scheduled
run fires with stale context, costs tokens every time, and duplicates the
judgement of the session already working the problem. Invoke it when a trigger
above is actually met.

## Security note

This skill is prose only: no scripts, no bundled files, no network or credential
access. It spawns one subagent through the existing `subagent` tool and adds no
new capability or privilege.

## Appendix: what was verified, and how

Measured 2026-09-22 in this session, because each item was a plausible-sounding
assumption that turned out to need checking.

- **The route resolves.** A live consult to `codex`/`gpt-6-astra`/`medium`
  returned an answer. Enumeration is not authentication: `list_subagent_models`
  listing a model proves nothing about whether the route serves it. Separately
  measured in an earlier session: `gpt-6-astra` serves on `codex` but **not** on
  `workbuddy-ai`, even though the WorkBuddy catalog advertises that name. A
  wrong-provider error looks like an entitlement problem if you only read the
  catalog.
- **The child is continuable.** The tool is configured `backgroundMode:
  continuable` in the `standard` preset, so a spawn returns `started subagent
  <id>` and the child appears in `list_agents`. Confirmed by continuing it with
  `send_message` and getting a reply that restated the original brief.
- **One-shot vs continuable is silent until it matters.** An earlier smoke-test
  spawn in this session came back one-shot and never appeared in `list_agents`;
  it answered its question correctly and would only have failed at the first
  attempt to continue it. Always confirm with `list_agents` if you intend to
  reuse the child.
- **Cross-session is not reachable from the model.** DSH ships a session
  reference service (`dsh-session-reference`) and the host serves a cross-session
  child query to the browser, but the reference service is **user-initiated**: it
  turns a mention the *human* types into a read-only snapshot. There is no
  model-facing tool to message another session or another session's child.
