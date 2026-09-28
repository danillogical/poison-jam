# `A2h-callback-slot-writer-r1` — row selection: **`O-OPEN`** (named edge: `edi` identity)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-callback-slot-writer-r1`, frozen
**`D2CA02E17E2BACC1DE3726B3247965EA3A3B3061B504C06C9601F7FB4B894E3A`**.
**Authority:** the Advisor ruling `docs/reviews/a2h-callback-slot-identity-advisor-ruling.md` (turn
`01a0e82b`), which confirmed the Session's own reading.

---

## ROW: `O-OPEN`

**The named edge: `edi`'s actual identity.**

**The packet's `O-OPEN` predicate is met** — *"No demonstrated writer, unresolved alias/boundary/order,
incomplete section coverage or value path, or any other insufficient evidence"* — **because the context
object the failing call reads through is still unidentified.**

## Why NOT `O-ALTERNATE-PATH`

**The Advisor's reasoning, verbatim:** *"`O-ALTERNATE-PATH` requires *verified distinct context identity*
**plus** routes/mismatch defeating the path, and its next action is a packet *'on the REAL context object'* —
which cannot be scoped around an unknown."*

**The Session delivered a REFUTATION, not an IDENTIFICATION:**

| Delivered | Status |
|---|---|
| **No call site constructs `device+0x2268`** (all four decodable sites pass `ecx` as a parameter) | **verified** |
| **`[device+0x2268] = 0xFD` in one mapping-clean snapshot** | **verified** |
| **`edi`'s actual identity** | **`UNKNOWN`** |

**So the row's positive half is unmet.** **The Session's own instinct was right** — it wrote that it *"would
rather record `O-OPEN` with a precise named edge than claim `O-ALTERNATE-PATH` on a refutation alone"* — **and
the Advisor confirmed it.**

## What the refutation DID establish — permanently

**The Advisor drew out a consequence the Session had not:** *"What you delivered … **defeats the device-slot
branch permanently — no future packet may re-litigate the alias without new evidence.**"*

> **So the device-slot branch is CLOSED.** **`device+0x242C` is not the slot this call reads, and no future
> packet may reopen that question absent new evidence.** **That is a real narrowing even at row `O-OPEN`.**

**And the installer chain's status is now precise:** **`MEM32(device+0x242C) = 0x0015F9D0` confirms
`sub_0018CE30` installed the refcount thunk, exactly as the generated source says.** **The chain is CORRECT
and it RAN — it simply does not feed this call.**

## The successor IS the identification

**Per the ruling:** *"note the successor **IS** the identification (caller-trace), not conditioned on it."*

**So the next static step is discriminator-1-one-level-up:** **trace the caller of each containing function to
identify `edi`.** **The five call sites cluster in one polling family** (all read `[esi+0x100]` testing bit
`0x01000000`), **which is the thread to pull.**

**But it is QUEUED BEHIND THE AUDIT** — *"its register inputs are exactly what the audit clears."*

## Caveats the ruling attached, recorded so they are not lost

- **The `0xFD`/layout values are SINGLE-SNAPSHOT corroboration.** **The static no-construction-site finding
  carries the refutation**; the snapshot corroborates it.
- **The 23/24 integrity census must NOT be cited for any specific row.** **Per-row primary mapping checks are
  required.**

## Prohibitions and status

**Static and offline only** — no game run, no source edit, **no instrumentation.** **No synthetic
completion.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE` stays
DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired
NULL line was not reopened and no DR record was cited**; **float-bit siblings remain contrastive.**
