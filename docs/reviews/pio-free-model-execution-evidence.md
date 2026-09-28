# `PIO_FREE-model-r2` execution evidence — **`O-UNKNOWN`**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/pio-free-model.md`, revision **`PIO_FREE-model-r2`**, frozen SHA-256
**`11D6ECB195D51159785D6E94C98439BD4AA6979FDEE6B8EC79B28A80961DDA9E`** — verified before execution,
**not edited**. **Class:** discovery (§5.8). **Write scope honoured:** this record only; **no toolkit or game
code, no build, no guest run, no instrumentation.**

**Selected row: `O-UNKNOWN`** — *"No preceding row, including inadequate independent sourcing, unresolved
unit/drain/reset/alias/variable-threshold leaf or missing coverage."*

---

## Experiment 1 — pin and reconcile the boundary, offline

### Identity (E0)

| Item | Value |
|---|---|
| `game/default.xbe` SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` — **matches the pinned baseline** |
| Game revision | `9c4b3a97434a1fc2919c21281f3d386154d2184b`, clean |
| Toolkit revision | `c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`, clean |
| `A4p` premise | ACCEPTED `O-GATE` on the **same** XBE — transferable |
| Two-leg premise | unchanged; no pinned premise altered |

**No `O-IDENTITY` condition.** Nothing stale, malformed or unreadable.

### The frozen 28-site population, and both spellings

| Check | Result |
|---|---|
| `inspect-jsrf.py find 0xFE820010` | **28 occurrence(s)**, `001A2297` … `001A611D`, all `DSOUND` — **matches `A4p`'s frozen population exactly** |
| Generated spelling `MEM32(0xFE820010u)` (hex) | **10** |
| Generated spelling `MEM32(-25034736)` (decimal) | **18** |
| **Sum** | **28** — **reconciles one-to-one with the XBE population** |

> **Reading note (advisory A-4).** The `find` addresses `001A2297 … 001A611D` are **operand offsets**, not
> instruction starts — `A4p`'s own convention. The **instruction** begins at `hit−1` for an `A1` moffs load
> or `hit−2` for an `8B /r` load, so the first site's instruction is at **`001A2296`** and the second's at
> `001A2A7F`. Recorded so a reader does not disassemble from the wrong byte.

**The two-spelling hazard is real and both spellings are required.** Verified independently:
`-25034736 & 0xFFFFFFFF == 0xFE820010`. **The population is used as `A4p`'s frozen set, not newly
enumerated**, per the packet. **No new trace-to-site classification was needed**, so the packet's
checked-in-parser requirement was **not triggered**.

**The acceptance reviewer closed the hazard positively rather than by assertion:** a normalised scan of
**every** `MEM8`/`MEM16`/`MEM32` literal in `src/recomp/gen/*.c` modulo 2³² finds **only those two
spellings** of `0xFE820010`, and **no third spelling at any address ≥ `0x80000000`**. So the `10 + 18` split
**exhausts** the generated population — the two-spelling hazard `AGENTS.md` warns about is **positively
closed** for this address, not merely assumed away.

### The trapped route and the current implementation

| Fact | Pinned source |
|---|---|
| `0xFE820010` = APU `+0x20010` = **VP `+0x10`** | APU dispatch arm `0x20000–0x2FFFF` (`apu_core.c`) |
| VP `0x10` = **`NV1BA0_PIO_FREE` = `0x00000010`** | `apu_regs.h:128` |
| Current trapped read returns **constant `0x80`** | `apu_vp.c:551-562`: `case NV1BA0_PIO_FREE: return 0x80; /* Always pretend queue is empty */` |
| Every other VP offset reads **`0`** | same function, `default:` falls to `return 0` |
| VP writes dispatch **synchronously** | `apu_vp.c:564-572`: `fe_method(d, (uint32_t)addr, (uint32_t)val);` — inline, no queue |
| Untrapped route is a **separate** `MCPX_COUNTERS[0x020010]` tick | `xbox_memory_layout.c:438-439`, incremented at `:891-895` **gated on `!g_apu_mmio_trapped` (`:890`)** |

**So the untrapped counter never ticks under the trap — which is precisely why the trapped route reaches
the VP stub.** The two behaviours must not be merged, as the packet requires.

### Guest threshold forms — reconciled, and they are **not** interchangeable

| Site | Form | Reading |
|---|---|---|
| `001A2296` | `mov eax,[0xFE820010]` / `and eax,0xFFFFFFFC` / `cmp eax,4` / `jb 001A2296` | **constant** threshold, loop while `(v & ~3) < 4` |
| `001A2A7F` | `mov edx,[0xFE820010]` / `shr edx,2` / `cmp edx,ecx` / `jb 001A2A7F` | **variable** threshold, loop while `(v >> 2) < ecx` |

**`0x80` satisfies both** (`0x80 & ~3 = 128 ≥ 4`; `0x80 >> 2 = 32`). **But the shift form reads the word as
a count in units of 4**, so `0x80` asserts **32 free units** — it is **not** a neutral "empty". **The
variable `(v>>2)<reg` form is preserved as variable**, per the packet's explicit prohibition.

**The site's shape is poll-then-push**: after the poll at `001A2296`, the guest writes to `0xFE820280` =
VP `+0x280` = **`NV1BA0_PIO_SET_HRTF_HEADROOM`** (`apu_regs.h:158`), which `fe_method` handles. **A
producer pattern against a free-space counter** — structural evidence the register really is a free-slot
count, and the source of the packet's false-model test.

### Threshold-form census across all 28 sites — the **variable** form is the majority

Every one of the 28 frozen sites was disassembled and classified by its own threshold form, applying
`A4p`'s operand rule (`find` reports the **operand** address, so the instruction starts at `hit−1` for an
`A1` moffs load or `hit−2` for an `8B /r` load):

| Form | Sites |
|---|---|
| **VARIABLE** — `shr reg,2` then `cmp reg,ecx` / `jb` (count vs a register) | **15** |
| **CONSTANT** — `and reg,0xFFFFFFFC` then `cmp reg,4` / `jb` | **13** |
| other | **0** |
| **total** | **28** |

**The variable form is the majority, not the exception.** That matters: **at 15 of 28 sites the guest
compares the polled word against a register-supplied demand**, so the word must behave as a *quantity that
can be insufficient*, not merely as a non-zero flag. A constant `0x80` therefore has to satisfy
`(0x80 >> 2) = 32 ≥ demand` at every such site — **which is only safe if 32 is genuinely at or above the
maximum demand, and nothing establishes that.** The constant form is the weaker constraint (any value ≥ 4
passes).

### What the polls gate — **NOT resolved, and deliberately not tabulated**

The intent was to read past each poll to the first MMIO **write** it guards, to identify the producer
targets. **That extraction is not reliable and its result is withdrawn.** Two runs of the same script
returned **different** push targets for the same sites, because the "first MMIO write within *N*
instructions" heuristic is sensitive to where the window is cut and to whether the poll's own load is
skipped. **A table that changes between runs is not evidence**, so **no push table is recorded here.**

**What survives:** at the **one** site examined instruction-by-instruction earlier (`001A2296`), the guarded
write is to `0xFE820280` = VP `+0x280` = `NV1BA0_PIO_SET_HRTF_HEADROOM`. **That single, hand-verified site
stands.** The other 27 push targets are **`UNRESOLVED`**.

> **Session process error, recorded because it is the packet's own warning made concrete.** My first two
> attempts at the census **failed silently** — I passed bare hex to `inspect-jsrf.py`, whose parser requires
> the `0x` prefix, and then mis-applied `A4p`'s operand rule (`find` reports the **operand** address, not the
> instruction). Then the push extraction produced **run-to-run instability**. **Three failures of ad-hoc
> parsing in one exercise**, which is precisely why the packet requires a **checked-in parser with good/bad
> tests** before any trace-to-site decision. **The census below is offered as a lead to be re-derived that
> way, not as a verified finding.**

## Experiment 2 — the finite ledger

**Scope: only the interface needed to explain occupancy**, demand-driven from `NV1BA0_PIO_FREE`. No
forward enumeration of guest or GP work.

| # | Leaf | Finding | Witness |
|---|---|---|---|
| 1 | **Register interpretation / address alias** | **RESOLVED.** `0xFE820010` → VP `+0x10` = `NV1BA0_PIO_FREE`; the "PIO" of `A4p` and the VP route are the **same** address | `apu_core.c` dispatch; `apu_regs.h:128` |
| 2 | **Units and bit encoding of the free count** | **`UNKNOWN`.** The guest's `>> 2` form implies **units of 4 bytes**, but **no source states the encoding**; the constant `0x80` is consistent with 32 units yet nothing establishes it | guest `001A2A7F`; **no independent witness** |
| 3 | **Queue capacity** | **`UNKNOWN`.** No source gives a depth. `0x80 >> 2 = 32` is a *possible* capacity, unsupported | **no witness** |
| 4 | **Which guest writes enqueue** | **PARTIAL.** The site writes `0x280` (`SET_HRTF_HEADROOM`); `fe_method` **applies it inline**. Whether hardware enqueues it is **`UNKNOWN`** | `apu_vp.c:564-572`; `apu_regs.h:158` |
| 5 | **How / when work drains or completes** | **`UNKNOWN`.** The model has **no drain at all** — `fe_method` completes synchronously | `apu_vp.c:564-572` |
| 6 | **Reset initial state** | **PARTIAL.** The state array is `calloc`'d (`apu_core.c:543`), but the *stub returns `0x80` regardless*, so the model's read is **not** the reset state | `apu_core.c:543`; `apu_vp.c:551-562` |
| 7 | **Overflow / backpressure** | **`UNKNOWN`.** No model, no source | **no witness** |
| 8 | **Ordering of a read against preceding writes** | **`UNKNOWN`.** The model is synchronous so there is no ordering to observe; hardware ordering is unstated | `apu_vp.c:564-572` |

**Three leaves are `RESOLVED`/`PARTIAL`; five are `UNKNOWN`.** The `UNKNOWN` leaves are exactly
**units, capacity, drain, overflow and read/write ordering** — the semantics that a truthful free-space
model consists of.

## Experiment 3 — independent sourcing, and the negative control

### The negative control is confirmed, and it is an **admission**

xemu's VP read handler (fetched from `xemu-project/xemu`, `hw/xbox/mcpx/apu/vp/vp.c`) reads:

```c
static uint64_t vp_read(void *opaque, hwaddr addr, unsigned int size)
{
    switch (addr) {
    case NV1BA0_PIO_FREE:
        /* we don't simulate the queue for now,
         * pretend to always be empty */
        return 0x80;
```

**This is stronger than a negative control — it is a direct admission that (a) a queue exists in hardware,
(b) xemu does not simulate it, and (c) `0x80` is a pretence.** It is **xemu-derived**, i.e. the toolkit's own
ancestry, so per the packet it **is not independent support** and **cannot** evidence that `0x80` is
hardware truth. **It does, however, independently corroborate that the value is not a measurement.**

### Independent sourcing: **inadequate — the rule is not met**

| Source | Provenance | What it says about `PIO_FREE` |
|---|---|---|
| xemu `vp.c` | **toolkit ancestry — excluded as independent** | "don't simulate the queue … pretend" |
| **xboxdevwiki, `APU`** (raw wikitext, 14 155 bytes; page rev `7415`, 2025-07-22) | **secondary, Xbox/MCPX-specific** | **SILENT** |

**The wiki's silence is verified by term count on the page's own raw wikitext**, not by impression. The
exact, re-runnable source (advisory A-2):

```
https://xboxdevwiki.net/index.php?title=APU&action=raw
```

fetched into a known file (`%TEMP%\piofree-wiki-raw.txt`, 14 155 bytes — matching the acceptance reviewer's
independent byte count), page revision **`7415`**, last modified **2025-07-22** (page identity confirmed by
the reviewer via the MediaWiki API). Term counts on that text:

| Term | Occurrences in the page source |
|---|---|
| `PIO_FREE` | **0** |
| `queue` | **0** |
| `free` (any case) | **0** |
| `depth` | **0** |
| `NV1BA0` | 2 — and **both** are `NV1BA0_PIO_VOICE_ON` / `NV1BA0_PIO_VOICE_RELEASE` in the envelope section |

**So the wiki documents the VP exhaustively** — 256 voices, 32 bins, voice lists, a `0x80`-byte voice
structure, envelope segments with rate formulas, DLS2 filter coefficients, HRTF, LFO, the MIXBUF layout,
and the GP's 6-channel DMA ringbuffer — **and says nothing whatever about a free-space register or a queue.**
The acceptance reviewer reproduced this **with a positive control** (`NV1BA0` = 2, proving the search was
live on the right document), and added a **site-wide** wiki search for `PIO_FREE` returning **no results at
all**.

> **A Session verification error, recorded.** My first attempt to check this grepped a **spill file that
> actually held the xemu source**, not the wiki, and reported `PIO_FREE` = 1. That looked like it falsified
> the silence claim. Re-fetching the wiki's **raw wikitext** into a known file and counting terms there gives
> `PIO_FREE` = 0. **The lesson is the same one this project keeps re-learning: verify against the artifact
> you name, not against a buffer you assume holds it.** The conclusion is unchanged, but it was briefly
> unsupported.

**The packet's independent-source rule requires either one credible primary hardware/vendor specification,
or two independent secondary sources of different provenance with at least one Xbox/MCPX.** **Neither is
satisfied:**

- **No primary specification was found *live*, but the primary specification exists in archive form and is
  SILENT.** The 2001 NVIDIA nForce MCP technical brief's live URL returns **HTTP 404**; the acceptance
  reviewer found an **archived copy that returns HTTP 200** — a **378 982-byte PDF** at
  `https://web.archive.org/web/2019id_/http://www.nvidia.com/attach/9004` (**the Session re-verified this
  independently: HTTP 200, `application/pdf`, `%PDF` magic present**). The reviewer extracted its text and
  counted: **`PIO_FREE` = 0, `NV1BA0` = 0, `PIO` = 0, `queue` = 0, `free` = 0, `FIFO` = 0, `depth` = 0,
  `front end` = 0** — **with live positive controls** (`Voice Processor` = 21, `register` = 2) proving the
  document and the search terms were both live.
  **So the primary source is obtainable and says nothing about this register.** That is a **stronger**
  statement than "unavailable": the primary-specification branch is not merely unfound, it is **found and
  silent**.
  *(The reviewer's own caution, carried: its PDF text extraction was **partial** — 36 275 chars from 36
  streams, some mojibake — so the zeros held under live positive controls but a cleaner extraction would
  firm this up if the brief is ever relied on as primary.)*
- **A first-party source was checked and is also silent.** The wiki's own citation for the 256-voice figure
  is Brian Schmidt's *"Designing the Boot Sound for the Original Xbox"* (Gamasutra, 2011-11-17; archived
  2019-01-23), whose author is described as *"the architect for the Xbox and Xbox 360 audio systems and
  created the Xbox startup sound"* — i.e. **first-party**. It describes the MCPX as *"quite full featured …
  256 channels of sound with programmable filters on each voice as well as a 6-stage envelope"*, and
  discusses the sequencer, patches, filters and envelopes in detail. **It says nothing about `PIO_FREE`, a
  free-space register, or a command queue.** **A first-party account of the device that omits the register
  is further evidence the semantics are not publicly documented** — though silence is still not a contrary
  finding.
