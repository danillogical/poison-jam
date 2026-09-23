> **RETIRED 2026-09-22. Archived for history only. Do not follow this file.**
>
> The model roster and delegation policy now live in **\docs/agent-workflow.md\**,
> which supports exactly two harnesses (Codex and DeepSeek/DSH). This document
> describes a superseded harness and names models that are retired
> (Grok, \gpt-5.6-sol\, \gpt-5.6-luna\, \gpt-5.6-terra\, \gpt-5.5\,
> \workbuddy-ai/gpt-*\). It is kept because its harness mechanics may still
> explain an old log or commit message. Nothing here is a current instruction.

# DeepSeek harness map for JSRF

This is the DeepSeek/DSH overlay for this project, the counterpart to
`grok-role-map.md`. `AGENTS.md` owns operating knowledge and the plan owns
acceptance criteria; **this file owns how a DeepSeek session runs the work** —
its model routes, its delegation rules, and the harness mechanics that are not
guessable from the tool descriptions.

Read this with `AGENTS.md`, the current plan row, and the CURRENT STATE block in
`report-deepseek.md` before selecting work.

---

## 1. The session shape

**Decision (2026-09-22): one DeepSeek parent, free DeepSeek workers for reading, an
hy4-preview-f reviewer for acceptance, and the Astra advisor for judgement.** The
parent is `workbuddy-ai/deepseek-v4.1-flash` and owns orchestration, adjudication
and acceptance. It is the only writer of the report, the plan and the commit
history.

There are exactly three delegation routes, and they exist for **different
reasons**. Choosing the wrong one is the most common mistake in this harness:

| Need | Route | Effort | Why this one |
|---|---|---|---|
| **Context isolation** — read a large file, log, dump or artifact and return a bounded summary so the raw content never enters the parent's window | `workbuddy-ai` / `deepseek-v4.1-flash` | `high` | Same model, so no diversity is lost; **free (x0.00)**; unlimited. |
| **Acceptance review** — a packet's criteria are met and must be independently verified before acceptance | `workbuddy-ai` / `hy4-preview-f` | `high` | A *third* model family, so it shares neither the parent's nor the advisor's blind spots. Required, not optional — see the gate below. |
| **Independent judgement** — two measurements contradict, two hypotheses failed, a universal claim is about to be made, an acceptance gate fires, or the session and the hy4 reviewer disagree | `codex` / `gpt-6-astra` | `medium` | A *different* model family is the whole point: a same-model agent shares the blind spot you are trying to escape. On a review disagreement its call is **final**. |

### The acceptance review gate

**Standing instruction (2026-09-22): after a packet's acceptance criteria are met,
do not accept it alone.** Three steps, in order:

1. **Spawn a `workbuddy-ai` / `hy4-preview-f` reviewer** on the completed packet.
   It is a *third* model family, so it does not share either the parent's or the
   advisor's blind spots. Brief it with the claim, the criteria, and the evidence
   paths, and ask it to **verify or refute each criterion independently** — not to
   summarise. Tell it explicitly to reproduce the load-bearing measurements
   itself, and to give a per-criterion verdict of AGREED / DISAGREED / CANNOT
   VERIFY with the command or `file:line` that produced it.
2. **If the reviewer agrees**, the packet is accepted. Record the review and what
   it independently reproduced in `report-deepseek.md`.
3. **If you and the reviewer disagree**, send the disagreement — both positions
   and the evidence each rests on — to the **Astra advisor**, whose call is final.
   Do not simply out-vote the reviewer, and do not let it out-vote you: an
   unresolved disagreement is a signal that a measurement is broken, which is
   exactly the case the advisor exists for.

Two things make this gate worth its cost, and both are about the briefing:

- **Ask for the falsification, not the confirmation.** The strongest reviewer
  behaviour is destroying a load-bearing measurement. A review that agrees with
  everything without reproducing the numbers is worthless — say so in the prompt.
- **Give it a positive control.** If the review checks that something is *absent*,
  it must also check that its detector finds something *present*. This project has
  twice published a "found nothing" result from a broken scanner.

**Reviewer prompt skeleton** (fill the bracketed parts; keep the rest verbatim,
because each clause is there for a reason measured in this project):

