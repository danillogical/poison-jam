# `A4b2-NR-epoch-slice-followup-r1` execution evidence — the backward slice

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-nr-epoch-slice-followup.md`, revision
**`A4b2-NR-epoch-slice-followup-r1`**, class **discovery**, frozen SHA-256
**`03CE475DA2F48618BE93E6803F3168732A416DC38D00C4E649252BD87F6FEBA6`** — verified before and after
promotion, **not edited**.
**This is the FINAL `L1` packet** (Advisor terminality). Verification:
`docs/reviews/a4b2-nr-epoch-slice-followup-r1-session-verification.md`.

**This is exploratory/diagnostic evidence — knowledge only, never acceptance of a strict criterion.**

---

## The slice: all five named doorbell fields CLOSE as immediates

The Advisor's method: seed at the five field definitions and the trigger guards, expand **only** through
may-reaching definitions, classify every frontier leaf. Result:

| # | Field | Seed | Reaching definition | Leaf class |
|---|---|---|---|---|
| 1 | guest VA / destination | `P 00E0 move a,x:(r0+0)` | `P 00DE or #$4000,a` ← `P 00DC and #$3fff,a` ← `P 00DB move r0,a` | **CLOSED** — derived from `r0`, itself an immediate |
| 2 | descriptor word 1 | `P 00E3 move a,x:(r0+1)` | `P 00E1 move #$0059e0,a` | **CLOSED — immediate `0x59E0`** |
| 3 | length | `P 00E4 move r3,x:(r0+2)` | `P 0006 move #$06,r3` | **CLOSED — immediate `0x06`** |
| 4 | descriptor word 3 | `P 00E6 move r1,x:(r0+3)` | `P 0003 move #$00,r1` | **CLOSED — immediate `0x00`** |
| 5 | **DSP source** | `P 00E8 move r2,x:(r0+4)` | `P 0004 move #$000800,r2` | **CLOSED — immediate `0x000800`** |

**Field 5 is the decisive one:** the DSP source address is the immediate `#$000800`, which is exactly the
`dsp_addr=000800` observed at the exchange. **No stub input appears anywhere in the five fields' reaching
definitions.**

### The trigger guards

| Guard | Site | Executions |
|---|---|---|
| DMA trigger call | `P 00C3 jsr p:$00d2` | ×1 |
| DMA control `0xFFFFD7` | `P 00D4 movep #$000000,x:$ffffd7` | ×1 |
| DMA source `0xFFFFD5` | `P 00D8 movep a,x:$ffffd5` | ×1 |
| **the exchange `0xFFFFD4`** | `P 00D9 movep a,x:$ffffd4` | ×1 |

## The stub inputs do not reach the slice

**`0xFFFFB3` (four sites, ×767 each)** — `P 002D`, `0033`, `003A`, `004D`. Each value goes to `x0`/`a1` and
is stored to **`x:$007c`–`x:$007f`**, then used only by the `cmpu`/`blt` loop control at `P 003E`/`003F`,
`0051`/`0052`, `0059`/`005A`. **None reaches a slice node.**

**MIXBUF** — the mixbin table `P 00CC`–`00D1` (six data words, MIXBUF addresses) feeds `movem p:(r2)+,x0`
in the `P 009E` loop, which builds the **audio output descriptor** at `r4=0x25` — **not** the doorbell's
descriptor at `r0=6`. The packet requires analysing the mixbin path **only at its interface** (X-words
registers actually read by worklist nodes); **no worklist node reads a mixbin-derived X word.**

## A Session tooling defect found and fixed, and it mattered

**The first version of this slice reported `r1`, `r2` and `r3` as `UNRESOLVED` for fields 3, 4 and 5** —
i.e. it failed to close three of the five fields, including the decisive DSP source.

