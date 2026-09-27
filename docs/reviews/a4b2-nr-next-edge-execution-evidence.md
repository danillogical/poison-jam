# `A4b2-NR-next-edge` execution evidence — the GP loads a **second program image**, and the doorbell code is in the **first**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-nr-next-edge.md`, revision **`A4b2-NR-next-edge-r1`**, class **discovery**,
frozen SHA-256 **`7461AAA471D5EE0B78348AC17F7BF5B8CD333A860437705B65946B349311ACFF`** — verified before and
after promotion, **not edited**.
**Selected outcome row: `O-INCONCLUSIVE`** — see §6. **No strict criterion is discharged.**

**This is exploratory/diagnostic evidence — knowledge only, never acceptance of a strict criterion.**

---

## 1. Headline: there are two P-memory images, and the analysis was describing the wrong one

The packet's premise was that the executed program is the boot image `I` (371 words) plus a region above
it, and that the predecessor's error was analysing only the first 512 words of it. **That premise was
incomplete, and the truth is more consequential.**

The GP **loads a second program into P-memory after the bootstrap.** Two full-PRAM decodes — one at
bootstrap, one at the first exchange — show:

| P-memory band | At bootstrap | At the first exchange |
|---|---|---|
| boot image `I` (`0x000`–`0x172`) | real code (1 zero word) | **real code — unchanged** |
| `0x173`–`0x7FF` (1 677 words) | **1 677 zeros** | **real code** (25 zeros) |
| `0x800`–`0xFFF` (2 048 words) | **2 048 × `0xCACACA`** | **real code** (215 × `0xCACACA`, 19 zeros) |

`0xCACACA` is the toolkit's own `memset` fill from `dsp_c_init` (`dsp_cpu.c:299-301`), so the `0x800`–`0xFFF`
region at bootstrap is **uninitialised fill**, not program. By the exchange it holds real instructions:

```
P 0173  021594  move x:(r5 + 4), x0
P 0174  0CC4A0  brset #0,x0,p:$01b4
P 0178  44F400  move #$00002a,x0
P 017A  22AE00  move r5,a
P 0183  060D90  dor #$000d,p:$0189
```

**Quantified:** of the 2 957 words present in both snapshots, **2 480 changed**; 1 007 words that were
decodable at bootstrap are no longer present as instructions at the exchange. The at-exchange image is
**2 957** instruction lines against the bootstrap's **3 964**.

### The consequence, stated precisely

1. **The bootstrap-time decode — and therefore every slice built on it — describes the wrong bytes for
   essentially the whole executed program.** The executed-PC histogram shows 2 340 distinct PCs, of which
   only **193 (8.2%)** lie inside the boot image `I`; **2 147 (91.8%)** are above it.
2. **The zero and `0xCACACA` regions ARE executed, overwhelmingly.** Measured against the bootstrap
   snapshot:

   | Bootstrap-snapshot word at the executed PC | PCs | Executions | Share of all executions |
   |---|---|---|---|
   | `0x000000` (zero) | 1 089 | 6 151 177 | **42.0%** |
   | `0xCACACA` (memset fill) | 1 059 | 8 420 201 | **57.4%** |
   | **combined** | **2 148** | **14 571 378** | **99.4%** |

   Total executions in the run: 14 659 333. **So 99.4% of everything the GP executed was at a PC whose
   bootstrap-snapshot word was either zero or uninitialised `0xCACACA` fill.** The bootstrap decode was
   therefore not merely incomplete — for those PCs it was **wrong**, and a slice built on it would reason
   about `nop`s and fill words that the GP never fetched.
3. **This is exactly the class of error the packet exists to prevent**, one level deeper than expected: the
   predecessor analysed too few words, and the widened analysis initially analysed them **at the wrong
   time**. The Session's own first widened analysis (the CFG in §3, and the `full_decode.txt` artifact)
   inherited this defect.
4. **The second image is a subset in PC terms:** 2 957 words are present in both snapshots, **2 480
   changed**, and **1 007 words decodable at bootstrap are no longer instruction starts at the exchange** —
   consistent with the real code having different (mostly multi-word) instruction lengths. No PC appears
   only in the second snapshot, i.e. the second image occupies a **subset** of the first's instruction
   boundaries.

## 2. What survives: the doorbell code is in the FIRST image

**The doorbell's own instructions are unchanged between the two snapshots** — verified word-for-word:

