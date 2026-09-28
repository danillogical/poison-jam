# `A2h` — the callback CONTEXT PRODUCER: **FOUND** (static + XBE bytes + one gated dump read)

**Executor:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Contract:** `docs/packets/a2h-callback-context-identity.md` (r2).
**Starting point:** `docs/reviews/a2h-callback-context-identity-identified.md` (`0889ae9`) — the identity
and the `context → device` invariant were **not** re-derived.
**Identity re-pinned at execution:** game `d36d64b915f291de02b134f1c2dd49e7bb5c7fda` (branch `master`),
XBE sha256 `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C`.

**Row:** **`O-ALTERNATE-PATH`** — producer **VERIFIED**, field layout **BINDS all five sites**.

---

## 1. THE ANSWER

> **The context object is `device + 0x2268`, where `device = MEM32(0x19DCE0)` — the address of the
> statically allocated NV2A software device at `0x0019B200`.** **It is not separately allocated: it is a
> sub-object of the device, produced by *address arithmetic on the device pointer*, and it is bound to the
> five call sites through two distinct, both-verified mechanisms.**

**The disambiguation that resolves the packet's `+0x2268` prohibition — read carefully:**

| Claim | Status |
|---|---|
| **`[device + 0x2268]` IS the device** | **STILL FALSE — not re-litigated.** Gated dump reads `[0x19D468] = 0xFD000000`. |
| **The CONTEXT is LOCATED AT `device + 0x2268`** | **VERIFIED — this is the new finding.** `lea edi,[esi+0x2268]` with `esi = ecx = device`. |

**These are not in conflict.** **`0x2268` is the context's *address* offset from the device, not a *pointer
slot* that holds the device.** **The earlier refutation killed the pointer-slot reading; it never tested the
address reading, and the address reading is what the bytes say.**

**Two independent binding mechanisms, both verified:**

1. **The DPC binding — `sub_00194480`.** `sub_00194ADD` executes
   `KeInitializeDpc(DeferredRoutine = 0x00194480, DeferredContext = context)`. **The kernel stores the
   context in the DPC and hands it to `sub_00194480` as its argument** — that is why `sub_00194480` has no
   caller and reads its context from `[esp+0xC]`.
2. **The thunk binding — `sub_001941E0`.** `sub_00194EEF` passes `context + 0x1A0` to the IAT slot
   `0x001C40D8`, and `sub_001941E0` recovers `context = arg − 0x1A0`. **The gated dump proves the pairing:**
   `[context + 0x1A0] = 0x001941E0` and `[context + 0x84 + 0xC] = 0x00194480`.

**The "device" the wrappers reach through `[context]` is the NV2A MMIO aperture `0xFD000000`** — written by
`sub_0019460A` as `mov dword ptr [ecx], 0xfd000000` with `ecx = context`, called from `sub_00194ADD`. **So
`[context+N]` device offsets are hardware register offsets, which is exactly why they read like register
numbers (`0x400700`, `0x3214`, `0x2500`).**

---

## 2. CALLER CHAINS — per function, with the broken/closed edge

| # | Function | Its own caller(s) | How `ecx` is supplied | Edge |
|---|---|---|---|---|
| 1 | **`sub_00196C0B`** | `sub_00194C3F` (×3: `0x194CD1`, `0x194DC1`, `0x194E0D`), `sub_00194E2D` (`0x194E7E`) | `ecx = [ebp−8]` / `ecx = [ebp−4]` — **the caller's own incoming `ecx`**, saved in its prologue | **CLOSED** |
| 2 | **`sub_00194A72`** | `sub_00194C3F` (`0x194C56`), `sub_00194E2D` (`0x194E44`) | `ecx` **untouched from the prologue** (`[ebp−8] = ecx` / `[ebp−4] = ecx`) | **CLOSED** |
| 3 | **`sub_00194300`** | `sub_00194480` (`0x1944E9`), `sub_00194A72` (`0x194A9A`), `sub_00194EEF` (`0x194F54`) | `ecx = ebx` / `ecx = edi` — **each caller's own context register** | **CLOSED** |
| 4 | **`sub_00194EEF`** | `sub_001925A0` (`0x192727`), `sub_001941E0` (`0x1941FF`) | `ecx = [esp+8]` / `ecx = [esp+4] − 0x1A0` | **CLOSED — the producer edge** |
| 5 | **`sub_00194480`** | **NO CALL SITE EXISTS** | **DPC `DeferredRoutine`** — kernel-supplied argument | **CLOSED — not a broken edge** |

**Then, one level up:**

