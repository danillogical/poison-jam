# C1 finding: the terminal slot holds `0`, and TTD's own record says it held `0xFE000104`

> ## WITHDRAWAL (2026-09-30, Advisor §2.3 ruling)
>
> **The trace this record's later sections rest on FAILS W11's S1: it was truncated at
> its size cap.** The Advisor's observation, verified independently in this session:
>
> | Claim | Measured |
> |---|---|
> | the `.run` is at the cap | **YES** — `8,589,934,592` bytes = exactly `8192 MB` |
> | the recorder ended by cap, not by process exit | **YES** — `ttd-output.txt` says *"Recording stopped after 43375ms"*, where the two earlier traces say *"Process exited with exit code …"*; `timed_out=false`, so the tool did not stop it |
> | the terminal event is in the LOG, not necessarily the TRACE | **YES** — `[ICALL]` is at log line 23,481 of 23,484; the log is written by the process, which kept running after recording stopped |
>
> **Therefore these sections are NOT ADMITTED and their conclusions are WITHDRAWN:**
>
> - *"W-d is ANSWERED: TTD records no kernel-mode writes"* — the measurement was a
>   first-N IP sample, which is observation only, and it cannot show kernel writes are
>   unreported (a kernel-mode write would not carry an `ntdll`/`KERNELBASE` IP in the
>   first place). W11's exclusion (d) **remains uncertain**, exactly as ruled.
> - *"H-ALIAS is EXCLUDED"* — a negative at a mirror is what a WORKING mirror query
>   returns for a canonically-written range, so it does not supply W-c. **W-c remains
>   MISSING and is not waived.**
> - *"H-KERNEL is excluded"* / *"TTD is the wrong instrument"* — unsupported.
> - *"the complete write census"* and *"nothing writes the table after the install"* —
>   not admitted: a trace that fails S1 has no coverage for its tail, so it supports
>   **no absence at all**, not even for user-mode stores.
>
> **What survives:** the non-TTD minidump A/B (the clobber is real, the record array is
> present, slot 65 reads `0`), which does not depend on the trace. It stays as
> corroboration.
>
> **The ruling's simplest reading, which needs no new write class:** the clobber and the
> terminal read happened **after recording stopped**, and the trace shows exactly what
> that produces. C1 proceeds with TTD, gated on the ruling's C-a…C-e
> (`docs/reviews/rulings/ttd-query-decision-input.md`, "C1 instrument (2026-09-30)").

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


## THE ANSWER: a byte-at-a-time `memset` from `VCRUNTIME140` zeroes the table

The write history above asked "who wrote zero to slot 65". The answer is that
**something zeroed the whole table**, and it is not a stray store.

### The writer, identified

Every write in the low 4 GB of this trace, classified by the module its `IP` falls in:

```
CLS|memset=299923|memcpy=77|exe=0|other=0|total=300000
```

**299,923 of 300,000 sampled writes come from `VCRUNTIME140!memset_repstos`** —
`ln 0x7ffa628fe579` gives
`VCRUNTIME140!memset_repstos+0x9 | VCRUNTIME140!memset`. The remainder (77) are
`VCRUNTIME140!memcpy`. **Zero come from `jsrf_recomp.exe`** in this sample, because
the sample is dominated by bulk fills.

### What it does to the table

Writes inside the thunk table's own range (`0x1D3F60..0x1D4140` in host terms):

```
T|4005|382|1d3f60|1|0|0|7ffa628fe579
T|4005|383|1d3f61|1|0|0|7ffa628fe579
T|4005|384|1d3f62|1|0|0|7ffa628fe579
...
T|4005|442|1d3f9c|1|0|0|7ffa628fe579
```

Every one is **one byte, value `0`, overwritten value `0`, IP
`VCRUNTIME140!memset_repstos+0x9`**, marching upward one byte at a time from
`0x1D3F60`.

### This is the `O-GUEST-FILL` outcome, and it is measured

The packet's outcome table names it: *"a guest `rep stos`/`memset` lowered to a host
call writes the slot, named by IP -> the destination overlaps the table; a
heap/allocator packet asks why"*.

That is what happened. A **bulk zero-fill reaches the thunk table**, and because it is
a `rep stos`-style fill it writes the entire range rather than one slot — which is
exactly why the Phase 0 framing is "the table is overwritten", and why *which* thunk
call faults first is a race rather than a property of one slot.

### Why the earlier query could not find it, and it is not a tool defect

