# `A4b2-NR-r2` discovery evidence — non-reliance of the GP doorbell on two stub inputs

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-nonreliance-discovery.md`, revision **`A4b2-NR-r2`**, class **discovery**,
frozen SHA-256 **`7EF30508CCC2588748EAA92E9B56B12D038D425CA925CA360AA9059DFAF33E38`** — verified before and
after promotion, **not edited**.
**Mandate:** `docs/reviews/a4b2-r7-expl-input-ruling.md` (the Persistent Advisor's two-leg standard).

## Selected outcome row: **`O-INCONCLUSIVE`** (`L1 = INCONCLUSIVE`, `L2 = INVARIANT`)

**The packet's rule, applied faithfully.** `O-TWO-LEG` requires **`L1 = PROVEN` *and* `L2 = INVARIANT`**.
`L2` is satisfied. **`L1` is not**, and the packet's own definition of `L1 = INCONCLUSIVE` names my exact
situation — *"unresolved register-indirect/computed/table-driven branch"* — and warns: *"Do not treat an
indirect edge's absence in one trace as a proof of all feasible edges."* I have one genuinely unresolved
computed-pointer read on the executed path (`P 00B9`), and my Leg 1 work is a **targeted dependency trace**,
not the exhaustive PC-indexed CFG/def-use slice the packet requires for `L1 = PROVEN`. Claiming `PROVEN`
would overclaim, so I do not.

**This is the conservative, honest row, not a failure.** The evidence obtained is *strongly suggestive of
non-reliance* and is recorded in full below; what is missing is the completeness argument, not the
direction. `A4b2-r7` remains `R2-EXPL-INPUT` and is **not** retroactively PASS.

**UPDATE after `A4b2-NR-followup-r1` executed (§8): the row is unchanged, but the reason is now precise.**
Gap 1 — the unresolved `P 00B9` computed read — is **CLOSED by measurement**: the effective address is
internal scratch `0x24` on all six executions, never the mix buffer, with `events == execs == 6`. In its
place the followup exposed a **larger** gap: the GP executes **2 340 distinct PCs spanning `0x0000`–`0x0F28`**,
while the Leg 1 analysis above reasoned over only the first **512 words** (`0x0000`–`0x01FF`). So `L1` stays
`INCONCLUSIVE`, now for a quantified scope reason rather than an unresolved edge. Next packet:
**`A4b2-NR-next-edge`** (the row the followup's own outcome table names for `O-INCONCLUSIVE`), scoped to the
full executed program.

**This is exploratory/diagnostic evidence — knowledge only, never acceptance of a strict criterion.**

---

## 1. Identity and provenance

| Item | Value |
|---|---|
| Game commit | `a000662aef3bb4d7088f3fa367d19e7ce7c157df` |
| Toolkit commit (base) | `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d` |
| XBE SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` |
| exe SHA-256 — **Leg 2 runs** (all six) | `11D33FC9897FDBB64DF3CF433E0D9A5C73DA76C8CCBE84AFE6D9EA96D9262B46` |
| exe SHA-256 — **Leg 1 decode run** | `7615E532FD67A2027C3464CA69D8D60C96F851F5490BB2C1BD9A71993E43224E` |
| Toolkit commit (committed) | `fdd62fb2356936e12648e0341720af5757f5a911` — **pushed to the fork** (`3a3c7c1..fdd62fb`), upstream untouched at `766ecef` |
| ctest | **18/18 passed** (14 pre-existing + 4 new selector arms) |

### Provenance note — two exe hashes, and why that is sound

Three builds occurred while fixing a decoder defect (§7). The **six Leg 2 runs** all used `11D33FC9…`;
the **Leg 1 decode run** used `7615E532…`, which is the **current, reproducible** build from the committed
toolkit (rebuilt twice from the clean tree at `fdd62fb` → identical hash both times).

**This does not weaken either leg:**

- **Leg 2's comparison is internally consistent** — all six runs, including the baseline and all four
  perturbation arms, used the *same* exe `11D33FC9…`. A bit-identical doorbell tuple across arms is exactly
  what that comparison requires, and no cross-build comparison is made.
- **The only source difference between the two builds is the decode-path guard**, which executes **only**
  when `RECOMP_APU_GP_DECODE` is set. Every Leg 2 run has the gate **absent**, and the measurement confirms
  it: **`GPDECODE` line count = 0 in all six Leg 2 runs**, versus 382 in the decode run. So the guarded code
  never ran in any Leg 2 run.
- The Leg 1 decode evidence comes from the **committed, reproducible** build.

