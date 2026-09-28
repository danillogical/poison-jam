# `A2h` mechanism — the heap-OOM is a **guest allocation failure with no NULL guard**, and it is **not** trap-caused

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

## 2. It is **not** caused by the trap

**The earliest OOM run predates the trap work entirely:**

| Run | Trap | Trace | Exe |
|---|---|---|---|
| **`20260922-224429-003-a2g-304f0-span`** | **ABSENT** | **ABSENT** | `2cd0472a256e9dad` |

It is an **a2g** run, days before the a4b2 trap work and before `A2h` was named. **So the trap does not
introduce this failure.** The trap makes the path **reachable sooner** in a4b2-era runs; the defect itself is
older and trap-independent.

**This corrects the working hypothesis** in `docs/reviews/pio-free-strict-horizon.md`, which said the OOM
*tracks* the recent-build + trap combination and offered a routing hypothesis. **The correlation is real but
the causation is not the trap**: the trap simply reaches a path that was already broken.

## 3. The full chain, from the no-trap run

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
| **A** | A guest request for **571 MB** — implausible, so the size is computed from bad input | the constant itself |
| **B** | **No NULL check on the allocation result**, so a recoverable `STATUS_NO_MEMORY` becomes a fatal crash | `0xC0000017` → NULL ICALL → `0xE0424943` |

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

## 5. What this means for the `A2h` packet

**It narrows `A2h` substantially and usefully:**

- **It is not an "A2h heap bug" in the toolkit.** The arena behaves correctly — it refuses an impossible
  request and returns the documented `STATUS_NO_MEMORY`. **No arena change is warranted**, and widening the
  arena would be **synthetic completion**: it would not make a 571 MB request correct.
- **The crash is a guest error-handling gap (Defect B).** The invalid call is on the guest's own path after
  a correctly-reported failure.
- **The implausible size (Defect A) is a bounded backward question** — `[ebp-0x24]` at `0x00149E24` — and it
  is the thing that actually needs explaining, because a *correct* size would not fail at all.
- **The trap is a red herring for causation.** It is *how the path becomes reachable sooner* in a4b2-era
  runs, which is why trapped runs die at 4.77 s and untrapped ones reach the pending-word hang instead.
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
