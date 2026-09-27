# `A4b2-r7` execution — Advisor ruling on the `R2-EXPL-INPUT` routing

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Advisor:** persistent handle **`muse_FkNhGaXtV9P5`**, model **`muse-spark-1.3-contributor`**,
**`reasoningEffort: max`** (reported by the completed turn).
**Consulted under:** §2.3, §4.1 `Session -> Advisor`; the `R2-EXPL-INPUT` row's own direction
(`packet:128`): *"the Advisor decides whether that input needs its own model packet first."*
**Context:** `A4b2-r7` executed → **`R2-EXPL-INPUT`**. Evidence: `docs/reviews/a4b2-r7-execution-evidence.md`.

Recorded **verbatim** as §3.3 and §4.4 require.

---

## Ruling (verbatim)

```text
RULING:

Q1 — Which stub matters: BOTH. Either alone sustains R2-EXPL-INPUT, and neither
is decided yet. MIXBUF is the heavy one: 26 bins × 7648 reads_while_stub — the GP
program's main loop wholesale consumes mixbin values while a VP voice is ACTIVE
(MIXBUF_STUB_READ latch observed=1, frame 17), and the first consumed value is 0
where real hardware would present mixed voice audio. Genuine value contamination
of GP inputs, not timing. 0xFFFFB3 is the narrow one: 1017 polls (≈4/frame, same
cadence as the modelled housekeeping polls) of a register whose hardware meaning
is unknown even to upstream (`v = 0; // core->num_inst; // ??`). Whether either
PROPAGATES to the doorbell exchange is currently UNKNOWN — and UNKNOWN stays
UNKNOWN: r7's R2-EXPL-INPUT is terminal for r7, the strict claim is not
established, and no part of this ruling converts that.

Q2 — Model or narrow: NEITHER directly. The correct next step is ONE DISCOVERY
packet proving or refuting NON-RELIANCE of the doorbell exchange on both stub
inputs, to a two-leg standard:
  Leg 1 (static mechanism): from image I (0x173 words), identify the doorbell
  write(s); show payload, address, and every branch guarding the write on the
  executed path are independent of mixbin values and 0xFFFFB3 values.
  Leg 2 (empirical invariance): perturbation runs varying consumed stub values
  adversarially (all-zero, all-ones/max, pseudorandom; 0xFFFFB3 likewise) must
  reproduce the identical doorbell latch (va, observed=3, payload=0, dsp_addr)
  with frames advancing. The perturbation hook is discovery instrumentation
  (§5.8: reversible, env-gated, off by default at closure) — a new packet's
  toolkit scope, NOT a reopening of closed A4b1.
  Either leg failing REFUTES non-reliance. 0xFFFFB3 register identification
  (docs/archaeology) rides along as a parallel thread: if hardware ties it to 0
  (reserved), it drops out as modelled-correct independently.
  Conditional technical ruling, recorded now so the Planner can rely on it: Q1
  condition 2's PURPOSE (per its own "Why" + the per-value "each value it RELIES
  on" rule) is satisfied FOR THESE TWO INPUTS by a completed two-leg
  non-reliance proof — the follow-up A4b2 revision may then cite the discovery
  and restate its input qualifier accordingly, with no model packet for those
  inputs. This substitutes a HIGHER purpose-directed bar (proof of
  non-reliance) for a blanket proxy (zero reads); it waives no evidence, sets
  no precedent beyond inputs with a completed proof, and does not touch Q1's
  text for any other input. If the discovery refutes non-reliance, its outcome
  table routes to the honest death branch: the strict A4b2 claim is BLOCKED
  until real VP implementation (not a model) + 0xFFFFB3 resolution, and A4b2
  retires or waits — that branch must be explicit in the discovery packet.

Q3 — Model admissibility: a MIXBUF value-model is RULED OUT — inadmissible under
run-profiles criterion 4. Mixbin contents are the RESULT of VP voice-mixing work
consumed by the GP program whose execution is being certified; supplying them
without the VP is synthetic completion by the policy's own definition ("no
computation behind it" fails). The narrow "idle bins read 0" escape is closed by
measurement (voice active, observed=1). No Planner time goes there. A 0xFFFFB3
model is PREMATURE (criterion 2 unmet — meaning unknown); it becomes assessable
only if the discovery identifies the register. PIO_FREE ordering: NO hard
constraint — guest-side FIFO polls and GP-side mixbin consumption are independent
mechanisms. Natural order is this discovery first (A4b2 critical path), PIO_FREE
after (it gates post-spin claims downstream of A4b2), but sequencing is the
Planner's call.

BASIS:
- Observed: r7 row + AC results incl. three stub conditions with counts
  (r7-evidence:95-117); steps 1–2 valid, blocks identical, zero (ii) violations
  (r7-evidence:98-105); row predicted in advance, not a surprise
  (r7-adequacy:19,28,35-52); R2-EXPL-INPUT row text routes here (packet:128).
- Observed: mixbuf_stub=(voices>0)||(nonzero sample), set per frame
  (apu_watch.c:604-618; gp_ep.c:583-585,599-610); VP results written to GP X mem
  every frame (gp_ep.c:601-610, pinned behavior); latch observed=1 voice active,
  payload=0 consumed, frame 17 (r7 R1 log:3998).
