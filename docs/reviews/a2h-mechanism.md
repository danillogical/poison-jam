# `A2h` mechanism — the heap-OOM is a **guest allocation failure with no NULL guard**, and it **predates** the A4b2 trap

> ## ⚠ DATED CORRECTION (2026-09-27, appended — the title and the text below are PRESERVED as written)
>
> **This document's title, its Defect B, and its mechanism steps 3–4 assert a mechanism that measurement has
> since FALSIFIED.** They are preserved rather than rewritten, so that the record shows what was believed and
> when it changed. **Read the correction before relying on any of it.**
>
> **What is FALSE: "the guest does not handle the failure" / "no NULL guard" / "Defect B is the crash."**
>
> Verified bytes immediately after the ordinal-184 call:
>
> ```
> 00149E4A  call  dword ptr [0x1c3f88]   ; NtAllocateVirtualMemory
> 00149E50  mov   dword ptr [ebp-0x12c], eax
> 00149E56  test  eax, eax
> 00149E58  jl    0x149eec               ; <-- A SIGNED CHECK ON THE RESULT
> ```
>
> **`0xC0000017` is negative as a signed 32-bit value, so `jl` IS taken**, and the error path carries
> `STATUS_NO_MEMORY` and returns cleanly through `__SEH_epilog` (`00149EF2 mov [ebp-0x188],0xc0000017` …
> `00149F40 call 0x17d231` … `00149F45 ret 0xc`). **The OOM is HANDLED. It is not the crash.**
>
> **The circularity, named:** this document cited the bare sequence (`0xC0000017` → NULL ICALL →
> `0xE0424943`) as *evidence* for "no check", and "no check" then explained the sequence. **Temporal
> co-occurrence is not attribution.** The originating packet had itself warned this inference *"must itself
> be tested, not assumed"* (`a2h-oom-causal-slice.md:16`).
>
> **What actually terminates the run:** the `call [0x1c4064]` at `0x00149828` read **`0`**;
> `RECOMP_ICALL_IS_CODE` rejected it as non-code and `recomp_icall_not_code_log` raised `0xE0424943`
> **NONCONTINUABLE**. **The original XBE holds `0x80000115` there — a valid ordinal-277 kernel thunk.**
> The NULL call is at the **top** of the function; the ordinal-184 call is near the **end** — **different
> passes.**
>
> **What STANDS:** **Defect A** (the 571 MB `MEM_COMMIT` with `BaseAddress = NULL` is not a legitimate
> reservation) — **handling-independent, unaffected by this correction.** Also standing: the failure
> **predates** the A4b2 trap work, and is **not trace-caused**.
>
> **What needs REFRAMING:** the baseline-variance paragraph at L94 — the variance between the OOM run and
> `a2b-nr-baseline` is in the **NULL event**, not in OOM handling.
>
> **Authority:** Advisor `muse_FkNhGaXtV9P5` (`muse-spark-1.3-contributor`, `max`), Q4 Obligation 2. Full
> ruling: `docs/reviews/a2h-critical-path-advisor-ruling.md`. Critical path is now *what zeroed / what read
> as zero at `0x001C4064`*. work

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Why this record exists:** the Advisor named `A2h` the critical path — *"the trapped crash at ≈4.77 s blocks
all trapped observation past the prefix."* The Session characterised it **from archived runs and the
original XBE only — no new run, no build, no toolkit change** — and the result **changes what `A2h` is**.

---

## 1. The request is a byte-identical constant across **35 archived runs**

| Quantity | Value |
|---|---|
| Requested | **598869040** = `0x23B20430` ≈ **571.1 MB** |
| Used | **12715008** |
| Arena | **50855936** (48.5 MB) |

**Not "similar" — identical to the byte, in all 35 runs** that show it. A constant of 571 MB against a
48.5 MB arena is not gradual memory pressure; it is a size that was **never plausible**, i.e. a size
computed from a bad input.

## 2. It is **not** caused by the trap — corrected: it **predates** the A4b2 trap work

> ### ⚠ CORRECTION — this section previously claimed A2g had NO trap. That was WRONG.
>
> **The acceptance reviewer caught it.** I read `run_profile.effective_settings`, which is **empty** for the
> A2g run, and reported `RECOMP_APU_TRAP` as **"ABSENT"**. The value actually lives in `metadata.json`'s
> top-level **`settings`** dict:
>
> ```
> settings.RECOMP_APU_TRAP    = 1
> settings.RECOMP_APU_DSP_ACK = 0x803C0810
> ```
>
> **and the A2g log's own line 26 says `APU: 0xFE800000..0xFE880000 trapped for MMIO`.**
>
> **So A2g IS trapped.** Re-censused across every archived run carrying the `598869040` request, reading the
> trap state from **all** plausible locations: **35 runs carry the request, all 35 are trapped, and ZERO are
> untrapped.** **The error class is the one this project keeps producing — reading an absent record as a
> negative measurement.**

**The earliest OOM run still matters, for a narrower and defensible reason:**

