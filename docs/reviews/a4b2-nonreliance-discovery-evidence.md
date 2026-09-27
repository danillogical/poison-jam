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
direction. `A4b2-r7` remains `R2-EXPL-INPUT` and is **not** retroactively PASS. Next packet:
**`A4b2-NR-followup`**, targeted at the specific gaps named in §4.

**This is exploratory/diagnostic evidence — knowledge only, never acceptance of a strict criterion.**

---

## 1. Identity and provenance

| Item | Value |
|---|---|
| Game commit | `a000662aef3bb4d7088f3fa367d19e7ce7c157df` |
| Toolkit commit (base) | `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d` |
| XBE SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` |
| **exe SHA-256 (all runs)** | **`11D33FC9897FDBB64DF3CF433E0D9A5C73DA76C8CCBE84AFE6D9EA96D9262B46`** |
| ctest | **18/18 passed** (14 pre-existing + 4 new selector arms) |

**Toolkit files changed** (all inside the packet's declared scope): `src/apu/apu_watch.c`,
`src/apu/apu_watch.h`, `src/apu/dsp/dsp.c`, `src/apu/dsp/gp_ep.c`, `src/apu/dsp/interp/dsp_cpu.c`,
`tests/apu_watch_fixture_test.c`. **Game files:** `CMakeLists.txt` only (CTest registrations).
`A4b1-r4` was **not** reopened; no file outside the declared scope was modified. (One out-of-scope edit —
a declaration in `dsp_cpu.h` — was made and **reverted** before building; the declaration was moved to a
local `extern` in the in-scope `gp_ep.c`.)

## 2. Leg 2 — empirical invariance (`L2 = INVARIANT`)

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

### The one path this analysis cannot close statically

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

## 4. Limits of this finding — and what `A4b2-NR-followup` must close

**The three gaps that keep `L1` at `INCONCLUSIVE`.** These are the follow-up's brief, in priority order:

1. **The unresolved computed read at `P 00B9`.** `r1 = 0x80 + x:$0000`, where `x:$0000` is a guest-written
   mailbox value. A static decode cannot bound it, so it cannot be excluded from the mix-bin address range.
   **Closing this requires either** (a) an instrumented read trace that records the *actual* `r1` at every
   `P 00B9` execution and shows it never lands in `0x1400..0x17FF`, **or** (b) a guest-side argument that
   bounds `x:$0000` (the CPU writes it — find the writer). (a) is a bounded toolkit/observation addition;
   (b) is guest analysis. **This is the single largest gap.**
2. **The 6 undecoded words at `P 00CC`–`00D1`.** Resolved in *this* record as a **data table** of MIXBUF
   addresses, on three independent grounds (no branch targets them; they are 32-word-aligned bin starts;
   `dor #$0006` reads exactly six words via `movem p:(r2)+,x0`). Additionally, the `op =` invalid-opcode
   diagnostics appear **6 times in the decode run and 0 times in the baseline and perturbation runs** — so
   the guest never executes them. **The remaining gap is only that the toolkit's decoder rejects them**,
   which is a decoder-scope limitation, not a live-code question. The follow-up should record the
   decode-rejection as understood rather than open.
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

- The perturbation gate (`RECOMP_APU_GP_INPUT_PERTURB`) and decode gate (`RECOMP_APU_GP_DECODE`) are
  **off by default**: absent → strict no-op, proven by the baseline run emitting zero `[GPPERTURB]` lines
  and by the fixture's absent-gate identity assertions.
- The `bad-output` control arm is **retained pending closure**: the packet requires it removed at closure.
  It is inert unless the variable is set, and the final default-control run verifies that.
- **Not yet done at the time of this record:** the final absent-env control run after any cleanup, and the
  `0xFFFFB3` source search. Both are recorded as open rather than claimed.
