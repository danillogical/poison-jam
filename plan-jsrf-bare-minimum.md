# Jet Set Radio Future: Windows port milestone plan

This file is the **execution authority**. It owns the current packet, current
blocker, packet status, next action, and milestone acceptance. Do not discover work
by scanning historical documents or old status tables. `docs/agent-workflow.md` owns
roles, the packet lifecycle and escalation; `AGENTS.md` owns operating/build/runtime
discipline; `docs/jsrf-run-profiles.md` owns evidence-profile semantics.

## CURRENT PACKET — `A4b2-NR-next-edge-r1` **EXECUTED 2026-09-27 → `O-INCONCLUSIVE`; the GP loads a SECOND program image**

**`A4b2-NR-next-edge-r1` (`7461AAA4…`) executed. Row: `O-INCONCLUSIVE`.** Evidence:
`docs/reviews/a4b2-nr-next-edge-execution-evidence.md`. Verification:
`docs/reviews/a4b2-nr-next-edge-r1-session-verification.md`.

### The headline finding: there are TWO P-memory images, and the analysis used the wrong one

The packet's premise was that the executed program is the boot image `I` (371 words) plus a region above
it, and that the predecessor's error was analysing only its first 512 words. **That premise was
incomplete.** Two full-PRAM decodes — one at bootstrap, one at the first exchange — show the GP **loads a
second program into P-memory after the bootstrap**:

| P-memory band | At bootstrap | At the first exchange |
|---|---|---|
| boot image `I` (`0x000`–`0x172`) | real code | **real code — unchanged** |
| `0x173`–`0x7FF` | **1 677 zeros** | **real code** (25 zeros) |
| `0x800`–`0xFFF` | **2 048 × `0xCACACA`** | **real code** (215 × `0xCACACA`) |

`0xCACACA` is the toolkit's own `memset` fill (`dsp_cpu.c:299-301`), so `0x800`–`0xFFF` at bootstrap is
**uninitialised fill**, not program. Of 2 957 words in both snapshots, **2 480 changed**.

**Measured against the bootstrap snapshot:** only **193 of 2 340 executed PCs (8.2%)** lie inside image
`I`. **99.4% of all executions** were at a PC whose bootstrap word was zero (**42.0%**) or `0xCACACA`
fill (**57.4%**). So the bootstrap decode was not merely incomplete — for those PCs it was **wrong**, and
the CFG in the evidence record, though methodologically sound, was built on those wrong bytes.

### What survives: the doorbell code is in the FIRST image

The doorbell's instructions are **word-for-word identical** in both snapshots — `P 0004` `62F400`
(`move #$000800,r2`), `P 0007` `0BF080`, `P 00B9` `57E100`, `P 00DB` `220E00`, `P 00E8` `0A7092`. So the
doorbell descriptor path lies entirely within image `I`, the `P 0004` immediate still matches the observed
`dsp_addr=000800`, and **the predecessor's in-window doorbell readings are not invalidated** — they were
unproven as a complete account of the program, which this run now shows is larger and later-loaded.

### Row and next action

**`O-INCONCLUSIVE`.** `L2 = INVARIANT` stands (tuple unchanged: `seq=198852 va=803C0810 observed=3
payload=0 dsp_addr=000800`). `L1` is `INCONCLUSIVE`: the feasibility slice was not completed (the CFG
reaches only 194 of 2 340 executed PCs from a single entry; interrupt/vector entry is not modelled), **and
its input bytes were wrong**. No concrete feasible causal chain to the named doorbell was found, so
`O-REFUTED` is **not** selected. `A4b2-r7` stays `R2-EXPL-INPUT`; no strict criterion discharged.

**Next authorized action:** **`A4b2-NR-next-edge-followup`** — the row the packet's own table names —
targeted at the specific unknown this run recorded: **the second image's provenance, load mechanism and
extent, plus a slice built on the correct bytes.**

### Instrumentation added (env-gated, GP-only, read-only, off by default)

- `RECOMP_APU_GP_DECODE` **second decode at the first exchange** (`dsp56k_request_decode2`, tagged
  `second-decode-at-exchange`) — **this is what produced the headline finding.**
- **Per-core identity logging** (`b9_note_core_identity`): measured **one** core,
  `opaque=00000239DED16900`, `is_gp=1`, so the trace's GP attribution is **verified, not assumed**.

### Two tooling defects found and fixed in the Session's own analysis

1. **Fall-through used `pc+1`** — DSP56300 instructions are 1–*N* words, so this is wrong for every
   multi-word instruction. Fixed to the next *decoded* PC.
2. **Any `(rN)` operand treated as an indirect jump** — misclassified **2 062 data moves** as control
   transfers, yielding a bogus "2 083 unresolved edges". Only `jmp`/`jsr` with a computed operand are
   indirect. After the fix: **0 indirect, 21 unresolved (returns only)**.

### Session defect, corrected: the packet was promoted mid-write

The promotion recorded 32 lines / `9E919866…`; the Planner's final file is **39 lines / `7461AAA4…`**. The
Session polled, saw a complete-looking body with an `ADEQUATE` verdict, and promoted while the Planner's
turn was still in flight. **A file that looks finished is not evidence its author has stopped writing.**
Repaired by re-promoting against the stable bytes; the packet was never edited by the Session.

---

## Previous packet — `A4b2-NR-next-edge-r1` (**discovery**: the full-program PC-indexed CFG/def-use slice) — **PROMOTED 2026-09-27, `ADEQUATE`**

- **Packet:** `docs/packets/a4b2-nr-next-edge.md`, revision **`A4b2-NR-next-edge-r1`**, class **discovery**,
  frozen SHA-256 **`7461AAA471D5EE0B78348AC17F7BF5B8CD333A860437705B65946B349311ACFF`** (**39 lines**).
  **This is the packet to execute.** Promotion byte-identical with no revision (§5.3).
  **Session defect, corrected:** the packet was first promoted **mid-write** at a 32-line draft
  (`9E919866…`); the Planner's turn was still in flight and it extended the file to 39 lines. The hash above
  is the **final, stable** artifact (re-read twice, unchanged). The packet was never edited by the Session.
  Recorded in `docs/reviews/a4b2-nr-next-edge-r1-session-verification.md`.
- **Adequacy:** **`VERDICT: ADEQUATE`** — the **writing Planner's own** review, as §5.8 requires for a
  discovery packet (child `1aadbb20-4a2f-4a3c-9ca5-9699806d2820`, `codex/gpt-6-sol` @ `high`). No second
  Planner; no Muse preflight repeated.
- **Why it exists:** `A4b2-NR-followup-r1` executed → **`O-INCONCLUSIVE`**. The `P 00B9` gap was closed by
  measurement, but the real obstacle is now quantified: the original Leg 1 reasoned over **380 of 2 340
  executed PCs** — a 512-word window covering ~16% of the executed program (`0x0000`–`0x0F28`). Evidence:
  `docs/reviews/a4b2-nonreliance-discovery-evidence.md` §8. Session verification:
  `docs/reviews/a4b2-nr-next-edge-r1-session-verification.md`.
- **The target:** the **PC-indexed CFG/def-use slice over every feasible route** to the first doorbell
  descriptor and DMA write, across the **full 3 881-word program** (3 964 decoded instructions). The
  Advisor's Q2 **SUSPENDED / CARRIES / FORBIDDEN** lists are carried, and the 2 340-PC histogram may be
  used **only** as a lower-bound cross-check — the `PROVEN` bar stays **feasibility**-based.
- **Q4 guardrails — carried with the Session's correction:** warning at `dsp_cpu.h:49`
  (`dsp_core_t.is_gp`, never populated) and `dsp.h:79` (`DspCoreState.is_gp`, clobbered by
  `dsp_c_sync_to_vm`); **`dsp.h:105` (`DSPState.is_gp`) is CANONICAL and must not be labelled unsafe** —
  the Advisor's ruling had this reference inverted and the packet says so explicitly. Both headers are
  **comments-only** new scope.
- **Runs:** exactly **one** fresh absent-gate baseline at the pinned new identity, which doubles as the
  final closure inertness control. **Condition (Advisor Q3):** if the packet's toolkit delta executes on
  the GP path with gates absent, a fresh same-exe mini-series is required.
- **Closure:** the retained `bad-output` arm is **removed** (bite preserved and re-verified); diagnostic
  changes reverted; all gates off by default.