```
READ-ONLY independent review. Do not modify, create or delete any file. Do not
build or run the game. Verify or refute an acceptance claim.

Project: <one paragraph — what is being built, and the conventions that make it
unusual: guest code is 32-bit Xbox VAs via runtime macros; generated functions are
void(void) and use a SIMULATED guest register file, not the host CPU registers.>
Repos: game <path>, toolkit <path>. Python: C:\Python313\python.exe -X utf8 with
PYTHONPATH=C:\Users\logic\AppData\Roaming\Python\Python313\site-packages.
The project's own disassembler is authoritative:
  C:\Python313\python.exe -X utf8 scripts/inspect-jsrf.py disasm <start> <end>

The claim: <what was fixed and how, in two or three sentences>
Claimed criteria, all asserted MET: <numbered list>
Claimed measured result: <before/after table with run paths>
Load-bearing measurements: <the specific numbers the claim rests on>

Your task: independently verify or refute each criterion. Do not take this summary
as evidence — read the artifacts and the machine code yourself.

Attack these, in priority order:
1. <the strongest falsifiable claim, and how to reproduce it>
2. <any "found nothing" claim — reproduce it AND run a positive control: the same
   scanner must find something known present. Report the control result explicitly.
   A scanner that finds nothing for everything is this project's known failure mode.>
3. <any arithmetic or decode claim, with the tool that settles it>
4. <boundary questions: is the chosen end correct, does it swallow or cut anything?>
5. <regression: read result.json and confirm the run is what it claims; if you
   cannot verify CTest from files, say so rather than accepting it.>

Deliverable, under 600 words: a verdict per criterion (AGREED / DISAGREED / CANNOT
VERIFY) each with the evidence you personally checked and the command or file:line
behind it; any counter-evidence or alternative reading, even if it does not change
the verdict; explicitly what you could not verify; and mark each of your own
conclusions measured or inferred.

If you disagree with any part, say so plainly and give the reason. A review that
agrees with everything without independently reproducing the load-bearing
measurements is not useful — the strongest thing you can do is falsify the
load-bearing claim. Do not propose code changes.
```

**A review prompt for A2e is already written** — see the A2e sections in
`report-deepseek.md`; it is the concrete instance of this skeleton and was
rejected only because the route was frozen, not because anything was wrong with
it. Reuse its structure.

**The reviewer route is frozen per session, and a restart does not unfreeze it.**
Read from `dsh-tool-subagent/lib/index.js` rather than inferred, because the
distinction decides whether the human has to restart DSH:

- The settings document itself **hot-reloads** (`dsh-settings-file`, `watch: true`),
  so an edit to `~/.dsh/settings.yaml` is live in the running host immediately.
- But the subagent tool reads it **once per session**. The projection is
  `init: () => null` with `apply: (policy, event) => { if (policy !== null …)
  return policy; … }`, and the writer's own comment is *"Append the route policy
  once, before its definition can reach a model request."*

| action | effect |
|---|---|
| edit `settings.yaml` | live for the host, but **not** for a session already composed |
| restart DSH, **resume** the session | **still the old routes** — the durable policy short-circuits |
| start a **new** session | samples the live settings; new routes available |

So adding a route needs **a new session, not a restart** — and the session that
added it can never use it. Confirm with `list_subagent_models` before promising a
review; if the route is missing, say so plainly rather than substituting a model
the workflow did not ask for.

**Watch `agent-default-model` when editing settings.** It is a separate key in the
same file, and it decides which model *implements* work rather than which reviews
it. Setting it to the reviewer's model would make the reviewer review its own
output, defeating the point of a third-family gate. Verified 2026-09-22: an edit
to `allowedModels` was accompanied by an unrelated change to `agent-default-model`
from `deepseek-v4.1-flash` to `hy4-preview-f`, which was reverted. Check the whole
file, not just the key you meant to change.

Two rules follow, and both are easy to violate by reflex:

- **Do not reach for a Codex-lineage name (Sol / Luna / Terra / Astra) to do
  reading work.** Those names are packet vocabulary here. They also spend paid
  Codex subscription quota duplicating something the free route does as well.
