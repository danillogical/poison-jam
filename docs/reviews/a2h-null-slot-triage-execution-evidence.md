# `A2h-null-slot-triage-r1` execution evidence — **`O-NO-BOUNDARY-TRANSITION`**

> ## ✅ STAGE-1 ACCEPTANCE: **`ACCEPT`** (2026-09-28)
>
> All three criteria **`AGREED`**, **`BLOCKING: NONE`**. Review:
> `docs/reviews/a2h-null-slot-triage-acceptance-review.md`.
>
> **The reviewer independently reproduced** the install control (reading `game/default.xbe` directly: `.rdata`
> at VA `0x001C3F60`, `0x001C4064` → file offset `0x001B4104` = **`0x80000115`**; index 65; `KERNEL_VA_BASE +
> 65*4` = **`0xFE000104`**), the **per-thread prefix identity** (all 5555 records on the terminating thread,
> 0 differences), the **nesting mechanism** (from `jsrf_run.log:9130-9156`), the **counts**, the **complete
> series** (0 missing), **`§6.1.6` compliance** (version 2 in both runs' `stacks.txt`), **synthetic completion
> absent**, and the **frozen packet hash** (`F9A6522E…A9F20`, 80 lines, 32704 bytes). **It also ruled the
> three toolkit test-file stubs WITHIN SCOPE** — *"the stop clause is about where diagnostic LOGIC lives, not
> link closure."*
>
> **Six findings were raised; five were corrections to this record and one was already fixed. All are applied
> below**, and the reviewer's verdict is that **none changes the row**:
>
> | # | Finding | Status |
> |---|---|---|
> | 1 | after-samples **7747**, not 7743 | **applied** (verified: 7751 before + 7747 after = 15498) |
> | 2 | "595 nested adjacencies / depth 7" were **raw-order** counts including cross-thread interleaving; **per-thread it is 5 / 4** | **applied** (verified both measures; per-thread is correct) |
> | 3 | "identical guest prefix" needed **per-thread on the terminating thread** precision | **applied** |
> | 4 | `index_integrity(budget=None)` never evaluated the **cap** half of the `O-OPEN` clause | **fixed** — the classifier now reads the budget from metadata (`100000`) and the cap test is live |
> | 5 | the collector prints the **thread** registry's `claimed`, not the latch's, so `install_ok`/`latch.claimed` are not readable from the archive | **recorded as an advisory** (the latch verdict reaches the record via the terminal print's `observed=0`, which suffices for this row) |
> | 6 | commit the classifier addition | **done** |
>
> **The reviewer also discharged the packet's `O-OPEN` "index gap/duplicate within a thread" clause by direct
> measurement** — which this record had argued past rather than tested — and confirmed **`DOES NOT APPLY`**.
> **The Session has since added `index_integrity()` to the classifier (11 self-tests) so the clause is tested
> mechanically from now on, failing closed to `O-OPEN`.**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-null-slot-triage.md`, revision **`A2h-null-slot-triage-r1`**, frozen SHA-256
**`F9A6522E8579AD756701C150A0AF60275DCFF4158705CE5331BE3BF2EA7A9F20`** (80 lines) — verified before execution,
**not edited**. **Class:** discovery (§5.8).

**Selected row: `O-NO-BOUNDARY-TRANSITION`** — *"valid install and last nonzero samples, complete per-thread
boundary series, raw-zero terminal ICALL, and **no** first-zero boundary transition before it; no missing
sampled windows."*

**Two runs, exactly as the packet pre-specified, same build.** Run 1 (gates OFF) is the live inertness
control; Run 2 (gates ON) is authoritative. **No third run.**

---

## Artifacts

| | Run 1 — inertness control | Run 2 — authoritative |
|---|---|---|
| Label | `a2h-null-slot-inert-off` | `a2h-null-slot-authoritative-on` |
| Directory | `logs/runs/20260928-001502-101-…` | `logs/runs/20260928-001520-474-…` |
| Profile | **STRICT** | **STRICT** |
| Outcome | `unhandled_exception` | `unhandled_exception` |
| Registry version archived | **2** | **2** |

**Classifier:** `scripts/a2h-slot-triage-classify.py` (**8 fixture self-tests OK**) — the row is selected by
tool, not by an operator reading a log. **Counts are citable only as `(artifact, query, value)` triples** per
the Advisor's binding count discipline; the classifier reports the log's SHA-256 with every result.

## The install positive control **PASSED**, exactly as predicted

```
[A2HSLOT] install tid=44768 slot=001C4064 raw=80000115 installed=FE000104 index=65
```

| Quantity | Observed | Predicted | |
|---|---|---|---|
| slot | `0x001C4064` | the packet's pinned slot | ✓ |
| index | `65` | `(0x001C4064 − 0x001C3F60)/4` | ✓ |
| raw image value | **`0x80000115`** | the ordinal-277 marker `0x80000000 \| 277` | ✓ |
| installed value | **`0xFE000104`** | `KERNEL_VA_BASE + 65*4` | ✓ |

**An image value, a logged event and a source transformation agree.** This is the independent control the
packet required, and it is the reason the rest of the run is interpretable.

## Run 1 is a valid live inertness control

| Check | Result |
|---|---|
| `[A2HSLOT]` lines | **0** — the gate produced nothing |
| `[KWATCH]` lines | **0** |
| Non-gate env identical to Run 2 | `RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, `RECOMP_KERNEL_LOG_BUDGET=100000` — **all MATCH** |
| Guest prefix through the OOM | **identical PER THREAD on the terminating thread**: all **5555** dispatch records match by `(call#, ordinal, slot, esp, ret PC)` with **0 differences**, identical through `#5555` (`#5551` 277, `#5552` 294, `#5553` 277, `#5554` 184→`0xC0000017`, `#5555` 294) |
| OOM / ICALL / exception | **identical**: `598869040`, `invalid target 0x00000000`, `0xE0424943` |

> **A precision correction the acceptance reviewer required.** This record said *"the numbered guest prefix
> is identical"* without qualification. **That is not exactly right, and the reason matters:** both runs are
> **six-threaded**, so a **whole-log** diff diverges immediately — that is **cross-thread interleaving**, not a
> prefix difference. **The meaningful test is per-thread, on the terminating thread** (`Run 1` tid 65896,
> `Run 2` tid 44768), where all 5555 records match exactly. The reviewer performed that comparison
> independently and confirmed it. **The claim is true as stated above and would have been false as
> originally worded.**

**So the instrumentation is inert on the real guest** — which is exactly what a fixture cannot establish, and
the reason the Advisor required a live Run 1 rather than fixture-tested inertness.

## The decisive observation

**The slot read `0xFE000104` at EVERY one of 15498 sampled bridge boundaries, on all six threads.**

| Quantity (Run 2) | Value |
|---|---|
| `[A2HSLOT]` before-samples | **7751** (one per dispatch) |
| `[A2HSLOT]` after-samples | **7747** |
| distinct sampled values, all threads | **`0xFE000104` only** |
| samples reading zero | **0** |
| `[KWATCH]` change lines | **0** (the one KWATCH line is the initial *before* print) |
| latch first-zero transitions recorded | **0** |
| terminal read | **`0x00000000`** |

*(The after-sample count was first written here as 7743. The stage-1 acceptance reviewer recomputed it as
**7747** and the Session has verified that — the total 15498 was correct, so the before/after split was
simply mis-added. Corrected above.)*

**The last bridge boundary is `call=#5555 ordinal=294`, and it read `0xFE000104` — immediately before the
terminal read of `0`:**

```
[A2HSLOT] tid=44768 call=#5555 ordinal=294 slot=001C4064 value=FE000104 phase=after
[KERNEL] → returned 0x00000000
[ICALL] invalid target 0x00000000 tid=44768 esp=00F7FD00 return=0014982E
[A2HSLOT] terminal tid=44768 slot=001C4064 live=00000000 call=#5555 observed=0
[EXCEPTION] tid=44768 code=0xE0424943
```

**So the slot was correct at the final bridge boundary and zero at the raw read, with no bridge call in
between.** The change happened in a region the sampler does not bracket — which is precisely what this row
names.

## Completeness — measured the right way, and a Session error corrected

> ### ⚠ The Session's first completeness test was WRONG and would have failed the row for the wrong reason
>
> The first version paired `after` lines to `before` lines **by call number** and reported **5 gaps** on the
> terminating thread (calls 1, 1757, 1758, 1762, 1763) plus 595 nested `before→before` adjacencies. Under the
> packet's wording — `O-OPEN` is selected on *"index gap/duplicate within a thread"* — that would have forced
> `O-OPEN`.
>
> **It was a printing artifact of NESTED dispatch, not missing observations.** A bridge can re-enter the
> dispatcher, which **increments the per-thread counter**, so an `after` line prints the **current** counter —
> already advanced past its matching `before`. Re-pairing by *ordinal* instead of call number leaves exactly
> **one** unmatched ordinal per thread (the outermost call still on the stack at capture).
>
> **A measurement correction the acceptance reviewer caught:** this record first reported **595 nested
> adjacencies** and **maximum nesting depth 7**. **Those are raw-log-order counts and they include
> CROSS-THREAD interleaving, which is not nesting.** Measured **per thread** — the correct basis, since the
> counter is per-thread — the terminating thread shows **5 adjacencies and depth 4**, and **every other
> thread shows 0 and depth 1**:
>
> | tid | samples | adjacencies | max depth |
> |---|---|---|---|
> | 44768 | 11109 | **5** | **4** |
> | 48528 | 517 | 0 | 1 |
> | 60860 | 4 | 0 | 1 |
> | 65292 | 656 | 0 | 1 |
> | 65888 | 2769 | 0 | 1 |
> | 65956 | 443 | 0 | 1 |
>
> **The mechanism is real and the conclusion is unchanged** — the reviewer independently confirmed the
> nesting from the log (`jsrf_run.log:9130-9156`: three `before` lines, then three `after` lines all stamped
> `#1759` with ordinals 225→232→219). **Only the corroborating numbers were inflated, by measuring the
> interleaved log instead of each thread.**
>
> **The correct test is: does every `[KERNEL]` dispatch have a `before`-sample on that thread?** The
> `before` series **is** the boundary series, because a `before` is emitted at dispatch entry before any
> nested call can occur.
>
> | tid | dispatches | before-samples | missing |
> |---|---|---|---|
> | 44768 | 5555 | **5555** | 0 |
> | 48528 | 259 | **259** | 0 |
> | 60860 | 2 | **2** | 0 |
> | 65292 | 328 | **328** | 0 |
> | 65888 | 1385 | **1385** | 0 |
> | 65956 | 222 | **222** | 0 |
>
> **No unsampled thread. No dispatch without a sample. The boundary series is COMPLETE.** The classifier now
> measures completeness this way and **fails closed to `O-OPEN`** if a thread is unsampled or a dispatch lacks
> a sample — both pinned by fixtures.

## Row selection

| Row | Applicable? | Why |
|---|---|---|
| `O-IDENTITY` | **No** | XBE/toolkit/exe pins verified; both runs STRICT; the install control passed; no malformed records |
| `O-OPEN` | **No** | Run 1 is a valid inertness control, Run 2 has the terminal zero, the runs differ **only** in gate states, the install control passed, and the series is **complete** |
| `O-BRIDGE` | **No** | Requires a first `nonzero→0` **inside** a bridge. **No sample ever read zero**; the latch recorded **no** transition |
| `O-GUEST` | **No** | Requires an observed inter-bridge `previous-after != 0 → next-before == 0`. **No sample read zero**, so there is no such boundary pair |
| **`O-NO-BOUNDARY-TRANSITION`** | **YES** | Valid install and last-nonzero samples, **complete** per-thread series, raw-zero terminal ICALL, and **no** first-zero boundary transition before it |

## What this establishes, and what it does NOT

**Establishes:** the install control is exact; the instrumentation is live-inert; the slot held
`0xFE000104` at **every** one of 15498 sampled bridge boundaries across **six** threads; **no** sample and
**no** latch record ever observed zero; the final boundary before the fault read `0xFE000104`; and the
terminal raw read was `0`.

**Does NOT establish — and the row's own wording forbids claiming:** that the slot was **never** zero. **A
transient zero and recovery between the last sample and the raw read is precisely what the successor packet
tests.** Also not established: any writer identity, any guest RIP, whether a bridge or guest code was
responsible, that the OOM and the NULL slot are related, a repair, or any strict criterion. **No synthetic
completion** — the trap and `0x80` were not suppressed, the allocation was not faked, the arena was not
widened, the NULL call was not bypassed, and no guest error-handling change was made or proposed.
**`PIO_FREE` remains DEFERRED**; `A4b2-r7`, accepted/closed `A4b2-r8` and `A4b1-r4` were **not** reopened;
`0xFFFFB3` stays **`UNRESOLVED`**.

**Successor named by the row:** `A2h-slot-read-path-displacement` — test transient zero/recovery, a torn or
displaced read, or a guest write between the last sample and the raw read.

---

## What the slot actually IS — the guest was entering a critical section

**Characterised from the toolkit's ordinal table, which the Session had not consulted before:**

| Quantity | Value |
|---|---|
| Slot `0x001C4064` | index **65** of the 120-entry table at `0x001C3F60` |
| Image content | **`0x80000115`** = ordinal **277** |
| **Ordinal 277 is** | **`RtlEnterCriticalSection`** (`kernel_bridge.c:8137` arg size, `:8443` bridge) |
| Bridged? | **YES** — no `unbridged function thunk` warning anywhere in either run |

**And the call site reads the lock pointer from the object the callee was given:**

```
00149822  push  dword ptr [esi + 0x580]   ; the CRITICAL_SECTION pointer
00149828  call  dword ptr [0x1c4064]      ; RtlEnterCriticalSection, via the thunk slot
```

**So the terminal event is: the guest attempted to enter a critical section, and the thunk slot it
dispatches through read `0`,** sending the call to address `0`. That is a **more concrete characterisation of the
failure than "a NULL indirect call"** — it names the kernel service the guest was invoking and the object
pointer it was passing.

**Recorded as a characterisation, NOT as a cause.** It does **not** explain what zeroed the slot, and it does
**not** establish that the critical section or its owner is implicated. **The successor packet owns that
question**, and this detail is recorded so it does not have to rediscover what the slot means.

**One observation worth carrying to the successor, offered as a lead and not a conclusion:** the zeroing loop
the Session analysed earlier takes its **length from `arg2`** and its **destination from `edi`**, and the
thunk table contains this slot. **The failing activation provably did NOT run that loop** (it took the
allocation path, which the loop's guard jumps over). But the function is called **many** times, and the
successor's write instrument is the first thing that could observe whether *another* activation ever reached
it with a destination in the table. **No claim is made that it did.**

---

## Offline work on the successor's hypotheses — three refuted, one lead recorded

Bounded, offline, and **not** a substitute for the successor packet. Recorded because each item either
narrows the successor or removes a hypothesis it would otherwise have to test.

### Refuted offline

| Hypothesis | Basis for refutation |
|---|---|
| **A macro mismatch** between the toolkit's sampler and the game's terminal read | **Identical in effect.** `BRIDGE_MEM32(a) = *(u32*)((uintptr_t)(a) + g_xbox_mem_offset)`; `MEM32(a) = *(u32*)XBOX_PTR(a)` where `XBOX_PTR(a) = (uintptr_t)(uint32_t)(a) + g_xbox_mem_offset`. **Both read the same location for the same VA** |
| **A different read ADDRESS** | **Refuted from source.** The sampler loads `BRIDGE_MEM32(0x001C4064)`; the generated code loads `MEM32(0x1C4064)` at `recomp_0003.c:20155`. **Same literal address.** |
| **A different read WIDTH** | **Refuted from source.** `BRIDGE_MEM32` is `*(volatile uint32_t *)`; `MEM32` is `*(volatile uint32_t *)`. **Both 32-bit.** |
| **A torn / partial read** | The slot is **4-byte aligned** (`0x001C4064 % 4 == 0`) and the access is a naturally-aligned `uint32`, which **cannot tear on x86** |
| **A static VA displacement** | The install sample read **`0x80000115`** at that VA through the *toolkit's* macro, so the VA resolved to the image's `.rdata` correctly. **`g_xbox_mem_offset` is written only at init** (`src/main.c:225`, `xbox_memory_layout.c:1768`) and by nothing afterwards in either repository |

**The terminal site, verified at a genuine instruction boundary:**

```
00149822  push  dword ptr [esi + 0x580]   ; the CRITICAL_SECTION pointer
00149828  call  dword ptr [0x1c4064]      ; RtlEnterCriticalSection, via the thunk slot
0014982E  mov   byte ptr [ebp - 0x1d], 1
```

**So exactly TWO candidates survive**, and they are precisely what the corrected successor packet
instruments:

1. **the memory genuinely changed to `0`** in the bridgeless gap — a guest or host write; and
2. **`g_xbox_mem_offset` differed at the terminal moment**, making the two reads resolve to different
   physical locations.

**The Session's offline grep is evidence against (2) but is NOT sufficient for a positive read-path
conclusion** — the Advisor ruled exactly that, and requires a **live re-read of the offset at terminal**.
**That requirement stands and belongs to the successor.**

**Also confirmed:** the terminal value was read **twice independently** — once by the generated
`MEM32(0x1C4064)` at `recomp_0003.c:20155`, and once by the terminal hook's own `MEM32(0x001C4064)` — and
**both produced `0`**.

### What remains, and why the successor's instrument is the right one

**Guest execution between the last bridge return and the faulting ICALL is invisible to this artifact.**
The only sampler brackets **bridge calls**; the kernel log records **bridge** calls; and the gap contains
exactly two lines (a bridge return and the ICALL). **So whether guest code wrote the slot to `0` in that gap
is not decidable here** — which is precisely why the row's successor is a **page-guard write history**: it is
the only instrument that observes guest writes to this page.

**This coverage limit is measured, not assumed.** The gap is lines `34632..34635`, and the run's *entire*
instrumentation census is known (`KERNEL` 15960, `A2HSLOT` 15500, `TRACE` 1268, `ESP` 1221, `RECOVERED` 345,
`READ` 14, …). **No `[TRACE]`, `[RECOVERED]` or `[READ]` line falls inside the gap** — the nearest are **85**,
**251** and **501** lines earlier respectively. **So no instrument in this run observed the guest code that
executed there**, and the artifact **cannot name it**.

### A second hypothesis refuted: the toolkit's documented static-data-corruption class

`xbox_memory_layout.c:2602-2615` documents a real failure mode — *"the worker spawned during engine init
wrote its frames over the game's own static data. Nothing faults … so it shows up later as globals that were
correct when written and wrong when read."* **That is the right shape for this symptom**, so it was checked
rather than assumed:

| Region | Extent |
|---|---|
| JSRF image (`.text` → `$$XSIMAGE` end) | `0x00011000` … **`~0x00288620`** |
| Guest thread stacks (all five, from `stacks.txt`) | **`0x00780000`** … `0x012ED000` |
| Bridge `esp` values observed (349 distinct) | `0x007BFF94` … `0x012ECF90` — **0 inside `.rdata`** |

**The image ends at ~`0x00288620` and the lowest stack base is `0x00780000` — no overlap.** So this title does
**not** exhibit that failure class, and the mechanism behind it is **refuted for this run**. *(Recorded because
it is a plausible-looking explanation that the successor would otherwise have to re-test.)*

### A lead that may connect the PARKED producer line to the current one

**Recorded as a lead, NOT as a conclusion, and it does not reactivate the producer line.**

`sub_001497DC` contains a bulk-zeroing loop **whose length is `arg2`** — the very slot whose ~571 MB value was
the original A2h finding:

```
00149DE2  test  byte ptr [ebp+0xc], 8
00149DE6  je    0x149f31          ; bit 8 clear -> skip the zeroing entirely
00149DEC  mov   ecx, dword ptr [ebp+0x10]   ; LENGTH = arg2
00149DF6  rep   stosd dword ptr es:[edi], eax
00149DFD  rep   stosb byte ptr es:[edi], al
00149DFF  jmp   0x149f31          ; and SKIP the ordinal-184 call
```

**The thunk table spans `0x001C3F60..0x001C4140` (120 entries), and the slot `0x001C4064` is inside it** —
so a zeroing loop that reached that range would zero the slot.

**But three facts cut against this being the mechanism, and they are why it stays a lead:**

1. **The failing activation demonstrably TOOK the allocation path**, not the zeroing path: the log records
   `#5554 ordinal 184 … ret=0x00149E50`, and `0x00149E4A` is the allocation call — which the zeroing branch
   **jumps over** (`jmp 0x149f31`).
2. **The OOM error path cannot reach the zeroing loop:** it starts at `0x00149E04` (after the loop) and
   `jmp 0x149eec`; a byte-scan of its whole span found **0 backward branches** into `0x00149DEC..0x00149DFF`.
3. **`edi` on that path is `[ebp-0x4C] + 0x10`**, a stack-derived value, not a `.rdata` constant — and the
   callee contains **0** constant-`.rdata` stores.

**So the loop exists, its length is arg2, and it is not on the path this run took.** Whether some *other*
activation reached it with a large count, and whether its `edi` could ever land in the thunk table, is a
**separate question the successor's write history would answer** — and it is recorded here so the successor
does not have to rediscover it.

**The producer line stays PARKED.** This lead does **not** satisfy either reactivation condition: the slot
death is not shown to be downstream of allocation-failure handling, and no later gate yet needs the size
explained.
