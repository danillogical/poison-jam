# ⚠ `A2h-slot-writer-attribution-r2` — **the `unknown=264` is TWO FOREIGN RIPs, not an instrument bug**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Follows:** `docs/reviews/a2h-attrib-exp1-competitor-found.md` (Exp1, run `…120832-477-a2h-attrib-exp1`).
**Diagnoses:** the `range` line of that run — **`game=9249 host=1079 unknown=264 unavailable=0`**.

---

## The Exp1 record's own disqualifier, quoted

> **"⚠ And `unknown=264` — 264 RIPs the classifier could not place, which the packet says is `INFRA
> FAILURE`. The Session records that: by the packet's own rule, `unknown > 0` is a failure condition, so this
> run is not clean even though the control fired."** ✓

**That was the right call on the evidence then available.** **A count is not a diagnosis, and the two
possible causes demand OPPOSITE responses** — **fix the classifier**, or **accept that some writers are
genuinely unplaceable.** **So the RIPs were identified.**

## The finding, from the run's OWN log

**All 264 `INFRA FAILURE` lines were read out of `jsrf_run.log` and reduced to their distinct addresses:**

| Item | Value |
|---|---|
| **the image** | **`[0x00007FF606630000, 0x00007FF608FF3000)`** |
| **the recompiled extent** | **`[0x00007FF606633440, 0x00007FF607245930)`** |
| **occurrences** | **264** |
| **DISTINCT unplaceable RIPs** | **2** — **`0x00007FFA628FCC71`** and **`0x00007FFA628FCC75`** |
| **inside our own image?** | **ZERO** |
| **distance above the image's end** | **`~0x459909C71`** — **about 18 GB** |

> ## **So the classifier did NOT fail inside its own code.** **It answered `UNKNOWN` CORRECTLY on writers that are genuinely outside anything this facility can place.** ✓

**And the two RIPs are FOUR BYTES APART** — **the shape of a short store sequence inside ONE host function**,
not a scattering of unrelated faults. **That is a single foreign writer, not 264 independent events.**

**For scale: the fixture's own foreign witness — a real `kernel32.dll` function address — is
`0x00007FFA6F391EF0`, in the SAME `0x7FFA…` region.** **That is where the unplaceable RIPs live.**

## ⚠ The packet's rule was written on an assumption this run contradicts

**`unknown > 0` is `INFRA FAILURE` because the rule assumes EVERY write to the watched page comes from the
guest.** **Exp1 is the first evidence that the assumption does not hold: a host module writes the watched
page too.**

**⚠ AND NOTHING HERE WEAKENS THE RULE OR PROMOTES A ROW.** **`unknown > 0` still fails the run closed by the
packet's own text.** **What changes is that the failure is now ATTRIBUTABLE**, so a successor can decide —
**with evidence rather than by re-running** — **whether the rule needs a foreign-writer carve-out, or whether
these two RIPs should be excluded some other way.**

> **That is a row decision for the Session and the Advisor, NOT for the implementation Worker, and it is
> deliberately NOT taken here.** ✓

## What changed in the instrument — diagnostic only

**The first `16` DISTINCT unplaceable RIPs are now recorded verbatim**, each with:

- **the RIP itself**, verbatim;
- **the `VirtualQuery` allocation base the OS reports for it**;
- **whether that base is THIS image** — which is the field that **separates an instrument bug from a foreign
  writer**.

**`range_unknown` remains the uncapped COUNT and `unknown_rip_count` is the bounded sample's size**, reported
**separately** so **a sample is never read as the population.**

**⚠ NOTHING HERE CHANGES ANY CLASSIFICATION.** **The allocation base is an OBSERVATION about the address**;
**the control and every record still key on the RANGE CLASS alone.**

### And the sampler is PROVEN rather than left as untested new code

| Arm | What it asserts |
|---|---|
| **the foreign witness is REAL** | **a `kernel32.dll` function address from `GetProcAddress`** — and **asserted to be outside our image**, so the arm cannot pass by accident |
| **the two cases are DISTINGUISHED** | **its own address → `same_image=1`**; **the foreign address → `same_image=0`**; **and their allocation bases DIFFER** |
| **the sample is a SET, not a log** | **a repeated RIP is NOT re-recorded** — so **a hot loop cannot fill the sample with one address** |
| **the sample is BOUNDED** | **16 entries after 64 more distinct RIPs** |
| **the arm cannot silently skip** | **if no foreign witness can be obtained it FAILS**, because **a skipped arm reads as coverage** |

## Verified state

| Gate | Result |
|---|---|
| **Build** | ✅ `python -X utf8 scripts/build-jsrf.py` — **succeeded** |
| **CTest** | ✅ **22/22 passed** |
| **Fixture (gate ON)** | ✅ **227 checks, 0 failed** |
| **Guards** | ✅ **9/9** |

## Prohibitions and status

**Observation only.** **Every new path is behind `JSRF_TRACE_A2H_SLOTW`, trace-only, OFF by default at
closure.** **No synthetic completion.** **Page-protection only — no DR anywhere; no DR record cited.**
**No fault-RIP encoding is cited as evidence.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)`
**unchanged.** **`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3`
stays `UNRESOLVED`**; **the retired NULL line was not reopened.**

**⚠ NO ON TRIAL WAS RUN BY THE IMPLEMENTATION WORKER.** **This record diagnoses an EXISTING run's archive.**
**The Session owns Exp1 and Exp2.**
