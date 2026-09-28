# `PIO_FREE-title-demand-bound-r1` — Session verification and promotion

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/pio-free-title-demand-bound.md`.
**Class:** **discovery** (§5.8) — so the **writing Planner** reviews its own packet against §5.3's two
questions and **no second Planner is spawned** (`docs/agent-workflow.md:615-617`, `:743`).
**Authoring Planner:** child `c52f4425-2b6c-42b8-816f-8e6e7e6f7d4a`, `codex/gpt-6-sol` @ `high`.
**Adequacy:** the writing Planner's own review — **`ADEQUATE`**, `BLOCKING: NONE`,
`PREMISE_FRESHNESS: BOUNDED`. **No shape preflight** (a discovery does not trigger one).

| Item | Value |
|---|---|
| Revision | **`PIO_FREE-title-demand-bound-r1`** |
| SHA-256 (frozen) | **`977A96F708919F309AE01009BA64AB75AA344794950553530CCA1AF958A49ABB`** |
| Lines | **34** |

**Hash verified 3× over ~12 s immediately before promotion**, all identical, and matching the Planner's own
reported hash.

## The identity question the Planner raised — resolved

**The Planner flagged that the Session's concurrent documentation commits had advanced game HEAD after its
own clean identity check, and asked the Session to pin the current revision.** That was the right thing to
flag. **Verified:**

| Check | Result |
|---|---|
| Commits between the packet's planning pin (`91c626a`) and HEAD | **2** — `ec57057`, `3217fd4` |
| Paths changed | `docs/packets/pio-free-title-demand-bound.md`, `docs/reviews/pio-free-sufficiency-analysis.md`, `plan-jsrf-bare-minimum.md` |
| **Paths under `src/`, `game/`, `config/`, `scripts/`, `tools/`** | **NONE** |
| Conclusion | **Documentation-only.** No code, no generated file, no asset, no toolkit change |

**So the packet's premise is intact**: the intervening commits cannot have changed the guest, the toolkit,
or any executable. The packet's own text already requires this check (*"later documentation-only commits
must be pinned and inspected by Session"*), and the Session has now performed it.

## Command validation (§5.1.5.2 — the Planner asked for it explicitly)

The packet names specific commands and tool paths. **Each was validated before freeze:**

| Named command / path | Validation |
|---|---|
| `python -X utf8 scripts\inspect-jsrf.py find 0xFE820010` | **works — reports `28 occurrence(s) of 0xFE820010`** |
| The packet's claim that the **`0x` prefix is mandatory** | **correct** — bare `FE820010` is **rejected**: `invalid <lambda> value: 'FE820010'` |
| `game/mygame_analysis.json` (the tool's `--analysis` input) | **exists, 17 047 bytes** — and `inspect-jsrf.py:29,70` genuinely reads it, so the path is real |
| `python -X utf8 -m unittest scripts.test_pio_free_demand` | **fails as expected** — the target does not exist yet; it is the packet's **deliverable** (`scripts/pio-free-demand.py` + `scripts/test_pio_free_demand.py`) |
| `scripts/pio-free-demand.py` | **does not exist yet** — likewise the deliverable |
| `python -X utf8 scripts\inspect-jsrf.py disasm 0x… 0x…` | works **with** the `0x` prefix; the packet requires it |

**No named command is broken, and the two "missing" paths are the packet's own outputs, not prerequisites.**
The packet is executable as written.

## Why the reframing is the right class and the right question

The packet **does not repeat the exhausted search**. It records that the `O-UNKNOWN` row's named successor
— a focused five-leaf *source/queue-interface* discovery — is **not presently feasible as a hardware
specification discovery**, because `docs/jsrf-run-profiles.md:245-267` admits only external hardware
documentation and **every lead has been searched and is silent** (primary archived and silent; secondary
silent; a first-party account silent; an independent emulator lineage silent).

**It then asks the question that is still admissible.** Guest code is *"evidence about the title, not about
the hardware"* (`:260-262`), so:

> *"is every feasible **direct** `PIO_FREE` gate in the frozen 28-site set guaranteed to have an unsigned
> compare demand at most the stub's available value …? Or is a reachable demand above the applicable value
> witnessed, or is a bound unprovable?"*

**That is a title question against a fixed stub, and it is decidable offline.** The packet is explicit that
it establishes **no hardware model** and that the five hardware leaves **remain `UNKNOWN` regardless of the
answer**.

## What the packet correctly treats as a lead rather than a finding

It cites the Session's sufficiency analysis but labels it **"a window heuristic, not a verified extractor or
field-maximum proof"**, singles out **`001A3F24`** as needing reaching-definition resolution, and **requires
a checked-in, tested classifier to re-derive the 15/13 census before any of it is used**. It also carries
both of the Planner's own corrections — **constant literals other than `4`** and **`eax` as well as `ecx`
demand registers** — and requires the classifier to **fail on inputs** rather than silently decode.

**That is exactly right**, and it is the discipline this analysis has repeatedly needed: **four ad-hoc
parsing failures** occurred in the Session's own work, including a disassembler that **silently returned
plausible garbage** for a mid-instruction start.

## The escalation path is encoded

`O-EXCEED` does **not** quietly become a pass: it requires a **feasible** demand `> 32` **with
original-byte, definition and path/input witnesses**, then names a **reachability discovery plus an explicit
Persistent Advisor hardware-sourcing/scope referral**, and states that a hardware model **cannot be
implemented or admitted as strict evidence merely on this title result**. `O-BOUND` likewise goes to the
**Advisor for a scope/defer/retire ruling** rather than authorizing a change packet. **No row claims strict
progress**, and `A2h` is named only where a later question needs **trapped** strict time beyond the captured
prefix.

## Promotion

`PIO_FREE-title-demand-bound-r1` at **`977A96F7…A49ABB`** was **frozen and promoted into `CURRENT PACKET` in
the same step**, byte-identical with no revision (§5.3). **The Session did not edit the packet.**

**Standing constraints carried:** no synthetic completion; no source becomes true by repeating `0x80`;
`0xFFFFB3` stays **`UNRESOLVED`**; **do not reopen `A4b2-r7`, accepted/closed `A4b2-r8`, or `A4b1-r4`**; a
toolkit advance requires **re-establishing the P4 discovery-transfer bridge** first.
