# A4b1-r3 / A4b2-r3 adequacy review — INADEQUATE (both); §5.5 triggers on `[GPIN]`

**Reviewer:** Planner child `17943fbb-15b8-49a4-8030-6a0d1ef52a6a`, fresh (§5.1.5).
**Hashes verified:** `A4b1-r3` `321ABCF7…7AFDA`; `A4b2-r3` `CFB8C0EB…BFCE9`.

**Gaps 1–3 from the r2 review are CLOSED.** DS6 defines every latch field (`seq` = the event's own
increment, `va = W_va`, `insns = gp_insns`); in (i) the DMA destination starts at `W_va−0x810` with
length `> 0x814`, so recording the destination start **FAILS**; (iv) asserts `site = S_1`; (vi)
asserts `boots`/`gp_frames`/`gp_insns`. The `[GPIN]` table is 256 entries plus an uncapped
`GPIN_OVERFLOW` counter and a write-once latch; the reset clears the table and the cut-off; (viii)
drives all four kinds through production hooks; (ix) exercises overflow and can fail; `AC-INPUTS` is
UNKNOWN on overflow or missing accounting; P2 pins `A4b1-r3` with a per-field map.

## BLOCKING — `A4b1` B1: the `[GPIN]` key space exceeds the table

**Session-verified:** `GP_DSP_MIXBUF_BASE 0x001400` (`apu_regs.h:322`) and `DSP_MIXBUFFER_SIZE 1024`
(`apu_state.h:36`) — the GP mix buffer is **1024 DSP words** (`X:0x1400–0x17FF`), and **MIXBUF is
keyed per word** into a **256-entry** table.

Recording runs from the bootstrap until `GP_CLEAR`, spanning hundreds of frames (A4a R1: frames open
at L3286; the guest's store of 3 is first seen at L5226). A GP program that reads its mixbins once per
frame touches **more than 256 distinct words in the first frame** → `GPIN_OVERFLOW > 0` →
`AC-INPUTS` UNKNOWN → **`R2-UNKNOWN` on both R1 runs, even when the GP truly cleared the word from
modelled inputs** — a false UNKNOWN. `AC-FIX` cannot catch it: (ix) tests overflow only with synthetic
keys.

**Required:** **bound the key universe by construction** — key MIXBUF per mixbin index or as a single
key with a sticky `vp_active_voices > 0 at any read` flag (which also closes r2 D5), or size the table
from a stated finite universe per kind. Keep the overflow accounting. Add an `AC-FIX` case where a
full mix-buffer sweep plus bootstrap-sized `DMA_READ` **does not** overflow.

## BLOCKING — `A4b1` B2 / `A4b2` B1: two decision inputs still undefined and unasserted

`sge0` (printed by `[GPBOOT]`, decides `AC-BOOT` → `R2-NOBOOT`) and the counts line's `seq` (decides
`AC-INPUTS` step 1 via `K.seq`) are **neither defined in `A4b1` nor asserted in `AC-FIX`**.
**Scenario:** an executor prints the SGE table base (`GPSADDR` = `803CC000`) instead of entry 0's page
(`B` = `803C0000`); every `AC-FIX` case passes and `A4b2` gets a **false `R2-NOBOOT`** — a wrong row
routing to `A4c`. **Required:** DS7 defines `sge0` as the guest address in SGE entry 0 (the page the
bootstrap reads); (vii) asserts `sge0` = the scratch page VA, with the fixture placing the SGE entry
and the page at **different** VAs so the check can fail. DS6 defines counts `seq`; (vi)/(ix) assert it
against the snapshot. **`A4b2` B1:** repin P2 to the revision that closes these and add both to the map.

## BLOCKING — `A4b2` B2: `AC-INPUTS` UNKNOWN preempts the rows it should not

`AC-INPUTS` is now UNKNOWN **unconditionally** when the accounting is missing or overflowed. But
recording stops only at `GP_CLEAR`, so **without a clear it runs the whole 30 s** and is far more
likely to overflow. In the `R2-NOCLEAR`/`NOEXEC`/`NOFRAMES` cases `AC-INPUTS` is then UNKNOWN, and
**`R2-UNKNOWN` is evaluated before those rows and matches first** → a **false `R2-UNKNOWN`** that
survives the rerun and routes to the Planner instead of `A4c`, whose brief is exactly this inventory.
**Required:** `AC-INPUTS` is evaluated **only when `AC-CLEAR` PASSes**; otherwise it is "not
evaluated" and its accounting is recorded for the brief — or `R2-UNKNOWN` counts an `AC-INPUTS`
UNKNOWN only when `AC-CLEAR` is PASS. Update the exhaustiveness note to match.

## §5.5 — third consecutive verdict on the input-accounting mechanism

`[GPIN]` has now blocked **two consecutive revisions** (r2: lossy log deciding `AC-INPUTS`; r3: key
space exceeding the table). The reviewer's own words: *"The Session should either scope r4 as a
redesign of GPIN keying (not a patch) or take the input-accounting method to the Advisor."* The
Session is taking it to the Advisor, per §5.5 and the owner's standing rule that technical questions
end at the Advisor. **`A4b1`/`A4b2` are not revised again until that ruling lands.**

## DEFERRED

`A4b1`: D2 (`GP_NONZERO_OVER` never fixture-exercised) and D4 (reset does not reset GP core state)
stand; (viii) leaves the executor to choose how each kind is driven — consider naming the program's
reads; line 285 compares `frame`/`insns`/`dsp_addr` "where the snapshot records them", slightly soft
but (i) pins `insns` and `dsp_addr`. `A4b2`: r2 D1 (the 400-line-capped `[APUMMIO]`) stands — it fails
closed but is in literal tension with §6.1 now that `[GPBOOT]` `gprst`/`prev` and counts `boots` are
fixture-asserted; D3–D6 stand; "distinct `[GPIN]` lines by seq" is redundant but harmless.

## DECISIONS (reviewer's)

Gap 1 closed (reversed if the fixture cannot place the write at `B`). Gap 2 closed as specified; B1 is
a new defect in **how it was sized**, not a reopening. D1 (the DS5 CAS race) correctly taken. Rows
still ordered and exhaustive, each naming its next step. `A4b2`: `AC-CLEAR` unchanged and sound;
`AC-INPUTS` no longer decides on a line's absence, which is correct in isolation — B2 is only about
which rows it can preempt.