| Run | Trap | Exe | Date |
|---|---|---|---|
| **`20260922-224429-003-a2g-304f0-span`** | **`1` (trapped)** | `2cd0472a256e9dad` | **five days before the a4b2 work** |

**So the finding is: the OOM predates the A4b2 work.** It appears on a **different exe five days earlier**
with the same size, same type, same OOM tuple and same terminal ICALL — **so it was not introduced by
whatever A4b2 changed.** **The trap-era runs reach the same pre-existing failure sooner.**

**What is NOT established:** that the trap is *necessary* for the failure. **All 35 archived runs with this
request are trapped**, so the archive cannot separate trap-necessity either way. **The strong claim is
withdrawn; the build-independence claim stands.**

## 3. The full chain, from the A2g run (an earlier build, five days before the a4b2 work)

```
[KERNEL] #5519: ordinal 184 (slot 10) esp=0x00F7FCF0 ret=0x00149E50
[KERNEL] NtAllocateVirtualMemory: base=0x00000000 size=598869040 type=0x801000 prot=0x4
xbox_HeapAlloc: out of memory (requested 598869040, used 12715008/50855936)
[HEAP] ... (the arena's live blocks)
[KERNEL] → returned 0xC0000017                     ; STATUS_NO_MEMORY
[KERNEL] #5520: ordinal 294 (slot 64) esp=0x00F7FCFC ret=0x00149F5D
[ICALL] invalid target 0x00000000 tid=2948 esp=00F7FD00 return=0014982E
[EXCEPTION] code=0xE0424943
```

**Read the last two lines carefully — this is the actual crash:**

1. The guest calls **`NtAllocateVirtualMemory` (ordinal 184)** from guest VA **`0x00149E50`**.
2. The allocator correctly returns **`0xC0000017` (STATUS_NO_MEMORY)**.
3. **The guest does not handle the failure.** It proceeds and calls **through a NULL pointer**
   (`ICALL invalid target 0x00000000`, returning to `0x0014982E`).
4. **That** raises `0xE0424943` — the project's fatal invalid-call path.

**So `A2h` is two distinct defects stacked:**

| # | Defect | Evidence |
|---|---|---|
| **A** | A guest `MEM_COMMIT` of **571 MB** with `BaseAddress = NULL` — **not** a legitimate reservation (see §4), and ~285× the size the same site normally passes | the two-size table; the `alloc_type` decode |
| **B** | **No NULL check on the allocation result**, so a recoverable `STATUS_NO_MEMORY` becomes a fatal crash | `0xC0000017` → NULL ICALL → `0xE0424943` |

**On Defect A's characterisation — stated with the care it needs.** The size is **implausible for a
commit** and the same site passes a small size normally. That makes **"the size is mis-derived"** the
leading hypothesis, **but it is a hypothesis, not a measurement**: the producer of `[ebp-0x24]` has **not**
been traced, and the Planner is right that *"whether the size comes from a bad input or a legitimate large
virtual-region request"* is not decided by the archived sequence alone. **§4 rules out the pure-reservation
defence for this invocation specifically; it does not identify what produced `0x23B20410`.** That trace is
exactly what the `A2h-oom-causal-slice` packet exists to perform.

**Defect B is the crash.** `0xC0000017` is a *return value*, not an exception — a guest that checked it
would simply report failure. **The run log even shows one archived run that did:**
`20260927-130036-879-a4b2-nr-baseline` *"returned NO_MEMORY but did NOT take the NULL call."* **So the
failure is survivable and the guest's own code path decides whether it is fatal** — which is why the same
request is fatal in 34 runs and not in that one.

## 4. The size is computed at the call site, not passed in

The caller at `0x00149E50` (XBE disassembly):

```
00149E1D  and   dword ptr [ebp - 0x130], 0     ; BaseAddress = NULL
00149E24  add   dword ptr [ebp - 0x24], 0x20   ; RegionSize += 0x20   <-- the size
00149E28  push  4                              ; Protect = PAGE_READWRITE
00149E2A  mov   eax, dword ptr [ebp + 0xc]     ; from the caller's arg
00149E2D  shl   eax, 0x14
00149E30  not   eax
00149E32  and   eax, 0x800000
00149E37  or    eax, 0x1000
00149E3C  push  eax                            ; AllocationType
00149E3D  lea   eax, [ebp - 0x24]
00149E40  push  eax                            ; &RegionSize
00149E41  push  0
00149E43  lea   eax, [ebp - 0x130]
00149E49  push  eax
00149E4A  call  dword ptr [0x1c3f88]           ; NtAllocateVirtualMemory
```

**So `RegionSize = [ebp-0x24] + 0x20`, where `[ebp-0x24] = 598869008 = 0x23B20410`.** The size is a
**stack local**, computed by the function's own logic before the call — so tracing `[ebp-0x24]`'s definition
backward is a **bounded, demand-driven** question, exactly the shape that has worked on this project.

### The same call site passes **two and only two** sizes