- **Do not use a DeepSeek worker where the question is judgement.** It will
  reproduce the parent's error rather than catch it. That is what the advisor is
  for.

The role names in `AGENTS.md`, the plan and older reports describe **what kind of
work** a packet is — a difficulty label, not a roster to spawn. `grok-role-map.md`
maps those same names onto Grok and does not apply to this session.

**A worker is a reader, not a decider.** It returns evidence with `file:line`
citations, marked *measured* or *inferred*, and the parent adjudicates. **One
owner performs build, regeneration and run — never a worker.** Workers must be
told explicitly that they are read-only when they are.

---

## 2. Model routes, as measured

Two independent providers, each with its own catalog. **The names overlap, which
is a trap.**

| Provider | Catalog file | Auth | Models |
|---|---|---|---|
| `workbuddy-ai` | `~/.dsh/.workbuddy-ai-catalog.json` | WorkBuddy app | 22, including `deepseek-v4.1-flash` and `hy4-preview-f` |
| `codex` | `~/.dsh/plugins/subscriptions/models.json` | ChatGPT subscription | 7 |

`deepseek-v4.1-flash`: context 300,000, `maxTokens` 128,000, cost **x0.00 (free)**.
`hy4-preview-f`: context 300,000, `maxTokens` 64,000.

**A catalog listing is not a routing guarantee.** Measured in an earlier session:
`gpt-6-astra` serves on `codex` but **not** on `workbuddy-ai`, even though the
WorkBuddy catalog advertises that name. A wrong-provider call fails in a way that
looks like an entitlement problem if you only read the catalog. `codex` rejects
unknown model ids rather than ignoring the field, which is why the positive
results are meaningful. The same test applies to `hy4-preview-f`: it is in the
WorkBuddy catalog, but a session must confirm the route resolves before promising
a review.

`settings.yaml` holds the session default and the subagent allow-list:

```yaml
agent-default-model:
  provider: workbuddy-ai
  model: deepseek-v4.1-flash     # the implementer, NOT the reviewer
  reasoningEffort: max
subagent-model-selection:
  enabled: true
  allowedModels: [ ...codex/*, workbuddy-ai/deepseek-v4.1-flash,
                   workbuddy-ai/gpt-5.5, workbuddy-ai/hy4-preview-f ]
```

**Check the whole file, not just the key you meant to change.** `agent-default-model`
is a separate key in the same document and decides which model *implements* work.
Setting it to the reviewer's model would make the reviewer review its own output,
defeating the third-family gate. On 2026-09-22 an edit to `allowedModels` was
accompanied by an unrelated change to `agent-default-model` (to `hy4-preview-f`),
which was reverted. Read the file after editing it.

---

## 3. Harness mechanics that are not guessable

Each of these was measured in this environment, not inferred. Several were
assumptions that turned out to be wrong.

### Spawning a worker

The `standard` preset configures the tool as
`provider: spawn`, `backgroundMode: continuable`, `modelSelectionSettings: true`.
So:

```
subagent(
  description: "...", provider: "workbuddy-ai", model: "deepseek-v4.1-flash",
  reasoning_effort: "high", run_in_background: true, prompt: "<self-contained>"
)
```

- **`provider` and `model` must be supplied together** or the route does not
  resolve.
- **Under `continuable` policy there is no background *job*.** A spawn returns
  `started subagent <childId>`; there is nothing to collect with `job_output`
  (that errors with `unknown job`). The answer arrives as a settlement notice.
- **Omit `run_in_background`, or pass `true`, to get a durable child. `false`
  gives you a ONE-SHOT child, and this corrects what this file used to say.**
  Measured 2026-09-22 with a controlled pair — two advisor spawns differing in
  nothing else:

  | spawn | `run_in_background` | recorded `mode` | `list_agents` | continuation |
  |---|---|---|---|---|
  | `32266b9a` | `false` | **`one-shot`** | absent | rejected |
  | `8808aa38` | `true` | `continuable` | present | **delivered** |

  The source agrees: `resolveDelegationRun` returns
  `{ runInBackground: request.run_in_background ?? options.continuable }`
  (`dsh-tool-subagent/lib/index.js:360`), and only the `runInBackground === true`
  branch consults `continuable` at all (`:521-526`). So `false` short-circuits to
  `settleForegroundRun` (`:557`) and the durable path is never taken. The old
  wording here — *"`false` waits in the foreground and still yields a continuable
  child"* — was wrong, and the skill's `advisor-escalation/SKILL.md:95` repeats it.
  **The cost is silent:** a one-shot child answers its first question perfectly and
  only fails at the first `send_message`, with *"has no supported continuation
  state and cannot be resumed"*.