| PC | Bootstrap | At exchange | |
|---|---|---|---|
| `P 0004` | `62F400` `move #$000800,r2` | `62F400` | **SAME** |
| `P 0007` | `0BF080` `jsr p:$00db` | `0BF080` | **SAME** |
| `P 00B9` | `57E100` `move x:(r1),b` | `57E100` | **SAME** |
| `P 00DB` | `220E00` `move r0,a` | `220E00` | **SAME** |
| `P 00E8` | `0A7092` `move r2,x:(r0+4)` | `0A7092` | **SAME** |

So the **doorbell descriptor path lies entirely within image `I`**, and the predecessor's in-window
readings of it — including the `P 0004` immediate `#$000800` matching the observed `dsp_addr=000800` — are
**not** invalidated by the second image. That is why this record does **not** claim the earlier findings
were wrong about the doorbell; it claims they were **unproven as a complete account of the program**, and
now shows the program is larger and later-loaded than assumed.

## 3. The CFG, built and honestly bounded — **and built on the wrong snapshot**

> **Caveat, stated before the numbers:** the CFG below was built on the **bootstrap-time** decode, which
> §1 shows does not describe the executed program. Its numbers are reported because the *method* is sound
> and the tooling defects found while building it are real and reusable, **not** because its edge set
> describes what the GP ran. A correct CFG must be built on the at-exchange image.

A PC-indexed CFG was built over the full bootstrap decode (script `a4b2-cfg.py`, scratch outside the repo):

| Quantity | Value |
|---|---|
| Nodes (decoded instructions) | 3 964 |
| Edges | 4 002, of which **21 unresolved** (all `rts`/`rti` returns) |
| **Indirect/computed control transfers** | **0** |
| Branch/call targets inside the `00CC`–`00D1` data gap | **NONE** |
| Reachable from entry `0x0000` | **194** |
| Executed PCs not covered by the PC universe | **0** |

**Two analysis defects were found and fixed in the Session's own tooling, and are recorded because both
produced plausible-looking wrong answers:**

1. **Fall-through used `pc+1`.** DSP56300 instructions are 1–*N* words, so `pc+1` is wrong for every
   multi-word instruction. Fixed to use the next *decoded* PC.
2. **Any `(rN)` operand was treated as an indirect jump.** That misclassified **2 062 data moves** —
   `move x0,x:(r1)`, `move x:(r1),b` — as control transfers, producing a bogus "2 083 unresolved edges".
   Only `jmp`/`jsr` with a computed operand are indirect. After the fix: **0 indirect transfers, 21
   unresolved edges (returns only).**

**The reachability result is the honest limit of the CFG:** only **194** of 2 340 executed PCs are
reachable from entry `0x0000` through the static CFG, while 2 147 executed PCs are not. That gap is not a
missing edge — it is **interrupt/vector entry and the second image's own entry paths**, which a linear
CFG from a single entry does not model. **The packet's `L1 = PROVEN` bar is a feasibility slice, and this
CFG does not meet it.** It is recorded as the boundary of what was achieved, not as a proof.

## 4. Instrumentation added, and why

Both additions are env-gated, GP-only, read-only, and off by default:

- **`RECOMP_APU_GP_DECODE` second decode at the first exchange** (`dsp56k_request_decode2`), tagged
  `second-decode-at-exchange begin/end`, so the two snapshots are comparable. **This is what produced the
  headline finding** — without it the second image would have gone unnoticed.
- **Per-core identity logging** (`b9_note_core_identity`), which prints each distinct `opaque` pointer with
  its resolved `is_gp`. Measured: **one** core, `opaque=00000239DED16900`, `is_gp=1`. So the trace's GP
  attribution is **verified, not assumed** — important because the previous packet found `core->is_gp` is
  never populated.

## 5. A Session defect in the promotion, corrected

**The Session promoted this packet mid-write.** The promotion recorded 32 lines / `9E919866…`; the
Planner's final file is **39 lines / `7461AAA4…`**. The promoted hash therefore described bytes that were
not the packet. Cause: the Session polled the file, saw a complete-looking body with an `ADEQUATE` verdict,
and promoted while the Planner's turn was still in flight. **A file that looks finished is not evidence
that its author has stopped writing.** Repaired by re-promoting against the stable bytes (hash re-read
twice, unchanged); the packet was never edited by the Session. Recorded in
`docs/reviews/a4b2-nr-next-edge-r1-session-verification.md`.

## 6. Selected row: `O-INCONCLUSIVE` — **confirmed by the Advisor**