**Cause:** the seeds are **inside** the builder body (`P 00E0`–`P 00E8`), and the builder's own `rts` sits
at `P 00DA`. Walking backwards from a seed hit that `rts` and stopped **before reaching the caller**, where
`r0`–`r3` are actually set. **Fixed** by walking back from the **call site** (`P 0007`) for the register
fields, since the builder takes its field values from the caller's registers.

**This is the third occurrence of the same class of error in this session** — the earlier CFG had a `pc+1`
fall-through bug, and the disjointness work had a `pc-1` walk bug. All three are the same mistake:
**ignoring that DSP56300 instructions are 1..*N* words and that a routine's registers are set by its
caller.** Recorded because an `UNRESOLVED` that is a tool artifact, not a program property, would have
produced a false `O-INCONCLUSIVE` here.

## The reader question — CLOSED, after correcting a classification error of mine

The slice's one remaining gap was whether a computed pointer could read one descriptor region **as** the
doorbell's. Closing it required enumerating **true readers** — and my first attempt got this wrong.

**My error:** I listed `P 00A4 move x0,x:(r1)` and `P 00BD move b,x:(r1)` as "computed readers". **They are
writes** — their X operand is the **destination**, after the comma. Same for the `x:(r4)+` sites. **An
instruction whose X operand is the destination is not a reader**, and mixing the two inflated the apparent
gap from one site to eleven.

**Corrected enumeration — every X-memory READ operand in image `I` (source position, before the comma):**

| # | Site | Operand | Executions |
|---|---|---|---|
| 1–4 | `P 002D`, `0033`, `003A`, `004D` | `x:$FFFFB3` (the stub) | ×767 each |
| 5 | `P 003C` | `x:$007D` | ×767 |
| 6 | `P 004F` | `x:$007F` | ×767 |
| 7 | `P 0060` | `x:$0004` | ×767 |
| 8 | `P 007D` | `x:$0001` | ×1 |
| 9 | `P 008A` | `x:$0000` | ×2 |
| 10 | `P 008D` | `x:$0002` | ×2 |
| 11 | `P 008F` | `x:$0003` | ×2 |
| **12** | **`P 00B9`** | **`x:(r1)` — the ONLY computed read** | ×6 |

**Decisive result:** **exactly one computed X reader exists in image `I`** — `P 00B9 move x:(r1),b` — and its
effective address is **measured** at `0x24` on all six executions (`events==execs==6`). **Zero direct reads
land in any descriptor region** (`x:[6..10]`, `[12..16]`, `[18..22]`, `[24..28]`, `[30..34]`).

**`r1`'s own definitions** are all immediates or a mailbox-derived `a`: `P 0003` (`0x00`), `P 000A` (`0x00`),
`P 0013` (`0x1560`), `P 0079` (`0x2971`), `P 008C` (`a`, from the mailbox path), `P 00A2` (`0x24`). Only the
`P 00A2` value is live at `P 00B9`, and `0x24` is **outside every descriptor region**.

**So the reader cannot alias the doorbell's descriptor on this evidence**, and the residual is now limited
to the feasibility caveat the packet itself imposes: one measured run is not a proof over all mailbox
values — but note that `P 00B9`'s `r1` is an **immediate** (`P 00A2`), not mailbox-derived, so the value is
statically fixed at `0x24` for this path regardless of the mailbox.

## Fault vectors — checked on their merits, not excluded because unobserved

The packet requires: *"do not exclude fault vectors merely because they were not observed."* So they were
checked individually. The toolkit's table has exactly four entries:

| Vector | Address | Instruction there | Executed? |
|---|---|---|---|
| **Reset** | `0x00` | `jsr p:$0155` | **YES, ×1 — this IS the entry** (`first_pc=0000`) |
| **Stack Error** | `0x02` | `move #$06,r0` | address executed ×1 — **but as entry-sequence fall-through, NOT as a dispatch** |
| **Illegal** | `0x04` | `move #$000800,r2` | address executed ×1 — **likewise fall-through, not a dispatch** |
| **Trap** | `0x08` | *(no decoded instruction)* | **not executed** |