- Observed: 0xFFFFB3 placeholder `v=0 // ??` (dsp.c:56-57); poll cadences
  ≈4/frame alongside modelled offsets (r7-evidence:111,115).
- Observed: admission criteria incl. criterion 4 + "no computation behind it"
  (run-profiles:269-298); Q1 condition 2 text + "exploratory for that
  dependency" + per-value purpose (a4b-q1:20-42, previously read); predecessor
  deferred this exact routing ("routes to me", a4b2-r4-planning-rulings:51).
- Inferred: 26×7648 with 256 frames to clear ≈ full per-frame bin consumption —
  the program's main loop is audio consumption; doorbell is a mailbox write
  within/after it (structure inference — the discovery must prove or refute,
  not assume, this).
- Uncertain: 0xFFFFB3's hardware meaning; whether any doorbell-path branch tests
  stub-derived values; image I's internal structure (all discovery scope).

REVERSED BY: a hardware source showing mixbin values are NOT VP-work results
(reopens modelability — implausible, the frame path overwrites all bins from VP
output); demonstrated dependence of the doorbell on stub values (collapses the
discovery to its death branch early — contained, not reversing); 0xFFFFB3 shown
reserved-zero by a primary source (removes it from discovery scope, mixbuf leg
proceeds alone).

RECORD IN: new A4b2 review record, verbatim with Advisor handle/model/
reasoningEffort (§3.3); the conditional Q1-purpose ruling recorded there as a
case ruling confined to these inputs; cross-referenced from the Planner's
discovery packet and the revision history. No edits to Q1, the ledger ruling, or
any packet contract.
```

## Session verification of the ruling's load-bearing citations

Both were checked directly at toolkit `3a3c7c1` before the ruling was relied on. **Session observations.**

| Advisor citation | Session check | Result |
|---|---|---|
| VP results are written to GP X memory every frame | `gp_ep.c:601-610` — `apu_gp_mixbuf_frame_begin(mcpx_apu_vp_active_voices(d))`, then a loop over all `NUM_MIXBINS` × `NUM_SAMPLES_PER_FRAME` doing `dsp_write_memory(d->gp.dsp, 'X', base + sample, w)` with `w = float_to_24b(mixbins[mixbin][sample])` | **CONFIRMED** — the mixbin content **is** VP output, so the GP genuinely consumes VP-produced data |
| `mixbuf_stub = (voices>0) \|\| (nonzero sample)` | `apu_watch.c:604-618` — `apu_gp_mixbuf_frame_begin` sets `(vp_active_voices > 0)`; `apu_gp_mixbuf_note_sample` sets `1` for any `word != 0` | **CONFIRMED** |
| MIXBUF_STUB_READ latch `observed=1`, `frame=17` → **a voice was active** | `[GPWATCH] latch class=MIXBUF_STUB_READ seq=3 va=00001400 observed=00000001 payload=00000000 … frame=17 insns=19492` in the r7 R1 log | **CONFIRMED** — this closes the "idle bins read 0" escape the Advisor names |
| `0xFFFFB3` is a placeholder | `dsp.c:56-57` — `case 0xFFFFB3: v = 0; // core->num_inst; // ??` | **CONFIRMED** |

## The conditional case ruling (recorded as §3.3 directs)

Confined **to these two inputs only**:

> Q1 condition 2's **purpose** — per its own *"Why"* and its per-value *"each value it **relies on**"*
> rule — is satisfied for `MIXBUF` and `0xFFFFB3` by a **completed two-leg non-reliance proof**. The
> follow-up `A4b2` revision may then cite the discovery and restate its input qualifier accordingly, with
> **no model packet** for those inputs. This substitutes a **higher**, purpose-directed bar (proof of
> non-reliance) for a blanket proxy (zero reads); it **waives no evidence**, sets **no precedent** beyond
> inputs with a completed proof, and **does not touch Q1's text** for any other input.

## Consequential obligations

1. **`r7`'s `R2-EXPL-INPUT` is terminal for `r7`.** The strict claim is **not established**; nothing in this
   ruling converts that. UNKNOWN stays UNKNOWN.
2. **Next packet: ONE discovery packet** (§5.8) proving or refuting **non-reliance** of the doorbell
   exchange on both stub inputs, to the **two-leg standard** (static mechanism from image `I`; empirical
   invariance under adversarial perturbation).
3. **The perturbation hook is discovery instrumentation** — reversible, env-gated, off by default at
   closure. It is a **new packet's toolkit scope**, **not** a reopening of closed `A4b1`.
4. **The discovery's outcome table must contain an explicit death branch:** if non-reliance is refuted,
   the strict `A4b2` claim is **BLOCKED** until real VP implementation (not a model) plus `0xFFFFB3`
   resolution, and `A4b2` retires or waits.
5. **A MIXBUF value-model is ruled out** as inadmissible (run-profiles criterion 4 — synthetic completion).
   **No Planner time goes there.** A `0xFFFFB3` model is **premature** until the register is identified.
6. **`0xFFFFB3` register identification** rides along as a parallel thread.
7. **`PIO_FREE` ordering is unconstrained;** natural order is this discovery first, `PIO_FREE` after, but
   **sequencing is the Planner's call**.
8. **No edits** to `a4b-q1-advisor-ruling.md`, `a4b-watch-ledger-ruling.md`, or any packet contract.
