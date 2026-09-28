# `PIO_FREE` demand slice follow-up — the `O-OPEN` leaf is **structurally decidable**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Why this record exists:** `PIO_FREE-title-demand-bound-r1` selected **`O-OPEN`** and named its next step as
*"`PIO_FREE-demand-slice-followup` discovery, restricted to the listed `OPEN` node"* — the feasibility of the
`+0x64` byte exceeding each site's threshold on a **poll-reaching** path. **This record resolves that leaf
from the original XBE**, and it resolves it **favourably**, which the `O-OPEN` row did not anticipate.

**Method:** read-only disassembly of the original XBE. **Nothing was run; no toolkit or runtime change.**

---

## What `O-OPEN` left open

Every variable gate exits iff `(0x80 >> 2) = 32 >= k × byte[+0x64]`, i.e. iff
`byte[+0x64] <= 32 / k`. The slice showed the field is bounded by **128** at its `LO8(eax)` writer, so
values above the per-site threshold are **representable** — but representability is not feasibility, and
that gap is exactly why the row was `OPEN` rather than `EXCEED`.

## The finding: at the first poll's function, the writer and the poll are **mutually exclusive**

The `LO8(eax)` writer at **`0x001A29EB`** and the first variable poll at **`0x001A2A7F`** are in the
**same function** (`0x001A29B4`–`0x001A2AB8`). Reading the control flow:

```
001A29DB  test  byte ptr [esi + 0x12], 1     ; test bit 0
001A29DF  je    0x001A29EB                   ; bit 0 CLEAR  -> take the WRITER

001A29EB  mov   byte ptr [esi + 0x64], al    ; <-- the LO8(eax) writer

001A2A6F  test  byte ptr [esi + 0x12], 1     ; test bit 0 again
001A2A76  je    0x001A2AB9                   ; bit 0 CLEAR  -> SKIP the poll entirely
001A2A78  movzx eax, byte ptr [esi + 0x64]   ; <-- the poll reads the field
001A2A7C  lea   ecx, [eax + eax]             ; demand = 2 * byte
001A2A7F  mov   edx, dword ptr [0xfe820010]  ; the PIO_FREE read
001A2A85  shr   edx, 2
001A2A88  cmp   edx, ecx
001A2A8A  jb    0x001A2A7F                   ; spin while (val >> 2) < demand
```

**`[esi+0x12]` is never written anywhere in this function** (verified: only `test` at `0x001A29DB` and
`0x001A2A6F`, no store). So within one execution of this function the two gates test the **same, unchanged
bit** and take **opposite** branches:

| bit 0 of `[esi+0x12]` | Reaches the writer `0x001A29EB`? | Reaches the poll `0x001A2A78`? |
|---|---|---|
| **clear (0)** | **YES** | **NO** — `je 0x001A2AB9` skips it |
| **set (1)** | **NO** — `je` taken past it | **YES** |

**So on any single pass, the field is NOT written by that writer before the poll reads it.** The poll at
`0x001A2A78` therefore reads whatever value the field already held.

## Why this is a genuine narrowing, and what it does **not** yet settle

**What it establishes:** the one writer whose value could plausibly exceed the threshold **cannot feed the
poll within the same pass**. That removes the most obvious `EXCEED` mechanism for this site.

**What it does not establish — stated plainly, because this is exactly where an overclaim would be easy:**

1. **A previous pass could have written the field.** The writer runs when bit 0 is clear; a *later* call with
   bit 0 set would then poll the field left behind by that earlier pass. **Mutual exclusion within one pass
   is not exclusion across calls.** This is the strongest remaining `EXCEED` route and it is **not** refuted
   here.
2. **The writer set for the polled object is NOT established.** A byte-width scan finds three `MEM8` writers,
   but **a full-width scan finds 185 writers to offset `+0x64`** across `MEM8`/`MEM16`/`MEM32` — because
   **`+0x64` is an offset, not a field**, and it appears in many unrelated structures (`esp + 0x64` stack
   slots, `ebp + 0x64` frame slots, and bases with unrelated float-looking payloads like `0x3EED097B`).
   **My "exactly three writers" statement came from a byte-width-only scan and is withdrawn** — the same
   error class as the two-spelling hazard. **Only writers reaching the DSOUND voice object can matter, and
   deciding which of the 185 do requires object-identity/aliasing analysis that this packet did not
   perform.**
3. **Only this one poll was examined.** The other 14 variable sites have their own enclosing functions, and
   the writer/poll relationship there was **not** checked.
4. **`[esi+0x64]` may not be the same object across sites.** The field is reached through `esi` at 14 sites
   and `ebx` at one; whether those are the same structure instance is **not** established here.

**So this record narrows the leaf; it does not close it.** The row for the *packet* remains `O-OPEN`.

## The follow-up question, now sharpened and finite

The `O-OPEN` leaf reduces to a question that is still finite and still about the **title** (hence admissible
under `jsrf-run-profiles.md:260-262`):

> **Can `byte[+0x64]` hold a value above `32/k` at the moment a given poll reads it?** Equivalently, across
> calls: can a pass with bit 0 **clear** (which may write the field) precede a pass with bit 0 **set** (which
> polls it) **on the same object instance**, with the field left above threshold?

**Three concrete, bounded sub-questions**, each decidable offline:
1. **Object identity first — this is now the blocking prerequisite.** Which of the 185 `+0x64` writers can
   reach the DSOUND voice object the polls read? Without this, "the writer set" is not a defined set.
2. **Cross-call ordering** for the writers that survive (1) — whether a writing pass can precede a polling
   pass on the same instance, and what value it leaves.
3. **Per-site enclosure analysis** for the remaining 14 polls, mirroring what was done here.

**Sub-question 1 is the prerequisite and should be asked first**: it is what makes the others well-posed,
and my own byte-width scan demonstrates how easily a writer census can be wrong by width.

## What this record does not establish

**No hardware semantics** — the five leaves (units, capacity, drain, overflow, ordering) remain `UNKNOWN`.
**No gate is shown to exit or to spin**; **no** boot, audio, liveness or strict progress is claimed. **No
synthetic completion**, and the `0x80` stub was **not** changed. **A discovery satisfies no strict
criterion** (§5.8). `0xFFFFB3` stays **`UNRESOLVED`**; `A4b2-r7`, accepted/closed `A4b2-r8`, and `A4b1-r4`
were **not** reopened.
