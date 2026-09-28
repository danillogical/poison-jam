# `A2h` — the AC97 interlock's real location, and what it means for the live packet

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why recorded:** the approved preflight says *"single-step re-arm keeps the AC97 interlock obligation"* and the
packet says *"the collector ALREADY HAS an AC97 interlock — find it and integrate rather than duplicating."*
**The Session located it, and its location is NOT where the packet's briefing implies.**

---

## The interlock is in the TOOLKIT, in `xbox_memory_layout.c` — not in the collector

**Session-verified:**

```
src/kernel/xbox_memory_layout.c:342  #define AC97_NABM_OFFSET  0x400000u   /* 0xFEC00000 within the MCPX aperture */
src/kernel/xbox_memory_layout.c:343  #define AC97_TRAP_BYTES   0x1000u
src/kernel/xbox_memory_layout.c:344  #define AC97_RR           0x02u
src/kernel/xbox_memory_layout.c:365      if (*r & AC97_RR)
src/kernel/xbox_memory_layout.c:366          *r = (uint8_t)(*r & ~AC97_RR);
src/kernel/xbox_memory_layout.c:386      VirtualProtect(g_ac97_page, AC97_TRAP_BYTES, PAGE_READONLY, &old);
```

> ## **So the AC97 mechanism is ALREADY a `VirtualProtect(..., PAGE_READONLY, ...)` trap — THE SAME MECHANISM the live packet is authorized to use.**

**That is a significant simplification, and the Session records it because it changes the integration shape:**

1. **The page-protection pattern the preflight mandates is NOT new** — **`xbox_memory_layout.c` already
   implements exactly it for AC97**, including the **`PAGE_READONLY` protection**, a **`0x1000`-byte
   (page-sized) trap region**, and a **reset-bit (`AC97_RR`) clear-and-retry protocol.**
2. **The packet's declared write scope already names `xbox_memory_layout.c`** — **so the natural home for the
   slot-page guard is the SAME FILE as the AC97 guard**, and the two must coexist without interfering.
3. **⚠ AND THAT IS THE REAL INTEGRATION RISK:** **two `PAGE_READONLY` traps on different pages, each with its
   own reset protocol.** **If the slot page and the AC97 page are distinct, they do not collide — but the
   fault handler must dispatch on the faulting PAGE, not on a single global "is a trap armed" flag.**
   **The Session flags this as the thing to check.**

## The AC97 reset-bit protocol, read from the bytes

```
if (*r & AC97_RR)
    *r = (uint8_t)(*r & ~AC97_RR);
```

> **The AC97 trap CLEARS A RESET BIT in the faulting register and continues** — **it is a
> write-and-clear-and-retry, not a single-step.**

**⚠ SO THE AC97 PATH DOES NOT USE `EFlags.TF` AT ALL.** **The Session searched `src/` for
`STATUS_SINGLE_STEP`/`SINGLE_STEP` and found ZERO hits, and the toolkit's AC97 code uses the reset-bit route.**

**That materially changes the packet's `#DB`-ownership requirement:** **the preflight said *"single-step
re-arm keeps the AC97 interlock obligation: specify the dual-mid-step `#DB` ownership exactly (own-TF vs
AC97-TF, bit-exact)."*** **But if AC97 does NOT set TF, then:**

- **the `AC97-TF` pending bit may be a DEFENSIVE requirement rather than an existing conflict;**
- **the real question is whether the SLOT trap's single-step can be swallowed by, or swallow, the AC97
  handler** — **and since AC97 clears-and-continues rather than single-stepping, the interaction may be
  simpler than the preflight assumed.**

**The Session does NOT conclude that the `AC97-TF` bit is unnecessary** — **the packet requires it and the
preflight mandated it.** **But the Session records that the AC97 mechanism is a RESET-BIT trap, not a TF
trap, so the dual-`#DB` ownership scheme protects against a conflict that the Session could not locate.**
**The fixtures' synthetic-overlap test is what would prove or refute its necessity.**

## The collector's own single-step machinery

**`tools/harness/collect.c` has the DR-era single-step accounting** (`dr_raw_single_step` at `:922`, the
completeness denominator bumped *"FIRST, before the code filter below can return early"*), **and the
`EXCEPTION_SINGLE_STEP` route at `:915`/`:2227`.**

**⚠ And that accounting is DR-gated** (`dr_on()`), **which is now EXCLUDED.** **So the collector's
single-step counters are part of the retired DR instrument and must NOT be reused as the live packet's
evidence channel** — **the packet's own counters are new and must be independent.**

## What the Session is flagging for the implementation

1. **The page-protection pattern already exists in `xbox_memory_layout.c`** — **reuse the pattern, do not
   reinvent it.**
2. **⚠ The fault handler must dispatch on the FAULTING PAGE**, so a slot-page fault and an AC97-page fault
   are distinguishable. **Two global "armed" flags would be a defect.**
3. **The AC97 trap is a reset-bit trap, NOT a TF trap** — **so the `AC97-TF` pending bit's necessity is
   UNCONFIRMED, and the synthetic-overlap fixture is what settles it.**
4. **The collector's existing single-step counters are DR-gated and therefore RETIRED** — **the live packet's
   counters must be new and independent.**
5. **`src/main.c`'s VEH (`:71`) handles `STATUS_GUARD_PAGE_VIOLATION` and `EXCEPTION_ACCESS_VIOLATION` and
   dispatches by guest-fault RANGE** (`0xFD000000..0xFE000000` for NV2A, `0xFE800000..0xFE880000` for APU).
   **A slot-page fault at `0x0019D000` is OUTSIDE both ranges, so it would fall through to the
   `[EXCEPTION first-chance]` print — which is a useful starting point but not the trap.** **The new handler
   must be added at a defined point in that dispatch, and the VEH ORDER is already a carry-forward gate.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**No DR record cited, and DR remains EXCLUDED as a mechanism.** **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was
not reopened.**
