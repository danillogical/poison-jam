# `A2h` — the component-bounds test: **UNINFORMATIVE** (bounds admit the value)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Authority:** the Advisor's ruling (`a2h-chain-completion-park-advisor-ruling.md`, turn `01a0e8d1`), which
authorized this as **hour-scale ANALYSIS if cheap** — *"bounds-exclude ⇒ exoneration finding, no packet;
bounds-admit ⇒ uninformative, don't packetize it either."*

## The test, and its result

**To produce `0x001D5078` the four packed bytes must be `0x00, 0x1D, 0x50, 0x78` = 0, 29, 80, 120.**

**The Worker traced each component through `clamp` → `fmul 255.0f` → `fadd 0.5f` → `call 0x192a80`.**
**The Session read the two clamp constants from the original XBE:**

| Constant | Value | As float |
|---|---|---|
| **`0x001C4578`** | **`0x3F800000`** | **1.0** |
| **`0x001C43D0`** | **`0x00000000`** | **0.0** |

> ## **So `Q(v) = (int)(clamp(v, 0.0, 1.0)·255.0f + 0.5f)` yields ANY byte in `0..255`.**

**`0, 29, 80, 120` are all in range. THE BOUNDS ADMIT THE VALUE.**

## Disposition: UNINFORMATIVE — and therefore NOT packetized

**Per the Advisor's rule, `bounds-admit` is uninformative and must not be packetized.** **So:**

- **the store is NOT exonerated by bounds** — **it can produce the value;**
- **and the store is NOT implicated by bounds either** — **the bounds say nothing;**
- **so this test neither closes nor narrows the `(a)`/`(b)` fork.**

**Recorded so the test is not re-run and its null result is not mistaken for evidence in either direction.**

## What this leaves standing

**The `(a)`/`(b)` fork — *coincidental byte tuple* versus *a different store wrote the slot* — is
EMPIRICALLY DECIDABLE ONLY BY OBSERVATION.** **That is the Advisor's ruling, and this null result confirms it:
the static budget on this question is exhausted.**

**And the `(a)`/`(b)` fork is now sharper than when the Advisor ruled, because the BYTE TUPLE reading is a
DATAFLOW finding rather than a decomposition:**

> **The store's output IS a packed byte tuple by construction. So if it wrote the slot, the four components
> took the values `0, 29, 80, 120` — a specific, checkable coincidence. If it did not, a COMPETITOR wrote a
> pointer.** **A watch on the slot address decides which, in one measurement.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited.**
