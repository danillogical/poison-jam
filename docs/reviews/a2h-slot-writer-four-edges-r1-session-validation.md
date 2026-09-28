# `A2h-slot-writer-four-edges-r1` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-slot-writer-four-edges.md`, authored by Planner
`09ddf607-d967-4f68-9999-027ae3ace038` (`codex/gpt-6-sol` @ `high`), committed `f2935d9`.
**Authority:** `a2h-callback-slot-writer-trace-acceptance-record.md` (`4c36bcc`),
`a2h-thunk-parent-dispatch-table.md` (`42dd8db`), `a2h-edge2-verified-from-bytes.md` (`948ad9c`).

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-slot-writer-four-edges-r1`** |
| Lines | **17** (18 with the trailing newline) |
| Bytes | **7947** |
| **SHA-256 (frozen)** | **`6C207AC90B853744A9C89B71C03187F5E000F0820C2D5D881994B4183A13FB9E`** |

## Validation — all checks pass

### The four edges, all carried, with EDGE 1's argument CORRECTED

| Edge | Packet's statement |
|---|---|
| **1 — `0x00199F45`** | `mov [esi+ebp*4+0x3ec],eax`, slot iff **`ebp = 0x810 = 2064`**. **And the packet states the ARG1 correction explicitly:** *"At `0x00199DC3` `[esp+0x20]=[E+0xC]` is ARG3, but after TWO more pushes at `0x00199DD9` the SAME displacement is `[E+4]` = ARG1; thus `ebp=arg1`… **tracing only ARG3 does not close this edge.**"* |
| **2 — `0x0018DF59`** | `mov [edi+esi*4+0xa78],ebx`, `edi = MEM32(0x19DCE0)`, `esi = arg1` unbounded, **`ebx` arbitrary caller word**; **`0x242C−0xa78=0x19B4=6580=4×1645`**; **"NEVER exclude as nondivisible."** |
| **3 — vtable dispatch** | **index 51 of the 52-entry vtable at `0x001E1270`**, installed at `object+0x00` **on teardown** by `0x00152244`; **"determine what the dispatch caller supplies as worker ARG1, not ARG3."** |
| **4 — stored context alias** | `0x00192562 add ecx,0x2268` / `0x00192568 mov [eax+0xc],ecx`; **"a scan for literal `0x2268` cannot exclude such writes."** |

**The packet uses the CORRECTED index 51 / 52 entries, not the Session's superseded index 39 / 40.** ✓

### The controls, all encoded as BINDING

- **`VA`/`VA+1` offset-shift**, requiring `30766A64` then `3030766A`, with *"one-address control does NOT discriminate."* ✓
- **`int(text,16)`, NOT `int.from_bytes(bytes.fromhex(text),"little")`.** ✓
- **`(v1 & 0x00FFFFFF) == (v0 >> 8)` gives `0x0030766A == 0x0030766A` and PASSES** — **with the explicit instruction *"do not drop the `0x30` byte"*, which is the exact error the previous executor made.** ✓
- **The disasm-alignment trap:** *"`inspect-jsrf.py disasm <start> <end>` decodes from the REQUESTED start and can misalign"* — **with the remedy: decode each section from its own start and slice.** ✓
- **X-flagged sections including `.rdata`/`.data` must be swept.** ✓
- **Per-run gate before ANY dump read; the 23/24 census BARRED per-row.** ✓

### C-5 stays separate, and the vocabulary is fixed

**Line 13:** *"caller of `sub_00194ADD` is `0x00192135`; `KeInsertQueueDpc` site UNLOCATED. **Neither is this
writer question**; do not conflate DPC initialization with queuing or use C-5 to select a row."* ✓

**Line 4:** **`software_device` and `aperture` distinguished, *"NOT interchangeable"*, and the ONE-slot
identity stated.** ✓

**And line 4 carries the non-NULL constraint:** *"zero skips the call, so require a real write."* ✓

## ⚠ The Planner caught a staleness defect in the SESSION's own record

**The Planner flagged it unprompted:** *"vtable review retains stale earlier index-39 narrative in lower
sections; packet uses its corrected index-51/52-entry finding."*

**The Session verified and FIXED it.** **`a2h-thunk-parent-dispatch-table.md` still carried, in its
"Established" list:**

- ~~*"the table is `0x001E12A0..0x001E133C`, 40 entries"*~~ → **corrected to `0x001E1270..0x001E133C`, 52 entries**;
- ~~*"`0x00153790` is the LAST entry (index 39)"*~~ → **corrected to index 51 of 52**;
- ~~*"what dispatcher reads the table at `0x001E12A0`… for entry 39"*~~ → **corrected to index 51, and the argument question corrected to ARG1.**

**The Session had corrected the table's extent in one section and left the superseded figures standing in
another.** **Recorded because it is the same failure pattern: a correction applied where the error was found
and not propagated to the rest of the record.** **The Planner's habit of reading the Session's records
critically is what caught it.**

## Freeze decision

**The packet is four-edge, correctly scoped, its controls are binding, and it carries the acceptance
corrections faithfully.** **It is `ADEQUATE`, validated, and frozen.**

**Nothing changes its class, rows or prohibitions.** **No synthetic completion.** **No toolkit change required
or authorized.** The producer line stays **PARKED**; `PIO_FREE` stays **DEFERRED**;
`A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**.
