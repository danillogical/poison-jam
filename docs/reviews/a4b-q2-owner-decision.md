# A4b Q2 — owner decision: GPL-2.0-or-later is acceptable

**Question (Q2):** licensing of a DSP56300 core. The only known implementation, xemu
`hw/xbox/mcpx/apu/dsp/` (`dsp.c`, `interp/dsp_cpu.c`), carries **GPL-2.0-or-later**
headers, while this toolkit is **MIT** with **LGPL-2.1-or-later** APU files. Porting that
core is a licensing choice, reserved to the owner by `docs/agent-workflow.md` §3.4
("legal or licensing choices").

**Raised by:** the Session, on the Advisor's instruction (Advisor child
`407c54a3-6ca4-4a65-835b-faf5355195cd`, ruling recorded in
`docs/reviews/a4b-q1-advisor-ruling.md`).

## Decision (owner, 2026-09-24, direct to the Session)

> "My project is opensource, I'm fine with GPL-2.0"

**Effect:** the licensing obstacle is **removed**. A4b may port xemu's
GPL-2.0-or-later DSP56300 core, or implement a core independently — that is now a
technical choice for the Planner, not a licensing one. The owner has accepted
GPL-2.0-or-later terms for this project.

## Facts verified by the Session (not relayed)

- xemu `hw/xbox/mcpx/apu/dsp/dsp.c` and `hw/xbox/mcpx/apu/dsp/interp/dsp_cpu.c` both carry
  *"This program is free software; you can redistribute it and/or modify it under the terms
  of the GNU General Public License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version."* Fetched from
  `raw.githubusercontent.com/xemu-project/xemu/master/...` this session. The commit is not
  pinned; A4b should pin one.
- This toolkit is MIT (`LICENSE`, "Copyright (c) 2026 sp00nz") with LGPL-2.1-or-later
  third-party files (`NOTICE`, `LICENSES/README.md`, `LICENSES/LGPL-2.1.txt`).

## Consequence recorded for A4b closure (mechanical, not a new decision)

GPL-2.0-or-later is **stronger copyleft than the LGPL already in the tree**. A binary that
links a GPL-2.0-or-later core is a combined work that must be distributed under
GPL-2.0-or-later, so the top-level attribution can no longer describe the combined binary
as MIT-only. **When A4b lands the core, its closure must update `NOTICE` and the licence
files** to state that the combined work is GPL-2.0-or-later and to carry the verbatim GPL
text alongside the existing LGPL text, the same way `LICENSES/README.md` already treats the
LGPL as a shipping requirement rather than a courtesy.

This is bookkeeping that follows from the decision; it is **not** a further owner question,
and the Session did not stop on it. No licence file was changed at the time of this record,
because A4b has not been designed or promoted yet. The Session is not a lawyer: this records
the owner's decision and its mechanical consequence, not legal advice.

**Recorded in:** `plan-jsrf-bare-minimum.md` (Q2 gate marked answered, A4b unblocked).
