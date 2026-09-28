# ✅ `A2h` — the competitor's IDENTITY: **`sub_00038530`** — and the mapping method's proven limit

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why recorded:** Exp1 (`75ee2e4`) observed a second writer at native `0x00007FF606B5FE38` writing
`0x001D5078` to the slot. **The Session mapped it to a guest function using the archived linker map, and
validated the method against a KNOWN control.**
**Tool:** `scripts/a2h-map-rip.py` (new, checked in).

---

## THE RESULT

**Using the run's archived `jsrf_recomp.map` (the run's own symbol file, archived by the collector):**

| RIP | Map symbol | Guest VA |
|---|---|---|
| **the competitor `0x00007FF606B5FE38`** | **`sub_00038530`** | **`0x00038530`** |
| **the installer `0x00007FF60716D82D`** | **`sub_0018CE30`** | **`0x0018CE30`** ✓ |

> ## **THE COMPETITOR IS THE RECOMPILED BODY OF GUEST FUNCTION `sub_00038530` — `0x00038530..0x0003885D`, 813 bytes, 308 instructions, `cdecl`, 0 params, `fpo_leaf`.**

**And the mapping method is VALIDATED by the control:** **the installer's RIP maps to `sub_0018CE30` — the
EXACT function the packet names as the installer.** ✓ **So the method places a RIP in the right function.**

## ⚠⚠ AND THE SAME CONTROL PROVES THE METHOD'S LIMIT — the OFFSET is NOT guest-meaningful

**The installer's delta is `+0x5D`**, which would give guest `0x0018CE30 + 0x5D = 0x0018CE8D`.** **But the
packet establishes the installer's WRITE is at guest `0x0018CE3A`** — **a delta of `0x3A`.**

> **So the SYMBOL is right and the OFFSET is WRONG by `0x23`.** ✓

**And the competitor's own numbers confirm the reason independently:**

| Quantity | Value |
|---|---|
| **the guest function's size** | **813 bytes (`0x32D`)** |
| **the competitor's native delta** | **`0x398` = 920 bytes** |
| **920 > 813** | **⚠ the native offset EXCEEDS the guest function's entire length** |

> ## **So a native interior offset CANNOT be a guest interior offset — the native body is GENERATED C, roughly 5.5× the guest size (`0x11B0` native for `0x32D` guest).**

**⚠ THE CORRECT CLAIM IS THEREFORE ONLY THIS:** **the writer RIP is INSIDE THE RECOMPILED BODY OF guest
function `sub_00038530`.** **It is NOT "at guest offset N of `0x00038530`."** **The Session does NOT make
that claim.**

**And this is the paradigm correction applied correctly:** **the Session did NOT subtract a guessed image base
to produce a guest VA.** **It used the run's own archived symbol table, and it validated the method against a
known answer — which is how the limit was found rather than assumed away.** ✓

## What `sub_00038530` is — read from the generated source

**The Session searched the function's 493-line body for `0x1D5078`, `0x242C` and `MEM32(0x19DCE0)`: ZERO
direct hits.**

**So the store is through a COMPUTED pointer** — **consistent with the observed `fault_va=0x001AD62C` being
reached via a base register, and consistent with the static finding that `0x00199F45`'s store used
`[esi+ebp*4+0x3EC]`.** **The Session does NOT claim which instruction it is** — **mapping the native
instruction to its guest source line needs the PDB, not the map.**

**What the function's head shows:** **it takes its argument in `eax`, reads a pointer from `esp+0xC`, and
writes a structure at `ebx+4`, `ebx+0x9C`, `ebx+0xA0`, `ebx+0xA4`…** — **a structure-initialisation shape.**

## ⚠ AND THIS REFUTES THE STATIC CANDIDATE, which is the substantive result

**The packet's question was whether `0x00199F45` writes the slot.**

> **The observed competitor is inside `sub_00038530` (`0x00038530`), NOT `sub_00199DB0` (`0x00199DB0`) — the
> function holding the candidate `0x00199F45`.**

**So the static candidate is NOT the writer, and the writer is a function the static sweep never named.**
**That is a NEW function for this line, and it is the answer to the `(a)`/`(b)` fork's *"who"* half.** ✓

## The Session's verification, stated with its limits

| Claim | Status |
|---|---|
| **the competitor's RIP maps to `sub_00038530`** | ✅ **via the run's own archived map** |
| **the mapping method places the INSTALLER correctly** | ✅ **`sub_0018CE30`, the named installer** |
| **the method's OFFSET is not guest-meaningful** | ✅ **proven by the `0x23` control discrepancy AND by `920 > 813`** |
| **the competitor is NOT `sub_00199DB0`** | ✅ **different symbol** |
| **the competitor's exact guest instruction** | ❌ **NOT established — needs the PDB, not the map** |
| **whether `sub_00038530` is a writer on every run** | ❌ **one run only** |

## ⚠ The tool's own defect, found and fixed in place

**The Session's first parser scanned for "the first 16-hex-digit token" per map line and reported the symbol
as `f`** — **a column of the map's `Lib:Object` field.** **The map's column order is `Address`, `Symbol`,
`Rva+Base`, so the symbol is `parts[1]` and the address `parts[2]`.**

**The Session caught it because the symbol came back as `f`, which is not a symbol.** ✓ **Fixed by indexing
the columns explicitly, and the tool now reports `sub_00038530`.**

**The Session records this as the SAME extraction family it has hit repeatedly:** **a parse that produced
plausible-looking output (`f`, a real token) from the wrong field.** **The tell was that the output was
IMPLAUSIBLE as a symbol name** — **and the fix was to index columns rather than pattern-match.**

## What the successor needs

1. **The exact guest instruction** — **map the native RIP through the PDB, not the map**, since the map gives
   the FUNCTION and not the instruction.
2. **`sub_00038530`'s role** — **what calls it, and what the structure at `ebx` is.**
3. **A run with `unknown=0`** and **a terminal of `0x001D5078`** — **Exp1 had `unknown=264` and terminal
   `0x00000000`, so it is `INFRA FAILURE` by the packet's rule.**
4. **⚠ And the zeroing between `seq=3` and the terminal** — **`term_slot=00000000`** — **is still unexplained.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit beyond the new tool.** **No synthetic completion.**
**No DR record cited.** **No fault-RIP encoding cited.** **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was
not reopened.**
