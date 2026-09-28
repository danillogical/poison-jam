# `A2h-live-slot-write-r1` — implementation record: **five findings, TWO of which correct the Session**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-live-slot-write-r1`, frozen
**`8F3C6291C42ED3D9C3B96D879A9BB9D46391E5FEEA3F4BDC1E8712F46FFF7D4F`**.
**Worker:** `b5e01c5e-4278-45ba-bd78-62bcb62854dc`, commits `3e6724c` (game) / `09db685` (toolkit).
**Record:** `docs/reviews/a2h-live-slot-write-implementation-record.md`.

**BUILD succeeded · CTEST 22/22 · HARNESS 19 probes + 9 delivery fixtures + native-`#DB` control · GUARDS 9/9.**
**Both trees clean. Nothing pushed.**

---

## ⚠ CORRECTION 1 — the Session's AC97 fact was HALF RIGHT, and the half it got wrong mattered

**The Session wrote** (`a2h-ac97-interlock-location.md`, `ef80d80`): *"The AC97 path does NOT use `EFlags.TF`.
It is a write-and-clear-and-retry on a reset bit. I searched `src/` for `STATUS_SINGLE_STEP`/`SINGLE_STEP` and
found ZERO hits."*

**The Worker's correction:** *"the AC'97 handler in `xbox_memory_layout.c:370` **DOES single-step; it is
spelled `EFlags |= 0x100u` / `&= ~0x100u`, not with the constant.**"*

**The Session verified it:**

```
src/kernel/xbox_memory_layout.c:401   s_a2h_slotw_saved_tf = (uint32_t)eflags & 0x100u;
src/kernel/xbox_memory_layout.c:419   return (DWORD)(eflags | 0x100u);
src/kernel/xbox_memory_layout.c:421   return (DWORD)((eflags & ~0x100u) | s_a2h_slotw_saved_tf);
src/kernel/xbox_memory_layout.c:513   ep->ContextRecord->EFlags |= 0x100u;   /* TF: step the write */
```

> **The Session searched for a NAMED CONSTANT and concluded the mechanism was absent.** **It is present, written
> as the raw bit `0x100u`.** **That is a search-for-the-name-instead-of-the-thing error — the same family as
> searching one address spelling, which this project's own `AGENTS.md` warns about.**

**And the consequence was concrete, not cosmetic:** **both handlers register at priority 1, AC'97 arms first,
so its `EXCEPTION_CONTINUE_EXECUTION` would have resumed the thread with the slot owner still pending —
page left open, post-value never read.** **The Worker FIXED it** (`:498-500`):

```c
if (s_a2h_slotw_pending & A2H_SLOTW_PEND_OWN)
    return EXCEPTION_CONTINUE_SEARCH;   /* release the #DB down the chain */
return EXCEPTION_CONTINUE_EXECUTION;
```

> **So `AC97-TF=0x2` is NECESSARY, not vestigial, and the Session's "conflict I could not locate" was a real
> conflict the Session missed by searching for a name.**

**The Session had ALSO written:** *"the synthetic-overlap fixture is what SETTLES whether the bit is
necessary."* **It did settle it — against the Session.** **The fixture caught the collision under BOTH VEH
orders.** ✓

## ⚠ CORRECTION 2 — `MEM32(0x19DCE0)` is NOT a stable pointer, and it explains an earlier misattribution

**The Worker:** *"`MEM32(0x19DCE0)` IS NOT A STABLE POINTER. It is 0 in every probe run, and on the archived
real run `20260928-052216-631` the **terminal** read returned `0x30766A64` — ASCII "djv0", string data.
**`0x19DCE0` and the slot `0x19D62C` are on the SAME PAGE `0x0019D000`**, so the ARM global is itself inside
the watched page and is overwritten by the same data traffic the watch exists to sort from the slot."*

**The Session verified the page identity:**

| Object | Address | Page |
|---|---|---|
| **the base global** | **`0x0019DCE0`** | **`0x0019D000`** |
| **the slot** | **`0x0019D62C`** | **`0x0019D000`** |
| **SAME PAGE** | | **TRUE** |

