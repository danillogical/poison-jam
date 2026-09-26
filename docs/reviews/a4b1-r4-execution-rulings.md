# A4b1-r4 execution rulings â€” binding

**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25/26.
**Advisor:** child `5c555969-dea9-4b47-be05-62aa0835cde2`, `claude/claude-opus-5-5` @ `high` (persistent, Â§4.4).
Marker **`JSRF-ADVISOR-PROBE-K7Q4M-20260926`**.

---

## Ruling 1 â€” AC-FIX (viii) `FIFO_READ`: **(C)**, an interpretation ruling execution follows

**No `Â§5.4` revision. No fresh Planner.** The Session put options (A) and (B) to the Advisor; the
Advisor **rejected both** and ruled (C). Recorded **verbatim** as Â§4.4 requires.

> **RULING: Neither A nor B. It is (C), an interpretation ruling that execution follows. No Â§5.4 revision and no fresh Planner.**
>
> **1. Reject (A). FIFO_READ stays inputs-only, and the `if (!dir)` guard stays.**
> - Output-FIFO traffic is data the GP produced. It is not an input.
> - Counting it would make AC-INPUTS classify GP output as a candidate stub input.
> - It would also satisfy (viii) with an event of the wrong kind. That is a false-PASS path for the fixture.
>
> **2. Reject (B)'s premise. The input path is not "structurally 0" in the build that runs.**
> - The APU library is built with NDEBUG, so every pinned `assert` is compiled out.
> - In dsp_dma.c's read arm (direction == 0), a buf_id other than 0xE/0xF prints "Unhandled DSP DMA buffer: 0x%x". It then does NOT stop: `assert(!"Unhandled dsp dma buffer")` is empty.
> - The loop that follows writes `scratch_buf` into DSP memory through mem_write. scratch_buf is the file-static intermediate buffer, so it holds stale bytes from the previous transfer.
> - So a GP program that requests a FIFO read DOES consume an input, and it is stub content. No [GPIN] hook records it.
> - This is a hook-completeness gap. The accounting ruling names it as a reversal condition ("a GP input path that bypasses the enumerated hooks"). (viii) exists to catch exactly this, and it did.
>
> **3. Repair, implementing DS6 and AC-PORT step 4 as written:**
> **(a)** Add a marked local modification in dsp_dma.c's read-arm `else` branch, before the fall-through. For buf_id < GP_INPUT_FIFO_COUNT, call `apu_gpin_fifo_read(buf_id, transfer_size)`, using input-FIFO index = buf_id, as your gp_fifo_rw hook already does. Any other buf_id calls `apu_gpin_record_out_of_universe(APU_WATCH_GPIN_FIFO, buf_id)`: a read-direction transfer naming no buffer the pin knows. That fails closed to UNKNOWN.
> **(b)** Keep the gp_fifo_rw hook. It is unreachable at the pin, but it covers a future pin that wires input FIFOs through fifo_rw. The two cannot both fire for one transfer, because the read arm never calls fifo_rw.
> **(c)** The static per-FIFO classification (AC-PORT step 4) states the meaning of all 6 array indices:
>   - input FIFOs 0..1: source "none modelled at the pin â€” the pinned read arm does not implement buf_id 0..3; the consumed data is the stale intermediate buffer". Classified stub/unknown, so any count > 0 is AC-INPUTS FAIL, which is R2-EXPL-INPUT in A4b2;
>   - output indices: "GP-produced, not an input; never recorded".
>   - Packet line 128's parenthetical, "at the pin: the SGE-described guest memory at GPFADDR", was my ruling's inference. It was marked uncertain and fail-closed at the time, and the static classification now decides it the other way. Record that. It is not a text defect needing revision, because the ruling delegated the decision to the static classification.
> **(d)** (viii)'s FIFO_READ case drives the pinned DMA engine through production code, with a read-direction descriptor, buf_id = 0 and a known count. It expects:
>   - fifo[0].reads = 1, and words equal to the transfer;
>   - the "Unhandled DSP DMA buffer: 0x0" line present;
>   - out_of_universe = 0.
>   Add one can-fail twin: buf_id = 5 read-direction. It expects out_of_universe = 1 and fifo counts unchanged. The twin may be folded into (x).
> **(e)** Record the pin-record local-modification entry for dsp_dma.c, with its line and reason.
>
> **4. The latch mirroring (apu_watch.c 566 and 641) is CONFIRMED as the right repair.** It is implementation conformance to DS6 ("the [GPIN] summary carries every array element and the at_clear block"), not a design change.
> - It is safe: the mirror is written under the same write-once guard, and at_clear is a struct memcpy, so the two stay consistent.
> - One addition: the line-vs-snapshot rule must cover both slots. (vi) asserts the printed boot_scratch_read equals the class latch. (viii)'s vp_active_voices > 0 MIXBUF case asserts the printed mixbuf_stub_read equals the MIXBUF_STUB_READ latch. Without that, the next unwritten slot hides the same way.
>
> **5. Claim limit, stated in the evidence:**
> - The ported core runs with NDEBUG, so every pinned xemu `assert` is a silent fall-through, not a stop.
> - This ruling closes the one such fall-through that creates a GP input.
> - Asserts on other paths are not audited. The known cases are unk2/unk13, format default, and dsp_offset out of range, where the DMA continues with mem_space X / address 0. That is a follow-up lead: "enumerate NDEBUG-elided asserts in src/apu/dsp/ that change GP state or inputs". It is not an A4b1 obligation, unless one of them lies on a path an AC-FIX case drives.
>
> **REASON:**
> - Observed: the read-arm fall-through exists in source, and NDEBUG is in the Release defines. The result is an unrecorded stub input that would give a false R2-PASS in A4b2 if the title issues such a DMA.
> - The repair adds the hook DS6 and AC-PORT step 4 already require ("every GP input path â€¦ with the line where it calls the recording hook") on the path that actually exists.
> - (viii) then becomes satisfiable exactly as worded: one FIFO read through production code. No criterion text changes.
> - (A) would satisfy (viii) with the wrong event.
> - (B) would record as a structural fact a claim ("0 by construction") that the shipped build falsifies.
>
> **REVERSED_BY:**
> - the APU library being built without NDEBUG in the configuration AC-BUILD and the A4b2 run use. The fall-through then aborts the process, the input cannot be consumed, and (B) plus a claim limit would suffice;
> - or the pinned source containing a read-direction FIFO path I did not see, so the hook belongs there instead.
>
> **RECORD IN:**
> - docs/reviews/a4b1-r4-execution-rulings.md, verbatim;
> - the pin record's local-modification list (dsp_dma.c);
> - the AC-PORT step 4 enumeration and classification in the evidence file;
> - the follow-up lead in the plan.
>
> Aside, not part of the ruling: `?? tests/apu_watch_fixture_test.c` is untracked in the toolkit. It must be committed before acceptance, and before any push under the owner policy, since a push requires a clean tree.