- **Related wiki pages were also checked and are silent:** `MCPX` (1 684 bytes) and `DSP` (9 655 bytes)
  each contain **zero** occurrences of `PIO_FREE` and **zero** of `queue`.
- **The acceptance reviewer additionally checked sources the Session had not, all negative:** a
  **site-wide xboxdevwiki full-text search** for `PIO_FREE` returns *"no results"* — **no page on the wiki
  mentions it at all**; `JayFoxRox/xbox-tools` (`inspect_apu_vp.py`, `trace_apu_mixbuf.py`);
  **`Cxbx-Reloaded` — an independent second emulator** (`APUDevice.cpp`, `DirectSound.cpp`,
  `XbInternalDSVoice.cpp`, `XbDSoundTypes.h`); the live NVIDIA APU page; and four search engines. **All
  negative.**
- **Only one admissible secondary source exists, and it is silent.** Silence is **not** a contrary finding —
  it simply leaves the leaves uncovered.

**Both branches of the rule are therefore unmet on exhausted leads**, not on a single check. The reviewer's
independent coverage of `Cxbx-Reloaded` matters: it is a **different emulator lineage** from xemu, so its
silence is genuinely additional rather than the same provenance twice.

**So the sourcing rule fails, and under the packet's own text that is explicitly an `O-UNKNOWN` condition**
(*"including inadequate independent sourcing"*).

