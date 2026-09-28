# `A2h-slot-read-path-displacement-r1` — implementation record

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-slot-read-path-displacement-r1`, frozen
**`75E6AE7C9A26A2CF63F9AD19502A170B2960FE2C90523A566F3F382942BB9B6A`**.
**Implemented by:** Worker `b333b530-139e-4a89-bcb4-27ee02802c52` (`workbuddy-ai/deepseek-v4.1-flash` @ `max`),
under the Session's bounded brief. **All four stages complete.**
**Feasibility:** `docs/reviews/a2h-slot-read-path-feasibility-assessment.md` — the packet's STOP condition was
assessed and **not triggered**.

| Item | Value |
|---|---|
| Game commits | `110b544` (stage 1) · `f5b709d` (stage 3+4) · `0e461df` · `3cfde3c` · `93a00d5` |
| Toolkit commits | `07f6b06` (stage 2) · `5528d00` (handshake acknowledgement VEH) |
| **Session-verified build** | **success** (`scripts/build-jsrf.py`, with both trees settled and clean) |
| **Session-verified ctest** | **18/18 passed** |
| **Session-verified guards** | **all eight suites `OK`** + extractor `SELF-TEST OK` |
| **Extractor self-tests** | **43** (was 17), and **both real archives still extract**: Run 2 `install_present=True install_ok=1`; Run 1 `install_present=False install_ok=0` — **the v2 polarity the packet requires is preserved** |
| New gates | **`JSRF_TRACE_A2H_DR`** (new); `JSRF_TRACE_A2H_SLOT` (pre-existing, now also enables the terminal witness) |

**Build note, recorded because it looked like a defect and was not:** an early Session build reported
`link_error` while the log contained **zero** `error LNK` lines, then `Source changed during build`. **Cause:
the Worker was still editing `tools/harness/collect.c` concurrently.** `scripts/build-identity.py` hashes
`tools/harness/*.c` and correctly refused to stamp an identity for a moving source. **Not a code defect** —
re-run after the tree settled and it succeeds. Recorded so a future reader does not chase it.

---

## Three measured defects the Worker found and fixed

**Each would have produced a plausible-looking WRONG result, which is why they are recorded.**

### 1. The handshake was **fatal**

`DBG_EXCEPTION_NOT_HANDLED` resumes the search **inside the target**, where nothing handled `0xE0424452`, so
the process died on its own diagnostic. **Fixed** with a priority-1 VEH claiming only that code (`5528d00`).
The Worker's note is the honest part: *"the first version of this seam died with exit code 0xE04…"* — i.e. it
was **observed**, not theorised.

### 2. The DR6 "mixed status" test was **always true** — a silent false negative

`dr6 & ~1` is **always nonzero** on x86-64, because **a real data breakpoint delivers `DR6 = 0xFFFF0FF1`** —
the reserved bits 4-11 and 12-15 read as `1`. So **every genuine hit would have been logged `MIXED` and never
claimed**, reporting **a silent absence of writes for a run that had one**. That is precisely the failure class
this packet exists to prevent.

**Fixed** with `A2H_DR6_MEANINGFUL = 0x0000E00F` (B0–B3, BD, BS, BT — the architecturally defined bits).
**Session-verified in source** at `collect.c:67,118,281,287,330`. The same mask fixed the **arm-time collision
check**, where reserved bits would have **refused to arm every thread**.

### 3. The terminal witness was gated on the **wrong variable**

It was gated only on `JSRF_TRACE_A2H_SLOT`, so **Run 2 could have armed the write watch and silently omitted
the packet's required terminal record.** **Fixed** (`93a00d5`).

### And one thing validated rather than assumed

**`DR7` reads back `0x0D0401` for a requested `0x000D0001`** — bit 10 is reserved and forced to `1`. **An exact
compare would have reported every correctly armed thread as FAILED.** The readback compares through
`A2H_DR7_OWNED = 0x000F03FF`. **Session-verified** at `collect.c:60,143`.

---

## Fail-closed paths — Session-verified

| Path | Behaviour |
|---|---|
| `dr_arm_thread` | checks `SetThreadContext` **and reads DR0/DR7 back**; any failure increments `dr_failed` and prints `GUEST_DR_ARM_FAIL` |
| DR-owner collision | **refuses to arm** a thread that already has a DR owner rather than clobbering it |
| `GUEST_DR_ARM … ok=0` | makes an unarmed run **visible in `stacks.txt`** |
| `dr_handle_single_step` | services **only** threads in the arm-time tid set; only pure `DR6.B0` |
| alias touch | the toolkit opens an alias page **only** on a successful publication — **record-before-open** |
| `jsrf_slot_watch_alias_armed` | armed **only** when `mapped_mask == protect_mask` |
| install handshake | reports the install **UNARMED** rather than raising into a process that would die |
| registry size | **compile-time assertion** against the extractor's computed size |
| extractor | raises on unknown version, dump-vs-printed version disagreement, wrong watch offset, absent range |

## The Worker's four declared uncertainties — carried forward, not dismissed

**1. `#DB` chance semantics — the most load-bearing unknown.** A first-chance `EXCEPTION_SINGLE_STEP` is
claimed when `DR6.B0` is pure, but **whether a data breakpoint surfaces as first-chance, second-chance, or is
swallowed under this collector could not be tested without running the game**, which the brief forbade.
**If `#DB` never arrives, the canonical install trap will be absent and the run must select `O-COVERAGE` —
never be read as "no write occurred."** `GUEST_DR_ARM` / `GUEST_DR_HIT` make this visible immediately.

**2. The class-keyed canonical-write witness is fed by a gated boundary sampler in the toolkit, not by the DR
handler** — the collector is a separate process and cannot call into the game. **Its provenance is recorded as
`UNKNOWN` by design.** A write occurring entirely within one bridge call with no boundary crossing is therefore
**not** witnessed by the class records; **the DR0 hit is what covers that gap, which is why uncertainty 1
matters.**

**3. The real DR0 arm path was never exercised against the live game** (forbidden). Only its **failure
reporting** is verified; `dr_arm_all`'s behaviour on the real thread set is **unverified**.

**4. The positive-control harnesses live in `%TEMP%` and are not committed.** The alias-census control
(write through mirror 1 → `touches=1 published=1 index=0 va=041D4064`, `canonical_visible=FE000104`, RO filter
rejects reads, re-arm works) was **measured deterministically across three runs** but is **not durable**. **A
checked-in fixture is a follow-up packet.**

**Uncertainty 1 is the first thing to check in Run 2's `stacks.txt`, and the Session has made it a Run-2
acceptance gate rather than a note.**
