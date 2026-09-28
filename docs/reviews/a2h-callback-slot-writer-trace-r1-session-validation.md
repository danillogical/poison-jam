# `A2h-callback-slot-writer-trace-r1` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-callback-slot-writer-trace.md`, authored by Planner
`770b392f-6f8c-4b37-9a40-6076a34bf6a5` (`codex/gpt-6-sol` @ `high`), committed `ec108cf`.
**Authority:** `a2h-callback-context-producer-acceptance-record.md` (`9bcfc93`),
`a2h-acceptance-condition-a-satisfied.md` (`aed7998`), reviewer C-5
(`a2h-callback-context-producer-acceptance-review.md`, `58694a5`).

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-callback-slot-writer-trace-r1`** |
| Lines | **17** (18 with the trailing newline) |
| Bytes | **7231** |
| **SHA-256 (frozen)** | **`C5FD9CAA405819255E0FF63DE35DB21E7F41E3F16E3B392842BF5CD44CD32A76`** |

## Validation — 24 of 24 checks pass

**The Session's first pass reported one MISSING, and the fault was the Session's check string, not the
packet.** **The packet says *"their absence as proof about the slot writer"*; the Session's test looked for
*"not proof about the slot writer"*.** **Recorded because it is instance 10 of the Session's extraction
family — a literal-match check whose pattern was wrong — and because the correct response is to read the
artifact rather than to trust the check.** **The Session read line 10 and confirmed the requirement is
present.**

### The vocabulary fix is adopted — and it is the reviewer's condition (b)

| Name | Address | Role |
|---|---|---|
| **`software_device`** | **`MEM32(0x19DCE0) = 0x0019B200`** | the software NV2A device object — **the `device` in `device+0x242C`** |
| **`aperture`** | **`[context] = 0xFD000000`** | the NV2A MMIO register aperture |

**The packet states them as *"not interchangeable"*.** **And it states the ONE-slot identity:**
**`context+0x1C4 == software_device+0x242C`** because `0x2268 + 0x1C4 = 0x242C`.

### The three searches, all `loc_`-anchored and section-complete

1. **Direct stores** — every `[reg+0x242C]` across **EVERY original-XBE executable section**, with each base's
   **complete reaching definition** traced to `software_device` or refuted.
2. **Context aliases** — every `[context+0x1C4]` write, **including the DPC consumer path**.
3. **Computed stores** — `[base+index*4]`, `[base+reg]` and bulk-write destinations, **proved or excluded**
   against the slot.

**And it is explicit that generated text is for leads only:** *"Generated/recovered text can locate leads,
never establish exhaustive byte coverage or boundaries."*

### ⚠ The packet encodes BOTH of the Session's script bugs as BINDING instructions

**This is the most valuable part, and the Session did not ask for it explicitly:**

| Instruction | Which Session bug it prevents |
|---|---|
| **the `VA`/`VA+1` offset-shift control is BINDING**; *"a single-address control does NOT discriminate"* | **instance 7** — the byte-reversal |
| **parse with `int(text,16)`, NOT `int.from_bytes(bytes.fromhex(text),"little")`** | **instance 8** — the double reversal |
| **compare `(v1 & 0x00FFFFFF) == (v0 >> 8)`, not `v1 == v0 >> 8`** | **instance 9** — the too-strict shift check |

> **So the practice note's control set is now ENFORCED BY A FROZEN PACKET rather than by the Session's
> discipline.** **That is the systemic form the Advisor asked for, and the Planner produced it without
> being told the specific bugs.**

### C-5 is correctly kept SEPARATE

**Line 10 is a *"C-5 ledger, NOT this question"*:** **record the caller of `sub_00194ADD` and the
`KeInsertQueueDpc` site as UNTRACED/UNLOCATED**; **do not treat `KeInitializeDpc` as a queue operation, a
mid-instruction candidate call as an edge, or *"their absence as proof about the slot writer."***

**That last clause is exactly the right guard** — **the Session's own C-5 record (`227917a`) found
`sub_00194ADD` is a dispatch-table target with zero call sites, and the packet forbids inferring anything
about the slot writer from that.**

### The non-NULL constraint is carried, and it matters

**Line 5:** *"A zeroed slot skips the call; `0x001D5078` requires an ACTUAL reaching write, not an inferred
missing initialization."*

**So the packet forecloses the tempting shortcut** — **the NULL test at `0x00193E62` means a zeroed slot
would SKIP the call, so the filename value must have been genuinely written.** **That is a real constraint on
the search, not a restatement.**

## Freeze decision

**The packet is one-edge, correctly scoped, its input restrictions are the Advisor's, and it encodes the
Session's own failure modes as binding controls.** **It is `ADEQUATE`, validated, and frozen.**

**Nothing changes its class, rows or prohibitions.** **No synthetic completion.** **No toolkit change required
or authorized.** The producer line stays **PARKED**; `PIO_FREE` stays **DEFERRED**;
`A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**.
