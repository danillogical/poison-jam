# A4b revision history (non-authoritative)

Per `docs/agent-workflow.md` §5.7. Not reviewed for accuracy; loses to the frozen contract
on any conflict. It exists so the packet carries no revision narrative.

## A4b-r1 — first revision

Written by Planner child `fa3b28a7-8f76-491e-a359-f523a7a45b7b` (route
`claude` / `claude-opus-5-5` @ `high`) against the frozen brief built from the accepted
`A4a-r2` outcome row `O-6`. Class: **change**. Original SHA-256
`99CA97BE57C80D208402E846D2D81F350AB3A884FD1231935DC1C556FD162995`.

The Planner worked under the §5.1.4 checkpoint regime. Its own planning record, moved here
from the packet before freezing:

- Checkpoint 20 (sketch 1 written late, at call 27): change class; unknowns SGE addressing,
  pin, GP inputs, no oracle.
- Checkpoint 40 (at call 45): sketch 2 + yield (SGE masking is wrong, not merely
  VA-vs-PA; no hardware watch facility → witness moved into the DMA path; U5 frame-gate
  found) sent to the Advisor via Session relay (direct send refused by the harness).
  Advisor replied: continue, seven binding constraints — all adopted (U5 ordering row;
  single fail-closed VA translation; three-part witness; input classes; fixture labelled
  non-acceptance and inherited trust stated; landing-only as a pre-frozen row with its own
  criteria; licence as a criterion).
- Stopped at 60 calls to write; no 60-call Advisor consult needed because the packet was
  written before it.

### What the Planner found that changed the design

- **The SGE address path was wrong as specified.** The brief implied SGE entries could be
  resolved the way xemu does (`ldl_le_phys(addr & 0x03FFFFFF)`). The Planner established
  that the `0x80000000` window is *separate storage* in this kernel
  (`xbox_memory_layout.c:1539-1543`), so that masking would read `0x003CC000` instead of
  `G = 0x803CC000` — the wrong bytes. The Session reproduced this independently and the
  Advisor ruled the fix a documented runtime adaptation (constraint 2).
- **U5, the frame gate (unforecast).** The APU frame thread runs `se_frame` — hence any
  DSP — only when `SECTL.XCNTMODE != OFF` and `FECTL` is not trapped/halted, and the test
  tone is off. The Advisor checked R1 and found the gate open only *after* GPRST=3, so the
  packet must not assume the GP runs at GPRST time. That ordering became `AC-RUN`.
- **No hardware watchpoint facility exists**, so the "who wrote the 0" witness moved into
  the GP DMA write path. The Advisor then required a three-part witness rather than the
  before/after sample alone, because the guest CPU runs on other host threads.

## Session mechanical fill-in (before adequacy review)

Per §5.1.2 the Session fills commands, paths, hashes and environment, and verifies every
command runs. Done on `A4b-r1` before it went to adequacy review:

- **Pin record written:** `docs/reviews/a4b-xemu-pin.md` — xemu commit
  `67cc79e663038d1f55448c0f566b37dde016adf6`, with per-file byte counts, SHA-256 and the
  licence read from each file's own header.
- **The four "inferred" facts were resolved from the pinned source**, as the packet's
  advisory 1 asked: the bootstrapping GPRST transition (`proc_rst_write`), the GP-enable
  preference name (`g_config.audio.use_dsp_jit`), the bootstrap's scratch read
  (`0x800 * 4` bytes from scratch address 0, masked `0xFFFFFF`), and the GP frame path.
  Recorded in the pin record; **not** added to criteria, per that advisory.
- **Checkpoint-40 ruling recorded verbatim** in `docs/reviews/a4b-planning-rulings.md`,
  which the packet cites, with the Session's reproduction of its load-bearing claims.
- **Every cited line number verified** against the working trees: the AC-NOCPU sites
  (`recomp_0005.c:6746`, `:6524`, `:8088`; `recomp_0000.c:135283`, `:135406`, `:135871` —
  and the generated-store pattern yields exactly those four), the ten `AC-PIO` sites, the
  `apu_core.c:454-468` gate, `apu_core.c:602-619` / `:626-634` routing, the `apu_regs.h`
  FECTL/SECTL masks, and the G4 pre-check (`recomp_0005.c:6748`/`:6751`).
- **Gate arithmetic re-derived:** `0x100F & 0xE0 = 0` (FREE_RUNNING), `0xF & 0x18 = 0x8`
  (not OFF).
- **Readiness tools re-verified to run** at baseline: `build-jsrf.py`, `ctest` (12 tests),
  `run-jsrf.py`, `check-run-profile.py`, `check-dump-mapping.py`, `inspect-jsrf.py`.
- **A licence correction recorded.** The brief described the core as GPL-2.0-or-later and
  `gp_ep.c` as LGPL-2.1-or-later. The pin record shows the split is finer: the DMA layer
  (`dsp_dma.c`, `dsp_dma.h`, `dsp_dma_regs.h`) is **LGPL-2.1-or-later** too, like the APU
  files already in the tree, while `dsp.c`/`dsp_c.c`/`interp/*` are GPL-2.0-or-later.
  `AC-LIC` must therefore list ported files by their actual licence. The combined-work
  statement is unchanged.
- The non-operative "Planning record" section was moved out of the packet to this file
  (§5.7), and the Baseline line was completed with the promotion commit.
