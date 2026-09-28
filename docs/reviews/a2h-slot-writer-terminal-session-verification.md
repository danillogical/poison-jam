# `A2h-slot-writer-terminal-r1` — **`O-DATA-AS-CALL`**: THE WRITER IS FOUND

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-slot-writer-terminal-r1`, frozen
**`116884E8D474A0238096ACD43111E089E5A1A89C87F861E9FCBA953B95E05E9F`**.
**Worker:** `8e323046-c92f-4166-8a9c-6c21debe2e7b`, commit `f63dc53`.
**Finding:** `docs/reviews/a2h-slot-writer-terminal-evidence.md`.

> # **ROW: `O-DATA-AS-CALL`.** **The chain is COMPLETE. `WRITER: FOUND.`**

---

## THE ANSWER

**`software_device+0x242C` is written by:**

```
0x00199F45  mov dword ptr [esi + ebp*4 + 0x3ec], eax
```

**inside `sub_00199DB0`** (declared `0x00199DB0..0x0019A037`):

| Element | Value | Anchor |
|---|---|---|
| **BASE** | **`esi = MEM32(0x19DCE0)` = `software_device`** | **`0x00199DB4`** |
| **INDEX** | **`ebp` = ARG1 = `2064`** | **`0x00199DD9 mov ebp,[esp+0x20]`** |
| **SLOT** | **`0x3EC + 2064·4 = 0x242C`** | **Session-verified arithmetic: EXACT** |

**And the Session verified the arithmetic independently: `0x3EC + 2064·4 = 0x242C`, EQUAL.**

## THE VALUE IS NOT AN ADDRESS — it is a PACKED COLOUR WORD

**`sub_00199DB0` quantizes four float components per element** — `(int)(v*255.0f + 0.5f)`, with
`0x1C4CCC = 437F0000 = 255.0f` and `0x1C4550 = 3F000000 = 0.5f` — **and packs them:**

```
eax = (Q(edi+8)<<24) | (Q(edi-4)<<16) | (Q(edi+0)<<8) | Q(edi+4)
```

**For `0x001D5078` that is four ordinary quantized bytes: `0x00, 0x1D, 0x50, 0x78` = 0, 29, 80, 120.**

> **A packed colour word landed in the poll callback slot, and the guest executed `call eax` on it.**
> **`DATA-AS-CALL` — CONFIRMED, and it is the terminal's own failure class.**

## ⚠ WHY IT WAS INVISIBLE — and my own §6.1 rule is what explains it

**The Session verified the encoding difference directly from the bytes:**

| Store | Bytes | Form |
|---|---|---|
| **`0x0018CE3A`** (the installer) | **`89 81 2C 24 00 00`** | **ModRM `disp32`** — **carries the literal `2C 24 00 00`** |
| **`0x00199F45`** (THE WRITER) | **`89 84 AE EC 03 00 00`** | **SIB form** — **`disp32 = 0x3EC`** |

**And the Session verified: `2c240000` does NOT appear anywhere in the writer's bytes.**

> **So the line's long-standing *"exactly ONE direct store"* result was TRUE FOR ITS STATED ENCODING and
> STRUCTURALLY BLIND to the SIB form.** **The writer computes the slot as `0x3EC + index·4` and never
> contains the displacement.**

**This is EXACTLY the Advisor's §6.1 refinement, applied to the Session's own finding:**

> *"Byte-pattern scans are alignment-independent for **existence**; for **uniqueness** ('exactly one') state
> the **encoding coverage** over all instruction forms that could carry the pattern."*

**The Session's `2C 24 00 00` scan stated its coverage as *"the literal displacement"* — and the packet
records that limitation in writing.** **But *"exactly ONE"* was still read downstream as uniqueness, and the
SIB form was outside its coverage.** **The rule the Session helped place is what makes this legible rather
than mysterious.**

## THE COMPLETE CHAIN

```
install   0x0018CE3A   mov [ecx+0x242C], eax     ecx = MEM32(0x19DCE0), value 0x15F9D0
   ↓
write     0x00199F45   mov [esi+ebp*4+0x3EC], eax   esi = MEM32(0x19DCE0), ebp = ARG1 = 2064
   ↓
read      0x00193E62   mov eax, [esi+0x1C4]      esi = ecx = context, NULL-tested
          0x00193E6A   je 0x193ECE               (a zeroed slot would SKIP the call)
   ↓