- **Therefore: spawn the advisor with `run_in_background: true` when you intend to
  continue it, even though the answer gates your next action.** Waiting for a
  continuable child is a matter of not doing other work until its notice arrives;
  it does not require `false`. Confirm with `list_agents` before relying on it —
  a continuable child appears there, a one-shot child does not.
- **Use `subagent`, never `subagent_fork`, for the advisor.** `subagent_fork`
  seeds the child with this conversation, which destroys the independence that
  makes the advisor worth consulting.

### Isolation, and why it is the *parent's* job

`grok-role-map.md` says to use `isolation: worktree` for implementation that
edits manifests, recovered bodies, tests or toolkit sources. **That option does
not exist in this harness.** The `subagent` tool exposes exactly these fields:
`provider`, `model`, `reasoning_effort`, `description`, `prompt`,
`run_in_background`. There is no `isolation` parameter and no worktree support.
A worker inherits the session's working directory and edits the same tree.

So the policy has to be enforced by the parent, in the briefing:

- **Default a worker to read-only.** State it explicitly: "Do not modify, create
  or delete any file. Do not build or run the game." A reader that edits is a
  liability, and the honest default for a context-isolation worker is that it
  never writes at all.
- **Never let a worker build, regenerate or run.** One owner performs those — the
  parent. Two writers in one tree is how a build becomes unattributable.
- **Serialise the parent's own writes.** Spawn workers, wait for the evidence,
  then edit. Do not edit the report while a worker is reading it.

Git worktrees are available (`git worktree add`, git 2.39.2) and *do* isolate
commits — verified: a commit made in a worktree left the parent's `HEAD`, status
and working files untouched. But **a worktree of this repository is not usable
as-is for a worker**, because everything a worker needs to read is gitignored:

| Path | Size | In a fresh worktree |
|---|---|---|
| `game/` (the XBE and assets) | 2,382 MB | **absent** |
| `logs/` (all run evidence) | 96,050 MB | **absent** |
| `tools/disasm/output/` | 62.6 MB | **absent** |
| `build/` | 138.5 MB | **absent** |
| `AGENTS.md`, `src/` | — | present |

The ignored paths can be bridged with directory junctions (`mklink /J`), which
was verified to work, but that points the worktree at the *same* files, so it
buys isolation of commits and nothing else. For a read-only worker — the default
— it buys nothing at all, and it costs 96 GB of apparent duplication and a
cleanup hazard.

**Conclusion: do not use worktrees for workers in this project.** The isolation
that actually prevents conflicts here is (a) read-only briefings, (b) a single
writing owner, and (c) the parent committing only its own files by explicit path
— never `git add -A`, which would sweep a worker's or another session's
half-finished edits into an unrelated commit. Worktrees remain the right tool if
a *future* need is concurrent write-isolated branches; they are the wrong tool
for readers.

### Continuable versus one-shot — silent until it matters
A one-shot child answers its first question perfectly and only fails at the first
attempt to continue it. Nothing in the result announces the difference except the
shape of what comes back:

| Result | Meaning |
|---|---|
| the child's **answer inline** in the tool result | **foreground / one-shot — not reusable** |
| `started subagent <id>` | continuable — reusable |
| `started background subagent job <id>` | background job — not reusable |

Measured 2026-09-22: the `run_in_background: false` advisor returned its whole
answer inline and was one-shot; the `run_in_background: true` one returned
`started subagent 8808aa38` and was continuable. Under this preset `continuable` is
true, so the third row cannot occur here — the choice is entirely yours, made by
that one flag (see "Spawning a worker" above).

**Confirm with `list_agents`** whenever the child is meant to be reused: a
continuable child appears there with a status; a one-shot child does not appear
at all. This session spawned an advisor one-shot and did not notice until the
continuation failed.

