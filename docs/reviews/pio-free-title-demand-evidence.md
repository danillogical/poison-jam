# `PIO_FREE-title-demand-bound-r1` execution evidence — **`O-OPEN`**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/pio-free-title-demand-bound.md`, revision **`PIO_FREE-title-demand-bound-r1`**, frozen
SHA-256 **`977A96F708919F309AE01009BA64AB75AA344794950553530CCA1AF958A49ABB`** — verified before execution,
**not edited**. **Class:** discovery (§5.8).

**Selected row: `O-OPEN`** — *"Otherwise valid finite analysis cannot prove all demands ≤32, cannot exhibit a
feasible >32, or cannot reconcile a changed form / coverage under the accepted population."*

---

## Identity and provenance

| Item | Value |
|---|---|
| Game revision | `6345d279239796b98b731967c3e531e584e74c37`, clean at execution |
| Toolkit revision | `c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`, clean — **unchanged; no toolkit write** |
| XBE SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` |
| ctest | **18/18** (unchanged; no build) |
| **Toolkit/game runtime changes** | **NONE.** Write scope honoured: the two diagnostic scripts, their test module, and JSON/evidence records only |

### Diagnostic tools (the packet's Experiment 2 deliverables)

| Artifact | SHA-256 |
|---|---|
| `scripts/pio-free-demand.py` | *(recorded at freeze; see the commit)* |
| `scripts/test_pio_free_demand.py` | *(recorded at freeze)* |
| `scripts/pio-free-demand-slice.py` | *(recorded at freeze)* |

**Commands, exactly as run:**

```powershell
python -X utf8 -m unittest scripts.test_pio_free_demand
python -X utf8 scripts\pio-free-demand.py --xbe game/default.xbe --analysis game/mygame_analysis.json --gen-dir src/recomp/gen --out docs/reviews/pio-free-title-demand-sites.json
python -X utf8 scripts\pio-free-demand-slice.py --out docs/reviews/pio-free-title-demand-slice.json
```

## Experiment 1 — bind source and finite population

`inspect-jsrf.py find 0xFE820010` reports **28 operand offsets**, all `DSOUND`. **The classifier derives
instruction starts by DECODING**, never by assuming `offset−1`/`offset−2`, and **28/28 reconcile** with
`A4p`'s frozen instruction set:

```
001A2296 001A22D0 001A2A7F 001A303D 001A30E4 001A3414 001A34EA 001A35AA 001A3710 001A3847
001A38F4 001A39AE 001A3A48 001A3B69 001A3C21 001A3EB3 001A3F24 001A3FDB 001A4181 001A4242
001A4325 001A4A34 001A4E4C 001A5671 001A579E 001A57CD 001A60B8 001A611C
```

**The two-spelling hazard is closed positively.** The classifier normalises **every** `MEM8`/`MEM16`/
`MEM32` literal in `src/recomp/gen/**/*.c` to uint32 and compares, so both spellings are found by
construction: **hex 10 + decimal 18 = 28**, matching the operand population one-to-one. `-25034736` is
asserted to equal `0xFE820010` in the test suite.

> **The classifier corrected an off-by-one in the Session's earlier ad-hoc work.** The earlier census listed
> a zero-margin site at `0x001A3FDA`; decoding from a verified boundary shows the instruction is at
> **`0x001A3FDB`** (`mov eax,[0xfe820010]` follows `mov byte ptr [esi+0x65],al` at `0x001A3FD8`). **The
> tested tool is authoritative and the ad-hoc figure is superseded.**

## Experiment 2 — the tested classifier

**`python -X utf8 -m unittest scripts.test_pio_free_demand` → 29 tests, OK.**

The suite covers every fixture the packet names: positive `A1` and ModRM `disp32` encodings; operand-vs-
instruction correction; variable **`eax` and `ecx`** compare registers; masked constants **other than 4**; and
negatives for malformed analysis, missing analysis, analysis without sections, a section missing a required
key, truncated sections, a range outside any section, a missing XBE, a changed population, duplicate sites,
an unknown threshold form (`test`), no compare, a `mov` from an unrelated register breaking the stub chain, a
missing generated-source directory, and a directory with no `.c` files.

**The mid-instruction hazard is a fixture.** A `xor eax,eax` precedes an `A1` load, so a caller applying the
ModRM convention would start one byte early — the exact case that produced plausible garbage in the earlier
session. The test requires the tool to (a) find the true start via the `A1` encoding, (b) **not** report the
mid-instruction start, and (c) **raise** for a mis-derived operand VA.

**Two real defects in the classifier were found by these tests, not by inspection:**

1. **A fixed decode window overran short sections**, so a site near a section end failed classification
   instead of decoding the bytes that exist. Fixed with a section-clamped `xbe_window`.
2. **`absolute_memory_va` inspected only operand 0.** For a *load*, the memory reference is the **source**
   (`mov edx,[addr]` has the register in operand 0), so **every ModRM load was unrecognised**. Fixed by
   scanning all operands. **Without the tests this would have rejected half the population.**

## Experiment 2 — census (re-derived mechanically)

| Quantity | Value |
|---|---|
| operand offsets found | **28** |
| sites classified | **28** |
| sites rejected | **0** |
| **CONSTANT** | **13** |
| **VARIABLE** | **15** |

**Constant literals:** `0x4`×4, `0x8`×3, `0xC`, `0x20`, `0x48`, `0x4C`, **`0x80`×2**.
**Variable demand registers:** **`ecx`×11, `eax`×4**.
**Constant sites failing the stub:** **none**.
**Constant sites at ZERO margin:** **`0x001A3EB3`, `0x001A3FDB`** — both compare against `0x80`, and the
stub's masked value is exactly `0x80`, so **`0x80` passes *at equality*, not with headroom**. This is
recorded explicitly, as the packet requires: **the correct statement is "the stub passes all 13, with zero
margin at two" — never "any value ≥ 4 passes."**

**The 15/13 split, the literals, and the register split all reproduce the earlier lead**, now from a tested
extractor rather than a window heuristic. **Determinism verified:** two runs produced byte-identical output
(`A5FE6F6E…9E76C66B`).

## Experiment 3 — backward demand slice: **15 × `OPEN`**

`docs/reviews/pio-free-title-demand-slice.json`. All 15 variable gates are **backward-branching spins**
(`jb` to the poll). Slicing backward from each `cmp`'s demand operand reaches a **byte load** in every case:

| Site | demand reg | k | max demand if the byte were 255 | byte must be ≤ | verdict |
|---|---|---|---|---|---|
| `0x001A2A7F` | `ecx` | 2 | 510 | 16 | **OPEN** |
| `0x001A303D` | `ecx` | 3 | 765 | 10 | **OPEN** |
| `0x001A3414` | `ecx` | 6 | 1530 | 5 | **OPEN** |
| `0x001A34EA` | `ecx` | 9 | 2295 | 3 | **OPEN** |
| `0x001A3847` | `ecx` | 8 | 2040 | 4 | **OPEN** |
| `0x001A38F4` | `eax` | 6 | 1530 | 5 | **OPEN** |
| `0x001A39AE` | `ecx` | 2 | 510 | 16 | **OPEN** |
| `0x001A3A48` | `ecx` | 3 | 765 | 10 | **OPEN** |
| `0x001A3B69` | `eax` | 7 | 1785 | 4 | **OPEN** |
| `0x001A3C21` | `ecx` | 9 | 2295 | 3 | **OPEN** |
| `0x001A3F24` | `eax` | 7 | 1785 | 4 | **OPEN** |
| `0x001A4181` | `ecx` | 7 | 1785 | 4 | **OPEN** |
| `0x001A4242` | `eax` | 10 | 2550 | 3 | **OPEN** |
| `0x001A4325` | `ecx` | 7 | 1785 | 4 | **OPEN** |
| `0x001A4A34` | `ecx` | 2 | 510 | 16 | **OPEN** |

**`k ∈ {2,3,6,7,8,9,10}` reproduces the earlier lead**, and the byte source is the **same field at `+0x64`**
in every row (`[esi+0x64]` at 14 sites, `[ebx+0x64]` at one).

### Why `OPEN` and not `EXCEED` — the discipline the packet demands

**A byte load's generic range `0..255` bounds the demand's FINITENESS, not its sufficiency.** The packet is
explicit: *"generic `byte≤255` only establishes finiteness (`255×10=2550`), **not sufficiency**"*, and
*"On unbounded/unresolved input or uncertain feasibility, mark `OPEN` (not `EXCEED`, not `BOUND`)."*

**An earlier version of the slice returned `EXCEED` ×15 from the generic byte bound. That was wrong and is
withdrawn**: it asserted a reachable over-demand without a **feasibility** witness, and the packet states
that *"an isolated unconstrained register value is **not** a witness."* The corrected slice returns `OPEN`
with the missing witness named per site.

## What the slice did establish about the field — a real narrowing

The packet asked specifically to test the `k × byte[base+0x64]` lead, and the byte field's provenance was
traced. **`MEM8(.. + 0x64)` has exactly three writers** in the generated source:

| Writer | Form |
|---|---|
| `recomp_0002.c:56809` | `MEM8(esi + 0x64) = MEM8(esi + 0x64) \| 0x80;` — sets **bit 7 only** |
| `recomp_0005.c:10083` | `MEM8(esi + 0x64) = 1;` — constant 1 |
| `recomp_0005.c:10282` | `MEM8(esi + 0x64) = LO8(eax);` — **a full byte** |

**Tracing the `LO8(eax)` writer at `0x001A29EB`** (which sits in the same function as the first variable
poll and is compared against the field at `0x001A29D6`):

```
001A29CA  mov   ecx, dword ptr [esi + 0x78]
001A29CD  movzx eax, byte ptr [ecx + 0xe]    ; eax in 0..255
001A29D1  dec   eax                          ; -1..254
001A29D2  sar   eax, 1                       ; -1..127   (arithmetic)
001A29D4  inc   al                           ; al = ((byte-1) >> 1) + 1, i.e. 0..128
001A29EB  mov   byte ptr [esi + 0x64], al
```

**So at this writer the field is bounded by `128`, not `255`** — verified exhaustively over all 256 inputs:
`byte[ecx+0xe]=255 → dec 254 → sar 127 → al 128`.

**Consequence, stated carefully.** With `k=2` at `0x001A2A7F`, a field value above **16** would make the
demand exceed the stub's 32, and field values up to **128** are representable at this writer. **But that is
an upper bound on the field, not a witness that a poll-reaching path actually carries a value above 16.**
The source `byte[ecx+0xe]` is itself guest data loaded through a pointer (`[esi+0x78]`), so **feasibility is
not established** and the row stays `OPEN`.

**This narrows the follow-up usefully:** the question is no longer "what is the hardware capacity?" but
**"can `byte[[esi+0x78]+0xe]` exceed 32 (resp. 16) on a path that reaches the poll?"** — a finite question
about the title's own data, which is admissible evidence about the title.

## Experiment 4 — deliver and check

**Stale-source and identity checks:** XBE matches the pinned hash; game and toolkit revisions recorded;
toolkit clean and **unchanged**. **Coverage:** 28/28 operand offsets classified, **0 rejected**, so no site
was silently skipped. **Contradictions:** none — the mechanical census agrees with the earlier lead on the
15/13 split, the literals, the `ecx`/`eax` split, and `k`, while **correcting one VA** (`0x001A3FDB`).
**No trace-to-site parsing was used**: every classification comes from the checked-in tested classifier.

## Selected row: `O-OPEN`

| Row | Applicable? | Why |
|---|---|---|
| `O-IDENTITY` | **No** | XBE, route, parser identity, population and reconciliation all clean; 28/28, 0 rejects |
| `O-EXCEED` | **No** | No site has a **feasible** demand `>32` with a path/input witness; the generic byte bound is not a witness, and no constant threshold exceeds 128 |
| `O-BOUND` | **No** | Not every variable operand has a reproducible `≤32` universal bound — 15 are `OPEN` |
| **`O-OPEN`** | **YES** | Valid finite analysis that cannot prove all demands `≤32` and cannot exhibit a feasible `>32` |

**Next per the row:** **`PIO_FREE-demand-slice-followup`** discovery, restricted to the listed `OPEN` node —
namely the feasibility of `byte[+0x64]` exceeding the per-site threshold on a poll-reaching path. **If that
is not a finite resolvable leaf, the row directs an explicit scope/defer decision to the Advisor instead of
iterating.** **Hardware-model sourcing remains blocked.**

## What this establishes, and what it does not

**Establishes:** a tested, deterministic classifier; the census re-derived mechanically and reconciling 28/28
with `A4p`; **13 constant sites all passing the stub, two at exact equality**; **15 variable sites all
`OPEN`** with the per-site multiplier, byte source and the threshold each would need; and a **derived
`≤128` bound** on the byte field at one identified writer.

**Does not establish:** that any gate actually exits; that `0x80` is the true device value; any hardware
units, capacity, drain, overflow or ordering — **the five hardware leaves remain `UNKNOWN`**; boot, audio,
liveness or any strict progress. **A discovery satisfies no strict criterion** (§5.8). No synthetic
completion; the `0x80` stub was **not** changed; `0xFFFFB3` stays **`UNRESOLVED`**; `A4b2-r7`,
accepted/closed `A4b2-r8`, and `A4b1-r4` were **not** reopened.