- **Session preparation already done:** `docs/reviews/a4b2-nr-next-edge-q2-preparation.md` re-derives all
  seven suspended items over the full program. **Six survive unchanged; the seventh (`P 00DB` "sole
  builder") is refuted as to its reason** — there are **three** callers plus two for sibling `P 00EB` — but
  the callers write **pairwise disjoint** descriptor regions (`x:[6..10]` doorbell, `x:[12..16]`,
  `x:[18..22]`), so the original *conclusion* survives on a reason the original never stated. The open
  question is the **reader**: whether a computed pointer can read another region as the doorbell's
  descriptor. Durable slice inputs are archived (`full_decode.txt`/`.json`, `gpb9_trace.txt`) and the
  decode is verified to cover **100%** of the 2 340 executed PCs.

---

## Previous packet — `A4b2-NR-followup-r1` **EXECUTED 2026-09-27 → gap 1 CLOSED; `O-INCONCLUSIVE` stands, reason now quantified**

**`A4b2-NR-followup-r1` (`886620CC…C5F5`) executed.** Gap 1 — the unresolved `P 00B9` computed read — is
**CLOSED BY MEASUREMENT**, and in closing it the run exposed a **larger** gap that now governs. Evidence:
`docs/reviews/a4b2-nonreliance-discovery-evidence.md` §8. Verification:
`docs/reviews/a4b2-nr-followup-r1-session-verification.md`.

### Gap 1: CLOSED — `P 00B9` reads internal scratch, never the mix buffer

Gate `RECOMP_APU_GP_B9_TRACE=1`. Run `20260927-132641-253-a4b2-nrf-b9-final`, rerun
`20260927-132808-143-a4b2-nrf-b9-full`. **All six executions** (matching `dor #$0006`) read effective
address **`0x24`** — internal scratch — with `mixbuf=0 alias=0` on every event:

| ord | `r1` | `x:$0000` | derived `0x80+x:$0000` | **effective** | value | mixbuf | alias |
|---|---|---|---|---|---|---|---|
| 0–5 | `000024` | `000000` | `000080` | **`000024`** | `008000`→`00A800` step `0x800` | 0 | 0 |

Terminal `events=6 execs=6 in_mixbuf=0 in_alias=0 invalid=0` — **both counters agree**, so this is a
*complete* record of every `P 00B9` execution in the bounded exchange, not a sample. Two surprises worth
recording: the live `r1` comes from **`P 00A2 move #$000024,r1`**, *not* from the `P 0086`–`P 008C`
`0x80 + x:$0000` computation the predecessor's gap was written around; and the six values are **guest DMA
pointers advancing by exactly `0x800`** (`P 00BB add #$0800,b`), i.e. the audio output walk — not the
doorbell, and not a mix-buffer read.

### The larger gap this exposed: the executed program is ~12× the decoded window

| Quantity | Value |
|---|---|
| GP distinct PCs executed | **2 340** |
| GP PCs at or above `0x200` | **2 052** |
| GP PC range | **`0000..0F28`** |
| Predecessor's decode/analysis window | `0x0000..0x01FF` (**512 words**) |
| Full-PRAM decode now available | **3 964 instructions**, 6 `<UNDECODED>` |

**The predecessor's Leg 1 reasoned over 380 of the 2 340 executed PCs (~16%), and 512 of the 3 881 words
the program spans (~13%).** That is the same partial-enumeration failure `AGENTS.md` records for the
`PIO_FREE` list (10 of 28 sites via one spelling), and it is a **stronger** ground for `L1 = INCONCLUSIVE`
than the one originally recorded. **Corrective action taken:** the decode range was widened to
`DSP_PRAM_SIZE-1` and the diagnostic PC histogram from 512 to 4096 entries, so nothing above `0x1FF` is
silently dropped. A follow-up slice now has the whole executed program available.

### Two latent toolkit defects found and fixed

1. **Decoder NULL dereference.** `lookup_opcode_slow()` ends in `assert(!"Invalid op code"); return NULL;`
   and Release `assert` is a no-op, so an unrecognised word made `disasm_instruction()` dereference NULL and
   killed the run (`0xC0000005`) — reachable only when decoding an image containing a **data table**. First
   guard attempt tested `->template` on the NULL pointer and crashed again; the fix tests the pointer.
2. **`core->is_gp` is never populated.** It is written only by `dsp_c_sync_from_vm()`, which is registered
   in the ops table (`dsp_c.c:324`) but **never called**, so the field is permanently `0`. The first B9 run
   reported `gp_exec_total=0` while its own histogram showed 288 GP-image PCs executing. The reliable source
   is `core->opaque` → `DSPState.is_gp` (set at `dsp.c:143`). **Any code classifying work by `core->is_gp`
   will silently see "not the GP"** — recorded because a future reader could repeat it.

### Row: still `O-INCONCLUSIVE`, for a now-quantified reason

`L2 = INVARIANT` is unchanged and reconfirmed (the doorbell tuple is again bit-identical:
`seq=198852 va=803C0810 observed=3 payload=0 dsp_addr=000800`). `L1` remains short of `PROVEN` — but the
`P 00B9` edge can no longer be cited, and what remains is a **scope** problem, not an unresolved edge. No
strict criterion is discharged; `A4b2-r7` stays `R2-EXPL-INPUT`.

**Next authorized action:** **`A4b2-NR-next-edge`** — the row the followup's own outcome table names for
`O-INCONCLUSIVE` — scoped to the **full executed program** (`0x0000`–`0x0F28`, 2 340 PCs, 3 964 decoded
instructions), building the PC-indexed CFG/def-use slice over every feasible route to the first doorbell
descriptor and DMA write.

### Advisor ruling on this execution — binding on the next packet

`docs/reviews/a4b2-nr-followup-advisor-ruling.md` (handle `muse_FkNhGaXtV9P5`, `muse-spark-1.3-contributor`,
effort `max`). **Row call `O-INCONCLUSIVE` CONFIRMED.** No contract impact; **no `§5.4(2)` anywhere** — the
predecessor never premised "the program is 512 words", so nothing downstream relied on an invalidated
premise. The scope defect only strengthens the direction already recorded.

**The `A4b2-NR-next-edge` Planner brief MUST carry the Advisor's Q2 lists.**

**SUSPENDED — every completeness/negative claim from the 512-word analysis. What died is every
"only / never / every / no-other" uttered over a 16% enumeration.** Must be re-derived over the full
3 881-word program before next-edge may rely on any of it:

1. All `0xFFFFB3` read sites — the **"four reads" count is forbidden** until re-enumerated.
2. Full def-use of `x:$007c`–`x:$007f` — **"loop-control-only" suspended** (upper code may read them).
3. All callers of builder `P 00DB` + all reaching definitions of `r0`–`r3` at entry — **"sole builder /
   all-immediate" suspended**.
4. All DMA-register writes + trigger sequences — **"sole trigger" suspended**.
5. All X-writes to `$0000`–`$0004` — **"mailbox never written" suspended**.
6. All branch/call targets program-wide — the **"none targets `00CC`–`00D1`" *feasibility* ground
   suspended** (its *execution* ground stands).
7. `r1` reaching-definitions at `P 00B9` including `P 008C` and any upper writers — feasibility closure of
   the measured edge (the trace answers this run's reads, not all feasible mailbox values).

**CARRIES without re-derivation:** the 512-word decode bytes; the `op =` execution-absence
(window-independent: the interpreter decodes on opcache miss during execution, so every executed PC was
decoded, and `op =` fired 0× across execution runs against the decode run's 6× positive control); the
`P 00B9` measurement table; the six table words' values and bin-alignment arithmetic; the doorbell tuple;
literal readings (`P 0004`/`P 000B`, `dor #$0006`); per-instruction semantics within the window.

**FORBIDDEN:** citing any 512-word-era "only/never/every" claim; citing `P 00B9` as open; treating trace
absence as feasibility proof. The 2 340-PC histogram may be used **only** as a cross-check lower bound
(executed ⊆ slice-covered) — the `PROVEN` bar remains **feasibility**-based.

**`L2` status:** the r2 four-arm same-exe series **STANDS** as the `L2` core (window-independent). The
eight followup runs are **admissible as cross-build transfer/robustness corroboration but FORBIDDEN as
perturbation evidence** — they test instrumentation/build-invariance, a different counterfactual. They
bridge r2's `L2` to the next identity, so only **one** fresh absent-gate baseline is required there (the
closure control doubles as it). **Condition:** if next-edge changes GP-path behaviour with gates absent, a
fresh same-exe mini-series is required (baseline + one changed-input arm with changed-counters +
comparator-validity recheck).

**`bad-output` removal: authorized** (bite preserved and re-verified).

**Q4 guardrails — Planner must place in next-edge scope or a bounded commit:** source comments at both
field declarations (`dsp_cpu.h:49` — never populated, do not read; `dsp.h:105` — clobbered by
`sync_to_vm` in traced runs, do not read) plus a canonical-GP-test note (`DSPState.is_gp` via `opaque`;
`dma.is_gp` in DMA paths). **Future classification by either core field is FORBIDDEN.** Deleting the dead
`is_gp` copies in both sync functions is **permitted, not required** (zero observable change; Planner's
call). `A4b1-r4` is **unaffected** and must not be touched.

**Session correction accepted:** my §8 account said `core->is_gp` is "permanently 0"; the accurate picture
is that `dsp_init` sets the **VM** field correctly but `dsp_c_sync_to_vm` **clobbers it** with the
interpreter core's unwritten `0` on every traced GP frame, so **both** fields read `0` in traced runs.
Contained (VM field has zero readers; interpreter field's only readers are the corrupting copy, a
compiled-out trace macro, and my fixed helper). The evidence record is corrected accordingly.

---

## Previous packet — `A4b2-NR-followup-r1` **EXECUTED 2026-09-27 → gap 1 CLOSED; `O-INCONCLUSIVE` stands, reason now quantified**

- **Packet:** `docs/packets/a4b2-nr-followup.md`, revision **`A4b2-NR-followup-r1`**, class **discovery**,
  frozen SHA-256 **`886620CC6797FC89130B1F89310E6B41C92E809F32820A8133609FBEEC9CC5F5`** (**33 lines**).
  **This is the packet to execute.** Promotion byte-identical with no revision (§5.3).
- **Adequacy:** **`VERDICT: ADEQUATE`** — the **writing Planner's own** review, as §5.8 requires for a
  discovery packet (child `e537374a-aaaf-49c7-9f3a-247bec80e784`, `codex/gpt-6-sol` @ `high`). No second
  Planner; no Muse preflight repeated.
- **Why it exists:** `A4b2-NR-r2` executed → **`O-INCONCLUSIVE`** (`L2=INVARIANT`, `L1` open). Evidence:
  `docs/reviews/a4b2-nonreliance-discovery-evidence.md`. Session verification:
  `docs/reviews/a4b2-nr-followup-r1-session-verification.md`.
- **The three gaps it closes:**
  1. **The unresolved `P 00B9` computed read** (`move x:(r1),b`, `r1 = 0x80 + x:$0000`, guest-written
     mailbox). Closed by exact env-gated GP-only `RECOMP_APU_GP_B9_TRACE=1` instrumentation recording **one
     uncapped event per actual execution** at the `dsp56k_read_memory` call — actual effective address,
     `r1`, `x:$0000`, the derived cross-check, returned value — classifying both `0x1400..0x17ff` **and**
     the `0x0c00..0x0fff` alias, with an **independent execution counter**, contiguity and counter-equality
     checks, no sampling/ring/first-N, trace INVALID on failure, and **no inference of a negative**.
  2. **The six table words** at `P 00CC`–`00D1`: decided as P-memory **data**, **no decoder surgery** —
     carried on the Session's three checks plus the `op =` diagnostic occurring 6× only during decode and
     0× in execution, with the falsifier *"if the table is actually executed or altered, do not reuse this
     conclusion."*
  3. **The completeness bar:** the Planner **retains the full PC-indexed CFG/def-use slice** over every
     feasible route to the first doorbell descriptor and DMA write, and rules the targeted trace
     **insufficient**. An in-range read is **not** automatically `REFUTED` — a concrete feasible causal
     chain to the **named doorbell** output or guard is required; influence on the other audio output is
     not enough.
- **Bounded toolkit scope:** `src/apu/dsp/interp/dsp_cpu.c`, `src/apu/dsp/gp_ep.c`, `src/apu/apu_watch.c`,
  `src/apu/apu_watch.h`, `tests/apu_watch_fixture_test.c`. **Game:** `CMakeLists.txt` (CTest registrations
  only). The committed `r2` work is **baseline, not an authorization**; `src/apu/dsp/dsp.c` is baseline but
  **not** re-authorized. **`A4b1-r4` is not touched.**
- **Closure:** the predecessor's retained `bad-output` control arm is removed at closure **after** its bite
  is preserved; a final absent-env default-control run must establish every diagnostic gate is inert.
- **Outcome rows:** name the next packet per result, including the death branch if non-reliance is refuted.

---

## Previous packet — `A4b2-NR-r2` **EXECUTED 2026-09-27 → `O-INCONCLUSIVE`** (`L1=INCONCLUSIVE`, `L2=INVARIANT`)

**`A4b2-NR-r2` executed under its frozen contract. Row selected: `O-INCONCLUSIVE`.** Evidence:
`docs/reviews/a4b2-nonreliance-discovery-evidence.md`.

**The packet's rule applied faithfully.** `O-TWO-LEG` requires **`L1 = PROVEN` *and* `L2 = INVARIANT`**.
`L2` is satisfied; **`L1` is not**, and the packet's own `L1 = INCONCLUSIVE` definition names the exact
situation — *"unresolved register-indirect/computed/table-driven branch"* — and warns *"do not treat an
indirect edge's absence in one trace as a proof of all feasible edges."* **No strict criterion is
discharged; `A4b2-r7` remains `R2-EXPL-INPUT` and is not retroactively PASS.**

**What was established — every measurement points one way, away from reliance:**

| Finding | Basis |
|---|---|
| **`L2 = INVARIANT`** | Six 30 s runs (baseline, `zero`, `max`, `prng:1`, `prng:2`, `bad-output`). The doorbell tuple `va=803C0810 observed=3 payload=0 dsp_addr=000800` is **bit-identical** across all four input-perturbation arms and the baseline. |
| **The hook demonstrably fired** | 1 623 104 mix-bin reads and 3068 `0xFFFFB3` reads in **every** run; `max`/`prng` changed **1.3M–1.6M** consumed values; first-substituted values differ per seed. `zero` correctly reports `changed=0` (originals were already zero). |
| **The known-bad control bites** | `bad-output` leaves both stub inputs untouched and **suppresses `GP_CLEAR`** (`GP_NONZERO_OVER` latches instead) — so the comparator *can* see a changed doorbell. |
| **`0xFFFFB3` is confined to scratch** | Its four reads (`P 002D`, `0033`, `003A`, `004D`) flow only into `x:$007c`–`x:$007f`, used solely for `cmpu`/`blt` loop control. Nothing reaches `r2`/`r3`, the DMA registers, or the descriptor. |
| **The doorbell descriptor is all immediates** | `P 0004 move #$000800,r2` → `P 00E8 move r2,x:(r0 + 4)`. `dsp_addr=000800` is a literal, matching the observed value exactly. |
| **The image does consume mix bins** | Via a **P-memory data table** at `P 00CC`–`00D1` (`001400 001440 001420 001480 0014A0 001460` = MIXBUF bins 0,2,1,4,5,3), read by `movem p:(r2)+,x0` under `dor #$0006`. That feeds a **different** descriptor (the audio output path), not the doorbell. |
| **Decode identity** | 380 instructions decoded, **0 mismatches** vs `I[i]=LE32(xbe[0x1A7D60+4i])&0xFFFFFF`; one-offset-shift negative control differs on 238 words. |

**The gap that keeps `L1` open:** one unresolved computed read, `P 00B9 move x:(r1),b`, where
`r1 = 0x80 + x:$0000` and `x:$0000` is a guest-written mailbox value. A static decode cannot bound it, so it
cannot be excluded from `0x1400..0x17FF`. The packet forbids treating that as benign.

**A Session error, corrected and recorded.** A first Leg 1 pass claimed *"the image never addresses the mix
buffer"* on the strength of a text search over instruction operands. **That was wrong** — the addresses are
in a **data table**, invisible to an operand grep. It is the same class of error `AGENTS.md` records for the
`PIO_FREE` enumeration. The corrected reading is in the evidence record, and the superseded reading is kept
visible rather than deleted so a reader can tell which stands.

**Next authorized action:** **`A4b2-NR-followup`** discovery packet, targeted at the three gaps the evidence
record names in priority order: (1) the unresolved `P 00B9` computed read — close it with an instrumented
read trace recording the actual `r1`, or bound `x:$0000` from its writer; (2) the decode-rejection of the
six table words, which the Session resolved as data on three independent grounds; (3) the **completeness
argument** — the packet's `PROVEN` bar is a full PC-indexed CFG/def-use slice, and what exists is a targeted
dependency trace. Whether the targeted trace suffices, or the full slice is required, is a **Planner**
judgment.

**Not reopened:** `A4b1-r4` stays accepted and closed; no toolkit file outside the discovery packet's
declared scope was modified (one out-of-scope declaration was made and **reverted** before building).

---

## Previous packet — `A4b2-NR-r2` (**discovery**, promoted 2026-09-27; executed → `O-INCONCLUSIVE`)

- **Packet:** `docs/packets/a4b2-nonreliance-discovery.md`, revision **`A4b2-NR-r2`**, class **discovery**,
  frozen SHA-256 **`7EF30508CCC2588748EAA92E9B56B12D038D425CA925CA360AA9059DFAF33E38`** (**33 lines**).
  **This is the packet to execute.** Promotion was byte-identical with no revision (§5.3).
- **Adequacy:** **`VERDICT: ADEQUATE`**, `BLOCKING: NONE` — the **writing Planner's own** review, as §5.8
  requires for a discovery packet (child `2565775c-9aa9-436b-b917-1efb2d01784a`, `codex/gpt-6-sol` @
  `high`). No second Planner was spawned; no Muse shape preflight was repeated, because the mechanism is
  the one the Advisor itself mandated.
- **Question:** are the `B+0x810` GP DMA doorbell write's **payload, address and executed-path write guards
  independent of both** VP-produced MIXBUF samples **and** placeholder `0xFFFFB3` reads? Output is a
  two-leg **discovery finding**, never a strict `A4b2` verdict.
- **The Advisor's two-leg standard** (`docs/reviews/a4b2-r7-expl-input-ruling.md`): **Leg 1** static
  mechanism from image `I`; **Leg 2** empirical invariance under adversarial perturbation. **Either leg
  failing REFUTES non-reliance.**
- **Outcome rows:** **`O-REFUTED`** (the death branch — strict `A4b2` **BLOCKED** until real VP
  implementation plus `0xFFFFB3` resolution; next `A4b-VP-real-implementation` and
  `A4b-B3-register-resolution`); **`O-TWO-LEG`** (both legs valid → `A4b2-r8` may cite both and the
  Advisor's input-specific conditional ruling); **`O-INCONCLUSIVE`** (→ `A4b2-NR-followup`). An unresolved
  leg **never rounds up**.
- **Bounded toolkit write scope:** `src/apu/dsp/interp/dsp_cpu.c`, `src/apu/dsp/dsp.c`,
  `src/apu/dsp/gp_ep.c`, `src/apu/apu_watch.c`, `src/apu/apu_watch.h`,
  `tests/apu_watch_fixture_test.c` (selector-control assertions only). **Bounded game write scope:**
  `CMakeLists.txt` (only new process-isolated CTest registrations for that existing fixture target). No
  other toolkit or game source. **`A4b1-r4` is NOT reopened** — this is new packet work.
- **Session verification:** `docs/reviews/a4b2-nr-r2-session-verification.md` — every command verified to
  run; one **blocking** scope defect found in `r1` and repaired in `r2`.
- **Closure:** instrumentation off by default; the `bad-output` control arm removed at closure; final
  absent-env control run verifies inertness. Evidence record:
  `docs/reviews/a4b2-nonreliance-discovery-evidence.md`. **Exploratory/diagnostic evidence is knowledge
  only, never acceptance of a strict criterion.**

---

## Previous packet — `A4b2-r7` **EXECUTED 2026-09-27 → `R2-EXPL-INPUT`** (claim NOT established; Advisor ruled)

**`A4b2-r7` executed under its frozen contract. Row selected: `R2-EXPL-INPUT`.** Evidence:
`docs/reviews/a4b2-r7-execution-evidence.md`. The row was **predicted in advance** by the adequacy
reviewer (`docs/reviews/a4b2-r7-adequacy-review.md`) and independently by the Session from the archived
`r6` bytes, so it is not a surprise.

**Four of five criteria PASS — the GP clear is real and well-attributed:**

| Criterion | Result |
|---|---|
| `AC-DEFAULT2` | **PASS** — R0 `diagnostic_deadline`, `Wf0=3`, `F0=2`, zero `[GP*]` lines, no TRAP/TRACE in env |
| `AC-BOOT` | **PASS** — one complete `n=1` block, 64 pram lines, last counts `boots=1=N`, common `sge0_va = T = 0x803C0000`, anchor cross-check `0x803C0810 − 0x810 = B`, **371/371 image words equal**, control differs |
| `AC-RUN` | **PASS** — `boots=1, gp_frames=768, gp_insns=33120534` |
| `AC-CLEAR` | **PASS** — `GP_CLEAR seq=198852 va=803C0810 observed=00000003 insns=124652 dsp_addr=000800`, `CPU_ANCHOR seq=198851` immediately before, `[GPDMA] watch … cas=ok` |
| `AC-NOCPU` | **PASS** — six-site reconciliation exact (6 found = 6 frozen, 0 missing/extra); no ack in env, source or exe; `CPU_ZERO=0` everywhere, no contested zero |
| **`AC-INPUTS`** | **FAIL** — steps 1–2 valid; step 3 finds named stub reads |

**Why `AC-INPUTS` fails — three named stub conditions, all in the `at_clear` freeze:** PERIPH index
**`0x33` (`0xFFFFB3`) reads = 1017** (the placeholder `v = 0; // core->num_inst; // ??`); MIXBUF
**`reads_while_stub` nonzero in 26 of 32 bins** (each `7648`); and **`mixbuf_stub_read = 1`**. The only
other nonzero PERIPH offsets (`0x45`, `0x56`) are inside the five-offset modelled set. DMA's nonzero class
is `CONTIG`, guest-written by region.

**So: the `3→0` GP memory-write-path transition at `B+0x810` is observed and attributed, but it is
EXPLORATORY for the named stub inputs — the packet's *Establishes* claim is NOT satisfied.** No `R2-PASS`.

**Next action (the row's own direction): the Advisor has ruled.** Verbatim:
`docs/reviews/a4b2-r7-expl-input-ruling.md`.

> **Next packet: ONE discovery packet** (§5.8) proving or refuting **non-reliance** of the doorbell exchange
> on **both** stub inputs, to a **two-leg standard**: **Leg 1 (static mechanism)** — from image `I`
> (`0x173` words), identify the doorbell write(s) and show its payload, address and every branch guarding
> the write on the executed path are independent of mixbin and `0xFFFFB3` values; **Leg 2 (empirical
> invariance)** — perturbation runs varying consumed stub values adversarially (all-zero, all-ones/max,
> pseudorandom) must reproduce the **identical** doorbell latch (`va`, `observed=3`, `payload=0`,
> `dsp_addr`) with frames advancing. **Either leg failing REFUTES non-reliance.**
>
> **The discovery's outcome table must carry an explicit death branch:** if non-reliance is refuted, the
> strict `A4b2` claim is **BLOCKED** until real VP implementation (not a model) plus `0xFFFFB3` resolution,
> and `A4b2` retires or waits.
>
> **A MIXBUF value-model is RULED OUT** — inadmissible under `docs/jsrf-run-profiles.md` criterion 4
> (mixbin contents are the *result* of VP voice-mixing work consumed by the GP whose execution is being
> certified, so supplying them without the VP is synthetic completion). The "idle bins read 0" escape is
> **closed by measurement**: the `MIXBUF_STUB_READ` latch reads `observed=1` (a VP voice was genuinely
> active). **A `0xFFFFB3` model is PREMATURE** until the register is identified. **No Planner time goes
> there.** `0xFFFFB3` register identification rides along as a parallel thread.
>
> **Conditional case ruling (confined to these two inputs):** Q1 condition 2's *purpose* — per its own
> "Why" and its per-value "each value it **relies on**" rule — is satisfied for these two inputs by a
> **completed two-leg non-reliance proof**; the follow-up `A4b2` revision may then cite the discovery and
> restate its input qualifier accordingly, with **no model packet** for them. This substitutes a **higher**
> bar (proof of non-reliance) for a blanket proxy (zero reads); it waives **no** evidence, sets **no**
> precedent beyond inputs with a completed proof, and does not touch Q1's text for any other input.
>
> **The perturbation hook is discovery instrumentation** (§5.8: reversible, env-gated, off by default at
> closure) — a **new packet's toolkit scope**, **not** a reopening of closed `A4b1`. **`PIO_FREE` ordering
> is unconstrained**; natural order is this discovery first, `PIO_FREE` after, but **sequencing is the
> Planner's call**.

**`r7`'s `R2-EXPL-INPUT` is terminal for `r7`** — the strict claim is **not established**, and no part of
the ruling converts it. `r6`'s `R2-UNKNOWN` remains terminal for `r6`.

**Also recorded:** the A2h displacement did **not** block this decision. Under `r7` no PASS-path claim reads
the R1 dump, so the R1 `content-mismatch: 1` was correctly **not** a gate; the displaced-dump `Wf` was not
read, and the no-`GP_CLEAR` branches that would have required it were not reached. That is the `r7` repair
working as designed. `r6`'s artifacts discharged nothing.

**Open lead, unchanged:** the **A2h displacement writer** (characterized, never identified;
`docs/jsrf-operating-history.md:1000-1045`). It no longer gates this claim but still gates later
progress-past-spin claims — the trapped run dies at ~4.8 s.

---

## Previous packet — `A4b2-r7` (promoted 2026-09-27, `ADEQUATE`; executed → `R2-EXPL-INPUT`)

- **Packet:** `docs/packets/a4b2-gp-clears-pending-word.md`, revision **`A4b2-r7`**, class **change**,
  frozen SHA-256 **`93E410C3742D0E290A0C0BBA71DCFEC547A77ECAC497E389BA5D58026E551F95`** (**136 lines**).
  **This is the packet to execute.** Promotion was **byte-identical with no revision**, as §5.3 requires —
  the file still reads `Status: draft` in its own text, exactly as `A4b1-r4`'s and `A4b2-r6`'s frozen files do.
- **Adequacy:** **`VERDICT: ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED` — recorded
  **verbatim** in `docs/reviews/a4b2-r7-adequacy-review.md` (fresh Planner child
  `7c6fd178-49cf-4f00-af5f-3401f3b6d85c`, `codex/gpt-6-sol` @ `high`). The authoring child
  `b430c48f-42ea-463f-99e6-97da5989e73a` did not review it.
- **Why `r7` exists — §5.4(1):** `A4b2-r6` was executed and selected **`R2-UNKNOWN`** because gate **G2**
  failed for R1. A **no-A4b2-edit control** reproduced that failure identically, proving the cause is a
  **pre-existing guest defect** (the A2h dump displacement, exactly `0x37608`), not this packet's edits.
  The Advisor ruled `AC-CLEAR`'s binding of `B` to the **post-mortem dump** a blocking defect with a
  concretely realized **false UNKNOWN**, and mandated four repairs, all implemented:
  **`B` rebound to log evidence** (common `sge0_va`, cross-checked against `anchor va − 0x810`); a new
  **independent title-page oracle `T = 0x803C0000`** so `AC-BOOT` is not circular; **G2 deleted** as a
  global PASS gate (no PASS-path claim reads the R1 dump); **`Wf`** meaningful only under a passing R1
  mapping check, and the no-`GP_CLEAR` branches guarded so a displaced dump yields UNKNOWN rather than a
  false `R2-UNATTRIBUTED`/`R2-CPU`/`R2-NOCLEAR`. Ruling:
  `docs/reviews/a4b2-r6-execution-ruling.md`; execution record: `docs/reviews/a4b2-execution-evidence.md`.
- **`r6`'s `R2-UNKNOWN` is terminal for `r6`** — not reopened, not rescored. **`r6`'s run archives are
  STALE for `r7`** (`packet:42`, `:135`): `r7` re-executes **fresh** R1/R0 under this contract.
- **Claim (Establishes, on `R2-PASS` only):** in one STRICT run with `RECOMP_GPU_ACK=0` and
  `RECOMP_APU_TRAP=1` plus observation-only APU trace, the **`3→0` transition at `B+0x810` was performed by
  the GP engine's memory-write path while executing validated image `I`**, after the anchored store was
  recorded, with no synthetic ack, no instrumented competing CPU zero, and all pre-exchange GP inputs
  guest-written by region or modelled. **It does not establish** that the guest observed the `0`, exited the
  spin at `loc_001A18D0`, or progressed past it.
- **Baseline:** game `a000662aef3bb4d7088f3fa367d19e7ce7c157df`, toolkit
  `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d` (clean). **Step 1's game-only edits are already applied** in
  the working tree (40 insertions, 0 deletions) and `r7` says to verify and preserve them, not duplicate them.
- **Write scope (game only):** `src/recomp/gen/recomp_0000.c`, `src/recomp/gen/recomp_0005.c` (step-1 calls
  and their `extern` declarations only), `src/diagnostics.c` (the `jsrf_watch_store` forwarder only),
  `docs/reviews/a4b2-*.md`. **No toolkit file.** Build/run owner: the Session.
- **Expected row, stated in advance by the adequacy reviewer:** a faithful fresh repetition is predicted to
  select **`R2-EXPL-INPUT` or `R2-UNKNOWN`, not `R2-PASS`** — the archived `r6` `at_clear` block already
  prints `mixbuf_stub_read=1` with **26 of 32** MIXBUF bins showing nonzero `reads_while_stub` (Session
  independently reproduced). `R2-EXPL-INPUT` routes to the **Advisor** to decide whether that named stub
  input needs its own model packet first. This prediction is **not** a defect in `r7` and is **not**
  permission to rescore `r6`.
- **Next after `R2-PASS`:** the `PIO_FREE` model packet, **before** any strict liveness/boot claim past the
  spin. `R2-NOBOOT`/`R2-NOFRAMES`/`R2-NOEXEC`/`R2-NOCLEAR` route to `A4c` discovery.
- **Open lead, cross-referenced (not newly mandated):** the **A2h displacement writer** was characterized
  (`docs/jsrf-operating-history.md:1000-1045`) but never identified; `A2h-r6` was retired on a *different*
  refuted premise, so the writer investigation **stays open**. It is on the critical path for later
  progress-past-spin claims — the trapped run dies at ~4.9 s — but it **does not gate** this claim.
- **Planning history (non-authoritative):** `docs/reviews/a4b2-revision-history.md`. `r1`–`r3`
  `INADEQUATE` on `[GPIN]` accounting; `r4`–`r5` `INADEQUATE` on `AC-BOOT`; `r6` `ADEQUATE` then executed
  → `R2-UNKNOWN`; `r7` `ADEQUATE` on the §5.4(1) evidence-binding ground.

**Route note:** the old `claude` route stays unusable and is not re-probed. Current DSH staffing is
`docs/agent-workflow.md` §1: Session/Workers `workbuddy-ai/deepseek-v4.1-flash` @ `max`; Planner and final
adjudicator **GPT-6 Sol** @ `high` (`provider: codex`); Advisor **Muse Spark 1.3** @ `max`
(`muse-worker`, handle `muse_FkNhGaXtV9P5`); acceptance stage 1 `hy4-preview-f` @ `high`; stage 2
`deepseek-v4.1-flash` @ `max`.

---

## Previous packet — `A4b2-r6` (promoted 2026-09-27, `ADEQUATE`; executed → `R2-UNKNOWN`; superseded by `r7`)

**`A4b2-r6` was executed on 2026-09-27 and selected row `R2-UNKNOWN`** because gate **G2** failed for R1.
**That UNKNOWN is terminal for `r6`** — failure stays failure, no rescoring, and no PASS survives from its
artifacts. Evidence: `docs/reviews/a4b2-execution-evidence.md`. Ruling:
`docs/reviews/a4b2-r6-execution-ruling.md`.

**What execution established (observed, positive — the packet's own subject):** the ported GP engine's DMA
write path took the `3→0` CAS at `0x803C0810` while executing image `I` — `GP_CLEAR seq=198852
observed=00000003 insns=124652 dsp_addr=000800`, with `CPU_ANCHOR seq=198851` immediately before it,
`[GPDMA] watch … cas=ok`, `CPU_ZERO_OVERFLOW=0`, no synthetic ack, and **`AC-BOOT`'s image comparison
passing 371/371 words** with a differing control. The rerun reproduced `seq=198852` identically.
`AC-DEFAULT2` is **PASS** (R0 `diagnostic_deadline`, `Wf=3`, `F=2`, zero `[GP*]` lines).

**Why G2 failed — a pre-existing guest defect, not this packet's edits.** A **no-A4b2-edit control**
(all three edits reverted, rebuilt, identical command) crashes **identically**: same
`unhandled_exception`, same `[ICALL] invalid target 0x00000000 return=0014982E`, same
`requested 598869040`, same `content-mismatch: 1`. The displacement is **exactly `0x37608`** — the A2h
separation — and the same signature exists in a **pre-`A4b1`** archive (`20260922-224429-003-a2g-304f0-span`,
2026-09-22). **`A4b1-r4` is not implicated and remains accepted and closed.**

**Next packet: `A4b2-r7` — a §5.4(1) revision** (a blocking finding from execution with a concrete
false-UNKNOWN scenario; **not** §5.4(2) — no load-bearing premise was invalidated). The Advisor's mandated
repair shape:

1. **Rebind `B` to validated log evidence** — e.g. `sge0_va` from the every-instance-validated `[GPBOOT]`
   block(s), cross-checked against `anchor va − 0x810`; multi-boot `sge0_va` disagreement → UNKNOWN.
   **`AC-BOOT`'s `sge0_va = B` conjunct must be restructured with it**, or it becomes tautological.
2. **No PASS-path dependence on dump integrity** — **G2 as a PASS-gate goes with it** (delete or demote;
   Planner's choice, one-line reason recorded).
3. **Displaced-dump `Wf` (reads 0) must not be recorded as meaningful corroboration** — qualify by mapping
   integrity or drop it.
4. **`r6`'s run artifacts are STALE for the revision** (§2.2.5, §2.4.7): the revision **re-executes** new
   R1/R0 runs under the new contract; the `r6` logs are leads and premise evidence only.

**The Planner writes the predicates.** Next: revision → **full fresh §5.3 adequacy review by a
non-authoring Planner** → re-execution → decision from log evidence.

**Open lead, cross-referenced (not newly mandated):** the **A2h displacement writer** was characterized
(`docs/jsrf-operating-history.md:1000-1045`) but never identified; `A2h-r6` was retired on a *different*
refuted premise, so the writer investigation **stays open**. It remains on the critical path for later
progress-past-spin claims — the trapped run dies at ~4.9 s — but it **does not gate** this claim.

**Route note:** the old `claude` route stays unusable and is not re-probed. Current DSH staffing is
`docs/agent-workflow.md` §1: Session/Workers `workbuddy-ai/deepseek-v4.1-flash` @ `max`; Planner and final
adjudicator **GPT-6 Sol** @ `high` (`provider: codex`); Advisor **Muse Spark 1.3** @ `max`
(`muse-worker`, handle `muse_FkNhGaXtV9P5`); acceptance stage 1 `hy4-preview-f` @ `high`; stage 2
`deepseek-v4.1-flash` @ `max`.

---

## Previous packet — `A4b2-r6` (promoted 2026-09-27, `ADEQUATE`; executed → `R2-UNKNOWN`)

- **Packet:** `docs/packets/a4b2-gp-clears-pending-word.md`, revision **`A4b2-r6`**, class **change**,
  frozen SHA-256 **`3273409262437D42A500A1E988E39CB25234C2E5E2A96007A9BE74FEB6B250E5`** (**134 lines**).
  **This is the packet to execute.** Promotion was **byte-identical with no revision**, as §5.3 requires —
  the file still reads `Status: draft` in its own text, exactly as `A4b1-r4`'s frozen file does.
- **Adequacy:** **`VERDICT: ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED` — recorded
  **verbatim** with the reviewer's child ID and route in `docs/reviews/a4b2-r6-adequacy-review.md` (fresh
  Planner child `36e21ac4-ffcf-49b6-a3ae-80abe39da281`, `codex/gpt-6-sol` @ `high`). The authoring child
  `636dc591-2e30-41fb-8cf6-46d108135165` did not review it.
- **Claim (Establishes, on `R2-PASS` only):** in one STRICT run with `RECOMP_GPU_ACK=0` and
  `RECOMP_APU_TRAP=1`, the **`3→0` transition at `B+0x810` was performed by the GP engine's memory-write
  path while executing validated image `I`**, after the anchored store was recorded, with no synthetic ack,
  no instrumented competing CPU zero, and all pre-exchange GP inputs guest-written by region or modelled.
  Every recorded GPRST bootstrap loaded words equal to `I`. **It does not establish** that the guest
  observed the `0`, exited the spin at `loc_001A18D0`, or progressed past it.
- **Baseline:** game `a000662aef3bb4d7088f3fa367d19e7ce7c157df`, toolkit
  `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d` (clean).
- **Write scope (game only):** `src/recomp/gen/recomp_0000.c`, `src/recomp/gen/recomp_0005.c` (step-1
  calls and their `extern` declarations only), `src/diagnostics.c` (the `jsrf_watch_store` forwarder only),
  `docs/reviews/a4b2-*.md`. **No toolkit file.** Build/run owner: the Session.
- **Runs:** R1 (trap+trace) and R0 (default), 30 s each, `--profile strict`, archived under `logs/runs/`.
- **Next after `R2-PASS`:** the `PIO_FREE` model packet, **before** any strict liveness/boot claim past the
  spin. Rows `R2-NOBOOT`/`R2-NOFRAMES`/`R2-NOEXEC`/`R2-NOCLEAR` route to `A4c` discovery.
- **Planning history (non-authoritative):** `docs/reviews/a4b2-revision-history.md`. `r1`–`r3` were
  `INADEQUATE` on `[GPIN]` accounting; `r4` and `r5` were `INADEQUATE` on `AC-BOOT`. The binding rulings
  that shaped `r6` are `docs/reviews/a4b2-r4-planning-rulings.md` and
  `docs/reviews/a4b2-r4-advisor-blocker-ruling.md`; the two verdicts are
  `docs/reviews/a4b2-r4-adequacy-review.md` and `docs/reviews/a4b2-r5-adequacy-review.md`.

---

## Last closed packet — `A4b1-r4` (GP/DSP core port) — **ACCEPTED 2026-09-26, `R1-PASS`, PUSHED (complete)**

**`A4b1-r4` is accepted and its closure push is done. It must not be reopened.** `A4b1` is accepted with
exactly its "Establishes" claim: the pinned xemu GP core (interpreter only), GP MMIO routing and one GP
DMA write choke point are in the toolkit with per-file provenance; the synthetic ack is gone from source
and exe; a thread-safe watched-word ledger is exported and fixture-exercised; the combined work's licence
is recorded; the tree builds and every ctest passes; one STRICT default run matches the `A4s` baseline
stop. **The core is reachable only through GP MMIO, which requires `RECOMP_APU_TRAP`.**

**Next packet: `A4b2` → `A4b2-r4` — ✅ PLANNING RESUMED (owner decision, 2026-09-27).**

> **RESUMED: the staffing authority was replaced by owner decision.** The `§1` roster no longer assigns
> the Planner or the persistent Advisor to `claude/claude-opus-5-5`; that route's failure and the
> eight-probe ladder remain recorded in `docs/reviews/blocker-20260926-claude-route-down.md`, and the
> suspension it produced is **lifted**. The owner instruction that replaced the staffing, the Advisor
> ruling confirming the resumption, and the Session's re-verification that no load-bearing premise
> changed are recorded together in **`docs/reviews/owner-staffing-resumption-20260927.md`**.
>
> **Current DSH staffing** (`docs/agent-workflow.md` §1): Session and Workers
> `workbuddy-ai/deepseek-v4.1-flash` @ `max`; **Planner and final acceptance adjudicator GPT-6 Sol** @
> `high` (`provider: codex`, `LIVE_RESOLVE`); **persistent Advisor Muse Spark 1.3** @ `max`
> (`muse-worker`, handle `muse_FkNhGaXtV9P5`); acceptance stage 1 `hy4-preview-f` @ `high`; stage 2
> `deepseek-v4.1-flash` @ `max`.
>
> **Preserved and still valid — nothing was lost:** the Planner's **complete sketch** (packet lines 1–25)
> over the **intact `A4b2-r3` body** (27–334); the Advisor's **`SHAPE: PROCEED`** ruling recorded verbatim
> in `docs/reviews/a4b2-r4-planning-rulings.md`; the Session's pre-preflight verification
> (`a4b2-r4-sketch-verification.md`); and the `0xFFFFB3` decision-class note applied to
> `a4b1-r4-acport-step4-enumeration.md`. **Work resumed at "write the `A4b2-r4` body" — no re-planning
> and no re-preflight**, because the sketch was already cleared and the ruling is recorded. The old
> Planner child `cee46374-…` is retired with the old configuration and is not reused.
>
> **What the `A4b2` planning already settled, so it is not re-litigated on resume:**
> grounds are **§5.4(2) + §5.4 After-INADEQUATE + §5.4(3)** (not "only a re-bind"); five edits approved
> in substance; the `803CC000` GPSADDR conjunct is demoted to a recorded value (the Advisor confirmed
> **its own boundary note missed this**); `AC-INPUTS` decides from the **first `[GPIN] at_clear` block**
> **on a stated inference plus two can-fail checks**; **`0xFFFFB3` is STUB, not modelled** — the modelled
> set is **five** offsets (`0x45`, `0x54`–`0x57`) with a claim limit on `0x56`; **P3** (EP unreachability)
> is bound to the **build-identity commit**, not "HEAD"; and the **NDEBUG** coverage claim is narrowed to
> a claim limit on `R2-PASS`.

**Route-availability note (unchanged in effect):** the old `claude` route stays unusable and is not
re-probed; the current roster above is what §1 requires. A required route that is unavailable is still
`BLOCKED` under §1 — never a silent fallback.

| Result | Value |
|---|---|
| Gates | **G1** `STRICT`; **G2** `matches 1, content-mismatch 0`; **G3** readable; **G4** `L = 6748`, `goto` at `6751`; **`F = 2`** (matches the `A4s` baseline) |
| `AC-PORT` | **PASS** — stage 1 `DISAGREED` (correctly), corrected, **stage 2 `AGREED`** |
| `AC-LIC` | **PASS** |
| `AC-FIX` | **PASS** — **14/14 ctest**, 17 cases, both can-fail twins **mutation-proven** |
| `AC-DEFAULT` | **PASS** — `diagnostic_deadline`, `W = 3`, `F = 2`, all counts 0, **positive control 954 vs 0** |
| Row | **`R1-PASS`** |
| R0 | `logs/runs/20260926-010303-411-a4b1-default` |
| Acceptance | stage 1 `NOT ACCEPTED` (`fb109c49-…`, Hy4 @ `high`) → stage 2 **`ACCEPT`** (`f3f02108-…`, DeepSeek @ `max`) |
| **Pushed** | **`origin` (the fork), `main`, `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d`**, fast-forward `3f8bf67..3a3c7c1`, 8 commits. **`upstream` untouched** at `766ecef`. No force used. |
| Toolkit | HEAD `3a3c7c1…`, clean |
| exe | `B13521858A344919731E73F6E602186E951A49D7E57CA4E929BEA15CC2736ADD` |

**Two Advisor rulings were needed during execution**, both recorded verbatim in
`docs/reviews/a4b1-r4-execution-rulings.md`: the `FIFO_READ` ruling **(C)** — `AC-FIX (viii)` found a
**genuine hook-completeness gap** (the pinned DMA read arm falls through because its `assert` is
`NDEBUG`-elided, consuming stale bytes as a GP input, unrecorded) — and its **addendum**, which caught a
defect in the Session's own fix (`dsp_dma.c` is shared by the GP and the EP, so the hook needed an
`is_gp` gate).

**Stage-1 acceptance then blocked on `AC-PORT`**, and the block was correct: the step-4 enumeration's
write-callback row was factually wrong (`ep_scratch_rw` recorded as not reaching the choke point, citing
an `is_gp` gate that does not exist). The Session verified it and found the error **larger than reported —
three false cells, not one**; `gp_fifo_rw` and `ep_fifo_rw` also reach it via
`circular_scatter_gather_rw` and were not listed. **All four pinned write callbacks reach the choke
point.** Root cause: the generating script computed no reachability, so that column was hand-written.

> **Pattern recorded for the next packet.** **Three** completeness/correctness claims in this packet were
> wrong and **all three were caught by adversarial review, not by the Session's own checking**: the
> missing `FIFO_READ` hook, the enumeration, and (twice) the Session's first reading of a premise being
> reversed by the Advisor. Two were "every X" claims. **For `A4b2`, derive enumerations and reachability
> from source rather than writing them by hand, and treat any "every"/"all" claim as the highest-risk
> sentence in the document.**

**New `A4b2` decision input, carried forward (do not silently absorb).** Because `ep_scratch_rw` and
`ep_fifo_rw` reach the choke point, **an enabled EP can land an exchange on `W_va` and take
`GP_CLEAR`**. The EP is gated on `EPRST` (`gp_ep.c:650`) and runs every 8th frame (`:652`), but the path
is real. The Session does **not** rule on whether it is benign — that is a Planner/Advisor judgment, and
`DS5`'s text ("from every pinned write callback") already requires the shared choke point, which holds.

**Follow-up leads carried into `A4b2` planning (recorded, not done):**
1. **`NDEBUG`-elided asserts** — enumerate those in `src/apu/dsp/` that change GP state or inputs. Known:
   `unk2`/`unk13`, the `format` default, `dsp_offset` out of range. *(Advisor follow-up lead.)*
2. The classifier's treatment of the inert `RECOMP_APU_DSP_ACK`.
3. The game repository has no licence file.
4. EP routing.
5. `gp_ep.c:382-387` (the `GPBOOT` trace block) reads guest memory via `apu_guest_dma_ptr` **without**
   `apu_gp_dma_read` — trace-gated, feeds only the printed line, not a consumed GP input. *(Stage-2
   advisory 2.)*

Evidence: `docs/reviews/a4b1-execution-evidence.md` (the index), `a4b1-r4-implementation-verification.md`,
`a4b1-r4-fixture-green.md`, `a4b1-r4-acport-step4-enumeration.md`, `a4b1-r4-execution-rulings.md`,
`a4b1-r4-stage1-acceptance-review.md`, `a4b1-r4-stage2-acceptance-review.md`.

- **Packet:** `docs/packets/a4b1-gp-core-port.md`, revision **`A4b1-r4`**, class **change**, frozen
  SHA-256 **`6DD62A57E87445F5C12085210146204AA4E77D26FD316071FD41CAEC76835C38`** (**445 lines**).
  **This is the packet to execute.** Promotion was **byte-identical with no revision**, as §5.3
  requires. The superseded `A4b1-r3` (`321ABCF7…`) remains recoverable from git
  (`git cat-file blob 3f05c0f95205bfef06c0d75a0ba1038b670e9550`).
- **Adequacy:** **`VERDICT: ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS: PASS` — recorded
  **verbatim** with the reviewer's child ID and route in `docs/reviews/a4b1-r4-adequacy-review.md`
  (fresh Planner child `5db5fd71-b1db-479d-9e5c-14043d54e211`, `workbuddy-ai/kimi-k3`). It was the
  **full formal §5.3 review** the Advisor mandated, not a delta re-review, and it checked **all six**
  mandated scope items individually. It independently re-verified the packet SHA, reproduced the
  load-bearing toolkit citations at `M`, confirmed the **8 live `0x03FFFFFF` sites**, and confirmed the
  packet uses the **Advisor's** DS3 rather than the Session's superseded formula.
- **Why this packet is larger than a re-baseline — and correctly so.** It carries **two** revision
  grounds: **(1)** a **§5.4 After-INADEQUATE** repair of `A4b1-r3`'s blocking defects **B1** (the
  `[GPIN]` 256-entry table's key space exceeded the MIXBUF universe → false `R2-UNKNOWN`) and **B2**
  (`sge0` and the counts-line `seq` undefined and unasserted → false `R2-NOBOOT`); and **(2)** a
  **§5.4(2) `PREMISE_CHANGED`** re-bind to `M`. **No `A4b1` revision had ever been `ADEQUATE`; `A4b1-r3`
  is superseded.** The Planner found the `[GPIN]` lineage conflict; the Advisor ruled the redesign **in
  scope**, because it is caused by the inadequate verdict, **not** by A4s changing files.
- **Advisor rulings (binding, verbatim):** `docs/reviews/a4b1-r4-planning-rulings.md` — shape
  **`PROCEED`**; `[GPIN]` = the **finite-universe/provenance-class design in full**; identifier
  **`A4b1-r4`**; and **part 4 — P-F is load-bearing**, the forward map is now **non-injective**, so DS3's
  inverse is rewritten (the Session's earlier formula was **wrong** and is superseded).
- **What the packet carries:** DS3's four-case inverse (window-VA **first**, then high-water
  `XBOX_CONTIG_BASE + P`, then mapped low-RAM identity, then fail closed), citing `dma_resolve` **and**
  `xbox_memory_layout.c:843`, with `GPDMA_AMBIGUOUS` and the aliasing claim limit; DS6's **full**
  finite-universe `[GPIN]` design (MIXBUF `[NUM_MIXBINS = 32]`, PERIPH `[128]`, FIFO `[6]`, DMA region
  classes `[4]`, `BOOT_SCRATCH_READ`, `GPIN_OUT_OF_UNIVERSE` as a bug detector, `at_clear` freeze at
  `GP_CLEAR`, capped observation, fixed-count emission) with the r3 table / `GPIN_OVERFLOW` / per-key
  lines / cut-off **retired and named as must-not-reappear**; DS5's CPU-site table sized
  one-per-enumerated-`jsrf_watch_store`-site with `CPU_ZERO_OVERFLOW` a bug detector; DS7's `[GPBOOT]`
  printing **both** `sge0` (raw) and `sge0_va` (translated); and AC-FIX `(viii)` kept with
  `(ix′)/(x)/(xi)` replacing `(ix)`, and `(vii)` in **two** forms that can fail in **both** directions.
- **`A4b2` boundary note is in the packet** — `A4b2-r3`'s precondition P2 and its `AC-INPUTS` become
  **stale**; the `A4b2-r4` re-brief items are recorded there. **`A4b2` is not revised here.**
- **Regeneration is a non-goal**, so the pytest gated lead **stays gated and unexercised** — no `pytest`
  work is manufactured.
- **Deferred advisories (recorded, not reopening the packet):** AC-FIX `(v)`'s `CPU_ZERO_OVERFLOW ≥ 1`
  is a deliberate lower bound under finite-universe sizing (the can-fail property is preserved by `(x)`);
  a soft "where the snapshot records them" qualifier carried from r3; and two observation-only subfields
  that **no row reads**.
- **Next action: execute `A4b1-r4` literally**, beginning with its **P0/Readiness** gates. `A4s` is
  complete; `A4b1` reaching a durable disposition is the precondition for `A4b2`.

### Predecessor — `A4s-r6` (toolkit sync) — **ACCEPTED 2026-09-25, `R-SAME`, PUSHED** (complete)

> **RESULT: the merge succeeded, the strict stop is UNCHANGED, and `M` is ACCEPTED and PUSHED.**
> Every gate passed. Toolkit `main` is at **`M` = `3f8bf67c450861aefcbc376698750bc1446bc9dd`** (parents
> `0d7929c`, `766ecef`), clean, and now **published to the owner's fork**.
>
> - **Acceptance:** **stage-1 `ACCEPT`**, `BLOCKING: NONE`, **all ten mandatory criteria `AGREED`**
>   (`docs/reviews/a4s-r6-acceptance-review-stage1.md`; reviewer child
>   `f6ba7864-07ae-4388-9583-9a265e63c935`, `workbuddy-ai/hy4-preview-f` @ `high`, route verified live).
>   Per §2.2 a first-stage `ACCEPT` is **final** and is not passed to a second stage. State: **accepted**.
> - **Closure push — DONE** (owner policy, all five checks): `main` → `origin`, fast-forward
>   `766ecef..3f8bf67`. Verified: `git rev-parse origin/main` = `M` and `git ls-remote origin
>   refs/heads/main` = `M`. **No force, no `upstream` push** (`upstream/main` still `766ecef`, untouched).
>   Tracking moved to `origin/main` (recorded, not reverted). Log:
>   `docs/reviews/owner-push-policy-xboxrecomp-fork.md`.
>
> **The reviewer independently reproduced more than the Session measured** — worth keeping: positive
> controls for `AC-MERGE` (b)/(c) (**143** and **28** excess on the two parents, so those checks *can*
> fail); a positive control for the `|= MCPX_AC97_CODEC_READY` grep (**1** on `766ecef`, so the zero at
> `M` is meaningful); **link falsification** for `AC-BUILD` (all **147** sources in the run's
> `build-source.json` hash-match the `M` worktree); an independent conflict-set guard via
> `git merge-tree --write-tree`; and **AST-based** verification of hunks 5/6/9 instead of text
> comparison.
>
> **One documentation error it caught was CORRECTED:** `docs/reviews/a4s-r6-ac-merge.md` had named the
> hunk-8 function `test_seeds_align_16`, which does not exist; the hunk sits in
> `test_seeds_drops_unaligned_targets` (L96), where HA-LOCAL is correct. A record error, not a resolution
> defect.
>
> **Deferred advisories (recorded, not reopening the packet):** step-4 evidence-table labelling (raw
> preview **15** vs resolved-all-ours **3** should be labelled separately); the `AC-STRUCT`
> duplicates-not-omissions limit; the pre-existing set-G failure; the 13-byte PE-timestamp exe delta; and
> the reviewer's own disclosed instrument errors.
>
> **Two claim limits and one gated lead carry forward** — see the block at the end of this section.
>
> - **Step 1** — recovery point discharged as **verify-and-reuse** of the pre-existing `a4s-pre-sync`
>   (per **Q-B**); nothing created, deleted, or force-moved.
> - **Step 2** — controls: build OK; `ctest` 12/12; K4 27 tests OK; KX 33 modules / Ran sum 133 with zero
>   per-module differences; set G 11 files / 10 pass / 1 fail (carve-out by identity).
> - **Step 3** — the **9-hunk guard passed exactly** (5 files, 9 hunks, every section and line span
>   matching); all 9 hunks resolved **by exact text** per the pre-ruled table.
> - **Step 4 `AC-STRUCT` PASS** (0 findings, 0 UNKNOWN; all controls green) — and it **caught a real
>   defect in the Session's own first resolution**: edit (b) implemented as a blanket text delete left
>   switch 4 with a duplicate `case 138` and **switch 5 with none at all**, silently dropping ordinal-138
>   dispatch, which the build would never have caught (`docs/reviews/a4s-r6-ac-struct-catch.md`). The
>   merge was aborted, re-run deterministically, and the rule corrected to *keep the first occurrence per
>   switch* — the **operative reading of edit (b)**, confirmed by the Advisor.
> - **Step 5 build PASS** (relinked; no structural diagnostics, so the `R-BUILD` backstop never arose).
> - **Step 6 `AC-TEST`** — C **12/12** incl. the named witness `jsrf_inplace_event_bridge`; G same
>   failing identity as step 2; K4 30 tests OK; KX 56 modules with **0 regressions, 0 lost coverage**.
> - **`AC-MERGE` PASS** (16/16 targeted checks; the `(d-twin)` deletion witness took four attempts and
>   all three failures were mine — *"credited"* is load-bearing, `docs/reviews/a4s-r6-ac-merge.md`).
> - **`AC-KEEP` (iv)/(v)/(vi) PASS** — three model ranges byte-identical (anchor counts 1/1/1, working
>   can-fail controls); deleted names **0** in `SCOPE`; all four controls match **2/2/0/0**; and the
>   **(vi) `[A3A]` witness on the run: `gc=0x00000002 gs=0x00000100`** — GC bit1 and GS bit8 both set,
>   matching the reference control exactly.
> - **`AC-INV` PASS** — new env names exactly `RECOMP_APU_MIXDOWN_ALL` + `RECOMP_USB_PORT`, none removed.
> - **`AC-GEN` PASS**, **`AC-NOPUSH` PASS** (`ahead 25`, remotes exact, no push/fetch during execution).
> - **`M` conforms to Appendix A verbatim** — HA-COMBINED-1/-2 and Expected-5/6/9 byte-for-byte, plus
>   10/10 structural checks (`docs/reviews/a4s-r6-appendix-a-conformance.md`).
> - **Step 7 — the one strict run:** `logs/runs/20260925-210315-113-a4s-sync-strict`. **V holds**
>   (STRICT; dump `matches: 1, content-mismatch: 0`); **`outcome = diagnostic_deadline`**;
>   **`B = 0x803C0000`**; **`W = 3`**; **`F = 2`** (identical to the `A4a-r2` R0 control). **No rerun.**
> - **Row `R-SAME`:** V holds; `outcome = diagnostic_deadline`; `B` usable and `= 0x803C0000`; `W = 3`;
>   `F ≥ 1`. All six gate rows and `R-INVALID`/`R-MOVED` were evaluated first and are unmatched. **The
>   stop is unchanged — still the DSP pending-word spin** at `loc_001A18D0` (`recomp_0005.c`), pending
>   word `MEM32(0x803C0810) = 3`.
>
> **Next packet:** Planner revises **`A4b1` → `A4b1-r4`** (`PREMISE_CHANGED`, §5.4(2)) — new baseline =
> toolkit **`M`**, exe
> `E45026C3DF5AACAF3D66FCC1E17D9C6C1A12247864D58D0911CDBA8435A2A3C7`, this run as the reference R0, and
> upstream's `src/apu/apu_dsp.c`/`CMakeLists.txt` as the starting state; then `A4b2`. **`A4b1`/`A4b2` are
> no longer parked behind `A4s`** — `A4s` now has a durable final disposition.
>
> **Closure push (owner policy) — after ACCEPT only.** `R-SAME` leaves `main` at `M`, so
> `git push -u origin main` is permitted once acceptance returns `ACCEPT` and the five pre-push checks
> hold. **The acceptance reviewer must be shown the Q-C claim limit explicitly**; acceptance with that
> recorded limit satisfies the policy's "active packet's tests/acceptance passed" — not all 56 KX modules
> passing.
>
> **CLAIM LIMIT (Q-C) — must appear in the acceptance review.** *KX does not exercise 7 pytest-dependent
> upstream modules under `unittest`* (`test_block_dispatch`, `test_incdec_carry`, `test_incdec_result`,
> `test_lifter_double_shift`, `test_lifter_result_clobber`, `test_sar_width`, `test_x87_classification`).
> *The lifter/translator conflict resolutions (hunks 5–7 and 9) are therefore witnessed only by K4 and the
> existing KX modules, not by upstream's own tests of those paths.* Harmless **for this packet only**
> because `AC-GEN` holds — no regeneration, so `M`'s lifter produced none of the linked code. The 7 are
> **not a FAIL**: per Q-C a module whose only error is an absent third-party import ran no test, and all
> four fail-closed conditions were verified. **No `pytest` was installed and none was borrowed.**
>
> **GATED LEAD (binding on later planning).** Before **any** packet regenerates or relifts with the
> toolkit at `M` or later, all **56+** KX modules — including these 7 — must run under **real `pytest`**
> in an **owner-authorized** environment; parametrized and fixture tests included.
>
> **`AC-STRUCT` claim limit (Q-C).** It **detects duplicates, not omissions** — the switch-5 damage was
> invisible to it, and is excluded only by the explicit post-condition (one `case 138` per switch). For
> any future revision, **each HA edit should carry an expected-count post-condition enforced as an
> `AC-MERGE` check**.

- **Packet:** `docs/packets/a4s-toolkit-sync.md`, revision **`A4s-r6`**, class **change**, frozen
  SHA-256 **`75207C41B8E9964D3E1467F5E980D1B8DE4CB75A1E0AFE954F59D40FA27D86E3`** (**374 lines**).
  **This is the packet that was executed.** It replaced `A4s-r5` at the canonical path **byte-identically
  to the reviewed revision**; `A4s-r5` remains recoverable from git
  (`git show HEAD:docs/packets/a4s-toolkit-sync.md`, blob `f918b998c20a5be2bcc832fbde527a5754634183`).
- **Status: EXECUTED (see the result block above).** Promotion was **byte-identical with no revision**,
  exactly as §5.3 requires: `ADEQUATE` ends plan iteration, so the packet was **not** edited — the stale
  "358 lines" note in the packet and its tool record (the scanner is really 355 lines) is a **recorded
  advisory only** and is used in **no** predicate. Editing it would have changed the SHA and invalidated
  the review.
- **Adequacy:** **`VERDICT: ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED` — recorded
  **verbatim** with the reviewer's child ID and route in `docs/reviews/a4s-r6-adequacy-review.md`
  (fresh Planner child `95429607-3d77-44a9-8f38-47e978ece759`, `workbuddy-ai/kimi-k3`, effort omitted).
  It independently re-verified both pinned tool hashes and found the hunk-1/2 C text **byte-identical**
  to the Advisor's ruling, the 9-hunk table complete, and H2 to be the Advisor's **edit-application**
  version rather than the rejected precondition.
- **The review's one persistence risk is CLOSED** (`DEFERRED 1`). The reviewer asked the Session to
  confirm `logs/a4s/conflicted-*` survive until the `AC-STRUCT` positive control runs. Rather than merely
  confirming existence — they are in **gitignored** `logs/`, so nothing guarantees them — the Session
  tested whether the risk is real, and **it is not**:
  - the fidelity source is **durable in git**: the raw preview **`75083476` is a git tree object**,
    recoverable with `git show 75083476:<path>`;
  - the saved files are **not** byte-identical to the preview blobs, and the reason matters: the saved
    files are **diff3** (3 `|||||||` base markers) while `git merge-tree --write-tree` emits **2-way**
    markers (0 base markers) — the same 2-way/diff3 distinction that caused an earlier Session error;
  - **it does not change the control**: resolving **both** sources to all-ours gives **byte-identical
    content for all five conflicted files**, and both give **exactly 3 findings, 0 UNKNOWN** over `SCOPE`
    (`dup-case case 138 [8019,8322]`, `dup-case case 138 [8481,8847]`, `dup-def bridge_KeResetEvent
    [1377,6713]`) — precisely the 2 + 1 the criterion expects.
  - **A Session error found and corrected while closing it:** the first attempt resolved **all five**
    conflicted files, including three under `tools/recomp/` that are **outside `SCOPE`** (toolkit tooling,
    not build inputs of `jsrf_recomp.exe`). Feeding Python to a C-oriented scanner produced **79 spurious
    `dup-def` findings and one UNKNOWN** in `lifter.py`. The corrected control scans only the two in-scope
    files. Recorded because it is exactly the mistake the `SCOPE` guard exists to prevent.
- **Readiness gates the Session has already verified:** both pinned gating tools match their pins —
  `scripts/check-merge-structure.py` = `A1FDCE26755DAB3A0B6AA6E23D380A8F95E097439C2BE91D6318BF4402AA1AFF`
  and `tests/test_merge_structure.py` = `A42F4A1E2C473892DC28A0F5B21923F4BC0AEFCFFE859C7FC20006F49DB0744C`
  — and the scanner's four controls reproduce (`0d7929c` 87/109/**0**, `766ecef` 87/111/**0**,
  `75083476` raw 90/111/**15**, worktree 87/109/**0**; fixtures **14/14 OK**).
- **Set-G baseline at this revision: 11 files, 10 pass / 1 fail** (the one failure is `E2 =
  `tests/test_ac2_provenance.py`, pre-existing and pristine-HEAD-reproduced). It has moved **three times
  this session** (8/2 → 9/1 → 10/1), which is why the packet requires the carve-out to be **re-derived
  from each execution's own step-2 run by test identity, never carried forward**.
- **Next action: Planner revises `A4b1` → `A4b1-r4`** (`PREMISE_CHANGED`, §5.4(2)), then `A4b2`.
  `A4s` now has a durable final disposition, so **`A4b1`/`A4b2` are no longer parked behind it**.

### `A4s-r6` execution — **COMPLETE**, steps 1–8 all run

The full narrative is in `docs/reviews/a4s-r6-execution-evidence.md`; the summary is in the result block
at the top of this `CURRENT PACKET` section. Points worth keeping here:

- **P0 passed under the Advisor's interpretation rulings** — **Q-A** (a pin on a tracked file identifies
  the **committed blob**; P0.10 passed by **branch (a)**, the literal working-tree hash, which equals the
  pin) and **Q-B** (proceed, discharging P0.7/step 1 as **verify-and-reuse** of the pre-existing
  `a4s-pre-sync` under four fail-closed conditions, all of which held). **Neither required a packet
  revision** (§5.4 interpretation rulings), and **no `.gitattributes` was added** (owner/packet-scope
  work, per the Advisor).
- **`AC-STRUCT` earned its place.** It caught a real defect in the **Session's own** first resolution —
  a blanket text delete of `case 138` that left switch 4 with a duplicate (compile error) and **switch 5
  with none at all**, silently dropping ordinal-138 dispatch. **The build would never have caught the
  second one.** Direct evidence for the Advisor's ruling that `AC-STRUCT` must run **before** the build.
- **The `AC-MERGE` `(d-twin)` deletion witness took four attempts and all three failures were mine** —
  the word *"credited"* is load-bearing. Recorded in full, including the two false alarms caused by
  over-broad file-wide text searches (`docs/reviews/a4s-r6-ac-merge.md`,
  `docs/reviews/a4s-r6-appendix-a-conformance.md`).
- **A recurring Session failure mode, recorded because it repeated:** every verification failure in this
  execution was an **over-broad text match** — a file-wide search used where the criterion was
  function-scoped, or a substring test matching a longer identifier. **No defect in `M` was ever found
  by these.** The lesson is that each criterion's scope must be implemented literally, and the targeted
  per-region checks are what make the results meaningful.
- **The KX/`pytest` question was resolved by the Advisor as `Q-C`: NOT `R-TEST`.** The 7 modules are
  *"not exercised: new at M, dependency absent (pytest)"* — **not a FAIL and not a PASS witness** — under
  four fail-closed conditions, **all verified**. **No `pytest` was installed, and none was borrowed**
  from the unrelated venv the Advisor identified. The claim limit and the gated lead are in the result
  block above and **must be shown to the acceptance reviewer**.

**Current blocker / next action.** **`A4s` is COMPLETE** — `A4s-r6` is executed, **accepted** (`ACCEPT`,
all ten criteria `AGREED`), and **pushed** to the owner's fork; the toolkit has a durable final
disposition at `M`. **The current work is the Planner's revision of `A4b1` → `A4b1-r4`**
(`PREMISE_CHANGED`, §5.4(2)), then `A4b2`.

**`A4b1-r4` — ADEQUATE, PROMOTED, EXECUTING (2026-09-25).** The Advisor ruled **`SHAPE: PROCEED`** on
the Planner's sketch; the packet body was written; a **full formal §5.3 review** by a fresh Planner
returned **`ADEQUATE`** (`BLOCKING: NONE`, `PREMISE_FRESHNESS: PASS`); the packet was **promoted
byte-identically** and **execution is in progress**. (Both the packet and its verdict were authored by
Kimi K3 Planners, which was the roster in force at the time — recorded as **provenance**; see the
staffing-state block below, which supersedes that arrangement for *future* Planner work.)

- **Packet:** `docs/packets/a4b1-gp-core-port-r4.md`, revision **`A4b1-r4`**, class **change**, SHA-256
  **`6DD62A57E87445F5C12085210146204AA4E77D26FD316071FD41CAEC76835C38`**, **445 lines**. The canonical
  `docs/packets/a4b1-gp-core-port.md` (`A4b1-r3`, `321ABCF7…`) is **untouched** — promotion is the
  Session's step.
- **Advisor rulings (binding, verbatim):** `docs/reviews/a4b1-r4-planning-rulings.md`. Four parts:
  **shape `PROCEED`**; **`[GPIN]` = the finite-universe/provenance-class design in full** (part 2);
  **identifier = `A4b1-r4`** (part 3); and **part 4 — P-F is load-bearing, not a wording fix.**
- **The packet is LARGER than a pure re-baseline, and the class statement says so:** it is *both* a
  **§5.4 After-INADEQUATE repair** of `A4b1-r3`'s blocking defects B1/B2 *and* a **§5.4(2)
  `PREMISE_CHANGED`** re-bind to `M`. **No `A4b1` revision has ever been `ADEQUATE`; `A4b1-r3` is
  superseded.** Hence a **full** adequacy review, not a delta re-review.
- **What the packet carries:** DS3 rewritten for the **non-injective** forward map (four-case inverse —
  window-VA first, then high-water `XBOX_CONTIG_BASE + P`, then mapped low-RAM identity, then fail closed
  — citing `dma_resolve` **and** `xbox_memory_layout.c:843`, with `GPDMA_AMBIGUOUS` and the aliasing claim
  limit); DS6 with the **full** finite-universe `[GPIN]` design (MIXBUF `[NUM_MIXBINS = 32]`, PERIPH
  `[128]`, FIFO `[6]`, DMA region classes `[4]`, `BOOT_SCRATCH_READ`, `GPIN_OUT_OF_UNIVERSE` as a bug
  detector, `at_clear` freeze at `GP_CLEAR`, capped observation, fixed-count emission) with the r3
  table/`GPIN_OVERFLOW`/per-key lines/cut-off **retired and named as must-not-reappear**; DS5's CPU-site
  table sized one-per-enumerated-`jsrf_watch_store`-site with `CPU_ZERO_OVERFLOW` a bug detector; DS7's
  `[GPBOOT]` printing **both** `sge0` (raw) and `sge0_va` (translated); and AC-FIX `(viii)` kept with
  `(ix′)/(x)/(xi)` replacing `(ix)`, `(vii)` in **two** forms that can fail in both directions.
- **`A4b2` boundary note is in the packet** (`A4b2-r3`'s P2 and `AC-INPUTS` become stale; `A4b2-r4`
  re-brief items recorded). **`A4b2` is not revised here.**
- **Regeneration is a non-goal**, so the pytest gated lead **stays gated and unexercised** — no `pytest`
  work is manufactured.
- **Session verifications supporting the packet:** the Advisor's one **inferred** claim is now
  **OBSERVED** (slot `0x001C40E8` holds `0x800000AD` = ordinal 173 `MmGetPhysicalAddress`, with exactly
  **10** calls through it and exactly **6** returning into the DSOUND range
  `0x1A4E42–0x1A70C6`); and the DS3 citations are verified with the **ordering nuance** that DS3 must test
  window-VA **before** the high-water mark (`docs/reviews/a4b1-r4-ds3-citations-verified.md`).

**The premise reconnaissance (below) remains the measured basis for the re-bind.**

- **Frozen brief:** `docs/reviews/a4b1-r4-planning-brief.md` (the task, the verified baseline, the
  ordered reading list, the measured premise delta, the four premise answers, the pytest lead, and the
  mandatory sketch → Opus shape-preflight workflow).
- **Premise re-check:** `docs/reviews/a4b1-r4-premise-recheck.md`. **A4s changed exactly two of the files
  `A4b1` touches** — `src/apu/apu_dsp.c` (+56/−3) and `src/apu/CMakeLists.txt` (+7/−1);
  `apu_core.c`, `apu_state.h`, `apu.h` and `apu_mmio_hook.c` are **byte-identical**. So the APU surface
  is almost untouched, which is why this is a premise re-check and not a redesign.
  - **P-F is CORRECTED by the Advisor (part 4):** the Session's original reading — *"premise
    strengthened, a wording correction"* — **understated the change.** The forward map went from
    **identity** to **non-injective**, so low-RAM VA `X` and window VA `0x80000000+X` both map to physical
    `X`; the Session's proposed inverse formula was **wrong** and is superseded by the Advisor's four-case
    rule. The record carries a correction banner.
  - The other premises **hold or are unchanged**, including the strict stop.
  - **The inverse the packet needs is documented in-tree** (`docs/reviews/a4b1-r4-inverse-precision.md`,
    now superseded in its *formula* by the Advisor's part 4): `XBOX_CONTIG_BASE = 0x80000000` **is** the
    contiguous window, and `xbox_memory_layout.c:843-845` states the round trip — *"The contiguous window
    IS the physical-address view, so OR-ing its base is the documented round trip, not a guess."*
    **The Advisor's four-case inverse replaces the Session's `XBOX_CONTIG_BASE | (P & 0x0FFFFFFF)` form,
    which is wrong for any identity-passed low-RAM address.**
  - **The `& 0x03FFFFFF` hazard is real and pre-existing:** 8 live sites, including `apu_shim.h:101-123`
    and `apu_vp.c:846` — both **byte-identical** across the baseline move. Note the masks differ
    (`0x03FFFFFF` = 26 bits vs the round trip's `0x0FFFFFFF` = 28 bits), which is exactly the distinction
    Device semantics 3 exists to prevent.
  - **The `MIXDOWN_ALL` question is RESOLVED by source read** — it was the plan's flagged *uncertain and
    load-bearing* item. `monitor.frame_buf` is `int16_t[256][2]` at `apu_state.h:500` inside the **host**
    `MCPXAPUState` struct, its instances are host allocations, and there are **zero** guest-mapping
    references to it. So the default-on path writes only host monitor storage: **not new device
    behaviour, no new criterion.** It remains a `jsrf-run-profiles.md` classification item.
- **Everything else holds:** P-B, P-C, P-G (byte-identical), P-E, P-D's gate, and the strict stop. All
  `A4a` R0/R1 observations, the watch-ledger ruling, the Q1/Q2 rulings, the checkpoint-40 constraints
  and the `[GPIN]` redesign remain **admissible and un-reopened** — a baseline change alone does not
  reopen a technical ruling.

The new baseline for that revision: toolkit **`M`** = `3f8bf67c450861aefcbc376698750bc1446bc9dd`; exe
`E45026C3DF5AACAF3D66FCC1E17D9C6C1A12247864D58D0911CDBA8435A2A3C7`; the run
`logs/runs/20260925-210315-113-a4s-sync-strict` as the reference R0.

**Identifier correction (Advisor ruling, part 3; 2026-09-25).** This revision is **`A4b1-r4`**, not
`A4b1-r2`. The Session named it `A4b1-r2` in error and that label propagated into the frozen `A4s-r6`
packet's `R-SAME` cell. `A4b1-r2` is already taken: `r1` (commit `8735165`), `r2` (`127d203`, itself
adequacy-reviewed at `839E9BEC…D2BB7`) and `r3` (`fea49f1`) all exist, and an identifier that names a
reviewed document must never be reused. The frozen `A4s` packet is a closed record and owes nothing
(§5.4: record pointers never reopen a frozen packet); the label is corrected here. Full ruling:
`docs/reviews/a4b1-r4-planning-rulings.md`; the Session's error record:
`docs/reviews/a4b1-revision-identifier-collision.md`.

**Two claim limits and one gated lead carry forward into that planning:**

1. **KX does not exercise 7 pytest-dependent upstream modules under `unittest`** — `test_block_dispatch`,
   `test_incdec_carry`, `test_incdec_result`, `test_lifter_double_shift`, `test_lifter_result_clobber`,
   `test_sar_width`, `test_x87_classification`. *The lifter/translator conflict resolutions (hunks 5–7 and
   9) are therefore witnessed only by K4 and the existing KX modules, not by upstream's own tests of those
   paths.* Harmless for `A4s-r6` only because `AC-GEN` held (no regeneration). **`A4b1-r4` does not
   regenerate either** (regeneration is already one of its non-goals), so this lead stays **gated and
   unexercised** — no `pytest` work is manufactured to clear it early.
2. **`AC-STRUCT` detects duplicates, not omissions** — it did not see that one switch had lost `case 138`
   entirely; that class is excluded only by an explicit expected-count post-condition. **For any future
   revision, each HA edit should carry such a post-condition, enforced as an `AC-MERGE` check.**
3. **GATED LEAD (binding):** before **any** packet regenerates or relifts with the toolkit at `M` or
   later, all **56+** KX modules — including these 7 — must run under **real `pytest`** in an
   **owner-authorized** environment; parametrized and fixture tests included. **Do not install, borrow, or
   otherwise introduce `pytest` without owner authorization.**

### Predecessor — `A4s-r5` — **EXECUTED 2026-09-25, selected `R-CONFLICT`** (superseded by `A4s-r6`)

- **Packet (superseded):** `docs/packets/a4s-toolkit-sync.md`, revision **`A4s-r5`**, class **change**,
  frozen SHA-256 **`09DA9413C028D61BD28D9E4007AF6DDE3474ED6869F77B0095D03DB2C4FB86FB`** (263 lines).
  Recoverable from git; no longer the file at that path.
- **Status: executed. Selected row `R-CONFLICT`** (fail-closed, as predicted). **Toolkit `main` is left
  at `0d7929c`** (rolled back; working tree clean). **No push was performed at execution** — `main` at
  `0d7929c` is not a descendant of `origin/main`, so per the Closure rule: **"no push: main at
  `0d7929c`"**. (The accepted **baseline** was later published to the fork's `jsrf/integration` branch
  under the owner's standing push policy — see the push log above; `main` itself is still unpushed.)
- **Execution evidence:** `docs/reviews/a4s-execution-evidence.md`. **Conflict inventory:**
  `docs/reviews/a4s-r5-conflict-inventory.md` + raw hunk text in `logs/a4s/conflict-hunk-inventory.txt`.
- **Result:** the merge attempt produced **5 conflicted files, all inside the packet's 10-file set**
  (`kernel_bridge.c`, `xbox_memory_layout.c`, `lifter.py`, `test_icall_feedback.py`, `translator.py`)
  and **9 conflict hunks: 1 × HA, 1 × H1, 2 × H2, 5 × UNDECIDED**. Five UNDECIDED hunks select
  `R-CONFLICT`; the packet does not authorize choosing a side by judgment for them. `AC-MERGE`,
  `AC-KEEP`, `AC-INV`, `AC-BUILD`, `AC-TEST` and the strict run were **not reached** (row 2 precedes
  them); their read-only pre-checks are recorded as non-binding supporting evidence.
- **Pre-merge controls (step 2, all on `0d7929c`):** build OK; pre-merge exe
  `9597FF7C2A377265ABA8DBB90B461EBE763E02D65432E9DFA13ACD925539C553` (matches the recorded
  expectation); `ctest` **12/12**; set K4 **27 tests OK**; set KX **33 modules, Ran sum = 133**;
  set G **8 pass / 2 fail**, both failures **measured pre-existing** (see the `E1`/`E2` follow-ups
  below — `E1` now passes, so the current set-G baseline is **9 pass / 1 fail**).
- **Rollback rebuild exe SHA-256:** `AEC1F0FF7FB944DA44487EA15C3E94FF342751D4168E699F544A58895CF083C3`
  — **differs** from step 2; the packet says record, not gate, so no row turns on it. Cause **measured**:
  `/Zi` + `/DEBUG:FULL` with no `/Brepro`, so a **relink** stamps fresh PE timestamps. Step 2's build
  found the tree already up to date and **did not relink** (which is why it matched the archived
  `9597FF7C…`); the merge attempt and `merge --abort` then rewrote **58 toolkit files** (mtimes only —
  `git status` clean, `git diff HEAD` empty), forcing the rollback build to relink. A **no-op build was
  measured not to relink**. The two exes are identical in size, layout and all but **13 bytes** (four
  PE timestamp fields plus one stamp byte).
  Analysis: `docs/reviews/a4s-r5-rollback-exe-hash.md`. *(An earlier note here called the rebuild
  "deterministic"; that inference was wrong and is retracted — see that record.)*
- **Adequacy:** **`ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED`, `DEFERRED: NONE`
  — `docs/reviews/a4s-r5-adequacy-review.md`, fresh Planner child `f339b306-0473-4777-a211-78271453f2b9`.
- **Baseline recorded at execution:** game **`c1cdb91`** (the commit that adds the promotion;
  the plan previously named `b7d6af5`, which is the preceding startup-receipt commit — advisory,
  not revised), toolkit `main` **`0d7929c`**, `upstream/main` = `origin/main` = `v0.11.0^{commit}` =
  `766ecef`, merge base `051a128`, no `a4s-*` branch existed before step 1 (`a4s-pre-sync` now exists
  at `0d7929c` and still resolves to it).
- **Revision log:** `docs/reviews/a4s-revision-history.md` (non-authoritative).

**Push log — first durable-checkpoint push (owner-authorized, 2026-09-25).**

```text
PUSHED_TO:  https://github.com/danillogical/xboxrecomp.git   (remote `origin` = the owner's fork)
BRANCH:     jsrf/integration   (NEW branch; `main` cannot fast-forward yet)
COMMIT:     0d7929c86771dd0b971941592fd4f15436116e82
REMOTE_URL: https://github.com/danillogical/xboxrecomp.git
RESULT:     SUCCESS — verified independently with `git ls-remote origin`, which reports
            0d7929c…  refs/heads/jsrf/integration and 766ecef…  refs/heads/main (unchanged)
```

The 24 commits published are the **accepted toolkit baseline** (`A4a-r2`, `A3a-r25`, `A4p-r1` all
accepted on `0d7929c`; baseline tests re-measured this session: ctest 12/12, K4 27 OK, KX 133 OK). The
`A4s-r5` `R-CONFLICT` attempt produced **no commit**, so none of its output is in this push. **No force,
no `upstream` push, `main` untouched.** `main` stays at `766ecef` because it cannot be fast-forwarded
until the `A4s` merge lands `766ecef` in local `main`'s history; the owner's branch-handling rule covers
exactly this case. Full record and all five pre-push checks:
`docs/reviews/owner-push-policy-xboxrecomp-fork.md`.

**`A4s-r6` packet delivered; adequacy review completed and superseded by execution.** (This paragraph and
the ones that follow are the **planning-phase record**; the outcome is in the `CURRENT PACKET` result
block at the top of this file.) `docs/packets/a4s-r6-toolkit-sync.md`,
revision **`A4s-r6`**, SHA-256 **`75207C41B8E9964D3E1467F5E980D1B8DE4CB75A1E0AFE954F59D40FA27D86E3`**,
**374 lines** (the Planner's reported "325" was a non-empty-line miscount; the project convention is
total lines — `A4s-r5` is cited as 263, its LF count). The frozen `A4s-r5` is **untouched**
(`09DA9413…86FB`). A **fresh Kimi Planner** performed the binding §5.3 adequacy review of that exact
SHA, returning **`ADEQUATE`** (`BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED`) — recorded in
`docs/reviews/a4s-r6-adequacy-review.md`. Per §5.3, `ADEQUATE` requires only `BLOCKING = NONE` and
`PREMISE_FRESHNESS` not `FAIL`; on `ADEQUATE` the revision was frozen and promoted in the same step,
with no polishing pass.

**Current blocker / next action.** The Advisor's hunk rulings are **recorded and binding**
(`docs/reviews/a4s-r6-advisor-hunk-ruling.md`). The frozen `A4s-r6` planning brief was dispatched to the
Planner (`workbuddy-ai/kimi-k3`, effort omitted per the owner's instruction). The Planner wrote its
**sketch** (`docs/packets/a4s-r6-toolkit-sync.md`, 14 lines, ≤20 tool calls) and the Session relayed it
for the **mandatory shape preflight**. The Advisor returned **`SHAPE: PROCEED`** with three bounded
policy items to write into the packet:

1. **Gating tools must be tracked and pinned.** `AC-STRUCT`'s scanner, and any verifier the packet
   relies on, must live at a **tracked** path (e.g. the game repo's `scripts/`) pinned by SHA-256 —
   **not** in `logs/a4s/`, which is gitignored and which `AGENTS.md` forbids for durable helper source.
   Its controls must be packet criteria (positive on the merge preview; zero on both parents; UNKNOWN
   for an unparseable merge-changed file).
2. **The pre-ruled table does not exempt run-profiles rules 1–5.** Keep `AC-INV` and the `SCOPE` guard
   exactly as they are; and if the real merge produces a hunk outside the recorded 9, **or** a recorded
   hunk whose base/ours/theirs text differs from `logs/a4s/conflict-hunk-inventory.txt`, the result is
   **`R-CONFLICT`** — never resolved by analogy.
3. **"Expected `R-SAME`" is a forecast only** — no criterion may assume it. `R-PUSH` follows the owner
   push policy: all five pre-push checks, and **never** after `R-CONFLICT`, a rollback, or `INADEQUATE`.

The Advisor's stated **REVERSED_BY** conditions, which return the packet to sketch: the real merge's
conflict set differing from the 9 recorded hunks; the verifier showing edit-application H2 changes any
recorded classification other than hunk 5's; or the scanner failing its positive/negative control on the
pinned trees.

**Session support already delivered to the Planner** (`docs/reviews/a4s-r6-ac-struct-controls.md`):
measured control numbers for the scanner over `SCOPE` — `0d7929c` and `766ecef` both **0 findings**;
the raw preview **15** (12 marker lines + 3 structural); the resolved all-ours preview **3 structural,
0 markers**; resolved all-theirs **2** (resolving hunk 1 to upstream removes the duplicate
`bridge_KeResetEvent` that lives inside it). Two wording traps are recorded there: the criterion must
say **which tree** each number belongs to, and must name the **resolution** the control uses.

**The Advisor's rulings, in one line each** (full text and BASIS in the ruling record):

1. **Hunks 1–2 — COMBINED form**, fixed precedence: (1) in-place KEVENT if `guest_va_is_inplace_kevent`
   accepts, (2) `ke_shadow_lookup` handle if non-NULL, (3) `XBOX_TO_NATIVE` fallback. **Upstream's
   `bridge_resolve_handle` tier is NOT carried over** (for a nonzero VA it returns the token itself,
   which would pass a guest VA as a host HANDLE and make tier 3 unreachable). The ruling supplies the
   **exact C text** for all three functions; **exactly one** `bridge_KeResetEvent` survives, with the
   same three tiers. Two further edits are pre-ruled HA: delete upstream's duplicate clean-hunk
   `bridge_KeResetEvent`, and keep **one** `case 138` per switch (local's positions), located **by exact
   text, not line number**.
   - *Why not upstream alone:* the accepted, gating ctest `jsrf_inplace_event_bridge` asserts guest
     `SignalState` and previous-state values that only local's bodies write.
   - *Why not local alone:* upstream's timer model arrives in a **clean hunk** and registers timers only
     in the shadow table; ordinals 113 and 159 are both declared, so a wait on a timer would never see
     the event that fires it.
   - *Why in-place first:* `ke_shadow_remove` has **zero callers**, so shadow entries are never retired;
     and every reachable shadow populator writes type 8/9, which the sniffer rejects — so in-place-first
     never takes an object away from the shadow tier. It also defuses the `Size` hazard without deciding
     the `Size` question.
2. **Hunk 5 — per-line combined form** (derived by rule): `_FLAGS_UNDEFINED` loses `"lock xadd"`, keeps
   upstream's `"popfd"` and its comment; `_EFLAGS_SETTERS` keeps local's line; hunk 6's H2 result stands.
3. **Hunk 8 — LOCAL** (`"--functions", fns,`), pinned.
4. **Hunk 9 — full union**, local's members then upstream's, upstream's `_RESULT_SNAPSHOT_SETTERS` clause
   kept.
5. **H1 — accepted as proposed**: containment must include deletions.
   **H2 — the Advisor REJECTED the Session's added delete-vs-keep precondition** and kept the original
   one; H2's **action** must be defined as **EDIT APPLICATION** (apply each side's per-line edit to the
   base, then insert each side's insertions, local first), **not** a union of line presence. Ambiguous
   attribution → UNDECIDED. The Session **verified the Advisor's prediction exactly**: hunk 5 changes
   from UNDECIDED to H2 applies, every other hunk keeps its classification, and edit application
   produces the ruled `_FLAGS_UNDEFINED` text.
6. **`AC-MERGE`(d) gains a twin witness:** every base line a credited side deleted is absent from the
   result unless the other side replaced it.
7. **Post-resolution structural check required** (conflict markers; duplicate `case` values per switch
   per branch path; duplicate file-scope definitions per branch path), on the **resolved** tree
   **before** the build, over `SCOPE`, with positive and negative controls, selecting **`R-CONFLICT`,
   never `R-BUILD`**, plus a C2084/C2196/C2371 backstop. The Advisor ruled **against** a broader semantic
   review as unbounded. **H3 is not audited** (no corpus) — the packet must say so.

**Follow-up leads the Advisor recorded (NOT `A4s-r6` obligations):** the correct `Size` convention
(4 vs 16/40 — needs a primary source); tier 3's handle-typed treatment of a non-handle Ke* object
(pre-existing); `ke_shadow_remove` having no callers; the upstream timer model's observable behaviour
for JSRF; an indirect-thunk-call search behind "not declared ⇒ unreachable"; case 207's argument count
(local 36 vs upstream 40 — the merge silently took upstream's 40). Prior leads carry forward:
`MIXDOWN_ALL`/`USB_PORT` classification, E2, the function-style KX modules, and the exe-relink advisory.

**Staffing state — PLANNER CHANGED TO CLAUDE OPUS 5.5 @ `medium` (owner §3.4 decision, 2026-09-25).**
The owner **replaced `docs/agent-workflow.md`** and directed a staffing change. The file was **re-read
from disk**: its SHA-256 is now **`973CDDEF0C5206349E66DE15A6C0E830F85ACAD488231D3867F25B583E911748`**
(previously `CBEF9041…`), so it genuinely changed. Full record:
`docs/reviews/owner-staffing-update-20260925-planner-opus.md`.

```text
WORKFLOW_RELOADED: YES
WORKFLOW_SHA256:   973CDDEF0C5206349E66DE15A6C0E830F85ACAD488231D3867F25B583E911748
PLANNER_ROUTE:     claude/claude-opus-5-5
PLANNER_EFFORT:    medium
ADVISOR_CHILD:     5c555969-dea9-4b47-be05-62aa0835cde2
ADVISOR_ROUTE:     claude/claude-opus-5-5 @ high
```

**SECOND RELOAD, same day — implementation-worker progress gate (2026-09-26).** The owner replaced
`docs/agent-workflow.md` **again**; re-read from disk, its SHA-256 is now
**`F4F5D029D45A96CE5E4CA61ED106C0EFDF7CFD3FC99EB5F331E4D22D12F04A0C`** (was `973CDDEF…`). Full record:
`docs/reviews/owner-workflow-update-20260926-worker-gate.md`.

```text
WORKFLOW_RELOADED: YES
WORKFLOW_SHA256:   F4F5D029D45A96CE5E4CA61ED106C0EFDF7CFD3FC99EB5F331E4D22D12F04A0C
PLANNER_ROUTE:     claude/claude-opus-5-5 @ medium
ADVISOR_CHILD:     5c555969-dea9-4b47-be05-62aa0835cde2
ACTIVE_WORKER_PROGRESS_GATE_APPLIED: YES
```

- **Staffing is unchanged** — the Session read the new `§1` roster rather than assuming; all six DSH
  rows are identical to the previous reload, so **no route re-resolution was needed** and the existing
  Planner resolution (`claude/claude-opus-5-5`, `medium`) and Advisor child stand.
- **The only substantive addition** is the `§2.2` **implementation-worker anti-loop progress gate**: by
  **15 tool calls** a bounded implementation worker must have produced a concrete execution artifact
  (edit, compile/build attempt, test run, generated fixture, or bounded blocker report); after the first
  attempt further reads must be tied to a **specific observed** compiler/linker/test/runtime failure;
  **two consecutive 10-call stretches** with no new artifact/measurement/narrowed blocker mean the worker
  stops; re-reading the same files or re-litigating settled alternatives counts as **no progress**; and
  a blocker returns the fixed `STATUS: BLOCKED` form. **A no-progress or blocker return is an escalation
  signal — the Session does not auto-spawn an identical replacement**, but routes it through the
  escalation ladder first.
- **Applied immediately to the active worker** (`1fe1b4f2-…`, rewriting the `AC-FIX` fixture for the
  `FIFO_READ` ruling (C)). It had **already exceeded the new first-artifact budget**, so per the owner it
  was **not** given a fresh 15-call window; the owner's bounded correction
  (*"STOP RESEARCH CHURN AND EXECUTE OR REPORT A BLOCKER"*, next 3 tool calls: produce the smallest safe
  artifact and compile/test it, **or** return the bounded `BLOCKED` report) was sent, with the production
  changes listed factually so no re-derivation was needed. **The correction was recorded as issued under
  the new workflow.**
- **Packet, baseline, Advisor child and every durable ruling are preserved**; scope is unchanged. The
  workflow change alone reopens nothing.

- **The Planner route resolves live and exactly** (§1 `LIVE_RESOLVE`): `list_subagent_models` returns
  **one** canonical match, `claude/claude-opus-5-5`, and it advertises **`medium`** among its efforts
  (`low, medium, high, xhigh, max`). **No fallback was used.**
- **The §1 roster changed in three rows, not one** — found by diffing, not assumed. The **Planner** moved
  to Opus 5.5 @ `medium`; and the **acceptance reviewers swapped** to stage 1 `workbuddy-ai/hy4-preview-f`
  @ `high` and stage 2 `workbuddy-ai/deepseek-v4.1-flash` @ `max`, which is exactly the pairing the
  session had already been using, so the revision **aligns the document with practice**.
- **No in-flight Planner work existed to abandon.** Every Kimi Planner child had **finished** and its
  output is durable: `3fbfef10` (`A4s-r6`), `7332c25d` (the `A4b1-r4` packet), `95429607` and `5db5fd71`
  (the two `ADEQUATE` verdicts). The only running child, `f9d0e439`, is a **Worker** — a contract role
  whose route (`workbuddy-ai/deepseek-v4.1-flash` @ `max`) is **unchanged** — so it continues.
- **Durable work is preserved with its original model identity as provenance.** `A4b1-r4` remains the
  promoted, authorized packet and **execution continues**; the owner explicitly forbids retroactive
  invalidation of work completed before the change.
- **Planner/Advisor separation (same model family, different roles):** the **Opus 5.5 Medium** child is
  **Planner authority only**; the existing **Opus 5.5 High** persistent child
  (`5c555969-…`) is **Advisor authority only**; **neither is reused for the other role in the same
  decision**, and the Advisor child is **kept, not replaced or reprobed**. The **mandatory Advisor shape
  preflight remains in force** for new change packets/material redesigns, and it is **not** the formal
  adequacy review. For a change packet materially authored by an **Opus 5.5 Medium** Planner, the
  **binding adequacy review goes to a fresh Opus 5.5 Medium Planner child** — not the authoring child and
  not the Opus High Advisor.
- **Going forward, no new Planner work goes to Kimi K3.** The next Planner dispatch — the `A4b2-r4`
  revision the `A4b1-r4` boundary note calls for — goes to a **fresh `claude/claude-opus-5-5` @ `medium`**
  child. The Session route is unchanged (`workbuddy-ai/deepseek-v4.1-flash` @ `max`).
- **Superseded (provenance only):** the earlier Planner arrangement in which `docs/agent-workflow.md` §1
  named **Kimi K3** with a `max` qualifier, and the owner's then-instruction *"kimi k3 doesn't take an
  effort, so don't worry about effort for kimi k3"*. That arrangement is **no longer in force**; it is
  retained here only because the packets and verdicts authored under it are durable and cite it.
  `docs/agent-workflow.md` §1 was **not** edited by the Session (§3.4).

**Execution rulings (binding, recorded).** `docs/reviews/a4s-r5-execution-startup-ruling.md`:
**Q1** Planner effort (superseded by the owner instruction above); **Q2** P0.8 recorded as a literal
**FAIL** and execution proceeded under a **process exception** (the only dirty path is the owner's
`docs/agent-workflow.md`, a non-build, non-evidence file outside every path-scoped check) — the file was
**not** committed or normalized; **Q3** the `AC-INV` "new environment names" command cannot run as
written on PowerShell 5.1 (embedded `"` is stripped), so an **interpretation ruling** authorized one
substitute form with three controls (42 / 44 / empty-and-exit-1), all of which passed;
**Q4** two pre-existing set-G failures (`E1` `test_agent_docs.py`, `E2` `test_ac2_provenance.py`) are
**named exceptions** — measured byte-identically on a pristine HEAD extraction, hence not
merge-relevant — and the KX carve-out was **not** extended to set G wholesale. `AC-TEST` was not
reached, so Q4 is prospective.

**Follow-ups (recorded, not authorized work):**

- **E1 — RESOLVED as a side effect of this closure; recorded as `PREMISE_CHANGED`.** Four
  current-tense mentions of a since-retired route name tripped `scripts/check-agent-docs.py`'s
  `RETIRED_NAMES` check. They sat in the superseded staffing block that this closure rewrite replaced,
  so they are gone and `tests/test_agent_docs.py` now **passes** (`Ran 30 tests, OK`); the checker
  reports **0 findings**. No test, script, packet or frozen artifact was edited to achieve this.
  **Any re-run of `A4s-r5` must re-measure the set-G baseline** — it is now **9 pass / 1 fail**, not the
  8/2 measured at step 2. Full record: `docs/reviews/a4s-execution-evidence.md`, "Premise change to
  `Q4`'s E1 exception".
- **E2** — `tests/test_ac2_provenance.py` fails on `A0` and `P1` with clause-D reason "the classifier
  was NOT edited; step 3 requires line 391 to be corrected". Cause unconfirmed; diagnose before anyone
  relies on the AC2 provenance tool.
- **KX claim limit** — ≥16 function-style toolkit test modules are not exercised by `unittest`, before
  or after the merge; run them under their own runner (they have `__main__` blocks) or convert them.
- **CRLF working-tree drift** — the merge/abort rewrote 4 toolkit files to CRLF (`core.autocrlf = true`).
  `git status` reports no change and the CRLF→LF bytes hash exactly to the archived `A4a-r2` reference;
  recorded so a future raw-hash comparison is not misread as a regression.

**ROUTE STATE — RESOLVED (2026-09-25, `session-58e86358`); the senior-judgment routes are live.**
The previous state (both senior routes BLOCKED; the substitute unresolvable) is **superseded**, as is
the temporary owner-authorized staffing exception recorded in
`docs/reviews/startup-20260925-session-910703eb.md` — the owner replaced `docs/agent-workflow.md`, and
its §1 roster is now the only persisted staffing policy. This session resolved every §1 DSH row live:
**Claude Opus 5.5 @ `high`** (`claude/claude-opus-5-5`) spawned and passed the two-turn continuity and
read-yourself probe; **Kimi K3** (`workbuddy-ai/kimi-k3`) resolved and answered (effort omitted per the
owner instruction); first-stage reviewer `workbuddy-ai/hy4-preview-f` @ `high` and second-stage
`workbuddy-ai/deepseek-v4.1-flash` @ `max` both answered their probes. **No route was substituted.**
Receipt: `docs/reviews/startup-20260925-session-58e86358.md`.

> **SUPERSEDED FOR FUTURE PLANNER WORK (owner §3.4 decision, 2026-09-25, later the same day).** The
> owner **replaced `docs/agent-workflow.md`** again and moved the **Planner** to
> **`claude/claude-opus-5-5` @ `medium`**. The paragraph above remains an accurate **receipt of the
> startup probes at the time it was written** — including the Kimi K3 Planner resolution — and is
> **retained as provenance**, because the packets and verdicts authored under that roster are durable.
> **For all future Planner dispatches the staffing-state block above governs: Opus 5.5 Medium, no Kimi.**
> The Advisor row (`claude/claude-opus-5-5` @ `high`) is **unchanged**, and its existing child
> `5c555969-…` is **kept, not reprobed**.

**Preserved binding rulings.** The `A4s` AC'97 hunk rulings (`docs/reviews/a4s-ac97-hunk-ruling.md`,
including "Interpretation ruling 2: scope") and the `A4b` rulings remain **binding** and were
**preserved, not reopened** — the packet *cites* them rather than restating them, and the model/provider
change does not reopen a ruling.

**Superseded route state (historical).** The earlier `ROUTE STATE` and temporary-staffing blocks
recorded in this plan and in `docs/reviews/startup-20260925-session-910703eb.md` are **superseded**.
They described a since-replaced staffing arrangement under the previous workflow revision, including a
now-retired route that ran out of quota during the `A4s-r5` review. The owner has since replaced
`docs/agent-workflow.md`, whose §1 roster is the only persisted staffing policy. Prior route-failure
diagnoses are retained as provenance only:
`docs/reviews/route-failure-20260924-workbuddy-substitute.md`,
`docs/reviews/route-failure-20260924-claude-pool.md`. **`docs/agent-workflow.md` §1 was not edited**
by this session — §1 is an owner-reserved decision (§3.4).

**Planner research-sufficiency stopping rule (unchanged):** the Planner stops investigating as soon as
§5.1's planning test is met and leaves remaining unknowns to the packet.

**Queued in order:** (1) **DONE** — the `A4s-r5` re-review returned `ADEQUATE`, the revision was
promoted, and it has now been **executed** (row `R-CONFLICT`, above); (2) **`A4s-r6`** — the Planner
revision that resolves the five UNDECIDED hunks from the inventory above, with hunks 1, 2, 5, 8 and 9
going to the Advisor first and hunk 5's `H1` rule gap clarified; (3) the `A4b1-r4`/`A4b2-r4` revision
per the binding `[GPIN]` redesign. The existing Advisor rulings are **binding** and are not reopened by
a provider or staffing change.

**Revision history of this packet** (each round's blocker was in the same criterion, so the
Session ruled the last fix a **simplification** under §5.5, not a patch):

| Rev | Verdict | Blocker |
|---|---|---|
| `r1` | INADEQUATE | a false `R-MOVED` on a shifted `B` |
| `r2` | **ADEQUATE** | — (but the fork premise then changed) |
| `r3` | INADEQUATE | the new greps matched a **comment** and an **unbuilt scaffold** → guaranteed false FAIL |
| `r4` | INADEQUATE | `AC-KEEP` (iv)'s **end anchors were not unique** (`        }` ×37, `            }` ×20) → false FAIL on a preserved merge |
| `r5` | **ADEQUATE** | — promoted, then **executed** → `R-CONFLICT` (B1-r4 closed: unique-first-line anchor + fixed length 7/56/25 + byte compare; anchors 1/1/1) |

**The sync itself:** merge **`upstream/main`** (**v0.11.0**, `766ecef`) into local `main`,
rebuild **without regenerating `src/recomp/gen`**, run both repositories' tests, rerun **one
strict baseline**, and push nothing during execution. **If the strict stop moves, that is the
next brief.** Verified facts and the conflict surface: `docs/reviews/toolkit-sync-instruction.md`.

**The AC'97 hunk is pre-ruled** (`HA`): it resolves to the **LOCAL** `A3a-r25` model; upstream's
NABM trap enters **unarmed** with zero call sites. The Advisor's **"Interpretation ruling 2:
scope"** fixes the check's scope: `SCOPE` = the toolkit's **`src/` and `include/`**, deleted
names match only as **quoted string literals**, and comments/docs/tests/unbuilt scaffolds are
**inventoried, never failed**. See `docs/reviews/a4s-ac97-hunk-ruling.md`. In the executed attempt this
was the **only** pre-ruled hunk and it was classified `HA`; the five UNDECIDED hunks are the blocker.

**Push policy (owner) — superseded and broadened 2026-09-25.** The previous wording was *"push toolkit
`main` to `origin` when a packet closes"*. The owner has now made **durable-checkpoint pushes the
standing practice** for toolkit work: push after an accepted toolkit implementation, a completed
sync/merge packet, a materially useful commit later packets depend on, or a clean milestone boundary —
not only at the end of the project. Full text, the verified remote table, and the pre-push checklist:
**`docs/reviews/owner-push-policy-xboxrecomp-fork.md`**; operating summary in `AGENTS.md` "Toolkit
remotes".

**Remotes verified this session with `git remote -v` (names were not assumed):** the owner's fork is
**`origin`** → `https://github.com/danillogical/xboxrecomp.git` (fetch **and** push); `upstream` →
`https://github.com/sp00nznet/xboxrecomp.git` (fetch; push `DISABLED`). **The fork is already
configured, so no remote was added, renamed, or altered, and `upstream` was not touched.** The
instruction's suggested name `fork` was deliberately **not** used — a second name for the same URL is a
way to push to the wrong destination by accident. The game repository has no remote.

**A measured constraint that decides how a future push works.** `origin/main` is `766ecef` (*"Release
v0.11.0"*, i.e. upstream's release line) and is **not** an ancestor of local `main`; the two have
diverged **169 upstream-only / 24 local-only**. So `git push origin main` is currently **non-fast-forward
and would be rejected**. It becomes a valid fast-forward only once the `A4s` merge lands `766ecef` in
local `main`'s history — which is exactly what `A4s` is for, and what the packet's Closure clause
already assumes. Nothing is manufactured to force it; the workflow decides when a merge is valid.

**No push occurred in this execution** — `main` is at `0d7929c`, which is not a descendant of
`origin/main`, and `R-CONFLICT` is a **no-push state** under the new policy as well.

**Follow-up — two new upstream variables need a `jsrf-run-profiles.md` classification.** The sync brings
in two environment names that did not exist locally (Advisor-observed at the merge tree `75083476`, and
**independently re-measured by this session** with the authorized `-f` pattern-file form: 42 unique
names at `0d7929c`, 44 at `75083476`, difference exactly these two, none removed):
**`RECOMP_APU_MIXDOWN_ALL`** (`apu_dsp.c:91`, **default ON**, sums all 32 mixbins into the host monitor
buffer) and **`RECOMP_USB_PORT`** (`ohci.c:819`, selects the port the virtual pad appears on). Neither is a
classifier-listed or deleted name, so neither fails the merge; they are recorded as **"new unclassified
variable"** and need a classification edit. **Uncertain and load-bearing:** whether `MIXDOWN_ALL`'s
default-on path writes anything the **guest reads back** — the Advisor did not verify that
`monitor.frame_buf` is guest-invisible. **If it is guest-visible, it is new default-on device behaviour**,
and `A4b1` (which rewrites `apu_dsp.c`) must classify it before any strict APU claim.

**Candidate future blocker — the RR wait (Advisor lead, not a packet).** JSRF contains
upstream's AC'97 "Reset Registers" pattern: `0x1A6F6F mov byte [eax+0xFEC0010B],2`, then
`0x1A6F7F mov cl,[eax+0xFEC0010B]` / `and cl,2` / `0x1A6F88 test cl,cl` / `jne 0x1A6F88` — a
hoisted single read that spins on the RR bit self-clearing. The same RR write occurs at
`0x1A7406`. **Uncertain whether current runs reach it**; the A3a/A4a runs stop at the DSP spin
first, which is consistent with not reaching it. If a later run stalls at `0x1A6F88`, the RR
self-clear becomes its own admission packet (the trap then becomes a precondition of `A4b`
rather than a follow-up). It is **not** a reason to arm the trap in `A4s`.

**Then:** `A4b1`/`A4b2` (drafted, split by a fresh Planner from `A4b`) resume with
re-established baselines. Their in-flight adequacy verdicts remain useful as design
feedback. `A4b1` = port, licence, fixtures, unchanged default path, no guest-run claim,
toolkit-only writes; `A4b2` = the strict trap+trace run (boot, run, clear, no-CPU, inputs),
game-only writes, with preconditions P1 (`A4p-r1` ACCEPTED `O-GATE`) and P2 (`A4b1`
ACCEPTED).

## Draft packets — `A4b2` only (`A4b1` is **ACCEPTED and PUSHED**; see `CURRENT PACKET` above)

> **Corrected 2026-09-26.** This block previously described **both** packets as drafts at r3. **`A4b1` is
> no longer a draft:** `A4b1-r4` was **accepted (`R1-PASS`)** and **pushed** to the fork at
> `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d`. Its history is preserved below rather than deleted, because
> the `A4b2` revision cites it.

- **`A4b1` — ACCEPTED, PUSHED, COMPLETE (historical entry).** Was `A4b1-r3`, SHA-256
  `321ABCF7C9319B5AC4661B384A21CA2D2CE39C792D9820C9C322B7979EC7AFDA` (373 lines); **superseded by
  `A4b1-r4`** (`6DD62A57E87445F5C12085210146204AA4E77D26FD316071FD41CAEC76835C38`, 445 lines), which
  passed **all four criteria** and every gate and was **accepted by the two-stage review** — stage 1
  `NOT ACCEPTED` on `AC-PORT`, corrected, **stage 2 `ACCEPT`**. `A4b1-r3` remains recoverable from git
  (`git cat-file blob 3f05c0f95205bfef06c0d75a0ba1038b670e9550`). **Do not reopen.**
- **`A4b2`:** `docs/packets/a4b2-gp-clears-pending-word.md`, currently `A4b2-r3`, SHA-256
  `CFB8C0EBFBC9BE3CFB62677C4C756694DDE9663542E239570B639DED9F5BFCE9` (334 lines, including the appended
  r4 sketch). Claim: in one strict run (`RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`) the `loc_001A18D0` wait
  was satisfied by modelled GP execution of the guest's own command. Preconditions P1 (`A4p` `O-GATE`) and
  P2 (`A4b1` ACCEPTED) are **satisfied** — but **P2 pins `A4b1-r3` and its `AC-INPUTS` decides from the
  retired per-key `[GPIN]` lines and `GPIN_OVERFLOW`, so the packet is stale against the accepted
  baseline.** Its revision to **`A4b2-r4`** is planned and its **sketch was cleared by the Advisor
  (`SHAPE: PROCEED`)**; **the body is not yet written because the Claude route is down** — see the
  `CURRENT PACKET` block. Game-only writes.
- **The ledger (ruling 1) is sound and unchanged.** Device semantics 6 is a **write-once
  watched-word ledger** (atomic `seq`, uncapped counters, per-class latches — `GP_CLEAR`,
  `GP_ZERO_OVER_ZERO`, `GP_ZERO_OVER_OTHER`, `GP_NONZERO_OVER`, `GP_PARTIAL`; CPU
  `ANCHOR`/`ZERO`/`ZERO_OVERFLOW`/`OTHER` via an exported `apu_watch_cpu_store`); Device
  semantics 7 is trace-only. Three independent reviews confirmed it faithful to the ruling and
  closed all seven r1 blocking defects.
- **The input accounting (`[GPIN]`) is being redesigned (ruling 3, §5.5 third trigger).** Three
  consecutive blocking verdicts: a lossy once-per-key log decided `AC-INPUTS`; the fix was a
  256-entry table; and that table's key universe is **1024 words** for MIXBUF alone
  (`GP_DSP_MIXBUF_BASE 0x001400` + `DSP_MIXBUFFER_SIZE 1024`, Session-verified), so one frame's
  mixbin sweep overflows it → false `R2-UNKNOWN`. The redesign keys **provenance classes over
  statically enumerated finite universes** (MIXBUF 32 bins, PERIPH 128, FIFO 6, DMA region
  class 4), with counters that **cannot overflow by construction**, a write-once `at_clear`
  freeze at `GP_CLEAR`, and `GPIN_OUT_OF_UNIVERSE` as a **bug detector**. See
  `docs/reviews/a4b-gpin-accounting-ruling.md`.
- **Split decision (Planner, one line):** split, because the port's scale risk is settled by
  build+ctest+one default run and should not wait for, or be reviewed with, the run criteria.
  The Advisor confirmed **do not merge the packets** — the leak was a decision rule placed in a
  packet that cannot change the code producing its input, not the split itself.
- **`AC-PIO` and `R-PIO-DATA` are deleted** from both — `A4p`'s `O-GATE` discharged Q1
  condition 3, so the criterion is retired rather than repaired.
- The former `docs/packets/a4b-gp-dsp-engine.md` (`A4b-r3`) is **superseded** by this split
  and retained as provenance only.
- **Open:** `A4s` is not accepted, so neither packet has a baseline yet. If a pinned xemu write
  path cannot route through the single GP write function, `A4b1` stops and goes to the Advisor.
- **Records:** `docs/reviews/a4b-watch-ledger-ruling.md` (ruling 1, verbatim);
  `docs/reviews/a4b-gpin-accounting-ruling.md` (ruling 3, verbatim);
  `docs/reviews/a4b1-a4b2-r2-adequacy-review.md` and `…-r3-…` (both `INADEQUATE`);
  `docs/reviews/a4b-pio-methodology-ruling.md`; `docs/reviews/a4b-xemu-pin.md`;
  `docs/reviews/a4b-q1-advisor-ruling.md`; `docs/reviews/a4b-q2-owner-decision.md`.

## Last closed packet — `A4p-r1` (discovery, ACCEPTED 2026-09-24)

- **Packet:** `docs/packets/a4p-pio-gate-analysis.md`, revision `A4p-r1`, class
  **discovery**, frozen SHA-256
  `B8BDBFAEC31213AA43D7A2BC8771326A73BE6CC124B6E2993BD2D33C57687A4B`.
- **Question:** are the 28 direct reads of `0xFE820010` in `DSOUND` gate-only under the
  locally-checked calling-convention rule C1–C4?
- **Result — selected outcome row `O-GATE`:** E0–E2 clean, **all 28 sites PASS**, 0 FAIL,
  0 UNKNOWN, E3 clean. Every loop is a threshold re-poll with no store; on every exit path
  `T` empties before any use, by immediate overwrite, by a callee-save `pop`, or by C1 at a
  call. **C3 was invoked at 0 of 28 sites.** The `0xFE800000` table resolves to voice-list
  registers (`0x2054…0x2074`), not `PIO_FREE`, and the two stray constants are unreferenced.
- **This discharges Q1 condition 3**, and therefore **retires `AC-PIO`**: `PIO_FREE` is
  gate-only, so the stub decides whether the guest reaches a point but not the data it
  writes. `A4b` cites this row as a precondition.
- **Claim limits:** a discovery packet never satisfies a strict criterion and never claims
  anything works (§5.8). The result rests on the **inferred** premise that DSOUND follows
  the standard x86 convention, checked at every boundary the analysis relies on but not
  everywhere; it cannot see a custom `edx`-return convention passed on untouched; it covers
  only the 28 direct reads, not register-indirect or computed access beyond E3, timing, or
  whether `0x80` is the true device value.
- **Records:** adequacy `docs/reviews/a4p-r1-adequacy-review.md` (`ADEQUATE`);
  execution `docs/reviews/a4p-execution-evidence.md` (SHA-256
  `3DF5D833E89863E5E9BD0264CC64154C6FEADBD6FE10FC1806F6323FF20981F9`); acceptance
  (`ACCEPT`, first stage, final) `docs/reviews/a4p-r1-acceptance-review.md`; revision log
  `docs/reviews/a4p-revision-history.md`.

**Follow-ups carried forward (not authorized until promoted in a packet):**

- **`A4b` ordinary repairs** (§5.4, different mechanism from the retired `AC-PIO`): the
  `[GPDMA] watch` cap must exempt the deciding `payload=0` line (Advisor ruling (d)), plus
  r3 deferred D1, D2, D4, D5. D3 is superseded by C4 and is discharged by `A4p`.
- **C3 caller-enumeration text** (in the frozen `A4p-r1`, deferred): it searches
  `sub_<ENTRY>(`, which matches only the definition; any future C3 use must enumerate
  callers by value — XBE `call rel32` to the entry, cross-checked by the normalised target
  literal in `RECOMP_ABI_CALL(0x<ENTRY>u, sub_<ENTRY>)`. Second instance of the
  `AGENTS.md` rule against enumerating by one spelling.
- **`PIO_FREE` model packet:** still required before the **first strict liveness or
  boot-progress criterion past the spin**. Finding for it: xemu's own `vp_read` calls its
  `0x80` a pretence, so the obvious secondary source cannot corroborate it.
- **Scope `A4b` to the whole GP block, not just GPRST:** R1 shows five GP offsets written.
- `gp_ep_reads` counts only `[APUMMIO] read` lines, so a GP read-modify-write logged as a
  write would not register. Tighten if the row is reused.
- Archive raw `ctest` output in run directories.
- `P0.1-AC1` re-review; moving `RECOMP_AC97_READY` into the classifier's
  `RETIRED_OVERRIDES` registry; the unmodelled AC'97 registers `0xFEC0017C` / `0xFEC00100`.
- Owner-reserved, unchanged: the `0xFE820010` "GP sample counter" / `PIO_FREE` naming
  conflict (`docs/jsrf-run-profiles.md:288-292`).

## `A4b` — parked, awaiting `A4p`'s outcome

**`A4b`, the GP DSP56300 engine**, is the packet `A4a-r2`'s row `O-6` selects, and both its
gates are answered (Q1 by the Advisor, Q2 by the owner). It is **not** promoted and has had
three revisions and two `INADEQUATE` verdicts, both on its `AC-PIO` criterion — which is why
the §5.5 redesign moved that criterion into `A4p`. When `A4p` returns `O-GATE`, the next
`A4b` revision deletes `AC-PIO` and `R-PIO-DATA`, adds the `A4p` precondition, and folds in
the ordinary repairs (different mechanism, §5.4): the `[GPDMA] watch` cap must exempt the
deciding `payload=0` line, plus r3 deferred D1, D2, D4 and D5. D3 is superseded by C4 and
lives in `A4p`. Its other criteria are otherwise unaffected.

**Records:** `docs/reviews/a4b-r3-adequacy-review.md` (second `INADEQUATE`, verbatim);
`docs/reviews/a4b-r2-adequacy-review.md` (first `INADEQUATE`); `docs/reviews/a4b-r2-session-checks.md`
(the import at `0x1C4004` = kernel ordinal 161 `KfLowerIrql`); `docs/reviews/a4b-r1-adequacy-attempt-1.md`
(`pending — reviewer unavailable`); `docs/reviews/a4b-q1-advisor-ruling.md` (Q1 plus the
PREMISE_CHANGED addendum and the condition-3 handoff to `A4p`); `docs/reviews/a4b-planning-rulings.md`
(checkpoint-40 plus the 3(ii) amendment); `docs/reviews/a4b-xemu-pin.md` (xemu pin
`67cc79e663038d1f55448c0f566b37dde016adf6`); `docs/reviews/a4b-q2-owner-decision.md`;
`docs/reviews/a4b-revision-history.md` (non-authoritative).

**General rules recorded** (Advisor-directed): (`docs/agent-workflow.md` §5.5) a criterion
whose evaluation is itself a multi-step static or dynamic analysis the Planner cannot
complete by reading belongs in a discovery packet that runs first, and the change packet
cites its accepted outcome as a precondition; (§6.1) a criterion claiming something about
*every* access to an address must derive its population from the original XBE with operands
normalised to `uint32`, reconcile by normalised **value** rather than spelling, freeze the
count and generating command, condition PASS on `count == frozen count`, and state what the
method cannot see. The lifter spelling fact is operating knowledge in `AGENTS.md` under
"Generated-source rules".

**General rule recorded** (Advisor-directed, `docs/agent-workflow.md` §6.1): a criterion
claiming something about *every* access to an address must derive its population from the
original XBE with operands normalised to `uint32`, reconcile against the generated code by
normalised **value** rather than spelling, freeze the count and generating command, condition
PASS on `count == frozen count`, and state what the method cannot see. A text search is a
lead, never a completeness witness. The lifter spelling fact (an `A1` moffs load → hex; a
ModRM `disp32` → signed decimal; every address ≥ `0x80000000` exposed) is now operating
knowledge in `AGENTS.md` under "Generated-source rules".

## Last closed packet — `A4a-r2` (discovery, ACCEPTED 2026-09-24)

- **Packet:** `docs/packets/a4a-dsp-pending-word.md`, revision `A4a-r2`, class
  **discovery**, frozen SHA-256
  `2366E18C583B3ED82124F0E5D3AAD14DB38B4D3CA493F1695E44FFE779722FBC`.
- **Question:** is the next packet the GP DSP56300 engine (`A4b`), and what must its scope
  include? Unknowns U1–U4: the last `GPRST` value; whether the title reads GP/EP at all;
  whether scratch page 0 holds the XBE `0x001BA0A0` image with the pending word `3` at
  `+0x810`; whether the stop holds on the instrumented build.
- **Result — selected outcome row `O-6`** ("the start write is the only GP interaction; the
  image is in place"), from two strict runs on game `5b58d13` / toolkit `0d7929c`:
  - `U1` answered: `last_GPRST = 0x00000003` (`&3 = 3`), and the trace now reports the
    values the guest actually wrote. The value oracle matched the original XBE immediates
    exactly: `write 0x3FF14 = 000000FF` (`0x001A582A`) and `write 0x3FFFC = 00000003`
    (`0x001A585E`), where the pre-change build logged `00000000` for both.
  - `U2` answered: **zero** GP/EP reads, with a coverage witness (the once-per-block read
    note is unbounded by the trace cap and never fired).
  - `U3` answered: `G = 803CC000`, `S0 = MEM32(G) = 803C0000 = B`, and all `0x5CC` bytes
    equal the XBE image at file offset `0x1A7D60`.
  - `U4` answered: both runs `diagnostic_deadline` with `F = 2` on the offset-unpinned
    frame pattern, i.e. the stop holds with and without the trap.
- **Instrumentation:** toolkit `0d7929c` (`src/apu/apu_mmio_hook.c` only, 39+/6−). T1
  reports the value the access moved; T2 names the 400-line cap when reached. Both sit
  inside the existing `RECOMP_APU_TRACE` block, off by default; the R0 leak check is their
  witness (0/0/0). exe `87971604…7ef` → `9597ff7c…c553`; `ctest` 12/12.
- **Claim limits:** a discovery packet claims observations only and never satisfies a
  strict criterion (§5.8). R1 reached the spin only after its `0xFE820010` polls were
  answered by the `PIO_FREE` stub constant `0x80`, so **its arrival at the spin is
  exploratory-grade however R1 classifies** (Advisor ruling B). Nothing here shows the GP
  would clear `+0x810` if it ran — only real GP DSP56300 execution can (ruling D).
- **Records:** adequacy `docs/reviews/a4a-r2-adequacy-review.md` (`ADEQUATE`,
  `BLOCKING: NONE`); execution `docs/reviews/a4a-execution-evidence.md`; acceptance
  (`ACCEPT`, first stage, final) `docs/reviews/a4a-r2-acceptance-review.md`; revision log
  `docs/reviews/a4a-revision-history.md` (non-authoritative). Superseded `A4a-r1` is
  preserved at game commit `5e739f6`.

**Follow-ups carried forward (not authorized until promoted in a packet):**

- **Gates `A4b`:** **Q1 — ANSWERED** (Advisor, 2026-09-24): the upstream `PIO_FREE` stub
  dependence does **not** contaminate A4b's strict claim, and a `PIO_FREE` model is neither
  a prerequisite for A4b nor part of it. Case ruling recorded verbatim in
  `docs/reviews/a4b-q1-advisor-ruling.md` (Advisor child
  `407c54a3-6ca4-4a65-835b-faf5355195cd`, route `claude/claude-opus-5-5` @ `high`); its
  general clarification is in `docs/jsrf-run-profiles.md` §"Feature enablement". A4b must
  still establish the ruling's four conditions (who wrote the 0; the GP's input
  provenance; that `PIO_FREE` only gates; the claim limits) and must cite the ruling.
  **Q2 — ANSWERED, owner decision (§3.4), 2026-09-24:** the owner accepts
  **GPL-2.0-or-later** for this open-source project, so porting xemu's DSP56300 core is
  permitted. Recorded in `docs/reviews/a4b-q2-owner-decision.md`. The licensing obstacle is
  removed; whether A4b ports the existing core or implements one independently is now a
  **technical** choice for the Planner. **Mechanical consequence for A4b's closure:** a
  binary linking a GPL-2.0-or-later core is a combined work that must ship under
  GPL-2.0-or-later, so closure must update `NOTICE` and the licence files (verbatim GPL text
  alongside the existing LGPL text). That is bookkeeping following from the decision, not a
  further owner question. A4b should also **pin** the xemu commit it ports; the sources
  fetched this session were `master` and are not pinned.
- **`PIO_FREE` model packet (Advisor-directed prerequisite):** required before the **first**
  strict liveness or boot-progress criterion past the spin (e.g. "the title reaches
  <checkpoint> in a strict run"). It is its own packet, placed before that criterion, not
  inside A4b. **Finding for it:** xemu master `hw/xbox/mcpx/apu/vp/vp.c` `vp_read` returns
  `0x80` for `NV1BA0_PIO_FREE` with the comment *"we don't simulate the queue for now,
  pretend to always be empty"* — the obvious secondary source describes itself as a
  pretence, so it cannot corroborate `0x80` as device state, and our `apu_vp.c` is
  xemu-derived and therefore not independent either. The `jsrf-run-profiles.md` reversal
  condition (ii) most likely needs a real FIFO/free-count model or a primary source, not a
  citation.
- **Scope `A4b` to the whole GP block, not just GPRST:** R1 shows five GP offsets written
  (`0x3FF00`, `0x3FF04`, `0x3FF10`, `0x3FF14`, `0x3FFFC`×3). Recorded as an acceptance
  advisory on `A4a-r2`.
- `gp_ep_reads` (as defined in `A4a-r2`) counts only `[APUMMIO] read` lines, so a GP
  read-modify-write logged as a write would not register. It did not bite here. Tighten if
  the row is reused.
- Archive raw `ctest` output in run directories; the packet's Closure named it but only
  `CTestCostData.txt` survives.
- `P0.1-AC1` re-review; moving `RECOMP_AC97_READY` into the classifier's
  `RETIRED_OVERRIDES` registry; the unmodelled AC'97 registers `0xFEC0017C` / `0xFEC00100`.
- Owner-reserved, unchanged: the `0xFE820010` "GP sample counter" / `PIO_FREE` naming
  conflict (`docs/jsrf-run-profiles.md:288-292`).

**Measured blocker (unchanged):** the guest spins at `loc_001A18D0`
(`recomp_0005.c:6748-6751`, live frame in `sub_001A1769`) on a DSP pending word at
`+0x810` that nothing clears. The run ends `diagnostic_deadline`.
`RECOMP_APU_DSP_ACK` stays forbidden as acceptance evidence. **Caution:** A3a's predicted
next stop (`div@0x001A2BFC`) came from an exploratory run with `RECOMP_GPU_ACK` enabled,
whose synthetic completion cleared this word; it is not a prediction of strict behaviour.

**Baseline (2026-09-24).** The accepted A3a work is committed: toolkit `c97ce2c`
(`src/kernel/xbox_memory_layout.c`) and game `a16350f` (`scripts/jsrf_run_profile.py`,
`scripts/ac2-provenance.py`, `tests/test_ac2_provenance.py`). Both trees were clean
after the baseline commits; any later dirty file is new work.

## Last closed packet — `A3a-r25` (ACCEPTED 2026-09-24)

- **Packet:** `docs/packets/a3a-ac97-codec-model.md`, revision `A3a-r25`, SHA-256
  `6F907A42EAC6FD02392E11EADEF127AE840D97A245E771CD15B63A387F0E5BDC`.
- **Change:** the environment-gated `RECOMP_AC97_READY` override was replaced by an
  always-on model in the `nv2a_ack_thread` loop:
  `GS(0xFEC00130).bit8 := GC(0xFEC0012C).bit1`, level-evaluated each tick, atomic,
  outside the `g_apu_mmio_trapped` gate; `GC` is read only.
- **Result:** one **strict** run selected `R-PASS`; `AC1`–`AC6` all PASS. The codec
  poll succeeds, the vector-6 ISR is connected, and the guest proceeds into DSP
  initialisation, where it now spins (the blocker above). The strict baseline
  previously self-relaunched before reaching the poll.
- **Claim limits:** one strict run's codec-presence wait was satisfied by modelled
  device state. This does not establish faithful hardware behaviour (secondary
  sources only), audio, a complete DSP handshake, liveness, or strict boot.
- **Records:** adequacy `docs/reviews/a3a-r25-adequacy-review.md`; execution
  `docs/reviews/a3a-execution-evidence.md`; acceptance (`ACCEPT`, no disagreements)
  `docs/reviews/a3a-r25-acceptance-review.md`.

**Follow-ups carried forward (not authorized until promoted in a packet):**

- `P0.1-AC1` is reopened by A3a (see P0 below).
- Move `RECOMP_AC97_READY` into the classifier's `RETIRED_OVERRIDES` registry and
  update `tests/test_run_profiles.py:560`. Its `removed_commit` boundary now exists
  (toolkit `c97ce2c`); the `added_commit` must be found in toolkit history.
- `RECOMP_APU_TRAP` belongs with the DSP packet.
- The AC'97 registers A3a does not model (`0xFEC0017C`, `0xFEC00100`) wait for a
  later packet.

## Retired packet — `A2h-r6` (premise refuted, 2026-09-23)

`docs/packets/a2h-writer-investigation.md` was retired at adequacy review, never
executed. Record: `docs/reviews/a2h-r6-adequacy-review.md`. Its premise — an invalid
target `0x00700010` dispatched from `0x0017DC23` — was refuted: that failure came
through the static-initializer walker's `tail_jump_alias` entry (`0x0017E58F`) and
was already fixed by commit `cb7cae2`; every run that logged it predates the fix.
`sub_0017DBBD`'s execution status is simply uninstrumented (UNKNOWN). Do not revive
this area without new evidence. Two lessons stand for any future packet that must
observe **inside** a generated function: it first needs a trace seam outside the
provenance-guarded files and a trace mode that keeps the **last** N records; and the
collector's call history plus the frozen dump should be tried first, since they
distinguish call sites with no code change.

## P0 — ACCEPTED

**P0.S and P0.1–P0.7 are accepted.** They are completed prerequisites, not active
implementation instructions. Do not reopen them unless a later edit invalidates an
affected accepted criterion; then reopen only that criterion and re-review it.

Durable evidence:

- `docs/reviews/p0-1-execution.md`
- `docs/reviews/p0-1-vblank-adjudication.md`
- `docs/reviews/p0-2-acceptance.md`
- `docs/reviews/p0-3-to-p0-7-acceptance.md`

The original P0 contracts under `docs/packets/` are frozen provenance; their old
`proposed`, `pending`, `next packet` and `TOOLING REQUIRED` wording is not current
work authorization.

### Reopened: `P0.1-AC1` (by `A3a-r25`, 2026-09-24)

Re-review only `P0.1-AC1` against the new runtime.

1. **Cited sites moved.** Its accepted evidence names `xbox_memory_layout.c:684-686,1613`
   (`docs/reviews/p0-1-vblank-adjudication.md:265`,
   `docs/packets/p0-acceptance-contract.md:78`). A3a changed that file, so both ranges
   shifted. The re-review should cite **symbols**, not line numbers.
2. **`P0.1-AC3` is affected in principle.** `_valid_new_archive` compares recorded
   reasons verbatim (`scripts/jsrf_run_profile.py:585-586`) while `CLASSIFIER_VERSION`
   stays `jsrf-run-profile/1`, so editing the reason string at `:391` makes an archive
   recorded under the old string unreclassifiable.
3. **Measured impact today is zero.** Of 650 archived runs, exactly one carries a
   `run_profile`, and it holds no AC97 reason. The classifier still treats the retired
   name as exploratory, which can never produce a false `STRICT`.

## Evidence and decision rules

- `MEASURED` means inspected source/artifact evidence with an identity/procedure.
  `INFERRED` means a hypothesis or expected consequence.
- Missing, malformed, stale, unexercised or `CANNOT VERIFY` evidence is never PASS.
- Only a verified **strict** run can support strict integration/boot/liveness/device
  claims. Exploratory/fixture evidence remains bounded to what it actually measured.
- A reviewer verifies or refutes each mandatory criterion.
- A disagreement goes to the Persistent advisor, whose ruling is final within the
  evidence invariants of `docs/agent-workflow.md` §2.4.
- A failed measurement cannot be converted to PASS by Planner, Session, reviewer or
  advisor interpretation.
- A post-review code/evidence change reopens the affected criterion.
- Original assets and existing saves remain unchanged.

## Roadmap policy

Older A1–A5 sequences, GPU/audio milestone diaries, historical `next packet`
statements, old model assignments and legacy status tables live in
`docs/jsrf-operating-history.md`. They are provenance only.

New executable work follows the packet lifecycle in `docs/agent-workflow.md` §5: the
Planner designs the packet, an adequacy review of the exact revision returns
`ADEQUATE`, and that frozen revision is promoted into **CURRENT PACKET** in the same
step. Historical evidence may motivate a packet, but a historical failure does not
imply the same failure exists in the target revision.

If no packet is explicitly promoted in this file, implementation is **BLOCKED**.