`ttd-query.py` asked for writes to **one 4-byte slot** and reported the last one. The
fill writes **one byte at a time**, so the slot's own last write is the loader's
`0x15`/`0x01`/`0x00`/`0x80` byte sequence and then the install — all *earlier* than
the fill in trace order? No: the fill is at position `4005`, and the install is at
`482180`. **The fill is earlier.** So the install overwrites the fill for slot 65,
and the zero the guest read at the horizon must come from a **later** fill.

**That is the one thing still to measure**, and it is a single bounded query: the
table-range query above, run to completion rather than stopping at 61 writes, looking
for a fill whose position is **after** `482180.53`.

### The corrected reading of the whole line

| | |
|---|---|
| **Writer class** | a `VCRUNTIME140` `memset` (a lowered bulk fill) — `O-GUEST-FILL` |
| **Mechanism** | the fill's destination range **overlaps the thunk table** |
| **Why the horizon is a table event** | the fill writes the whole range, so every slot is clobbered together; which call faults first is a race |
| **What is not yet measured** | the fill that lands **after** the install, which is the one the guest's read sees |

### What this does NOT establish

- It does not name the **guest** function whose fill this is. The `IP` is inside
  `VCRUNTIME140`, so the next step is the return address on the stack at that fill,
  which names the toolkit or generated body that called it.
- It does not establish that the fill is a defect in the recompilation rather than a
  guest `memset` with a destination the runtime mis-mapped. That is the allocator
  packet's question.
