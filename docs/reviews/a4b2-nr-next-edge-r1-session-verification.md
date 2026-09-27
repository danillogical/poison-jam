# `A4b2-NR-next-edge-r1` — Session verification and promotion

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-nr-next-edge.md`.
**Authoring Planner:** child `1aadbb20-4a2f-4a3c-9ca5-9699806d2820`, `codex/gpt-6-sol` @ `high`.
**Class:** discovery (§5.8). **Adequacy:** the **writing Planner's own** review, per §5.8 — **`ADEQUATE`**.

| Item | Value |
|---|---|
| Revision | **`A4b2-NR-next-edge-r1`** |
| SHA-256 (frozen) | **`7461AAA471D5EE0B78348AC17F7BF5B8CD333A860437705B65946B349311ACFF`** |
| Lines | 39 |

## Session defect, found and corrected: the packet was promoted mid-write

**The Session promoted a draft of this packet that the Planner was still writing.** The promotion
recorded **32 lines / `9E91986656E5AF857DB22E278EFC3682374F6F43BE8FD6F0129FE892350D695D`**; the Planner's
**final** file is **39 lines / `7461AAA471D5EE0B78348AC17F7BF5B8CD333A860437705B65946B349311ACFF`**. The
promoted hash therefore described bytes that were **not** the packet, and the `CURRENT PACKET` block briefly
cited a hash no reader could reproduce from the file.

**Cause.** The Session polled the file, saw a complete-looking 32-line body with an `ADEQUATE` verdict, and
promoted on that observation — while the Planner's turn was still in flight and it went on to extend the
packet to 39 lines. **A file that looks finished is not evidence that its author has stopped writing.**

**Repair.** Promotion was re-done against the **final, stable** bytes: the hash was re-read twice with a
delay and confirmed unchanged, the revision line re-checked (`A4b2-NR-next-edge-r1`), and the `CURRENT
PACKET` block and this record were corrected to `7461AAA4…` / 39 lines. The packet itself was **not edited**
by the Session at any point, so the frozen artifact is the Planner's own final output.

**Why this is recorded rather than quietly fixed.** The packet is the frozen contract the next execution
binds to; a promotion citing a hash that does not match the artifact would let a later reader "verify" a
contract that never existed, and the failure mode is invisible — the draft was coherent and its adequacy
verdict was genuine. The correct discipline is to promote only after the authoring turn has **settled**,
which the Session now does.

## What this packet is for

`A4b2-NR-followup-r1` executed → **`O-INCONCLUSIVE`**. The followup closed the `P 00B9` gap by measurement
but exposed the real obstacle: the original Leg 1 reasoned over **380 of 2 340 executed PCs** — a 512-word
window covering ~16% of the executed program (`0x0000`–`0x0F28`). This packet builds the **PC-indexed
CFG/def-use slice over every feasible route** to the first doorbell descriptor and DMA write, across the
**full 3 881-word program**.

## The Advisor's Q2 lists are carried — verified present

The packet reproduces the seven **SUSPENDED** claims, the **CARRIES** list, and the **FORBIDDEN** list from
`docs/reviews/a4b2-nr-followup-advisor-ruling.md`, and states the 2 340-PC histogram may be used **only**
as a cross-check lower bound (executed ⊆ slice-covered) with the `PROVEN` bar remaining
**feasibility**-based. Verified at line 20 and the surrounding rows.

## The Q4 guardrail correction is carried — verified present

The packet instructs the **corrected** treatment, matching the Session's independent verification:

- `dsp_cpu.h:49` (`dsp_core_t.is_gp`) — never populated, **do not read**.
- `dsp.h:79` (`DspCoreState.is_gp`) — clobbered by `dsp_c_sync_to_vm` in traced runs, **do not read**.
- `dsp.h:105` (`DSPState.is_gp`) — **the canonical GP classifier**, distinct from `dsp->core.is_gp`.

It states plainly: *"The Advisor's Q4 line reference to `dsp.h:105` as clobbered identifies the wrong
field; `:105` is canonical and **must not be labeled unsafe**."* That is the correction the Planner found
and the Session verified, and it is carried explicitly rather than silently patched.

## Session command feasibility

| Check | Result |
|---|---|
| Run command | the packet uses the existing bounded runner at a pinned new exe identity — **runs as written** |
| `ctest` | targeted fixture arms plus full set — **available** |
| Write-scope files exist | `dsp_cpu.h`, `dsp.h`, `apu_watch.c/.h`, `tests/apu_watch_fixture_test.c`, `dsp_cpu.c`, `gp_ep.c` — **all present** |
| Scope discipline | `dsp_cpu.h`/`dsp.h` **comments only**; `apu_watch.c/.h` + fixture **`bad-output` removal and narrow comparator fixture only**; `dsp_cpu.c`/`gp_ep.c` **only if needed** for reversible GP-only env-gated trace, off by default |

**`A4b1-r4` is not touched.** The packet's scope names both headers as **newly authorized for comments
only**, consistent with the predecessor's rule that committed work is baseline rather than authorization.

## Session preparation that fed this packet

Before promotion the Session independently re-derived all seven of the Advisor's suspended items over the
full program and recorded them in `docs/reviews/a4b2-nr-next-edge-q2-preparation.md`. The headline result
is carried into the packet's brief: **`P 00DB` is not the sole descriptor builder** — it has **three**
callers and sibling `P 00EB` has two — but the callers write **pairwise disjoint** descriptor regions
(`x:[6..10]` doorbell, `x:[12..16]`, `x:[18..22]`), so the original *conclusion* survives on a reason the
original never stated. The remaining open question is the **reader**: whether a computed pointer can read
one of the other regions as the doorbell's descriptor.

Durable slice inputs are archived with the trace (`full_decode.txt`, `full_decode.json`,
`gpb9_trace.txt` in `logs/runs/20260927-134343-777-a4b2-nrf-b9-arch/`), and the decode is verified to
cover **100%** of the 2 340 executed PCs with **zero** executed PCs missing.

## Promotion

`A4b2-NR-next-edge-r1` at **`9E919866…D695D`** was **frozen and promoted into `CURRENT PACKET` in the same
step**, byte-identical with no revision (§5.3). For a discovery packet §5.8 makes the writing Planner's own
review the adequacy verdict, so **no second Planner was spawned** and no Muse shape preflight was repeated
— the mechanism is the one the Advisor mandated and no policy question arose.