call      0x00193EB5   call eax                 → 0x001D5078, the ADX filename
```

**Base, index and value are all INSTRUCTION-ANCHORED. No broken edge.**

**And the LOG CORROBORATES rather than infers:** `jsrf_run.log` shows **four identical
`[0xFE000190, 0x00193D90, 0xFE0000B4, 0x0015F9D0]` groups, then `[15] 0x001D5078`** — **exactly the *"fourth
polling iteration"* the whole line has been chasing.** **Recorded as INPUT, not inference.**

## ⚠ GAP B — and it corrects the line's central belief

**ARG1 at `0x00012319` is pushed at `0x000122EA` from `ebx`, which is zeroed by `xor ebx,ebx` at
`0x0001224F` and never reassigned through the call.** **So `ARG1 = 0`.**

**And `0x0015F9E0` then substitutes the CONSTANT `0x0015F9D0`, which `0x0018CE3A` installs.**

> **So `0x001D5078` CANNOT be written to the slot by the direct-store path.** **The direct-store path is the
> INSTALLER — a DECOY, and the line spent several packets on it.**

**Provenance is from the decoded argument, NOT from value equality.** ✓

**And the Worker read it at the DECLARED boundary `0x00012210`** — because **`0x000122E8` is mid-`rep stosd`
and decoding from it drifts to `.byte 0x68 / insb`.** **That is control 7 applied correctly, and it is the
same trap that produced the Session's failed census.**

## ⚠ MY `+0x3C8` LEAD IS REFUTED — empirically, and the refutation is the right kind

**The Worker bound the object and the lead does not survive:**

| Object | vtable | `+0x3C8` role |
|---|---|---|
| **the cluster's object** | **`0x001CB040`** (installed `0x00060CF2`) | **a COUNT used to size allocations via `0x4A8F0` and zero-fill** |
| **the installer's object** | **`0x001CE478`** (installed `0x000D45D4`) | the table base |

**And in the cluster's object `+0x3C8` is NEVER DEREFERENCED** — `0x000623B4` does
`mov eax,[esi+0x3c8] / test eax,eax / jle` — **a count again.**

> **`GAP_A_ENTRY8: NO.`** **The cluster never calls `0x000D4DA0` and never dereferences `+0x3C8`.**

**So the Session's warning — *"different object types can share an offset"* — was RIGHT, and the barred
offset-arithmetic inference would have been wrong.** **The Worker bound it EMPIRICALLY rather than by
offset, which is exactly what the Session said it must do.**

**And the seed classes it found are the real GAP A answer:**
- **`0x000D4DA0`** — **an imm32 absolute-pointer raw-dword scan: 6 occurrences, ALL at offset ≡ 1 (mod 4)**
  — **so an `--aligned` scan reports ZERO.** **This REPRODUCES the `0x001E1270` defect for a second address,
  independently confirming the §6.1 rule's second demonstration.**
- **`0x000D4684`** — **a generated declared-function-entry seed**; it is an **interior instruction of
  `sub_000D4590`** (`recomp_dispatch.c:3137`), validated against bytes
  **`C7 86 C8 03 00 00 E0 7D 25 00`**.
- **Entry 8 confirmed from the runtime construction at `0x0018B1CD`:** **`0x257DE0 + 8·4 = 0x257E00` holds
  `0x000D4DA0`.** ✓

## The new missing edge — RECORDED, NOT CHASED

**No reader of table `0x257DE0` through `object+0x3C8` was located** — **`find 0x257DE0` returns only the
installer immediate and the construction immediate.**

**The Worker recorded it and did NOT chase it, because it is not on this row's chain.** **That is the
terminality bound honoured exactly.**

## The controls — and the honest non-claim

**ZERO dump reads were performed, so `check-dump-mapping.py` was NOT run and the per-run gate is NOT claimed
as passed.** ✓ **The Worker states it explicitly rather than claiming a control with no input.**

**The offset-shift control was applied to ORIGINAL-XBE bytes instead** — `0x001D5078 → 30766A64` and
`0x001D5079 → 3030766A`, **both MATCH**, and **`(v1 & 0x00FFFFFF) == (v0>>8)` PASSES with the `0x30`
retained.** ✓ **`int(text,16)` throughout.** ✓

**And the method discipline held:** **two disasm drift incidents hit and discarded**, **one raw-scan false
positive (`0x0018F413`, mid-instruction) caught by decode classification**, **every decode anchored to a
declared boundary**, **and completeness claims labelled EXISTENCE enumerations over a named encoding rather
than uniqueness.** ✓

## Per the packet: THE LINE CLOSES HERE

> **CLOSE the writer ⇒ mechanism consideration is a SEPARATE LATER DECISION.**

**The Worker stopped there and did not auto-chain.** ✓ **The mechanism — *how a colour/palette element comes
to be written into the callback slot* — is explicitly NOT investigated and is a separate decision.**

## What this line established, in one paragraph

**The `0x001D5078` terminal is a DATA-AS-CALL: the guest polled a device flag four times, and on the fourth
iteration the callback slot at `software_device+0x242C` held a PACKED COLOUR WORD written by
`sub_00199DB0`'s palette-quantization loop at `0x00199F45`, which the guest then executed as code.** **The
installer path was a decoy; the alias path was a dead end; and the one real store was invisible to a
literal-displacement scan because it computes its address as `0x3EC + index·4`.**

## Prohibitions and status

**Static and read-only.** **No game run, no source edit, no instrumentation.** **No synthetic completion.**
`RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was
not reopened and no DR record was cited**; **C-5 stayed separate**; **float-bit siblings contrastive.**
**All eight guards pass.**