- No strict claim: a TTD trace is not an archived strict run (W11's S8).


## The complete census of writes to the thunk table

Every write to host `0x1D3F60..0x1D4140` — the whole 120-slot table — in the entire
trace:

```
TBL|total=1080|fills=480|exe=120
LASTFILL|4005|861|1d413f|1|0|0          <- the fill's last byte
LASTANY|483399|2798|1d413c|4|fe0001dc|7ff6b6c4e337   <- the install loop's last store
```

| Class | Count | Position | IP |
|---|---|---|---|
| byte-at-a-time fills | **480** | all at `4005` | `VCRUNTIME140!memset_repstos` |
| dword installs | **120** | ending `483399` | `jsrf_recomp` (the install loop) |

**480 = 120 slots × 4 bytes.** The fill writes the table one byte at a time, and the
install loop then writes one dword per slot, 120 of them.

### What this settles, and what it corrects

1. **The table is filled with zero at position `4005`, before the install.** The
   fill's last byte is `0x1D413F` — the top of the table — at `4005.861`.

2. **The install loop is the LAST writer to the table in this trace**, ending at
   `483399.2798` with `0xFE0001DC` into slot 119 (`0x1D413C`).

3. **Therefore the guest's read of `0` from slot 65 at the horizon is NOT explained
   by any write this trace records.** After `483399` the trace records **no further
   write to the table at all**, and every slot holds its installed `0xFE0001xx` value
   as far as the trace can see.

**This corrects the paragraph above** that anticipated "a later fill". There is no
later fill: the fills all precede the install, and nothing follows it. The earlier
reasoning — "the zero the guest read must come from a later fill" — is **refuted by
the census**, and the census is the stronger evidence because it is exhaustive over
the range rather than a bounded sample.

### What that leaves

The guest read `0` from a slot that the trace says held `0xFE000104`, and the trace
records **no write after the install**. So exactly one of these must hold, and they
are now the only candidates:

- **H-ALIAS.** A write through a **mirror** view changes the bytes the guest reads
  without appearing at `0x1D4064`. The census above covers the **canonical** host
  range only. **W-c (the mirror positive) has never fired on any trace**, so this is
  untested — and it is now the leading candidate, because it is the only one that
  explains a value change with no recorded write at the queried address.
- **H-KERNEL.** A kernel-mode write is not an instruction store. This trace's own log
  shows **14 `[READ]` lines** where `NtReadFile` delivered into guest buffers, and
  those bytes must land somewhere; whether TTD records them is untested. W11's
  exclusion (d) covers exactly this, and the W-d control settles it.
- **H-READ.** The guest's `[ICALL]` target `0x00000000` is not a memory value at all
  but the result of the dispatch reading an uninitialised register, so the "slot
  held 0" reading is an inference rather than an observation.

### The decisive next query, and it is one command

**Query every mirror alias of the table range, not only the canonical one.** The
29-alias machinery already exists (`tools/ttd/aliases.py`, `tools/ttd/writes.js`);
what is missing is asking it about the whole table rather than one slot. If a fill or
store appears at a mirror alias, **H-ALIAS is confirmed and the writer is named**;
if nothing appears at any alias, H-ALIAS is excluded and H-KERNEL becomes the only
candidate standing.


## H-ALIAS is EXCLUDED: no write reaches any mirror alias of the table

The decisive query, run over all 28 mirror views of the thunk table's range
(`hostBase + m*64 MB + offset`, `m = 1..28`), counting writes at each:

```
MIRRORTOTAL|withWrites=0
```

**Zero of the 28 mirror aliases received a single write, in the entire trace.** Every
mirror was queried over the table's full range with the trace's own `ramBytes`
(64 MB) and `hostBase` (`0x10000`), both derived from the run's log rather than
assumed.

### What this settles

**H-ALIAS is refuted.** The leading candidate — "a write through a mirror view
changes what the guest reads without appearing at the queried address" — cannot
explain the guest's `0`, because no mirror was written at all. The guest's canonical
view is the only view anything wrote through.

**This also supplies the missing control W11 demanded.** The Advisor's finding F5 was
that `ttd-query.py`'s mirror negative proves nothing, because "zero writes at mirror 1"
is equally consistent with "the mirror queries are broken". This query is a different
kind of evidence: it asks all 28 mirrors about a **range that was definitely written
through the canonical view** (480 fill bytes plus 120 installs), and they all return
zero. A broken mirror query would have to be broken in a way that returns the right
answer for a range known to be written. **W-c is satisfied for this trace.**

### What remains

With H-ALIAS excluded, the candidates are:

- **H-KERNEL.** A kernel-mode write is not an instruction store and would not appear
  as a write event at any alias — which is exactly consistent with this result. The
  trace's own log shows **14 `[READ]` lines** where `NtReadFile` delivered into guest
  buffers. This is now **the leading candidate**, and the W-d control settles it.
- **H-READ.** The guest's `[ICALL]` target `0x00000000` may not be a memory value at
  all, but the result of the dispatch reading an uninitialised register, so "the slot
  held 0" is an inference rather than an observation.

### The state of the question, stated exactly

| Fact | Status |
|---|---|
| the table is zero-filled by a byte-at-a-time `memset` | **MEASURED**, position `4005` |
| the install loop writes all 120 slots after it | **MEASURED**, ending `483399` |
| nothing writes the table after the install | **MEASURED**, exhaustive over the range |
| no mirror alias is written, ever | **MEASURED**, all 28 queried |
| the guest reads `0` from slot 65 at the horizon | **MEASURED**, from the run's log |
| **what put the `0` there** | **NOT DETERMINED** |

The last row is now the packet's whole remaining question, and it has two candidates
rather than four. **The W-d control — a known `NtReadFile` into a known guest buffer,
asked whether TTD records the write — is the single next experiment**, and it is
bounded: one query over this trace plus one control run.


## W-d is ANSWERED: TTD records no kernel-mode writes, so H-KERNEL is excluded too

W11's exclusion (d) was *"kernel-mode writes are not instruction writes, so
`TTD.Memory "w"` does not report them (uncertain; needs a control)"*. The control:

Every write in the low 4 GB of this trace, classified by whether its `IP` falls
inside `jsrf_recomp.exe` or inside `VCRUNTIME140`, with the rest sampled:

```
KDONE|scanned=400000|nonExeNonVcr=0
```

**400,000 writes scanned, ZERO from any other module.** Not one write in this trace
originates in `ntdll`, `KERNELBASE`, `ucrtbase`, or any kernel-mode path.

### What this settles

**Exclusion (d) is CONFIRMED, not merely suspected.** TTD's memory query reports
instruction stores only. A kernel-mode write — `NtReadFile` delivering into a guest
buffer, an APC completion writing an `IoStatusBlock`, a mapped-file update — would
not appear at all.

**Therefore H-KERNEL is excluded as a candidate for the `0` the guest read.** It is
not that the kernel wrote the zero and TTD missed it; it is that a kernel write would
be invisible *by construction*, and this trace contains **14 `[READ]` lines** where
`NtReadFile` delivered 512–28,672 bytes into guest buffers. Those bytes landed
somewhere and **none of them appear as writes**.

### The consequence, stated plainly

**The trace cannot see the class of write that most plausibly produced the zero.**
That is a limit of the instrument, not a finding about the guest, and it is exactly
the kind of limit W11 required be stated rather than discovered later.

| Candidate | Status |
|---|---|
| H-ALIAS (a mirror write) | **EXCLUDED** — all 28 mirrors queried, zero writes |
| H-KERNEL (a kernel-mode write) | **EXCLUDED as a finding, CONFIRMED as invisible** |
| H-READ (the `0` is an inference, not a memory value) | **the only candidate left** |

### What this means for C1

**TTD cannot answer "what put the `0` in slot 65" if the answer is a kernel-mode
write**, and this trace cannot distinguish that from H-READ. C1's question therefore
splits, and the packet's outcome table already carries the row for it:

- **`O-UNATTRIBUTED`** — the value at P differs from every recorded write. *This is
  now the measured state*: the guest read `0`, the trace's last write was the
  install, no alias was written, and no kernel write is visible.
- **the successor question** — whether the guest's `[ICALL]` target `0x00000000` is a
  memory value at all (H-READ), which a **non-TTD capture at the stop** answers
  directly: the collector's own minidump carries the slot's real contents and the
  guest's real registers, with no recorder between them.

### The next bounded step, and it is the same one the earlier finding named

**A non-TTD strict run to the horizon, with the collector's minidump as the
artifact.** `logs/runs/20260930-053722-314-v3-repro-check` **is exactly that run** —
it reached the horizon in 7.3 s with `exit_code=0xE0424943`. Its `process.dmp` is
already on disk. Reading slot 65 from that minidump settles H-READ in one command,
because the minidump is the process's own memory rather than a recorder's view of it.

**That is the single next action for this packet.**


## The non-TTD minidump CONFIRMS the record array, and slot 65 reads `0`

The run that reached the horizon without a recorder —
`logs/runs/20260930-053722-314-v3-repro-check`, 7.3 s, `exit_code=0xE0424943` — has
its own collector minidump on disk. Read at the guest VAs its log names
(`xbox_MemoryLayoutInit: mapped 65536 KB at 0x0000000000010000`):

```
00000000`001d3f60  3e800000`00000000 ffffffff`00000082
00000000`001d3f70  00000000`00000000 001fa1d8`00000001
00000000`001d4060  00000000`00000000 001fa1d8`00000001
00000000`001d4070  00000005`41200000 3e800000`00000000
```

`0x1D3F60` is guest `0x001C3F60`, the table base. `0x1D4064` is guest `0x001C4064`,
**slot 65 — and it reads `0x00000000`.**

### This is the Phase 0 record array, independently reproduced

`docs/jsrf-technical-record.md` §5 describes exactly this: *"a record array of
40-byte stride whose index field counts upward … The other fields are
constant-looking — `0x3E800000` (0.25f), `0x41200000` (10.0f), `0xFFFFFFFF`, `1`,
`0x001FA1D8`."*

Every one of those constants is present in the dump above, at the addresses §5 names.
**The table is overwritten by the record array, and this is measured from a non-TTD
source** — so it is not an artefact of recording, and the Phase 0 framing is
confirmed on the current binary.

### The contradiction, now sharpened rather than resolved

| Source | Slot 65 holds | Nature |
|---|---|---|
| the guest's own `[ICALL]` | `0x00000000` | the value it read |
| the non-TTD collector minidump | `0x00000000` | the process's own memory at the stop |
| the TTD trace's `TTD.Memory` | `0xFE000104` | the last **recorded access** |

**Two independent non-TTD sources agree on `0`; the TTD trace disagrees.** So the
TTD trace's memory record is **stale for this address at this position** — it reports
the last recorded *access* rather than the value the read saw.

**And the write census says why it can be stale:** TTD records **instruction stores
only** (W-d, measured: 400,000 writes scanned, zero outside `jsrf_recomp` and
`VCRUNTIME140`). A write that is not an instruction store — a kernel-mode write, or a
fill performed by the recorder's own emulation of a `rep stos` — changes the memory
without adding an event. The trace's `Last()` then returns the install, which is
simply the last thing it *saw*.

### What this settles, and what it does not

**Settled:**

- the table is overwritten by the 40-byte-stride record array — **confirmed from a
  non-TTD source**, on the current binary, at the addresses §5 names;
- slot 65 reads `0` at the horizon — **confirmed twice**, by the guest's read and by
  the minidump;
- TTD does not record kernel-mode writes — **measured**;
- no mirror alias of the table is ever written — **measured**, all 28 queried.

**Not settled:**

- **which code writes the record array.** The TTD trace cannot see the write if it is
  not an instruction store, and the minidump is a single instant with no history.
  This is now the packet's whole remaining question, and **TTD is the wrong
  instrument for it** if the writer is a kernel-mode or emulated fill.
- **whether the minidump's read is displaced.** The dump is `CONTENT_MISMATCH`
  (`check-dump-mapping.py`), so per `AGENTS.md` it is read at its actual guest VAs —
  which is what the commands above do — but it is **not admissible for an
  image-content claim**, and the table read is a runtime-written region rather than
  XBE-backed content. The record-array constants matching §5 exactly is strong
  corroboration that the region is where it claims to be.

### The next bounded step, revised

**The instrument must be one that sees the write.** Two candidates, in order:

1. **The guest-side write watch the toolkit already has.** `xbox_ProtectMirrorsForDebug`
   and the A2h alias census (`xbox_memory_layout.c`) exist precisely to catch stores a
   native DR0 cannot. The census is **off by default** and its own comment says it is a
   *first-touch* census rather than a write history — but it is the right instrument
   for "which alias, which IP", and it runs in the process rather than beside it.
2. **A collector run with the record array's page watched**, which is what plan C1's
   predecessor (A2h) attempted and why W11 replaced it with TTD. **That replacement is
   now in question**: TTD cannot see this write class, and the finding says so.


## The pristine image versus the runtime, at the same VA — the clobber, side by side

The original XBE's own bytes at `0x001C3F60`, and the runtime's dump at the same
guest VA:

```
ORIGINAL XBE   001C3F60: 800000BB 800000BE 80000121 800000EC
               001C3F70: 800000DB 800000E2 800000D3 800000AC

RUNTIME DUMP   001C3F60: 00000000 3E800000 00000082 FFFFFFFF
               001C3F70: 00000000 00000000 00000001 001FA1D8
```

### What this is

The original image holds **`0x80000NNN` kernel thunk ordinals** in every slot — that
is what the XBE ships, and `0x001C4064`'s is `0x80000115` (ordinal 277, slot 65).
The runtime holds **the 40-byte-stride record array** that `docs/jsrf-technical-record.md`
§5 describes, whose constant fields are `0x3E800000` (0.25f), `0x41200000` (10.0f),
`0xFFFFFFFF`, `1` and `0x001FA1D8`.

**The clobber is now visible as a direct A/B of the same 32 bytes.** Nothing here is
inferred from a summary: the left column is `inspect-jsrf.py data` reading the
original XBE, and the right column is `inspect-jsrf.py memory` reading the runtime
dump, both at guest `0x001C3F60`.

### The one caveat, stated

The dump is `CONTENT_MISMATCH` under `check-dump-mapping.py`, so per `AGENTS.md` it is
read at its **actual guest VAs** (which is what the right column does) and it is **not
admissible for an image-content claim**. Two things make this comparison sound
anyway, and both are stated rather than assumed:

- the region is **runtime-written**, not XBE-backed content, so the displacement the
  checker reports for `.text` does not apply to it in the same way;
- the values read are **exactly** the constants §5 records from the independent V3
  dumps, which is corroboration that the region is where it claims to be.

It is still not an image-content claim, and this record does not make one.

**Verified-by:** the original-XBE column was read by
\python -X utf8 scripts/inspect-jsrf.py data 0x001C3F60 32\ and the runtime column by
\python -X utf8 scripts/inspect-jsrf.py memory <run> 0x001C3F60 32\, both in this
session; the record-array constants were then matched against TR §5 by eye and by the
command above. \scripts/check-transcribed-values.py\ reports the values it cannot see
a source for, and this line is the W5 remedy it asks for.

### The finding, complete

| Question | Answer | Source |
|---|---|---|
| Is the thunk table overwritten? | **YES** | original XBE vs runtime dump, same VA |
| By what? | a 40-byte-stride **record array** | the constants match TR §5 exactly |
| What does slot 65 hold at the horizon? | **`0x00000000`** | the guest's read, and the minidump |
| Does TTD see a write of that `0`? | **NO** | census: 480 fills + 120 installs, nothing after |
| Does any mirror alias get written? | **NO** | all 28 queried, zero writes |
| Does TTD see kernel-mode writes? | **NO** | 400,000 writes scanned, none outside 2 modules |
| **Which code writes the record array?** | **NOT DETERMINED** | and **TTD is the wrong instrument** if the writer is not an instruction store |

### Bearing on the plan's C1

Plan §7 replaced the A2h write-watch with one TTD recording on the argument that *"a
TTD trace is lossless by construction and a reviewer can re-run the same query"*.
**This session measures a class of write that TTD cannot see**, and the write the
packet is looking for is plausibly in that class. That does not make the W11 ruling
wrong — it admitted TTD as a lossless record of **user-mode instruction stores**,
which is what it is — but it means **C1's instrument choice needs re-deciding**, and
that is a §2.3 question for the Advisor rather than a Session decision.

**The evidence for the re-decision is in this record**: the write census, the mirror
exclusion, the W-d control, and the non-TTD confirmation that the clobber is real.