**The distinction that matters.** `0x02` and `0x04` are executed because the **normal entry sequence falls
through them** — `P 0000 jsr`, then `P 0002 move #$06,r0`, `P 0003`, `P 0004 move #$000800,r2` are the
doorbell's own descriptor setup, which this slice already traced. **They are not vector dispatches.** Reading
"EXECUTED" as "the fault fired" would be exactly the kind of misreading this session has already made twice
(operand-scan MIXBUF, and readers-vs-writers), so it is called out.

**Feasibility of each fault:**

- **Reset** — not a fault here; it is the entry, measured `first_pc=0000`.
- **Stack Error** — requires stack imbalance. The image has 27 `jsr`/`bsr` and 21 `rts`; the executed path's
  CFG has **0 unknown control transfers**, and the toolkit's stack has explicit bounds checks. Not raised.
- **Illegal** — requires an undecodable opcode. The `op =` diagnostic fires **0 times** in execution runs,
  against the decode run's **6× positive control** (the `P 00CC`–`00D1` data table being decoded as
  instructions by the full-PRAM decoder, **not executed**). Not raised.
- **Trap** — **no raiser exists in the toolkit** for either core.

**So no fault vector can dispatch into slice state**, and the reasoning is per-vector rather than
"we didn't see it".

## Entry set — PROVEN, after a tooling bug that had fabricated five false edges

The packet's `PROVEN` bar requires an **entry-set proof**. Two questions decide it: can control reach the
doorbell's builder call by any path other than the entry sequence, and can the entry sequence re-run?

**Result, with the corrected analysis:**

| Check | Result |
|---|---|
| Incoming edges to `P 0000` (reset vector) | **0 — reachable ONLY as the reset entry** |
| Incoming edges to `P 0007` (the builder call) | **1** — fall-through from `P 0006` |
| The chain `P 0000 → 0002 → 0003 → 0004 → 0006 → 0007` | **each step has exactly one incoming edge** |
| Executions | `P 0000`, `P 0002`, `P 0004`, `P 0006`, `P 0007` all **×1** |

**So the entry sequence runs exactly once, and the descriptor's field registers are fixed by it.** No other
context can reach the builder call with different values.

### The bug that nearly produced the opposite answer

**My first entry-set analysis reported five incoming edges to `P 0000`** — from `P 003F`, `0052`, `005A`,
`0064`, `00FD`. That would have meant the entry sequence could re-run and rebuild the descriptor.

**It was a regex artifact.** The disassembler prints 24-bit addresses as **six** hex digits, and my pattern
captured only **four**:

| Instruction | Real target | What my 4-digit pattern read |
|---|---|---|
| `blt p:$000044` | `0x44` | **`0x0000`** |
| `blt p:$000029` | `0x29` | **`0x0000`** |
| `beq p:$00009d` | `0x9d` | **`0x0000`** |
| `brclr #4,x:$ffffd6,p:$0000fd` | `0xfd` | **`0x0000`** |

Every truncated target became `0x0000`, **fabricating five false edges into the reset vector.** Corrected by
matching 1–6 hex digits: **incoming to `P 0000` is 0.**

**This is the fourth occurrence of the same class of error in this session** — after the `pc+1`
fall-through bug, the `pc-1` walk bug, and the readers-vs-writers misclassification. All four are a
tooling assumption silently producing a *plausible-looking* wrong answer. Recorded because two of the four
would have changed a row selection if they had stood.

## Shared-state reaching definitions — the trigger's `a` also closes as an immediate

The Advisor requires **shared-state (X/registers) reaching-defs**, not just the five descriptor fields. The
trigger writes `a` to `0xFFFFD5` and `0xFFFFD4`; if that `a` derived from a stub input, the trigger would be
stub-dependent even with immediate fields.

**Traced:**

