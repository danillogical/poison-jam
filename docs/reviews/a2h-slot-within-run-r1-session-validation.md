# `A2h-slot-within-run-attribution-r1` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-slot-within-run-attribution.md`, revision
`A2h-slot-within-run-attribution-r1`, authored by Planner `e2fce333-67c0-40cb-8248-36f0b9babe41`
(`codex/gpt-6-sol` @ `high`), committed `d4bfea7`.
**Authority:** the Advisor re-ruling `docs/reviews/a2h-slot-read-path-advisor-reruling-after-run1.md`.
**Adequacy:** the authoring Planner's own §5.8 review — **`ADEQUATE`**, conditional on the Session's
literal invocation and identity validation.

**The Planner named its own uncertainty precisely:** *"Session must verify archived/current executable and
code identity, and literal ON gate commands before freezing."* **This record discharges that, and the
validation found one real defect.**

---

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-slot-within-run-attribution-r1`** |
| Lines | **33** |
| **SHA-256 (frozen)** | **`3865FACC776BC64C6B0CFE8DF6C6BD0287DBD6EFC339C4E7FFAA7406667BEC27`** |

## Validation results

### 1. Gate spellings — **VALID**

| Gate | Where it exists |
|---|---|
| **`JSRF_TRACE_A2H_DR`** | `tools/harness/collect.c:49` (collector, cached), `src/recomp_manual.c:59` (terminal witness) |
| **`JSRF_TRACE_A2H_SLOT`** | `src/recomp_manual.c:59` |

**Both are real, present in the built instrument, and OFF by default** (`getenv(...) ? 1 : 0`).

### 2. Exe identity — **OFF CONTROL CARRIES**

| | SHA-256 |
|---|---|
| Archived Run-1 OFF (`…014526-583`) | **`A7E324642A41DF3F9070A0366E4E199CCA6DED34304320BC12CEF24E7ACDEAD9`** |
| **Current build** | **`A7E324642A41DF3F9070A0366E4E199CCA6DED34304320BC12CEF24E7ACDEAD9`** |

**Byte-identical.** So per the packet line 15, the existing Run-1 OFF inertness evidence **carries**, and
**no fresh OFF run is needed.**

**One clarification the Session must state, because the packet's wording could be read too broadly:** the
Session changed **`scripts/a2h-read-registry.py`** after the OFF run (the locator fix below). **That is a
read-side analysis tool and does not run inside the game**, so it **cannot** affect the run's behaviour — the
executable, the toolkit, the collector and the instrument code are all unchanged. **The OFF control therefore
still carries**, and the Session records the extractor's new hash rather than treating a reader change as an
instrument change.

### 3. Target site — **CONSISTENT with the original XBE**

```
00149822  push  dword ptr [esi + 0x580]
00149828  call  dword ptr [0x1c4064]      <== the target site
0014982E  mov   byte ptr [ebp - 0x1d], 1
```

**The call at `0x00149828` returns to `0x0014982E`**, exactly as the packet's target definition states, and
the slot is `0x001C4064`. **Verified from the original XBE, not from generated code.**

### 4. **A REAL DEFECT FOUND AND FIXED — the extractor could not read the gate-OFF archive**

**The packet requires the gate-OFF archive's frozen-registry extraction.** **It failed:**

```
ERROR: the write-watch arming signature is ambiguous (1453754 and 1612683);
       a search-based location is only sound when the signature is unique