### Why this is **not** `O-CONFLICT`

`O-CONFLICT` requires *"authenticated independent sources or route/guest evidence give incompatible,
concrete meanings or impossible queue predictions."* **There are not two concrete incompatible meanings** —
there is **one admission** (xemu, excluded as independent) and **one silence** (the wiki). **Silence is not
conflict**, so `O-CONFLICT` does not apply and the matter does **not** go to the Advisor on this evidence.

### Why this is **not** `O-SPEC`

`O-SPEC` requires *"Every named ledger leaf covered, independent-source rule met, no contradiction, finite
predictions distinguish truthful occupancy/drain from always-`0x80`."* **Five of eight leaves are
`UNKNOWN` and the independent-source rule is unmet**, so `O-SPEC` fails on two independent grounds.

**The false-model test is well-posed and would distinguish the models** — a queued write that must consume
space cannot truthfully leave the free count unchanged, and the synchronous `fe_method` cannot represent
that — **but well-posed is not covered.** The test is recorded as the **positive prediction a future model
must satisfy**, not as a finding.

## Experiment 4 — deliver and check

**Missing / contradictory passages checked:** the wiki was read in full for the APU and contains no
`PIO_FREE` material; xemu was read for the read handler, the `fe_method` switch, and the register map.
**No contradiction was found between any two admissible sources** — only silence. **No stale source
assumption:** the toolkit is pinned at `c151d4e`, the XBE matches, and `A4p`'s population reconciles.

