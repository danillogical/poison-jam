# A4b1-r2: the `[GPIN]` lineage conflict (raised by the Planner, verified by the Session)

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** **open Advisor-class question.** Referred to the persistent Opus 5.5 Advisor with the
`A4b1-r2` shape preflight. Recorded now so the facts cannot be lost while the ruling is pending.
**Not** a packet revision and **not** a Session decision.

## The conflict

Two authorities that the `A4b1-r2` brief tells the Planner to preserve are **at different lineage
points**, and the packet on disk pre-dates one of them.

| | Authority | What it says | On disk as a packet? |
|---|---|---|---|
| (a) | `docs/packets/a4b1-gp-core-port.md` = **`A4b1-r3`** | `[GPIN]` is a **fixed 256-entry `(kind, addr)` table** + `GPIN_OVERFLOW` latch (L123-124); `AC-FIX` (ix) is the 258-distinct-keys overflow case (L312) | **yes** — this is the current packet |
| (b) | `docs/reviews/a4b-gpin-accounting-ruling.md` | The table design is **wrong**; replace it with **provenance classes over finite universes** (MIXBUF `bins[32]`, PERIPH `[128]`, FIFO `[6]`, DMA region classes `[4]`), an `at_clear` write-once freeze at `GP_CLEAR`, and `GPIN_OUT_OF_UNIVERSE` as a **bug detector**. `AC-FIX` becomes `(ix′)/(x)/(xi)` | **NO** — its `RECORD IN` targets **`A4b1-r4`**, which was never written |
| (c) | `docs/reviews/a4b1-a4b2-r3-adequacy-review.md` | **`INADEQUATE`** on `A4b1-r3`/`A4b2-r3`; third consecutive §5.5 trigger on `[GPIN]`; blocking `A4b1` B1 = *"the `[GPIN]` key space exceeds the table"* | n/a |

**So the current on-disk `A4b1` packet carries a mechanism that has been ruled against and marked
`INADEQUATE`, because the revision that was supposed to fix it (`A4b1-r4`) does not exist.**

## Session verification (measured, read-only — the Session did not decide the question)

- `docs/reviews/a4b1-a4b2-r3-adequacy-review.md` exists (5516 bytes); its title line reads
  *"`A4b1-r3` / `A4b2-r3` adequacy review — **INADEQUATE** (both); §5.5 triggers on `[GPIN]`"*, with
  *"BLOCKING — `A4b1` B1: the `[GPIN]` key space exceeds the table"*.
- `docs/reviews/a4b-gpin-accounting-ruling.md` exists (14069 bytes). Its `RECORD IN` says verbatim:
  *"`A4b1-r4`: DS6 input accounting per (a); AC-FIX (ix′)/(x)/(xi); fold in the r3 B2 fixes …"* and
  *"`A4b2-r4`: AC-INPUTS per (b); rows and exhaustiveness per (c); P2 repinned with the new fields
  mapped."*
- **No `A4b1-r4` packet exists.** `docs/packets/` holds only `a4b1-gp-core-port.md` (35240 bytes) and
  the Planner's new `a4b1-gp-core-port-r2.md` (2670 bytes, sketch only). No `*r4*` packet exists
  anywhere under `docs/packets/`.
- The on-disk `A4b1-r3` **still carries the ruled-against design** at L123-124 and L312.

### Why this is subtle: the ruling's *policy* RECORD IN items were done, but its *packet* item was not

| Ruling `RECORD IN` item | Status |
|---|---|
| This ruling recorded verbatim | **done** — the file exists |
| Append the finiteness premise to `a4b-watch-ledger-ruling.md` (a)3 | **done** — L28 carries *"the bounded site table is safe only because the site universe is finite, enumerated in source…"* |
| Rule (d) as `docs/agent-workflow.md` §6.1 item **6b** | **done** — L699 carries *"**Decision inputs are bounded by construction.**"* |
| One sentence beside the evidence rule in `docs/jsrf-run-profiles.md` | **done** — L237 carries *"**A decision input must also be bounded by construction.**"* |
| **`A4b1-r4`: DS6 input accounting per (a); AC-FIX (ix′)/(x)/(xi)** | **NOT done** — no such packet was ever written |
| `A4b2-r4`: AC-INPUTS per (b); rows per (c); P2 repinned | **NOT done** |

**So the policy is recorded in three durable places while the packet text that must implement it was
never written.** That is why a "preserve the rulings" instruction and the on-disk packet can both be
honoured only one at a time.

## A further inconsistency the Session noticed

`docs/packets/a4b2-gp-clears-pending-word.md` (`A4b2-r3`) states in three places that `A4b1-r3` was
**`ACCEPTED` with `R1-PASS`** (L4 *"P2 repinned to `A4b1-r3`"*, L12 *"Depends on: `A4b1-r3`
(`ACCEPTED`, `R1-PASS`)"*, L24 *"P2. `A4b1-r3` was `ACCEPTED` with `R1-PASS`"*).

But `docs/reviews/a4b1-a4b2-r3-adequacy-review.md` says **`INADEQUATE`**.

**Those two on-disk statements are inconsistent**, and whichever way the `[GPIN]` ruling goes, one of
them is stale. This is flagged because `A4b2` is the successor packet and its preconditions `P2` rest on
the claim — and because the owner's directive is to proceed to `A4b2` only after `A4b1` reaches a
durable disposition.

## The question put to the Advisor

1. For `A4b1-r2`'s `[GPIN]` text, is the correct base **(i)** the on-disk `A4b1-r3` DS6 (256-entry
   table) — which would resurrect a ruled-against mechanism — or **(ii)** the provenance-class /
   finite-universe design of `a4b-gpin-accounting-ruling.md` (the `r4` design, never written into a
   packet)?
2. If (ii), confirm that `A4b1-r2` must incorporate the `r4` `[GPIN]` redesign as part of this
   revision — since a baseline-only re-bind cannot faithfully revise an `INADEQUATE`-r3 mechanism — and
   give a **bounded statement** the Planner can write into the packet's governing-requirement block.
3. If `A4b1-r2` should **not** carry it, say where it should go instead, given the owner wants `A4b2`
   only after `A4b1` reaches a durable disposition.
4. If the answer means `A4b1-r2` is **larger than a pure re-baseline**, say so explicitly — it affects
   the packet class statement and the adequacy review's scope.

## Session framing (offered as context, explicitly not a decision)

The owner's directive was to produce *"the smallest `A4b1-r2` packet necessary to re-bind the GP/DSP
investigation to toolkit baseline `3f8bf67c…`, incorporating only premise changes that can affect the
implementation/evidence decision"* and *"Do not redesign `A4b1` merely because A4s changed many
files."*

The Session reads the `[GPIN]` question as **not** falling under that prohibition, because it is **not
caused by A4s changing files** — it is a pre-existing `INADEQUATE` verdict plus a binding ruling that
was never written into a packet. But the owner also said to preserve the finite-universe redesign
*"unless a load-bearing premise is now contradicted"*, so this is put to the Advisor rather than
settled by the Session.
