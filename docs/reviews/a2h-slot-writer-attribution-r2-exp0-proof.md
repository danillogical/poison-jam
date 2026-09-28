# `A2h-slot-writer-attribution-r2` — **Exp0: the range-based classifier, PROVEN OFFLINE on the real modules**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-slot-writer-attribution.md` (**r2**), frozen
**`E209D1F4A4405F0B266E8D4DEFDA77A481A277D27615BFAE9ACB9F7A3A6C6378`**.
**Authority:** `docs/reviews/a2h-page-granularity-classifier-ruling.md` — **applied verbatim; the ruling IS the
preflight.**
**Phase:** **Exp0**, the OFFLINE proof that gates everything. **No run, no live claim.**

---

## What was replaced, and why the old thing could never have worked

**`a2h_slotw_classify_store` read the NATIVE instruction bytes at the faulting RIP and tested them for GUEST
encodings** — `p[0] == 0x89`, `(modrm & 0xC0) == 0x80`, then `disp32 == 0x242C` (the installer) or `disp32 ==
0x3EC` with SIB (the candidate).

**In recompiled code the native instruction at a faulting RIP is GENERATED C compiled by the host
toolchain.** It is not the guest's `mov [ecx+0x242c],eax` and there is no mechanism by which it could be. The
classifier was therefore **structurally incapable of returning `ENC_MODRM`**, and **every encoding
classification this facility ever emitted from a fault RIP was UNSOUND** — **ON-3's `enc=3` on the slot event
and the `enc` fields on ON-1/ON-2 among them.**

> **⚠ THE `0x7B3` RESIDUAL IS NOT VERIFIED, NOT REPAIRED AND NOT RE-DERIVED HERE.** *"It dies with the
> paradigm — do not fix its arithmetic."* **It appears in this record only as the thing that is gone.**

**The `enc` FIELD IS DELETED, not deprecated.** It is removed from the ledger, the event record, the
first-touch census and the collector's mirror. **A field that could only ever hold an unsound value is deleted
rather than left in place to be cited.**

---

## The replacement: classification by ADDRESS RANGE, in the NATIVE domain

| Test (in order) | Class |
|---|---|
| **`RIP` in `[start_k, start_{k+1})` for consecutive PUBLISHED RECOMPILED STARTS** | **`GAME_MODULE` (1)** |
| **else `RIP` ∈ this image's own PE bounds** | **`TOOLKIT_HOST` (2)** |
| **else** | **`UNKNOWN` (0) ⇒ `INFRA FAILURE`** |

**The recompiled test is made FIRST, and that ordering IS the classification:** the recompiled module is
**linked into this image**, so its addresses are inside the image bound too. Testing the image bound first
would classify **every** recompiled RIP as `TOOLKIT_HOST` and **the installer control could never fire.**

**Both inputs are read from the REAL artifact:**

- **the image bound** from this process's **own PE headers** (`__ImageBase` + `SizeOfImage`) — the module the
  RIP actually lives in, with no API call and no debugger;
- **the recompiled start set** **published by the EMBEDDER** through `xbox_A2hSlotWatchSetRecompStarts()`,
  derived from addresses the generated dispatch **`recomp_lookup` actually returns**. **The toolkit is a
  static library and cannot enumerate the game's own generated translation units**; an invented bound would
  be exactly the arithmetic this fix removes.

**⚠ `UNKNOWN` IS `INFRA FAILURE`, NOT A WARNING.** A RIP this facility cannot place is counted, the overflow
latch is set, and it is printed. **`range_unknown` and `range_unavailable` are printed WITH their
denominators**, so "0 unknown" is never confused with "the classifier never ran".

### Optional native-disasm corroboration — and it NEVER GATES

**It decodes the NATIVE bytes by NATIVE x86-64 semantics** ("is this instruction a store to memory?"), which
is a statement about **the HOST's** instruction set and carries **no guest-byte expectation**. It is recorded
**beside** the range class. **The control and every record key on the RANGE CLASS alone**, so a partial
hand-written decoder can never fail a run the range classifier placed correctly — **nor promote one it did
not place.**

---

## Exp0 — the proof, on the REAL loaded modules

**Run as the `jsrf_a2h_slotw_fixture_gate_on` arm, `227` checks, `0` failures.** The fixture reads the
**REAL image bounds from the real PE headers** and uses **REAL function addresses** — **not a synthetic
range.**