| Run | exe (12) | `GPDECODE` lines | `GPPERTURB` lines |
|---|---|---|---|
| baseline | `11d33fc9897f` | 0 | **0** (gate absent → inert) |
| `zero` / `max` / `prng:1` / `prng:2` / `bad-output` | `11d33fc9897f` | 0 | 12 each |
| `decode3` (Leg 1) | `7615e532fd67` | **382** | 0 |

## 2. Leg 2 — empirical invariance (`L2 = INVARIANT`)

**Toolkit files changed** (all inside the packet's declared scope): `src/apu/apu_watch.c`,
`src/apu/apu_watch.h`, `src/apu/dsp/dsp.c`, `src/apu/dsp/gp_ep.c`, `src/apu/dsp/interp/dsp_cpu.c`,
`tests/apu_watch_fixture_test.c`. **Game files:** `CMakeLists.txt` only (CTest registrations).
`A4b1-r4` was **not** reopened; no file outside the declared scope was modified. (One out-of-scope edit —
a declaration in `dsp_cpu.h` — was made and **reverted** before building; the declaration was moved to a
local `extern` in the in-scope `gp_ep.c`.)

Six runs, 30 s each, `--profile exploratory`, identical exe/XBE/bounds/environment apart from the mode
variable. All six reproduce the **identical doorbell tuple**.

| Run | Archive | `GP_CLEAR` tuple |
|---|---|---|
| baseline (gate absent) | `20260927-130036-879-a4b2-nr-baseline` | `seq=198852 va=803C0810 observed=3 payload=0 frame=256 dsp_addr=000800` |
| `zero:0` | `20260927-130056-827-a4b2-nr-perturb-zero-0` | **identical** |
| `max:0` | `20260927-130101-737-a4b2-nr-perturb-max-0` | **identical** |
| `prng:1` | `20260927-130106-656-a4b2-nr-perturb-prng-1` | **identical** (va/observed/payload/dsp_addr; `insns=124186`) |
| `prng:2` | `20260927-130111-523-a4b2-nr-perturb-prng-2` | **identical** (va/observed/payload/dsp_addr; `insns=124066`) |
| `bad-output:0` (control) | `20260927-130142-334-a4b2-nr-badoutput` | **`GP_CLEAR` absent; `GP_NONZERO_OVER` latched** |

`va`, `observed=3`, `payload=0` and `dsp_addr=000800` are **identical across all four input-perturbation
arms and the baseline**. `seq` is also identical (`198852`); `insns` varies slightly on the `prng` arms
(124652 → 124186/124066) because substituted values change the DSP's execution path length, which is
itself evidence the substitution reached the GP.

### The hook is proven to have fired (not merely configured)

| Run | `mixbuf_reads` | `mixbuf_changed` | `periph_reads` | `periph_changed` |
|---|---|---|---|---|
| `zero:0` | 1 623 104 | **0** | 3068 | **0** |
| `max:0` | 1 623 104 | **1 303 008** | 3068 | **3068** |
| `prng:1` | 1 623 104 | **1 623 104** | 3068 | **3068** |
| `prng:2` | 1 623 104 | **1 623 104** | 3068 | **3068** |

- **Both classes were consumed** in every run (nonzero counts) — the packet's changed-consumption witness.
- **Values actually differed** from the baseline on `max`/`prng` (nonzero `changed` counts), and the
  first-substituted values differ per seed (`max` → `FFFFFF`; `prng:1` → `55994F`/`042021`;
  `prng:2` → `98B906`/`084042`) — so the arms are genuinely distinct perturbations, not the same run
  relabelled.
- `zero:0` shows `changed=0` **correctly**: the consumed originals were already zero, so substituting zero
  is a no-op. This is a **useful control in its own right** — it demonstrates the counter reports actual
  difference rather than counting calls.
- **Absent gate is inert:** the baseline run emitted **zero** `[GPPERTURB]` lines.

### The known-bad control is proven to bite

`bad-output:0` leaves both stub inputs **untouched** (`mixbuf_changed=0`, `periph_changed=0`) and instead
overrides only the doorbell classification. Result: **`GP_CLEAR` never latched; `GP_NONZERO_OVER` latched
instead.** So the comparator **can** observe a changed doorbell outcome — a hook that cannot fail would
prove nothing, and this one demonstrably can.

**No stub-dependent divergence was observed.** `L2 = INVARIANT`.

## 3. Leg 1 — static mechanism (`L1 = INCONCLUSIVE`)

### Decode identity

The 371-word image `I` was decoded from the **live GP P-memory after bootstrap** via the toolkit's own
`disasm_instruction()`/`OpcodeEntry` decoder, exposed behind `RECOMP_APU_GP_DECODE`. Run:
`20260927-130336-814-a4b2-nr-decode3` (the decode run's `GP_CLEAR` is the same tuple as the baseline).

| Check | Result |
|---|---|
| Instructions decoded | **380** |
| Explicit `<UNDECODED>` gaps | **6** (at P `00CC`, `00CD`, `00CE`, `00CF`, `00D0`, `00D1`) |
| `end` marker emitted | **yes** |
| Decoded words compared to `I[i] = LE32(xbe[0x1A7D60+4i]) & 0xFFFFFF` | **239 compared, 0 mismatches** |
| Archived `[GPBOOT]` pram vs `I` | **0 mismatches** |
| **Negative control** — decode vs `I` shifted by one word | **238 mismatches** (must be > 0) |

The image is confirmed to be the title's own program, and the decode is confirmed to be that image.

### `0xFFFFB3` cannot reach the doorbell — this part IS conclusive

`0xFFFFB3` is read **four** times, at P `002D`, `0033`, `003A`, `004D`. Every one of those values flows
**only** into internal X-memory scratch:

```
P 002D  move x:$ffffb3,x0     -> P 002F  move x0,x:$007f
P 0033  move x:$ffffb3,x0     -> P 0035  move x0,x:$007d
P 003A  move x:$ffffb3,a1     -> P 0040  move a1,a  ; P 0042  move a1,x:$007c
P 004D  move x:$ffffb3,a1     -> P 0053  move a1,a  ; P 0055  move a1,x:$007e
```

`x:$007c`–`x:$007f` are used only for **comparison and loop control** (`cmpu x0,a` / `blt` at
P `003E`/`003F`, `0051`/`0052`, `0059`/`005A`). **No `0xFFFFB3`-derived value is ever written to `r2`,
`r3`, a DMA peripheral register, or the DMA descriptor.** Its influence is confined to how long the
housekeeping loop spins — it decides *when* the program proceeds, not *what* the doorbell transfers.

### The mix buffer IS addressed — through a data table, not an instruction operand

**Correction to an earlier Session reading.** A first pass reported "zero MIXBUF references", because no
*instruction operand* in the image names `0x1400..0x17FF`. **That reading was wrong**, and it is exactly
the failure mode `AGENTS.md` warns about: an address can be spelled more than one way, and a text search
over one spelling silently covers a subset. Here the addresses are **data**, not operands.

The six words at P `00CC`–`00D1` that the toolkit's decoder reports as `<UNDECODED>` are **not
instructions**. They are a **data table**, and the image reads them as data:

```
P 00A7  move #$0000cc,r2       ; table base = 0x00CC
P 00A9  dor #$0006,p:$00c0     ; six iterations
P 00B4  movem p:(r2)+,x0       ; read one table word per iteration
P 00B5  move x0,x:(r4)+        ; store it into the descriptor being built
```

**Three independent checks establish they are data, not code:**

1. **No branch targets them.** Every branch/jump target in the image is one of
   `0000, 0001, 0029, 005C, 0084, 009D, 009E, 00C0, 00D2, 00DB, 00EB` — **none** lies in `00CC..00D1`.
2. **They are 32-word aligned MIXBUF bin starts.** As X-memory addresses: `0x1400` (bin 0), `0x1440`
   (bin 2), `0x1420` (bin 1), `0x1480` (bin 4), `0x14A0` (bin 5), `0x1460` (bin 3) — all inside
   `0x1400..0x17FF`, all at bin boundaries. As *instructions* they are not valid DSP56300 opcodes, which is
   why the decoder rejected them.
3. **The loop count matches.** `dor #$0006` iterates exactly six times, and the table is exactly six words.

So the image **does** consume mix-buffer data, and the descriptor it builds is used to drive DMA: the loop
calls `jsr p:$00D2` → `bsr p:$010A`, which writes `x:$FFFFD6` (DMA_CONTROL) and polls it — the same
register the doorbell path polls. **The GP program's main loop reads the six mixbins the VP fills and DMAs
them out.**

**This does not by itself refute non-reliance**, and the reason is the distinction the packet's Leg 1
requires between *read* and *relied on*:

- The **doorbell's address and payload are not derived from those bins.** `dsp_addr=0x000800` comes from
  the immediate `P 0004 move #$000800,r2`; the doorbell's zero payload is the classified transfer's own
  data. The mixbin table feeds *other* descriptor fields (the per-voice DMA descriptors this loop builds),
  not the `B+0x810` doorbell.
- **Leg 2 is the empirical check on exactly this**, and it is unambiguous: substituting every consumed
  mixbin value with `max` (`0xFFFFFF`) or two different `prng` streams changed **1.3M–1.6M** consumed
  values and left the doorbell tuple **bit-identical** — same `va`, `observed=3`, `payload=0`,
  `dsp_addr=000800`. If the doorbell relied on mixbin content, that substitution had 1.6 million
  opportunities to change it.

**What this correction costs:** `L1` can no longer be reported as "the image never addresses MIXBUF". The
accurate statement is narrower and is what `L1 = PROVEN` rests on: **no instruction on the decoded executed
path derives the doorbell's payload, address or guards from either stub input** — and the one place the
image does consume mixbin data is a DMA descriptor path whose output is not the doorbell. Combined with
Leg 2's measured invariance, non-reliance holds; but the *reason* is the dependency structure, not the
absence of a MIXBUF reference.

### What the image actually does with the mix bins — and why that is not the doorbell

Refined by reading the whole decoded stream. The GP program has **two** descriptor-building paths, and the
distinction is the crux:

**(a) The doorbell's descriptor — `P 0000`–`P 0007`.** `r0=6, r1=0, r2=#$000800, r3=6`, then
`jsr p:$00db`. The descriptor builder stores:

```
P 00DB  move r0,a ; and #$3fff,a ; or #$4000,a
P 00E0  move a,x:(r0 + 0)      ; guest address, from r0
P 00E1  move #$0059e0,a
P 00E3  move a,x:(r0 + 1)
P 00E4  move r3,x:(r0 + 2)     ; length, from the immediate r3=6
P 00E6  move r1,x:(r0 + 3)     ; from the immediate r1=0
P 00E8  move r2,x:(r0 + 4)     ; DSP address, from the immediate r2=#$000800
```

**Every field of this descriptor is an immediate or a constant.** `r2 = #$000800` is exactly the observed
`dsp_addr=000800`. This is the `B+0x810` doorbell transfer.

**(b) The mix-bin output path — `P 009E`–`P 00CB`.** `r4=#$25`, `r2=#$00CC` (a **P-memory table**),
`dor #$0006` (six iterations), and per iteration:

```
P 00B4  movem p:(r2)+,x0   ; read a table word: 001400 001440 001420 001480 0014A0 001460
P 00B5  move x0,x:(r4)+    ; -> into a descriptor: these are MIXBUF addresses (bins 0,2,1,4,5,3)
P 00B9  move x:(r1),b      ; read the guest buffer
P 00BA  move b,x:(r4)+
P 00BB  add #$0800,b
P 00BD  move b,x:(r1)      ; advance the guest pointer by 0x800
```

So the program **DMAs mix-bin content out to the guest** — its descriptor names MIXBUF addresses as the
DSP-side source. That is why `mixbuf_reads` is enormous (1 623 104 over the run): the DMA read path routes
through `dsp56k_read_memory`, which carries the accounting hook. **This is the GP consuming and forwarding
VP-produced audio, and it is the designed data path.**

**Why this still does not make the doorbell rely on it.** The two paths build **different descriptors**:
(a) at `r0=6` with `r2=#$000800`; (b) at `r4=#$25` with table-derived addresses. The doorbell's observed
`dsp_addr=000800` matches (a), whose fields are all immediates. Path (b)'s outputs are the audio transfer,
not the doorbell.

**The guest mailbox.** `x:$0000`–`x:$0003` are **read but never written** by the GP program (only `x:$0004`
is written, at `P 0098`, to clear a flag). They are therefore **guest-written** mailbox values — the CPU
hands the GP work through them. That is consistent with the `A4b2` claim's own qualifier (*"guest-written
by region or modelled"*); it is not a stub input.

### The one path this analysis cannot close statically — **NOW CLOSED BY MEASUREMENT (§8)**

```
P 0086  move #$00000c,r0
P 0088  move #$000080,a
P 008A  move x:$0000,x0        ; x:$0000 is guest-written at runtime
P 008B  add x0,a
P 008C  move a,r1              ; r1 = 0x80 + x:$0000
P 00B9  move x:(r1),b          ; computed-pointer read, inside the mix-bin loop
```

`r1` is computed from the guest mailbox `x:$0000`, so **a static decode cannot say what `x:(r1)` reads**.
If `x:$0000` were large enough, `r1` could land in `0x1400..0x17FF`.

**This is the honest boundary of `L1`, and the reason the row is `O-INCONCLUSIVE` rather than `O-TWO-LEG`.**
Two things are true at once and neither cancels the other:

- The value read here flows into the **mix-bin output descriptor** (path (b)), whose fields are the ones
  this loop writes — **not** into the doorbell descriptor (path (a)), which the decoder shows is built
  entirely from immediates.
- But I have **not** proven that no feasible path routes it into the doorbell, and the packet explicitly
  forbids treating an unresolved indirect edge as benign: *"Do not treat an indirect edge's absence in one
  trace as a proof of all feasible edges."*

`L2` covers this empirically — substituting every mix-bin value the hook observed, 1.6M times, left the
doorbell bit-identical — but `L2` is *sampling*, and the packet requires the **static** leg for `PROVEN`.

### The doorbell's address is an immediate, not a stub read

The observed `dsp_addr=0x000800` is established by a **literal**:

```
P 0004  move #$000800,r2      (also P 000B)
```

and `r2` is stored into a DMA descriptor field at `P 00E8  move r2,x:(r0 + 4)` inside the descriptor
builder `P 00DB`–`P 00EA`. The descriptor's other fields come from `r3`/`r1` and from `r0`, which
`P 00DB  move r0,a` derives from the caller's `r0` — none of them from `0xFFFFB3`. This confirms the
Session's earlier lead that `I[5]=0x000800` is the DMA source address: it decodes as `move #$000800,r2`.

### Guard independence

Every branch guarding the DMA trigger sequence (`P 00D4`–`P 00DA`, which writes `0xFFFFD7`, `0xFFFFD5`,
`0xFFFFD4`) is reached from `P 00C3 jsr p:$00d2`, whose data source is `a` from the `r4` descriptor path,
not from either stub input. The DMA-ready polls (`P 00FD`, `0102`, `0107`, `010C`, `0110`, `0115`,
`011A`, `011E`) branch on `x:$ffffd6`, a **device status** register — the modelled `DMA_CONTROL` — not on
a stub value.

### Superseded reading (kept visible, not deleted)

An earlier draft of this record contained a section titled *"The image never addresses the mix buffer"*,
claiming **zero** MIXBUF references and citing `P 00B9 move x:(r1),b` with `r1 = #$000024` as the only
computed read. **That section was wrong and has been removed.** It was produced by a text search over
instruction operands, which cannot see the data table at `P 00CC`–`00D1` — the same class of error
`AGENTS.md` records for the `PIO_FREE` enumeration (10 of 28 sites found by grepping one spelling). It is
recorded here rather than silently deleted because the corrected reading above supersedes it, and a reader
comparing drafts must know which one stands. **The corrected reading stands: the image does consume mixbin
data, via the descriptor table.**

## 4. Limits of this finding — and what the follow-up had to close

**Status of this list after `A4b2-NR-followup-r1` executed (§8).** Kept in its original form so a reader can
see what was predicted against what was found; each item is annotated with its outcome.

**The three gaps that kept `L1` at `INCONCLUSIVE`:**

1. **The unresolved computed read at `P 00B9`.** `r1 = 0x80 + x:$0000`, where `x:$0000` is a guest-written
   mailbox value. A static decode cannot bound it, so it cannot be excluded from the mix-bin address range.
   **Closing this requires either** (a) an instrumented read trace that records the *actual* `r1` at every
   `P 00B9` execution and shows it never lands in `0x1400..0x17FF`, **or** (b) a guest-side argument that
   bounds `x:$0000` (the CPU writes it — find the writer). (a) is a bounded toolkit/observation addition;
   (b) is guest analysis. **This is the single largest gap.**
   → **CLOSED by (a) in §8.** Measured effective address `0x24` on all six executions, never the mix buffer,
   `events == execs == 6`. Note the surprise: the live `r1` comes from `P 00A2 move #$000024,r1`, **not**
   from the `P 0086`–`P 008C` computation this item was written around.
2. **The 6 undecoded words at `P 00CC`–`00D1`.** Resolved in *this* record as a **data table** of MIXBUF
   addresses, on three independent grounds (no branch targets them; they are 32-word-aligned bin starts;
   `dor #$0006` reads exactly six words via `movem p:(r2)+,x0`). Additionally, the `op =` invalid-opcode
   diagnostics appear **6 times in the decode run and 0 times in the baseline and perturbation runs** — so
   the guest never executes them. **The remaining gap is only that the toolkit's decoder rejects them**,
   which is a decoder-scope limitation, not a live-code question. The follow-up should record the
   decode-rejection as understood rather than open.
   → **CLOSED in the followup packet**, which decided no decoder surgery is required and added the falsifier
   *"if the table is actually executed or altered, do not reuse this conclusion."* Corroborated by §8: the
   six `P 00B9` reads walk guest pointers by `0x800` six times, consistent with a six-entry bin table.
3. **The completeness argument.** The packet requires a *PC-indexed CFG/def-use slice covering every
   feasible executed-path write and guard*. What this record supplies is a **targeted dependency trace**
   of the two stub inputs to the doorbell, plus the two descriptor paths. Those are not the same artifact,
   and the packet's `PROVEN` bar is the former. A follow-up could either build the full slice or argue the
   targeted trace is sufficient — **that argument is the Planner's, not the Session's.**

**Additional limits, stated:**

4. **`L2` is not a general proof by sampling.** It shows invariance across four adversarial vectors at one
   pinned exe/XBE/configuration; it does not prove invariance for every conceivable stub value.
5. **Neither leg establishes a device model**, strict doorbell behaviour, audio correctness, VP work,
   guest spin exit or liveness. No MIXBUF value-model is proposed (ruled out by the Advisor); no
   `0xFFFFB3` model is proposed (premature).
6. **The direction of the evidence is consistent and one-way.** Every measurement points away from
   reliance: `0xFFFFB3`'s four reads are confined to scratch used only for loop control; the doorbell
   descriptor is built entirely from immediates; and 1.6M substituted mix-bin values left the doorbell
   bit-identical. **Nothing observed points toward reliance.** The row is `INCONCLUSIVE` because the
   *completeness* argument is missing, not because the evidence is mixed or contradictory.

## 5. Parallel thread — `0xFFFFB3` register identification

**Status: `UNRESOLVED`.** No Xbox/MCPX hardware source was consulted in this session, and the packet
requires a qualifying source rather than a toolkit comment or a guest poll. The register's meaning remains
unknown; `dsp.c:56-57` still carries upstream's own `// core->num_inst; // ??`. Per the packet,
register-identification status **does not override a failed leg** — and it does not affect `L1`/`L2`, which
stand on the observed read-to-scratch confinement and the empirical invariance.

## 6. Instrumentation closure

- The perturbation gate (`RECOMP_APU_GP_INPUT_PERTURB`), decode gate (`RECOMP_APU_GP_DECODE`) and B9 gate
  (`RECOMP_APU_GP_B9_TRACE`) are **off by default**: absent → strict no-op, proven by the fixture's
  absent-gate identity assertions and by the final control below.
- **FINAL ABSENT-ENV INERTNESS CONTROL — PASSED.** Run
  `logs/runs/20260927-133008-913-a4b2-nrf-inert`, all three gates absent, current build:

  | Check | Result |
  |---|---|
  | `[GPPERTURB]` lines | **0** |
  | `[GPDECODE]` lines | **0** |
  | `[GPB9]` lines | **0** |
  | `gpb9_trace.txt` created | **no** |
  | Doorbell tuple | `seq=198852 va=803C0810 observed=00000003 payload=00000000 dsp_addr=000800` — **identical to every other run in this packet** |

  So every diagnostic gate is inert when unset, and the instrumented build reproduces the unmodified
  behaviour exactly.
- **`bad-output` arm: bite preserved, removal is the next packet's duty.** Its effect is archived and
  re-verified in the retained run `logs/runs/20260927-130142-334-a4b2-nr-badoutput`
  (`GP_CLEAR` latches = **0**, `GP_NONZERO_OVER` latches = **1**, 2 control log lines). The arm remains
  registered in `CMakeLists.txt` for now; the followup packet requires its removal **after** the bite is
  preserved, which is now satisfied, so `A4b2-NR-next-edge` should remove it.
- **`0xFFFFB3` source search: still open**, and still `UNRESOLVED` (§5). No qualifying hardware source was
  consulted. Recorded as open rather than claimed.

### `L2 = INVARIANT` independently reconfirmed

A useful by-product of the followup: **all eight** of its runs — across **six different builds**, with the
B9 gate absent, the B9 gate present, and the decode gate present — produce the **identical** doorbell tuple:

```
seq=198852 va=803C0810 observed=00000003 payload=00000000 frame=256 insns=124652 dsp_addr=000800
```

| Run | exe (12) | gate | `GP_CLEAR` |
|---|---|---|---|
| `-252-b9-absent` | `875f2b6882d5` | none | identical |
| `-119-b9-trace` | `875f2b6882d5` | B9 | identical |
| `-383-b9-hist` | `21a88a359983` | B9 | identical |
| `-520-b9-hist2` | `221793e9b9bd` | B9 | identical |
| `-549-b9-hist3` | `c4e913b40dd0` | B9 | identical |
| `-253-b9-final` | `8d507016f11c` | B9 | identical |
| `-143-b9-full` | `9e0a594e57d8` | B9 + decode | identical |
| `-913-inert` | `9e0a594e57d8` | **none** | identical |

This is stronger than the predecessor's Leg 2 in one respect worth stating: the predecessor's invariance was
measured across **one** build with varying *inputs*; this is the same tuple across **six** builds with
varying *instrumentation*, including builds whose decode path was rewritten. The doorbell outcome is
insensitive to both.

## 7. Defect found and fixed in the decoder path (recorded for the next reader)

The first two decode attempts **crashed the run** with an access violation (`0xC0000005`) at ~2.8 s. Cause,
found in the toolkit's own decoder:

`lookup_opcode_slow()` ends in

```c
fprintf(stderr, "op = %08x\n", op);
assert(!"Invalid op code in dsp_cpu");
return NULL;
```

and in a **Release build `assert()` is a no-op**, so an unrecognised word returns **NULL**. The pre-existing
`disasm_instruction()` path dereferenced it. This is a latent defect in the pinned decoder that only becomes
reachable when something calls the decoder on a word that is not valid DSP56300 code — which is exactly what
decoding a **program image containing a data table** does.

**Two failed attempts, recorded honestly:**

1. First guard tested `lookup_opcode(inst)->template == NULL` — **still a NULL dereference**, because the
   pointer itself is NULL. The run crashed again with the identical signature.
2. Correct guard tests **the pointer**: `lookup_opcode(inst) == NULL` → emit `<UNDECODED>` and skip.

The corrected version decodes cleanly and the run then terminates with the **same** `unhandled_exception` at
~4.75 s as every other run in this packet — i.e. the normal A2h end, not a decoder fault.

**Why this matters beyond this packet:** the six `<UNDECODED>` words are *data*, and the decoder's rejection
of them is what revealed that. A future packet that decodes a GP image must expect data tables inside
P-memory and must not assume every word is an instruction. The guard is now permanent in the committed
toolkit.

## 8. `A4b2-NR-followup-r1` execution — gap 1 CLOSED (measured, not inferred)

**Packet:** `docs/packets/a4b2-nr-followup.md`, revision **`A4b2-NR-followup-r1`**, frozen SHA-256
**`886620CC6797FC89130B1F89310E6B41C92E809F32820A8133609FBEEC9CC5F5`**. Gate:
`RECOMP_APU_GP_B9_TRACE=1`. Run: `logs/runs/20260927-132641-253-a4b2-nrf-b9-final` (and the
full-range rerun `20260927-132808-143-a4b2-nrf-b9-full`).

### The answer to gap 1

**`P 00B9` reads internal scratch `0x24` on every execution. It never reads the mix buffer.**

| ord | epoch | `r1` | `x:$0000` | derived `0x80+x:$0000` | **effective address** | value | mixbuf | alias |
|---|---|---|---|---|---|---|---|---|
| 0 | 1 | `000024` | `000000` | `000080` | **`000024`** | `008000` | 0 | 0 |
| 1 | 1 | `000024` | `000000` | `000080` | **`000024`** | `008800` | 0 | 0 |
| 2 | 1 | `000024` | `000000` | `000080` | **`000024`** | `009000` | 0 | 0 |
| 3 | 1 | `000024` | `000000` | `000080` | **`000024`** | `009800` | 0 | 0 |
| 4 | 1 | `000024` | `000000` | `000080` | **`000024`** | `00A000` | 0 | 0 |
| 5 | 1 | `000024` | `000000` | `000080` | **`000024`** | `00A800` | 0 | 0 |

Terminal: `events=6 execs=6 in_mixbuf=0 in_alias=0 invalid=0`. **Both counters agree** (`events ==
execs == 6`), so this is a *complete* record of every `P 00B9` execution in the bounded exchange, not a
sample — the packet's completeness requirement is met by measurement, not by assertion.

### Three things this settles

1. **The unresolved edge is resolved.** The predecessor recorded `r1 = 0x80 + x:$0000` as an unbounded
   computed pointer because `x:$0000` is guest-written. Measured: `x:$0000 = 0` at every one of the six
   executions, and **the effective address is `0x24`, not `0x80`** — so the `P 0086`–`P 008C` computation
   (`r1 = 0x80 + x:$0000`) is *not* the one in effect at `P 00B9`. The live path is `P 00A2
   move #$000024,r1`, which the earlier static reading had listed but not connected to the executed edge.
   `0x24` is internal scratch, far below `0x0C00`, so **the computed read cannot reach either mix-buffer
   window** — for the observed values.
2. **The read is a guest-pointer walk, not an audio read.** The six values are `0x8000`, `0x8800`,
   `0x9000`, `0x9800`, `0xA000`, `0xA800` — advancing by exactly **`0x800`** per iteration, matching
   `P 00BB add #$0800,b`. They are the **guest** DMA destination pointers the GP walks to hand audio
   out, confirming the descriptor path is the audio output path and is *not* the doorbell.
3. **The six iterations match `dor #$0006` exactly** — six reads, six bins. Consistent with the
   `P 00CC`–`00D1` table's six MIXBUF entries, and consistent with the predecessor's finding that those
   six words are table **data**.

### A second, larger finding — the executed program far exceeds the decoded window

The B9 trace's PC histogram exposed a **scope error in the predecessor's Leg 1**, which this run corrects:

| Quantity | Value |
|---|---|
| GP distinct PCs executed | **2 340** |
| GP PCs executed at or above `0x200` | **2 052** |
| GP PC range | **`0000..0F28`** |
| First GP PC at or above `0x200` | `02EC` |
| Predecessor's decode window | `0x0000..0x01FF` (**512 words**) |
| Full-PRAM decode (this run) | **3 964 instructions**, 6 `<UNDECODED>` |

**The GP executes 2 340 distinct PCs spanning `0x0000`–`0x0F28`; the predecessor decoded and reasoned
about only the first 512 words, containing 380 instructions.** So the predecessor's Leg 1 was performed
over **380 of the 2 340 executed PCs (~16%)**, and over 512 of the 3 881 words the program spans (~13%).
That is precisely the partial-enumeration failure `AGENTS.md` records for the `PIO_FREE` site list (10 of
28 sites found by grepping one spelling), and it is a **stronger** reason for `L1 = INCONCLUSIVE` than the
one originally recorded. The `P 00B9` gap is now closed; **this** gap is open and is the real obstacle to
`L1 = PROVEN`.

**Corrective action taken in this packet:** the decode range was widened from `0x1FF` to
`DSP_PRAM_SIZE - 1` (4095), and the diagnostic PC histogram from 512 to 4096 entries, so nothing above
`0x1FF` is silently dropped. The full-range decode produced **3 964 instructions**, so a follow-up slice
now has the whole executed program available.

### Instrumentation defect found and fixed (recorded for the next reader)

The first B9 run reported **`gp_exec_total=0`** and zero events, while its own PC histogram showed 288
distinct PCs of the GP image executing. Cause: `b9_core_is_gp()` originally read **`core->is_gp`**, which
is **only ever written by `dsp_c_sync_from_vm()`** — a function that is registered in the ops table
(`dsp_c.c:324`) but **never called** on this path, so the field is permanently `0`. The reliable source is
`core->opaque`, which `dsp_c_init()` points back at the `DSPState` (`dsp_c.c:282`) and whose `is_gp` *is*
set at `dsp_init()` (`dsp.c:143`).

**This is a latent defect in the pinned toolkit, not in this packet's instrumentation alone:** any code
that classifies work by `core->is_gp` on the interpreter core will silently see "not the GP". It is
recorded here because a future reader could otherwise repeat it. It also means the *first* B9 run is
**not** evidence of anything — it is preserved only as the record of the defect.

### What gap 1's closure does and does not license

- **Does:** removes the specific unresolved computed read from the predecessor's §4 gap list. The
  `P 00B9` edge can no longer be cited as an open mix-buffer path.
- **Does not:** establish `L1 = PROVEN`. The packet's `L1 = PROVEN` bar is a full PC-indexed CFG/def-use
  slice over every feasible route to the first doorbell descriptor and DMA write — and that slice must now
  cover **2 340 PCs across `0x0000`–`0x0F28`**, not 380 instructions of a 512-word window. The trace also
  answers only what *this run* reads: it does not prove an address bound for all possible future mailbox
  values, exactly as the packet states.

**Corrected `L1` status: still `INCONCLUSIVE`, but for a different and now precisely quantified reason** —
the executed-program scope, not the `P 00B9` edge.

The first two decode attempts **crashed the run** with an access violation (`0xC0000005`) at ~2.8 s. Cause,
found in the toolkit's own decoder:

`lookup_opcode_slow()` ends in

```c
fprintf(stderr, "op = %08x\n", op);
assert(!"Invalid op code in dsp_cpu");
return NULL;
```

and in a **Release build `assert()` is a no-op**, so an unrecognised word returns **NULL**. The pre-existing
`disasm_instruction()` path dereferenced it. This is a latent defect in the pinned decoder that only becomes
reachable when something calls the decoder on a word that is not valid DSP56300 code — which is exactly what
decoding a **program image containing a data table** does.

**Two failed attempts, recorded honestly:**

1. First guard tested `lookup_opcode(inst)->template == NULL` — **still a NULL dereference**, because the
   pointer itself is NULL. The run crashed again with the identical signature.
2. Correct guard tests **the pointer**: `lookup_opcode(inst) == NULL` → emit `<UNDECODED>` and skip.

The corrected version decodes cleanly (382 `[GPDECODE]` lines, `end` marker present) and the run then
terminates with the **same** `unhandled_exception` at ~4.75 s as every other run in this packet — i.e. the
normal A2h end, not a decoder fault.

**Why this matters beyond this packet:** the six `<UNDECODED>` words are *data*, and the decoder's rejection
of them is what revealed that. A future packet that decodes a GP image must expect data tables inside
P-memory and must not assume every word is an instruction. The guard is now permanent in the committed
toolkit.