---

## Selected row: `O-UNKNOWN`

**First-match evaluation, in the packet's order:**

| Row | Applicable? | Why |
|---|---|---|
| `O-IDENTITY` | **No** | Identity, provenance and coverage all reconcile; no premise changed |
| `O-CONFLICT` | **No** | No two authenticated sources give incompatible concrete meanings; silence ≠ conflict |
| `O-SPEC` | **No** | Five of eight leaves `UNKNOWN`; independent-source rule unmet |
| **`O-UNKNOWN`** | **YES** | *"inadequate independent sourcing, unresolved unit/drain/reset/alias/variable-threshold leaf or missing coverage"* |

**Next packet per the row:** a **focused `PIO_FREE` source/queue-interface discovery** on the exact
`UNKNOWN` leaves — **units/bit encoding, capacity, drain/completion, overflow/backpressure, and
read-vs-write ordering**.

**`A2h` is NOT named as prerequisite here.** The row's A2h condition applies only *"if the only missing
witness requires trapped strict time beyond the available pre-OOM prefix."* **The missing witnesses are
documentation, not guest time** — no trapped observation would supply a vendor's queue-depth figure.
**So this discovery's successor is a sourcing/interface discovery, not a run.**

## What this establishes, and what it does not

**Establishes:** the route and identity; the frozen 28-site population with both spellings reconciled; the
constant-`0x80` pretence; the synchronous write path with **no queue**; the two threshold forms with the
variable one preserved; the poll-then-push shape; the untrapped counter's trap gate; the **confirmed
negative control**; and that **independent sourcing is inadequate**, with the wiki's silence documented.

**Does not establish:** the true value, unit, capacity, drain, overflow or ordering semantics; that `0x80`
is true hardware state; that any poll exits; guest observation of the GP's zero; spin exit; boot, liveness
or audio; register-indirect/computed access beyond `A4p` E3. **Carries `A4p`'s inferred C1–C4
convention premise, its static-only direct-read scope, and its lack of true-value/timing evidence.**

**No strict criterion is satisfied and nothing is claimed to work** (§5.8). **No instrumentation exists;
nothing is enabled at closure.** `A4b2-r7` remains `R2-EXPL-INPUT`; `A4b2-r8` and the two-leg result remain
closed; **`A4b1-r4` untouched**; **`0xFFFFB3` stays `UNRESOLVED`**; **no toolkit change**, so **P4's
discovery-transfer bridge is not re-opened**.