```
P 00C1  move #$000025,a      ; a := 0x25   (the descriptor pointer for this frame)
P 00C3  jsr p:$00d2          ; -> bsr p:$010a (DMA_CONTROL handshake), then:
P 00D4  movep #$000000,x:$ffffd7
P 00D6  and #$3fff,a         ; a := 0x25 & 0x3fff  -- still an immediate-derived value
P 00D8  movep a,x:$ffffd5    ; DMA source
P 00D9  movep a,x:$ffffd4    ; the exchange
```

**The chain closes as an immediate**: `a` comes from `P 00C1 move #$000025,a`, masked by `and #$3fff,a`.
**No stub input participates.** The `bsr p:$010a` handshake writes only `x:$FFFFD6` (DMA_CONTROL) and polls
it — device state, not stub data.

## The consumer investigation — it did NOT close, and it raises a question that may matter

I said above that the descriptor's **consumer** was the one thing this work did not verify. I then went and
checked it. **It did not resolve cleanly, and what I found is material enough that it changes my
recommendation.**

### What the consumer chain is (verified from toolkit source)

```
P 00D9  movep a,x:$ffffd4          ; a = 0x25 & 0x3fff = 0x25
   dsp.c:117-118   case 0xFFFFD4: dsp_dma_write(&dsp->dma, DMA_NEXT_BLOCK, value)
   dsp_dma.c:426-427  case DMA_NEXT_BLOCK: s->next_block = v
   dsp_dma.c:116-117  addr = s->next_block & NODE_POINTER_VAL (0x3fff)
   dsp_dma.c:121-124  if (addr < 0x1800) block_space = X; block_addr = addr
   dsp_dma.c:137-144  reads SEVEN words: x:[block_addr .. block_addr+6]
```

**So the descriptor the trigger causes the DMA to read is at `x:[0x25 .. 0x2B]` — NOT at `x:[6..10]`.**

### And `x:[0x25..]` is the descriptor the **mixbin loop** builds

`P 009E move #$000025,r4`, then the loop writes seven words through `x:(r4)+`:

| Slot | Written by | Value |
|---|---|---|
| `+0` | `P 00AB` | `a`, starting `0x2C`, `+7` per iteration |
| `+1` | `P 00B0` | `0x59D2` |
| `+2` | `P 00B3` | `0x20` = **32 = NUM_MIXBINS** |
| **`+3`** | **`P 00B5`** | **the `P 00CC`–`00D1` table value — a MIXBUF address** |
| `+4` | `P 00B8` | `0` |
| `+5` | `P 00BA` | `0x8000`, `+0x800` per iteration (the guest pointer walk) |
| `+6` | `P 00C0` | `0x7FF` |

### Why this is a genuine open question, not a resolved one

Mapping both descriptors onto the DMA's field layout (`+0 next_block`, `+1 control`, `+2 count`,
`+3 dsp_offset`, `+4 scratch_offset`, `+5 scratch_base`, `+6 scratch_size`):

| Slot | Doorbell `x:[6..10]` | Mixbin `x:[0x25..0x2B]` |
|---|---|---|
| `+0` next_block | `0x4006` — **EOL set** | `0x2C` — no EOL |
| `+1` control | `0x59E0` | `0x59D2` |
| `+2` count | `6` | `0x20` = 32 |
| `+3` dsp_offset | `0` | **MIXBUF address** |
| `+4` scratch_offset | **`0x800`** | `0` |
| `+5` scratch_base | **not written** | `0x8000` |
| `+6` scratch_size | **not written** | `0x7FF` |

**`dsp_addr` is a DSP-side *scratch* address** — `dsp_dma.c:93` computes
`scratch_addr = scratch_base + *scratch_offset` and that value is what reaches `apu_gp_dma_write` as
`dsp_addr` (`dsp.c:96` → `gp_scratch_rw` → `gp_ep.c:144`).

