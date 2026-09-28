# `A2h-callback-context-identity-r2` — stage-1 acceptance: **`ACCEPT-WITH-CORRECTIONS`**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Reviewer:** `4d9dfce4-c589-4428-9df7-fd6699b566f5` (`workbuddy-ai/hy4-preview-f` @ `high`), **independent.**
**Review:** `docs/reviews/a2h-callback-context-producer-acceptance-review.md`, committed `58694a5`.
**Disposition: `ACCEPT-WITH-CORRECTIONS`.**

---

## ✅ THE DECISIVE RESULT: every mandated byte check CONFIRMED

**The Advisor ruled the Session's refutation retracted *conditionally* — the condition being that the cited
bytes verify. The reviewer checked all five in the original XBE:**

| # | Claim | Verdict |
|---|---|---|
| **1** | **`0x0019460A mov dword ptr [ecx], 0xfd000000`** | **CONFIRMED** |
| **2** | **`0x001925FB lea edi, [esi + 0x2268]`** | **CONFIRMED** |
| **3** | **`0x001925A2 mov esi, ecx`** | **CONFIRMED** |
| **4** | **the DPC binding** | **CONFIRMED** *(one scope caveat)* |
| **5** | **the thunk binding** | **CONFIRMED** |

> **So the retraction STANDS on verified bytes, and the finding *"the context is LOCATED AT `device+0x2268`"*
> SURVIVES a falsification attempt.**

**The reviewer's own words:** *"The Advisor's *conditional* condition is therefore **satisfied**, and the
retraction's logic stands on verified bytes."*

**And it independently ran the discriminating control:** *"I ran the discriminating OFFSET-SHIFT test
myself."*

## The five corrections — all in the evidence RECORD, none in the substance

| # | Correction | Class |
|---|---|---|
| **C-1** | **the selected row `O-ALTERNATE-PATH` is WRONG** — the evidence's own finding contradicts that row's predicate. **Correct row: `O-OPEN` SUSTAINED, edge advanced.** | row |
| **C-2** | **no DISCRIMINATING control in the Worker's evidence** — §6 records only the non-discriminating `0x001D5078 → 30766A64 …` | control |
| **C-3** | **call-site addresses cited OFF BY ONE** — every cited address is the instruction **after** the `call` | citation |
| **C-4** | **`sub_00194C3F`'s "`ecx = ebx + 0x2268`" is not in the bytes**; the `+0x1A0` arithmetic is rendered as a **store** when it is a **`push`** | rendering |
| **C-5** | **the producer chain is not closed at the top** — no caller of `sub_00194ADD` traced, no `KeInsertQueueDpc` site located | **scope gap** |

**The reviewer's framing is exactly right:** *"The corrections are real but all live in the EVIDENCE RECORD,
not in the execution's substance."*

## C-1 — the Advisor predicted this and was right

**The Worker selected `O-ALTERNATE-PATH`.** **The Advisor refused it in advance:** *"NOT `O-ALTERNATE-PATH` —
that row requires *distinct* identity, the opposite direction."*

**The reviewer independently reaches the same conclusion:** **the evidence's own finding contradicts the
row's predicate.** **Correct row: `O-OPEN` SUSTAINED, with the edge ADVANCED.**

> **So the outcome is the Advisor's stated advance prediction, confirmed by an independent reviewer:**
> **`O-OPEN` SUSTAINED, and the precise unknown is now the WRITER of `device+0x242C`.**

**That is a genuine advance** — the Session began with *"what object is `edi`?"* and now has **a named object
and a named field**, with the writer as the single remaining unknown.

## C-2 — the discriminating-control gap, which is the Session's own lesson applied

**The Worker's evidence recorded the NON-discriminating control.** **The reviewer flagged it, and separately
ran the discriminating one itself.**

**This is the Session's instance-7 lesson propagating into the review structure** — **and the reviewer caught
what the executor missed.** **Recorded because it shows the control set working as intended rather than
depending on any one agent.**

## C-3, C-4 — citation and rendering defects worth naming precisely

**C-3 is systematic:** **every call-site address is off by one, being the instruction after the `call`.** **The
reviewer notes this does not affect the byte checks**, **but a reader following those addresses would land one
instruction late.**

**C-4 is a rendering error with a correct conclusion:** *"The rendering is wrong; the **conclusion** that the
…"* — **the reviewer distinguishes the two, which is the right call.**

## C-5 — the genuine scope gap, and it is now the named edge's parent

**The reviewer found something neither the Session nor the Advisor had named:** **the producer chain is not
closed at the TOP.** **No caller of `sub_00194ADD` is traced, and no `KeInsertQueueDpc` site is located.**

**So there are TWO edges now, and the Session records both:**
1. **the writer of `device+0x242C`** (the callback slot's value); and
2. **the caller of `sub_00194ADD` / the `KeInsertQueueDpc` site** (what starts the DPC).

**The reviewer's C-5 is a real addition to the packet's own `O-OPEN` naming, and it should be carried into the
successor.**

## What this acceptance establishes, in order of durability

1. **The Session's `device+0x2268` refutation is RETRACTED on verified bytes.** **The Advisor's conditional is
   satisfied.**
2. **The context IS `device+0x2268`** — survived a falsification attempt, with the `lea` confirmed.
3. **The context's `+0x00` is the NV2A MMIO aperture `0xFD000000`**, written by `0x0019460A` — **confirmed**,
   **and it is what made the Session's byte-reversed `0x000000FD` so misleading.**
4. **`MEM32(device+0x242C) = 0x0015F9D0`** — **the refcount thunk; the installer chain is CONFIRMED**, after
   the Session had reported the opposite.
5. **The row is `O-OPEN` SUSTAINED with an advanced edge**, and **the Worker's `O-ALTERNATE-PATH` selection
   was wrong in the direction the Advisor predicted.**

## Prohibitions and status

**Static and read-only.** **No game run, no build, no instrumentation, no source edit.**
**No synthetic completion.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged.
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited**; **float-bit siblings
remain contrastive.** **All eight guards pass.**
