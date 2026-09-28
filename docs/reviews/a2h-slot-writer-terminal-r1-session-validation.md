# `A2h-slot-writer-terminal-r1` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-slot-writer-terminal.md`, authored by Planner
`a7eea818-c93e-460c-80f0-ddd791c74dfa` (`codex/gpt-6-sol` @ `high`), committed `da63d34`.
**Authority:** `a2h-completeness-review-closed.md` (turn `01a0e89e`) — **the terminality bound.**

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-slot-writer-terminal-r1`** |
| Lines | **15** (16 with the trailing newline) |
| Bytes | **6036** |
| **SHA-256 (frozen)** | **`116884E8D474A0238096ACD43111E089E5A1A89C87F861E9FCBA953B95E05E9F`** |

## Validation — 23 of 23 checks pass

### The terminality bound is encoded exactly

| Bound | Packet |
|---|---|
| **ONE bounded terminal packet** | *"ONE bounded terminal packet, **not permission to execute further packets automatically**"* |
| **only GAP A and GAP B authorized** | *"Only GAP A and GAP B below are authorized; **no new methods or new enumeration**"* |
| **seed failure ⇒ `O-OPEN` + STOP** | *"Failure of seed coverage is `O-OPEN` + STOP, **never a chain**"* |
| **close ⇒ separate later decision** | *"CLOSE writer ⇒ **mechanism consideration is a SEPARATE LATER DECISION**"* |
| **new gap ⇒ PARK + re-refer** | *"PARK, **RE-REFER for scope** (pivot/retire/other)"* |
| **NEVER auto-chain** | stated in caps |
| **no image-wide re-enumeration** | *"No image-wide re-enumeration authorized"* |

### The binding scope is exact

- **`DIRECT PATHS ONLY`** — *"EDGE 1/index-51 dispatch and direct-store path on accepted `software_device`
  identity."* ✓
- **Alias identity as a DOWNSTREAM DEPENDENCY** — *"EDGE 4 descriptor path to the slot is refuted, **but alias
  IDENTITY remains OPEN: record only as DOWNSTREAM DEPENDENCY, never premise it**."* ✓ **This carries the
  Advisor's precision exactly, and it is the distinction the Session itself had blurred.**
- **Read-side linkage likewise downstream** ✓

### The §6.1 method rule is encoded WITH both demonstrations

**Line 11 states the rule and cites the evidence:** *"Linear `.text` decode yielded 29 548 plausible
instructions yet reached neither `0x000D4DA0` nor `0x00199F45`; aligned dword scan found zero vtable
references where unaligned `C7 06 70 12 1E 00` immediates establish three."*

**And it states the refinement:** *"Raw-byte scans prove existence independent of alignment; **uniqueness
needs coverage of every instruction encoding capable of carrying the pattern**."* ✓

### The controls and vocabulary

**All four controls encoded:** the `VA`/`VA+1` offset-shift, `int(text,16)`, the masked shift agreement, and
the `disasm` drift warning. **The 23/24 census is BARRED per-row. The vocabulary distinguishes
`software_device` from `aperture`. C-5 stays separate. No DR record.**

## ⚠ THE SESSION'S GAP A LEAD — the executor must have it

**After the packet was drafted, the Session attacked GAP A by a DIFFERENT method than the Worker's and found
a lead the Worker's probe missed.** **Record:** `docs/reviews/a2h-gapA-3c8-read-cluster.md` (`4786c19`).

**Method — chosen to avoid the drift defect:** **a raw byte scan for the disp32 encoding `C8 03 00 00`**,
**then classifying each occurrence by decoding the instruction that contains it** (trying starts 1–3 bytes
before, since a ModRM `disp32` operand is preceded by opcode + ModRM). **Alignment-independent for
existence, with the encoding coverage STATED.**

**What it found:**

| Result | Value |
|---|---|
| **occurrences image-wide** | **76** |
| **object-field accesses** | **overwhelmingly READS, in ONE region** |
| **`[esi + 0x3c8]` reads in `0x0005F6E5..0x0005F92B`** | **25** |
| **the containing function** | **`sub_0005F6B0`**, `0x0005F6B0..0x0005F96D`, **701 bytes**, **containing 24 of the 25** |

> **A 701-byte function whose body is dominated by repeated reads of ONE object field is the DISPATCHER
> CANDIDATE** — **and it has a DECLARED boundary from `recomp_0000.c`'s `Original:` header, so it is
> decodable without drift.**

**And it CONFIRMS the Worker's own caveat:** **the Worker's recursive-descent probe returned ZERO sites
across 39 functions touching `+0x3C8`, while this enumeration finds accesses it could not reach** — **so its
seeding gap is REAL rather than merely suspected.**

### ⚠ What the lead does NOT establish — and the executor must bind it

1. **Whether this `+0x3C8` is the SAME field** that `0x000D4684` installs `0x257DE0` into. **`+0x3C8` is an
   OFFSET, and different object types can share an offset.** **The Session did NOT establish that the object
   carrying vtable `0x1E1270` is the `esi` at `0x0005F6E5`.** **Binding that is the executor's step.**
2. **Whether the cluster calls entry 8 (`0x000D4DA0`).**
3. **Whether the cluster is reachable from the failing path.**

**And the Session explicitly did NOT conclude that `+0x3C8` is one field because it has one offset** — **that
is the offset-arithmetic inference the Advisor barred for `device+0x242C`.**

**⚠ And a boundary-source note:** **`0x0005F6E5` is NOT in `config/recovered-functions.json`.** **The
boundaries above come from the GENERATED source's `Original:` headers, which are themselves declared.**
**The executor should state which source it used.**

## Freeze decision

**The packet is terminal, correctly bounded, its controls are binding, and it carries the Advisor's
precisions faithfully.** **It is `ADEQUATE`, validated, and frozen.**

**Nothing changes its class, rows or prohibitions.** **No synthetic completion.** **No toolkit change required
or authorized.** The producer line stays **PARKED**; `PIO_FREE` stays **DEFERRED**;
`A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**.