> ## **⚠ AND THIS RETROACTIVELY EXPLAINS THE SESSION'S OWN EARLIER ERROR.**

**Much earlier, the Session read `MEM32(0x19DCE0)` as `0x30766A64` = ASCII `"djv0"`, called it a
MISMATCHED-DUMP ARTIFACT, and used that conclusion to discard the reading.** **The `djv0` value is real — the
global is genuinely overwritten by string data on that page.**

**So the Session's *"mismatched dump"* diagnosis was WRONG for the RIGHT reason to be suspicious:** **the value
was anomalous, the Session correctly refused to interpret it, and it attributed the anomaly to the dump
rather than to the page sharing.** **The Worker found the actual cause.**

**The consequence for the trial is designed for:** **ARM DEFERS until the pointer is plausible, and the
terminal row carries `term_base_ok` to separate *"device moved"* from *"global was overwritten."*** **The
Worker's instruction to the Session is explicit:** *"Expect `stable=0` on the terminal row — read it as a
re-scope, not a failed comparison."*

**That is the packet's line-12 carry-forward gate firing on real data, and it is why the packet refused to
preselect `0x0019D62C`.** ✓

## The other three findings

**3. TWO COUNTERS RECORDED *"WHICH HANDLER RAN FIRST"*, NOT *"WHAT WAS SERVICED".*** **Both VEH orders now
converge — asserted, not assumed.** ✓

**4. A PACKET CLAIM IS MEASURED FALSE.** *"On this host a store through **mirror view 1 does NOT become
visible in the canonical view**."* **The watch reads the post-value through the SAME alias the store used, so
the ledger is correct either way — the packet's refusal to rely on the alias for value readback was right.**
**The fixture now RECORDS this instead of asserting the aliasing.** ✓

**5. A GUARD THAT COULD NOT FAIL.** *"The collector's `_Static_assert` pins its own mirror, so adding a field
to the real struct left it passing. The sufficient check is the runtime `size` comparison + version
equality. **Ledger version 1 → 2.**"* ✓ **A can-fail check replaced with one that can fail — exactly the
discipline the owner asked for long ago.**

## And two self-catches worth recording

**The Worker caught ITSELF reusing the collector's DR-gated counters** (`dr_create_thread_events` /
`dr_exit_events`) — *"that would have read 0 on every run this packet may make."* **It added independent
census counters.** ✓ **That is the DR-exclusion rule applied to its own code.**

**And it REMOVED a `pre == 0` condition it had put on the installer control:** *"that was an assumption about
the title, and it would have fired INFRA FAILURE on a run where the control HAD been observed."* ✓
**A control that could have produced a false INFRA FAILURE, caught before the trial.**

## The installer trap is now byte-classified, which is a genuine strengthening

**The Worker:** *"the control is classified from the **bytes at the recorded RIP**, because a native RIP is not
a guest VA."* **And the fixture reads the REAL XBE:**

| Address | Bytes | Encoding |
|---|---|---|
| **`0x0018CE3A`** (the installer / control) | **`89 81 2C 24 00 00`** | **MODRM** |
| **`0x00199F45`** (the candidate) | **`89 84 AE EC 03 00 00`** | **SIB** |

> **Distinct encodings, so the control and the candidate CANNOT be confused.** ✓ **That is a stronger control
> than a RIP comparison, and it uses the line's own encoding finding.**

## The Session's next step

**Per the Worker's note:** *"the runner refuses a strict launch without `RECOMP_GPU_ACK=0` … set it
explicitly when you run."*

**And the implementation needs a FRESH OFF before any ON** (the carry rule — new code). **The Worker verified
inertness on a real OFF launch:** **0 `[A2HSLOTW]` lines, and `GUEST_SLOTW unarmed
reason=never_initialised`** — **an all-zero ledger is reported as UNARMED, not as a layout mismatch.** ✓

**So the trial sequence is: fresh OFF → up to 5 ON, stop at the first qualifying run.**

## Prohibitions and status

**Observation only.** **Page-protection only — no DR0/DR6/DR7 anywhere in the facility.** **No DR record
cited.** **No synthetic completion.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged.
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened.** **All nine guards pass.**