- The **doorbell** descriptor has `scratch_offset = 0x800` (matching the observed `dsp_addr=0x800` if
  `scratch_base` is 0) — **but its `+0` has the EOL bit set**, so as a chain it is terminal, and the
  trigger's `next_block = 0x25` does **not** point at it.
- The **mixbin** descriptor has `scratch_base = 0x8000`, `scratch_offset = 0`, giving
  `scratch_addr = 0x8000` — **which is not `0x800` either**.

**So neither descriptor's arithmetic reproduces `dsp_addr=0x800` under my reading of the field layout, and
the trigger demonstrably points the DMA at the mixbin descriptor.** I am **not** going to assert which
descriptor produces the observed transfer, and I am **not** going to assert that the mixbin path is
irrelevant to the doorbell — which is the opposite of what the earlier packets concluded.

**Two possibilities, and I cannot distinguish them on this evidence:**

1. **My field-layout reading is wrong** — e.g. `+3`/`+4` are ordered differently, or `scratch_base` is
   supplied from elsewhere (the doorbell builder does not write `+5`/`+6`).
2. **The trigger genuinely drives the mixbin descriptor**, in which case the doorbell's `B+0x810` event
   **does** transit the mixbin path, and the "non-reliance" conclusion of the earlier packets would need
   re-examination — a potential `O-REFUTED`, not an `O-TWO-LEG`.

**This is exactly the kind of question I must not decide alone**, and it is why the referral stands. It also
vindicates the Advisor's insistence on tracing the consumer rather than inferring it from the
`dsp_addr=0x800` coincidence — which is what I had been treating as a consistency argument.

## The consumer — MEASURED, and the answer is "both, in one chain"

I could not settle the consumer statically, so I built the instrument the packet authorizes and
**measured** it: `RECOMP_APU_DMA_DESC_TRACE` records `next_block` at every `dsp_dma_run()` pass.
Run `20260927-145355-186-a4b2-nr-dmadesc2`.

**First attempt was wrong and is recorded.** The first version was **not GP-gated**, and `dsp_dma_run()` is
shared by the GP and the EP. It reported **8 445 events** and an interleaved `0x06 / 0x25 / 0x1E` cycle that
was actually **two programs' chains alternating**. This is the *same hazard* the Advisor flagged for the
P-write watch, and I reproduced it in a new instrument. Fixed by gating on `DSPState`'s DMA-side `is_gp`
(set in `dsp_init`, never overwritten). GP-only result: **`events=766 ngp_skipped=0`** at the exchange.

### The measurement

| Property | Result |
|---|---|
| Events at the first exchange | **766** |
| Non-GP reads excluded | **0** — the trace is entirely GP |
| **`first_block`** | **`0x0006`** |
| Reads in the doorbell region `x:[6..10]` | **255** |
| Reads in the mixbin region `x:[0x25..0x2B]` | **254** |

**The DMA's first descriptor read is `block_addr = 0x0006` — the doorbell's own `x:[6..10]`.** The chain then
cycles:

```
ord 0  next_block=00000006  block_addr=0006   <- DOORBELL
ord 1  next_block=00000025  block_addr=0025   <- MIXBIN
ord 2  next_block=0000001E  block_addr=001E
ord 3  next_block=00000006  block_addr=0006   <- DOORBELL (repeat)
...
```

**So the chain interleaves: the `P 0007` descriptor (`x:[6..10]`), the mixbin descriptor (`x:[0x25..]`) and
block `0x1E` are read in a repeating cycle.**

### THE DECISIVE MEASUREMENT — which block produces the exchange

The block trace alone says which descriptors are *read*. To answer which one *produces the exchange*, the
GP_CLEAR latch now reports the block it was consuming:

```
[GPDMADESC] GP_CLEAR produced by block_addr=0018 (dsp_addr=000800)
[GPWATCH] latch class=GP_CLEAR seq=198852 va=803C0810 observed=00000003 payload=00000000 ... dsp_addr=000800
```

