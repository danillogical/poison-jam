# `A2h-slot-writer-attribution-r2` — Session verification: **Exp0 PASSES, both fixes implemented, and the residual is stated rather than hidden**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-slot-writer-attribution-r2`, frozen
**`E209D1F4A4405F0B266E8D4DEFDA77A481A277D27615BFAE9ACB9F7A3A6C6378`**.
**Worker:** `432cadc1-348c-4473-9709-e730d066e341`.
**Commits:** game `52e7ec4`…**`7f2422b`**; toolkit `0f7ac48`…**`f1adbd8`**.
**Record:** `docs/reviews/a2h-slot-writer-attribution-r2-exp0-proof.md`.

**BUILD succeeded · CTEST 22/22 · HARNESS 19 probes + 9 A2h delivery fixtures + 1 native-`#DB` control, all
PASS.** **Both trees clean.**

---

## ✅ Exp0 PASSES — the classifier is rebuilt in the native domain and proven on the REAL modules

| Test (in order) | Class |
|---|---|
| **`RIP ∈ [start_k, start_{k+1})` for consecutive PUBLISHED RECOMPILED STARTS** | **`GAME_MODULE` (1)** |
| **else `RIP` ∈ this image's own PE bounds** | **`TOOLKIT_HOST` (2)** |
| **else** | **`UNKNOWN` (0) ⇒ `INFRA FAILURE`** |

**And the ORDER is the classification, with a reason the Session had not seen:**

> *"The recompiled test is made FIRST … the recompiled module is **linked into this image**, so its addresses
> are inside the image bound too. **Testing the image bound first would classify EVERY recompiled RIP as
> `TOOLKIT_HOST` and THE INSTALLER CONTROL COULD NEVER FIRE.**"* ✓

**Both inputs come from the REAL artifact:** **the image bound from the process's own PE headers**
(`__ImageBase` + `SizeOfImage`), **and the recompiled start set PUBLISHED BY THE EMBEDDER** through
`xbox_A2hSlotWatchSetRecompStarts()`, **derived from what `recomp_lookup` actually returns.**

**And the reason the toolkit cannot derive it alone is stated:** *"The toolkit is a static library and cannot
enumerate the game's own generated translation units; **an invented bound would be exactly the arithmetic
this fix removes.**"* ✓ **That is the paradigm correction applied correctly.**

**`UNKNOWN` is `INFRA FAILURE`, not a warning** — **and `range_unknown`/`range_unavailable` are printed WITH
their denominators**, so *"0 unknown"* is never confused with *"the classifier never ran."* ✓

## ⚠ THE RESIDUAL — and it is the most creditable part of this work

**The Worker's own statement:**

> **"The set was adopted expecting it to beat a single `[lo, hi)` interval. IT DOES NOT, and the fixture's
> discriminating arm is what showed it."**

**MEASURED on the real image:** **the game's own probe objects (`harness_probes`, `video_probes`,
`gpu_probes`) are linked into the MIDDLE of the recompiled extent as a single `0x1690`-byte run of 14 host
functions** — **`probe_worker_fault`, `gpu_probe_wait` and `jsrf_probe_gpu` among them, all of which RUN
DURING A PROBE RUN AND TOUCH MEMORY.**

**So a host run lying strictly between two published starts is attributed to the recompiled function below it
under EITHER test** — **and distinguishing them needs function ENDS, which the generated dispatch does not
answer.**

> **"So the residual is a REAL LIMIT of what this embedder can publish, not a defect in the test — and the
> fixture ASSERTS it (the in-extent non-member must classify `GAME_MODULE`), so a future change that made the
> classifier stricter would FAIL the arm and have to be told to a reader rather than passing silently as an
> improvement."** ✓

**The Session records this as exemplary:** **the Worker built a discriminating arm, the arm REFUTED its own
design expectation, and the Worker kept the arm as a CAN-FAIL GUARD against a future "improvement" that would
silently change the semantics.** **That is precisely the owner's standing instruction — *"add can-fail
validation when a parser can silently return plausible output"* — applied to its own work.** ✓

