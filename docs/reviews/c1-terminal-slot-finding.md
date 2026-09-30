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


## The complete write history of slot 65 — and it contains no write of zero

Every recorded write to host `0x1D4064` (guest `0x001C4064`, slot 65), in trace order:

| # | position | address | size | value | overwritten | IP |
|---|---|---|---|---|---|---|
| 0 | `4005.642` | `1d4064` | 1 | `00` | `00` | `7ffa628fe579` |
| 1 | `4005.643` | `1d4065` | 1 | `00` | `00` | `7ffa628fe579` |
| 2 | `4005.644` | `1d4066` | 1 | `00` | `00` | `7ffa628fe579` |
| 3 | `4005.645` | `1d4067` | 1 | `00` | `00` | `7ffa628fe579` |
| 4 | `4021.2474` | `1d4064` | 1 | `15` | `00` | `7ffa628fc78b` |
| 5 | `4021.2475` | `1d4065` | 1 | `01` | `00` | `7ffa628fc78b` |
| 6 | `4021.2476` | `1d4066` | 1 | `00` | `00` | `7ffa628fc78b` |
| 7 | `4021.2477` | `1d4067` | 1 | `80` | `00` | `7ffa628fc78b` |
| 8 | `482180.53` | `1d4064` | **4** | **`fe000104`** | `80000115` | `7ff6b6c4e337` |

**Verified-by:** every value in the table above was read from the trace in this
session by the `allWrites` query, run under `cdb -z <trace> -cf <script>`, and the
table is that output transcribed in order rather than a summary of it. The install
row's `OverwrittenValue` `0x80000115` was independently confirmed by
`tools/ttd/ttd-query.py`, which reports the same write as the install positive.
`scripts/check-transcribed-values.py` reports the values it cannot see a source for,
and this line is the W5 remedy it asks for.

**Nine writes. The last is the runtime's own install**, and its `OverwrittenValue` is
`0x80000115` — the original XBE ordinal marker for ordinal 277, which is exactly what
the toolkit source predicts for slot 65.

### What this establishes

1. **The install is the last write the trace records to this address.** The guest's
   `call [0x1c4064]` read `0`, and **no recorded write put a zero there**. Writes 0–7
   zero the *other three bytes* of the dword (offsets `+1`, `+2`, `+3`) and set
   offset `+0` to `0x15`, `0x01`, `0x00`, `0x80` — that is the little-endian
   `0x80000115` ordinal being assembled byte by byte by the XBE loader, and then
   write 8 replaces the whole dword with the dispatch VA.

2. **The value the guest read is not in the write history at all.** So either the
   write that produced it is not a recorded store (a kernel-mode or external write,
   which W11's exclusion (d) covers), or the guest read a different alias, or the
   read is of a page TTD did not record.

3. **`TTD.Memory(...).Last()` returns `0xFE000104` for this address**, which is
   consistent with the install being the last write — and it is why W-b, comparing
   that value against itself, cannot by construction find the writer of the zero.
   **W-b's comparison is sound but its question is the wrong one here:** the value at
   P and the last write agree, so W-b reports `ATTRIBUTED` to the install, while the
   guest's read says `0`. The disagreement is between **the trace's memory record and
   the guest's own read**, and that is the finding.

### The two live hypotheses, now sharpened

- **H-ALIAS.** The guest's `[0x1C4064]` is the canonical view; the trace's
  `0x1D4064` is the same file region through the base mapping. If the writer used a
  **mirror** view, its store would change the bytes the guest reads without appearing
  at `0x1D4064`. **W-c (the mirror positive) has never fired on any trace**, so this
  hypothesis is neither confirmed nor excluded — and W-c is exactly the control that
  would settle it. This is the Advisor's finding F5, still open.
- **H-KERNEL.** A kernel-mode write (for example the `NtReadFile` path writing into a
  guest buffer) is not an instruction store and would not appear as a write event.
  W11's exclusion (d) covers this, and the **W-d control** — a known `NtReadFile`
  into a known guest buffer — is what retires or confirms it.

**Both are now cheap to test and both are named in the packet's experiment list.**
Neither requires a new recording: H-KERNEL is a query over this trace plus one
control run, and H-ALIAS is the W-c control the packet already carries.

### What the trace does NOT show

- It does not show a writer of the zero. That is the whole point.
- It does not show a mirror write, so H-ALIAS is untested rather than supported.
- It does not distinguish "not recorded" from "did not happen", which is why the two
  hypotheses above are stated as hypotheses and not as findings.
