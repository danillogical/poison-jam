# ✅✅ `A2h` — the `unknown` RIPs are a **CRT `memset` from `VCRUNTIME140.dll`** — and it is **THE THIRD ACTOR**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Found by:** the implementation Worker (`432cadc1`, `2022fac` / `1402718`), **and independently confirmed by the
Session.**
**Why it matters:** it resolves the `unknown > 0` disqualifier on three runs AND names the zeroing the Session
had flagged as an unexplained third actor.

---

## ✅ THE SESSION'S INDEPENDENT VERIFICATION

**The Worker claimed the same two RIPs appear across three runs with different image bases.** **The Session
read the runs' own logs and confirmed it byte for byte:**

| Run | image base | image end | `unknown` | distinct unplaceable RIPs |
|---|---|---|---|---|
| **Exp1** | **`0x7FF606630000`** | **`…08FF3000`** | **264** | **`00007FFA628FCC71`, `00007FFA628FCC75`** |
| **Exp2-2** | **`0x7FF74A860000`** | **`…4D223000`** | **282** | **`00007FFA628FCC71`, `00007FFA628FCC75`** |
| **Exp2-3b** | **`0x7FF68B060000`** | **`…8DA23000`** | **262** | **`00007FFA628FCC71`, `00007FFA628FCC75`** |

> ## **THREE DIFFERENT IMAGE BASES, and THE SAME TWO RIPs — byte for byte, unmoved.**

**The Worker's argument is correct and the Session verified it:** **an artifact of OUR image's ASLR would MOVE
with our base.** **These do not move, which is exactly what a SEPARATELY LOADED MODULE looks like.** ✓

**And the OCCURRENCE COUNT varies (`264`/`282`/`262`) while the ADDRESSES do not** — **the same foreign
writer, reached a different number of times per run.** ✓

**The RIPs are ~12–17 GB above our image end, and FOUR BYTES APART** (`…C71` and `…C75`) — **the shape of a
short store sequence in ONE host function.**

## ✅✅ AND THE SESSION IDENTIFIED THE MODULE: **`VCRUNTIME140.dll`'s `memset`**

**The run's own archived linker map lists the exe's imports:**

```
0001:00c86260  memmove  0000000140c87260  f  vcruntime:VCRUNTIME140.dll
0001:00c86266  memset   0000000140c87266  f  vcruntime:VCRUNTIME140.dll
```

> ## **The exe imports `memset` and `memmove` from `VCRUNTIME140.dll` — and `VCRUNTIME140.dll` loads in the `0x7FFA…` region, exactly where the two RIPs are.**

**So the foreign writer is a CRT `memset`/`memmove` zeroing part of the watched page.**

**⚠ AND THAT EXPLAINS TWO THINGS AT ONCE:**

1. **The `unknown` count** — **the classifier answered `UNKNOWN` CORRECTLY on a writer in a different module.**
   **It is NOT an instrument defect.** ✓
2. **THE ZEROING THE SESSION FLAGGED AS A THIRD ACTOR** — **the Session recorded that the slot was zeroed
   between the competitor's write and the terminal in Exp1 and Exp2-2, and called it *"a THIRD actor in the
   chain, and the line has never named it."*** **IT IS THE CRT `memset`.** ✓

**And the varying count is explained:** **the same CRT routine, called a different number of times per run.**

## ⚠ SO THE PACKET'S `unknown > 0 ⇒ INFRA FAILURE` RULE RESTS ON A PREMISE THAT IS NOW REFUTED

**The rule assumes *"every write to the watched page comes from the guest."*** **Three runs show a HOST module
writes it too.**

**The Worker's handling is exactly right and the Session records it:**

> **"I did NOT weaken the rule and did NOT promote a row — that is a row decision for you and the Advisor.
> What I did is make the failure ATTRIBUTABLE: the first 16 distinct unplaceable RIPs are now recorded with
> their `VirtualQuery` allocation base and whether that base is THIS image, so a reader can tell an instrument
> bug from a foreign writer FROM THE ARCHIVE. `range_unknown` stays the uncapped count; the sample size is
> reported separately. Nothing changes any classification — the base is an observation, the control still keys
> on the range class alone."** ✓

> **So the Worker made the failure DIAGNOSABLE without changing the rule that produced it.** **That is the
> correct separation: evidence improves, the contract does not move unilaterally.** ✓

## ⚠ AND A SECOND, DEEPER CONSEQUENCE THE SESSION MUST STATE

**If a CRT `memset` writes the watched page, then the page-level watch is observing a HOST allocator/CRT
operation as well as guest writes.**

**And the CRT `memset` ZEROES memory — which is exactly the `post=00000000` the Session has been reading as
*"the slot was zeroed by an unknown third actor."***

**⚠ So the slot's `0x001D5078` → `0` transition may be a HOST CRT `memset` over a region that HAPPENS to
include the slot's page** — **not a guest write at all.**

**The Session does NOT conclude that** — **it records it as the leading hypothesis with a stated test:**
**whether the CRT `memset`'s faulting address range covers the slot's offset `0x62C`.** **The archive now
carries the allocation base, so a successor can answer it offline.** ✓

## The complete chain as it now stands

| Step | Actor | Evidence |
|---|---|---|
| **1** | **the installer** (`sub_0018CE30`) writes **`0x0015F9D0`** | **observed, 3 runs, control green** |
| **2** | **`sub_00038530`** (guest `0x00038530`) writes **`0x001D5078`** | **observed, 2 runs, same RVA `0x52FE38`, K≥2** |
| **3** | **a CRT `memset` (`VCRUNTIME140.dll`)** zeroes part of the page | **2 RIPs, unmoved across 3 bases** |
| **4** | **the read at `0x00193E62`** takes whatever is there | **`return=00193E62`, `eax=0` in Exp2-2** |
| **5** | **`0x00193EB5 call eax`** | **`invalid target 0x00000000`** |

> **And the line's `0x001D5078` terminal is a DIFFERENT path — the one where step 3 does NOT intervene before
> the read.** ✓ **That is consistent with the line's long-standing knowledge that the terminal varies across
> runs.**

## What the Session is NOT doing

- **NOT promoting a row.** **The packet's qualifying criteria are unmet** (no matching `0x001D5078` terminal;
  `unknown > 0`). **The `unknown` premise is refuted but the RULE has not been amended, and amending it is a
  row decision.**
- **NOT editing the packet or the rule.**
- **NOT claiming the CRT `memset` covers the slot** — **that is the successor's offline test.**

## Prohibitions and status

**Observation only.** **Page-protection only — no DR anywhere.** **No DR record cited.** **No synthetic
completion.** **No fault-RIP encoding cited.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)`
unchanged. **`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened.** **3 of N=5 used; no further ON run pending a row
decision.**
