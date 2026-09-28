# `A2h` — the `edi` IDENTITY: **IDENTIFIED** (Session offline trace, static + XBE bytes only)

> ## ⚠ CORRECTION 2026-09-28 — my `+0x100` field attribution was WRONG
>
> **The Planner (`6df57264`) challenged two claims and BOTH CHALLENGES ARE CORRECT.** **Verified by the
> Session:**
>
> **(a) `[esi + 0x100]` is `DEVICE + 0x100`, NOT a context flag.** **The Session's original detection script
> matched `mov dword ptr [esp + 8], ecx` as if it were `edi = ecx`, which corrupted the `edi ← ecx` column.**
> **The real provenance at each site:**
>
> | Function | how `esi` is established | so `[esi+N]` is |
> |---|---|---|
> | `sub_00196C0B` (poller) | **`esi = [edi]`** (`0x00196C0F`) | **DEVICE** |
> | `sub_00194300` | **`esi = [ecx]`** (`0x00194307`) | **DEVICE** |
> | `sub_00194480` | `ebp = [ebx]` | **DEVICE** |
> | `sub_00194A72` | **`esi = [edi]`** (`0x00194A76`) | **DEVICE** |
> | `sub_00194EEF` | **`esi = [edi]`** (`0x00194EFA`) | **DEVICE** |
> | **`sub_00193D90` (the callee)** | **`esi = ecx`** (`0x00193D96`) | **CONTEXT** |
>
> **So `[esi+0x100]`, `[esi+0x2100]`, `[esi+0x3214]`, `[esi+0x400700]` are ALL DEVICE offsets**, and
> **`[esi+0x1C4]` in the callee is the ONLY context-relative field.** **The corrected table appears below.**
>
> **(b) `sub_00194300` does NOT set `edi = ecx`** — **it stores `ecx` to `[esp+8]` and leaves `edi` untouched**
> until `push edi` at `0x00194314`. **So "four of five share `edi = ecx`" is FALSE.**
>
> **What SURVIVES unchanged and is the finding's core:** **every call site passes the context as a PARAMETER,
> and the context's FIRST DWORD IS THE DEVICE** (`esi = [context]`, then device offsets through `esi`). **The
> callback slot `+0x1C4` belongs to the CONTEXT, read by the callee via `esi = ecx`.** **The identification
> stands; my field ATTRIBUTION table did not.**
>
> **Recorded as the FIFTH reading error this session, and the pattern is the same as the fourth: I derived a
> table from a script whose match condition was too loose, and did not verify each row against the bytes.**
> **The Planner caught it by checking the generated source line by line — which is what I should have done
> before publishing the table.**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Status:** **the packet's named edge, answered offline from XBE bytes and generated source — no dump, no run.**
**Authority:** `a2h-callback-slot-writer-r1-row-selection.md` (named edge = `edi`'s identity); Advisor input
restrictions `a2h-integrity-audit-remediation-advisor-ruling.md`.
**Row impact:** **this is `O-ALTERNATE-PATH` evidence — a VERIFIED DISTINCT context identity.**

---

## THE FINDING: the context is a distinct object whose FIRST DWORD IS THE DEVICE

**All five incoming call sites to `sub_00193D90` receive `ecx` as a PARAMETER, and every one of them reaches
the DEVICE by dereferencing that parameter's first dword:**

| Function | how the context is held | how the DEVICE is reached | device offset read |
|---|---|---|---|
| **`sub_00196C0B`** (the poller) | **`edi = ecx`** | **`esi = [edi]`** | **`[esi + 0x400700]`, `[esi + 0x100]`** |
| **`sub_00194300`** | **`[esp+8] = ecx`** *(NOT `edi`)* | **`esi = [ecx]`** | **`[esi + 0x2100]`** |
| **`sub_00194480`** | **`ebx = [esp+0xC]`** | **`ebp = [ebx]`** | `[ebp + 0x100]` |
| **`sub_00194A72`** | **`edi = ecx`** | **`esi = [edi]`** | **`[esi + 0x3214]`, `[esi + 0x2400]`** |
| **`sub_00194EEF`** | **`edi = ecx`** | **`esi = [edi]`** | **`[esi + 0x3214]`** |

**The invariant is: THE DEVICE IS `[context]` — the context's FIRST DWORD.** **The register holding the
context differs (`edi`, `ecx`, `ebx`, `[esp+8]`), and `sub_00194300` does NOT use `edi` for it at all.**

**⚠ The Session's original version of this table claimed `edi ← ecx` for four of five and filed `+0x100` as a
CONTEXT offset. BOTH WERE WRONG** — **see the correction at the top of this record.** **The invariant above is
what actually holds, and it is verified per-site against the bytes.**

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

## The context's field layout — CORRECTED

> **⚠ The table below replaces the Session's original, which wrongly filed `+0x100` under the CONTEXT.**
> **`[esi + 0x100]` is `DEVICE + 0x100` at every site except the callee, because `esi = [context]` there.**
> **Only `+0x1C4` and the callee-read fields are CONTEXT-relative.**

**CONTEXT-relative fields** (established inside `sub_00193D90`, where `esi = ecx` at `0x00193D96`):

| Offset | Meaning | Evidence |
|---|---|---|
| **`+0x00`** | **the DEVICE pointer** | **every site does `esi = [context]`**, then reads device fields through `esi` |
| **`+0x1C4`** | **THE CALLBACK SLOT** | **`0x00193E62 mov eax,[esi+0x1C4]`**, **NULL-tested** — **the field that held `0x001D5078`** |
| `+0x20C` | read by `sub_00193D90` | `0x00193DA0` |
| `+0x208` | written by `sub_00193D90` | `0x00193DAE` |
| `+0x1F4`, `+0x1F8`, `+0x1FC` | read by `sub_00193D90` | `0x00193DB4+` |

**DEVICE-relative fields** (reached as `[esi + N]` where **`esi = [context]`**):

| Offset | Meaning | Site |
|---|---|---|
| **`+0x100`** | **the polled flag word**, tested against **`0x01000000`** | `sub_00196C0B` (`0x00196C1C`), `sub_00194480` |
| `+0x2100` | a device status/flag word | `sub_00194300` (`0x00194309`) |
| `+0x2400`, `+0x3214` | device flags | `sub_00194A72`, `sub_00194EEF` |
| `+0x400700` | the "while nonzero" loop counter | `sub_00196C0B` (`0x00196C11`) |

**So the context is a per-`sub_00193D90`-consumer object whose FIRST DWORD IS THE DEVICE — a wrapper/adapter,
not a device sub-object.** **That is why `[device+0x2268]` was never going to be it: the relationship is
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