**And the miss direction is proven SAFE rather than asserted:**

> *"Missing a START can only make the classification STRICTER. A RIP in an unlisted function is attributed to
> the listed function below it — still a recompiled body, so still `GAME_MODULE` — and if it fell below the
> lowest start it would be `TOOLKIT_HOST`, a CONSERVATIVE miss and never a false `GAME_MODULE`. **A false
> `GAME_MODULE` would require a HOST symbol between two published starts, and missing an entry cannot create
> that.**"* ✓

**And the probe's own coverage is MEASURED:** **the stride-4 probe reaches 7 458 of the dispatch's 8 768
entries and misses 1 310** — **and the missed entries are aliases and interior labels, with ZERO falling
outside the published extent and ZERO real recompiled bodies outside it.** ✓

## ✅ FIX 2 — the terminal-value coherence gate

| Last-recorded slot write | Terminal slot read | Verdict |
|---|---|---|
| **`0x0015F9D0`** | **`0x001D5078`** | **`MISMATCH` ⇒ `UNKNOWN`** — **ON-3's own numbers; the gate WORKING** |
| **any value** | **the same value** | **`COHERENT`** — **removes the mismatch objection from a RECORDED positive** |
| — | — | **`NOT_COMPARABLE`** when no slot write was recorded (`seq == 0`) |
| — | — | **`NO_TERMINAL`** when no terminal was reached |

**And two details the Session checked:**

1. **"The gate is driven through the REAL decision rule, not a restatement of it — the rule is one function so
   the fixture exercises PRODUCTION behaviour."** ✓
2. **"`coherence_mismatch` is a CUMULATIVE latch and is never cleared, so a run that mismatched once and later
   matched CANNOT be read as coherent throughout."** ✓ — **that is the exact failure mode a clearable flag
   would introduce, closed in advance.**

## ✅ Shape (a) implemented, and the narrowing correctly NOT implemented

**`VirtualProtect` is page-granular and the slot is at offset `0x62C` of page `0x0019D000`, so opening the
page for a NON-SLOT write also makes the SLOT writable.** **The Worker implemented RE-ARM AFTER EVERY WRITE
and did NOT implement the leave-RW narrowing.** ✓ **That is the Planner's structural finding applied
correctly — the flaw the Planner caught would have blinded the instrument, and it is not in the code.**

## The Session's verification

| Claim | Verified |
|---|---|
| **build succeeds** | ✅ |
| **CTEST 22/22** | ✅ |
| **harness 19 + 9 + 1 all PASS** | ✅ |
| **both trees clean** | ✅ |
| **the recompiled test precedes the image bound** | ✅ **read at `:2903`** |
| **`UNKNOWN` ⇒ INFRA FAILURE** | ✅ |
| **the residual is asserted by a fixture arm** | ✅ |
| **no fault-RIP encoding is cited** | ✅ **the classifier no longer emits one** |
| **AC97 / dual-`#DB` preserved** | ✅ **fixture matrix unchanged** |
| **the leave-RW narrowing is absent** | ✅ |

## What remains

**Exp1 — the control run.** **The installer trap must fire, now classified by RANGE.** **Then Exp2 — bounded
attribution, N ≤ 5, early-stop, K ≥ 2-agree.** **Both are the Session's to run.**

**And the Session notes the one thing Exp0 could NOT settle:** **whether the installer's write will classify
`GAME_MODULE` at run time.** **The classifier is proven on the real modules offline, but the installer's own
RIP has not been placed.** **Exp1 is what tests it** — **and the residual above means a `GAME_MODULE`
classification is not by itself proof that the RIP is the installer's.**

## Prohibitions and status

**Observation only.** **Page-protection only — no DR anywhere.** **No DR record cited.** **No synthetic
completion.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **No fault-RIP encoding is
cited as evidence.** **`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**;
**`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was not reopened.**