### Reach: an agent may message only its own direct children

`send_message` requires **exact adjacency** — a direct continuable child, or your
direct parent if you are a resident child. `list_agents` shows your own children
only.

| Target | Result |
|---|---|
| your direct **continuable** child | delivered |
| your own **one-shot** child | `has no supported continuation state and cannot be resumed; choose a different target` |
| **another session's** child | `belongs to another parent session` |
| a **top-level session** | `belongs to another parent session` |

Read the error rather than retrying: `has no supported continuation state` means
*you spawned it wrong*; `belongs to another parent session` means *you are the
wrong session* and no amount of retrying helps.

**A user-created session cannot be used as the advisor.** A session the human
opens in the Web UI is a *sibling* (`delegationDepth: 0`), not a child, so it is
rejected exactly like another session's child. An advisor is **session-scoped**:
it is addressable only while its exact parent is live, and a later session starts
with a fresh brief. Plan for that — a fresh brief is cheap, but do not treat an
advisor as a durable project asset.

### Watching workers and the advisor in DSH Web

The ordinary sidebar **omits subagent conversations by design**. The entry point
is the **session header's `/` count trigger**, which appears when a session has
subagent descendants and opens the descendant catalog. Selecting a row opens that
child's conversation.

Because the preset's children are continuable, a child with a live parent keeps
the ordinary composer: the human can read the exchange **and type into it
directly**, while it runs. A one-shot row opens read-only.

Distinguishing rows when both are the same model: the **`mode` column**
(`one-shot` vs `continuable`) is the reliable tell; the label, title, turn count
and token totals also differ.

### The per-response output cap

`deepseek-v4.1-flash` has `maxTokens: 128000`. This is a **hard cap on one
assistant response**, and exceeding it truncates the reply mid-sentence with
`Output token limit reached`. It is *not* a context problem — context pressure is
reported separately and is far larger.

It is reached by narrating between many tool calls, especially while reading long
files. The fixes, in order of effect:

1. **Delegate the reading** (the free worker route) so raw file contents never
   enter the parent's window.
2. **Batch reads and stay terse** between tool calls.
3. **Read ranges, not whole files**, and prefer `grep` with tight patterns when
   only one fact is needed.

Nothing is lost when it fires: the report's CURRENT STATE block is the durable
memory, and `continue` resumes. That is the reason to keep the report current
*during* work rather than at the end.

### Verifying a route

**Do not ask a model what it is.** Asked directly, the advisor declined to name
itself — correctly: it has no introspective access to which model served the
request, and a name visible in its own context is a label, not verification. A
confident self-identification is *weaker* evidence than that refusal, because the
name is already in the briefing.

Read the session log instead. `subagent/descriptor` carries `agentProvider`,
`agentModel` and `agentReasoningEffort` at spawn; each `request/header` repeats
the resolved `config` per turn. Those record what DSH **requested**. Confirming
what the provider **served** needs provider-side metadata, which is not available
locally — so state it as *dispatched*, never as *confirmed served by*.

---

## 4. Skills

Local skills are discovered at `<projectRoot>/.dsh/skills/<name>/SKILL.md` (rank
100) and `$DSH_HOME/skills/`. The project tracks `.dsh/skills/` in Git, so a
skill travels with the repository.

| Skill | Owns |
|---|---|
| `.dsh/skills/advisor-escalation/SKILL.md` | The advisor: route, triggers, briefing contract, adopt/reject discipline, and the measured reach rules above. **Authoritative for this harness.** |
| `.workbuddy-ai/skills/recompiler-hang-triage/SKILL.md` | Hang triage. |
| `~/.workbuddy-ai/skills/advisor-escalation/SKILL.md` | The older WorkBuddy-harness copy. Its `Agent(resume=...)` invocation does **not** apply here. Kept deliberately: rewriting it would make it wrong for the harness that still uses it. |

---

## 5. Briefing a worker

A worker has **none** of this conversation. A prompt that assumes shared context
returns generic advice. Include:

1. **Read-only or not, stated explicitly.** "Do not modify, create or delete any
   file. Do not build or run the game." A reader that edits is a liability, and
   since the harness cannot isolate a worker's writes (§3), the briefing is the
   only thing standing between a worker and the tree.