**`block_addr=0018` is DECIMAL 24** (`dsp_dma.c` uses `block_addr` directly as an X address). **Block 24 is
the `P 000E` call's descriptor**, built by builder B (`P 00EB`) with:

| Field | Value | Source |
|---|---|---|
| `+0` next_block | `(24 & 0x3fff) \| 0x4000` = `0x4018`, **EOL set** | builder's own computation |
| `+1` control | `0x59E2` | `P 00F1` **immediate** |
| `+2` count | **`6`** | `P 000D move #$06,r3` — **immediate** |
| `+3` dsp_offset | **`0`** | `P 000A move #$00,r1` — **immediate** |
| `+4` scratch_offset | **`0x000800`** | `P 000B move #$000800,r2` — **immediate** |

**`scratch_offset = 0x000800` reproduces the observed `dsp_addr=000800` exactly** (with `scratch_base = 0`),
and **every field is an immediate**. **Neither stub input appears in any of them.**

### What this settles

**The exchange-producing descriptor's fields are ALL IMMEDIATES.** Per the packet's own leaf classification,
immediate leaves **CLOSE**. Therefore:

- The `B+0x810` exchange is produced by a descriptor whose guest VA, length, DSP offset and scratch offset
  are **immediate constants**.
- **No stub-derived chain reaches the named doorbell's transfer fields.**
- The mixbin descriptor is a *different* block in the same chain (255 doorbell-region reads vs 254
  mixbin-region reads), but it is **not the block that produces the exchange**, and its `+3` MIXBUF address
  does not enter the exchange-producing descriptor.

### Two corrections to my own earlier claims, both recorded

1. **I had identified the wrong descriptor.** Throughout this packet and the earlier ones I called
   `x:[6..10]` (the `P 0007` call) "the doorbell descriptor". **The exchange is produced by block 24 — the
   `P 000E` call's descriptor.** The `x:[6..10]` descriptor is read in the same chain but is **not** the one
   that lands the `W_va` exchange. Statements tying the exchange to `x:[6..10]`'s fields are **superseded**.
2. **My "neither descriptor reproduces `dsp_addr`" claim was wrong**, and so was my follow-up correction that
   `x:[6..10]`'s was the match. **Block 24's `scratch_offset = 0x800` is the one that matches**, exactly.

**A trap worth naming:** `#$18` and `block_addr=0018` are both **decimal 24**, but `#$000012` is decimal 18
while looking like hex `0x12`. My first pass read `0018` as hex and identified the wrong descriptor. **The
correct descriptor was found only by measurement.**

## Row: referred to the Advisor — the frontier now closes on measured evidence

**What the evidence now shows:**

- **All five doorbell fields are immediates**, the trigger's shared state closes, computed readers and
  writers close, the entry set is proven, interrupts are excluded per-vector, and `L2` is carried.
- **The doorbell's `x:[6..10]` descriptor IS consumed by the DMA and is the FIRST block read** — measured,
  not inferred — and its arithmetic reproduces the observed `dsp_addr=0x800` under `scratch_base = 0`.
- **The mixbin descriptor is a block in the SAME chain** (`6 → 0x25 → 0x1E` cycle, 255 vs 254 reads).

**The one question that remains, stated narrowly:** *which block's transfer is the `B+0x810` exchange?* The
chain interleaves the doorbell and mixbin descriptors, and the trace records `block_addr` per pass but not
which pass covered `W_va`.

**Why I am not selecting a row.** Two candidates, and the distinction is exactly this question:

- **`O-TWO-LEG`** — the exchange is produced by **block 24**, the `P 000E` call's descriptor, whose fields
  are **all immediates** (`r1=0`, `r2=0x000800`, `r3=6`) and whose `scratch_offset = 0x800` **reproduces the
  observed `dsp_addr=000800` exactly**. Immediate leaves **CLOSE** per the packet's own classification.
