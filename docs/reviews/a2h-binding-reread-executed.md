# The binding re-read — executed, and it exposes the EXACT mechanism of my error

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Authority:** Advisor ruling `a2h-device-2268-retraction-advisor-ruling.md` (turn `01a0e852`) **point 2** —
*"re-read `0x0015F9D0` and every row-feeding uncorroborated memory read, with offset-shift controls, BEFORE any
row leans on one."*
**Status:** **precondition EXECUTED. One earlier reading was WRONG and is corrected below.**

---

## Step 1 — the per-run integrity gate (mandatory first)

**Run `20260928-035337-386-a2h-repeat-on-1`: `matches: 1 / content-mismatch: 0 / unreadable: 0 / missing: 0`.**
**PASSES.** **Reads from it are admissible.**

## Step 2 — the OFFSET-SHIFT control, which is the discriminator

| Read | Observed | LE-DWORD-VALUE predicts | Memory-order predicts |
|---|---|---|---|
| `0x001D5078` | **`30766A64`** | **`30766A64`** ✓ | `646A7630` ✗ |
| **`0x001D5079`** | **`3030766A`** | **`3030766A`** ✓ | `6A763030` ✗ |

> **CONTROL: PASS — the tool prints LITTLE-ENDIAN DWORD VALUES.**

**The Session's ORIGINAL control (`0x001D5078 → 30766A64` alone) was satisfied by BOTH readings and never
discriminated.** **The offset-shift test is the control that actually works, and it is now the standard.**

## Step 3 — the re-read values, with their shift checks

| Value | Printed | Shift check (`+1`) |
|---|---|---|
| `MEM32(0x19DCE0)` | **`0019B200`** | `000019B2` ✓ consistent |
| **`MEM32(device+0x2268)`** | **`FD000000`** | `00FD0000` ✓ consistent |
| **`MEM32(device+0x242C)`** | **`0015F9D0`** | `000015F9` ✓ consistent |
| `MEM32(device+0x100)` | `00000000` | `00000000` ✓ |

## ⚠ Step 4 — THE EXACT MECHANISM, and it is worse than "a reading error"

**The Session reported:**

> `MEM32(device+0x2268) = 0x000000FD` … **"a small INTEGER — not a pointer, and not the device"** … **"it is a
> COUNTER/FIELD, not a sub-object whose first dword points back at the device."**

**The truth is `0xFD000000`.**

**`0xFD000000` is the NV2A MMIO APERTURE BASE — a RECOGNIZABLE HARDWARE CONSTANT, documented 29 times in this
repository** (`docs/jsrf-gpu-setup-contract.md:131`: *"All offsets are relative to the 16 MiB NV2A aperture at
`0xFD000000`"*; `:172`: *"`0xFD000000` is the NV2A register aperture in both xemu (16 MiB BAR0) and …"*).

> **The byte-reversal is what made the refutation LOOK valid.** **`0x000000FD` is a meaningless small integer,
> so "a counter, not a pointer" was a natural reading.** **`0xFD000000` is a hardware constant that a device
> sub-object's first dword SHOULD hold.**
>
> **Had the Session read `0xFD000000`, it would have recognised the aperture and asked why a "counter" held
> one. The reversal did not merely garble a value — it DESTROYED THE CLUE.**

**And the same reversal corrupted a second reading:**

| Value | Session reported | True |
|---|---|---|
| **`MEM32(device+0x242C)`** | **`0xD0F91500`** — *"is it the refcount thunk `0x15F9D0`? **False**"* | **`0x0015F9D0`** — **the refcount thunk. TRUE.** |

**So the installer-chain reading SURVIVES and is now CONFIRMED:** **`sub_0018CE30` did install `0x15F9D0` into
`device+0x242C`.** **The Session had reported the opposite.**

## The corrected picture

| Read | Correct value | Meaning |
|---|---|---|
| `MEM32(0x19DCE0)` | **`0x0019B200`** | the software device — **matches `recomp_0004.c:40150`** |
| **`MEM32(device+0x2268)`** | **`0xFD000000`** | **the NV2A MMIO aperture base** |
| **`MEM32(device+0x242C)`** | **`0x0015F9D0`** | **the refcount thunk — installer CONFIRMED** |
| `MEM32(device+0x100)` | `0x00000000` | the polled flag — **branch never fires, consistent with this run having no terminal** |

**And the whole picture is INTERNALLY CONSISTENT with the Worker's finding:** **the context at `device+0x2268`
holding the MMIO base at `+0x00` is exactly what `0x0019460A`'s `mov dword ptr [ecx], 0xfd000000` writes, and
exactly why the wrappers' "device offsets" read like hardware register numbers (`0x400700`, `0x3214`).**

## What this means for the rows

| Row / claim | Status |
|---|---|
| **`MEM32(device+0x242C) = 0x0015F9D0`** | **CORRECTED to TRUE — the installer chain is confirmed** |
| **The `device+0x2268` refutation** | **RETRACTED** (`a2h-device-2268-refutation-retracted.md`) |
| **`O-OPEN` for `a2h-callback-slot-writer`** | **STANDS** — the row evaluated at selection time, and the identity is now the Worker's claim pending acceptance |
| **`[reg+0x1C4]` has no static write in D3D** | **Unaffected** — a static observation |
| **The call site `0x00193EB5` is unique** | **Unaffected** |

## The methodological form of this error, which is new

**The Session's earlier errors were loose matches and a transposition.** **This one is different and more
dangerous:**

> **The Session applied a transformation (byte-reversal) that produced a PLAUSIBLE-LOOKING value, and then
> reasoned confidently from the transformed value. The transformation was invisible because its output was
> well-formed.**

**`0x000000FD` looks exactly like a counter. `0xD0F91500` looks exactly like a junk pointer.** **Neither
prompted doubt.** **The offset-shift control is what catches this class, because a reversal does not shift
correctly** — **and that is why the Advisor made it binding rather than advisory.**

**Per the Advisor's note, recorded as the Session's own conclusion:** **the controls caught this, and the
bounded re-read rule is the systemic form of the method.** **The Session will not treat seven caught errors as
seven failures — it will treat the control that caught this one as mandatory from here.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited.**