**Advisor ruling:** `docs/reviews/a4b2-nr-next-edge-advisor-ruling.md` (handle `muse_FkNhGaXtV9P5`,
`muse-spark-1.3-contributor`, effort `max`). **Row `O-INCONCLUSIVE` CONFIRMED**, and the ruling adds a
binding **withdraw/re-scope list** and a **loader-first mandate** for the next packet, both summarised in
§7 below.

**`L2 = INVARIANT` stands** (carried; the doorbell tuple is unchanged at
`seq=198852 va=803C0810 observed=3 payload=0 dsp_addr=000800`).

**`L1` is `INCONCLUSIVE`, and this run explains why more sharply than before:**

- The full-program feasibility slice the packet requires was **not** completed — the CFG reaches only 194
  of 2 340 executed PCs from a single entry, and interrupt entry is not modelled.
- More importantly, the slice's **input was wrong**: it was built on the bootstrap snapshot, and the
  executed program is a **later-loaded second image**.
- No concrete feasible causal chain from either stub input to the named doorbell was found, so
  **`O-REFUTED` is not selected** — and the packet is explicit that an in-range read is not by itself
  refutation.

**Per the packet's outcome table, `O-INCONCLUSIVE` selects `A4b2-NR-next-edge-followup`.** The Advisor
confirmed the target and **mandated its phase order** (§7).

**`A4b2-r7` remains `R2-EXPL-INPUT`.** No strict criterion is discharged.

## 7. The Advisor's binding consequences for this record

**Withdrawn or re-scoped — may NOT be relied on in current form (W1–W4):**

1. **`full_decode.txt` as a slice input above `0x172`** — wrong bytes. Kept, **relabeled**
   `bootstrap_state_decode.txt`/`.json`, as the bootstrap-state record (that role is cross-validated and
   valid). A `README.md` now sits in the artifact directory stating the withdrawal.
2. **The §3 CFG edge set and reachability numbers as claims about the executed program.** The **method**,
   both **tooling fixes**, and the finding that a single-entry linear CFG undercovers interrupt/vector
   entry all stand — that gap is about the entry model, not the bytes.
3. **The seven Q2 re-derivations' scope** ("over all 3881 words" of *one* image). The seven **tasks**
   survive; their **input bytes** are void pending the loader/epoch work.
4. **The "one 3881-word program" framing.** It is **≥2 images/epochs** until the loader work says
   otherwise.

**Not reached (still valid):** every local reading inside unchanged `0x000`–`0x172` — the doorbell path,
the `0xFFFFB3` sites and scratch flows, the `00CC`–`00D1` table, `P 00B9`/`P 00A2`/`P 0086`–`008C`; the §8
measurement table; the doorbell tuple; `L2`; the `op =` non-execution inference; `A4b2-r7`'s
`EXPL-INPUT`; `A4b1-r4`. **No `§5.4(2)` anywhere.**

**Forbidden (F1–F3):**

- **F1** — citing the doorbell instructions' byte-identity as **path-independence**. Two gaps byte-identity
  cannot close: **transient modification** between snapshots, and the fact that **the doorbell path may
  extend into the second image** (who calls `P 0000`–`0007`, what guards the trigger, and whether *those*
  depend on stub inputs is entirely open). **Instruction survival ≠ path survival.**
- **F2** — building the slice directly on the at-exchange image. If P-memory is modified continuously
  (overlays/staging), at-exchange bytes are wrong for parts of the execution too.
- **F3** — a fudged single-image slice if P-memory never quiesces. Record per-epoch slicing or
  `O-INCONCLUSIVE` with the reason.

**The next packet must be phased (N1–N3):** **phase 1** establishes the load event(s) — trigger, source
bytes, extent — **ruling a second GPRST bootstrap in or out from existing logs first** (`boots` +
`[GPBOOT]` block count, before any new run), plus **epoch structure and a P-write watch** over image `I`
showing quiescence or mapping every modification (this watch also closes Q2's transient gap). **Phase 2**
(the slice) proceeds only on phase-1-covered bytes.

**Admissibility of this run's instrumentation (Q5):** the Advisor ruled the two-image finding
**admissible as relied-on diagnostic fact on four observed legs** — GP attribution structural *and*
measured (exactly one core, `is_gp=1`), read-only, no-perturbation (the decode runs after the latch and
freeze, so it cannot touch the recorded tuple), and bootstrap-snapshot cross-validation with arithmetic
closure. **No extra absent-control is required as a reliance condition**, though standard closure (final
absent-env run, zero diagnostic tags, tuple intact) still applies.
