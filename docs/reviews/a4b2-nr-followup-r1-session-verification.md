# `A4b2-NR-followup-r1` — Session verification and promotion

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-nr-followup.md`.
**Authoring Planner:** child `e537374a-aaaf-49c7-9f3a-247bec80e784`, `codex/gpt-6-sol` @ `high`.
**Class:** discovery (§5.8). **Adequacy:** the **writing Planner's own** review, per §5.8 — **`ADEQUATE`**.

`docs/agent-workflow.md` §5.1.5(2) requires the Session to verify that every command runs before
submitting. Every row is a **Session observation**.

## Identity

| Item | Value |
|---|---|
| Revision | **`A4b2-NR-followup-r1`** |
| SHA-256 | **`886620CC6797FC89130B1F89310E6B41C92E809F32820A8133609FBEEC9CC5F5`** |
| Lines | 33 (long compact lines) |

## The three gaps, and the Planner's decisions — all verified present

**GAP 1 — the unresolved `P 00B9` computed read.** The packet adds exact env-gated GP-only
`RECOMP_APU_GP_B9_TRACE=1` instrumentation recording **one uncapped event per actual execution** at the
`dsp56k_read_memory` call, with bootstrap epoch, execution ordinal, `pc=00B9`, the **actual effective
address**, `r1`, `x:$0000`, the derived `0x80+x:$0000` cross-check and the returned value; classifying both
`0x1400..0x17ff` **and** the `0x0c00..0x0fff` alias. It requires:
- `address == r1` and exactly one matching read per executed `P 00B9`; a mismatch is **evidence failure**,
  not an absent mix read;
- an **independent uncapped** `P 00B9` execution counter at the instruction-entry choke point, plus a
  matching read-event counter, first-offending-address latch and status flag;
- **every** event streamed synchronously to a dedicated artifact — *"no first-N limit, no sampling, no
  bounded ring, no best-effort drop"* — with trace INVALID on any failure and **no inference of a negative**;
- ordinal contiguity, terminal counters equal to the parsed records, and no omitted epoch;
- a 30 s deadline **without** an exchange/terminal marker is `INCONCLUSIVE`.

**Assessed sound.** It closes the gap with a *complete record of the observed exchange*, and it explicitly
states its own limit — *"it does not prove an address bound for all possible future mailbox values"* — which
is exactly the honesty the predecessor's `INCONCLUSIVE` needed.

**GAP 2 — the six table words.** The packet decides **no decoder surgery**: it carries the Session's three
checks (no incoming branch target; six aligned table words; read by `movem p:(r2)+,x0` under `dor #$0006`),
notes the `op =` diagnostic occurred 6× only during decode and 0× in execution, and concludes they are
P-memory **data** for the distinct audio descriptor — *"`<UNDECODED>` is the instruction decoder rejecting
data, not a live-code hole."* It adds a falsifier: *"if the table is actually executed or altered, do not
reuse this conclusion."* **Assessed correct and appropriately bounded** — it declines to invent work, which
is what the Session's brief asked for.

**GAP 3 — the completeness bar.** The Planner **retains the predecessor's full-slice bar** and states the
targeted trace is **insufficient**: a PC-indexed CFG/def-use slice over the 371-word image covering *every
feasible route to the first doorbell descriptor and DMA write*, with explicit `UNKNOWN` for unresolved
indirect edges, reaching definitions for every DMA-register/descriptor write and guard, the two descriptor
paths partitioned, and `P 00B9`'s value traced through `P 00BA`/`00BD` including aliasing and loop-carried
state. It also gets the row logic right: an actual in-range read is **not** automatically `REFUTED` — a
concrete feasible causal chain to the **named doorbell** output or guard is required, and influence on the
*other* audio output is not enough. **Assessed sound**, and it is the conservative reading: 380 decoded
instructions make the slice tractable, so declining the shortcut costs little and buys a real proof.

## Command feasibility

| Check | Result |
|---|---|
| `run-jsrf.py --seconds 30 --profile exploratory --label …` | `--profile {strict,exploratory,fixture}` and `--seconds`/`--label` all exist — **runs as written** |
| Scope files exist | `dsp_cpu.c`, `gp_ep.c`, `apu_watch.c`, `apu_watch.h`, `tests/apu_watch_fixture_test.c`, game `CMakeLists.txt` — **all present** |
| `RECOMP_APU_GP_B9_TRACE` launchable | not in `RETIRED_OVERRIDES` (only `RECOMP_VBLANK`) — **not refused** |
| The choke point it instruments exists | `dsp56k_read_memory` (`dsp_cpu.c:1012`) is the single X/Y/P read entry — **verified in the predecessor's work** |

## Scope discipline

**Toolkit:** `src/apu/dsp/interp/dsp_cpu.c`, `src/apu/dsp/gp_ep.c`, `src/apu/apu_watch.c`,
`src/apu/apu_watch.h`, `tests/apu_watch_fixture_test.c`. **Game:** `CMakeLists.txt` (CTest registrations
only). The packet correctly treats the **committed** `r2` work as **baseline, not an authorization** — and
notes `src/apu/dsp/dsp.c` is baseline but **not** re-authorized. `A4b1-r4` is explicitly not touched.

## Promotion

`A4b2-NR-followup-r1` at **`886620CC…C5F5`** was **frozen and promoted into `CURRENT PACKET` in the same
step**, byte-identical with no revision (§5.3). For a discovery packet §5.8 makes the writing Planner's own
review the adequacy verdict, so **no second Planner was spawned** and the Muse shape preflight was **not**
repeated — the mechanism is the one the Advisor mandated and no policy question arose.