```

**Cause.** The extractor located the v3 write watch by **searching** for its own arming record
`(armed=1, alias_count, mapped_mask, protect_mask)` and requiring a unique hit. **That pattern is not
distinctive:** `alias_count == 1` produces **`(1,1,1,1)`**, which occurs **dozens of times** in a 129 MB dump.
In a **gate-OFF** run the true record is **all zeros**, so the only hits are coincidental — **and the
uniqueness guard correctly refused an unsound search.**

**The guard was right; the search was the defect.**

**Fix (`scripts/a2h-read-registry.py`, committed `1195b8e`): derive the location the way `LATCH_OFF` already
is.** The registry header is verified **unique** and `WATCH_OFF` is computed from the C layout, so the watch
offset is **derived from the registry rather than searched for**. Content is now only a **corroborating**
check that must match a state the writer can actually produce:

- **gate OFF** — the watch is **all zero**, the legitimate inertness record;
- **gate ON** — `armed` is exactly 0 or 1, `alias_count` within capacity, and when armed the mapped and
  protect masks **agree** (arming succeeds only if every mapped mirror page was protected).

**A derived location holding anything else still raises**, so the fail-closed property is **preserved** — the
check moved from *"is the pattern unique"* (unsound) to *"does the derived location contain a state the writer
could have produced"* (sound).

**The old test that expected a distance mismatch died with the search and was replaced by four content-based
tests:** `armed=5` raises, `armed=2` raises, armed-with-mismatched-masks raises, and **all-zero reads as
gate-OFF rather than an error** — the last being the case the old search could not read and **the one this
packet depends on.**

**Verified: all four archived runs now extract** — three v2 archives unchanged, and the **v3 gate-OFF run with
`install_present=False`**.

### 5. Registry version — **the extractor accepts both**

Archived Run-1 OFF is **version 3** (the instrument bumped it), and the extractor reads v2 and v3. The packet's
registry-version pin is therefore satisfiable.

---

## Two Session measurements that strengthen the packet

### The target is the MOST COMMON outcome, so `N = 5` is comfortably justified

Measured across **all 39 archived OOM-line runs**:

| Terminal event | Count |
|---|---|
| **`0x00000000@0014982E` — THE TARGET** | **26 (67%)** |
| `0x00000000@00147DBC` | 9 |
| `0x41200000@00147D36` | 8 |
| `0x00000087@00147D52` | 2 |
| `0x3E800000@00147DE2` | 2 |
| `0x00000001@0018CE73` | 2 |

**The packet already states this and already carries the correct caveat** — that the 39 span multiple
binaries and therefore motivate the bound **without** estimating this build's rate. **That framing is right
and the Session endorses it unchanged.**

### The packet's fail-closed multi-terminal rule is FREE — it discards no target data

The Planner asked the Session to scrutinize its deliberate strictness: *"strict 'unique fatal target'
treatment excludes multi-event sets even if target present."* **The Session measured whether that costs
anything:**

| | Count |
|---|---|
| Runs with the target terminal | **26** |
| **Target runs carrying a competing terminal** | **0 of 26** |
| Non-target runs with 1 / 2 / 3 terminals | 2 / 9 / 1 |

**No archived target-bearing run carries a sibling terminal**, so **failing closed on mixed sets discards
zero target realizations** while still protecting against the case that **does** occur — every multi-terminal
run in the archive is a **non-target** run. **The rule is correct AND non-restrictive**, which is what a
fail-closed rule should be, and it means **K ≥ 2 at N = 5 is not eroded by the rule.**

**Caveat, which the Session records rather than hides:** this is measured across **many different binaries**.
It establishes the rule is non-restrictive **historically**; the successor's own runs decide it, and the rule
must still fail closed if a target run carries a sibling.

### A correction to the Advisor's anchor basis, already carried into the packet

The ruling said the identity-1 prefix is identical *"across all five known runs."* **It is measurable on
four**, because `tid=` was added to the `[KERNEL]` line by the A2h triage diagnostic. **All four give exactly
5555**, so the anchor is **sound and available on every successor run** — but it is **not** cross-historically
confirmed. **The packet states this correctly at line 13** (*"verified on all four runs that print dispatch
`tid=` … the fifth historical run lacks `tid=`"*). **Contrast: the OOM anchors ARE cross-historically
confirmed — identical on 39 of 39 OOM-line runs.**

---

## Freeze decision

**All four of the Planner's stated freeze conditions are satisfied**, one of them only after a **real defect
was found and fixed**. **The packet is `ADEQUATE`, validated, and frozen.**

**Nothing in this validation changes the packet's class (`discovery`), its row set, its `N = 5` bound, its
validity anchors, or its prohibitions.** **No synthetic completion.** The producer line stays **PARKED**;
`PIO_FREE` stays **DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3` stays
**`UNRESOLVED`**.
