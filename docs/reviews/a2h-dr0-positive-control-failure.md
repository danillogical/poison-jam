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

## Consequence for the row just ruled — **the Planner REVERSED the ruling to `O-COVERAGE`**

**The Planner re-ruled on this `PREMISE_CHANGED`, and it reversed its own `O-READ-PATH` ruling.** **Its
reasoning is decisive, and its source citations are exact — the Session verified both:**

**Citation 1 — the predecessor frozen packet ALREADY REQUIRED a native DR0 record, and explicitly said the
software control cannot substitute.** `docs/packets/a2h-slot-read-path-displacement.md:27`:

> *"The install write **MUST cause a native DR0 record** with expected before/after, site, tid, mapped VA and
> index; **the frozen latch's own `install_seen=1`, `install_ok=1`, raw=`0x80000115` and
> installed=`0xFE000104` are required IN ADDITION TO the trapped write**, not inferred from a printed line.
> The old `jsrf_slot_latch_install` callback at `:9325-31` occurs **BEFORE** that store and **cannot itself
> count as a trapped-install witness**."*

**Citation 2 — a missing trapped install selects `O-COVERAGE`, and the packet SAW THIS COMING.**
`docs/packets/a2h-slot-within-run-attribution.md:19`:

> *"**A missing canonical install trap is `O-COVERAGE`, never no-write: live `#DB` chance semantics and
> all-thread arming have not yet been proven in the game.**"*

**And line 25 lists the row trigger:** *"target but alias touch, **arming/install**/rearm/mapping/three-leg/
ledger/ordering/certification gap … ⇒ `O-COVERAGE`."*

**So the frozen contract anticipated exactly this outcome and prescribed the row in advance.** **The install
trap has never been demonstrated, and the packet says that is `O-COVERAGE` — *"never no-write."***

**The Planner's own words on the reversal:** *"I incorrectly treated software `install_ok` as a positive DR
firing control. Aliases + reads remain certified but cannot satisfy the required canonical/DR leg; K=3
reproducibility of terminal/control/census, not a certified writer row."*

## The corrected row

| Row | Status |
|---|---|
| **`O-READ-PATH`** | **WITHDRAWN** — it required the DR leg to contribute, and the DR leg has never certified anything |
| **`O-COVERAGE` → `A2h-slot-write-coverage-provenance`** | **SELECTED** — per the packet's own line 19/25 trigger |

**What K = 3 still buys, unchanged:** **reproducibility** of the terminal triple, the install control and the
census across three independent realizations. **It does not buy a certified writer row**, and it never did.

## The finding's own provenance, recorded because it matters

**The frozen packet PREDICTED this failure mode in writing** — *"live `#DB` chance semantics … have not yet
been proven in the game"* — and **the Session found it by asking what a fix packet would target, then noticing
that the install store is a known write under an armed watch.** **The prediction was in the contract; it took
a question about the NEXT packet to test it.** **Recorded as a process observation: a contract's stated
uncertainty is a test waiting to be written.**

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