### Session verification of the ruling's load-bearing facts

The Session **verified the decisive fact before implementing**, because the ruling reverses the
Session's own reading and rests on a build-configuration property:

| Fact | Session check | Result |
|---|---|---|
| The read arm **falls through** rather than stopping | `dsp_dma.c:287-292` â€” the `else` prints, `assert`s, and the `for` loop at `:292` follows | **CONFIRMED** |
| `assert` is elided in the shipping configuration | `build/xboxrecomp/src/apu/xbox_apu.vcxproj` â€” **`NDEBUG` appears in every Release `PreprocessorDefinitions`** (L147, L152, L191, L196, L235, L240) | **CONFIRMED** |

**So the Session's leaning to (B) was wrong, and the Advisor's (C) is right.** A GP program requesting
a FIFO read **does** consume an input â€” the stale intermediate buffer â€” and **no hook records it**. That
is precisely the hook-completeness gap `DS6` and `AC-PORT` step 4 exist to prevent, and `(viii)` caught
it. Recording it as "structurally 0" would have written a claim into the evidence that the shipped build
falsifies.

**This is the second time in this packet that the Session's first reading was the wrong one** (the first
was `P-F`, where the Session judged the non-injectivity a "wording" issue and the Advisor ruled it
load-bearing). Both times the failure mode was the same: reasoning from the source text without checking
the configuration the artifact actually ships in.

### Follow-up lead (recorded, not an `A4b1` obligation)

> Enumerate `NDEBUG`-elided `assert`s in `src/apu/dsp/` that change GP state or inputs.

Known cases named by the Advisor: `unk2`/`unk13`, the `format` default, and `dsp_offset` out of range
(where the DMA continues with `mem_space` X / address 0). Not an `A4b1` obligation **unless** one lies on
a path an `AC-FIX` case drives.

---

## Addendum to Ruling 1 — **binding** (marker `JSRF-ADVISOR-PROBE-K7Q4M-20260926`)

Recorded **verbatim** as the Advisor sent it.