2. **The system**, including the non-obvious conventions that invalidate normal
   intuition — for JSRF: guest code is 32-bit Xbox VAs via runtime macros; a
   guest VA is a *host* address plus a constant offset; generated functions are
   `void(void)` communicating through guest registers and a simulated stack; the
   host CPU register file and the guest's simulated registers are **different
   register files** and confusing them produces confident nonsense.
3. **Established facts, marked** *measured* or *inferred*, with the method.
4. **The exact question**, ideally one question.
5. **Where to look**, by absolute path.
6. **The deliverable and its size** ("under 500 words"), plus the requirement to
   cite `file:line` and to mark conclusions *measured* or *inferred*.
7. **An explicit request for what could not be determined.** A named gap is more
   useful than a guess, and it is the field a reader is most tempted to fill
   with invention.

Two standing instructions that have paid for themselves:

- **"If two facts conflict, say so rather than choosing."** A worker that
  silently resolves a contradiction destroys the most valuable signal it has.
- **Ask for a cheap falsifier.** "What is the cheapest way to tell these apart
  from the original machine code alone?" produces a test, not a restatement.

---

## 6. Failure modes seen in practice

Recorded because each cost real time here, and each will recur.

- **Duplicating your own worker.** The parent investigated the same question it
  had just delegated, re-deriving a mechanism a worker was already reporting on.
  **Wait for the worker, or do genuinely different work.** Delegation that runs
  in parallel with the same investigation saves nothing.
- **Not checking `git log` before starting.** HEAD had already moved past the
  packet being worked on. One command would have shown the work was delivered.
  **Check both repositories' revisions before reading a single file.**
- **Misreading your own subagents as a rival session.** Subagent children get
  their own session directories, so a worker looks like a second session on disk.
  Check the session header's `delegationDepth` and the parent id before
  concluding there is a concurrency problem.
- **Trusting a catalog over a call.** See §2.
- **Treating a self-report as evidence.** See §3.
- **`git add -A` in a shared tree.** It commits whatever else is modified —
  a worker's draft, another session's half-written paragraph. Commit by explicit
  path. This is the concrete conflict risk in this project, and no worktree
  setting prevents it because the tool has none.
- **A worker's confident conclusion is still a claim.** The worker that reported
  "the fix is already in the tree" was right — but it was checked against
  `git log` before being believed, which is the standard to hold every one to.
- **Discovering a new route mid-session and expecting to use it.** The allow-list
  is frozen at session composition, so the session that adds a route can never use
  it, and a restart does not help either — only a new session does. Confirm with
  `list_subagent_models` *before* promising a review, and if the route is missing,
  say so rather than substituting a model the workflow did not ask for.
- **Editing one settings key and getting another changed.** The settings watcher
  rewrites the whole document, so mtime moves even when content does not — and a
  concurrent editor (including a settings UI) can change a key you never touched.
  Read the file back after editing it. See §2 on `agent-default-model`.
- **Treating "the reviewer agreed" as the goal.** The gate exists to falsify, not
  to ratify. If a review agrees without reproducing the load-bearing numbers, the
  review failed even though its verdict was AGREED.

---

## 7. Where things live

| Location | Role |
|---|---|
| `AGENTS.md` | Operating knowledge, project-wide. |
| `plan-jsrf-bare-minimum.md` | Milestones, acceptance criteria, the advisor gate. |
| `report-deepseek.md` | Live decisions; the CURRENT STATE block is the durable memory. |
| `grok-role-map.md` | The Grok overlay. Not authoritative for this session. |
| `deepseek-harness.md` | **This file.** The DeepSeek/DSH overlay. |
| `.dsh/skills/` | Tracked local skills. |
| `~/.dsh/settings.yaml` | Session default model and subagent allow-list. |
| `~/.dsh/sessions/` | Transcripts, `session.v3.jsonl.zstd` (zstd; multi-frame). |
| `~/.dsh/storages/session_projcache/sessions/` | Per-session projections: title, mode, token use. |

A session transcript is a multi-frame zstd stream: `zstdDecompressSync` returns
only the **first frame**. Decompress frame by frame, or use the projection cache,
which holds the same facts as plain JSON.
