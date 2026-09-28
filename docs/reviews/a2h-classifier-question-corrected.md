# `A2h` — the classifier question, corrected: my `0x7B3` residual was MY OWN artifact, and the real defect is narrower

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Corrects:** the `0x7B3` claim in `a2h-on3-corrected-blind-spot.md` (`2f2be3c`) and
`a2h-on3-blind-spot-advisor-ruling.md` (`ac07f2d`).
**Why:** the Session read `a2h_slotw_classify_store`'s actual implementation and found that its own residual
arithmetic was meaningless.

---

## ⚠ CORRECTION: the `0x7B3` residual was MY arithmetic, not a mapping bug

**The Session had written:** *"I computed the image base from the event's native RIP and got a `0x7B3`
residual — not a plausible image base — so the classifier's RIP→guest mapping needs verifying."*

**The Session read the classifier, and IT DOES NO RIP→GUEST MAPPING AT ALL:**

```c
static uint32_t a2h_slotw_classify_store(uint64_t rip)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)rip;
    ...
    if (p[0] != 0x89)                     /* mov r/m32, r32 */
        return XBOX_A2H_SLOTW_ENC_OTHER;
    modrm = p[1];
    if ((modrm & 0xC0u) != 0x80u)         /* both sites are [base + disp32] */
        return XBOX_A2H_SLOTW_ENC_OTHER;
    has_sib = ((modrm & 0x07u) == 0x04u);
    ...
    d = the disp32
```

> ## **The classifier reads the NATIVE INSTRUCTION BYTES at the RIP and classifies by ENCODING.** **It never subtracts a base.**

**So the Session's `base = rip − guest_va` computation was a hypothetical the code never performs, and its
`0x7B3` residual was an artifact of the Session's own arithmetic.**

**⚠ The Session had reported that residual as evidence that *"the classifier's RIP→guest mapping needs
verifying."*** **There is no such mapping.** **WITHDRAWN.**

**This is the fourth time in this line the Session has drawn a conclusion from arithmetic the code does not
perform** — **and the correction is recorded rather than quietly dropped.**

## The REAL defect, which is narrower and checkable

**The encoding constants:**

| Constant | Value | Meaning |
|---|---|---|
| **`ENC_UNKNOWN`** | **0** | — |
| **`ENC_MODRM`** | **1** | **`disp32 == 0x242C` — the installer's own encoding** |
| **`ENC_SIB`** | **2** | **`disp32 == 0x3EC` with SIB — the candidate's encoding** |
| **`ENC_OTHER`** | **3** | **a store to the page with neither displacement** |

**And the control's condition (`:3173`):**

```c
if (slot_hit && enc == XBOX_A2H_SLOTW_ENC_MODRM) {
    A2H_SLOTW_INC64(&L->loss.installer_control_hits);
}
```

**⚠ ON-3's slot event recorded `enc=3` — `ENC_OTHER`, NOT `ENC_MODRM`.**

> **So the classifier read the native bytes at the installer's RIP and did NOT find `89 81 2C 24 00 00`.**

## Why that is a real, concrete question — and where the answer lies

**The classifier assumes the NATIVE instruction bytes at the RIP match the GUEST instruction bytes.**

**For `mov dword ptr [ecx+0x242C], eax` the guest encoding is `89 81 2C 24 00 00`.** **If MSVC emits the same
form for the recompiled store, the bytes WOULD match and the classifier would work.**

**⚠ But `enc=3` says they did not.** **Possible causes the Session records WITHOUT choosing between:**

1. **a REX prefix** — any `0x40–0x4F` byte would shift `p[0]` away from `0x89`;
2. **a different base register or displacement** — if the compiler folded `xbox_base + ecx` differently, the
   displacement would not be `0x242C`;
3. **a different store form** — e.g. `mov [mem], reg` with an absolute address, or a wider/narrower store;
4. **the RIP is not the store** — e.g. the fault is reported at a different instruction.

**The Session does NOT choose.** **It records that the question is DECIDABLE OFFLINE, which is exactly what
the Advisor's Exp0 requires.**

## ⚠ And the value evidence still says the write WAS the installer's

**`post=0x0015F9D0` IS the installer's constant.** **And the Session notes the classifier's own comment
explains why the control does NOT test the value:**

> *"The control is 'a store with the installer's encoding landed on the derived slot' — that is what makes it
> a control — and the VALUE is recorded in the step record's `post_value` so a reader compares it against
> `0x0015F9D0` offline rather than having the comparison silently gate the hit count here."*

**So the design deliberately separates ENCODING (the control) from VALUE (the reader's check).** ✓ **That is
good design** — **and it means the Session's earlier inference *"`post=0x0015F9D0` so the control is green"*
was exactly the substitution the design forbids.**

**The Session's withdrawal of that claim was therefore CORRECT, and the reason is now precise:** **the
control is an ENCODING test, the value is a reader's check, and the Session conflated them.** ✓

## What Exp0 must settle

**Per the Advisor:** **Exp0 is *"classifier unit proof on known RIP↔guest pairs, incl. the `0x7B3` residual
(offline)."***

**The Session's correction refines Exp0's scope:** **there is no RIP↔guest mapping to prove — the question is
whether the NATIVE bytes at a given RIP carry the expected ENCODING.** **So Exp0 must:**

1. **locate the native instruction that performs the installer's store** (via the debug info or by
   disassembling the built binary at the recorded RIP);
2. **read its actual bytes**;
3. **run `a2h_slotw_classify_store` on them** and see whether it returns `ENC_MODRM`;
4. **do the same for the candidate `0x00199F45`** and check for `ENC_SIB`.

**And if the native encoding differs from the guest encoding, the classifier's premise is WRONG and must be
replaced** — **for example by classifying from the GUEST VA that the recompiled code corresponds to, which
the recompiler's own dispatch or a `loc_` label can supply.**

**That is a design question for the successor packet, and the Session records it as the finding rather than
prescribing the fix.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation change.** **No synthetic completion.**
**No DR record cited.** **`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**;
**`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was not reopened.** **No further ON run.**