- **`O-REFUTED`** would require a **concrete feasible stub-derived chain** to a field or guard of *that*
  descriptor. **None exists**: neither `0xFFFFB3` nor MIXBUF appears in any of its fields.

**The `O-REFUTED` branch is therefore not available on this evidence**, and the mixbin concern I raised
earlier is resolved: the mixbin descriptor is a *different block* in the same DMA chain, and it is **not**
the block that produces the exchange.

**My recommendation is `O-TWO-LEG`.** I still refer it rather than select it, because the packet's terminal
rows reserve this decision and because **I have been wrong repeatedly in this analysis** — including, in
this very section, misidentifying the exchange-producing descriptor as `x:[6..10]` when it is block 24.
The measurement is the strongest evidence in this packet; whether it satisfies the bar is the Advisor's call.

**`A4b2-r7` remains `R2-EXPL-INPUT`. No strict criterion is discharged.**

### The field-level results, for the record

These are the slice's measurements against the packet's `L1 = PROVEN` bar.

| Requirement | Status |
|---|---|
| All five field leaves close | **YES** — all immediates (`0x59E0`, `0x06`, `0x00`, `0x000800`, and field 1 from `r0`) |
| Trigger guards traced | **YES** — all ×1, `a` from `P 00C1 move #$000025,a`, masked |
| **Shared-state reaching-defs** | **YES** — the trigger's `a` closes as an immediate; no stub participates |
| No stub-derived chain to any of the five fields | **YES** — none found |
| **Alias-disjointness** | **YES for all statically-targeted writers** — 5 builder call sites, all 10 pairs disjoint; **all 39 direct X writes** enumerated, **none** in `x:[6..10]`; the two extra builder blocks **statically unreachable** |
| **Computed writers** | **CLOSED** — `x:(r0)+` with `r0=0`; `x:(r4)+` with `r4=0x25`; `x:(r1)` with `r1=0x24` — all outside every descriptor region |
| **Computed readers** | **CLOSED** — image `I` has **exactly one** (`P 00B9`), and its `r1` is the **immediate** `0x24` from `P 00A2`, statically fixed, outside every region |
| **Entry-set proof** | **YES** — `P 0000` has **0** incoming edges (reset-only); `P 0007` has **1**, chaining from it; all ×1 |
| **Interrupt scope incl. faults** | **YES, per-vector** — no interrupt machinery in either image; the table's four entries have no peripheral/DMA/timer/audio raiser; Reset *is* the entry; `0x02`/`0x04` execute only as entry fall-through, **not** as dispatches |
| Version/provenance | **YES** — image `I` `0x000`–`0x170` proven write-free **by watch**; the whole slice is inside it |
| Second-image byte provenance | **NOT NEEDED** — no chain crossed into the second image |
| Carried `L2 = INVARIANT` | **YES** |
| **Consumer traced, and the exchange-producing block identified** | **YES — MEASURED: `block_addr=0018` (decimal 24), the `P 000E` call's descriptor, all fields immediate** |

### Why I am not selecting a row unilaterally, beyond the consumer gap

**My analysis tooling also produced four wrong answers in this session**, all recorded above or in the
linked preparations:

1. **`pc+1` fall-through** (CFG) — wrong for multi-word instructions.
2. **`pc-1` backward walk** (disjointness) — wrong for the same reason.
3. **Readers-vs-writers misclassification** — inflated the reader gap from **1 site to 11**.
4. **4-digit address truncation** — fabricated **five false edges** into the reset vector, which would have
   meant the entry sequence could re-run.

**Two of those four would have changed a row selection if they had stood.** Every one was found by
re-checking, not by a test failing. The closure above rests on the **corrected** analyses, but the
demonstrated error rate is material evidence about how much weight it can bear — and the consumer finding
shows the concern was not hypothetical.

**`A4b2-r7` remains `R2-EXPL-INPUT`. No strict criterion is discharged.**
