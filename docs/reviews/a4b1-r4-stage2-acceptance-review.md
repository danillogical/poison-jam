# A4b1-r4 — stage-2 acceptance review: **ACCEPT** (final disposition)

**Reviewer:** child `f3f02108-9fd7-42f7-9b6d-bede0a06e04d`, `workbuddy-ai/deepseek-v4.1-flash` @ `max`
(`docs/agent-workflow.md` §1, §2.2 second-stage reviewer).
**Scope:** `AC-PORT` **alone** — the only criterion the first stage did not `AGREE`.
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-26.

## Disposition

```text
VERDICT: ACCEPT
CRITERION: AC-PORT
DISPOSITION: AGREED
BLOCKING: NONE
```

**Acceptance is final.** §2.2: *"The second stage is the final reviewer disposition. `ACCEPT` requires
every mandatory criterion `AGREED`, whether in the first stage or the second."* Every other criterion was
`AGREED` at stage 1 and was out of scope here.

---

## Verbatim review

**1. The corrected row — CONFIRMED, and the correction is right.** (MEASURED)

- `apu_gp_dma_write`: decl `apu_watch.h:341`, def `apu_watch.c:385`, **one production call site**
  `gp_ep.c:144`. (The only other caller anywhere is `tests/apu_watch_fixture_test.c:930`, the fixture
  itself, which packet line 200 requires.)
- `scatter_gather_rw` (def `gp_ep.c:85`) callers: `:166` (`gp_scratch_rw`), `:175` (`ep_scratch_rw`),
  `:196` (`circular_scatter_gather_rw`).
- `circular_scatter_gather_rw` (def `:179`) callers: `:261` (`gp_fifo_rw`), `:327` (`ep_fifo_rw`).
- ⇒ **ALL FOUR pinned write callbacks reach the choke point.** The stage-1 finding was correct **and** the
  Session's "larger error" (`gp_fifo_rw`/`ep_fifo_rw` were missing entirely) is also correct.
- `is_gp` in `gp_ep.c` occurs only at `:351` (comment), `:364`, `:366` — **no `is_gp` gate in the write
  path**. Confirms the false reason.
- The four callbacks are genuinely **pinned**, not Session-authored: vendor blob
  `090682ef:src/apu/dsp/gp_ep.c` has `gp_scratch_rw:92`, `ep_scratch_rw:101`, `gp_fifo_rw:143`,
  `ep_fifo_rw:198`, registered `:498`/`:502`; `scatter_gather_rw:51` and `circular_scatter_gather_rw:110`
  are pinned too. **The edge structure is the pin's.**
- **No other caller is missed**: `dsp_init()` is called exactly twice (`gp_ep.c:676`, `:680`) with exactly
  those four functions; the typedefs are only `scratch_rw`/`fifo_rw`.

**2. "Exactly one choke point" — verified independently.** (MEASURED)

Every guest-memory writer in `src/apu/**`: `stl_le_phys`/`stw_le_phys`/`stb_phys` (defined
`apu_shim.h:181-192`) used **only** at `apu_core.c:165` (`FEMEMDATA`) and
`apu_vp.c:57,58,119,432,445,527` (VP) — exactly the two packet line 170 permits. No `ram_ptr[...] =`
writes; no `MEM32` stores in `src/apu`. The remaining `g_apu_ram_ptr` sites (`apu_watch.c:413/426/488`)
are inside the choke point itself, and `:994` is a read for the trace line. `apu_vp.c:846` is
`memcpy(adpcm_block, &d->ram_ptr[...])` — a **read**, correctly excluded. **Claim holds.**

**3. Input-path table completeness — no missing kind.** (MEASURED)

All four hooks at the cited lines: `PERIPH` `dsp.c:87`; `DMA_READ` `gp_ep.c:110`, `:146`, `:135` (boot
scratch); `FIFO_READ` `gp_ep.c:252`, `dsp_dma.c:333`; `MIXBUF` `dsp_cpu.c:916` **and** `:922` (the
`0xc00` alias — both real). `apu_watch_gpin_kind` has exactly 4 kinds + sentinel, each hooked.
**No fifth GP input kind with no hook was found.**

**4. `PERIPH` classification — reproduced exactly.** (MEASURED)

`dsp.c:52-91`: `v = 0xababa` at `:54`; modelled at `:56` (`0xFFFFB3`), `:59` (`0xFFFFC5`), `:65`
(`0xFFFFD4`), `:68` (`0xFFFFD5`), `:71` (`0xFFFFD6`), `:74` (`0xFFFFD7`) = **6**. Universe
`DSP_PERIPH_SIZE = 128` (`dsp_cpu_regs.h:121`), base `0xFFFF80`. **6 of 128 modelled / 122 stub. EXACT
MATCH.**