```
sub_001925A0        push ecx / push esi / mov esi,ecx        <== esi = ecx = DEVICE
                    lea edi,[esi+0x2268]                    <== CONTEXT = device + 0x2268
                    mov [esp+8],edi                         <== save context (frame slot)
                    mov ecx,edi / call 0x193C20             <== poll loop
                    mov ecx,[esp+8] / call 0x194EEF         <== site 4, ecx = CONTEXT

sub_00194EEF        mov edi,ecx                             <== context
                    lea eax,[edi+0x1A0] / push eax
                    call [0x1C40D8]                         <== passes context+0x1A0 to the thunk

sub_001941E0        mov ecx,[esp+4] / add ecx,0xFFFFFE60    <== context = arg - 0x1A0   [VERIFIED XBE]
                    mov eax,[ecx-0x1A0] / mov [eax+0x140],0
                    call 0x194EEF                            <== site 4 again, ecx = CONTEXT
```

**`sub_001925A0` is entered with `ecx = device`** from both of its callers: `sub_0018CDF0`
(`ecx = edi`, `edi = MEM32(0x19DCE0)`) and `sub_0018E460` (`ecx = 0x19B200`).

**Container arithmetic cross-check (XBE-verified):** `sub_00194C3F` does `ecx = ebx + 0x2268` and
`sub_00194EEF` does `[edi + 0x1A0] = [ebx + 0x2268 + 0x1A0] = [ebx + 0x2408]` — **and the gated dump reads
`[0x19D608] = 0x001941E0`.** **`0x19D608 = 0x19B200 + 0x2408`. The thunk is stored exactly where the static
chain predicts.** **The two paths are the same object.**

---

## 3. FIELD LAYOUT — does it bind all five sites? **YES**

**CONTEXT-relative** (`esi = ecx` in the callee; `edi`/`ebx`/`[esp+8]`/`[esp+0xC]` at the sites):

| Offset | Contents | Evidence |
|---|---|---|
| **`+0x00`** | **`0xFD000000`** — the NV2A MMIO base | written by `sub_0019460A` (`0x0019460A mov dword ptr [ecx],0xfd000000`), called from `sub_00194ADD`; dump `[0x19D468]=FD000000` |
| `+0x84` | **KDPC** | `KeInitializeDpc` in `sub_00194ADD` (`lea eax,[esi+0x84]`, `push 0x194480`) |
| **`+0x1A0`** | **`0x001941E0`** — the DPC thunk | `sub_00194EEF` pushes `edi+0x1A0`; dump `[0x19D608]=0x001941E0` |
| **`+0x1C4`** | **THE CALLBACK SLOT** | `0x00193E62 mov eax,[esi+0x1C4]`, NULL-tested; dump `[0x19D62C]=0x0015F9D0` |
| `+0x1C8` | KEVENT | `KeSetEvent(context+0x1C8,…)` in `sub_00193D90` |
| `+0x1D8` | second Type-0 dispatcher | `sub_00194ADD` evidence |
| `+0x1F0`,`+0x1F4`,`+0x1F8`,`+0x1FC`,`+0x208`,`+0x20C` | callee-read/updated fields | `sub_00193D90` |
| `+0x104`,`+0x124`,`+0x130`,`+0x134`,`+0x13C`,`+0x148`–`+0x154` | channel/instance fields | `sub_00194EEF`, `sub_00196B6A`, `sub_00196C4A` |

**DEVICE-relative** (reached as `[esi+N]` where `esi = [context] = 0xFD000000` → **hardware registers**):

| Offset | Meaning |
|---|---|
| **`+0x100`** | **the polled flag word**, bit `0x01000000` |
| `+0x2100`,`+0x2400`,`+0x3214`,`+0x3220` | PGRAPH/PFIFO status |
| `+0x400100`,`+0x400700` | FIFO registers |

**Binding verdict.** **All five sites' contexts are the same object.** The layout alone would not prove it
(the packet is right about that) — **but the producer does:** `sub_00194300`, `sub_00194A72` and
`sub_00194EEF` receive `ecx = context` **directly** from the `sub_001925A0`/`sub_00194C3F` chain that
computes `device+0x2268`; `sub_00196C0B` receives it from that same chain via `[ebp−8]`/`[ebp−4]`;
**and `sub_00194480` receives it from the kernel as the DPC's `DeferredContext`.** **Four by direct
register lineage, one by kernel binding, zero unbound.**

---

## 4. DEVICE_DERIVATION — does any site derive from `MEM32(0x19DCE0)`?

**YES — ALL FIVE, through the same global, and `0x19DCE0` is a POINTER to the device, not the device.**

**Both writer sites of the global were located (the only two):**

| Site | Function | Store |
|---|---|---|
| `0x0018E4B9` | `sub_0018E460` | `MEM32(0x19DCE0) = 0x19B200` (and `0x19DCE4`), then `ecx = 0x19B200` and calls `0x00192090` |
| `0x0018CE09` | `sub_0018CDF0` | `MEM32(0x19DCE0) = 0` on the last reference (`+0x43C` refcount → 0) |

