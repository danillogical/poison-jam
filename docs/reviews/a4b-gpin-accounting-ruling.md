# A4b `[GPIN]` input-accounting ruling (§5.5, third trigger)

**Advisor:** child `407c54a3-6ca4-4a65-835b-faf5355195cd`, route `claude` / `claude-opus-5-5` @ `high`.
**Trigger:** three consecutive blocking verdicts on input accounting — r1 (a lossy once-per-key log decided `AC-INPUTS`), r2 (the fix was a 256-entry table), r3 (the table's key universe is **1024 words** for MIXBUF alone; `GP_DSP_MIXBUF_BASE 0x001400` + `DSP_MIXBUFFER_SIZE 1024`, Session-verified, so a single frame's mixbin sweep overflows it → false `R2-UNKNOWN`).

## Ruling (verbatim)

> RULING: Your observation is correct, and part of the fault is mine. My ledger ruling's (a)3 said "bounded site table (say 16 entries) plus overflow latch" without stating the premise that made it safe: the key universe (the instrumented CPU store sites) is finite, enumerated in source, and independent of how long or busy the run is. The pattern was then applied to a key (DSP word address, guest page) whose universe is not bounded by anything the decision needs. The redesign: AC-INPUTS keeps deciding, but on **provenance classes over a statically enumerated, finite universe**, recorded as fixed arrays of uncapped counters that cannot overflow by construction. Completeness comes from **structure** (every input path enumerated, each with a fixture case), not from counting distinct keys at run time. Per-address and per-page inventories become capped **observation** for the A4c brief and never decide anything. The input counters are frozen into a write-once snapshot at GP_CLEAR, which replaces the "cut-off". No third patch of the table/overflow shape.
>
> (a) Design — key each kind by what its classification depends on, with a stated finite universe (A4b1 DS6, replacing the 256-entry table):
> - MIXBUF: provenance is a property of the frame's mix-buffer content, not of the word. The pinned frame path writes the VP mixbins into GP X memory at GP_DSP_MIXBUF_BASE before running the GP (pin record item 4). At that write, set a per-frame flag mixbuf_stub = (vp_active_voices > 0) || (any sample written is non-zero); the second clause is conservative against the test tone and other writers. The read hook indexes a fixed array [NUM_MIXBINS = 32] by bin = (addr − 0x1400) / NUM_SAMPLES_PER_FRAME, with uncapped {reads, reads_while_stub} per bin, plus one write-once latch MIXBUF_STUB_READ (first read with mixbuf_stub set: addr, frame, seq, vp_active_voices). Universe: 32 bins. This also closes r2 D5 (first value only): the decision is "was any stub-content word read", counted over every read.
> - PERIPH: fixed array [DSP_PERIPH_SIZE = 128], indexed by peripheral offset, with uncapped {reads, first value, first seq}. The Session classifies each of the 128 offsets once, statically, from the ported read_peripheral source: modelled (the core computes it from tracked state) or stub/unknown (a constant or unported register). Universe: 128.
> - FIFO_READ: fixed array [GP_INPUT_FIFO_COUNT + GP_OUTPUT_FIFO_COUNT = 6], indexed by FIFO, uncapped {reads, words}. Each FIFO's data source is classified statically from the pinned dsp_dma/fifo_rw path (at the pin: the SGE-described guest memory at GPFADDR). Universe: 6.
> - DMA_READ: key by **region class** of the translated address, decided inside the one translation function apu_guest_dma_ptr (the existing choke point). Classes: LOW_RAM [0, g_memory_size), CONTIG (the 0x80000000 window), DEVICE (the MCPX, NV2A or flash apertures, i.e. any address ≥ 0xFD000000), OTHER_MAPPED. Uncapped {reads, bytes} per class, plus a write-once first-VA latch per class. Universe: 4. The bootstrap's scratch read gets its own write-once latch BOOT_SCRATCH_READ {va, first dword, seq}, set in the bootstrap path. That is the presence witness, and it no longer depends on a table having room.
> - Out-of-universe: one uncapped counter GPIN_OUT_OF_UNIVERSE plus a write-once latch, for any record call whose kind or index is out of range (for example a peripheral offset ≥ 128). This is a **bug detector, not a volume guard**: no run length or input volume can raise it.
> - Freeze at the clear: when GP_CLEAR latches (on the APU thread, in the DMA write path), copy every input counter and latch into a write-once `at_clear` block. The running counters continue for the whole run and serve the NOCLEAR/NOEXEC brief. This deletes the cut-off state and its reset hazards.
> - Observation, never decisive: a capped list (for example the first 64 distinct DMA pages and the first 64 distinct PERIPH offsets, with first values) for the A4c brief. The claim limit states it is capped; no row may read it.
> - Emission: under the trace, the `[GPWATCH] counts` line gains the fixed arrays (or a `[GPIN] summary` line per kind carrying every array element), printed at the existing cadence and once for `at_clear`. There is a fixed number of lines per emission and no per-key lines, so nothing can be lost.
> - AC-FIX: replace (ix) with:
>   - (ix') a volume case: a full 1024-word mix-buffer sweep for each of N ≥ 300 frames, a bootstrap-sized DMA (0x2000 bytes), and reads of all 128 peripheral offsets, giving exact expected counts and GPIN_OUT_OF_UNIVERSE = 0. This is the known-good the reviewer required, and the known-bad for the r3 design, which would overflow on it.
>   - (x) the out-of-universe detector is reachable (a direct call with offset 128 fires the latch).
>   - (xi) at_clear freezes: reads before GP_CLEAR appear in at_clear, reads after appear only in the running counters.
>   Keep (viii) (one case per kind through the production hook). AC-PORT step 4's hook enumeration is what makes a zero count mean something.
>
> (b) Should overflow make a strict criterion UNKNOWN? After this redesign overflow cannot happen, so the question dissolves. The general answer: an accounting record may make a criterion UNKNOWN only for **integrity** failures that no run length can cause:
> - the accounting is absent (no at_clear block, or no counts line);
> - GPIN_OUT_OF_UNIVERSE > 0;
> - AC-BOOT PASS without a BOOT_SCRATCH_READ latch;
> - a hook enumerated in AC-PORT step 4 whose kind shows no fixture exercise.
> Never for volume. And the distinction that makes a zero admissible: **a zero from an uncapped counter, updated at the event, over a hook set shown complete by enumeration and fixture, is positive evidence that no such read occurred.** The absence of a line is not. This is consistent with the losslessness rule; it is what that rule was for.
>
> AC-INPUTS (A4b2), rewritten on the at_clear block:
> - PASS: every one of the following is 0 in at_clear:
>   - MIXBUF reads_while_stub, summed over bins;
>   - reads of every PERIPH offset the static classification marks stub/unknown;
>   - reads of every FIFO whose source is classified stub/unknown;
>   - DMA DEVICE and OTHER_MAPPED reads;
>   and BOOT_SCRATCH_READ is present.
> - FAIL (→ R2-EXPL-INPUT): any of those is > 0. The evidence record names which ones, from the latches and arrays.
> - UNKNOWN: an integrity failure from the list above.
> Claim limits, stated:
> - LOW_RAM and CONTIG reads are classed guest-written **by region, not by writer**. Host writers into guest RAM (kernel-bridge HLE results, the VP stub's stl_le_phys voice and SGE writes at apu_vp.c:119/432/445/527) are not distinguished. The classification is not a data-flow proof.
> - The static per-offset and per-FIFO classification is pinned to the ported source revision.
>
> (c) Ordering: the fix is right, and the rows get one structural rule. AC-INPUTS is a **qualifier**: its only job is to split R2-PASS from R2-EXPL-INPUT. So:
> - AC-INPUTS is evaluated only when AC-BOOT, AC-RUN, AC-CLEAR (PASS) and AC-NOCPU all PASS. Otherwise it is recorded as "not evaluated", and the running counters go into the brief.
> - R2-UNKNOWN reads: "a gate fails; or any AC **other than AC-INPUTS** is UNKNOWN; or AC-CLEAR PASS while AC-BOOT or AC-RUN is not PASS; or AC-INPUTS is evaluated and UNKNOWN."
> - The exhaustiveness note is updated: after the first four rows, AC-INPUTS is only consulted in its final line.
> No wider reordering is needed. With (a) in place, AC-INPUTS can only be UNKNOWN for integrity failures, which are legitimately R2-UNKNOWN, and those can no longer preempt NOCLEAR/NOEXEC/NOFRAMES.
>
> (d) General rule — yes. It is the bounded-size companion to the losslessness rule. Record it as §6.1 item 6(b):
> "**Decision inputs are bounded by construction.** A record a row decides from must have a size fixed by a finite universe that is stated and derived from source (a register file, a FIFO count, a fixed set of classes, the enumerated instrumentation sites), independent of run length and input volume. Key it by the property the decision classifies (provenance class, bin, region), not by the identity of individual events (address, page, value). A table whose key universe is not shown finite is observation only. An overflow or out-of-universe counter is a bug detector; if a record can overflow because the run was long or busy, the key is wrong. Completeness of instrumentation is established structurally (enumerated hook sites, each with a fixture case), not by counting distinct keys at run time. A criterion whose only role is to qualify a PASS is evaluated only when that PASS holds."
> Retroactive note on my ledger ruling: the 16-entry CPU-site table is compliant only because its universe is the enumerated jsrf_watch_store call sites (7 listed at baseline, plus those found by the text match). r4 states that count and sizes the table ≥ it, index-per-site. CPU_ZERO_OVERFLOW is then a bug detector, as rule 6(b) says.
>
> BASIS:
> - Observed (read myself this turn):
>   - the r3 review blocks B1/B2;
>   - A4b1 DS6 lines 116–131, AC-FIX (viii)/(ix) at 311–312, AC-PORT step 4 at 236;
>   - A4b2 AC-INPUTS 246–267, rows and exhaustiveness 269–291;
>   - pin record items 3–4 (bootstrap scratch read of 0x800 words; mixbins written into GP X memory before the GP runs).
> - Observed, toolkit 0d7929c: apu_regs.h:322 GP_DSP_MIXBUF_BASE 0x001400, :324–325 GP output/input FIFO counts 4/2, :333 NUM_SAMPLES_PER_FRAME 32, :334 NUM_MIXBINS 32; apu_state.h:36 DSP_MIXBUFFER_SIZE 1024 (= 32×32), :37 DSP_PERIPH_SIZE 128; apu_vp.c stl_le_phys guest writes at 119/432/445/527; apu_core.c:557 single APU frame thread.
> - Inferred: GP input hooks and the GP DMA write that latches GP_CLEAR run sequentially on the APU frame thread (the interpreter runs inline in dsp_run inside se_frame), so the at_clear freeze orders them exactly. The xemu pinned source has not been re-read for thread use.
> - Inferred: MIXBUF bin = (addr − 0x1400)/32 matches the pinned layout (32 bins × 32 samples). The ported layout must be confirmed in AC-PORT step 4.
> - Uncertain: whether the pinned GP FIFO path ever sources anything other than SGE guest memory. The static FIFO classification decides it, and it is fail-closed: stub/unknown counts toward FAIL.
>
> REVERSED BY:
> - The ported core using GP threads other than the APU frame thread, or a GP input path that bypasses the enumerated hooks. The freeze or the completeness claim then needs a different choke point.
> - The mix-buffer layout not being bin-major 32×32 in the pinned source. The bin index then changes; the single-key form (reads_while_stub only) remains valid.
> - A provenance distinction the decision needs that a region or class key cannot express, for example the owner requiring the writer of each guest-RAM word. That is a data-flow packet, not accounting.
>
> RECORD IN:
> - This ruling, verbatim with this child's ID and route, in a new docs/reviews/a4b-gpin-accounting-ruling.md, cross-referenced from a4b1-a4b2-r3-adequacy-review.md §5.5.
> - Append to docs/reviews/a4b-watch-ledger-ruling.md (a)3: "premise: the site universe is finite and enumerated; see a4b-gpin-accounting-ruling.md".
> - A4b1-r4: DS6 input accounting per (a); AC-FIX (ix′)/(x)/(xi); fold in the r3 B2 fixes (sge0 defined as the address in SGE entry 0 and asserted with the entry and the page at different VAs; counts seq defined and asserted).
> - A4b2-r4: AC-INPUTS per (b); rows and exhaustiveness per (c); P2 repinned with the new fields mapped.
> - Rule (d) as docs/agent-workflow.md §6.1 item 6(b), plus one sentence beside the evidence rule in docs/jsrf-run-profiles.md.

## Session verification of the load-bearing constants

| Claim | Check | Result |
|---|---|---|
| MIXBUF base `0x001400` | `apu_regs.h:322` | **Confirmed** |
| MIXBUF size 1024 words | `apu_state.h:36` `DSP_MIXBUFFER_SIZE 1024` | **Confirmed** — and `0x1800−0x1400 = 1024` |
| The r3 table (256) is smaller than the universe (1024) | arithmetic | **Confirmed** — the r3 B1 defect is real |

## Session note

The Advisor owned the fault: its ledger ruling's (a)3 gave the *bounded table + overflow latch*
pattern **without stating the premise that made it safe** (a finite, source-enumerated key
universe). The fix is a **redesign**, not a patch, and rule 6(b) is the general form that stops
this being re-derived. The 16-entry CPU-site table is retroactively justified because its
universe *is* the enumerated call sites.