| # | Requirement | Result |
|---|---|---|
| **1** | **a RIP inside the recompiled module resolves to `GAME_MODULE`** | ✅ **a real published start → `GAME_MODULE(1)`**, and it is **also inside the image bound**, so the **recompiled-before-image ordering is PROVEN**, not assumed |
| **2** | **a RIP inside the toolkit/host image resolves to `TOOLKIT_HOST`** | ✅ **a real in-image function the set does not name → `TOOLKIT_HOST(2)`** |
| **3** | **a RIP outside both resolves to `UNKNOWN`** | ✅ **`image_hi + 0x100000000` → `UNKNOWN(0)`**, with the arm asserting the witness is outside both ranges so it cannot be vacuous |
| **4** | **native-disasm corroboration where available** | ✅ **a scan of 512 bytes of real compiled code finds STOREs and decodable instructions** (measured **186 / 290** on the verified binary — the exact count is a property of the built fixture and is not the claim), so the decoder is **NOT vacuous**; a RIP outside the image reports **`UNDECODED`** rather than reading unreadable memory; **and the range class is unmoved by it** |
| **5** | **the boundary cases — the module's own start/end** | ✅ **every published start is INCLUSIVE**; **just below the lowest start is NOT `GAME_MODULE`**; **the interval between consecutive starts IS body** |

**Fail-closed arms:** **`NULL`, empty and over-cap publications are REFUSED WHOLE and CLEAR the set**, after
which the classifier answers `TOOLKIT_HOST` for the very address it accepted a moment earlier. **Never
truncated** — keeping the first N would classify the functions that happened to fit and **silently misplace
every other one.**

---

## ⚠ Exp0's own strongest corroboration: a REAL ARCHIVED RIP, placed by the new classifier

**The fixture proves the classifier on the fixture's own addresses. This proves it on ON-3's.**

**The archived run's module load base is recovered from the ARCHIVE'S OWN ARTIFACTS** — the collector's
reported runtime address of `g_xbox_a2h_slotw` (`0x00007FF64DDE9FC0`) minus that symbol's RVA from the
archive's own map (`0x1929FC0`) — **giving `0x00007FF64C4C0000`, with no assumption about ASLR.**

| Step | Value |
|---|---|
| **ON-3's recorded installer RIP** | **`0x00007FF64CFFD5ED`** |
| **RVA under the measured base** | **`0xB3D5ED`** |
| **New classifier's answer** | **`GAME_MODULE`** |
| **The archive's own map, at that RVA** | **`sub_0018CE30` at RVA `0xB3D590`**, in `recomp_0004.obj` |
| **Containment** | **`+0x5D` into `sub_0018CE30`** — **guest `0x0018CE30`**, the installer's own function; the next symbol `sub_0018CE50` is `+0x13` away |

> ## **The replacement classifier places ON-3's REAL archived installer RIP inside the SAME function the map places the installer's generated body in.** ✓

**And the archived `0x001D5078`-terminal event (`rip=0000000000000000`, a synthetic read sample) is
`UNKNOWN` under the same rule** — correctly, since address zero is in no range.

### ⚠ This places the installer's RIP under the PRODUCTION rule, not merely under map containment

**The Session's verification record notes the one thing Exp0 could not settle:** *"whether the installer's
write will classify `GAME_MODULE` at run time — the classifier is proven on the real modules offline, but the
installer's own RIP has not been placed."* **It is placed here**, by running **the production rule itself**
(consecutive published starts → image bound → `UNKNOWN`) over the **archived** artifacts:

| Step | Value |
|---|---|
| **the published start set** | **4 849 entries**, extent **`[0x140003200, 0x140c156f0]`** |
| **ON-3's installer RIP** | **`0x00007FF64CFFD5ED`**, RVA **`0xB3D5ED`** |
| **classification by the PRODUCTION rule** | **`GAME_MODULE`** |
| **the OWNING published start** | **`0x140B3D590` → `sub_0018CE30`**, guest **`0x0018CE30`** |
| **offset into it** | **`+0x5D`** |
| **the installer's store site** | guest **`0x0018CE3A`** = **`+0xA` into that same guest function**, whose dispatch entry is **`0x0018CE30 → sub_0018CE30`** |

> ## **So the installer's own write WILL classify `GAME_MODULE`: its recorded RIP lies in the body of the installer's own generated function, found by the set, with the store site `+0xA` into the same guest function.** ✓

