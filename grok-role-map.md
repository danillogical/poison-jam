# Grok role map for JSRF

This overlay maps the Codex role names in `AGENTS.md` onto the current Grok
session. `AGENTS.md` still owns operating knowledge.
`plan-jsrf-bare-minimum.md` owns acceptance criteria and statuses.
`report-jsrf-bare-minimum.md` owns live decisions. This file owns model,
effort, and spawn mechanics.

Read this file with `AGENTS.md`, the current plan row, and the live report
before selecting work. Keep original assets and existing saves unchanged.

**Decision (2026-09-21): the parent orchestrator is Grok 4.7 xhigh.** That
replaces the short-lived Grok 4.7 high parent and the earlier Grok 4.6 xHigh
parent. The same xhigh budget covers orchestration and review. Codex Sol /
Luna / Terra / Astra names remain packet vocabulary; they do not name Grok
models.

## Role mapping

| Packet role | Codex (historical) | Current Grok session | When |
|---|---|---|---|
| Orchestrator | `gpt-5.6-sol` / `low` | Parent at **Grok 4.7 xhigh** | Decompose the next packet, adjudicate evidence, integrate, and review meaningful results. |
| Standard worker | `gpt-5.6-luna` / `medium` | This parent, or a fresh subagent with a self-contained prompt | Leaf recovery, original-XBE contract tables, ABI/behavior tests, GPU/register/mapping once the contract is written, docs, localized fixes. |
| Expert worker | `gpt-5.6-sol` / `medium` | Same parent | Same failure after two attempts, or ABI / translation / weak-memory / multithreading / subsystem / contradictory-evidence problems. |
| Architect | `gpt-6-astra` / `medium` | Same parent in **plan mode** (`/plan`) | Evidence shows the design, interfaces, or task graph must change. Implementation returns to a sequential packet on the parent. |
| Reviewer | `gpt-5.6-terra` / `low` | This **xhigh** parent, or a fresh xhigh subagent | Review a meaningful packet. The orchestrator may review. A spawned reviewer is a fresh context, not `resume_from` the implementer. |

There is no separate cheaper worker in this session and no Home Qwen path.
`spawn_subagent` inherits this parent's model, so a child of this session is
also Grok 4.7 xhigh.

GPU or kernel work stays on this parent once its contract is clear. The expert
gate is a second attempt or a plan-mode pass on the same parent.

## How Grok runs this repo

The parent session is the orchestrator. Grok children cannot spawn children.
A worker prompt must not tell the child to launch a reviewer or architect.
The parent launches the worker, then reviews the result itself or launches a
fresh reviewer.

`spawn_subagent` inherits the parent's model. This session's spawn tool has
no model or effort argument. The parent is Grok 4.7 xhigh, so a child of this
session is also Grok 4.7 xhigh. Put the full packet in the child prompt.

Codex `collaboration.spawn_agent`, `fork_turns="none"`, and explicit
`model` / `reasoning_effort` arguments do not exist here. Put the full packet
in the subagent prompt so the child starts without the parent's investigation
dump.

Isolation:

- Shared-tree `explore` (read-only) for original-XBE disassembly and contract
  tables.
- `isolation: worktree` for implementation that edits manifests, recovered
  bodies, tests, or toolkit sources.
- Shared tree for the parent when it is the single build/regeneration/run
  owner.

Prefer sequential small packets. Use one optional second worker only for an
independent useful packet. After two attempts on the same root problem,
collect missing evidence or request an architectural replan.

Trivial mechanical edits fully verified by objective checks do not need a
separate reviewer. Workers still run their own isolated acceptance tests
before review.

## Packet contract

Every delegated packet names all of the following. One owner performs build,
regeneration, and run.

- Milestone and expected outcome
- Source range and files
- Facts versus hypotheses
- ABI / interface constraints
- Baseline artifact
- Exact acceptance commands
- File ownership

Role names in custom Grok agents or personas are optional. They do not imply
a mandatory first hop or recurring fresh architect involvement.

## Effort gates

Do these on the Grok 4.7 xhigh parent, or hand them to a fresh subagent with
a self-contained prompt:

- One helper or one register contract with a written expected behavior
- Approved recovery entries inside the current manifests
- Bounded ABI / behavior tests and focused fixtures
- Offline decoder / harness scripts
- Documentation and report/plan/guide updates
- Localized fixes
- GPU / register / mapping implementation after the contract is clear
- Review of a completed packet, by this parent or a fresh xhigh context

Stay on this parent, and use `/plan` when the interface itself is the
question, for:

- The same failure after two attempts
- ABI, translation, weak-memory, multithreading, subsystem-boundary, or
  contradictory-evidence problems
- Architecture, interface, or task-graph changes
- The kick/GET contract named below

Keep fatal traps in place until the named contract passes. Do not add a
success stub, synthetic status clear, worker-inline workaround, or production
deadline.

## Session shape

1. Start the parent at Grok 4.7 xhigh. Read `AGENTS.md`, this map, the
   current plan row, and the live report.
2. If the packet is a known leaf with a written contract, do it on this
   parent or hand a self-contained prompt to a fresh subagent.
3. Expert-gate and architecture packets stay on this parent. Use `/plan`
   when the interface itself is the question.
4. After a meaningful change, this parent reviews it, or spawns a fresh
   xhigh reviewer / runs `/review`. Do not `resume_from` the implementer
   for that review.
5. One owner builds, regenerates, and runs. Update the report with bold
   **Decision** entries.

## Current handoff

The expert-gate contract across `0x00193D90..0x00193EE2`,
`0x00194210..0x001942F8`, and `0x00197AAC..0x00197BCF` is accepted in
`docs/jsrf-callback-reentry-contract.md` with 5,440 bounded checks.

`0x00193D90`, `0x00194210`, `0x00197AAC`, and `0x00196C4A` are recovered.
In-place Type-0 `KeSetEvent` and PGRAPH_INTR W1C are in the toolkit.

Hardware setup through framebuffer publish is recovered, including
`0x00194ADD`, `0x00194780`, `0x00197C50`, RAMIN object helpers, and
`0x001912A0`. Ordinary startup stops at `0x001918E0`. Keep `0x00193F70`,
`0x0018E120`, and `0x001918E0` fatal. The next packet is the kick/GET
contract for `0x001918E0` → `0x001917F0` → `0x001916B0`/`0x00191530`
plus `0x00190240`, on this Grok 4.7 xhigh parent. The same parent reviews
that contract before any recovery.

## Reading older Codex wording

When `AGENTS.md`, the plan, or the report say:

| Older phrase | Current meaning |
|---|---|
| Sol low / orchestrator | Parent at Grok 4.7 xhigh |
| Luna / Luna medium | This parent, or a fresh subagent |
| Sol medium / Expert / xHigh | This parent |
| Astra / Astra medium | This parent in plan mode |
| Terra / Terra low | This xhigh parent, or a fresh xhigh reviewer |
| Grok 4.6 High / xHigh | Historical |
| Grok 4.7 high | Historical; this parent is Grok 4.7 xhigh |
| Home Qwen | Unused; skip optional parallel cheap drafts |
| `collaboration.spawn_agent` | `spawn_subagent` with a self-contained prompt |

Standing delegation authorization still applies. No fresh request is needed
for a suitable High implementation packet.