> **ADVISOR — ADDENDUM to the FIFO_READ ruling (C).**
>
> **1. On your addition:**
> - Your semantic ground for rejecting (A) is the one I ruled. FIFO_READ counts GP-consumed inputs only, and the `if (!dir)` guard stays. The per-direction index collision is a second, independent reason (A) is unsafe, and I adopt it into the record.
> - Your xemu-master fact strengthens (C): the read arm is unimplemented upstream too, so the fall-through is not a pin artefact. Your EP fact is also correct: ep_fifo_rw does not reach the hook.
> - Neither fact restores (B). The only premise that makes (B) true, "the input cannot be consumed", is what NDEBUG falsifies: `assert(!"Unhandled dsp dma buffer")` is compiled out. That is the argument of (C), and it stands.
>
> **2. Precision to (C)(a), binding:**
> - dsp_dma.c is SHARED by the GP and the EP. DSPDMAState is embedded in DSPState as `dma`, and `mem_opaque`/`rw_opaque` do not identify the side. gp_ep.c:653–657 also runs EP frames.
> - An unguarded hook in the read arm would therefore record EP fall-throughs as GP inputs. That is a false R2-EXPL-INPUT path, in the opposite direction from the gap it closes.
> - So the hook fires only when the DMA belongs to the GP: `container_of(s, DSPState, dma)->is_gp`. dsp.h:96–105 defines DSPState with `DSPDMAState dma` and `bool is_gp`, set by dsp_init.
> - A `bool is_gp` copied into DSPDMAState in dsp_init is also acceptable. Either way, record it as a marked local modification.
> - The (viii) FIFO case must drive the GP's DMA. Add one can-fail twin: the same read-direction buf_id 0 transfer on the EP's DMA must leave every gpin.fifo count at 0.
>
> **3. The index layout is fixed as follows, to avoid your collision:**
> - Slots 0..GP_INPUT_FIFO_COUNT-1 (0..1) = the GP input FIFOs. This matches the existing gp_fifo_rw hook's `apu_gpin_fifo_read(index, …)` under `!dir`.
> - Slots 2..5 = the output FIFOs' reserved places in the declared 6-element universe. They are never written. The fixture asserts that they are 0 after (viii), as a guard against a future hook recording outputs into them.
> - No change to the existing hook code is needed for this.
>
> **4. Uncertainty stated, not decided:**
> - That a read-arm buf_id 0..1 denotes input FIFO 0..1 is an INFERENCE. It mirrors the write arm's buf_id 0..3 ? output FIFO, and no pinned source states it.
> - The decision does not depend on it. The consumed data is the stale intermediate buffer whatever the buf_id means, and both input slots are classified stub/unknown, so any count > 0 is AC-INPUTS FAIL. A read-arm buf_id outside 0..1 and 0xE/0xF goes to GPIN_OUT_OF_UNIVERSE, which fails closed to UNKNOWN.
> - Record the inference in the per-FIFO classification.
>
> **REVERSED_BY:** unchanged, plus one more condition: evidence that the EP's DMA shares a DSPState with the GP, which would make is_gp insufficient to separate the two.

### Session application of the addendum

**The addendum caught a real defect in the Session's first fix.** The Session had added the read-arm
hook **ungated**; the Advisor showed `dsp_dma.c` is shared by the GP and the EP and both run frames, so
an ungated hook would have recorded EP fall-throughs as GP inputs — a false `R2-EXPL-INPUT` path in the
opposite direction from the gap being closed.

**Verified by the Session before applying:**

| Fact | Check | Result |
|---|---|---|
| `DSPDMAState` is embedded in `DSPState` | `dsp.h:96-108` — `DSPDMAState dma;` and `bool is_gp;` | **CONFIRMED** |
| `container_of` is **not** available | no `container_of` anywhere in `src/apu`, and the QEMU shim does not define it | **CONFIRMED** |
| The EP runs frames too | `gp_ep.c:649-661` — the EP frame block calls `dsp_run(d->ep.dsp, 1000)` | **CONFIRMED** |
| `is_gp` is stable after init | set only at `dsp.c:133-134`; the `dsp_c.c` writes are the **snapshot** copies, not the live flag | **CONFIRMED** |
| Adding a field is safe | nothing `memcpy`s `DSPState`/`DSPDMAState` and nothing serializes them | **CONFIRMED** |

**Applied:** the Advisor's permitted alternative — `bool is_gp` copied into `DSPDMAState` in `dsp_init`
(`dsp.c`), because `container_of` is unavailable — and the hook is gated on `s->is_gp`. Recorded as a
marked local modification in both files. The per-FIFO classification (addendum §3, §4) is written into
`apu_watch.h` beside the universe definition, including the stated **inference**.

**Build re-verified green** with the gate in place.

### Follow-up lead (recorded, not an `A4b1` obligation)

> Enumerate `NDEBUG`-elided `assert`s in `src/apu/dsp/` that change GP state or inputs.

Known cases named by the Advisor: `unk2`/`unk13`, the `format` default, and `dsp_offset` out of range
(where the DMA continues with `mem_space` X / address 0). Not an `A4b1` obligation **unless** one lies on
a path an `AC-FIX` case drives.