**⚠ AND THE CROSS-BUILD MIX IS JUSTIFIED RATHER THAN ASSUMED.** That placement reads the **archive's** map for
native addresses and the **current** build's generated dispatch for the guest-VA → function mapping, which is
only sound if the two builds place the recompiled functions the same way. **MEASURED across all 8 920
recompiler symbols: every one is present in both maps and every one has moved by EXACTLY `-0x240`** — **one
uniform delta, so the two builds place the recompiled functions in the SAME ORDER at the SAME SPACING and only
the block's start moved.** A varying delta would have meant the layouts genuinely differ and **no cross-build
claim could be made**; the single delta is what licenses it.

**⚠ AND A `GAME_MODULE` CLASSIFICATION IS NOT BY ITSELF PROOF THE RIP IS THE INSTALLER'S** — the residual above
means any RIP in the stretched region classifies the same way. **What makes THIS placement specific is the
containment**: the owning start is `sub_0018CE30`, **the installer's own function**, and the store site the
packet names is `+0xA` into that same guest function.

---

## ⚠ WHAT THE SET TEST DOES **NOT** FIX — measured, asserted, and stated rather than claimed away

**The set was adopted expecting it to beat a single `[lo, hi)` interval. It does not, and the fixture's
discriminating arm is what showed it.**

**MEASURED on the real image** (linker map + PE section table): the game's own probe objects
(`harness_probes`, `video_probes`, `gpu_probes`) are linked into the **MIDDLE** of the recompiled extent as a
**single `0x1690`-byte run of 14 host functions** — **`probe_worker_fault`, `gpu_probe_wait` and
`jsrf_probe_gpu` among them, all of which RUN DURING A PROBE RUN AND TOUCH MEMORY.**

**A host run lying strictly BETWEEN two published starts falls inside the interval attributed to the
recompiled function BELOW it under EITHER test** — **the set test and the interval test have EQUIVALENT
COVERAGE on that case.** Distinguishing them needs function **ENDS**, and **the generated dispatch answers
only function ENTRIES.**

> **So the residual is a REAL LIMIT of what this embedder can publish, not a defect in the test — and the
> fixture ASSERTS it** (the in-extent non-member must classify `GAME_MODULE`), **so a future change that made
> the classifier stricter would FAIL the arm and have to be told to a reader rather than passing silently as
> an improvement.** ✓

**The set is kept for what it genuinely provides:** it is **the REAL data the dispatch can answer** (so the
archive carries the actual function starts and **a reader can re-classify every recorded RIP by hand**), and
it makes the extent's definition **explicit** — **first start to last start** — instead of a min/max over a
strided probe that could silently narrow it.

### And the probe's own coverage is MEASURED, not argued

**Guest x86 functions are not 4-byte aligned**, so the stride-4 probe misses entries: **MEASURED, it reaches
7 458 of the dispatch's 8 768 entries and misses 1 310.**

| Question | Measured answer |
|---|---|
| **do any missed entries' native addresses fall OUTSIDE the published extent?** | **ZERO** |
| **do any real recompiled bodies have a start outside it?** | **ZERO** |
| **is the published extent the extent of all real bodies?** | **YES — `[0x140003430, 0x140c15920]`, identical** |

**So the missed entries are aliases and interior labels of functions the probe already found** (many guest
VAs map onto one generated body), **not functions of their own.**

> **⚠ AND THE MISS DIRECTION IS THE SAFE ONE REGARDLESS.** **Missing a START can only make the
> classification STRICTER.** A RIP in an unlisted function is attributed to the listed function below it —
> **still a recompiled body, so still `GAME_MODULE`** — and if it fell below the lowest start it would be
> `TOOLKIT_HOST`, **a CONSERVATIVE miss and never a false `GAME_MODULE`.** **A false `GAME_MODULE` would
> require a HOST symbol between two published starts, and missing an entry cannot create that.** ✓

---

## FIX 2 in the same change: the terminal-value coherence gate

**Implemented and proven in the same packet, per *"classifier fix → coherence gate → attribution read."***

| Last-recorded slot write | Terminal slot read | Verdict |
|---|---|---|
| **`0x0015F9D0`** | **`0x001D5078`** | **`MISMATCH` ⇒ `UNKNOWN`** — **ON-3's own numbers; the gate WORKING, not a defect** |
| **any value** | **the same value** | **`COHERENT`** — **removes the mismatch objection from a RECORDED positive ONLY; never an absence proof** |
| — | — | **`NOT_COMPARABLE`** when no slot write was recorded (`seq == 0`): the terminal stands **ALONE** |
| — | — | **`NO_TERMINAL`** when no terminal was reached |

