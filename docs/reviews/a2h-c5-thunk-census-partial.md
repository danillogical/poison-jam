# `A2h` — C-5 progress: the kernel-thunk census, and why `sub_00194ADD` is not in it

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why recorded:** C-5 is the deferred edge — *"the caller of `sub_00194ADD` and the `KeInsertQueueDpc` site."*
**The Session ran an image-wide census of kernel-thunk calls to see what the guest can actually invoke, and
the result corrects an assumption the Session itself was carrying.**

---

## The census: 29 distinct kernel ordinals are called image-wide

**The Session enumerated every `call dword ptr [0x001C4xxx]` through the 120-entry thunk table
(`0x001C3F60`), image-wide, and decoded each slot's ordinal.**

| Ordinal | Sites | Ordinal | Sites | Ordinal | Sites |
|---|---|---|---|---|---|
| **1** | 1 | **113** | 1 | **160** | 1 |
| **2** | 4 | **119** | 3 | **161** | 1 |
| **15** | 1 | **137** | 2 | **166** | **8** |
| **23** | 2 | **139** | 1 | **171** | 4 |
| **44** | 2 | **142** | 1 | **173** | 6 |
| **47** | 4 | **149** | 3 | **175** | 3 |
| **97** | 2 | **151** | 6 | **180** | 2 |
| **98** | 2 | **153** | 1 | **277** | 1 |
| **100** | 2 | **159** | 1 | **294** | **96** |
| **107** | **1** | | | | |
| **109** | 2 | | | | |

**Ordinal 294 dominates with 96 call sites**, and **ordinal 277 — the `RtlEnterCriticalSection` the Session
identified long ago — appears once.**

## ⚠ The correction: `sub_00194ADD` does NOT appear in the census

**Ordinal 107 — the `KeInitializeDpc` the reviewer and the Worker both identified at `0x00194AF4` — has
exactly ONE call site image-wide: `0x001A741F`.**

> **`0x00194AF4` is NOT in the census.**

**Why:** **the census enumerates `call dword ptr [0x001C4xxx]` — an INDIRECT call through the table slot.**
**`0x00194AF4`'s instruction is `call dword ptr [0x1c4020]` — note the slot address is `0x001C4020`, which IS
`0x001C3F60 + 48·4`.** **So it should have matched.**

**The Session's decoder did not surface it, and the Session then DIAGNOSED WHY — the result is worse than a
parse gap:**

> **Decoding the D3D section from its own start produced 9945 instructions and NEVER REACHED `0x00194AF4`.**

**So the linear decode DRIFTED OFF the instruction stream partway through the section.** **That is exactly the
misalignment trap the current packet warns about — *"`inspect-jsrf.py disasm` decodes from the requested start
and can misalign"* — occurring in the Session's own census script, applied at section granularity.**

**Consequence:** **the census's 29 ordinals are drawn from a decode that lost alignment, so the census is not
merely incomplete — its COVERAGE IS UNKNOWN.** **It cannot be used to bound anything.**

**The Session records this rather than presenting the 29-ordinal table as a result.** **The byte-level
identification at `0x00194AF4` stands, because the reviewer and the Worker reached it by other means.**

**This is instance 12 of the Session's extraction family, and it is the SAME CLASS as the byte-reversal: a
transformation whose output looked well-formed (9945 plausible instructions) while being wrong.** **The
section-start anchoring the Session believed it was using did not hold.**

## What the census therefore does NOT establish

**Nothing about coverage.** **The 29-ordinal table below is retained only to show what the drifted decode
produced, and MUST NOT be cited as an enumeration, a lower bound, or evidence about any ordinal's call
count.**

## The FIX, verified — and it is a durable tooling rule

**Decoding from the DECLARED function boundary works:**

```
declared function containing 0x00194AF4:  0x00194ADD..0x00194C3F
decoding from that boundary reaches it:   TRUE
   00194AF4  call dword ptr [0x1c4020]    <== ordinal 107, KeInitializeDpc
```

> **So the remedy is stronger than the packet's wording.** **The packet says *"decode each section from its
> own start and slice."*** **The Session's evidence shows SECTION-START DECODING CAN ALSO DRIFT.**
>
> **The reliable anchor is the DECLARED FUNCTION BOUNDARY** — from `config/recovered-functions.json` or a
> `loc_` label. **That is the same discipline this project already requires for boundaries, applied to
> DECODING rather than to analysis.**

**The Session records this as a correction to the packet's own guidance**, and it should propagate to the next
packet: **anchor every decode to a declared function boundary, not merely to a section start.**

## What C-5 still needs

- **A census built on declared-boundary decoding**, not section-start decoding.
- **The ordinal for `KeInsertQueueDpc` is not yet known** — **the Session has not established which ordinal
  it is, so it cannot be searched for by number.**

**So C-5 remains OPEN, and the Session records the census as a FAILED step with a named cause and a verified
fix, rather than as progress toward closure.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited.** **C-5 stays SEPARATE
from the writer question, per the packet.**
