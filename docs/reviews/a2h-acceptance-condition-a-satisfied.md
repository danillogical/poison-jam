# `A2h` — acceptance condition (a) SATISFIED: all five memory values corroborated

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Authority:** the stage-1 review `a2h-callback-context-producer-acceptance-review.md` (`58694a5`), which
attached **two conditions to the ROW** (not the finding): **(a)** no row may lean on a memory-derived value
until each is re-read with the **offset-shift control**; **(b)** the successor must fix the *"device"*
vocabulary and canonicalise the open edge as **`device+0x242C`**.
**This executes (a) and settles (b)'s canonicalisation.**

---

## Condition (a): SATISFIED — all five values corroborated

**Per-run gate first:** `20260928-035337-386-a2h-repeat-on-1` → **`matches: 1`, no mismatch.** **PASS.**

**The discriminating control, established from BOTH directions:**

| Read | Printed text | LE-DWORD-VALUE predicts | Memory-order predicts |
|---|---|---|---|
| `0x001D5078` | **`30766A64`** | **`30766A64`** ✓ | `646A7630` ✗ |
| `0x001D5079` | **`3030766A`** | **`3030766A`** ✓ | `6A763030` ✗ |

> **CONTROL PASS — the tool prints little-endian DWORD VALUES, so the correct parse is `int(text, 16)`.**

**All five values, each with a shift check:**

| Address | Value | Expected | Shift |
|---|---|---|---|
| `0x0019D468` | **`0xFD000000`** | `0xFD000000` | **OK** |
| `0x0019D608` | **`0x001941E0`** | `0x001941E0` | **OK** |
| **`0x0019D62C`** | **`0x0015F9D0`** | `0x0015F9D0` | **OK** |
| `0x0019D4F8` | **`0x00194480`** | `0x00194480` | **OK** |
| `0x0019D4FC` | **`0x0019D468`** | `0x0019D468` | **OK** |

**All five MATCH and shift consistently.** **Condition (a) is SATISFIED.**

**And the arithmetic cross-check is independent of every read:**
`0x19B200 + 0x2268 = 0x0019D468` ✓ · `+0x242C = 0x0019D62C` ✓ · `context + 0x1A0 = 0x0019D608` ✓

## Condition (b): the vocabulary, and the ONE slot

**The reviewer's most consequential finding:** **`context+0x1C4 == (device+0x2268)+0x1C4 == device+0x242C`.**
**Verified arithmetically: `0x2268 + 0x1C4 = 0x242C`.** **So the packet's successor edge and the Advisor's
predicted edge are THE SAME SLOT.**

> **CANONICALISE AS `device+0x242C`.** **There is ONE unknown, not two.**

**And the vocabulary must be fixed, because *"device"* names TWO OBJECTS in this line:**

| Name | Address | What it is |
|---|---|---|
| **`software_device`** | **`MEM32(0x19DCE0) = 0x0019B200`** | **the software NV2A device object** — the `device` in `device+0x242C` |
| **`aperture`** | **`[context] = 0xFD000000`** | **the NV2A MMIO register aperture** — what the wrappers read through |

**Both are simultaneously true and they are NOT the same object.** **The packet's phrase *"the context's FIRST
DWORD is the device"* conflates them**, **and the reviewer warned that the next reviewer would hit the same
trap.** **The successor must use `software_device` and `aperture` explicitly.**

## ⚠ The Session's own two script bugs while running this check — instances 8 and 9

**Recorded because they are the SAME failure family, committed while testing for it.**

**Instance 8 — a DOUBLE REVERSAL.** The Session's first re-read script did
`int.from_bytes(bytes.fromhex(text), "little")`. **But the tool's text IS ALREADY THE VALUE**, so that reversed
a value a second time: `"30766A64"` became `0x646A7630`. **The script reported the control as FAIL, and the
control was fine.** **This is structurally identical to the original `device+0x2268` error — applying a
transformation to a value already in final form.**

**Instance 9 — a TOO-STRICT SHIFT CHECK.** The corrected script asserted `v1 == (v0 >> 8)`, which **silently
requires the byte AFTER the dword to be zero.** **The correct check is
`(v1 & 0x00FFFFFF) == (v0 >> 8)`.** **Reading one byte later drops the low byte and admits the NEXT byte as the
new high byte** — for `0x0019D4F8` the next byte is `0x68`, which is simply the adjacent field's low byte.

**Both were caught by running the check against a KNOWN value** — the string control for instance 8, the
arithmetic cross-check for instance 9. **The control set worked; the Session's scripts did not.**

> **This is now nine instances, and the two newest were committed WHILE TESTING FOR THE CLASS.** **The lesson is
> not "be careful" — it is that the Session's own scripts need the same known-answer controls as tool output,
> which the practice note already says and which the Session had not applied to its own verification code.**

## What this settles for the line

| Item | Status |
|---|---|
| **The five memory values** | **CORROBORATED — condition (a) satisfied** |
| **`MEM32(device+0x242C) = 0x0015F9D0`** | **CONFIRMED — the installer chain ran** |
| **`[context] = 0xFD000000`** | **CONFIRMED — the MMIO aperture** |
| **The two DPC fields** | **CONFIRMED** |
| **The open edge** | **ONE slot: the writer of `device+0x242C`** |
| **Vocabulary** | **`software_device` = `0x0019B200`; `aperture` = `0xFD000000`** |
| **The second edge (C-5)** | **the caller of `sub_00194ADD` / the `KeInsertQueueDpc` site — still open** |

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited.**