**The gate is driven through the REAL decision rule, not a restatement of it** — the rule is one function so
the fixture exercises **production behaviour**. **`coherence_mismatch` is a CUMULATIVE latch and is never
cleared**, so a run that mismatched once and later matched **cannot be read as coherent throughout.**

**Shape: RE-ARM AFTER EVERY WRITE (option (a)).** `VirtualProtect` is **page-granular** and the slot is at
offset `0x62C` of page `0x0019D000`, so **opening the page for a NON-SLOT write also makes the SLOT
writable.** **The leave-RW narrowing is NOT implemented and must never be**: ON-3 had **9 077** non-slot
writes, so it would have blinded the instrument **after the first one.** Every write is single-stepped and the
page re-armed immediately; **each window is counted and TICK-LOGGED** (`window_open_count`,
`window_open_ticks_last`, `window_close_ticks_last`, `window_open`) **so a reader sees how much unprotected
time the run contained instead of assuming zero.**

---

## The installer control, re-expressed in the native domain

**Old:** `slot_hit && enc == ENC_MODRM` — **unsatisfiable by construction**, which is why ON-3's slot event
recorded `enc=3`. **The control's condition was never true, on any run, for any writer.**

**New:** **"a store LANDED ON THE DERIVED SLOT (the unchanged address comparison against `arm_slot`) **AND**
the faulting RIP is inside the recompiled module."**

**⚠ THE PRE-VALUE IS STILL NOT PART OF THE TEST** — requiring `pre == 0` was a defect, because it made the
control depend on nothing having touched the slot earlier, **so a run in which the control DID fire could be
reported as `INFRA FAILURE`.** **The VALUE is recorded in the step record's `post_value` and compared against
`0x0015F9D0` OFFLINE.**

**⚠ AND THE NATIVE-DISASM CORROBORATION IS DELIBERATELY NOT PART OF IT**, so a partial hand-written decoder
can never fail a run the range classifier placed correctly.

**The fixture now carries the control as a POSITIVE arm**: a synthetic write AV delivered with a **REAL**
recompiled RIP on the derived slot **fires `installer_control_hits`** — **so the control is proven
SATISFIABLE, which is precisely what the void encoding test could never be.**

---

## Verified state

| Gate | Result |
|---|---|
| **Build** | ✅ `python -X utf8 scripts/build-jsrf.py` — **succeeded**, source/executable identity recorded |
| **CTest** | ✅ **22/22 passed** |
| **Fixture (gate ON)** | ✅ **227 checks, 0 failed** |
| **Fixture (gate OFF)** | ✅ **3 checks, 0 failed** — arm refused, no page protected, ledger inert |
| **Harness probes** | ✅ **19 probes + 9 A2h delivery fixtures + 1 native `#DB` control**, all PASS |
| **Guards** | ✅ **9/9** — `test_agent_docs`, `test_recorded_reviews`, `test_run_profiles`, `test_markdown_tables`, `test_pio_free_demand`, `test_a2h_oom_slice`, `test_a2h_frame_audit`, `test_a2h_null_slot_triage`, `a2h-read-registry.py --self-test` |
| **AC97 / dual-`#DB`** | ✅ **NOT regressed** — both VEH orders × masks `0/1/2/3` × both entry TF values, each handler servicing its own bit exactly once and preserving entry TF |
| **Inertness** | ✅ **`JSRF_TRACE_A2H_SLOTW` OFF at closure**; the game's probe loop is behind `xbox_A2hSlotWatchEnabled()`, so an OFF run does **no work and prints nothing** |

**Live end-to-end verification of the publisher on a bounded probe run:** the set publishes **4 849 distinct
starts over 7 458 probes**, and the collector reports `version=4 size=86616 recomp_valid=1
recomp_start_count=4849` with the set, the coherence verdict and the window counters in the archive.

---

## Prohibitions and status

**Observation only.** **Every new path is behind `JSRF_TRACE_A2H_SLOTW`, trace-only, OFF by default at
closure.** **No synthetic completion.** **No generated-code edits.**
`RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` **unchanged.** **Page-protection ONLY — DR stays
EXCLUDED entirely and no DR record is cited.** **The retired NULL line was not reopened.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**.

**⚠ THE ON TRIALS WERE NOT RUN.** **The Session owns Exp1 and Exp2.** **No attribution claim is made, and
none may be read out of this record: it is Exp0, and Exp0 is OFFLINE.**

**Next:** **Exp1 — the control run, with the installer trap REQUIRED to fire and be classified by native RIP
RANGE.**
