# `A4b2-NR-epoch-slice-followup-r1` — Session verification and promotion

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-nr-epoch-slice-followup.md`.
**Authoring Planner:** child `f151b920-f96e-4fcb-b8fc-ea2e0fd71397`, `codex/gpt-6-sol` @ `high`.
**Class:** discovery (§5.8). **Adequacy:** the **writing Planner's own** review, per §5.8 — **`ADEQUATE`**.

| Item | Value |
|---|---|
| Revision | **`A4b2-NR-epoch-slice-followup-r1`** |
| SHA-256 (frozen) | **`03CE475DA2F48618BE93E6803F3168732A416DC38D00C4E649252BD87F6FEBA6`** |
| Lines | 34 |

## This is the LAST `L1` packet — terminality is carried

The Advisor ruled this is the **final** `L1` attempt **either way**
(`docs/reviews/a4b2-nr-epoch-slice-advisor-ruling.md`). The packet carries all three terminal rows:

| Row | Terminal action |
|---|---|
| **`O-REFUTED`** | Death branch: `A4b-VP-real-implementation` **and** `A4b-B3-register-resolution`; **no fifth `L1` packet** |
| **`O-TWO-LEG`** | `A4b2-r8` change revision; **no fifth `L1` packet** |
| **`O-INCONCLUSIVE`** | **Back to the Persistent Advisor** for a final scope/defer/retire decision — **not** another `L1` packet |

**`F-D` (no fifth `L1` attempt) is carried explicitly.** So the chain cannot auto-extend.

## The redesigned method is implemented

The packet specifies the **backward demand-driven slice** the Advisor mandated: seed separate worklists at
**each of the five field definitions and each write/trigger guard**, expand **only may-reaching
definitions**, keep a finite `(version, PC, location, context)` worklist solved conservatively to a fixed
point, and classify **every** frontier leaf as:

- **immediate / guest-written / modelled** (with source and coverage proof) → **CLOSED**;
- **stub-derived MIXBUF or `0xFFFFB3`** with a **concrete feasible causal chain** to a field or guard → **REFUTED**;
- **unresolved** feasibility/alias/byte/provenance → **UNKNOWN** with the precise node, address, edge and missing witness.

It states plainly that *"a mere read, observed invariance or unreachable trace does not refute/close"*, and
that forward whole-program enumeration is **forbidden** (`F-A`). The mixbin path is analysed **only at its
interface** — X-word/register writes actually read by worklist nodes — which is the Advisor's `Q2(a)` answer.

## The Advisor's required elements — verified present

| Requirement | Where | Verified |
|---|---|---|
| **Backward slice + frontier classification** | Exp. 3 | **present** |
| **Descriptor-disjointness PROVED alias-closed** | Exp. 4 — re-enumerates every caller of `P 00DB`/`P 00EB`, context-sensitive `r0`, the five store effective addresses, then **alias closure** including computed readers | **present** |
| **Interrupt scoping mandatory, incl. faults** | Exp. 2 — SR/mask, exhaustive table (Reset `0x00`, Stack Error `0x02`, Illegal `0x04`, Trap `0x08`), vector word versions, handlers, state effects; **"do not exclude fault vectors merely because they were not observed"** | **present** |
| **Entry-set proof** | Exp. 1 — reset entry, interrupt vectors, **bounded** second-image targets **into slice nodes**, not whole-program | **present** |
| **B3 enumeration over correct bytes** | Exp. 5 — all direct/indirect sites, both versioned images, **"without asserting a total of four"** | **present** |
| **Zero/fill decodability + wrong-version control** | Exp. 1 | **present** |
| **Second-image byte provenance if reached** | Exp. 1 — actual DMA source bytes, guest-staged → guest-written, unresolvable → UNKNOWN | **present** |
| **Executed counts as cross-check ONLY** | Exp. 1 and `F-C`; the ×1 counts are explicitly *"cross-check only, never a feasibility exclusion"* | **present** |
| **`F-A`–`F-D`, `F1`–`F3`, `W1`–`W4`** | "Limits, forbids and stops" | **all present** |

It also carries the two **twice-recorded traps**: the misleading operand-scan zero-MIXBUF result (Exp. 3),
and the `DspCoreState.is_gp` vs `DSPState.is_gp` distinction by **struct-qualified name** rather than line
number (`F` block).

## Session command feasibility

| Check | Result |
|---|---|
| Run command | `python -X utf8 scripts\run-jsrf.py --seconds 30 --profile exploratory --label a4b2-nr-epoch-slice` with the specified env — **runs as written** (same shape used throughout this session) |
| Closure run | one fresh absent-gate baseline at the final identity — **runs as written** |
| Scope files exist | `dsp_cpu.c`, `dsp.c`, `dsp_dma.c`, `dsp_c.c`, `gp_ep.c`, `apu_watch.c`, `tests/apu_watch_fixture_test.c`, game `CMakeLists.txt` — **all present** |
| Archived inputs referenced | `bootstrap_state_decode.*`, `cfg_nodes.json`, `cfg_edges.json`, `slice_doorbell.json`, `unknown.json` in `logs/runs/20260927-134343-777-a4b2-nrf-b9-arch/` — **all present** |

## Staleness note — one place the packet is now out of date, in the safe direction

Exp. 4 lists the extra builder-shaped blocks `P 00F0..00F8`, `0138..0141`, `0149..0150` as
**"prepared-but-unclosed"**. Since the Planner read the tree, the Session **closed two of the three**:

- **`P 0133`–`0143` and `P 0144`–`0154` are statically unreachable** — verified three ways: **no incoming
  branch/call target** anywhere in the program, their **predecessor is `rts`** (so no fall-through), and they
  execute **0 times**.
- `P 00F0..00F8` is builder B's body, called from `P 000E` (`r0=0x18` → `x:[24..28]`) and `P 0019`
  (`r0=0x1E` → `x:[30..34]`) — neither in range.
- **All 39 direct X writes** in image `I` were enumerated; **none** has an address in `x:[6..10]`.

**So the only writer that can reach the doorbell's `x:[6..10]` is the `P 0007` call — its own
construction.** The packet's demand that alias-closure be *proven* still stands and is still the right
requirement; the Session's preparation simply supplies more of the proof than the packet assumes. Recorded
in `docs/reviews/a4b2-nr-epoch-slice-disjointness-preparation.md` §4.

## Promotion

`A4b2-NR-epoch-slice-followup-r1` at **`03CE475D…FEBA6`** was **frozen and promoted into `CURRENT PACKET`
in the same step**, byte-identical with no revision (§5.3). Promotion discipline: the authoring agent
reported **finished**, the hash was read **three times over nine seconds** and was **identical each time**
and matched the Planner's own reported hash, and the packet was **never edited by the Session**. For a
discovery packet §5.8 makes the writing Planner's own review the adequacy verdict, so **no second Planner
was spawned**.