**5. Per-FIFO classification — verified.** (MEASURED)

`apu_watch.h:51-80`: `APU_WATCH_FIFO_COUNT = 4+2 = 6` (`apu_regs.h:324-325`). Slots 0..1 input/stub;
2..5 output, never written. Hooks `gp_ep.c:252` (`if (!dir)`) and `dsp_dma.c:333` (gated `s->is_gp`,
`buf_id < GP_INPUT_FIFO_COUNT`, else out-of-universe `:335`). Fixture asserts slots 2..5 == 0
(`apu_watch_fixture_test.c:1710-1715`). Consistent.

**6. Other `AC-PORT` PASS conditions.** (MEASURED)

Packet SHA **matches** (`6DD62A57…35C38`, 445 lines, LF=445/CRLF=0). Toolkit HEAD
`3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d`, tree **clean**. `ls-files --eol src/apu/dsp`: all **27** files
`attr/-text`. **17/17** vendor-commit blobs match the pin record's SHA-256 **and byte sizes** exactly.
Build succeeded. **ctest 14/14**, including both fixture registrations. Both searches **zero hits** with
the **`A4s` control exe live** (it does contain `RECOMP_APU_DSP_ACK`).

**`NDEBUG` independently confirmed in the binary:** the `fprintf` string `"Unhandled DSP DMA buffer"` **is**
present in the built exe, while the `assert` strings `"Unhandled dsp dma buffer"` / `"Unknown dsp dma
buffer"` are **not** — so the asserts really are elided and the `dsp_dma.c` read-arm fall-through is real.

### Asserted rather than derived (the Session asked to be told)

- §2's *"No other write path exists in `src/apu/**`"* was **asserted by reference** to another document,
  not shown. It is nonetheless **true** — the reviewer re-derived it (item 2). **Corrected in the
  document**: the claim is now stated with the reviewer's derivation.
- §1's *"Exactly one call site in the tree"* was **imprecise**: the fixture calls the choke point at
  `apu_watch_fixture_test.c:930`, which packet line 200 requires. **Corrected** to "one call site in
  production code".
- The step-4 tables themselves are now genuinely derived: the reviewer reproduced every edge, every hook
  line and both classifications **from source without relying on the document**.

### Advisories (no effect on the verdict)

1. **`a4b-xemu-pin.md` prose totals were stale** — "5 files" (7 differ from the vendor commit, matching the
   record's own 7-row table) and "28 markers" (**29** exist). The per-file table numbers were all correct.
   **Fixed by the Session.** Step 1's required fields were present throughout, so "the record is complete"
   held.
2. `gp_ep.c:382-387` (the `GPBOOT` trace block) reads guest memory via `apu_guest_dma_ptr` **without**
   `apu_gp_dma_read`. It is trace-gated, feeds only the printed line, and is not a consumed GP input — so
   it is **not** a `DS6` input path. Noted for completeness, not a gap.
3. The carried-forward `A4b2` decision input is **real**: because `ep_scratch_rw`/`ep_fifo_rw` reach the
   choke point, an enabled EP can land an exchange on `W_va` and take `GP_CLEAR`. Correctly flagged, not
   silently absorbed.
4. Read-only respected: toolkit clean at `3a3c7c1`; the game repo shows only the pre-existing
   `M docs/agent-workflow.md`, untouched.

### A reviewer process note worth preserving

> My first search used `Select-String -Recurse`, which **errors** (`parameter cannot be found`). Its
> "ZERO HITS" was a **false negative from a failed command**. I redid it with
> `Get-ChildItem -Recurse | Select-String` and re-confirmed.

**This is the same failure mode the Session hit repeatedly in this packet** — an absence check that reads
as satisfied while the command actually failed. It is recorded because a reader who trusted that output
would have been trusting an error, and because it is now the **fourth** independent occurrence.

---

## Session response

**The stage-2 reviewer confirmed the stage-1 finding, confirmed the Session's larger correction, and
independently re-derived every step-4 claim rather than relying on the document.**

Its two "asserted rather than derived" observations were **both fair and are fixed**:
- the enumeration now states the one-choke-point claim with the derivation, not a reference;
- "one call site in the tree" is corrected to "one call site in production code".

Its advisory 1 was a **real staleness defect in the Session's own pin record** (two prose totals not
updated when the table gained rows). **Fixed**: "5 files" → **7**, "28 markers" → **29** (the table's
per-file numbers sum to 29, so the totals now agree with the table).

**Acceptance is final: `ACCEPT`.** `A4b1` is accepted with exactly its "Establishes" claim. The core stays
in the tree, reachable only through GP GP MMIO, which requires `RECOMP_APU_TRAP`. Per the packet's
Closure, the toolkit `main` now pushes to `origin` — **never `upstream`** — after the five pre-push checks.
