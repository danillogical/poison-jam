# `A2h` — the `edi` IDENTITY: **IDENTIFIED** (Session offline trace, static + XBE bytes only)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Status:** **the packet's named edge, answered offline from XBE bytes and generated source — no dump, no run.**
**Authority:** `a2h-callback-slot-writer-r1-row-selection.md` (named edge = `edi`'s identity); Advisor input
restrictions `a2h-integrity-audit-remediation-advisor-ruling.md`.
**Row impact:** **this is `O-ALTERNATE-PATH` evidence — a VERIFIED DISTINCT context identity.**

---

## THE FINDING: the context is a distinct object whose FIRST DWORD IS THE DEVICE

**All five incoming call sites to `sub_00193D90` receive `ecx` as a PARAMETER, and four of the five containing
functions share one shape:**

| Function | `edi ← ecx` | `esi ← [edi]` | device offset via `esi` |
|---|---|---|---|
| **`sub_00196C0B`** (the poller) | **yes** | **yes** | **`[esi + 0x400700]`** |
| **`sub_00194300`** | **yes** | *(stored to `[esp+8]`)* | **`[esi + 0x2100]`** |
| **`sub_00194480`** | *(reads `[esp+0xC]`)* | `ebp = [ebx]` | `[ebp + 0x100]` |
| **`sub_00194A72`** | **yes** | **yes** | **`[esi + 0x3214]`, `[esi + 0x2400]`** |
| **`sub_00194EEF`** | **yes** | **yes** | **`[esi + 0x3214]`** |

**The shape is: `edi = ecx` (the context), `esi = [edi]` (the device), then device fields through `esi`.**

**And the failing callee confirms it:** `sub_00193D90` begins `mov esi, ecx` (`0x00193D96`), **so `esi` IS the
incoming context**, and the callback read is:

```
00193E62  mov  eax, dword ptr [esi + 0x1c4]     <== THE CALLBACK SLOT
00193E68  test eax, eax
00193E6A  je   0x193ece                          <== NULL test, as the predecessor found
```

> ## **So the callback slot belongs to the INCOMING CONTEXT OBJECT — `edi+0x1C4` — and NOT to the device, and
> NOT to `device+0x242C`.**

**This is exactly the `O-ALTERNATE-PATH` condition:** *"Verified distinct context identity
(`edi≠device+0x2268`)"* — **and now the distinct object is CHARACTERISED, not merely distinguished.**

## The context's field layout, established across the five sites

| Offset | Meaning | Evidence |
|---|---|---|
| **`+0x00`** | **the DEVICE pointer** | `esi = [edi]`, then device offsets through `esi` — **four functions agree** |
| **`+0x100`** | **the polled flag word** | `[esi + 0x100]` tested against **`0x01000000`** before every `sub_00193D90` call |
| **`+0x1C4`** | **THE CALLBACK SLOT** | `0x00193E62`, **NULL-tested before the call** — **the field that held `0x001D5078`** |
| `+0x20C` | read by `sub_00193D90` | `0x00193DA0` |
| `+0x208` | written by `sub_00193D90` | `0x00193DAE` |
| `+0x1F4`, `+0x1F8`, `+0x1FC` | read by `sub_00193D90` | `0x00193DB4+` |

**So the context is a per-`sub_00193D90`-consumer object that POINTS AT the device — a wrapper/adapter, not a
device sub-object.** **That is why `[device+0x2268]` was never going to be it: the relationship is
`context → device`, not `device ⊃ context`.**

## Why this is coherent with everything already established

- **`[device+0x2268] = 0xFD`** — **a device field, and irrelevant to this call.** ✓
- **`MEM32(device+0x242C) = 0x0015F9D0`** — **the installer's slot is real and populated, in the DEVICE.**
  **The context's `+0x1C4` is a DIFFERENT slot in a DIFFERENT object.** ✓
- **No call site passes `lea ecx,[reg+0x2268]`** — **because the context is passed in, not constructed
  there.** ✓
- **`ecx − esp = 0x20`** at the poller — **the context is a stack-passed argument.** ✓

> **The two slots coincidentally share the offset `0x1C4` vs `0x242C` only through the now-dead arithmetic
> `0x2268 + 0x1C4`.** **They are unrelated.**

## What this does NOT establish — stated plainly

1. **The context's ALLOCATION SITE is not yet found.** **The Session has characterised the object and its
   layout, but not where it is created or who fills `+0x1C4`.** **That remains the open edge.**
2. **Therefore `O-ALTERNATE-PATH`'s second half — *"feasible field/writer routes"* — is NOT yet met.**
   **The identity is verified; the WRITER is still unknown.**
3. **`edi`'s concrete runtime address is unknown** — **and the Session is NOT reading a dump to get it, per
   the Advisor's input restrictions** (only one archived run passes the gate, and it has no terminal).

**So the row is `O-ALTERNATE-PATH` on the IDENTITY half, with the writer as the successor's edge.** **The
Session records this as the named edge being ANSWERED, not as the row being closed.**

## Method compliance

- **XBE bytes + generated source only.** **No dump read, no run, no instrumentation.** ✓
- **Anchored to DECLARED boundaries** — the four containing functions came from `recomp_0004.c` headers with
  explicit `Original:` ranges, **not from an inferred boundary.** ✓
- **No re-litigation of the `device+0x2268` alias.** ✓
- **Known-answer control:** the decode of `0x00193E62` **reproduced the predecessor's independently recorded
  `[esi+0x1C4]` read and its `loc_00193ECE` NULL target exactly** — **an independent witness that the
  disassembly boundary is correct.** ✓
- **`uint32` normalization** throughout. ✓

## Prohibitions and status

**Static and offline only.** **No synthetic completion.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE,
…)` unchanged. **`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited**; **float-bit siblings
remain contrastive.**
