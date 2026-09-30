# C1 finding: the terminal slot holds `0`, and TTD's own record says it held `0xFE000104`

**Status:** discovery result, produced by `docs/packets/c1-slot-write-attribution.md`'s
experiment. Recorded here so the next session starts from it rather than re-deriving.

**This is not yet an accepted packet outcome.** The packet is a draft, no Planner has
reviewed it, and no Acceptance reviewer has reproduced this. It is a measured result
with its controls, written down where the packet's evidence index will point.

## The measurement

Trace `logs/ttd/20260930-053811-458-ttd-aputrap/jsrf_recomp01.run` (8192 MB, the
horizon-reachable recording). Run `logs/runs/20260930-053722-314-v3-repro-check`
supplies the terminal event.

| Fact | Value | Source |
|---|---|---|
| Terminal event | `[ICALL] invalid target 0x00000000`, `return=0014982E` | the run's log |
| The call that faulted | `0x00149828 call dword ptr [0x1c4064]` — slot 65 | `inspect-jsrf.py disasm` |
| What that call read | **`0x00000000`** | the run's log |
| Terminal position P | sequence `38090827` | `ttd-terminal.py` |
| What TTD says slot 65 held | **`0xFE000104`** | `TTD.Memory` at P |
| When TTD last saw that value | position `116C62:1E2`, **earlier than P** | `ttd-terminal.py` |
| Install positive | `0xFE000104` written to slot 65 by `xbox_kernel_bridge_init` | `ttd-query.py` |
| Current run's dump, slot 65 | **`0x00000000`** | `inspect-jsrf.py memory` |
| Dump integrity | `CONTENT_MISMATCH` — read at actual VAs, do not shift | `check-dump-mapping.py` |

## What this means, stated as narrowly as the evidence allows

1. **The slot was correctly installed and then reverted.** TTD records the runtime's
   own install write of `0xFE000104` into slot 65, and it records the slot holding
   `0xFE000104` at position `116C62:1E2`. The failing call at P reads `0`.
   **Something between `116C62:1E2` and P set slot 65 back to zero.**

2. **The query did not find that writer, and its own controls say why it could not
   be sure it looked everywhere.** W11's W-b selected `UNATTRIBUTED WRITER`:

   ```
   FAIL  S6_value_consistency  the value at P is 0xFE000104 but the last recorded
                               write before P is 0x00000000fe000104 from IP
                               0x00007ff6b6c4e337
   ```

   That verdict is about **slot 64**, not slot 65: `ttd-terminal.py` defaults to the
   slot of the last kernel call (`ordinal 294`, slot 64) while the failing call used
   slot 65. Reading slot 65 at P gives `0xFE000104`, which *matches* the install
   write — so at position P the data model still shows the installed value, and the
   `0` the guest read is **not** what TTD's access record reports for that address.

3. **The dump is `CONTENT_MISMATCH`.** Per `AGENTS.md`, such a dump is still
   structurally readable and must be read at its actual guest VAs — which is what the
   `0x00000000` above is — but it is **not** admissible for an image-content claim,
   and a `CONTENT_MISMATCH` value must not be used as a decision input.

## The contradiction, and why it is the finding

**The guest read `0` from an address that TTD's own record says held `0xFE000104`.**
Those cannot both describe the same memory at the same instant unless one of these
holds, and they are now the packet's discriminating outcomes:

- **the read is of a different alias** — the guest's `[0x1C4064]` and the TTD query's
  `0x1D4064` are the same file region through different views, and a write through a
  mirror would change what the guest reads without appearing at the queried host
  address. W-c (the mirror positive) has never fired, so this is **unresolved**;
- **the write happened after P's last recorded access but before the read**, and TTD's
  `Last()` returns the last *recorded* access rather than the value at the read;
- **a kernel-mode or external write** changed it, which W11's exclusion (d) covers and
  the W-d control would settle;
- **the `CONTENT_MISMATCH` dump is displaced**, so its `0` is not the guest's `0`.

## What is NOT established

- **No writer is named.** `O-UNATTRIBUTED` is the closest row and it is selected by a
  comparison that used the wrong slot; the corrected comparison selects neither row.
- **No strict claim.** A TTD trace is not an archived strict run (W11's S8).
- **No claim about the title.** This is one slot at one moment.
- **The `CONTENT_MISMATCH` dump is not evidence of image content** and was used here
  only for the table's last row, marked as such.

## The next bounded steps, in order

1. **Fix the slot in the comparison.** `ttd-terminal.py` should default to the slot of
   the **failing call** (`return=0014982E` → `0x001C4064` → slot 65), not the slot of
   the last kernel call. That is a one-line defect with a control.
2. **Re-run W-b with the correct slot** and see whether it selects `ATTRIBUTED`,
   `UNATTRIBUTED`, or `READ PATH`.
3. **Settle the alias question** (W-c). The mirror positive must fire on a real trace
   or the alias hypothesis cannot be excluded — this is the Advisor's finding F5 and
   it remains open.
4. **Record a trace with `--max-file-mb 20480`** so S1 passes by construction.
