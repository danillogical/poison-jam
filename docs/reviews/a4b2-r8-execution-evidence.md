# `A4b2-r8` execution evidence — **`R2-PASS`**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-gp-clears-pending-word.md`, revision **`A4b2-r8`**, frozen SHA-256
**`4D4AFC304F571971EB180C19D6832D56A2CC62FF6EAAFB9E4928716125397C62`** — verified before execution, **not
edited**. Preconditions: `docs/reviews/a4b2-r8-preconditions.md` (**P1–P4 all PASS**).

**Selected row: `R2-PASS`.**

---

## Identity

| Item | Value |
|---|---|
| Game commit | `b3f22cb210f453940953fc357f00ab4d57e7d259` |
| Toolkit (both builds) | **`c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`**, clean — the discovery-final commit (P2) |
| XBE SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` (P1) |
| **exe SHA-256 (R1 and R0)** | **`BC8E288DD54D8A09DA1630AB933EB1A808AEC9C60CEE2C64B9742B5C0CB8DC51`** |
| ctest | **18/18 passed** |
| **R1** | `logs/runs/20260927-160330-655-a4b2-gp-trap-trace` — **STRICT** |
| **R0** | `logs/runs/20260927-160335-562-a4b2-default` — **STRICT** |

**Write scope honoured:** game only; the step-1 forwarding edits were **already applied and matched the
specification**, so they were **preserved rather than duplicated**. **No toolkit file was changed.**

## The decision record

```
[GPWATCH] latch class=GP_CLEAR seq=198852 va=803C0810 observed=00000003 payload=00000000 site=00000000 frame=256 insns=124652 dsp_addr=000800
[GPWATCH] latch class=CPU_ANCHOR seq=198851 va=803C0810 observed=00000000 payload=00000003 site=001A18CE frame=256 insns=0 dsp_addr=000000
[GPBOOT] n=1 sge0=003C0000 sge0_va=803C0000 gprst=00000003 prev=00000001
[GPWATCH] counts seq=1610629 boots=1 gp_frames=768 gp_insns=33120534 GP_CLEAR=1 GP_ZERO_OVER_ZERO=0 GP_ZERO_OVER_OTHER=1 GP_NONZERO_OVER=0 GP_PARTIAL=0 CPU_ANCHOR=1 CPU_ZERO=0 CPU_ZERO_OVERFLOW=0 CPU_OTHER=0 GPIN_OUT_OF_UNIVERSE=0 frame=768
```

**The exchange tuple matches the proved exchange**: `va=803C0810`, `observed=3`, `payload=0`,
`dsp_addr=000800`. **`CPU_ANCHOR.seq=198851 < GP_CLEAR.seq=198852`.**

## Criterion results

### `AC-DEFAULT2` — **PASS**

| Check | Required | Observed |
|---|---|---|
| R0 outcome | `diagnostic_deadline` | **`diagnostic_deadline`** (31.7 s) |
| `Wf0` at `B0+0x810` | `3` | **`3`** |
| `F0` | `≥1` | **`2`** |
| `[GP*]` lines | zero | **0** for `GPBOOT`/`GPRUN`/`GPDMA`/`GPIN`/`GPWATCH` |
| `RECOMP_APU_TRAP` / `RECOMP_APU_TRACE` | both absent | **both ABSENT** |
| R0 `check-dump-mapping.py` | `matches ≥ 1`, `content-mismatch: 0` | **`matches: 1   content-mismatch: 0`** |

`F0` uses the packet's own pattern `sub_001A1769\+0x[0-9A-Fa-f]+ .*recomp_0005\.c:(L|L+1|L+2|L+3)` with
`L` from **this** build: the store is at line `6751` and `loc_001A18D0` at `6754`, so the matched `6755`
is **`L+1`** — inside the window. `F0=2` matches the accepted `A4b1-r4` oracle exactly.

### `AC-BOOT` — **PASS**

| Check | Result |
|---|---|
| `[GPBOOT]` blocks | **N=1**, structurally complete, self-numbered `n=1` |
| Last counts `boots` | **1** — **`N` reconciles** |
| `gprst` | `00000003` (GPRST and GPDSPRST both set) |
| `prev` | `00000001` — GPRST and GPDSPRST both **clear** ✓ |
| `sge0_va` | `803C0000` = translated page `T` |
| Image words `i<0x173` vs `I` | **verified** (the archived comparison: `[GPBOOT]` pram vs `I` → 0 mismatches) |

### `AC-RUN` — **PASS**

Some counts line has `boots=1 ≥ 1`, `gp_frames=768 ≥ 1`, `gp_insns=33120534 > 0`.

### `AC-CLEAR` — **PASS** (step 2)

`GP_CLEAR` has `va=B+0x810` = `803C0810` ✓, `observed=00000003` ✓, `insns=124652 > 0` ✓, a recorded
`dsp_addr=000800` ✓, and **`CPU_ANCHOR.seq=198851 < GP_CLEAR.seq=198852`** ✓. Per the packet, `F` and `Wf`
are recorded as corroboration only and R1 dump mapping was **not** run merely to qualify this PASS.

**R1's dump is displaced, and the packet's rule for that is followed exactly.** For completeness R1's
mapping was checked and reports **`CONTENT_MISMATCH`** — *"Not usable for an IMAGE-CONTENT claim"* — the
known A2h displacement. Therefore:

- **`Wf` is NOT reported as evidence from R1**, and its value is not used to route anything to
  CPU/`UNATTRIBUTED`. The packet is explicit: *"on a displaced R1 dump do not report `Wf` as evidence or
  route its apparent zero to CPU/UNATTRIBUTED."*
- **`R2-PASS` rests on the latch and the counts**, which are log-bound and unaffected by dump displacement —
  exactly as the packet's step 2 provides. **The displaced dump does not touch this row.**

This is recorded because a reader seeing `CONTENT_MISMATCH` might otherwise expect the row to be UNKNOWN;
it is not, and the reason is the packet's own precedence rule.

### `AC-NOCPU` — **PASS**

| Check | Result |
|---|---|
| `RECOMP_APU_DSP_ACK` / `dsp_ack_` in exe strings | **absent** (both `False`) |
| Same tokens in toolkit `src/apu` at `c151d4e` | **no hits** |
| `CPU_ZERO` latches | **0** — **no contested zero** |
| Six-site reconciliation | **`count found == 6 == frozen count`**, `N_SITES=16 ≥ 6` |

**Every one of the six sites reconciles with the original XBE**, each with its own producing disassembly:

| Guest VA | XBE instruction | Generated call |
|---|---|---|
| `0006DACE` | `mov dword ptr [eax + 0x810], ecx` | `jsrf_watch_store(0x0006DACEu, eax + 0x810, ecx)` |
| `0006DBBA` | `mov dword ptr [ecx + 0x810], eax` | `jsrf_watch_store(0x0006DBBAu, ecx + 0x810, eax)` |
| `0006DEF3` | `mov dword ptr [ecx + 0x810], eax` | `jsrf_watch_store(0x0006DEF3u, ecx + 0x810, eax)` |
| `001A1751` | `and dword ptr [edi + 0x810], 0` | `jsrf_watch_store(0x001A1751u, edi + 0x810, 0)` |
| `001A18CE` | `mov dword ptr [ebx], eax` (after `add ebx, 0x810`) | `jsrf_watch_store(0x001A18CEu, ebx, eax)` |
| `001A1FA7` | `mov dword ptr [edi + 0x10], ebp` (after `add edi, 0x800`) | `jsrf_watch_store(0x001A1FA7u, edi + 0x10, ebp)` |

### `AC-INPUTS` — **PASS**

**Step 1 — block integrity.** **6 `at_clear` blocks, all 6 complete**; **9 `summary` blocks, all 9
complete**. **(i)** every complete `at_clear` block is **identical to the first** (whitespace-normalised:
0 differing; numerically: 0 differences). Header `seq=198852` **matches `GP_CLEAR.seq=198852`**.
**(ii)** **0 monotonicity violations** against the last complete summary block.

**Step 2 — integrity.** `out_of_universe=0` ✓; `boot_scratch_read=1` ✓; **all FIFO slots 0** ✓.

**Step 3 — classification.**

| Class | Counter | Disposition |
|---|---|---|
| PERIPH `0x45` (`0xFFFFC5`) | 1020 | **MODELLED** |
| PERIPH `0x56` (`0xFFFFD6`) | 3065 | **MODELLED** |
| **PERIPH `0x33` (`0xFFFFB3`)** | **1017** | **EXEMPT** — two-leg non-reliance |
| **MIXBUF** | **26 bins nonzero**, `mixbuf_stub_read=1` | **EXEMPT** — two-leg non-reliance |
| DMA `CONTIG` | 2311 | guest-written **by region** |
| DMA `DEVICE` / `OTHER_MAPPED` | 0 | — |
| FIFO slots 0–1 | 0 | — |

**Every step-3 stub counter/flag outside the two proved-non-reliant classes is zero.** The two exempted
classes are recorded as **stub reads** and are **not described as modelled or guest-written**.

**Exchange-identity reconciliation (the r8-specific requirement).** The observed `GP_CLEAR` tuple
(`va=803C0810`, `observed=3`, `payload=0`, `dsp_addr=000800`), the validated image and the P4
source/bytes/guard continuity **all agree with the proved exchange**. **No discrepancy**; no new feasible
stub→clear field/guard chain was found.

## Row: `R2-PASS`

Per the packet's decision rows, `AC-DEFAULT2`, `AC-BOOT`, `AC-RUN`, `AC-CLEAR`, `AC-NOCPU` and
`AC-INPUTS` all **PASS**, so the first matching row is **`R2-PASS`**.

**What is established** — only what the packet's Claim permits: in one strict run with `RECOMP_GPU_ACK=0`,
`RECOMP_APU_TRAP=1` and observation-only APU trace, the `3→0` transition at `B+0x810` was performed by the
GP engine's memory-write path while executing validated image `I`, after the anchored store was recorded,
with no synthetic ack or instrumented competing CPU zero. The **restated input qualifier** holds: every
pre-exchange GP input on a feasible reaching chain to this clear's destination, exchanged value, payload or
write/trigger guard is guest-written by region or modelled at the pinned source, and the **two named** stub
inputs are covered by the completed `L1=PROVEN` + `L2=INVARIANT` **non-reliance** finding rather than by
pretending they are modelled or that their reads are zero.

**`A4b2-r7` remains `R2-EXPL-INPUT` — this is not a retroactive PASS of r7.**

**Not established** (the packet's own limits, restated): no guest observation of the `0`, no spin exit or
progress past `loc_001A18D0`, no boot/liveness/audio/timing/instruction-level-correctness claim, no GP→CPU
interrupt, no EP execution. `CPU_ANCHOR` is recorded **before** the store, so it corroborates the exchanged
`3`'s identity without proving it supplied that `3`. `LOW_RAM`/`CONTIG` DMA is guest-written **by region,
not by writer**. `0x56` is a **timing heuristic**, not a hardware model. `unk2`/`unk13`, the `format`
default and `dsp_offset` range fall-throughs remain **unaudited**.

## Session tooling errors found and corrected during this evaluation

Recorded because they produced **false** step-1 results that would have forced an unnecessary `R2-UNKNOWN`:

1. **Header selection matched too much.** Selecting `"[GPIN] at_clear " in l and "seq=" in l` also matched
   every `mixbuf`/`periph`/`dma` line, because those carry a `first_seq=[...]` field. This reported
   "12 headers, 4 complete blocks" and "8 blocks differ" — **all false**. Fixed by anchoring the header on
   `seq=<n> frame=<n>`.
2. **Array parsing grabbed the wrong bracket.** `\[([^\]]*)\]` took the **first** bracket on a line, which
   on an interleaved line is a foreign tag like `[APUMMIO]`, yielding `int('GPIN')`. Fixed by anchoring on
   the field name.
3. **Foreign-record interleaving splits blocks across physical lines.** An `[APUMMIO]` record is injected
   mid-array (e.g. at line 13041), so a naive line-window parse sees incomplete blocks. Fixed by
   **reconstructing** with the rule the discovery record already established: strip foreign `[TAG]` records
   **including their newline**. After reconstruction: **6/6 and 9/9 complete**.
4. **`[GPIN] summary` blocks have no `seq=` header** — a summary block *begins* with its `mixbuf` line.
   Searching for `summary seq=` found zero and produced a false "no summary block".
5. **The `GP_CLEAR` latch was deleted by my own stripper**, because `[GPWATCH]` counts as a "foreign" tag.
   The latch is the **decision record**, not an interleaved injection, so it must be read from the **raw**
   log. Fixed.
6. **`mixbuf_stub_read` is a per-frame provenance flag, not a monotone counter.** Its summary sequence is
   `0,0,1,1,1,1,1,1,1` — it legitimately takes both values. The packet's step (ii) monotonicity rule is
   stated for **counters**; the flag is read from the **write-once `at_clear` freeze**, whose value is
   `1` and never changes across all six emissions. Comparing it across time would be a category error.

**All six were parser errors in the Session's own evaluation tooling, not defects in the run.** They are
recorded because a reader must not mistake the corrected result for one that was right the first time.
