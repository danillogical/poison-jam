# ⚠ `A2h` — **POSITIVE CONTROL FAILURE**: the DR0 write watch has never been observed to fire

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Found by:** the Session, while scoping what a fix packet would target after the `O-READ-PATH` ruling.
**Severity: this is the most consequential finding of the A2h line.** It **does not invalidate the census
result**, and it **does** invalidate every statement that rests on *"zero DR hits means no write."*

---

## The argument, link by link — every link verified in source or from the archive

**The install store is a KNOWN canonical-slot write performed under an armed DR0 watch. It should have
produced a `#DB` hit. It did not.**

| # | Link | Evidence |
|---|---|---|
| 1 | **The toolkit performs the handshake — which arms DR0 — BEFORE the patch loop** | `kernel_bridge.c:9395` `a2h_install_handshake();` precedes `:9411` (the loop) and `:9468` (the store) |
| 2 | **The loop READS the slot** | `:9412` `uint32_t current = BRIDGE_MEM32(va);` |
| 3 | **The loop WRITES the slot, UNCONDITIONALLY** | **`:9468` `BRIDGE_MEM32(va) = synthetic;`** — **outside the `a2h_slot_trace_on()` gate at `:9461-9467`, so it always executes** |
| 4 | **The write is to the WATCHED HOST ADDRESS** | `BRIDGE_MEM32(a) = *(u32*)(a + g_xbox_mem_offset)`; the run reports *"mapped 65536 KB at `0x10000`"*, so guest `0x001C4064` → host **`0x1D4064`** |
| 5 | **DR0 watches exactly that address, as a 4-byte WRITE watch** | `GUEST_DR_ARM_OK seq=20 tid=43016 dr0=00000000001D4064 dr7_readback=00000000000D0001`. **`DR7 = 0x0D0001`: `L0=1` (enabled), `R/W0=01` (data writes), `LEN0=11` (4 bytes)** — correctly programmed |
| 6 | **The store runs on the ARMED thread** | the `[A2HSLOT] install tid=43016` line carries **the same tid as the handshake**, and **43016 is guest identity 1** and **is in the armed list** |
| 7 | **The store happens AFTER the arm** | log line 50 `handshake … state=ack`, log line 52 `install …` — **the arm precedes the store, which is the handshake's entire purpose** |
| 8 | **NO HIT WAS RECORDED** | `GUEST_DR_DISARM … hits=0 hit_overflow=0`; **`GUEST_DR_HIT` count is 0 in every run of this line** |

**Every link holds. A 4-byte write to the watched address, by the armed thread, after the arm, under a
correctly programmed write watch, produced no `#DB`.**

## What this means

**The DR0 watch has never been observed to fire — not once, in any run of this line, including for a write
that demonstrably happened.** So:

> **zero DR hits cannot be read as "no write occurred." They are equally consistent with a watch that cannot
> fire.**

**This is the Advisor's flagged uncertainty #1 arriving as a positive result rather than a caveat.** The
implementation record said: *"whether a DR0 hit actually arrives as a first-chance `EXCEPTION_SINGLE_STEP`
under this collector could NOT be tested without running the game."* **It could be tested — the install store
is the test — and the answer is that it did not arrive.**

## What this does NOT invalidate

**The alias census is UNAFFECTED.** `armed=1 mapped=28 protected=28`, `touched_count=0` is **page-protection
based**, runs **process-wide**, and **does not depend on DR registers at all.** **That result stands, and it
remains a genuinely certified channel.** *(Its own completeness caveat — first-touch only — is unchanged and
unrelated.)*

**The terminal observations are UNAFFECTED.** The generated read and the hook read both returned zero; **those
are software reads of live memory**, not DR events.

**The install positive control is UNAFFECTED** — it is a software comparison of the raw and installed values,
and it **passed**.

**The arming records are UNAFFECTED** — `DR7` **did** read back `0x0D0001`, so **the registers were genuinely
programmed.** **The watch was armed; it simply never fired.**

## What this DOES invalidate

**Every statement in this line that rests on "zero DR hits = no canonical write."** **That includes:**

- the Session's own repeated phrasing that the faulting thread *"produced zero canonical DR hits"* — **true as
  a record, but it certifies nothing about writes**;
- **leg 3 of the `O-READ-PATH` row** as the Planner construed it — *"DR/mapping leg reconciled."* **The
  mapping half is certified; the DR half has never certified anything**, because there is no demonstrated
  firing to reconcile against.

## Consequence for the row just ruled

**The `O-READ-PATH` ruling rests on the claim that the observation surface was positively certified and
within it no zero-write was observed.** **After this finding, the certified surface is narrower than the
ruling assumed:**

| Channel | Status after this finding |
|---|---|
| **28-alias census** | **certified** (page protection, process-wide, zero touches) |
| **Mapping stability** | **certified** |
| **Generated read + hook read both zero** | **certified** (software reads) |
| **Canonical-address writes** | **NOT certified** — the only instrument for them has never been shown to fire |

**So the honest statement is narrower:** *no write was observed through any of the 28 aliases, and the
canonical-address channel was instrumented but the instrument was never demonstrated to work.*

**The Session is NOT re-deciding the row.** This is a **`PREMISE_CHANGED`** for the ruling that selected it,
and per `docs/agent-workflow.md` §4.3 it goes back to the Planner, and to the Advisor if the mechanism choice
is affected.

## The constructive consequence — this is now FIXABLE, and cheaply

**The install store is a BUILT-IN positive control that has been present all along and was never recognised
as one.** **The successor does not need a new mechanism to settle this:**

1. **Make the DR0 watch's firing a REQUIRED positive control**: the install store is a known canonical write
   under an armed watch, so **a `#DB` hit there must be observed, or the DR leg is declared non-functional
   and every DR-based claim is `UNKNOWN`.**
2. **If the watch cannot be made to fire, say so and drop the DR leg** rather than carrying a channel that
   certifies nothing — **the census and the two software reads are the real evidence.**
3. **The candidate cause is worth stating:** a data breakpoint delivered to a process under
   `DEBUG_ONLY_THIS_PROCESS` may surface as **second-chance**, or be **swallowed by the debugger's own
   handling** — the implementation record flagged exactly this and it is now the leading hypothesis.