**`0x0019B200` is NOT heap-allocated** — it is a **statically allocated (BSS) device object**; the original
XBE has no file-backed content at `0x19B200` (`inspect-jsrf.py data` → *"range is not contained in one
file-backed XBE section"*). **So the "allocation" the packet asked for is a static reservation plus a
refcounted publication through `0x19DCE0`; the context is a sub-object of it, never separately allocated.**

**Cross-check — `0x0018E160` uses the same context, XBE-verified:**

```
0018E164 mov ebx,[0x19dce0]        <== DEVICE
0018E1D0 lea ecx,[ebx+0x2268]      <== CONTEXT = device + 0x2268
0018E1D6 call 0x194c3f             <== enters the same wrapper chain
0018E1F4 lea ecx,[ebx+0x2268]      <== and again for 0x194e2d
```

**Two structurally unrelated parents (`sub_0018E160` and `sub_001925A0`) independently compute
`device+0x2268` and feed the same chain — a strong, independent confirmation of the offset.**

---

## 5. THE `0x00194480` RESOLUTION (the "no caller" row)

`sub_00194480` is **not** dead code and its absence of call sites is **not** a broken edge:

```
00194ADD mov esi,ecx                       <== context
00194AE7 push esi                          <== DeferredContext = CONTEXT
00194AE8 push 0x194480                     <== DeferredRoutine = sub_00194480
00194AED lea eax,[esi+0x84]                <== Dpc = &context->Dpc
00194AF3 push eax
00194AF4 call dword ptr [0x1c4020]         <== KeInitializeDpc(Dpc, Routine, Context)
```

**Gated dump corroboration:** `[0x19D4EC+0xC] = 0x00194480` (DeferredRoutine) and
`[0x19D4FC] = 0x0019D468` (DeferredContext = **the context**). **`sub_00194480`'s `ebx = [esp+0xC]` is the
kernel passing that context back** — which is exactly why it reads the context from the stack, the fact the
Session's correction flagged and could not yet explain.

**`sub_00194480` then returns to `sub_00194A72`/`sub_00194300`/`sub_00196C0B` and the callee — the DPC's
whole job is to drive the poll loop.** **The terminal is reachable from a DPC.**

---

## 6. CONTROLS AND GATES

| Control | Requirement | Result |
|---|---|---|
| **`inspect-jsrf.py data`** (original XBE) | `0x001D5078` → `30766A64 305F3030 7864612E` | **PASS** — exact match |
| **`inspect-jsrf.py memory`** (gated dump) | same bytes | **PASS** — exact match |
| **Per-run dump gate** | `check-dump-mapping.py 20260928-035337-386-a2h-repeat-on-1` | **PASS** — `.text[0:16] @0x00011000 = 8b512c85d28b4130c70190431c00741c`, `matches: 1 content-mismatch: 0 unreadable: 0 missing: 0` |
| **XBE byte anchoring** | every claim decoded from original bytes | `sub_001941E0`, `sub_001925A0`, `sub_00194EEF`, `sub_00194C3F`, `sub_00194E2D`, `sub_00194480`, `sub_0019460A`, `sub_00194ADD`, `sub_0018E160`, `sub_0019270E` all disassembled from `game/default.xbe` |
| **No loose pattern matching** | each row re-read against bytes | done — **no row was taken from a script match alone** |

**DUMP_READS:** all from `logs/runs/20260928-035337-386-a2h-repeat-on-1` (**the one archived PASS**), gate
run **for that run** and recorded above. **The 23/24 census was NOT used.** **No other run was read.**

**The run "never reached this terminal" — and that is consistent:** `[device+0x100]` reads `0` there, so the
`0x01000000` branch never fires. **The reads above are *state* evidence for the object's identity and
layout, not evidence that the terminal executed.**

---

## 7. WHAT THIS DOES **NOT** ESTABLISH

1. **Who writes `context+0x1C4`.** The slot holds `0x0015F9D0`, but **its writer was not traced** — that is
   the packet's explicit successor edge (`§10: do not enumerate or attribute its writer`).
2. **Why `[context] = 0xFD000000` rather than `0x0019B200`.** Both are "the device" in different senses
   (MMIO aperture vs software object). **The wrappers' `[context+N]` offsets are hardware registers, so the
   MMIO reading is the operative one — but the packet's original wording ("the context's FIRST DWORD is the
   device") is satisfied only under the MMIO reading.** **Recorded as a clarification, not a contradiction.**
3. **That `0x001941E0`'s thunk target is reached at runtime.** The IAT slot `0x1C40D8` is unresolved in the
   original XBE (`0x8000002F`) and holds an import thunk in the dump. **The static call and the stored
   pointer value are proven; the runtime dispatch through the import is not.**
4. **The refcount semantics of `0x0018CDF0`.** `+0x43C` reaching 0 tears the device down; **the ordering
   against the poll loop was not analysed.**

**No synthetic completion, no instrumentation, no game run, no generated-code edit.**
`RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` untouched. `PIO_FREE`, `A4b2-r7`, `A4b2-r8`,
`A4b1-r4`, `0xFFFFB3` untouched. **The retired NULL line was not reopened and no DR record was cited.**
**`device+0x2268` was disambiguated (address vs slot), not re-litigated.** Float-bit siblings contrastive.
