# `A4b2-r8` — Session verification and promotion

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-gp-clears-pending-word.md`.
**Class:** **change** (§5.3 — full adequacy by a **non-authoring** Planner required).
**Authoring Planner:** child `f6718b45-6d75-440a-b81f-a1169d1fe8b5`, `codex/gpt-6-sol` @ `high`.
**Adequacy reviewer:** child `bedc4a02-024c-46ac-91c8-8960942bdecb`, `codex/gpt-6-sol` @ `high` —
**non-authoring**, read-only per §5.1.3.

| Item | Value |
|---|---|
| Revision | **`A4b2-r8`** |
| SHA-256 (frozen) | **`4D4AFC304F571971EB180C19D6832D56A2CC62FF6EAAFB9E4928716125397C62`** |
| Lines | **161** |
| Adequacy | **`VERDICT: ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS BOUNDED` |

## The required gate was actually run

`A4b2` is a **change** packet. Under §5.8 a **discovery** packet's adequacy may be the writing Planner's own
review — which is what the Session used for the five discovery packets this session. A change packet
requires the **full §5.3 review by a non-authoring Planner**.

**The authoring Planner flagged this itself**, and the Session did **not** freeze on its self-check. The
draft was committed at `28a51db` as `Status: draft`, and a **separate** reviewer was run against those exact
bytes. **The reviewer independently reconfirmed the hash** (`4D4AFC30…397C62`) before and after its review,
and wrote the sole new file.

## The reviewer's judgement on the substantive change

The one genuinely load-bearing question was whether the restated **`AC-INPUTS`** qualifier quietly weakens
a criterion. The reviewer ruled it:

> **sound only as exchange-specific substitution under recorded Advisor conditional ruling, not a value
> model or retroactive PASS**: `r7` stays `R2-EXPL-INPUT`, all other stubs retain `FAIL`, P2/P4 stop on
> unverifiable proof transfer and fresh tuple/image checks fail closed.

That is the distinction the Session was trying to hold: the two named stub classes get **non-reliance**
established by measurement, **not** a relabelling as "modelled", and **no** other stub's `FAIL` predicate
moves. **`r7`'s failed result is not rewritten.**

## Verified by the reviewer

| Check | Result |
|---|---|
| Packet hash | **matched**, before and after review |
| Archive manifest | **25 entries checked, zero missing or mismatched** (8 scripts + 8 outputs + 9 input artifacts) |
| Q3 carry list | **all four carried** |
| Forbidden dependencies | **neither** DMA chain-restart theory **nor** region-counter tallies used for block identity |
| Radix | `%04X` = hex, `0018` = block 24; **obsolete block 18 expressly excluded** |
| The seven acknowledged errors | *"do not defeat independently measured closure"* |

## Two `DEFERRED` items the reviewer named — recorded, not dismissed

1. **`independent-block24.py:82-90` ends with a hard-coded always-positive textual verdict** and checks
   whether literal source lines *mention* stub names, rather than performing an independently executable
   data-flow proof. The reviewer held this is **not an operative blocker** because the packet relies on the
   **measured** evidence (latch, ordinal 765/766, measured `#FIELDS`) rather than that script's verdict.
   **Recorded as a genuine weakness in the archived tooling** — a reviewer should not treat that script's
   summary line as an independent proof.
2. **`v2-cross-boundary.out.txt` individually says `NOT CLEAN`**, because that script predates the finding
   that the first-exchange histogram renders the later computed writes moot. **The output is honest for
   what it computes**; the *resolution* is in `v2-pre-vs-post-exchange.out.txt`. **Recorded so a future
   reader does not read the archived output as contradicting the conclusion.**

**Both belong in `A4b2-r8`'s execution record** so the next reader sees them, rather than discovering them
as apparent contradictions.

## Promotion

`A4b2-r8` at **`4D4AFC30…397C62`** was **frozen and promoted into `CURRENT PACKET` in the same step**,
byte-identical with no revision (§5.3). The hash was verified **three times over ten seconds** immediately
before promotion, matching the author's reported hash and the reviewer's independently verified hash. The
packet was **never edited by the Session**.