`alloc_type = 0x801000` appears **70 times** in the archive, with **exactly two distinct sizes**:

| | Size | Pre-add local (`size − 0x20`) |
|---|---|---|
| **normal** | `2097200` = `0x00200030` | **`2097168` = `0x00200010`** |
| **failing** | `598869040` = `0x23B20430` | **`598869008` = `0x23B20410`** |

**Both have the same low 16 bits (`0x0430` / `0x0410` respectively) and differ by `0x23920400`.**

> **A Session error, corrected before it propagated.** My first pass claimed the normal pre-add local was
> **exactly `0x200000` (2 MB, a clean power of two)** and that this showed the same path normally computing
> a tidy 2 MB. **That was wrong** — I subtracted `0x20` from the *rounded* figure and then compared against a
> value I had assumed. The normal pre-add local is **`0x00200010`**, which is **not** page-aligned and **not**
> a power of two. **The "exactly 2 MB" reading is withdrawn.** What survives is the weaker but still useful
> fact: **the site passes one small size normally (`0x00200030`) and the implausible `0x23B20430` when it
> fails**, and the failing value is **~285× larger**.

### And the failing size is **not** a legitimate large *reservation*

The Planner raised a load-bearing nuance from `src/kernel/kernel_bridge.c:833-899`: the toolkit
**distinguishes** a large pure `MEM_RESERVE` from RAM backing, and on real hardware a reservation costs
address space rather than pages — so *"a large virtual request is not automatically invalid."* **That is
correct about the toolkit, and the Session verified it by reading the source.** But it **does not apply to
this invocation**, and the decode is decisive:

| Bit | Value | Present in `0x801000`? |
|---|---|---|
| `MEM_COMMIT` | `0x1000` | **YES** |
| `MEM_RESERVE` | `0x2000` | **NO** |

The toolkit's reserve branches (`:857` "grant above RAM" and `:876` "clamp") both require
**`(alloc_type & 0x2000) && !(alloc_type & 0x1000)`** — which is **FALSE** for `0x801000`.
**So neither reserve branch can run for the failing call**, and the archived logs confirm it: **zero**
"reserve of N granted" messages and **zero** "clamped" messages across the whole archive.

**So the failing call is a `MEM_COMMIT` with `BaseAddress = NULL` for 571 MB.** Committing means backing
pages, so on a 64 MB console a 571 MB commit **cannot succeed on hardware either.** **The
"legitimate large reservation" defence is therefore not available for this specific invocation** — though
it remains a correct general caution, and the archive shows `0x2000` reserves at `1048576`/`2097152` being
handled by the normal heap path.

**This narrows the `O-SEMANTICS` row rather than deleting it:** the row should not be read as *"this
invocation may be legitimate"*, because for `0x801000` it demonstrably is not. Whether *some other* large
virtual request in this title is legitimate is a separate question the packet may still ask.

## 5. What this means for the `A2h` packet

**It narrows `A2h` substantially and usefully:**

- **It is not an "A2h heap bug" in the toolkit.** The arena behaves correctly — it refuses an impossible
  request and returns the documented `STATUS_NO_MEMORY`. **No arena change is warranted**, and widening the
  arena would be **synthetic completion**: it would not make a 571 MB request correct.
- **The crash is a guest error-handling gap (Defect B).** The invalid call is on the guest's own path after
  a correctly-reported failure.
- **The implausible size (Defect A) is a bounded backward question** — `[ebp-0x24]` at `0x00149E24` — and it
  is the thing that actually needs explaining, because a *correct* size would not fail at all.
- **The trap is NOT established as a red herring.** The correlation is real — a4b2-era **trapped** runs die
  at 4.77 s while **untrapped** ones reach the pending-word hang instead — but **all 35 archived runs carrying
  this request are trapped**, so this archive **cannot separate trap-necessity either way**. **The withdrawn
  strong claim is replaced by the narrow one:** the failure **predates** the A4b2 trap work, and is **not
  trace-caused**.
  **So "fix A2h to extend trapped observation" may be the wrong frame**: the honest frame is *"the trapped
  path reaches a pre-existing guest defect at 4.77 s."*

**A caution against over-reading:** `ordinal 184` and the `[0x1c3f88]` / `[0x1c4064]` call targets are
**import thunks**; identifying them as `NtAllocateVirtualMemory` rests on the log's own naming, which the
runner emits. The `0xC0000017` return and the subsequent NULL ICALL are direct log observations.

## 6. What this does not establish

**No fix is proposed and no behaviour is changed.** No toolkit or runtime edit; no arena change; no
suppression of the trap; **nothing enabled at closure**. **The size's origin is not yet traced** — that is
the open question this record hands to the packet. **No strict criterion is satisfied**, no boot/audio/
liveness is claimed, and **no synthetic completion** is introduced. `PIO_FREE` stays **deferred**;
`A4b2-r7`, accepted/closed `A4b2-r8`, and `A4b1-r4` were **not** reopened.
