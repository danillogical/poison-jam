# `A4b2-NR-next-edge-followup-r1` — Session verification and promotion

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-nr-next-edge-followup.md`.
**Authoring Planner:** child `5416361e-11f8-4986-ae22-34a80d41c48d`, `codex/gpt-6-sol` @ `high`.
**Class:** discovery (§5.8). **Adequacy:** the **writing Planner's own** review, per §5.8 — **`ADEQUATE`**.

| Item | Value |
|---|---|
| Revision | **`A4b2-NR-next-edge-followup-r1`** |
| SHA-256 (frozen) | **`62A1BB38E4EA6FE4877EEDB6E37B27EFD01C88C50FFECC7924C7EF8F1226B3EE`** |
| Lines | 41 |

## Promotion discipline — the mid-write defect from the previous packet was NOT repeated

The previous packet was promoted **mid-write** (a 32-line draft hash against a 39-line final file). This
time the Session **waited for the authoring agent to settle** before promoting:

1. The Planner's `DONE` message arrived and the agent reported `finished`;
2. the file hash was read **three times over six seconds** and was **identical each time**
   (`62A1BB38…`);
3. the revision line was re-read (`A4b2-NR-next-edge-followup-r1`);
4. only then was the packet promoted, and the hash re-verified after writing the `CURRENT PACKET` block.

**No draft was ever promoted and the packet was never edited by the Session.**

## What this packet is for

`A4b2-NR-next-edge-r1` executed → **`O-INCONCLUSIVE`**, having found that the GP **loads a second program
into P-memory after the bootstrap** (`0x173`–`0x7FF` from zeros to code; `0x800`–`0xFFF` from `0xCACACA`
memset fill to code; 2 480 of 2 957 words changed; 99.4% of executions at bootstrap-zero/fill PCs). The
Advisor mandated **loader/epochs first, slice second**. This packet is that phased work.

## The Advisor's binding requirements — verified present

| Requirement | Where it lands | Verified |
|---|---|---|
| **N1** — rule a second GPRST bootstrap in/out **from existing logs, before any new run** | Exp. 1 (phase 1a): reconciles `boots=1` with the single `[GPBOOT] n=1` block in the archived decode2 log, and says *"do not repeat an exploratory run merely to ask N1 again"* | **present** |
| **N2** — epoch structure + **P-write watch** over image `I`, closing the transient gap | Exp. 2 (phase 1b): finishes the watch, GP-only via `DSPState.is_gp` through `core->opaque`, uncapped, ordinal + terminal reconciliation | **present** |
| **N3** — phase 2 **only** on phase-1-covered bytes | Exp. 4 (phase 2) opens *"only AFTER phase-1 byte coverage"*, and Exp. 3 requires a quiescent window **or** recorded per-epoch partition | **present** |
| **F1** — never cite doorbell byte-identity as path-independence | "Withdrawn / forbidden" bullet: *"instruction survival ≠ path survival"*, naming the five identical words | **present** |
| **F2** — never slice directly from the at-exchange snapshot | same bullet, explicit | **present** |
| **F3** — never fudge a single-image slice if PRAM never quiesces | same bullet: covered per-epoch slice or `O-INCONCLUSIVE` with reason | **present** |
| **W1–W4** — withdraw/re-scope list | "Withdrawn / forbidden" bullet, all four items | **present** |

**The Planner also carried the coverage caveat it found itself** — direct PRAM-write bypasses
(`dsp_c_bootstrap` scratch-to-PRAM, `sync_from_VM` copy, initial fill) must be shown unable to modify PRAM
inside the claimed interval **or** covered by narrow hooks, **failing closed** otherwise. That is exactly
the residual the Session's phase-1 work identified, and the packet makes it a phase-1b obligation rather
than an assumption.

## Session command feasibility

| Check | Result |
|---|---|
| Run command | `python -X utf8 scripts\run-jsrf.py --seconds 30 --profile exploratory --label a4b2-nr-next-edge-followup-watch` with the specified env — **runs as written** (this is the same shape used in every run this session) |
| Closure run | label `a4b2-nr-next-edge-followup-inert`, all diagnostic gates unset — **runs as written** |
| `RECOMP_APU_PWRITE_WATCH` launchable | not in `RETIRED_OVERRIDES` — **not refused** |
| Scope files exist | `dsp_cpu.c`, `apu_watch.c`, `apu_watch.h`, `tests/apu_watch_fixture_test.c`, `dsp_c.c`, `gp_ep.c`, game `CMakeLists.txt` — **all present** |
| `bad-output` still present for removal | **yes** — confirmed in the toolkit; bite archived and re-verified |

## Two staleness notes (recorded, not defects)

1. The packet's **Baseline/pins** line describes the P-write watch as *"uncommitted, in-progress"*. Since
   the Planner read the tree, the Session **committed and pushed** it (`c20fc75`), including the GP-only
   filter and the bypass-coverage hooks the Planner's own caveat asked for. The packet's **authorization**
   (finish/test/use the watch as new packet work) is unaffected and is what matters; the Session pins the
   exact revision at execution, as the packet requires.
2. The packet says the `is_gp` guardrail comments in `dsp_cpu.h`/`dsp.h` are *"already placed: no header
   rework"* — correct, and consistent with the Session's earlier work.

## Promotion

`A4b2-NR-next-edge-followup-r1` at **`62A1BB38…226B3EE`** was **frozen and promoted into `CURRENT PACKET`
in the same step**, byte-identical with no revision (§5.3). For a discovery packet §5.8 makes the writing
Planner's own review the adequacy verdict, so **no second Planner was spawned** and no Muse shape preflight
was repeated.
