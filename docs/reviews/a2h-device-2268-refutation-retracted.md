# ⚠ MAJOR CORRECTION — the `device+0x2268` refutation was based on an INVALID INFERENCE

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**What is corrected:** the Session's identity-gate finding
(`docs/reviews/a2h-callback-slot-identity-gate-executed.md`), which the Advisor ruled *"defeats the device-slot
branch permanently."* **That ruling rested on a false premise, and the premise is now retracted.**
**Found by:** the Worker `bdda7b7f` (`docs/reviews/a2h-callback-context-producer-evidence.md`, `537437f`).

---

## What the Session concluded, and why it was wrong

**The Session reasoned:**

> `edi` is the context. The poller does **`esi = [edi]`**, then reads `[esi + 0x400700]`, `[esi + 0x100]`.
> **So `[edi]` is "the device."** **If `edi = device+0x2268`, then `[device+0x2268]` must equal the device.**
> **It reads `0xFD000000`. Therefore `edi` is NOT `device+0x2268`.**

**The invalid step is the middle one.** **It assumed `[edi]` is the SOFTWARE DEVICE at `0x0019B200`.**

**The Worker found the write that settles it — and the Session verified it in the XBE bytes:**

```
0019460A  mov dword ptr [ecx], 0xfd000000
```

**`[context] = 0xFD000000` — the NV2A MMIO APERTURE, not the software device `0x19B200`.**

> **So `[edi] != 0x19B200` is EXPECTED and does NOT refute `edi = device+0x2268`.** **The Session's
> inference was invalid, and the "permanent" refutation was not a refutation at all.**

**And the Worker's `lea` is in the bytes:**

```
001925A0  push ecx
001925A2  mov  esi, ecx
...
001925FB  lea  edi, [esi + 0x2268]     <== the CONTEXT is device + 0x2268
00192605  mov  ecx, edi
```

## The second, independent way the Session's reasoning failed

**The Session also relied on `inspect-jsrf.py memory` printing little-endian DWORD VALUES**, and used
`0x001D5078 → 30766A64` as its control. **That control is consistent with BOTH readings**, so it never
discriminated. **The Session has now run an OFFSET-SHIFT test that does:**

| Read | Observed | Little-endian DWORD VALUES predicts | Memory-order bytes predicts |
|---|---|---|---|
| `0x001D5078` | **`30766A64`** | **`30766A64`** ✓ | `646A7630` ✗ |
| **`0x001D5079`** | **`3030766A`** | **`3030766A`** ✓ | `6A763030` ✗ |

**So the tool DOES print little-endian DWORD values — the Session's reading of the FORMAT was right.**

> **But the Session then treated the printed value `0x00B21900` as the CONTENT of `[device+0x2268]`.**
> **`0x00B21900` is `FD000000` byte-reversed.** **So the actual content is `0xFD000000` — and the Session's
> original "djv0" instinct was closer to the truth than its "correction."**

**The Session corrected a reading error into a DIFFERENT reading error, and the second one carried a
"permanent" ruling.** **Recorded plainly: the Session got the format right and the semantics wrong.**

## What this means for the line

| Claim | Status |
|---|---|
| **`device+0x2268` is PERMANENTLY REFUTED** | **RETRACTED.** **The refutation was invalid.** |
| **`[device+0x2268] = 0xFD000000`** | **TRUE — and it is the MMIO aperture, exactly as expected if the context lives there** |
| **The context is `device + 0x2268`** | **SUPPORTED by the Worker's `lea`, the DPC binding and the thunk binding** |
| **`MEM32(device+0x242C) = 0x0015F9D0`** | **The printed value must be re-read under the corrected semantics** |
| **`[reg+0x1C4]` has no static write in D3D** | **Still a static observation; unaffected** |
| **The call site `0x00193EB5` is unique** | **Unaffected** |

**The Advisor's *"no future packet may re-litigate the alias without new evidence"* clause is now satisfied:
the Session IS the new evidence, and it points the other way.**

## What the Session must NOT do

- **Do not quietly drop the refutation.** **It is retracted explicitly, and the ruling that depended on it must
  be re-referred.**
- **Do not treat the Worker's finding as accepted.** **It is the executor's evidence and needs stage-1 review
  like any other.**
- **Do not re-derive `device+0x242C` from the corrected semantics without a gate** — **the earlier
  `0x0015F9D0` reading is now suspect in the same way.**

## The lesson, which is the sixth-and-seventh instance of one pattern

**Instance 6 was a transposed address.** **This is instance 7, and it is the worst kind:** **the Session
verified a tool's FORMAT correctly, then reasoned about the tool's SEMANTICS from an unstated assumption
(`[edi]` is the software device), and the assumption was never checked.**

> **The practice note's rule — per-row verification against the underlying artifact — would have caught this,
> because checking `[edi]`'s meaning against the XBE would have found `mov dword ptr [ecx], 0xfd000000`.**
> **The Session had that write available and never looked for it.**

**Escalating to the Advisor, because a "permanent" ruling rests on the retracted premise.**
