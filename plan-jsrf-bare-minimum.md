# Jet Set Radio Future: Windows port milestone plan

This file is the **execution authority**. It owns the current packet, current
blocker, packet status, next action, and milestone acceptance. Do not discover work
by scanning historical documents or old status tables. `docs/agent-workflow.md` owns
roles, the packet lifecycle and escalation; `AGENTS.md` owns operating/build/runtime
discipline; `docs/jsrf-run-profiles.md` owns evidence-profile semantics.

## Previous packet — `A4b2-NR-epoch-slice-followup-r1` **EXECUTED 2026-09-27 → frontier CLOSED on measured evidence; row `O-TWO-LEG`** (superseded; `A4b2-r8` accepted and closed)

**`A4b2-NR-epoch-slice-followup-r1` (`03CE475D…`) executed** with the Advisor-mandated **backward
demand-driven slice**. Evidence: `docs/reviews/a4b2-nr-epoch-slice-execution-evidence.md`. Verification:
`docs/reviews/a4b2-nr-epoch-slice-followup-r1-session-verification.md`.

### The decisive measurement: which descriptor produces the exchange

The slice closed every field-level question but could not settle **which DMA descriptor** produces the
`B+0x810` exchange. Rather than argue it, the Session built the packet-authorized instrument
(`RECOMP_APU_DMA_DESC_TRACE`) and **measured** it:

```
[GPDMADESC] GP_CLEAR produced by block_addr=0018 (dsp_addr=000800)
```

**`block_addr` is decimal → block 24**, the descriptor built by the **`P 000E`** call (builder B,
`P 00EB`). **Every field is an immediate:** control `0x59E2`, count `6` (`P 000D`), dsp_offset `0`
(`P 000A`), **scratch_offset `0x000800`** (`P 000B`). **Neither stub input appears in any field.** Per the
packet's own leaf classification, immediate leaves **CLOSE**.

**THREE independent confirmations, each by a different method:**

1. **Block-identity trace** — the GP_CLEAR latch reports `block_addr=0018`, and block 24 is read **once, at
   ordinal 765** — the last read before the exchange fires at 766.
2. **Fresh-code re-derivation from the raw artifacts**, reusing none of the earlier tooling — six checks,
   all confirming.
3. **Direct measurement of the descriptor's own fields** — `#FIELDS block=24 base=0x0 offset=0x800
   → scratch_addr=0x800`, matching the observed `dsp_addr` **exactly** (the mixbin block gives `0x8000`,
   which does **not** match). This checked a load-bearing assumption (`scratch_base = 0`) that I had
   otherwise been taking on trust.

**Measured chain:** `6 → 0x25 → 0x1E` cycling, 255 doorbell-region reads vs 254 mixbin-region reads,
`events=766 ngp_skipped=0`. **The mixbin descriptor is a different block in the same chain and is not the
one that produces the exchange** — resolving the Session's earlier mixbin concern.

### What closed

| Requirement | Status |
|---|---|
| Five field leaves | **all immediates** → CLOSED |
| Trigger guards + **shared state** | CLOSED — `a` from `P 00C1 move #$000025,a`, masked |
| **Computed readers** | CLOSED — image `I` has exactly **one** (`P 00B9`), `r1` an immediate `0x24` |
| **Computed writers** | CLOSED — all outside every descriptor region |
| **Alias-disjointness** | PROVEN for statically-targeted writers; all 39 direct X writes clear |
| **Entry set** | PROVEN — `P 0000` has **0** incoming edges (reset-only) |
| **Interrupts incl. faults** | excluded **per-vector**, not for being unobserved |
| **Consumer** | **MEASURED** — block 24, all immediates |
| `L2 = INVARIANT` | carried |

### Row: **`O-TWO-LEG` — SELECTED** (`L1 = PROVEN`, `L2 = INVARIANT`)

The Advisor's terminal ruling made this conditional: *"SELECT `O-TWO-LEG` upon V2 clean (Session records it
without re-referral)."* **V2 is clean**, so the Session selects the row and records it, as authorized.

**V2 result** (`docs/reviews/a4b2-epoch-slice-v2-result.md`) — all four directions clean:

| V2 direction | Result |
|---|---|
| **(out)** image-I static transfers targeting ≥ `0x173` | **0** |
| **(out)** computed/indirect transfers in image `I` | **0** |
| **(in)** second-image transfers targeting slice nodes or dead-block PCs | **0** |
| **(data)** second-image direct X writes into a slice-read word | **0** |
| **(data)** second-image computed X writes (296) | **MOOT** |

**The decisive fact:** at the exchange, `gp_pc_range=0000..0172`, `gp_first_high=0000` — **only image `I`
had executed**. The second image is **loaded** before the exchange (the P-write watch shows 3 510 writes
above `0x172` inside the same window) but **not executed** at it. **Loaded ≠ executed**, cross-checked from
two independent artifacts.

**The Session's first V2 record concluded the opposite and was wrong** — it read the **last** histogram in
the artifact (run-end, post-exchange) instead of the block after the `first-exchange` terminal. **Seventh
self-caught error**, recorded in the V2 result. It produced a false `UNKNOWN` that would have triggered an
unnecessary re-referral.

**What this is:** `L1 = PROVEN` **for this discovery only**, with `L2 = INVARIANT` carried.
**`A4b2-r7` stays `R2-EXPL-INPUT`** — no retroactive PASS. **No strict criterion is discharged**; this is
discovery acceptance (artifacts + row selection).

**Next authorized action:** **`A4b2-r8`** change revision, carrying the Advisor's Q3 list: (1) the two-leg
outcome + the conditional-ruling citation; (2) the **block-24 identity with corrected radix**, so the wrong
identifier dies here; (3) the leaf table as the input-qualifier restatement basis; (4) boundary statements —
second-image **interface** enumerated per V2, **internals unanalyzed and unneeded** (state so, so a reader
does not mistake scope for oversight), B3-ID still deferred. **Then `PIO_FREE`** — the Advisor confirmed the
sequencing is unchanged. **A fifth `L1` packet is forbidden (`F-D`).**

**Advisor-mandated record obligations, outstanding:** archive the fresh-code re-derivation (script, inputs,
outputs, hashes) before `A4b2-r8` cites it; **forbid** citing region-counter tallies for block identity;
`A4b2-r8` must not depend on DMA chain-restart theory.

**Toolkit:** pushed `c151d4e`. **Game:** `c493fdc`.

---

## CURRENT PACKET — `A2h-dr0-delivery-gate-r1` (**discovery**: repair or retire the DR0 observation leg) — **PROMOTED 2026-09-28, `ADEQUATE`**

- **Packet:** `docs/packets/a2h-dr0-delivery-gate.md`, **16 lines**, frozen
  **`12A0B68EB550A96DCD4D2393DBC81262AC0CEE2838CC403A9FD99ED18D3BE537`** — **this is the packet to execute.**
  Validation: `docs/reviews/a2h-dr0-delivery-gate-r1-session-validation.md`.
- **Two phases, and Phase 1 GATES Phase 2:** **Phase 1** asks one question — *does a `#DB` hit arrive at the
  install store, a known canonical write under an armed DR0 watch?* **Phase 2 attribution runs ONLY if
  Phase 1 passes.** **DR records are INADMISSIBLE for any row until a live install trap passes.**
- **The losslessness split is the packet's most important line:** a write-once `install_executed` latch **plus
  uncapped counters**, so **`install_executed=1` with a complete raw-event count of 0 means NON-FIRING**, while
  **a missing latch or incomplete accounting means NOT-RECORDED / `UNKNOWN`.** **A software install comparison
  alone NEVER passes.**
- **Bounded: ONE packet only.** No verified install `#DB` by closure, unrepairable handling, or irreconcilable
  evidence ⇒ **STOP → Advisor**. *"No serial speculative repairs."*
- **THE DROP CEILING, stated explicitly:** if the leg is dropped, **census + software reads + install software
  control stand**; **no row reads DR records in either direction**; **canonical-write attribution and the DR leg
  of read-path become permanently unreachable on this line.** *"Never carry the channel as decoration."*
- **Declared write scope:** toolkit `kernel_bridge.c` (install-site witness / DR context handling); game
  `tools/harness/collect.c` (raw delivery / continuation / lossless ledger) and `scripts/test-harness.py`
  (fixtures). **No other source without new authorization.**
- **The Session stress-tested the finding's load-bearing link before freezing:** the zero-`0x80000004`
  inference needs the collector to print **every** exception event, and **`collect.c:1156` prints at the top of
  the branch, before any filtering** — the `EXCEPTION_SINGLE_STEP` check is at `:1163`. **The inference holds.**
- **Most informative new measurement the packet requires:** a **DR6/DR7 and thread-context readback on the far
  side of `ContinueDebugEvent`** — new, because **a context that reverts across the continue would explain zero
  hits while every existing check passes.**

**Next:** execute the packet (fixtures → fresh OFF if collector changes invalidate the carried one → one
bounded strict ON trial → the C2 gate → §5.8 acceptance).

**Toolkit:** `571982d` (pushed). **Game:** `00646db`.

- **Stage-1 review:** `docs/reviews/a2h-arming-coverage-repeat-acceptance-review.md` (`6a0eb11`) — **all eight
  criteria pass on independent verification.** **The EXECUTION is clean; the corrections concerned the row
  framing**, and all four are applied.
- **`K = 3`** = **reproducibility** of the terminal triple, the install control and the census across three
  independent realizations. **It does not buy a certified writer row.**
- **ROW `O-COVERAGE` → `A2h-slot-write-coverage-provenance`.** The Planner **withdrew its own `O-READ-PATH`
  ruling** on the DR0 `PREMISE_CHANGED`, and its citations are exact: the predecessor packet
  (`a2h-slot-read-path-displacement.md:27`) **already required the install write to cause a native DR0 record**
  and said the software `install_ok` **"cannot itself count as a trapped-install witness"**; and
  (`a2h-slot-within-run-attribution.md:19`) ***"A missing canonical install trap is `O-COVERAGE`, never
  no-write: live `#DB` chance semantics and all-thread arming have not yet been proven in the game."***
  **The frozen contract predicted this outcome and prescribed the row in advance.**

- **Packet:** `docs/packets/a2h-arming-coverage-repeat.md`, **15 lines**, frozen
  **`4AC455E7F427ABCD9AEB6A81FA34B2A570A826964A6FB76EC3478ADA4B1174A1`** — **not edited**.
- **Identity verified BEFORE any ON run:** `jsrf_recomp.exe`, `jsrf_collect.exe` and `default.xbe` all hash
  **byte-identically** to the pinned values, and source deltas since `d369880`/`571982d` are **empty** for
  `src/`, `tools/`, `CMakeLists.txt`. **So the OFF control CARRIED and no fresh OFF was taken.**
- **Five ON runs.** **Run 1 is `diagnostic_deadline` (exit 3), NOT ANCHORED** — capture bounded before any
  fatal terminal, so it never reached the OOM. **Excluded as not a realization, not scored a NON-TARGET.**
  **Runs 2 and 5 are TARGETs** (`0x00000000@0014982E`); runs 3 and 4 NON-TARGET (`0x00147D36`, `0x00147DBC`).
  **All anchored runs coverage-complete:** 17/17 arms, overflow 0, census 28/28 zero touches, install control
  positive, zero DR hits.
- **`K = 3`** — the pinned r2 run 2 supplies K=1 (Advisor K-scope ruling (i)) plus two new coverage-complete
  TARGETs. **K ≥ 2 MET for the first time in this line.**
- **SUPERSEDED — the Planner's first ruling was `O-READ-PATH`, and it was WITHDRAWN** when the DR0 positive
  control failure showed that **leg 3's DR half had never certified anything.** The Planner's own words: *"I
  incorrectly treated software `install_ok` as a positive DR firing control. Aliases + reads remain certified
  but cannot satisfy the required canonical/DR leg."* **The `O-READ-PATH` text is retained here only as
  history; the operative row is `O-COVERAGE` at the top of this section.**
- **The Session escalated rather than deciding, and the Planner's first decisive point was one the Session had
  missed:** *"Requiring a terminal-gap DR hit to select the expressly no-zero-write-observed audit row makes
  that row unreachable in its intended case."* **That reasoning was sound about the row's own gate — but it
  turned out to rest on a false premise about the instrument, and the Planner reversed on the DR0 finding.**
  **The lesson survives the reversal and is worth keeping: a gate that would make a row unreachable in its
  intended case is worth re-reading, but re-reading it is not a substitute for testing the instrument the row
  depends on.**
- **DURABLE WORDING REQUIREMENT for future packets:** state that *"no-zero-write"* means **no zero-write
  OBSERVED WITHIN POSITIVELY CERTIFIED COVERAGE**, and that **post-hit reconciliation is conditional on a
  hit.** The Session was the **second** reader to conflate the row predicate with its gate.
- **GENERAL LESSON:** *a positive demonstration that observation channels were continuously available makes
  "nothing observed on certified channels" NARROWER than "nothing happened."*

### ⚠ POSITIVE CONTROL FAILURE — the DR0 write watch has NEVER been observed to fire

**Found by the Session while scoping the fix packet. Record: `docs/reviews/a2h-dr0-positive-control-failure.md`
(`61f813c`). This is the most consequential finding of the A2h line.**

**The toolkit's install store is a KNOWN canonical-slot write under an armed DR0 watch, and it should have
produced a `#DB` hit. Every link verified:**

| # | Link | Evidence |
|---|---|---|
| 1 | the handshake (arms DR0) precedes the patch loop | `kernel_bridge.c:9395` before `:9411`/`:9468` |
| 2 | the loop reads the slot | `:9412 current = BRIDGE_MEM32(va)` |
| 3 | **the loop WRITES it UNCONDITIONALLY** | **`:9468 BRIDGE_MEM32(va) = synthetic;` — OUTSIDE the trace gate at `:9461-9467`** |
| 4 | the write is to the **watched host address** | guest `0x001C4064` → host `0x1D4064` |
| 5 | **DR0 watches it as a 4-byte WRITE watch** | `dr0=…1D4064`, `DR7=0x0D0001` = `L0=1`, `R/W0=01` (writes), `LEN0=11` (4 bytes) |
| 6 | the store runs on the **armed** thread | `install tid=43016` = handshake tid = guest identity 1 = armed |
| 7 | the store is **after** the arm | log line 50 ack, line 52 install |
| 8 | **NO HIT** | `GUEST_DR_DISARM … hits=0`; `GUEST_DR_HIT` zero in **every** run |

**⇒ zero DR hits CANNOT be read as "no write occurred" — they are equally consistent with a watch that
cannot fire.** This is the Advisor's flagged uncertainty #1 arriving as a **positive result**, not a caveat.

**NOT invalidated:** the **28-alias census** (page protection, process-wide, **no DR dependency**), the
**mapping stability**, the **two software zero reads**, the **install control**, and the **arming records**
(`DR7` really read back `0x0D0001` — the registers WERE programmed; the watch was armed and never fired).

**INVALIDATED:** every statement resting on *"zero DR hits = no canonical write"*, including the Session's own
phrasing. **The certified surface is now: 28 aliases + mapping + two software reads. The canonical-address
write channel is instrumented-but-never-demonstrated.**

**`PREMISE_CHANGED` for the `O-READ-PATH` ruling** — referred back to the Planner (§4.3); the stage-1
reviewer has been told to incorporate it rather than review against the pre-finding text.

**Constructive:** the install store is a **built-in positive control that has been present all along and was
never recognised as one.** So: **make DR0 firing a REQUIRED positive control** — a `#DB` hit at the install
store must be observed, or the DR leg is declared non-functional and every DR-based claim is `UNKNOWN`. **If it
cannot be made to fire, drop the leg rather than carry a channel that certifies nothing.** Leading candidate
cause, already flagged in the implementation record: **a data breakpoint under `DEBUG_ONLY_THIS_PROCESS` may
surface as second-chance or be swallowed by the debugger's own handling.**

**Next:** stage-1 acceptance (with the finding incorporated), then the Planner's re-ruling on the row scope,
then the fix packet — which now has a concrete, cheap first task: **make the install store fire the watch.**

**Toolkit:** `571982d` (pushed). **Game:** `6cc67a2`.

- **Packet:** `docs/packets/a2h-arming-coverage-repeat.md`, **15 lines**, **5552 bytes**, frozen
  **`4AC455E7F427ABCD9AEB6A81FA34B2A570A826964A6FB76EC3478ADA4B1174A1`** — **this is the packet to execute.**
  Validation: `docs/reviews/a2h-arming-coverage-repeat-r1-session-validation.md`.
- **It is a DELTA:** the r2 design repeats **verbatim** (Advisor: *"the design didn't fail — the yield did"*).
  **Four deltas only:** **no fresh OFF** (reuse `…030751-407`, gated on the pinned binary hashes
  `a7e32464…dead9` / `95440ec4…b3ef7`; **mismatch ⇒ `O-IDENTITY`/STOP**); **run 2 is the named comparator**,
  pinned by **artifact hash**; a **K-scope anti-fishing guard**; and the **stopping point fixed in advance**.
- **K-SCOPE RULED (i): RUN 2 COUNTS toward K.** It is same-instrument/same-build; **set-A does not** (different
  instrument). The Planner implemented the stricter reading and **flagged it rather than deciding silently**;
  the Session **escalated**; the Advisor ruled the stricter reading *"imports a restriction the text doesn't
  contain, at zero protective value."* **Early stop: the first new coverage-complete TARGET agreeing with run 2
  ⇒ K=2, general certified row, stop.** Disagreement ⇒ report both + `UNKNOWN` generality + re-refer, **no
  majority-seeking**.
- **Agreement is MECHANICAL:** same independently certified writer class/row, compatible mechanism/event order,
  **terminal triple** (slot `0x001C4064`, target zero, site `0x0014982E`), assessed from **both raw artifacts**,
  **re-assessed at adequacy AND acceptance.** **Shared silence is insufficient.** Run 2's `UNKNOWN` is **not**
  upgraded by resemblance.
- **STOPPING POINT:** K<2 ⇒ **re-refer for pivot-or-defer with 10+ qualifying runs on record.** The `0x001D5078`
  pivot is **queued, not concurrent**.

**Toolkit:** `571982d` (pushed). **Game:** `f391fc1`.

---

## Previous — `A2h-arming-coverage-attribution-r2` **ACCEPTED** (`ACCEPT`, all criteria `AGREED`, `BLOCKING: NONE`); **successor ruled: ONE MORE N=5 run of the same design**

- **Accepted packet:** `docs/packets/a2h-arming-coverage-attribution.md`, `A2h-arming-coverage-attribution-r2`,
  **23 lines**, frozen **`80E9425977547BC1EE44DD95645F72ACF4F7941857A39B6DB67B427E7DD81D84`**.
  Review: `docs/reviews/a2h-arming-coverage-acceptance-review.md` (`a7a6757`). **Stage-1 `ACCEPT` is final; no
  stage 2.**
- **Outcome `O-COVERAGE` on K ≥ 2** — observed TARGETs **1/5, K = 1**. **A YIELD shortfall, not a coverage
  failure:** all five runs are coverage-complete.
- **✅ The arming fix WORKED.** **17 `GUEST_DR_ARM_OK` per run — 10 `why=handshake`, 7 `why=create_thread`** —
  where the old record showed only 10. **Seven arms had been invisible all along.** Verified three independent
  ways by the reviewer. **Zero `GUEST_DR_HIT`** in all five. Fresh OFF **record-level inert**.
- **A latent break found and fixed (the Session's):** `scripts/test-harness.py` pinned the registry version at
  `1` in two ways; the version moved 1→2 (`009f624`) then 2→3 (`f5b709d`), so **it had been failing since
  `009f624` — before this packet — and no guard runs it.** Fixed (`2c8765c`); **19/19 harness probes pass.**

### ADVISOR RULING — `docs/reviews/a2h-null-line-yield-advisor-ruling.md` (verbatim)

**(a) once, bounded — then the decision tree forks. (c) queued behind it. (b) rejected. (d) not now.**

- **One more `N = 5` packet, r2 design repeated verbatim.** *"The design didn't fail — the yield did (66% hit
  chance at p≈0.4); one bounded repeat is the cheapest path to a decision."* **Keep all anti-fishing structure**
  (pre-specified N, early-stop only on agreeing K≥2, agreement rule).
- **No code changes ⇒ same build (`a7e3246`, hash-verified) ⇒ the existing OFF inertness evidence CARRIES. No
  fresh OFF.**
- **The 40% figure is a PLANNING INPUT ONLY.** *"K counts only r2-qualifying realizations — set-A targets stay
  leads, never K. Do not let '4/10' migrate into any K claim."*
- **(b) raised yield REJECTED:** *"nothing predicts the terminal (anchor 5555 in every run of both classes —
  measured, not assumed), so yield cannot be raised honestly; pinning the failure would change the system under
  test."*
- **(c) QUEUED WITH TRIGGERS:** **MISS (K<2 after the successor) → pivot to a `0x001D5078` packet; HIT → fix
  packet → reassess siblings after.** *"The sibling line does not advance concurrently — one critical path,
  ordered."* **The pivot condition as written does NOT fire** — 6 non-targets across 5 distinct terminals is
  **fragmentation, not dominance**.
- **Q4 — run 2 is NAMED** as a run-local record: `docs/reviews/a2h-run2-target-run-local-record.md`.
  **Coverage-complete TARGET, zero DR hits, zero alias touches, terminal zero, mapping re-proved — consistent
  with read-path, writer `UNKNOWN`, generality `UNKNOWN`.** It is the successor's **agreement comparator**.
- **STOPPING POINT STATED IN ADVANCE:** *"If the next packet also misses K≥2: re-refer for pivot-or-defer with
  10+ qualifying runs on record — that is the natural stopping point for the NULL line as framed."*

### QUEUED SECOND — `0x001D5078` (characterised by the Session, not yet a packet)

**`0x001D5078` lies in `.rdata` and the bytes are a FILENAME STRING — `"djv000_0.adx"`**, neighbours
`effect_006.adx` / `effect_005.adx`. **The guest performs an indirect call whose target is a DATA address
holding an ADX audio filename**, and `eax/ecx/edx/esp` are **byte-identical across two independent
realizations**, so the pointer source is deterministic. **3/10 of same-build terminals.** **Recorded as a
characterisation, not a cause.** *Lead (not finding): the float-bit terminals `0x3E800000`/`0x41200000` suggest
a garbage-pointer **family** — a future packet checks for shared mechanism but classifies per terminal first.*

**Next:** Planner authors the repeat-verbatim successor; Session validates, freezes, runs up to 5 ON (no fresh
OFF), then §5.8 acceptance.

**Toolkit:** `571982d` (pushed, `origin/main` verified). **Game:** `d369880`.

- **Packet:** `docs/packets/a2h-arming-coverage-attribution.md`, revision `A2h-arming-coverage-attribution-r2`,
  **23 lines**, frozen **`80E9425977547BC1EE44DD95645F72ACF4F7941857A39B6DB67B427E7DD81D84`** — **not edited**.
- **Runs:** one **fresh OFF** + **five ON**, same build, all **STRICT**, identical anchors, install control
  positive in all five, census **28/28** in all five.
- **Classification: run 2 is the ONLY TARGET** (`0x00000000@0014982E`). Runs 1 and 5 NON-TARGET (unresolved
  `0x001D5078`); runs 3 and 4 NON-TARGET (`0x00147DE2`, `0x00147D36`). **Observed TARGETs 1/5, K = 1.**
- **Row `O-COVERAGE` on the K ≥ 2 requirement** — the packet requires K ≥ 2 for general attribution, and K = 1
  *"preserves only its run-local row and `UNKNOWN` generality."* **No attribution row selected. No sixth run.**
- **NOT a coverage failure — a YIELD failure.** **All five runs are coverage-complete.** The instrument now does
  what the packet asked; **only one of five draws was a TARGET.** The previous five draws gave three. **That is
  the terminal nondeterminism, and it is why N was bounded with an anti-optional-stopping rule.**

### ✅ THE ARMING FIX WORKED — and it settles what inference could not

| Record | Value, **all five runs** |
|---|---|
| `GUEST_DR_ARM_TERMINAL` | `arms_recorded=17 armed_at_or_after_handshake=17 arm_attempt_failures=9 create_thread_events=16` |
| `GUEST_DR_ARM_RECONCILE` | `arm_tid_list=17 arm_tid_overflow=0 disarm_cleared=17 distinct_armed_tids=17` |
| **`GUEST_DR_ARM_OK`** | **17 per run — 10 `why=handshake`, 7 `why=create_thread`** |

**Seventeen threads were armed all along, and SEVEN of those arms were post-handshake and completely invisible
in the old record.** The old `armed=10` was exactly the handshake snapshot. **The Session had twice failed to
get this number by inference — tid list (undercounts), then `cleared=17` (overcounts). The positive per-arm
records settle it directly**, vindicating the Advisor's ruling. **`disarm_cleared=17` now coincides with the
armed count and the record explicitly refuses to be read as an arm count** — the coincidence is why the guard
exists. `GUEST_DR_ARM_PRE_MAPPING_BOUND` reports `decision=decidable_from_records` with an **EMPTY**
pre-mapping-exit window. **Zero `GUEST_DR_HIT` in all five.**

### A latent break found and fixed — and it was the Session's

**The packet required rerunning the collector harness probes; they FAILED, and not because of this packet.**
`scripts/test-harness.py` pinned the registry version **twice**, both correct only at version 1: the regex
matched literal `version=1`, and it asserted the registry's first dump word `== 1` — **but that word IS the
version.** The version moved 1→2 at `009f624` and 2→3 at `f5b709d`, so **the harness had been failing since
`009f624`, BEFORE this packet**, and nothing surfaced it because **the guard suite never runs it.** **Fixed**
(`2c8765c`) to compare against the version **the collector itself reported** — stronger than a constant.
**All 19 harness probes now pass.** **Process finding: a script outside the guard suite can rot silently.**

**Next:** stage-1 acceptance of this packet — **`ACCEPT`** (all criteria `AGREED`, `BLOCKING: NONE`); then the K=1 outcome and the yield problem go to the Planner (and Advisor if the mechanism is affected). **No extension of N without Advisor referral.**

**Toolkit:** **`571982d`** — **committed and pushed to the fork** (`origin/main` verified equal, `upstream` untouched at `766ecef`). Three accepted packets back it, so the pending-acceptance no-push state has cleared. Push recorded in `docs/reviews/owner-push-policy-xboxrecomp-fork.md`. **Game:** `b0be4fa`.

---

## Previous — `A2h-slot-read-path-displacement-r1` **EXECUTED (Run 1 OFF only) → `O-COVERAGE`**; superseded by the within-run redesign

- **Packet:** `docs/packets/a2h-slot-read-path-displacement.md`, revision
  `A2h-slot-read-path-displacement-r1`, frozen
  **`75E6AE7C9A26A2CF63F9AD19502A170B2960FE2C90523A566F3F382942BB9B6A`** — **not edited**.
- **Instrument: BUILT, TESTED and PROVEN INERT.** Game `110b544`/`f5b709d`/`0e461df`/`3cfde3c`/`93a00d5`;
  toolkit `07f6b06`/`5528d00`. 18/18 ctest; all eight guard suites; extractor self-tests **43**; v2 archive
  polarity preserved. Records: `docs/reviews/a2h-slot-read-path-implementation-record.md`,
  `docs/reviews/a2h-slot-read-path-feasibility-assessment.md`.
- **Run 1 (OFF)** `logs/runs/20260928-014526-583-a2h-slot-write-inert-off`: **gates inert at RECORD level**
  (zero gated lines; collector latch `install_seen=0 install_ok=0 claimed=0 overflow=0 partial=0`), **OOM
  identical** (`598869040`, `0xC0000017`), but **terminal site and auxiliary-thread prefix DIVERGENT**.
  **Run 2 NOT LAUNCHED** — the packet's own gate fired. Evidence:
  `docs/reviews/a2h-slot-read-path-run1-off-evidence.md`.
- **Row `O-COVERAGE` → `A2h-slot-write-coverage-provenance`** correctly selected. **The Advisor then
  WITHDREW that successor scope**, because it predicated a reproducible failure.
- **The decisive finding: the divergence is PRE-EXISTING, not instrument-caused.** Three archived runs share
  the identical `jsrf_recomp.exe` (`ddd7e353e769e073…`) and disagree on the terminal site — `0x0014982E`
  (identity 1) twice, and `0x00147DE2` (identity 4) once. **The divergent run predates the DR/alias
  instrument.** `0x00147DBC` also recurs in a different-binary run, so it is an alternative failure point.
- **Advisor re-ruling (verbatim): `docs/reviews/a2h-slot-read-path-advisor-reruling-after-run1.md`.**
  **Row stands** (run-scoped; cross-run variance cannot falsify within-run observations). **Instrument
  stands.** **The comparison design is abandoned.** **No `DONE:no`** — the instrument exists and the question
  is answerable.

### BINDING VALIDITY GATE — deterministic anchors only, terminal EXCLUDED

**Measured across all five known runs, these are the anchors that DO reproduce:**

| Anchor | Value |
|---|---|
| OOM request | `requested 598869040, used 12715008/50855936` |
| OOM status | `0xC0000017` |
| **Main-thread (identity 1) dispatch prefix through the OOM** | **5555** |

**The terminal event is explicitly NOT a validity anchor.** Anchor mismatch ⇒ `O-COVERAGE` **for that
realization**. **No run serves as a cross-run control for terminal behavior; controls must be structural
(gates-off silence) or prefix-scoped (deterministic anchors only).**

### Successor scope — WITHIN-RUN attribution + bounded repetition

1. **Within-run rows only.** Classify each realization **first by its observed terminal event** (target
   `0x0014982E` raw-zero **vs other**). Attribution rows apply **only** to target realizations with complete
   coverage; **all others recorded in full as contrastive data, never forced into rows.**
2. **Bounded repetition, pre-specified:** **N ON runs, same build** (N ≤ 5, Planner justifies); **need K ≥ 2
   target realizations**; attribute each independently; general claims require agreement — disagreement ⇒
   report both + `UNKNOWN` generality (per-realization findings stand as witnessed mechanisms). **Zero targets
   in N ⇒ report the rate + STOP + re-refer** (anti-optional-stopping; no extension without referral). **This
   supersedes the old two-run bound.**
3. **OFF control:** the existing Run-1 inertness evidence **CARRIES** iff same build + no code changes
   (**verify the exe hash**); a new build ⇒ **fresh OFF**. **No other OFF runs.**
4. **Coverage per realization** (arming census, no gaps, mapping stability, install trap), **read-path three
   legs**, closure discipline: **unchanged, applied per run.**
5. **Second-slot observations** (`0x1C4078`, `0x3E800000`, target `0x1`): **contrastive data only. NO
   second-slot attribution scope** — resist creep. **Pivot condition:** target rare + siblings dominate
   across N ⇒ re-refer for a line pivot.
6. **Routing record:** *"`O-COVERAGE` row named `A2h-slot-write-coverage-provenance`; Advisor redirected
   scope to within-run attribution because the reproducible-failure premise was falsified."* **Coverage
   questions survive as per-realization input checks, not as the packet.**

**Next:** the Planner authors the within-run attribution packet; the Session validates, freezes, then executes
the pre-specified N ON runs.

**Toolkit:** `5528d00` (unpushed — the A2h diagnostic is **pending-acceptance**, so it is a **no-push**
state until this packet is accepted). **Game:** `071e364`.

---

## Previous — `A2h-null-slot-triage-r1` **EXECUTED 2026-09-28 → `O-NO-BOUNDARY-TRANSITION`**, **`ACCEPT`** (stage 1, `BLOCKING: NONE`)

- **Packet:** `docs/packets/a2h-null-slot-triage.md`, revision **`A2h-null-slot-triage-r1`**, frozen
  **`F9A6522E8579AD756701C150A0AF60275DCFF4158705CE5331BE3BF2EA7A9F20`** (80 lines, 32704 bytes) —
  **verified unchanged**, not edited. **Acceptance:** `docs/reviews/a2h-null-slot-triage-acceptance-review.md`
  — all three criteria **`AGREED`**. **Evidence:** `docs/reviews/a2h-null-slot-triage-execution-evidence.md`.
- **Two pre-specified same-build runs**, exactly as required: Run 1 `a2h-null-slot-inert-off` (gates OFF,
  **live inertness control**) and Run 2 `a2h-null-slot-authoritative-on` (gates ON, authoritative).
  **No third run.**
- **The install positive control PASSED exactly:** `raw=80000115 installed=FE000104 index=65` — the
  reviewer re-derived it from `game/default.xbe` directly.
- **The decisive observation:** the slot read **`0xFE000104` at EVERY one of 15498 sampled bridge
  boundaries, across all six threads.** **Zero** samples read zero; **zero** KWATCH change lines; **zero**
  latch transitions. The **last** boundary (`call=#5555 ordinal=294`) read `0xFE000104` and the **very next
  event** is the terminal raw read of `0` — **no bridge call in between.**
- **The per-thread boundary series is COMPLETE**: dispatches == before-samples on all six threads
  (5555/259/2/328/1385/222), every thread's index sequence exactly `1..N`, **no gap, no duplicate, no cap**.
  *(The reviewer discharged the packet's `O-OPEN` "index gap/duplicate within a thread" clause by direct
  measurement — it **does not apply** — and the Session has since added `index_integrity()` to the classifier
  so the clause is tested mechanically and fails closed.)*
- **A Session error the reviewer caught:** the apparent "5 gaps + 595 nested adjacencies" were **raw-log-order**
  counts including **cross-thread interleaving**; **per-thread it is 5 adjacencies and depth 4** on the
  terminating thread, 0 and 1 elsewhere. The **nesting mechanism is real** (a bridge re-enters the dispatcher,
  advancing the counter, so an `after` prints the current number) and the conclusion is unchanged.
- **Row `O-NO-BOUNDARY-TRANSITION`**, selected by `scripts/a2h-slot-triage-classify.py` (**11 self-tests**),
  with `O-BRIDGE`/`O-GUEST` correctly rejected (no zero was ever sampled, so neither an intra- nor an
  inter-bridge zero boundary exists).
- **Refuted offline:** macro mismatch, torn read, static VA displacement, and the toolkit's worker-stack-over-
  static-data class (image ends `~0x00288620`, lowest stack base `0x00780000` — no overlap).
- **Measured coverage limit:** the gap is log lines `34632..34635`; **no `[TRACE]`, `[RECOVERED]` or `[READ]`
  line falls inside it** (nearest 85/251/501 lines earlier), so **no instrument observed the guest code there.**

### Successor: `A2h-slot-read-path-displacement` — **IN PLANNING**, two Advisor shape preflights recorded

**`docs/reviews/a2h-slot-read-path-advisor-shape-preflight.md`** holds **two** rulings:
1. **First (`SHAPE: REDIRECT`)**: DR0 primary, page-guard redirected away — **its instrument is superseded**;
   the Advisor later stated plainly that it *"is wrong for absence claims as specified."*
2. **Corrected (`SHAPE: REDIRECT`)**: **NO-WRITE IS reachable** via **DR0-canonical complete record + 28-alias
   first-touch census, same run, fail-closed throughout.** The Planner's falsification was **accepted**: a
   canonical DR0 misses alias writes (`xbox_memory_layout.c:2251-2257`, `:2362-2366`), and **4 comparators
   cannot cover 29 linear addresses.** Rotated-4 DR is **forbidden for absence**; `xbox_ProtectMirrorsForDebug`
   is **no as a function** (whole 64 MB views, count-only log, **no handler installed** — a tripwire, not a
   trap) but **yes as a technique, implemented fresh** on the 28 specific pages.

**Session errors recorded in this exchange, both caught by the Planner:**
- **A false physics claim in my relay** — I turned the Advisor's true *"page-sharing is moot under DR"*
  (neighbours on one page) into *"DR watches the physical page."* **DR compares LINEAR addresses.** Marked
  **`PREMISE_CHANGED`** per §4.3 and re-referred; the ruling above is the result.
- **A hand-derived alias range wrong by 4×** — I wrote `0x04000000..0x1C000000`; the real last alias base is
  **`0x70000000`** (`g_memory_size` = 64 MB, `29 × 64 MB = 1856 MB` per the run's own log). **Alias VAs must
  be derived live from `g_mirror_views`.** *Eighth instance of the hand-arithmetic pattern in this line.*
- **An unsatisfiable condition** — the ruling's `IsDebuggerPresent` assert would fail **every** run, because
  `collect.c:294-295` launches the target with `DEBUG_ONLY_THIS_PROCESS`. **Adopted:** no *additional/external*
  debugger; verify the **expected** collector attachment and **DR ownership** instead.

**Next:** the Planner expands the packet with the corrected design; the Session validates literal commands and
byte assertions, obtains no further preflight (the ruling says none is needed if corrections are applied
verbatim), then freezes and promotes.

**Toolkit:** **`6f049ce`** — the A2h diagnostic is now **committed and pushed to the fork** (`origin/main` verified equal, `upstream` untouched at `766ecef`), because the packet's acceptance returned `ACCEPT` with `BLOCKING: NONE` and the change is therefore no longer pending-acceptance. Push recorded in `docs/reviews/owner-push-policy-xboxrecomp-fork.md`. **Game:** `9d2774a`.

---

## Previous — `A2h-null-slot-triage-r1` (**discovery**: what zeroed / what read as zero at `0x001C4064`?) — **PROMOTED 2026-09-27, `ADEQUATE`**

- **Packet:** `docs/packets/a2h-null-slot-triage.md`, revision **`A2h-null-slot-triage-r1`**, class
  **discovery**, **80 lines**, frozen SHA-256
  **`F9A6522E8579AD756701C150A0AF60275DCFF4158705CE5331BE3BF2EA7A9F20`** — **this is the packet to execute.**
  Promotion byte-identical with no revision (§5.3); hash read **3× over ~12 s** after the Planner settled
  (an earlier read caught the file **still changing** — the stale-pin hazard, avoided this time).
  Verification: `docs/reviews/a2h-null-slot-triage-r1-session-verification.md`.
- **Shape preflight OBTAINED** (required because it changes the pinned toolkit): **`SHAPE: REDIRECT`** with
  **four binding corrections, all adopted** — per-thread write-once CAS latch keyed by live-read `tid`;
  `tid` augmentation on `[KERNEL]`/`[KWATCH]`; row keying on `(tid, call#)`; **two pre-specified same-build
  runs** (Run 1 gates OFF = live inertness control, Run 2 gates ON = authoritative). Ruling:
  `docs/reviews/a2h-critical-path-advisor-ruling.md`.
- **The question:** what **live** value trajectory led from thunk installation at `0x001C4064` to the terminal
  raw-zero read, and where was the first zero relative to bridge calls?
- **Rows:** `O-BRIDGE` (intra-bridge valid→0, named ordinal) → thunk install/relocation audit · `O-GUEST`
  (earliest-tick inter-bridge) → slot page-guard write history · `O-NO-BOUNDARY-TRANSITION` → slot read-path
  displacement · `O-IDENTITY`/`O-OPEN` → trace-provenance discovery. **Latch-vs-series disagreement → `O-OPEN`.**
- **Exp1 is ALREADY DONE** by the Session (offline, zero new runtime code):
  `docs/reviews/a2h-null-slot-triage-exp1-evidence.md`, tool `scripts/a2h-null-slot-triage.py` (**17 tests**).
  Key results: **the index is per-thread** (`kernel_bridge.c:316` `RECOMP_TLS`, source-proven) so the Advisor's
  `#5551`/`#5553` "gap" is **per-thread restart, not loss**; budget **100000 from metadata**, headroom ~93.5k,
  **neither log truncated**; R1 and baseline have **different terminal forms on different tids**
  (`invalid target` tid 65356 vs `Failed to resolve 0xFFFFFFFF` tid 57592) so the baseline is an **earlier
  terminal boundary, not survival**; and **the archived `[KERNEL]` lines carry no `tid`**, so archived
  per-thread windows are **not reconstructable** — which makes `tid` augmentation a **precondition**.
- **Artifact extraction PROVEN — no scope revision needed.** The Planner made promotion conditional on this.
  `tools/harness/collect.c:207` resolves `g_jsrf_debug` **by symbol** and reads **`sizeof(JsrfRegistry)`** into
  the archived dump; **both runs already contain** `GUEST_REGISTRY version=1 claimed=5 overflow=0` with 5
  `GUEST_THREAD` lines. **So the latch extends a struct the collector already archives losslessly** — exactly
  §6.1.6's "lossless by construction", and the packet's write scope suffices.
  **Hard precondition the Session flags:** `JsrfRegistry` is **`version`-tagged** (`diagnostics.h:22`) and the
  collector prints that version, so **adding a latch field REQUIRES a version bump** and a readout that refuses
  an unknown version — otherwise collector and latch silently disagree about the layout.
- **A Session error, corrected:** the Session proposed carrying the authoritative record on the archived
  `jsrf_run.log` and dropping the latch. **The Planner refused with citations, and was right** —
  `docs/agent-workflow.md:794-798` requires decision inputs to be **lossless by construction**, and capped logs
  are **observation only**. **The Session had confused *archived* with *lossless*; the proposal is withdrawn.**
  `KWATCH` is **observation-only for the same reason** — no row may rest on the **absence** of a KWATCH sample.
- **Commands validated (§5.1.5):** the four named suites run **102 tests OK**; `--self-test` OK; both
  `check-run-profile` calls work (**STRICT** / **EXPLORATORY**, matching the packet's stated profile
  difference); both `check-dump-mapping` calls report **`content-mismatch`**, confirming the packet's
  prohibition on using either dump for image-content claims; `a2h-frame-audit.py verify` = **ALL VERIFIED**;
  and **`JSRF_TRACE_A2H_SLOT` does not exist yet** — it is the packet's own deliverable.
- **Input correction:** the Advisor's ruling spelled the contrast archive `…-a2b-nr-baseline`; **the real
  directory is `…-a4b2-nr-baseline`** — the Planner caught this and the packet uses the exact name.
- **Forbids:** no synthetic completion; no generated-code edits; **`PIO_FREE` DEFERRED**;
  `A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened; `0xFFFFB3` `UNRESOLVED`; **P4's discovery-transfer bridge must be
  re-established before any P4 inheritance**; **no third run** without Advisor re-referral.

**Next:** implement the gated diagnostic under the four binding corrections (**including the `version` bump**),
then Run 1 (gates OFF) → Run 2 (gates ON) → §5.8 acceptance.

**Toolkit:** `c151d4e` — **a bounded diagnostic change is authorized by this packet** (install loop
`:9198-9254`, dispatch/watch seam `:8944-9094`, `recomp_diagnostics.h`, game `diagnostics.c`/`.h`, and two
gated fields on the existing `[ICALL]` print in `recomp_manual.c:35-43` with `RaiseException` untouched).
**Game:** `50eaa00`.

---

## Previous — `A2h-named-producer-frame-r1` **ACCEPTED 2026-09-27** (`ACCEPT`, all criteria `AGREED`; row `O-OPEN`) — frame and caller BOUND; the failing allocation's read value is not in the artifact *(this supersedes the "EXECUTED → O-OPEN" block further down: same row, now accepted with three corrections applied)*

**`ACCEPT`** — stage 1, all three criteria `AGREED`, **no other row better supported**. Review:
`docs/reviews/a2h-named-producer-frame-acceptance-review.md`. **Three required corrections applied and
re-swept** (commit `379be56`); none changed the row. **Frozen packet `2333B652…F88887` verified unchanged.**

**The reviewer independently reproduced the frame arithmetic** (`E = 0x00F7FEA0`, `ebp = 0x00F7FE9C`,
`arg2 = 0x00F7FEAC`), **recomputed the row from the packet's own lines 26-31**, confirmed **synthetic
completion absent**, and confirmed the **frozen packet / toolkit / generated code unchanged**.

**What the reviewer caught that the Session did not — all three verified before applying:**
1. **A tool bug that produced a wrong number in an accepted record.** The Session claimed *"12 of 13 call
   sites pass three arguments; one passes two."* **It is 13 of 13.** `_safe_start` scanned backward for a `C3`
   byte and hit one at `0x0016B8EB` — the **ModRM byte of `add ebx,0x12` (`83 c3 12`)**, not a `ret` — so the
   decode swallowed a `push` and under-counted. **The fix uses the recompiler's own `loc_` labels as the
   boundary set**, in `gen/` **and** `recovered/`, with **four regression tests** (29 tests OK).
2. **A wrong pre-image interval and an unswept self-contradiction.** `V(0x4C000011) = 0x4C000030`, so the true
   pre-image of the crash's `eax` is **`[0x4C000001, 0x4C000010]`**; the Session's line also asserted
   `0x4C000010 + 0x20 = 0x4C000020` when it is `0x4C000030`, **contradicting its own earlier correct line.**
3. **An over-broad dead-slot scope.** The slot is not a witness for the **failing** activation's `arg2` — it
   **is** live caller argument space holding the **crash** activation's.

**The reviewer also supplied corroboration the Session had missed:** the **dump layout**, where `E−16`,
`E−12`, `E−8` and `E` hold exactly the predicted constants and return address. **The Session's "three
independent facts" was an over-claim** (two were one relation displaced a slot, the third reused the first's
`esp`); the dump layout is the genuinely independent check and the record now rests on it.

**Lesson recorded:** the misaligned-decode failure mode **survived into an accepted record** — the tool's
`verify()` was fail-closed but its **resynchronisation heuristic was not**. Fail-closed verification is only
as good as the boundary you start from.

---

## Earlier — **Critical path MOVED by Advisor ruling: the `A2h` producer line is PARKED.**

> ### Advisor ruling (2026-09-27): the critical path moved to `0x001C4064`
>
> **Recorded verbatim:** `docs/reviews/a2h-critical-path-advisor-ruling.md` (handle `muse_FkNhGaXtV9P5`,
> `muse-spark-1.3-contributor`, `max`).
>
> **The premise several packets rested on is FALSE: the ~571 MB OOM does not kill the run.** The guest
> **checks** it (`00149E56 test eax,eax` / `00149E58 jl 0x149eec`; `0xC0000017` is negative, so the branch is
> **taken**) and returns cleanly through `__SEH_epilog` carrying `STATUS_NO_MEMORY`. **The OOM is HANDLED.**
>
> **The actual terminal event:** `call dword ptr [0x1c4064]` at `0x00149828` read **`0`**;
> `RECOMP_ICALL_IS_CODE` rejected it as non-code and raised `0xE0424943` **NONCONTINUABLE**. **The original
> XBE holds `0x80000115` there — a valid ordinal-277 kernel thunk** — and the toolkit patches that table at
> runtime. The NULL call is near the **top** of the function; the ordinal-184 call is near the **end** —
> **different passes.**
>
> **The lapse, named by the Advisor:** an untested lead **hardened into accepted fact**, and the reasoning was
> circular — the bare sequence (`0xC0000017` → NULL ICALL → `0xE0424943`) was cited as *evidence* for "no
> check", and "no check" then explained the sequence. **The packet itself had warned this inference "must
> itself be tested, not assumed"** (`a2h-oom-causal-slice.md:16`). This is a **compliance failure with
> existing discipline** (§2.4.1 observed/inferred labelling; §2.4.2 cheap direct checks) — **not a policy
> gap, so no new policy**, but it **binds the next packet**.

### `A2h` producer line — **PARKED, not retired**

**Reactivation conditions (either one reopens it):** (i) the NULL investigation shows the slot death is
**downstream of allocation-failure error handling**; or (ii) a later gate **needs the size explained** after
the NULL issue resolves. **The byte-identical-35-runs anomaly stays unexplained and stays recorded.** **The
producer work product transfers intact** — same function: frame/caller/ABI binding, the no-writer proof, the
call-site census, and the tested tools **all directly serve the NULL analysis**.

**Superseded:** `docs/packets/a2h-live-oom-arg2-bridge.md` (draft, never promoted). Its seam analysis is
**validated and reused**, but its framing inherits the OOM premise the Advisor ruled against; **it is not the
next packet.**

### Next authorized work — a **NULL-slot triage discovery**, two phases, per the Advisor's Q2

**Discovery, no behavior change.** Exp1 is **offline log mining at zero new code**; Exp2 uses the
**existing** `RECOMP_KERNEL_WATCH` gate, also zero new code. **A new toolkit instrument is NOT first** —
that is the Advisor's explicit redirection: decide toolkit-vs-guest-vs-neither **before** building anything.

- **Exp1 (offline, zero new code):** OOM-vs-NULL ordering + last-N-ICALL context; **count provenance** —
  the log's cap and loss accounting (`RECOMP_KERNEL_LOG_BUDGET` vs actual lines, ordinal continuity incl. the
  `#5551`/`#5553` gap) and a **tool-computed** dispatch count from a **named artifact**; the **thread check**
  (`tid=` is already in every ICALL line); the **nr-baseline contrast** (`20260927-130036-879-a2b-nr-baseline`
  — establish comparability first); and backward-edge exclusion for the faulting region.
- **Exp2 (`RECOMP_KERNEL_WATCH=0x1C4064`, existing gate):** samples the VA either side of every bridge call,
  names a bridge ordinal that changed it, and distinguishes **bridge-change from guest-side change by timing**
  (`kernel_bridge.c:8921-8937,9026-9094`). Plus a **write-once first-`0`-transition latch** (new,
  off-by-default) as the authoritative transition record, with the KWATCH series as corroboration under a
  completeness check.
- **Outcomes route to named seconds, not back to the Advisor by default:** bridge-named change → toolkit
  install/relocation audit; guest-side change → **page-guard write history** on the slot page from install
  (install writes are the **positive control**; absence of a zero-write = never-zeroed); no transition at
  boundaries → read-path/displacement analysis.

**A toolkit change IS expected for the Exp2 latch and IS proportionate** (Advisor Q3), but it requires the
**Muse Advisor shape preflight before promotion**, and **P4's discovery-transfer bridge must be
re-established before any P4 inheritance**.

### Binding discipline for the next packet (Advisor Q5)

**No hand counts in decision inputs.** Every count must be **tool-computed, from a named artifact, with
positive controls and loss accounting.** Two hand-counts for the same quantity (1177 and 1909) disagreed and
**both are withdrawn as uncitable** — the seventh instance of this project's hand-count failure mode.

**Also recorded:** the terminal-event rule — **attribute a terminal event by reading the instructions past the
failing call, never by temporal co-occurrence.**

---

## Previous — `A2h-named-producer-frame-r1` **EXECUTED 2026-09-27 → `O-OPEN`** (frame and caller BOUND; the failing allocation's read value is not in the artifact)

**Executed, not edited.** Frozen `2333B652…F88887`. Evidence:
`docs/reviews/a2h-named-producer-frame-evidence.md`. **No guest run** — the packet allows one only if the
offline audit fails *and* a safe non-generated seam exists; the audit **succeeded at binding the frame**, but
the seam is **toolkit-side**, outside this packet's write scope. **Acceptance pending.**

### What was established — the frame question is DECISIVELY answered

**The SEH helper REPLACES `ebp`** (the packet required tracing it rather than assuming):
`0017D213 lea ebp,[esp+0x10]` → `ebp = E−4`, where `E` is the callee's entry `esp`. The recompiled helper
**publishes** that frame (`recomp_0004.c:1589 g_seh_ebp = ebp`) and the callee **reads it back**
(`recomp_0003.c:20122`). **So `[ebp+0x10]` at `0x00149800` IS the caller's arg2** — the helper's frame
**aliases the standard argument positions**, which is what `__SEH_prolog` is for.

| Fact | Result |
|---|---|
| `E`, one consistency check | ordinal-277 `esp+0x1A0`, ordinal-184 `esp+0x1B0`, and the post-prologue arithmetic — **all give `0x00F7FEA0`**. *(Originally described as "three independent ways": **an over-claim** — the first two are the same relation displaced one slot and the third reuses the first's `esp`. **Genuinely independent corroboration is the DUMP LAYOUT**, supplied by the reviewer: `E−16`, `E−12`, `E−8` and `E` all hold exactly the predicted constants/return address)* |
| The frame | `ebp = 0x00F7FE9C`, **`[ebp+0x10] = 0x00F7FEAC`** |
| **The caller, BOUND** | **`call@0x0017C921`**, `ret=0x0017C926` — a real `call 0x1497dc` ends exactly there |
| Competing writers | **NONE** — the callee has **0 writes** to the slot across its whole 841-line body (5 reads) |
| Call-site arity | **13 of 13** direct call sites pass **three** arguments — *(an earlier entry said "12 of 13; one passes two": **FALSE**, a misaligned-decode artifact in the Session's own tool, falsified by the acceptance reviewer and fixed with regression tests)* |

### Why the row is `O-OPEN` — a coverage gap, not a defect

**The archived frame is the CRASH activation's, not the failing allocation's:**
`align16(0x4C000010) + 0x20 = 0x4C000020` — **exactly the `eax` in the crash registers** — while the failing
size `0x23B20430` was requested **exactly once**. And the bridge call after the OOM returns to `0x00149F5D`,
**outside** the callee's range, so **the failing activation returned** and a later one **reused the same stack
addresses at the same depth**. Its `arg2` was overwritten. **`[TRACE]` does not help** — the callee is not
traced, and `0x0017C926` appears **0** times as a `from=`.

**The one smallest missing witness:** a **recorded value of `[ebp+0x10]` at guest address `0x00F7FEAC` on the
OOM activation**, before it returned. **The address is known exactly; only the moment is missing.**

### Seam status — the blocking question for the next packet

**`kernel_thunk_dispatch()` (`kernel_bridge.c:8944`, TOOLKIT) is the only seam that sees every bridge call
with `esp`.** The game-side `src/diagnostics.c` observes six fixed store sites, not this read. **This packet
authorized at most a diagnostic-only GAME hook, so the Session stopped rather than editing the toolkit or
generated code** — the owner and the packet agree.

### Session errors caught by measurement in this execution

**Three hand-analyses of the same stack produced three different answers.** My first count said the bound
caller passed **one** argument (wrong — the intervening `call 0x14a838` is a zero-consumption getter, so the
pushes below it survive); my correction then mis-read the slot by double-counting the getter's popped return
address. **Both are now impossible**: `scripts/a2h-frame-audit.py` tracks ESP symbolically, and
`scripts/test_a2h_frame_audit.py` (**29 tests OK**) pins the behaviour. **The tool caught two of its own
author's errors** — a mis-transcribed byte string (`89442410` vs the real `896c2410`), and a linear section
sweep returning **zero** call sites for a target with **thirteen**.

**Toolkit:** `c151d4e` (unchanged). **Game:** `c914448`.

---

## Previous — `A2h-named-producer-frame-r1` (**discovery**: which caller frame supplied the failing word?) — **PROMOTED 2026-09-27, `ADEQUATE`**

- **Packet:** `docs/packets/a2h-named-producer-frame.md`, revision **`A2h-named-producer-frame-r1`**, class
  **discovery**, frozen SHA-256
  **`2333B6523B39866F1AEB9C1BEFFE1CCE42176E35BA3ADB86F8965A265BF88887`** (**37 lines**). **This is the packet
  to execute.** Promotion byte-identical with no revision (§5.3); hash verified **3× over ~12 s**, matching
  the Planner's own report. Verification:
  `docs/reviews/a2h-named-producer-frame-r1-session-verification.md`.
- **Adequacy:** the **writing Planner's** own §5.3 two-question review — **`ADEQUATE`**, `BLOCKING: NONE`,
  `PREMISE_FRESHNESS: BOUNDED`. **Correct for a discovery**; **no shape preflight**.
- **The question:** on the **same failing invocation** (index 93, `ret=0x00149E50`, `esp=0x00F7FCF0`,
  `size=598869040`, `type=0x801000`), **which verified direct-call return PC and inherited frame address own
  the word read at `0x00149800` as `[ebp+0x10]`, what value was actually read, and can a specific caller write
  to that exact address be positively linked to it?**
- **Rows:** `O-IDENTITY` (provenance discovery) · **`O-FRAME-ARG`** (a **proved reaching call-site write to
  that exact address** → named-argument-origin discovery) · **`O-FRAME-OTHER`** (a **specific other witnessed
  writer** → named-frame-writer discovery) · **`O-OPEN`** (no unique link, ambiguity, coverage gap, unsafe
  seam, overflow → one missing-witness/tooling discovery; Advisor referral if bounded observation is
  infeasible). **No row is a fix.**
- **It applies the Planner's verified refutation of the Session's narrowing:** `sub_001497DC` is
  **`fpo_leaf`** reading **inherited `g_seh_ebp`**, so `[ebp+0x10]` belongs to the **caller's frame** and
  **all eight call sites have a frame**. The two three-push sites are **leads, not attribution**; the eight
  must be reconciled against **all indirect/other entries** with an **explicit unresolved entry class** if
  not exhaustive; **calls must be modelled as transfer/return/cleanup, not fall-through**; and
  **`g_seh_ebp` must be traced across the helper `sub_0017D1F8`** rather than assumed preserved.
- **It keeps the interval honest:** `0x23B20410` is the **aligned pre-add** value, **not** automatically the
  exact unaligned input — the possible input interval is **`0x23B203F1..0x23B20410`**.
- **It does NOT inherit the refuted premise:** the Planner states explicitly that the predecessor's
  *"no-trap A2g"* text (lines 7, 9, 16) **is refuted by acceptance and not inherited** — verified.
- **Deliverable:** extend the **existing tested** `scripts/a2h-oom-slice.py` + tests (31 OK), plus at most
  **one** off-by-default `JSRF_TRACE_A2H_FRAME=1` observation-only hook **only if a safe non-generated seam
  exists** — otherwise the row is **`O-OPEN`**, not permission to improvise. **No seam, no run.**
- **Commands validated:** both checkers work on the R1 archive (which is correctly reported
  *"Not usable for an IMAGE-CONTENT claim"*); `run-jsrf.py` accepts the named flags; and
  **`JSRF_TRACE_A2H_FRAME` does not exist yet** — it is the packet's own deliverable.
- **Forbids verified present:** no suppressing the trap or `0x80`, no faking the allocation, no widening the
  arena, no bypassing the NULL call, no guest error-handling change; **`PIO_FREE` stays DEFERRED**;
  `A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened; `0xFFFFB3` **`UNRESOLVED`**; P4 bridge required before any
  toolkit inheritance.

**Next:** execute — the **offline bounded audit** first; a bounded 8-second diagnostic capture **only if** a
safe seam exists; then §5.8 acceptance.

**Toolkit:** `c151d4e` (unchanged). **Game:** `d82f388`.

**`A2h` is ACCEPTED and CLOSED** (stage 2, final). The Planner is authoring the successor packet
**`docs/packets/a2h-named-producer-frame.md`**, revision **`A2h-named-producer-frame-r1`**, class
**discovery** — **currently `Status: draft` and still being written by the Planner child.**

> **No hash is pinned here, deliberately.** An earlier revision of this block quoted a draft hash read
> **while the Planner was still writing**, and the file changed underneath it. **A hash is pinned only at
> freeze, after the authoring agent has settled and the value has been read stable three times** — the same
> promotion discipline this project has had to learn more than once. The packet is **not verified and not
> promoted**, and **no command or path in it has been validated yet.**

**The Planner decided discovery, not change** — *"no cause-specific safe implementation while the
pre-add local's producer remains unknown"* — so **no Muse shape preflight**. Its key finding, which the
Session **verified from the generated source**: because `sub_001497DC` is **`fpo_leaf`** and reads
**inherited `g_seh_ebp`**, the six one-push call sites **cannot** be discarded as infeasible for
`[ebp+0x10]` — that slot is in the **caller's frame**, not necessarily the explicit pushed `arg2`. **So the
packet requires a frame/register/stack identity witness and models call ABI explicitly, treating the two
three-push sites as leads rather than positive attribution.** **That refutes the Session's narrowing**, and
the Session has swept the invalid claim out of the evidence record and the plan bodies.

**Next:** wait for the Planner to settle, **then** verify the packet's exact commands, paths and identities,
**then** freeze and promote (§5.3), **then** execute. **No change packet is authorized** — *"consider a
change only after cause is established."*

---

## Previous — `A2h-oom-causal-slice-r1` **ACCEPTED 2026-09-27** (`O-OPEN`, chain bound to the producer)

**`ACCEPT`** — stage 2, final, every mandatory criterion `AGREED`. Acceptance record:
`docs/reviews/a2h-oom-causal-slice-acceptance-record.md`. Reviews:
`a2h-oom-causal-slice-acceptance-review.md` (stage 1) and `a2h-oom-causal-slice-stage2-acceptance-review.md`
(stage 2, rounds 1–5). **No new guest run, no toolkit change, no runtime change, no instrumentation.**
**Frozen packet `E9CDB1B3…41108C` verified unchanged and untouched through all five rounds.**

**§5.8:** the acceptance confirms artifacts exist, match the commands, and select the recorded row.
**It satisfies no strict criterion** — nothing here says anything works.

### The acceptance took five rounds — recorded as the argument for the stage existing

| Round | Finding | Whose |
|---|---|---|
| 1 | **Banners added, bodies never swept** — the withdrawn claim survived unflagged in **six** places | Session |
| 1 | **A required parser fixture was missing** — it existed only in a docstring; the tool had no disassembly surface | Session |
| 2 | **The sweep's pattern was too narrow** — searched `no-trap`, survivor spelled `NO trap` | Session |
| 3 | **The fix malformed a table row** — a stray `|` left 3 cells against a 2-cell header | Session |
| 4 | **The new guard failed on CORRECT input** — it passed *by luck of the corpus* | Session |

**None of the five was found by the author re-reading his own work** — each came from a reviewer
**replaying a claim against real bytes**. And the guard prompted by round 3 **immediately found a sixth,
pre-existing defect no reviewer had reported**: `jsrf-run-profiles.md:92`, **born malformed at `ad400294`**,
missing its third cell in a durable evidence-rules document.

**Three failure modes, each now guarded rather than remembered:** (1) a correction banner is not a
correction — sweep the bodies; (2) a sweep is only as good as its pattern — enumerate the **claim**;
(3) a change applied to the sentence in front of you can break the structure it sits in — hence
`tests/test_markdown_tables.py`, a **machine check**, now correct **by construction** and verified to still
catch both real defects.

### What was established

| Finding | Witness |
|---|---|
| **The failure predates the A4b2 trap work** | Different exe (`2cd0472a256e9d` vs `bc8e288dd54d`), five days earlier, **all 94 invocation sizes identical index-for-index**, same size/type/OOM tuple/terminal ICALL. **Not trace-caused.** **The trap is NOT shown to be unnecessary** — **all 35** runs carrying the request are trapped |
| **The chain is bound to the producer** | `RegionSize = align16([ebp+0x10]) + 0x20`; producer **`0x0014980E`**; value from **`[ebp+0x10]`, the 3rd argument**; **reproduces both observed sizes exactly** |
| **Dominance, doubly derived** | Two independent CFG methods agree: 46 reaching, **2 writers**, single entry, call **unreachable** with writers removed; the `movzx` writer **never reaches the call** |
| **The arena and toolkit behaved correctly** | `alloc_type 0x801000` is `MEM_COMMIT`, **no `MEM_RESERVE`**; bridge returns `0xC0000017`; the guest then calls through NULL |

### Next authorized work

**An `A2h-named-producer` discovery** naming `[ebp+0x10]` of `sub_001497DC` at `0x0014980E`, with **the
caller's identity** as its remaining question. **The bounded caller lead was CORRECTED by the Planner and the
correction is verified:** `sub_001497DC` is **FRAMELESS** (`Frame: fpo_leaf`; `ebp = g_seh_ebp`), so
**`[ebp+0x10]` is a slot in the CALLER's frame, not necessarily the callee's pushed `arg2`** — **every one of
the 8 call sites has a frame and can in principle supply it**, so the two three-push sites are **leads, not a
candidate set**. The follow-up must require a **frame/register/stack identity witness**. **Whether any
particular site is the failing path is NOT established.** **No change packet is authorized** — *"consider a
change only after cause is established."*

**Advisories carried forward:** the guard's **unclosed-backtick** latent edge case (zero current exposure;
wire the predicate in if widened); and **three genuine malformed rows outside the guard's scope** at
`a4b2-r7-execution-evidence.md:54-56`, in an accepted packet's evidence, where a missing cell means **a column
of measurements is silently absent**.

**Toolkit:** `c151d4e` (unchanged). **Game:** `a19052c`.

---

## Previous — `A2h-oom-causal-slice-r1` **EXECUTED 2026-09-27 → `O-OPEN`**, chain bound to the producer; **acceptance: five rounds → `ACCEPT`**

**Row as selected: `O-OPEN`.** Evidence: `docs/reviews/a2h-oom-causal-slice-evidence.md`. Binding:
`docs/reviews/a2h-oom-causal-slice-binding.json`. **No new guest run was needed or performed**, **no toolkit
change, no runtime change, no instrumentation added**.

### Acceptance history — recorded honestly, because it is the process working

**Stage 1 (Hy4) returned `NOT ACCEPTED`** with two mandatory criteria `DISAGREED`. The reviewer reproduced
**every** load-bearing measurement independently — including writing **its own capstone CFG** to test the
dominance claim — and **confirmed** the producer formula, the dominance result, the `movzx` non-reachability,
the frameless function, the arena behaviour, the identity, and that **`O-OPEN` was correctly selected with an
honest closure account** (it called that handling *"a model of how to do it"*). **Both defects were the
Session's, and both are now fixed:**

| # | Defect | Fix |
|---|---|---|
| **1** | **A required parser fixture was missing** — the packet's *"mid-instruction disassembly start that is REJECTED"* fixture existed **only in a docstring**, because the tool had **no disassembly surface at all** | `a2h-oom-slice.py` now exposes `verify_instruction` + a `--verify VA:BYTES` CLI, rejecting misaligned starts, wrong bytes, length mismatches, malformed bytes and out-of-section VAs. **Nine new tests**, synthetic **and real** XBE. **31 tests, OK** |
| **2** | **The "no-trap" label on the A2g run was FALSE** — `metadata.json`'s `settings` dict sets `RECOMP_APU_TRAP=1` and its log line 26 says *"trapped for MMIO"*. The Session read `run_profile.effective_settings`, found it **empty**, and reported "ABSENT" | **Both records corrected** with visible banners; **the strong claim is WITHDRAWN**; re-censused from **all** plausible locations |

**The corrected census: 35 archived runs carry the `598869040` request, ALL 35 are trapped, ZERO untrapped.**
So *"the trap is not a necessary cause"* is **not established** by this archive set. **What survives:** the
failure is **not trace-caused** (R1 has `RECOMP_APU_TRACE` with 401 `[APUMMIO]` lines; A2g has none; trace is
observation-only) and it **predates the A4b2 trap work** (different exe, five days earlier, same size, type,
OOM tuple and terminal ICALL).

**Defect 2 is the instructive one:** reading `effective_settings`, finding it empty, and concluding "ABSENT"
is **reading an absent record as a negative measurement** — the same error class as the earlier `[GP*]`-zeros
mistake and the byte-width writer census.

> ### The acceptance took FOUR rounds, and every round found a real defect — none of them by the author
>
> **This is recorded because it is the strongest available argument for the acceptance stage existing.**
> Four rounds, four real defects, **none found by the author re-reading his own work** — each found because a
> reviewer with a different method replayed the claim against real bytes.
>
> | Round | Finding | Whose |
> |---|---|---|
> | 1 | **Banners added, bodies never swept** — the withdrawn claim survived unflagged in six places | **mine** |
> | 2 | **The sweep's pattern was too narrow** — it searched `no-trap` while the survivor spelled it `NO trap` | **mine** |
> | 3 | **The fix malformed a table row** — a stray `|` left 3 cells against a 2-cell header | **mine** |
> | 4 | **The new guard failed on CORRECT input** — `cells()` promised escaped-pipe handling it did not have, so it passed *by luck of the corpus* | **mine** (new non-contract code) |
>
> **And the guard the reviewer's round-3 diagnosis prompted immediately found a fifth, pre-existing defect no
> reviewer had reported:** `docs/jsrf-run-profiles.md:92` was **born malformed at `ad400294`** (2026-09-22),
> missing its third cell, sitting in a durable evidence-rules document. **The reviewer corrected the
> provenance** — `7f63c45` merely carried it forward.
>
> **Three distinct failure modes, each now guarded against rather than remembered:**
> 1. **A correction banner is not a correction** — sweep the bodies.
> 2. **A sweep is only as good as its pattern** — enumerate the claim, not a spelling of it.
> 3. **A change applied to the sentence in front of you can break the structure it sits in** — hence
>    `tests/test_markdown_tables.py`, a **machine check** rather than a resolution to be more careful.
>
> **The guard is correct by construction, not by luck.** Its first version passed only because none of the
> four guarded documents happened to contain an escaped pipe; the reviewer demonstrated it would report
> **correct** rows (`| \`A\|B\` | … |`, `| \`GS |= 1\` | … |`) as malformed. It now skips escaped characters
> and backtick code spans, and **replaying it over the historical revisions still catches both real defects**
> while HEAD is clean. **12 tests.**
>
> **Recorded for a possible future widening:** 767 markdown files scanned → 36 mismatching rows across 24
> files, of which **27 are escaped-pipe artifacts, 6 backtick artifacts, and 3 genuine** —
> `a4b2-r7-execution-evidence.md:54-56`, three real 2-cell rows under a 3-cell header **in an accepted
> packet's execution evidence**, where a missing cell means a column of measurements is silently absent.
> **Fix the counting first, then widen** — widening today would yield 3 real hits against 33 false positives.

> ### Stage 2 (`deepseek-v4.1-flash`) also returned `NOT ACCEPTED`, on criterion 2 only — and it was right
>
> **Criterion 1 (the parser fixture) it ruled `AGREED`**, and it verified the fix more sharply than stage 1:
> it **discriminated the two guards** rather than assuming. The bytes guard rejects `--verify
> 0x00149E23:8345dc20`; **but the *alignment* guard is independently real** — supplying the bytes *genuinely
> at* `0x00149E23` (`008345dc`) passes the bytes check and **still rejects**, decoding to
> `add byte ptr [ebx + 0x6a20dc45], al` — *"precisely the plausible-garbage hazard the packet exists to
> prevent."* It also confirmed the fix is **additive** (the pre-fix command still yields the identical
> `A11C55FE…B8B600C` stage 1 recorded), and gave the census the **positive control stage 1 lacked**:
> **657 runs are untrapped-observable, and 0 of them carry the request** — so the absence claim now has the
> coverage witness §2.4.5 requires.
>
> **Criterion 2 it kept `DISAGREED`, correctly: the Session added correction BANNERS but never swept the
> document BODIES.** The withdrawn claim survived **unflagged in six places**, including a **conclusions
> list** re-asserting the exact sentence the banner declares withdrawn, a **section heading**, and a
> **document title**. **All six are now corrected**, plus one more the Session found in this plan's own
> findings table.
>
> **The lesson is a process one worth carrying:** a correction banner is not a correction. **When a claim is
> withdrawn, every assertion of it must be swept, not just the section that discussed it.** The Session
> treated the banner as sufficient and it was not.
>
> **Stage 2 advisories recorded:** (a) `verify_instruction` is a **citation verifier, not a boundary oracle**
> — a 1-byte claim at a non-boundary can pass, because the guard only catches misalignment when the decoded
> length differs from the claimed length; (b) **the frozen packet's own premise text** (`a2h-oom-causal-slice.md`
> lines 7, 9, 16) still says "no-trap A2g" and is now known-false — **a frozen packet may not be edited
> (§2.2.4), so it is recorded here** so a future reader does not take the premise as established;
> (c) the measured **behavioural** trap difference (A2g *"STUBBED - passthrough mode"* with **0** `[APUMMIO]`
> vs R1's pinned DSP56300 with **401**) is now measurable and is recorded as a **limit**: **how deeply A2g's
> stub intercepted APU behaviour is not measured**, which is precisely why the **narrow** claim is the right
> one.

### What was established

| Finding | Witness |
|---|---|
| **The OOM predates the A4b2 trap work** | Different exe (`2cd0472a256e9d` vs `bc8e288dd54d`), five days earlier, **same size/type/OOM tuple/terminal ICALL** — and **all 94 invocation sizes identical index-for-index** across both logs |
| **The chain is bound to the producer** | `RegionSize = align16([ebp+0x10]) + 0x20`; producer **`0x0014980E`**; value from **`[ebp+0x10]`, the 3rd argument**. **Reproduces both observed sizes exactly**; both pre-add values exact multiples of 16 |
| **The producer's function** | **`sub_001497DC`** (`0x001497DC`–`0x00149F48`), **frameless** (`fpo_leaf`, inherits the caller's frame), so `[ebp+0x10]` is an offset in the *caller's* frame |
| **Dominance, doubly derived** | Two independent CFG methods agree: 46 reaching instructions, **2 writers**, single entry `0x001497DC`, call **unreachable** with writers removed; the `movzx` writer **never reaches the call** |
| **The failing argument was pointer-shaped** | `≈0x23B20410`, far outside the 64 MB RAM window — **the caller passed a pointer-like value where a size belongs** |
| **The arena and toolkit behaved correctly** | `alloc_type 0x801000` is `MEM_COMMIT`, no `MEM_RESERVE`, so no reserve branch applies; the bridge returns `0xC0000017`. ~~The guest then does not check the result and calls through NULL~~ **WITHDRAWN — the guest DOES check (`test eax,eax; jl 0x149eec`; `0xC0000017` is negative so the branch is taken) and the OOM is HANDLED. The NULL call is a different pass. See the erratum in `a2h-oom-causal-slice-evidence.md` and `a2h-critical-path-advisor-ruling.md`** |

**Three hypotheses refuted in sequence, all recorded:** the horizon record's reading that the OOM *tracks the
trap*; the Session's mid-execution **stale-stack-slot** hypothesis; and the Session's **"no-trap A2g"** claim.

### Next authorized work

**An `A2h-named-producer` discovery** naming `[ebp+0x10]` of `sub_001497DC` at `0x0014980E`, with **the
caller's identity** as its remaining question. **The Session's earlier "only two of the 8 call sites can
supply `arg2`" narrowing was INVALID and is withdrawn** — the Planner refuted it and the Session verified the
refutation from the generated source: `sub_001497DC` is **FRAMELESS** (`Frame: fpo_leaf`, `ebp = g_seh_ebp`),
so **`[ebp+0x10]` belongs to the CALLER's frame**, and **every call site has a frame**. The two three-push
sites are **leads, not a candidate set**, and the follow-up must require a **frame/register/stack identity
witness** rather than assuming an argument list. **Whether any particular site is the failing path is NOT
established.** **No change
packet is authorized** — the packet says *"consider a change only after cause is established."*

**Toolkit:** `c151d4e` (unchanged). **Game:** `e1dd3e2`.

---

## Previous — `A2h-oom-causal-slice-r1` **PROMOTED 2026-09-27, `ADEQUATE`** (now executed → `O-OPEN`, chain bound)

**Row as selected: `O-OPEN`.** Evidence: `docs/reviews/a2h-oom-causal-slice-evidence.md`. Binding:
`docs/reviews/a2h-oom-causal-slice-binding.json`. **No new guest run was needed or performed** (the packet's
steps 3–4 were not entered), **no toolkit change, no runtime change, no instrumentation added**.

### What was established

| Finding | Witness |
|---|---|
| **The failure predates the A4b2 trap work** | The chain is **semantically identical** on a **different exe** five days earlier (`2cd0472a256e9d` vs `bc8e288dd54d`): **all 94 invocation sizes identical index-for-index**, failing index 93, site `0x00149E50`, `esp=0x00F7FCF0`, size `598869040`, type `0x801000`, same OOM tuple, same ICALL `(0x0, 00F7FD00, 0014982E)`. **It is also not trace-caused** (R1 has `RECOMP_APU_TRACE` with 401 `[APUMMIO]` lines; A2g has none). **The trap is NOT shown to be unnecessary** — all 35 archived runs carrying this request are trapped |
| **The chain is now bound to the producer** | `RegionSize = align16([ebp+0x10]) + 0x20` — producer **`0x0014980E mov [ebp-0x24], eax`**, value from **`[ebp+0x10]`, the function's 3rd argument**. **Reproduces both observed sizes exactly**, both pre-add values exact multiples of 16 |
| **The producer's function** | **`sub_001497DC`** (`0x001497DC`–`0x00149F48`, `recomp_0003.c:20105`). **It is frameless and inherits the caller's frame** (`ebp = g_seh_ebp; /* fpo_leaf */`), so `[ebp+0x10]` is an offset in the *caller's* frame |
| **The failing argument was pointer-shaped** | `≈0x23B20410`, far outside the 64 MB RAM window (`0x04000000`) — **the caller passed a pointer-like value where a size belongs** |
| **The arena and toolkit behaved correctly** | `alloc_type 0x801000` has no `MEM_RESERVE`, so no reserve branch applies; the bridge returns `0xC0000017`. The guest then does not check the result and calls through NULL |

**Two hypotheses were refuted in sequence, and both corrections are recorded:** the horizon record's earlier
reading that the OOM *tracks the trap* (correlation real, causation wrong), and the Session's mid-execution
**stale-stack-slot** hypothesis (refuted by a bounded dominance analysis proving every path to the call
passes the writer).

### Next authorized work

**An `A2h-named-producer` discovery**, naming `[ebp+0x10]` of `sub_001497DC` at `0x0014980E` as the producer,
with **the caller's identity as its single remaining question**. The generated source names every call site of
`sub_001497DC` and the argument expression each pushes, so this is **bounded, not open-ended**. **No change
packet is authorized** — the packet says *"consider a change only after cause is established."*

**Toolkit:** `c151d4e` (unchanged). **Game:** `1201774`.

---

## Previous — `A2h-oom-causal-slice-r1` **PROMOTED 2026-09-27, `ADEQUATE`** (now executed → `O-OPEN`, chain bound to the producer)

- **Packet:** `docs/packets/a2h-oom-causal-slice.md`, revision **`A2h-oom-causal-slice-r1`**, class
  **discovery**, frozen SHA-256
  **`E9CDB1B39066CE5F5626FF74246E5EAD34C1612BC31BE622341B50FCC541108C`** (**47 lines**). **This is the packet
  to execute.** Promotion byte-identical with no revision (§5.3); hash verified **3× over ~10 s** immediately
  before and matching the Planner's report.
- **Adequacy:** the **writing Planner's** own §5.3 two-question review — **`ADEQUATE`**, `BLOCKING: NONE`,
  `PREMISE_FRESHNESS: BOUNDED`. **Correct for a discovery**; **no shape preflight**. Verification:
  `docs/reviews/a2h-oom-causal-slice-r1-session-verification.md`.
- **The question:** *which instruction and live input defined `[ebp-0x24] = 0x23B20410` before the
  `0x00149E24` addition produced the failing allocation size?* — answered by a **backward, invocation-bound
  slice** from the original XBE plus pinned archived runs, with **at most one bounded 8-second diagnostic
  strict capture** if needed.
- **Rows:** `O-IDENTITY` (provenance repair) · **`O-APU-INPUT`** (a modelled APU input binds the pre-add
  local → named-input discovery) · **`O-OTHER-INPUT`** (a witnessed non-APU producer → named-producer
  discovery) · **`O-SEMANTICS`** (a deliberate large virtual-region request → virtual-memory fidelity
  discovery **and Advisor ruling**) · **`O-OPEN`** (any unbound edge → one missing-witness/tooling
  discovery, **never a fabricated fix**).
- **Deliverables (not yet existing — the packet creates them):** a checked-in, tested backward-slice tool
  under `scripts/` with a matching unit-test module, required before parsed traces may select an attribution
  row; the evidence record; and at most one diagnostic hook, env-gated and **off at closure**.
- **Forbids carried and verified present:** does **not** suppress the trap, fake the allocation, or **widen
  the arena**; the guest's **NULL-guard repair is out of scope**; **`PIO_FREE` stays deferred**;
  `A4b2-r7`/accepted-closed `A4b2-r8`/`A4b1-r4` not reopened; `0xFFFFB3` **`UNRESOLVED`**; any toolkit
  advance requires **re-establishing P4's discovery-transfer bridge** first.

### What the Session measured before planning (both records committed)

| Record | Establishes |
|---|---|
| `docs/reviews/a2h-mechanism.md` | The request is a **byte-identical constant across 35 runs** (`598869040` = `0x23B20430` ≈ 571 MB vs a 48.5 MB arena); **the earliest OOM run is five days and one build earlier** (`20260922-224429-003-a2g-304f0-span`), so **the failure predates the A4b2 trap work** — it makes an already-broken path reachable sooner. **The trap is NOT shown to be unnecessary** (all 35 runs carrying this request are trapped); the chain is `NtAllocateVirtualMemory` (ordinal 184, from `0x00149E50`) → **`0xC0000017`** → guest calls through **NULL** → `0xE0424943`. **Two stacked defects: an implausible 571 MB commit, and no NULL check on the result.** The arena behaves **correctly** |
| same, §4 | The size is a **stack local** (`RegionSize = [ebp-0x24] + 0x20`), so its producer is a **bounded backward question**. The **same call site passes exactly two sizes**: `2097200` normally, `598869040` when it fails (**~285× larger**). The failing call is **`MEM_COMMIT` (`0x801000`)**, **not** a pure `MEM_RESERVE`, so the toolkit's reserve branches **cannot run for it** — the "legitimate large reservation" defence does **not** apply to this invocation, though it remains a correct general caution |
| same, §5 | **`0xC0000017` is survivable**: `20260927-130036-879-a4b2-nr-baseline` returned `NO_MEMORY` **without** taking the NULL call, so the guest's own path decides whether the failure is fatal |

**Three Session errors in that analysis are recorded and corrected in place:** a claim that the normal
pre-add local was "exactly 2 MB" (it is `0x00200010`, withdrawn); the horizon record's earlier causal
reading that the OOM *tracks* the trap (correlation real, causation wrong); **and the "no-trap A2g"
characterisation in this section's own table above — the A2g run IS trapped, so the strong "the trap is not
necessary" claim is withdrawn, and only the narrower "predates the A4b2 trap work" and "not trace-caused"
claims survive.**

**Next authorized action:** **execute `A2h-oom-causal-slice-r1`** — the backward slice, producing
`docs/reviews/a2h-oom-causal-slice-evidence.md` and a first-match row, then §5.8 acceptance.

**Toolkit:** `c151d4e` (unchanged). **Game:** pending commit.

---

## Previous — **`PIO_FREE` DEFERRED by Advisor ruling** (superseded as the current packet)

**The `PIO_FREE` line is closed at `O-OPEN` and DEFERRED.** `PIO_FREE-title-demand-bound-r1` executed to
`O-OPEN`, the Session made the scope/defer referral its row required, and the **Persistent Advisor ruled:
object-identity analysis is OUT OF PROPORTION and is NOT authorized; DEFER the leaf, do not retire it.**
Ruling verbatim: `docs/reviews/pio-free-advisor-scope-defer-ruling.md`.

### The leaf's terminal entry (Advisor-confirmed, adopt as the recorded state)

> *"The `0x80` stub passes all **13 constant gates** — two (`0x001A3EB3`, `0x001A3FDB`) at **exact
> equality, zero margin** — and is **UNPROVEN for the 15 variable gates** (all `OPEN`: demand
> `k × byte[+0x64]` needs byte ≤ `32/k`, i.e. 3–16, unproven). **Hardware semantics (units, capacity,
> drain, overflow, ordering) remain `UNKNOWN` and unsourced.**"*

**Carried premises:** `A4p`'s **inferred** C1–C4 convention premise; its E3 indirect-access limits; the
**direct-read scope only**; and that this is **sufficiency relative to the stub value, never hardware
truth**.

**Why the hole is acceptable (Advisor):** *"it cannot bind"* — any later strict progress claim is blocked
**harder** by sourcing (no admissible model exists) and by `A2h` (no trapped time past the prefix). *"The
demand hole matters only once a model exists — i.e., after sourcing, which doesn't exist. Deferral loses
nothing."*

**Named reopen conditions (binding):**
1. **Newly admitted hardware source evidence** — the only modelling unblocker.
2. **A specific strict progress/liveness claim traversing named gates** → **demand-driven per-gate analysis
   backward from that claim's checkpoint**, **never** the 185-writer forward census.
3. **Observed title behaviour implicating a specific gate hang** → pursue candidate (a) first: the
   `or_bit7` writer at `recomp_0002.c:56809`, since a single bit-7 set puts the field at ≥ 128 and exceeds
   `32/k` at **every** site.

**Durable assets inherited forward:** the checked-in tested classifier
(`scripts/pio-free-demand.py` + `scripts/test_pio_free_demand.py`, **29 tests**, deterministic, hashed),
which makes reopening cheap; and the concrete requirement any future model must satisfy (**≥ 128 at the two
zero-margin gates**). **Prioritization note for any future attempt:** the tightest gate is the **k=10 site
`0x001A4242`**, which needs byte ≤ 3.

**`A4b2-r8` is UNAFFECTED** — Advisor-confirmed: its decision inputs precede the prefix end, its contract
contains no `PIO_FREE`-demand premise, and the shared premises it *does* rely on (XBE identity, `A4p`'s
28-site population) were **re-confirmed** here (28/28 reconcile), so this work **corroborates** it. It stays
**accepted and closed**.

### Next authorized work — `A2h`

**The Advisor named the critical path plainly:** *"`PIO_FREE` is deferred. The critical-path work is
**`A2h`** — the trapped crash at ≈4.77 s blocks all trapped observation past the prefix, which every future
strict claim needing later behavior requires. Beyond that, the Planner's milestone order governs — not
`PIO_FREE`."*

**`A2h`** is the **known heap-OOM class** that ends instrumented strict runs at ≈4.77 s
(`xbox_HeapAlloc: out of memory (requested 598869040, used 12715008/50855936)` → invalid ICALL
`0x00000000` → `0xE0424943`). Measured in `docs/reviews/pio-free-strict-horizon.md`: it tracks the
**recent-build + trap** combination (untrapped runs reach the full ≈31.7 s deadline), and the **trapped**
run is the one making real progress (it boots the GP, drives the clear, and leaves the pending-word spin
with `F=0`, whereas the untrapped 31.71 s is a **hang** with `F=2`, `Wf0=3`).

**Planning for `A2h` is next.** Per the workflow, the Planner determines whether it is a **discovery** or a
**change** packet; a shape preflight is required only for a new **change** packet.

**Toolkit:** `c151d4e` (unchanged). **Game:** `452223b`.

---

## Previous packet — `PIO_FREE-title-demand-bound-r1` **EXECUTED 2026-09-27 → `O-OPEN`** (deferred by Advisor ruling; see above)

**Row: `O-OPEN`.** Evidence: `docs/reviews/pio-free-title-demand-evidence.md`. Follow-up:
`docs/reviews/pio-free-demand-slice-followup.md`. **No toolkit change, no game runtime change, no build, no
guest run, no instrumentation.**

### What was executed

| Experiment | Result |
|---|---|
| **1 — bind population** | `find` → 28 operand offsets, all `DSOUND`; the classifier derives starts by **decoding** (never `offset−1`/`−2`) and **28/28 reconcile** with `A4p`'s frozen set. Two-spelling hazard closed **positively**: normalising every `MEM8/16/32` literal gives **hex 10 + decimal 18 = 28** by construction |
| **2 — tested classifier** | `scripts/pio-free-demand.py` + `scripts/test_pio_free_demand.py`: **29 tests OK**, covering every fixture the packet names **including the mid-instruction hazard as a real fixture**. **The tests found two real defects** in the classifier that inspection missed: a fixed decode window overran short sections, and `absolute_memory_va` inspected only operand 0 — so **every ModRM load was unrecognised**, which would have rejected half the population |
| **2 — census** | **28 found, 28 classified, 0 rejected. 13 CONSTANT, 15 VARIABLE.** Literals `0x4`×4, `0x8`×3, `0xC`, `0x20`, `0x48`, `0x4C`, **`0x80`×2**; **none fail the stub**; **TWO at ZERO margin** (`0x001A3EB3`, `0x001A3FDB`) — so `0x80` passes **at equality**, never *"any value ≥ 4 passes"*. Variable demand registers **`ecx`×11, `eax`×4**. Determinism verified byte-identical |
| **3 — backward slice** | All 15 variable gates reach a **byte load** at `+0x64`, `k ∈ {2,3,6,7,8,9,10}`. **All 15 `OPEN`** |

**The classifier corrected an off-by-one in the Session's earlier ad-hoc census**: the zero-margin site is
`0x001A3FDB`, not `0x001A3FDA`.

**A withdrawn verdict:** the first slice returned `EXCEED`×15 from the **generic** byte bound `0..255`. The
packet is explicit that a generic byte bound gives **finiteness, not sufficiency**, and that an isolated
unconstrained register value is **not a witness** — so the corrected slice returns `OPEN` with the missing
witness named per site.

### The follow-up found the leaf is **more** open than expected

1. **A structural narrowing:** at the first poll's function, the `LO8(eax)` writer (`0x001A29EB`) and the
   poll (`0x001A2A7F`) are **mutually exclusive within one pass** — both are gated on bit 0 of `[esi+0x12]`,
   which is never written in that function, and they take **opposite** branches.
2. **But the exclusion is escapable:** a full-width scan finds **16 writers to `+0x12`**, **nine** of which
   set bit 0 from a register and the four masking writers all use masks with bit 0 **set**. So bit 0 **is**
   mutable and the **cross-call route is open**: one pass writes the field with bit 0 clear, a later call
   sets bit 0, the next pass polls what was left behind.
3. **And a Session error, corrected:** the previous commit claimed `MEM8(..+0x64)` has *"exactly three
   writers"*. **A full-width scan finds 185 writers to offset `+0x64`** — because **`+0x64` is an offset,
   not a field**, appearing in `esp+0x64` stack slots, `ebp+0x64` frame slots and unrelated bases with
   float-looking payloads. **The same error class as the two-spelling hazard: enumerating by one width and
   treating the result as complete.** The three `MEM8` writers are real but are **not** the writer set for
   the polled object. The invalid tool and its JSON were **removed**.

**So the `O-OPEN` leaf reduces to: which of the 185 writers can reach the DSOUND voice object, and can any
leave the field above `32/k` when a poll reads it?** That needs **object-identity/aliasing analysis** the
packet did not perform.

**Per the packet's own rule — *"if this is not a finite resolvable leaf, refer an explicit scope/defer
decision to Advisor instead of iterating"* — the Session is making that referral rather than opening another
round.** The question for the Advisor is whether object-identity analysis is in scope, deferred, or whether
the boundary should be retired at `O-OPEN` with the stub documented as **sufficient for 13 constant sites
and unproven for 15 variable ones**.

**Toolkit:** `c151d4e` (unchanged). **Game:** `e3009e0`.

---

## Previous — `PIO_FREE-title-demand-bound-r1` **PROMOTED 2026-09-27, `ADEQUATE`** (now executed → `O-OPEN`)

- **Packet:** `docs/packets/pio-free-title-demand-bound.md`, revision **`PIO_FREE-title-demand-bound-r1`**,
  class **discovery**, frozen SHA-256
  **`977A96F708919F309AE01009BA64AB75AA344794950553530CCA1AF958A49ABB`** (**34 lines**). **This is the packet
  to execute.** Promotion byte-identical with no revision (§5.3); hash verified **3× over ~12 s** immediately
  before.
- **Adequacy:** the **writing Planner's** own §5.3 two-question review — **`ADEQUATE`**, `BLOCKING: NONE`,
  `PREMISE_FRESHNESS: BOUNDED`. **Correct for a discovery**; **no shape preflight**. Verification:
  `docs/reviews/pio-free-title-demand-bound-r1-session-verification.md`.
- **The question:** for this exact XBE, *under the current trapped VP implementation that always returns
  `0x80`*, is every feasible **direct** `PIO_FREE` gate in the frozen 28-site set guaranteed an unsigned
  compare demand at most the stub's value (`0x80 >> 2 = 32` variable, `0x80 & ~3 = 128` masked constant) — or
  is a reachable demand above it witnessed, or a bound unprovable? **A question about the title against this
  stub, never about hardware capacity, units, queue, ordering, or that the device works.**
- **Rows:** `O-IDENTITY` (stale/malformed identity → repair discovery) · **`O-EXCEED`** (a reproducible
  **feasible** demand `> 32`, or a verified constant `> 128`, with byte/definition/path witnesses →
  reachability discovery **plus explicit Advisor hardware-sourcing/scope referral**) · **`O-BOUND`** (all 28
  classified, every constant `≤ 128`, a universal `≤ 32` bound for every variable compare → **Advisor
  scope/defer/retire ruling**; a change packet is conditional on **new** admissible hardware evidence) ·
  `O-OPEN` (cannot prove all `≤ 32`, cannot exhibit `> 32`, or cannot reconcile → focused demand-slice
  follow-up, or Advisor if not finite).
- **Why this is not the exhausted search repeated:** the `O-UNKNOWN` row's named five-leaf
  source/queue-interface successor is recorded as **not presently feasible as a hardware-specification
  discovery**, because `jsrf-run-profiles.md:245-267` admits only external hardware documentation and every
  lead is silent. Guest code is admissible **about the title** (`:260-262`), which is what this packet uses.
- **Session validation before freeze:** the intervening commits between the Planner's planning pin
  (`91c626a`) and HEAD are **documentation-only** (2 commits, **zero** paths under `src/`, `game/`,
  `config/`, `scripts/`, `tools/`) — so the premise is intact. **All named commands validated**:
  `find 0xFE820010` works (28); the packet's claim that the **`0x` prefix is mandatory is correct** (bare hex
  is rejected); `game/mygame_analysis.json` exists (17 047 bytes) and `inspect-jsrf.py:29,70` really reads it;
  and the two "missing" paths are the packet's **own deliverables**, not prerequisites.
- **Deliverables (not yet existing — the packet creates them):** a checked-in, tested classifier under
  `scripts/`, with a matching unit-test module, that must **fail on malformed input** and must **reject a
  mid-instruction start** rather than silently decode. **The 15/13 census and the `k × byte[+0x64]` lead must
  be re-derived by that tool before use**, and `001A3F24` specifically needs reaching-definition resolution.
- **Scope:** offline only. Diagnostic parser/tests, JSON and an evidence record. **No guest run, no build, no
  toolkit runtime change, no instrumentation.**

**Next authorized action:** **execute `PIO_FREE-title-demand-bound-r1`** — the four Experiments, producing
`docs/reviews/pio-free-title-demand-sites.json` and the evidence record, then §5.8 acceptance.

**Toolkit:** `c151d4e` (unchanged). **Game:** pending commit.

---

## Previous — `PIO_FREE-model-r2` **ACCEPTED 2026-09-27** (`O-UNKNOWN`) — successor planning opened

**`PIO_FREE-model-r2` (`11D6ECB1…1DDA9E`) executed → `O-UNKNOWN` → acceptance stage 1 `ACCEPT`,
`BLOCKING: NONE`.** Review: `docs/reviews/pio-free-model-r2-acceptance-review.md`; evidence:
`docs/reviews/pio-free-model-execution-evidence.md`; closure: `docs/reviews/pio-free-model-r2-session-closure.md`.
**No toolkit change, no game code change, no build, no guest run, no instrumentation.**

### The finding

| Fact | Value |
|---|---|
| **The address** | `0xFE820010` = `APU+0x20010` = **`VP+0x10`** = **`NV1BA0_PIO_FREE`** — the **same** address `A4p` called "PIO" |
| **The current model** | constant **`0x80`**, comment *"Always pretend queue is empty"*; every other VP offset reads `0` |
| **The write path** | **synchronous** via `fe_method` — **there is no queue at all** |
| **Threshold census** (stable ×3) | **15 VARIABLE** (`val >> 2` vs a register) vs **13 CONSTANT** (`val & ~3` vs `4`) — **the variable form is the majority**, so the word must be a *quantity that can be insufficient*, not a flag |
| **Ledger** | **3 of 8 leaves** resolved/partial; **5 `UNKNOWN`** — units, capacity, drain, overflow, ordering |
| **Sourcing** | **INADEQUATE on exhausted leads.** No primary spec *live*, and the **archived NVIDIA brief is silent** (`PIO_FREE`=0, `NV1BA0`=0, `queue`=0, `free`=0, `FIFO`=0, `depth`=0 with live positive controls); xboxdevwiki `APU` silent, **site-wide search returns no results**; first-party Brian Schmidt account silent; `Cxbx-Reloaded` (**independent emulator lineage**) silent; `JayFoxRox/xbox-tools` silent |

**The negative control is an admission:** xemu's `vp_read` says *"we don't simulate the queue for now,
pretend to always be empty"* — confirming **a queue exists in hardware, is not simulated, and `0x80` is a
pretence**. Toolkit ancestry, so **not independent support**.

**Why `O-UNKNOWN`:** `O-IDENTITY` no (all reconciles); `O-CONFLICT` **no** — conflict needs two
authenticated sources with **incompatible concrete meanings**, and there is one admission plus **silences**,
and silence is not conflict (reviewer independently upheld this); `O-SPEC` no on **two** grounds.

**`A2h` deliberately NOT named** — the missing witnesses are **documentation, not guest time**.

### ⚠ The constraint that shapes the successor — and the reframing that unblocks it

**`docs/jsrf-run-profiles.md:245-267`** requires a modelled cause to rest on **either** one credible
**primary** source **or two independent secondary sources of meaningfully different provenance**, and it is
explicit that:

> *"**observed guest behaviour may corroborate an interpretation but does not count as one of the two
> independent sources.** Guest code is evidence about the title, not about the hardware."* (`:260-262`)

**So the Session's 28-site census is a constraint on any model, not an admissible source for one**, and
acceptance has established that **no admissible external source documents the five `UNKNOWN` leaves**. The
`O-UNKNOWN` row's named successor — *"a focused source/queue-interface discovery on the exact listed
leaves"* — is therefore **not feasible as an admissible hardware-model route**, and the Planner has
confirmed that independently.

**But there are two different questions, and only one is blocked:**

| Question | Evidence class | Status |
|---|---|---|
| *What does the hardware's free-space register truly do?* | **hardware** | **BLOCKED** — no admissible source exists |
| *Is the existing stub **sufficient** for the title to proceed?* | **the title** | **DECIDABLE** — guest code **is** admissible about the title |

**The second is developed and measured in `docs/reviews/pio-free-sufficiency-analysis.md`:**

- Every variable site is `mov r,[0xFE820010] / shr r,2 / cmp r,demand / jb poll`, so it exits iff
  **`(val >> 2) >= demand`**; with the stub's `0x80` that is **`32 >= demand`**.
- **The demand is `k × <byte field>`, `k ∈ {2,3,6,7,8,9,10}`**, and the source is a **byte-sized load at all
  15 sites** (0 wider, 0 unresolved) — so **demand ≤ 255 × 10 = 2550**, a **derived** bound.
- **The byte field is at `[esi+0x64]` (14 sites) / `[ebx+0x64]` (1)** — **the same structure field**.
- **The 13 constant sites do not all compare against `4`** (the Planner's correction, verified): literals
  are `0x4`×4, `0x8`×3, `0xC`, `0x20`, `0x48`, `0x4C`, and **`0x80`×2** — all pass, but the two `0x80`
  sites sit at **ZERO margin**, so a stub of `0x7C` or less would fail them.
- **The variable demand register is `ecx` (11) or `eax` (4)** — a hardcoded-`ecx` assumption misses 4.

**So the decidable question is: what is the maximum value of one byte field at `+0x64`?** The stub is
sufficient iff `max_byte × k ≤ 32` at every site. **If it is, the PIO_FREE boundary is *bounded* rather than
unknown and needs no hardware model for progress. If any site can exceed 32, a truthful free-space model
becomes *necessary* — and that requires the currently-blocked hardware sourcing: a genuine,
evidence-backed escalation.**

**This does not model the hardware**, and the five `UNKNOWN` ledger leaves **remain `UNKNOWN`**.

**Toolkit:** `c151d4e` (unchanged). **Game:** `ec57057`.

---

## Previous — `PIO_FREE-model-r2` **EXECUTED 2026-09-27 → `O-UNKNOWN`** (now accepted)

**`PIO_FREE-model-r2` (`11D6ECB1…1DDA9E`) executed offline. Row: `O-UNKNOWN`.** Execution evidence:
`docs/reviews/pio-free-model-execution-evidence.md`; closure: `docs/reviews/pio-free-model-r2-session-closure.md`.
**No toolkit change, no game code change, no build, no guest run, no instrumentation.**

### The finding

| Fact | Value |
|---|---|
| **The address** | `0xFE820010` = `APU+0x20010` = **`VP+0x10`** = **`NV1BA0_PIO_FREE`** — the **same** address `A4p` called "PIO", not a second device |
| **The current model** | constant **`0x80`**, comment *"Always pretend queue is empty"*; every other VP offset reads `0` |
| **The write path** | **synchronous** via `fe_method` — **there is no queue at all** |
| **The guest's forms** | one constant (`val & ~3` vs `4`) and one **variable** (`val >> 2` vs a register); the shift form reads the word as a **count in units of 4**, so `0x80` asserts **32 free units** |
| **The site's shape** | **poll `0x10`, then push to VP `0x280`** = `NV1BA0_PIO_SET_HRTF_HEADROOM` — a producer against a free-space counter |
| **Ledger** | **3 of 8 leaves** resolved/partial; **5 `UNKNOWN`** — units, capacity, drain, overflow, ordering |
| **Sourcing** | **INADEQUATE.** No primary spec (the NVIDIA MCP brief is 404); the one admissible secondary source (xboxdevwiki `APU`) is **silent** — verified by term count on its raw wikitext (`PIO_FREE`=0, `queue`=0, `free`=0, `depth`=0) |

**The negative control is an admission:** xemu's `vp_read` says *"we don't simulate the queue for now,
pretend to always be empty"* — it confirms **a queue exists in hardware, is not simulated, and `0x80` is a
pretence**. Being toolkit ancestry, it is **not independent support**.

### Why `O-UNKNOWN`

`O-IDENTITY` — no, everything reconciles. `O-CONFLICT` — **no**: conflict needs two authenticated sources
with **incompatible concrete meanings**, and there is one admission plus one **silence**; silence is not
conflict. `O-SPEC` — **no**: five leaves `UNKNOWN` **and** sourcing unmet. **`O-UNKNOWN` by first match.**

**`A2h` is deliberately NOT named**: the missing witnesses are **documentation, not guest time**, so the
successor is a **sourcing/interface discovery**, not a run.

### Verified at closure

XBE matches baseline; game `d54b5ff`, toolkit `c151d4e` clean; **ctest 18/18**; **no instrumentation
exists**, so nothing is enabled at closure; §5.8 satisfied (knowledge output, no strict criterion
satisfied, nothing claimed to work).

### Next authorized action

**A focused `PIO_FREE` source/queue-interface discovery** on the five `UNKNOWN` leaves — **units/bit
encoding, capacity, drain/completion, overflow/backpressure, read-vs-write ordering** — carrying the
false-model test as the positive prediction it must satisfy.

**Toolkit:** `c151d4e` (unchanged). **Game:** pending commit.

---

## Previous — `PIO_FREE-model-r2` **PROMOTED 2026-09-27, `ADEQUATE`** (now executed → `O-UNKNOWN`)

- **Packet:** `docs/packets/pio-free-model.md`, revision **`PIO_FREE-model-r2`**, class **discovery**,
  frozen SHA-256 **`11D6ECB195D51159785D6E94C98439BD4AA6979FDEE6B8EC79B28A80961DDA9E`** (**36 lines**).
  **This is the packet to execute.** Promotion byte-identical with no revision (§5.3); hash verified **3×
  over ~10 s** immediately before.
- **Adequacy:** the **writing Planner's** own §5.3 two-question review — **`ADEQUATE`**, `BLOCKING: NONE`,
  `PREMISE_FRESHNESS: BOUNDED`. **Correct for a discovery packet**: §5.8 says the writing Planner reviews
  its own packet and **no second Planner is spawned** (`docs/agent-workflow.md:615-617`, `:743`).
  **No shape preflight** — that is triggered only for a **new change** packet. Verification:
  `docs/reviews/pio-free-model-r2-session-verification.md`.
- **The question:** for **trapped** guest reads at `0xFE820010` (= `APU+0x20010` = `VP+0x10` =
  `NV1BA0_PIO_FREE`), what device/register and free-space unit, capacity, producer/consumer, reset and
  update-order rules are supported by independent evidence, and can they specify a **non-synthetic** model
  for the observed gate thresholds — or is some named semantic/coverage leaf still unknown?
- **Rows:** `O-IDENTITY` (stale identity/premise → re-plan) · `O-CONFLICT` (incompatible sources →
  **Advisor device-boundary ruling**, no model guessed) · `O-SPEC` (all leaves covered, independent-source
  rule met, finite predictions distinguish truthful occupancy from always-`0x80` → **`PIO_FREE` change
  packet**) · `O-UNKNOWN` (focused source/queue-interface discovery).
- **Scope:** **offline only** — pinned source reads, original-XBE disassembly, bounded external sourcing.
  **No guest run, no build, no toolkit change, no instrumentation.** Existing strict logs are **prefix
  corroboration only**.
- **Forbids carried:** no synthetic completion; no source can make `0x80` true by repetition; the
  xemu-derived `return 0x80` is a **negative control**, not independent support; do not relabel the
  variable `(v>>2)<reg` threshold as constant; `0xFFFFB3` stays `UNRESOLVED`; `A4b2-r7`/`A4b2-r8`/`A4b1-r4`
  not reopened; any toolkit advance requires **re-establishing the P4 discovery-transfer bridge** first.
- **A2h:** named as prerequisite **only** for a later claim needing **trapped** strict observation beyond
  the captured pre-OOM prefix. The packet's own decision is decidable **offline** and needs **neither**
  runtime horizon.

### The Session's measured inputs to this packet (all committed)

| Record | What it establishes |
|---|---|
| `docs/reviews/pio-free-device-boundary.md` | The route and current implementation, from pinned source + original XBE: the identity, the constant-`0x80` pretence, the **synchronous** write path (no queue), the guest's two threshold forms, the poll-then-push shape to `NV1BA0_PIO_SET_HRTF_HEADROOM`, and the untrapped counter route gated at `xbox_memory_layout.c:890`. **Explicitly NOT independent hardware evidence.** |
| `docs/reviews/pio-free-strict-horizon.md` | All 20 archived strict runs: the OOM tracks the **recent-build + trap** combination; and the **corrected** finding that the untrapped 31.71 s is a **hang in the pending-word spin** (`F=2`, `Wf0=3`), not budget. Carries a correction banner for the Session's three errors. |
| `docs/reviews/pio-free-model-r2-session-verification.md` | The promotion record and the three bounded revisions required. |

**Next authorized action:** **execute `PIO_FREE-model-r2`** — the four Experiments, offline, producing
`docs/reviews/pio-free-model-execution-evidence.md` and a first-match row; then §5.8 acceptance.

**Toolkit:** `c151d4e` (unchanged — **no toolkit change is needed or permitted** by this packet).
**Game:** pending commit.

---

## Previous — `A4b2-r8` **CLOSED 2026-09-27** (`ACCEPT`, stage 1, final)

**Acceptance transaction reconciled and closed.** The Hy4 stage-1 review is **complete and durably
recorded**; the Session did **not** launch a duplicate review.

| Item | Value |
|---|---|
| Disposition | **`ACCEPT`**, **`BLOCKING: NONE`** |
| Criterion verdicts | **all 10 `AGREED`** — `P1`–`P4` and all six `AC`s |
| `DISAGREED` / `CANNOT VERIFY` | **0 occurrences** |
| Packet hash | independently reconfirmed by the reviewer: `4D4AFC30…397C62` |
| Stage 2 | **correctly NOT run** — §2.2: *"A first-stage `ACCEPT` is final and is not passed on."* |
| Recorded in | `docs/reviews/a4b2-r8-acceptance-review.md`, `docs/reviews/a4b2-r8-acceptance-record.md` |
| Closed at | game `510da98`, toolkit `c151d4e` (clean; origin synced; upstream `766ecef` untouched) |

**`A4b2-r8` is accepted and closed.** `A4b2-r7` remains `R2-EXPL-INPUT`; `A4b1-r4` untouched. Neither may
be reopened absent a workflow-valid ground (`PREMISE_CHANGED`).

**Carried constraints for the next work** (owner-stated, and consistent with the records):

- The accepted `A4b2` claim establishes the **GP-engine `3→0` transition under its bounded contract** — it
  does **not** establish guest observation of the zero, spin exit, or later liveness.
- The **non-reliance discovery is an established dependency** unless one of its pinned premises changes.
- **P4 is toolkit-identity-sensitive**: any toolkit advance requires **re-establishing the discovery-transfer
  bridge** before P4 can be inherited.
- **Strict execution currently terminates in the known A2h heap-OOM class at ≈4.8 s** despite a longer
  requested deadline. This is an **observed current horizon, not a guaranteed constant** — and per the
  Hy4 advisory it is *"not a violation"* for `A4b2-r8` because every decision input is log-bound and
  emitted before it.

### Next authorized work — the `PIO_FREE` model

`PIO_FREE` is the next admissible PIO/device-model boundary required **before any stronger strict
boot/progress claim**. Its purpose is to establish that boundary — **not** to introduce synthetic completion
merely to obtain progress.

**Starting context:** `A4p` was ACCEPTED with **`O-GATE`** on XBE
`FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` — all 28 direct reads of
`0xFE820010` are gate-only under `C1`–`C4` (`docs/packets/a4p-pio-gate-analysis.md`,
`docs/reviews/a4p-execution-evidence.md`). `A4p`'s own limits stand: it rests on an **inferred**
register-convention premise, it cannot see a whole-program custom convention, it covers **only the 28
direct reads** (not register-indirect/computed/table-driven access beyond E3), it is **static**, and it
never claims that audio, the GP, or anything else works.

**`A4b2-r8`'s own text** names the next step: a **`PIO_FREE` model packet before a strict liveness/boot
claim beyond the bounded `A4b2` contract**, and lists the `PIO_FREE` model as an explicit **non-goal of
r8** (so it is new packet work, not a revision of r8).

**Planning is in progress** — the Planner is determining, **from the actual unknowns**, whether this is a
**discovery** or a **change** packet, per the owner's instruction. A shape preflight is required only if it
is a **new change packet**; a discovery packet follows the §5.8 path and must not be given a manufactured
preflight requirement.

---

## Previous — `A4b2-r8` **ACCEPTED 2026-09-27** (`ACCEPT`, stage 1, final — no second stage)

**`A4b2-r8` executed → `R2-PASS` → acceptance stage 1 returned `ACCEPT` with `BLOCKING: NONE`.** All six
mandatory criteria and all four preconditions `AGREED`. Acceptance record:
`docs/reviews/a4b2-r8-acceptance-record.md`; reviewer: `docs/reviews/a4b2-r8-acceptance-review.md` (368
lines, child `c3d3eba7-…`, `workbuddy-ai/hy4-preview-f` @ `high`).

**No second stage — and that is required, not an omission.** §2.2: *"The second stage runs only when the
first-stage review does not return `ACCEPT`. A first-stage `ACCEPT` is final and is not passed on."* So the
DeepSeek Max reproducer is **correctly not run**, and the Sol High adjudicator is not engaged (it exists
only for a dispute surviving both stages).

**The reviewer reproduced rather than accepted.** It rebuilt the `AC-INPUTS` blocks independently (*"I did
NOT accept 'I fixed my parser'"*), checked the **full arrays** (PERIPH 128 with 125 zero, MIXBUF, FIFO, all
four DMA classes — *"NO OTHER nonzero stub index/class exists"*), verified the wording compliance, ran the
`AC-NOCPU` absence check **with a positive control**, reproduced the 371-word image comparison **and its
negative control**, and **measured P4's bridge** by diffing the discovery toolkit against r8's (`--numstat`
165/0 and 26/0 — purely additive, env-gated, never enabled in R1, so the relevant source is
byte-identical). It confirmed the forbidden bridges were **not** used.

**Advisories recorded (outside the contract, disposition unchanged).** The most consequential: **R1 ended
`unhandled_exception` (`0xE0424943`) at 4.77 s, not the 30 s deadline** — the known A2h heap-OOM class. It
is **not** a violation (the packet sets no R1 liveness requirement and every decision input is log-bound
and already emitted), but **R1 is a 4.8 s prefix of a 30 s window and this OOM class will bound other
strict criteria.** Also: block-24 identity still rests on one historical latch line; and **any future
toolkit advance re-opens P4 and cannot be inherited silently.**

**Next authorized action: the `PIO_FREE` model**, which the Advisor confirmed is genuinely next.

**Toolkit:** `c151d4e` (unchanged — no toolkit change was needed). **Game:** pending commit.

---

## Previous — `A4b2-r8` **EXECUTED 2026-09-27 → `R2-PASS`** (the claim is established for this revision)

**`A4b2-r8` (`4D4AFC30…397C62`) executed. Row: `R2-PASS`.** Preconditions **P1–P4 all PASS**:
`docs/reviews/a4b2-r8-preconditions.md`. Execution evidence: `docs/reviews/a4b2-r8-execution-evidence.md`.

### Identity

| Item | Value |
|---|---|
| Game | `b3f22cb210f453940953fc357f00ab4d57e7d259` |
| Toolkit (both builds) | **`c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`**, clean — the discovery-final commit (P2) |
| XBE | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` |
| **exe (R1 and R0)** | **`BC8E288DD54D8A09DA1630AB933EB1A808AEC9C60CEE2C64B9742B5C0CB8DC51`** |
| R1 / R0 | `20260927-160330-655-a4b2-gp-trap-trace` / `20260927-160335-562-a4b2-default` — **both STRICT** |
| ctest | **18/18** |

### The decision record

`GP_CLEAR seq=198852 va=803C0810 observed=00000003 payload=00000000 dsp_addr=000800`, with
**`CPU_ANCHOR.seq=198851 < GP_CLEAR.seq=198852`**. **The tuple matches the proved exchange.**

### All six criteria PASS

| AC | Result | Basis |
|---|---|---|
| **AC-DEFAULT2** | **PASS** | R0 `diagnostic_deadline`, `Wf0=3`, `F0=2`, **0** `[GP*]` lines, TRAP/TRACE absent, R0 mapping `matches:1 content-mismatch:0` |
| **AC-BOOT** | **PASS** | `N=1` complete block, last counts `boots=1` reconciles, `gprst=3`, `prev=1` (both clear), `sge0_va=803C0000`=T, **371 words compared vs `I`, 0 mismatches**; negative control (offset `0x1A7D64`) differs on all 371 with word 0 differing |
| **AC-RUN** | **PASS** | `boots=1`, `gp_frames=768`, `gp_insns=33120534` |
| **AC-CLEAR** | **PASS** | step 2: correct VA, `observed=3`, `insns>0`, `dsp_addr` recorded, anchor before clear |
| **AC-NOCPU** | **PASS** | no ack token in exe or source; **0** `CPU_ZERO` latches (no contested zero); **six-site reconciliation `6 == 6`**, `N_SITES=16≥6`, every site matched to its XBE instruction |
| **AC-INPUTS** | **PASS** | steps 1–2 valid (**6/6** `at_clear` and **9/9** `summary` blocks complete after reconstruction; all blocks identical; **0** monotonicity violations; `out_of_universe=0`, `boot_scratch_read=1`, FIFO all zero); step 3: only the two **proved-non-reliant** classes are nonzero (`PERIPH 0x33` = 1017, MIXBUF 26 bins / `mixbuf_stub_read=1`), **every other stub counter/flag is zero** |

**The two exempted classes are recorded as stub reads — never described as modelled or guest-written.**

### What is established, and what is not

**Established** (only what the packet's Claim permits): in one strict run with `RECOMP_GPU_ACK=0`,
`RECOMP_APU_TRAP=1` and observation-only APU trace, the `3→0` transition at `B+0x810` was performed by the
GP engine's memory-write path while executing validated image `I`, after the anchored store was recorded,
with no synthetic ack or instrumented competing CPU zero — with the **restated input qualifier** the
discovery earned.

**Not established:** no guest observation of the `0`, no spin exit or progress past `loc_001A18D0`, no
boot/liveness/audio/timing/instruction-level-correctness claim. `CPU_ANCHOR` corroborates the exchanged
`3`'s identity without proving it supplied it. `LOW_RAM`/`CONTIG` DMA is guest-written **by region, not by
writer**. `0x56` is a timing heuristic. `unk2`/`unk13`, the `format` default and `dsp_offset` range
fall-throughs remain **unaudited**.

**`A4b2-r7` remains `R2-EXPL-INPUT` — this is not a retroactive PASS of r7.**

### Two honest notes

1. **R1's dump is `CONTENT_MISMATCH`** (the known A2h displacement), so **`Wf` is not reported from R1** and
   nothing is routed to CPU/`UNATTRIBUTED` from it — the packet's rule. **`R2-PASS` rests on the latch and
   counts**, which are log-bound. R0's dump **is** mapping-valid, which `AC-DEFAULT2` needs.
2. **Six Session parser errors** were found and corrected while evaluating `AC-INPUTS`; all produced
   **false** step-1 results that would have forced an unnecessary `R2-UNKNOWN`. They are recorded in the
   evidence record so a reader does not mistake the corrected result for one that was right first time.

**Next authorized action:** the packet's **Closure** section — then **`Hy4` acceptance stage 1** →
**DeepSeek Max stage 2** for any non-AGREE criteria → **fresh Sol High adjudication** for any remaining
frozen-contract dispute. **Then `PIO_FREE`**, which the Advisor confirmed is genuinely next.

**Toolkit:** pushed `c151d4e` (unchanged — no toolkit change was needed). **Game:** pending commit.

---

## Previous — `A4b2-r8` (**change**) — **PROMOTED 2026-09-27, `ADEQUATE`** (now executed → `R2-PASS`)

- **Packet:** `docs/packets/a4b2-gp-clears-pending-word.md`, revision **`A4b2-r8`**, class **change**,
  frozen SHA-256 **`4D4AFC304F571971EB180C19D6832D56A2CC62FF6EAAFB9E4928716125397C62`** (**161 lines**).
  **This is the packet to execute.** Promotion byte-identical with no revision (§5.3); hash verified
  **3× over 10 s** immediately before, matching the author's report and the reviewer's independent check.
- **Adequacy:** **`VERDICT: ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS BOUNDED` — by a
  **non-authoring** Planner (child `bedc4a02-…`), as a **change** packet requires under §5.3. The authoring
  Planner flagged that its own self-check was **not** the required review, and the Session did **not**
  freeze on it. Review: `docs/reviews/a4b2-r8-adequacy-review.md`. Verification:
  `docs/reviews/a4b2-r8-session-verification.md`.
- **What it carries** (the Advisor's Q3 list): the terminal conditional **`O-TWO-LEG`** after clean V2 with
  `L1=PROVEN`/`L2=INVARIANT` + the earlier input-specific conditional ruling; **`r7` stays
  `R2-EXPL-INPUT`, no retroactivity**; the **block-24 identity with corrected radix** (`%04X` is hex, so
  `0018 = 0x18 = block 24`; the old `P 0007`/`x:[6..10]` identifier expressly retired); the **leaf table**;
  and the **second-image boundary** (interface enumerated per V2, **internals unanalyzed and unneeded**,
  `B3-ID` deferred).
- **The substantive change** — the restated **`AC-INPUTS`** qualifier — was ruled by the reviewer as
  *"sound only as exchange-specific substitution under recorded Advisor conditional ruling, **not a value
  model or retroactive PASS**"*: `r7` stays `R2-EXPL-INPUT`, **all other stubs retain `FAIL`**, and P2/P4
  **stop** on unverifiable proof transfer. The two named stub classes get **non-reliance by measurement**,
  **not** a relabelling as modelled.
- **Verified by the reviewer:** packet hash matched before and after; **25 manifest entries checked, zero
  missing or mismatched**; all four Q3 items carried; **neither** DMA chain-restart theory **nor**
  region-counter tallies used for block identity; the seven acknowledged analysis errors *"do not defeat
  independently measured closure."*
- **Two `DEFERRED` items recorded, not dismissed** (both go into the r8 execution record): (1)
  `independent-block24.py`'s final verdict is **hard-coded always-positive** and its stub check is textual —
  it is **not** an independent proof, and the packet relies on the **measured** evidence instead; (2)
  `v2-cross-boundary.out.txt` individually says `NOT CLEAN`, honest for what it computes, with the
  resolution in `v2-pre-vs-post-exchange.out.txt`.

**Next authorized action:** **execute `A4b2-r8`** — implement/run, classify by its decision rows, then
**Hy4 acceptance stage 1** → **DeepSeek Max stage 2** for any non-AGREE criteria → **fresh Sol High
adjudication** for any remaining frozen-contract dispute. **Then `PIO_FREE`**, which the Advisor confirmed
is genuinely next.

**Toolkit:** pushed `c151d4e`. **Game:** `28a51db`.

---

## Previous — **`A4b2-r8` DRAFT committed, awaiting non-authoring §5.3 adequacy** (superseded by promotion)

- **Packet:** `docs/packets/a4b2-gp-clears-pending-word.md`, revision **`A4b2-r8`**, class **change**,
  **161 lines**, SHA-256 **`4D4AFC304F571971EB180C19D6832D56A2CC62FF6EAAFB9E4928716125397C62`** — committed
  as a **draft** at `28a51db`, **NOT frozen and NOT promoted into `CURRENT PACKET`**.
- **Why not promoted:** `A4b2` is a **change packet**, not a discovery packet. Under §5.8 a discovery
  packet's adequacy may be the writing Planner's own review — which is what the Session used for the five
  discovery packets. **A change packet requires the full §5.3 adequacy review by a NON-AUTHORING Planner**,
  and the authoring Planner said so explicitly. Freezing on the author's self-check would skip a required
  gate. **A fresh non-authoring reviewer is running now.**
- **What r8 carries** (the Advisor's Q3 list): the terminal conditional **`O-TWO-LEG`** after clean V2 with
  `L1=PROVEN`/`L2=INVARIANT` + the earlier input-specific conditional ruling; **`r7` stays
  `R2-EXPL-INPUT`, no retroactivity**; the **block-24 identity with corrected radix** (`%04X` is hex, so
  `0018 = 0x18 = block 24`, explicitly rejecting the old `P 0007`/`x:[6..10]` identifier); the **leaf
  table**; and the **second-image boundary** (interface enumerated per V2, **internals intentionally
  unanalyzed and unneeded**, `B3-ID` deferred).
- **The substantive change** is the restated **`AC-INPUTS`** qualifier: only **clear-relevant feasible
  reaching** inputs need guest-written/modelled provenance, while the two named stub classes (MIXBUF,
  `0xFFFFB3`) instead have completed two-leg **non-reliance** — and are **not** relabelled as modelled.
- **Forbids carried:** no region-counter tallies for block identity; **no DMA chain-restart theory**; no
  fifth `L1` (`F-D`); `F1`–`F3` and `W1`–`W4`; no `0xFFFFB3` identification or model. **All seven
  self-caught analysis errors are acknowledged rather than glossed.**
- **Two gaps the authoring Planner found before freeze, both closed:** R3's **archived outputs and hashes**
  (`docs/reviews/a4b2-epoch-slice-verification-scripts/`, 18 files incl. `MANIFEST.sha256`), and the stale
  `(decimal 24)` in the evidence record.

**Next:** promotion on `ADEQUATE` → implement/execute `A4b2-r8` → **Hy4 acceptance stage 1** →
DeepSeek Max stage 2 for non-AGREE criteria → fresh Sol High adjudication for any remaining frozen-contract
dispute. **Then `PIO_FREE`**, which the Advisor confirmed is genuinely next.

**Toolkit:** pushed `c151d4e`. **Game:** `28a51db`.

---

## Previous — `A4b2-NR-epoch-slice-followup-r1` (**discovery**: the FINAL `L1` attempt) — **EXECUTED 2026-09-27 → `O-TWO-LEG`**

### ⚠ `A4b2-r8` MUST NOT inherit the old descriptor identifier

**The Session had identified the wrong descriptor for four packets.** `x:[6..10]` (the `P 0007` call) was
called "the doorbell descriptor" throughout; **the exchange is produced by block 24** (the `P 000E` call).
Erratum: **`docs/reviews/a4b2-descriptor-identity-erratum.md`** — lists the four frozen packets that carry
the stale identifier (not edited; a frozen contract is the artifact its revision executed under), what is
unaffected (the disjointness analysis, the field values, `L2`, image-`I` stability, the entry proof), and
that **no `§5.4(2)` is raised** because no contract premised the identity.

**Next authorized action:** **`A4b2-r8`** if the Advisor confirms `O-TWO-LEG`; otherwise the Advisor's final
scope/defer/retire decision. **A fifth `L1` packet is forbidden (`F-D`).** `A4b2-r7` stays
`R2-EXPL-INPUT`; no strict criterion discharged.

**⚠ ADVISOR ROUTE OUTAGE.** The terminal referral was attempted **four times** — three
`MODEL_UNAVAILABLE` (model stream idle timeout, `retryable: false`) and a fourth still `in_flight`
server-side. **No substitute model was used**, per the owner's staffing authority. Recorded in
`docs/reviews/a4b2-epoch-slice-advisor-route-outage.md`. **The Session did NOT open `A4b2-r8` on its own
recommendation** — the packet's terminal row and the Advisor's terminality ruling route that decision to the
Advisor, and an unavailable route is not converted into a self-granted one.

**Toolkit:** pushed `c151d4e` (DMA descriptor trace + scratch-field measurement). **Game:** `132a0cc`.

---

## Previous packet — `A4b2-NR-epoch-slice-followup-r1` (**discovery**: the FINAL `L1` attempt — backward demand-driven slice) — **PROMOTED 2026-09-27, `ADEQUATE`**

- **Packet:** `docs/packets/a4b2-nr-epoch-slice-followup.md`, revision
  **`A4b2-NR-epoch-slice-followup-r1`**, class **discovery**, frozen SHA-256
  **`03CE475DA2F48618BE93E6803F3168732A416DC38D00C4E649252BD87F6FEBA6`** (**34 lines**).
  **This is the packet to execute.** Promotion byte-identical with no revision (§5.3); hash read **3× over
  9 s**, identical, and matching the Planner's own reported hash, after the author confirmed finished.
  Verification: `docs/reviews/a4b2-nr-epoch-slice-followup-r1-session-verification.md`.
- **Adequacy:** **`VERDICT: ADEQUATE`** — the **writing Planner's own** review (§5.8), child
  `f151b920-f96e-4fcb-b8fc-ea2e0fd71397`, `codex/gpt-6-sol` @ `high`. No second Planner.
- **⚠ THIS IS THE LAST `L1` PACKET — terminality binds.** The Advisor ruled
  (`docs/reviews/a4b2-nr-epoch-slice-advisor-ruling.md`) that the chain ends **either way**:
  `O-TWO-LEG` → `A4b2-r8`; `O-REFUTED` → death branch (`A4b-VP-real-implementation` **and**
  `A4b-B3-register-resolution`); `O-INCONCLUSIVE` → **back to the Advisor** for a final
  scope/defer/retire decision. **A fifth `L1` attempt is FORBIDDEN (`F-D`).**
- **Why the method changed:** four consecutive `O-INCONCLUSIVE` rows, each closing its target but each
  discovering **more program**. The Advisor's diagnosis: the repeated shape was **forward whole-program
  enumeration**, which **diverges by construction on a growing program**. The `L1=PROVEN` bar was always
  correct and **interface-shaped**; the packet language pushed the Sessions into forward CFGs. **The claim
  is NOT narrowed — the analysis direction is reversed.**
- **The method:** **backward demand-driven slice** from the five doorbell field definitions + trigger
  guards; expand **only** through may-reaching definitions; classify **every** frontier leaf as
  **immediate / guest-written / modelled → CLOSED**, **stub-derived with a concrete feasible causal chain →
  REFUTED**, or **unresolved → UNKNOWN with the precise node/address/edge/missing witness**. A mere read,
  observed invariance, or unreachable trace does **not** close or refute.
- **Advisor-mandated elements, all present:** descriptor-disjointness **proved alias-closed** (not assumed);
  **interrupt scoping mandatory incl. fault vectors** (*"do not exclude fault vectors merely because they
  were not observed"*); **entry-set proof** (reset + interrupt vectors + bounded second-image targets *into
  slice nodes*); **B3 enumeration over correct bytes** (*"without asserting a total of four"*);
  zero/fill decodability with a wrong-version control; **second-image byte provenance** if reached;
  **executed counts as cross-check ONLY** (`F-C`).
- **Forbids carried:** `F-A` no forward whole-program enumeration; `F-B` no assumed disjointness; `F-C` no
  ×1-as-feasibility-exclusion; `F-D` no fifth `L1` round; plus `F1`–`F3` and `W1`–`W4`.
- **Session preparation already done** (not a substitute for the packet's criteria):
  - `docs/reviews/a4b2-nr-epoch-slice-interrupt-preparation.md` — **R4 answered**: no interrupt machinery in
    either image; the interrupt table has only 4 entries (Reset/Illegal/Stack Error/Trap) with **no
    peripheral/DMA/timer/audio raiser**. The 194-of-2 340 gap is therefore **cross-image**, not
    interrupt-driven.
  - `docs/reviews/a4b2-nr-epoch-slice-disjointness-preparation.md` — **R3 advanced**: 5 builder call sites
    enumerated with statically known `r0` (`x:[6..10]`, `[18..22]`, `[12..16]`, `[24..28]`, `[30..34]`),
    all 10 pairs disjoint; all 39 direct X writes enumerated, **none** in `x:[6..10]`; the two extra builder
    blocks are **statically unreachable** (no incoming target, `rts` predecessor, 0 executions). **Only the
    `P 0007` call reaches the doorbell region.** Remaining UNKNOWN: the computed writers and the **reader**.

---

## Previous packet — `A4b2-NR-next-edge-followup-r1` **EXECUTED 2026-09-27 → `O-INCONCLUSIVE`** (phase 1 complete, closure verified, slice attempted and bounded)

**`A4b2-NR-next-edge-followup-r1` (`62A1BB38…`) executed.** Row: **`O-INCONCLUSIVE`**. Evidence:
`docs/reviews/a4b2-nr-next-edge-followup-phase1-evidence.md`. Verification:
`docs/reviews/a4b2-nr-next-edge-followup-r1-session-verification.md`.

### Phase 1 — complete, and it answered the questions the Advisor made gating

| Finding | Result |
|---|---|
| **N1 — second GPRST bootstrap?** | **NO.** From existing logs before any new run: `boots=1`, exactly one `[GPBOOT] n=1` block. |
| **The loader** | A **DMA transfer the GP itself triggers** at `P 011C` (`movep #$000001,x:$ffffd6` = DMA_CONTROL). **Correction:** `P 011C` executes ×2 306, so the 3 512 writes are *not* one bulk pass from one trigger — the accurate statement is about the **destination addresses**. |
| **The load's shape** | **Strictly increasing, contiguous, ZERO duplicate writes** over `0x171`–`0x0F28`; even 878/878/878/878 across the window; completes inside it. Old values: `0xCACACA` ×1 833, `zero` ×1 678, one other. |
| **Image `I` stability** | **Only 2 writes, both at the boundary** (`0x171`, `0x172`). **ZERO writes in `0x000`–`0x170`** — including every doorbell-path instruction. Closes the Advisor's Q2 transient gap **by watch, not by sample**. |
| **Why two snapshots could not have shown this** | The region was being written **during** the interval between snapshots — now measured rather than argued. |

**Closure — performed and verified.** `bad-output` arm **removed** (bite preserved and archived:
`GP_CLEAR=0`, `GP_NONZERO_OVER=1`); new watch fixture arm (xiii) added; **closure control PASSED** — one
fresh absent-gate baseline at the final identity with **zero** `[GPPERTURB]`/`[GPDECODE]`/`[GPB9]`/
`[GPWRITE]` lines, no artifacts, tuple unchanged (`seq=198852 … dsp_addr=000800`). **ctest 18/18 green.**

### Phase 2 — the slice, on phase-1-covered bytes, and its honest verdict

The gate is satisfied **for image `I`**, so the slice proceeded on those bytes only (`F2` honoured). CFG:
239 nodes, **194 reachable from entry**, **0 UNKNOWN control transfers**. **`0xFFFFB3`'s four reads walk
2–3 instructions into scratch and reach NO doorbell site.**

**The same trap, walked into and caught.** The slice reports *"MIXBUF operand references inside image I:
0"* — **true and misleading**, exactly the error recorded in `a4b2-nonreliance-discovery-evidence.md` §3.
**The mixbin table `P 00CC`–`00D1` is *inside* image `I`** (`0xCC` ≤ `0x172`) and its six words **are
MIXBUF addresses** (`0x1400`, `0x1440`, `0x1420`, `0x1480`, `0x14A0`, `0x1460` = bins 0,2,1,4,5,3), read by
`movem p:(r2)+,x0`. **An operand scan cannot see them because they are data.** So MIXBUF reaches the GP by
a **table-and-DMA path inside the slice** — the "0 references" line must not be read as "MIXBUF is not
consumed".

**Row `O-INCONCLUSIVE`.** Not `O-TWO-LEG` — phase-1 coverage is achieved but the **reaching-definitions
closure is not**: (1) the mixbin table-and-DMA dependency is unproven; (2) the doorbell path's **callers and
guards** remain open (`F1`: instruction survival ≠ path survival); (3) the slice is over image `I` only. Not
`O-REFUTED` — no concrete feasible causal chain to the named doorbell was found.

**Next authorized action:** **`A4b2-NR-epoch-slice-followup`** — the row the packet's own table names —
targeted at the **mixbin table-and-DMA def-use closure** and the **doorbell path's callers/guards**.
`A4b2-r7` stays `R2-EXPL-INPUT`; no strict criterion discharged.

**Toolkit:** pushed `c20fc75` (watch) → `aaedd57` (bad-output removal + fixture). **Game:** `e13a68b`,
`11e7d8f`.

---

## Previous packet — `A4b2-NR-next-edge-followup-r1` (**discovery**: GP load epochs before any slice) — **PROMOTED 2026-09-27, `ADEQUATE`**

- **Packet:** `docs/packets/a4b2-nr-next-edge-followup.md`, revision **`A4b2-NR-next-edge-followup-r1`**,
  class **discovery**, frozen SHA-256
  **`62A1BB38E4EA6FE4877EEDB6E37B27EFD01C88C50FFECC7924C7EF8F1226B3EE`** (**41 lines**).
  **This is the packet to execute.** Promotion byte-identical with no revision (§5.3), and **verified
  stable before promoting** (hash read 3× over 6 s, identical; author confirmed finished) — the mid-write
  defect from the previous packet was **not** repeated. Verification:
  `docs/reviews/a4b2-nr-next-edge-followup-r1-session-verification.md`.
- **Adequacy:** **`VERDICT: ADEQUATE`** — the **writing Planner's own** review, as §5.8 requires for a
  discovery packet (child `5416361e-11f8-4986-ae22-34a80d41c48d`, `codex/gpt-6-sol` @ `high`). No second
  Planner; no Muse preflight repeated.
- **Why it exists:** `A4b2-NR-next-edge-r1` → **`O-INCONCLUSIVE`** after finding the GP **loads a second
  program into P-memory after the bootstrap**. The Advisor mandated **loader/epochs FIRST, slice SECOND**;
  this packet is that phased work. Evidence: `docs/reviews/a4b2-nr-next-edge-execution-evidence.md`; binding
  ruling: `docs/reviews/a4b2-nr-next-edge-advisor-ruling.md`.
- **Phase 1a** — rule a second GPRST bootstrap in/out **from existing logs before any new run**.
- **Phase 1b** — finish the GP-only `RECOMP_APU_PWRITE_WATCH`, with **complete** coverage accounting for the
  direct PRAM-write bypasses (`dsp_c_bootstrap` scratch-to-PRAM, `sync_from_VM` copy, initial fill): show
  they cannot modify PRAM inside the claimed interval **or** cover each with a narrow hook, **failing
  closed** otherwise. Fixture a known image-`I` write and a known above-`I` write; reconcile terminal
  counts to **every** ordinal; ordinal gap / unresolved bypass / wrong identity → **UNKNOWN, never
  quiescence**.
- **Phase 1c** — event/epoch proof: loader trigger, **actual source bytes**, extent, per-word deltas, epoch
  boundaries, and executed PCs bound to **the word version live when fetched**. Image `I` must show zero
  writes from the authenticated bootstrap boundary through the first exchange, or every modification must
  be enumerated with timing.
- **Phase 2** — the feasible CFG/def-use slice, **only** on phase-1-covered stable bytes or per fully
  covered epoch. `L1=PROVEN` needs closure of all feasible named-output/guard reaching definitions against
  both input classes plus image-`I` watch coverage and carried `L2`; `L1=REFUTED` needs a **concrete
  feasible causal chain**, not an in-range read.
- **Binding forbids carried:** **F1** never cite doorbell byte-identity as path-independence
  (*instruction survival ≠ path survival*); **F2** never slice directly from the at-exchange snapshot;
  **F3** never fudge a single-image slice if PRAM never quiesces. **W1–W4** withdraw/re-scope list also
  carried.
- **Closure:** `bad-output` removal (bite archived and re-verified); **exactly one** fresh absent-gate
  baseline doubling as the final closure control; diagnostics off by default; no active watch left behind.
- **Session phase-1 progress already made** (not a substitute for the packet's own criteria):
  `docs/reviews/a4b2-nr-next-edge-followup-phase1-evidence.md` — **N1 answered negative from existing logs**
  (`boots=1`, one `[GPBOOT]` block); the **loader identified** as a DMA transfer the GP itself triggers at
  **`P 011C`**, writing 3 512 words once each sequentially `0x171`–`0x0F28`; **image `I` stable** (only 2
  writes, both at the boundary `0x171`/`0x172`; **zero** writes in `0x000`–`0x170`, including every
  doorbell-path instruction) — which closes the Advisor's Q2 transient gap by **watch**, not by sample.
  Watch committed at toolkit `c20fc75` with the GP-only filter and bypass-coverage hooks.

---

## Previous packet — `A4b2-NR-next-edge-r1` **EXECUTED 2026-09-27 → `O-INCONCLUSIVE`; the GP loads a SECOND program image**

**`A4b2-NR-next-edge-r1` (`7461AAA4…`) executed. Row: `O-INCONCLUSIVE`.** Evidence:
`docs/reviews/a4b2-nr-next-edge-execution-evidence.md`. Verification:
`docs/reviews/a4b2-nr-next-edge-r1-session-verification.md`.

### The headline finding: there are TWO P-memory images, and the analysis used the wrong one

The packet's premise was that the executed program is the boot image `I` (371 words) plus a region above
it, and that the predecessor's error was analysing only its first 512 words. **That premise was
incomplete.** Two full-PRAM decodes — one at bootstrap, one at the first exchange — show the GP **loads a
second program into P-memory after the bootstrap**:

| P-memory band | At bootstrap | At the first exchange |
|---|---|---|
| boot image `I` (`0x000`–`0x172`) | real code | **real code — unchanged** |
| `0x173`–`0x7FF` | **1 677 zeros** | **real code** (25 zeros) |
| `0x800`–`0xFFF` | **2 048 × `0xCACACA`** | **real code** (215 × `0xCACACA`) |

`0xCACACA` is the toolkit's own `memset` fill (`dsp_cpu.c:299-301`), so `0x800`–`0xFFF` at bootstrap is
**uninitialised fill**, not program. Of 2 957 words in both snapshots, **2 480 changed**.

**Measured against the bootstrap snapshot:** only **193 of 2 340 executed PCs (8.2%)** lie inside image
`I`. **99.4% of all executions** were at a PC whose bootstrap word was zero (**42.0%**) or `0xCACACA`
fill (**57.4%**). So the bootstrap decode was not merely incomplete — for those PCs it was **wrong**, and
the CFG in the evidence record, though methodologically sound, was built on those wrong bytes.

### What survives: the doorbell code is in the FIRST image

The doorbell's instructions are **word-for-word identical** in both snapshots — `P 0004` `62F400`
(`move #$000800,r2`), `P 0007` `0BF080`, `P 00B9` `57E100`, `P 00DB` `220E00`, `P 00E8` `0A7092`. So the
doorbell descriptor path lies entirely within image `I`, the `P 0004` immediate still matches the observed
`dsp_addr=000800`, and **the predecessor's in-window doorbell readings are not invalidated** — they were
unproven as a complete account of the program, which this run now shows is larger and later-loaded.

### Row and next action

**`O-INCONCLUSIVE`.** `L2 = INVARIANT` stands (tuple unchanged: `seq=198852 va=803C0810 observed=3
payload=0 dsp_addr=000800`). `L1` is `INCONCLUSIVE`: the feasibility slice was not completed (the CFG
reaches only 194 of 2 340 executed PCs from a single entry; interrupt/vector entry is not modelled), **and
its input bytes were wrong**. No concrete feasible causal chain to the named doorbell was found, so
`O-REFUTED` is **not** selected. `A4b2-r7` stays `R2-EXPL-INPUT`; no strict criterion discharged.

**Next authorized action:** **`A4b2-NR-next-edge-followup`** — the row the packet's own table names —
targeted at the specific unknown this run recorded: **the second image's provenance, load mechanism and
extent, plus a slice built on the correct bytes.**

### Instrumentation added (env-gated, GP-only, read-only, off by default)

- `RECOMP_APU_GP_DECODE` **second decode at the first exchange** (`dsp56k_request_decode2`, tagged
  `second-decode-at-exchange`) — **this is what produced the headline finding.**
- **Per-core identity logging** (`b9_note_core_identity`): measured **one** core,
  `opaque=00000239DED16900`, `is_gp=1`, so the trace's GP attribution is **verified, not assumed**.

### Two tooling defects found and fixed in the Session's own analysis

1. **Fall-through used `pc+1`** — DSP56300 instructions are 1–*N* words, so this is wrong for every
   multi-word instruction. Fixed to the next *decoded* PC.
2. **Any `(rN)` operand treated as an indirect jump** — misclassified **2 062 data moves** as control
   transfers, yielding a bogus "2 083 unresolved edges". Only `jmp`/`jsr` with a computed operand are
   indirect. After the fix: **0 indirect, 21 unresolved (returns only)**.

### Session defect, corrected: the packet was promoted mid-write

The promotion recorded 32 lines / `9E919866…`; the Planner's final file is **39 lines / `7461AAA4…`**. The
Session polled, saw a complete-looking body with an `ADEQUATE` verdict, and promoted while the Planner's
turn was still in flight. **A file that looks finished is not evidence its author has stopped writing.**
Repaired by re-promoting against the stable bytes; the packet was never edited by the Session.

---

## Previous packet — `A4b2-NR-next-edge-r1` (**discovery**: the full-program PC-indexed CFG/def-use slice) — **PROMOTED 2026-09-27, `ADEQUATE`**

- **Packet:** `docs/packets/a4b2-nr-next-edge.md`, revision **`A4b2-NR-next-edge-r1`**, class **discovery**,
  frozen SHA-256 **`7461AAA471D5EE0B78348AC17F7BF5B8CD333A860437705B65946B349311ACFF`** (**39 lines**).
  **This is the packet to execute.** Promotion byte-identical with no revision (§5.3).
  **Session defect, corrected:** the packet was first promoted **mid-write** at a 32-line draft
  (`9E919866…`); the Planner's turn was still in flight and it extended the file to 39 lines. The hash above
  is the **final, stable** artifact (re-read twice, unchanged). The packet was never edited by the Session.
  Recorded in `docs/reviews/a4b2-nr-next-edge-r1-session-verification.md`.
- **Adequacy:** **`VERDICT: ADEQUATE`** — the **writing Planner's own** review, as §5.8 requires for a
  discovery packet (child `1aadbb20-4a2f-4a3c-9ca5-9699806d2820`, `codex/gpt-6-sol` @ `high`). No second
  Planner; no Muse preflight repeated.
- **Why it exists:** `A4b2-NR-followup-r1` executed → **`O-INCONCLUSIVE`**. The `P 00B9` gap was closed by
  measurement, but the real obstacle is now quantified: the original Leg 1 reasoned over **380 of 2 340
  executed PCs** — a 512-word window covering ~16% of the executed program (`0x0000`–`0x0F28`). Evidence:
  `docs/reviews/a4b2-nonreliance-discovery-evidence.md` §8. Session verification:
  `docs/reviews/a4b2-nr-next-edge-r1-session-verification.md`.
- **The target:** the **PC-indexed CFG/def-use slice over every feasible route** to the first doorbell
  descriptor and DMA write, across the **full 3 881-word program** (3 964 decoded instructions). The
  Advisor's Q2 **SUSPENDED / CARRIES / FORBIDDEN** lists are carried, and the 2 340-PC histogram may be
  used **only** as a lower-bound cross-check — the `PROVEN` bar stays **feasibility**-based.
- **Q4 guardrails — carried with the Session's correction:** warning at `dsp_cpu.h:49`
  (`dsp_core_t.is_gp`, never populated) and `dsp.h:79` (`DspCoreState.is_gp`, clobbered by
  `dsp_c_sync_to_vm`); **`dsp.h:105` (`DSPState.is_gp`) is CANONICAL and must not be labelled unsafe** —
  the Advisor's ruling had this reference inverted and the packet says so explicitly. Both headers are
  **comments-only** new scope.
- **Runs:** exactly **one** fresh absent-gate baseline at the pinned new identity, which doubles as the
  final closure inertness control. **Condition (Advisor Q3):** if the packet's toolkit delta executes on
  the GP path with gates absent, a fresh same-exe mini-series is required.
- **Closure:** the retained `bad-output` arm is **removed** (bite preserved and re-verified); diagnostic
  changes reverted; all gates off by default.
- **Session preparation already done:** `docs/reviews/a4b2-nr-next-edge-q2-preparation.md` re-derives all
  seven suspended items over the full program. **Six survive unchanged; the seventh (`P 00DB` "sole
  builder") is refuted as to its reason** — there are **three** callers plus two for sibling `P 00EB` — but
  the callers write **pairwise disjoint** descriptor regions (`x:[6..10]` doorbell, `x:[12..16]`,
  `x:[18..22]`), so the original *conclusion* survives on a reason the original never stated. The open
  question is the **reader**: whether a computed pointer can read another region as the doorbell's
  descriptor. Durable slice inputs are archived (`full_decode.txt`/`.json`, `gpb9_trace.txt`) and the
  decode is verified to cover **100%** of the 2 340 executed PCs.

---

## Previous packet — `A4b2-NR-followup-r1` **EXECUTED 2026-09-27 → gap 1 CLOSED; `O-INCONCLUSIVE` stands, reason now quantified**

**`A4b2-NR-followup-r1` (`886620CC…C5F5`) executed.** Gap 1 — the unresolved `P 00B9` computed read — is
**CLOSED BY MEASUREMENT**, and in closing it the run exposed a **larger** gap that now governs. Evidence:
`docs/reviews/a4b2-nonreliance-discovery-evidence.md` §8. Verification:
`docs/reviews/a4b2-nr-followup-r1-session-verification.md`.

### Gap 1: CLOSED — `P 00B9` reads internal scratch, never the mix buffer

Gate `RECOMP_APU_GP_B9_TRACE=1`. Run `20260927-132641-253-a4b2-nrf-b9-final`, rerun
`20260927-132808-143-a4b2-nrf-b9-full`. **All six executions** (matching `dor #$0006`) read effective
address **`0x24`** — internal scratch — with `mixbuf=0 alias=0` on every event:

| ord | `r1` | `x:$0000` | derived `0x80+x:$0000` | **effective** | value | mixbuf | alias |
|---|---|---|---|---|---|---|---|
| 0–5 | `000024` | `000000` | `000080` | **`000024`** | `008000`→`00A800` step `0x800` | 0 | 0 |

Terminal `events=6 execs=6 in_mixbuf=0 in_alias=0 invalid=0` — **both counters agree**, so this is a
*complete* record of every `P 00B9` execution in the bounded exchange, not a sample. Two surprises worth
recording: the live `r1` comes from **`P 00A2 move #$000024,r1`**, *not* from the `P 0086`–`P 008C`
`0x80 + x:$0000` computation the predecessor's gap was written around; and the six values are **guest DMA
pointers advancing by exactly `0x800`** (`P 00BB add #$0800,b`), i.e. the audio output walk — not the
doorbell, and not a mix-buffer read.

### The larger gap this exposed: the executed program is ~12× the decoded window

| Quantity | Value |
|---|---|
| GP distinct PCs executed | **2 340** |
| GP PCs at or above `0x200` | **2 052** |
| GP PC range | **`0000..0F28`** |
| Predecessor's decode/analysis window | `0x0000..0x01FF` (**512 words**) |
| Full-PRAM decode now available | **3 964 instructions**, 6 `<UNDECODED>` |

**The predecessor's Leg 1 reasoned over 380 of the 2 340 executed PCs (~16%), and 512 of the 3 881 words
the program spans (~13%).** That is the same partial-enumeration failure `AGENTS.md` records for the
`PIO_FREE` list (10 of 28 sites via one spelling), and it is a **stronger** ground for `L1 = INCONCLUSIVE`
than the one originally recorded. **Corrective action taken:** the decode range was widened to
`DSP_PRAM_SIZE-1` and the diagnostic PC histogram from 512 to 4096 entries, so nothing above `0x1FF` is
silently dropped. A follow-up slice now has the whole executed program available.

### Two latent toolkit defects found and fixed

1. **Decoder NULL dereference.** `lookup_opcode_slow()` ends in `assert(!"Invalid op code"); return NULL;`
   and Release `assert` is a no-op, so an unrecognised word made `disasm_instruction()` dereference NULL and
   killed the run (`0xC0000005`) — reachable only when decoding an image containing a **data table**. First
   guard attempt tested `->template` on the NULL pointer and crashed again; the fix tests the pointer.
2. **`core->is_gp` is never populated.** It is written only by `dsp_c_sync_from_vm()`, which is registered
   in the ops table (`dsp_c.c:324`) but **never called**, so the field is permanently `0`. The first B9 run
   reported `gp_exec_total=0` while its own histogram showed 288 GP-image PCs executing. The reliable source
   is `core->opaque` → `DSPState.is_gp` (set at `dsp.c:143`). **Any code classifying work by `core->is_gp`
   will silently see "not the GP"** — recorded because a future reader could repeat it.

### Row: still `O-INCONCLUSIVE`, for a now-quantified reason

`L2 = INVARIANT` is unchanged and reconfirmed (the doorbell tuple is again bit-identical:
`seq=198852 va=803C0810 observed=3 payload=0 dsp_addr=000800`). `L1` remains short of `PROVEN` — but the
`P 00B9` edge can no longer be cited, and what remains is a **scope** problem, not an unresolved edge. No
strict criterion is discharged; `A4b2-r7` stays `R2-EXPL-INPUT`.

**Next authorized action:** **`A4b2-NR-next-edge`** — the row the followup's own outcome table names for
`O-INCONCLUSIVE` — scoped to the **full executed program** (`0x0000`–`0x0F28`, 2 340 PCs, 3 964 decoded
instructions), building the PC-indexed CFG/def-use slice over every feasible route to the first doorbell
descriptor and DMA write.

### Advisor ruling on this execution — binding on the next packet

`docs/reviews/a4b2-nr-followup-advisor-ruling.md` (handle `muse_FkNhGaXtV9P5`, `muse-spark-1.3-contributor`,
effort `max`). **Row call `O-INCONCLUSIVE` CONFIRMED.** No contract impact; **no `§5.4(2)` anywhere** — the
predecessor never premised "the program is 512 words", so nothing downstream relied on an invalidated
premise. The scope defect only strengthens the direction already recorded.

**The `A4b2-NR-next-edge` Planner brief MUST carry the Advisor's Q2 lists.**

**SUSPENDED — every completeness/negative claim from the 512-word analysis. What died is every
"only / never / every / no-other" uttered over a 16% enumeration.** Must be re-derived over the full
3 881-word program before next-edge may rely on any of it:

1. All `0xFFFFB3` read sites — the **"four reads" count is forbidden** until re-enumerated.
2. Full def-use of `x:$007c`–`x:$007f` — **"loop-control-only" suspended** (upper code may read them).
3. All callers of builder `P 00DB` + all reaching definitions of `r0`–`r3` at entry — **"sole builder /
   all-immediate" suspended**.
4. All DMA-register writes + trigger sequences — **"sole trigger" suspended**.
5. All X-writes to `$0000`–`$0004` — **"mailbox never written" suspended**.
6. All branch/call targets program-wide — the **"none targets `00CC`–`00D1`" *feasibility* ground
   suspended** (its *execution* ground stands).
7. `r1` reaching-definitions at `P 00B9` including `P 008C` and any upper writers — feasibility closure of
   the measured edge (the trace answers this run's reads, not all feasible mailbox values).

**CARRIES without re-derivation:** the 512-word decode bytes; the `op =` execution-absence
(window-independent: the interpreter decodes on opcache miss during execution, so every executed PC was
decoded, and `op =` fired 0× across execution runs against the decode run's 6× positive control); the
`P 00B9` measurement table; the six table words' values and bin-alignment arithmetic; the doorbell tuple;
literal readings (`P 0004`/`P 000B`, `dor #$0006`); per-instruction semantics within the window.

**FORBIDDEN:** citing any 512-word-era "only/never/every" claim; citing `P 00B9` as open; treating trace
absence as feasibility proof. The 2 340-PC histogram may be used **only** as a cross-check lower bound
(executed ⊆ slice-covered) — the `PROVEN` bar remains **feasibility**-based.

**`L2` status:** the r2 four-arm same-exe series **STANDS** as the `L2` core (window-independent). The
eight followup runs are **admissible as cross-build transfer/robustness corroboration but FORBIDDEN as
perturbation evidence** — they test instrumentation/build-invariance, a different counterfactual. They
bridge r2's `L2` to the next identity, so only **one** fresh absent-gate baseline is required there (the
closure control doubles as it). **Condition:** if next-edge changes GP-path behaviour with gates absent, a
fresh same-exe mini-series is required (baseline + one changed-input arm with changed-counters +
comparator-validity recheck).

**`bad-output` removal: authorized** (bite preserved and re-verified).

**Q4 guardrails — Planner must place in next-edge scope or a bounded commit:** source comments at both
field declarations (`dsp_cpu.h:49` — never populated, do not read; `dsp.h:105` — clobbered by
`sync_to_vm` in traced runs, do not read) plus a canonical-GP-test note (`DSPState.is_gp` via `opaque`;
`dma.is_gp` in DMA paths). **Future classification by either core field is FORBIDDEN.** Deleting the dead
`is_gp` copies in both sync functions is **permitted, not required** (zero observable change; Planner's
call). `A4b1-r4` is **unaffected** and must not be touched.

**Session correction accepted:** my §8 account said `core->is_gp` is "permanently 0"; the accurate picture
is that `dsp_init` sets the **VM** field correctly but `dsp_c_sync_to_vm` **clobbers it** with the
interpreter core's unwritten `0` on every traced GP frame, so **both** fields read `0` in traced runs.
Contained (VM field has zero readers; interpreter field's only readers are the corrupting copy, a
compiled-out trace macro, and my fixed helper). The evidence record is corrected accordingly.

---

## Previous packet — `A4b2-NR-followup-r1` **EXECUTED 2026-09-27 → gap 1 CLOSED; `O-INCONCLUSIVE` stands, reason now quantified**

- **Packet:** `docs/packets/a4b2-nr-followup.md`, revision **`A4b2-NR-followup-r1`**, class **discovery**,
  frozen SHA-256 **`886620CC6797FC89130B1F89310E6B41C92E809F32820A8133609FBEEC9CC5F5`** (**33 lines**).
  **This is the packet to execute.** Promotion byte-identical with no revision (§5.3).
- **Adequacy:** **`VERDICT: ADEQUATE`** — the **writing Planner's own** review, as §5.8 requires for a
  discovery packet (child `e537374a-aaaf-49c7-9f3a-247bec80e784`, `codex/gpt-6-sol` @ `high`). No second
  Planner; no Muse preflight repeated.
- **Why it exists:** `A4b2-NR-r2` executed → **`O-INCONCLUSIVE`** (`L2=INVARIANT`, `L1` open). Evidence:
  `docs/reviews/a4b2-nonreliance-discovery-evidence.md`. Session verification:
  `docs/reviews/a4b2-nr-followup-r1-session-verification.md`.
- **The three gaps it closes:**
  1. **The unresolved `P 00B9` computed read** (`move x:(r1),b`, `r1 = 0x80 + x:$0000`, guest-written
     mailbox). Closed by exact env-gated GP-only `RECOMP_APU_GP_B9_TRACE=1` instrumentation recording **one
     uncapped event per actual execution** at the `dsp56k_read_memory` call — actual effective address,
     `r1`, `x:$0000`, the derived cross-check, returned value — classifying both `0x1400..0x17ff` **and**
     the `0x0c00..0x0fff` alias, with an **independent execution counter**, contiguity and counter-equality
     checks, no sampling/ring/first-N, trace INVALID on failure, and **no inference of a negative**.
  2. **The six table words** at `P 00CC`–`00D1`: decided as P-memory **data**, **no decoder surgery** —
     carried on the Session's three checks plus the `op =` diagnostic occurring 6× only during decode and
     0× in execution, with the falsifier *"if the table is actually executed or altered, do not reuse this
     conclusion."*
  3. **The completeness bar:** the Planner **retains the full PC-indexed CFG/def-use slice** over every
     feasible route to the first doorbell descriptor and DMA write, and rules the targeted trace
     **insufficient**. An in-range read is **not** automatically `REFUTED` — a concrete feasible causal
     chain to the **named doorbell** output or guard is required; influence on the other audio output is
     not enough.
- **Bounded toolkit scope:** `src/apu/dsp/interp/dsp_cpu.c`, `src/apu/dsp/gp_ep.c`, `src/apu/apu_watch.c`,
  `src/apu/apu_watch.h`, `tests/apu_watch_fixture_test.c`. **Game:** `CMakeLists.txt` (CTest registrations
  only). The committed `r2` work is **baseline, not an authorization**; `src/apu/dsp/dsp.c` is baseline but
  **not** re-authorized. **`A4b1-r4` is not touched.**
- **Closure:** the predecessor's retained `bad-output` control arm is removed at closure **after** its bite
  is preserved; a final absent-env default-control run must establish every diagnostic gate is inert.
- **Outcome rows:** name the next packet per result, including the death branch if non-reliance is refuted.

---

## Previous packet — `A4b2-NR-r2` **EXECUTED 2026-09-27 → `O-INCONCLUSIVE`** (`L1=INCONCLUSIVE`, `L2=INVARIANT`)

**`A4b2-NR-r2` executed under its frozen contract. Row selected: `O-INCONCLUSIVE`.** Evidence:
`docs/reviews/a4b2-nonreliance-discovery-evidence.md`.

**The packet's rule applied faithfully.** `O-TWO-LEG` requires **`L1 = PROVEN` *and* `L2 = INVARIANT`**.
`L2` is satisfied; **`L1` is not**, and the packet's own `L1 = INCONCLUSIVE` definition names the exact
situation — *"unresolved register-indirect/computed/table-driven branch"* — and warns *"do not treat an
indirect edge's absence in one trace as a proof of all feasible edges."* **No strict criterion is
discharged; `A4b2-r7` remains `R2-EXPL-INPUT` and is not retroactively PASS.**

**What was established — every measurement points one way, away from reliance:**

| Finding | Basis |
|---|---|
| **`L2 = INVARIANT`** | Six 30 s runs (baseline, `zero`, `max`, `prng:1`, `prng:2`, `bad-output`). The doorbell tuple `va=803C0810 observed=3 payload=0 dsp_addr=000800` is **bit-identical** across all four input-perturbation arms and the baseline. |
| **The hook demonstrably fired** | 1 623 104 mix-bin reads and 3068 `0xFFFFB3` reads in **every** run; `max`/`prng` changed **1.3M–1.6M** consumed values; first-substituted values differ per seed. `zero` correctly reports `changed=0` (originals were already zero). |
| **The known-bad control bites** | `bad-output` leaves both stub inputs untouched and **suppresses `GP_CLEAR`** (`GP_NONZERO_OVER` latches instead) — so the comparator *can* see a changed doorbell. |
| **`0xFFFFB3` is confined to scratch** | Its four reads (`P 002D`, `0033`, `003A`, `004D`) flow only into `x:$007c`–`x:$007f`, used solely for `cmpu`/`blt` loop control. Nothing reaches `r2`/`r3`, the DMA registers, or the descriptor. |
| **The doorbell descriptor is all immediates** | `P 0004 move #$000800,r2` → `P 00E8 move r2,x:(r0 + 4)`. `dsp_addr=000800` is a literal, matching the observed value exactly. |
| **The image does consume mix bins** | Via a **P-memory data table** at `P 00CC`–`00D1` (`001400 001440 001420 001480 0014A0 001460` = MIXBUF bins 0,2,1,4,5,3), read by `movem p:(r2)+,x0` under `dor #$0006`. That feeds a **different** descriptor (the audio output path), not the doorbell. |
| **Decode identity** | 380 instructions decoded, **0 mismatches** vs `I[i]=LE32(xbe[0x1A7D60+4i])&0xFFFFFF`; one-offset-shift negative control differs on 238 words. |

**The gap that keeps `L1` open:** one unresolved computed read, `P 00B9 move x:(r1),b`, where
`r1 = 0x80 + x:$0000` and `x:$0000` is a guest-written mailbox value. A static decode cannot bound it, so it
cannot be excluded from `0x1400..0x17FF`. The packet forbids treating that as benign.

**A Session error, corrected and recorded.** A first Leg 1 pass claimed *"the image never addresses the mix
buffer"* on the strength of a text search over instruction operands. **That was wrong** — the addresses are
in a **data table**, invisible to an operand grep. It is the same class of error `AGENTS.md` records for the
`PIO_FREE` enumeration. The corrected reading is in the evidence record, and the superseded reading is kept
visible rather than deleted so a reader can tell which stands.

**Next authorized action:** **`A4b2-NR-followup`** discovery packet, targeted at the three gaps the evidence
record names in priority order: (1) the unresolved `P 00B9` computed read — close it with an instrumented
read trace recording the actual `r1`, or bound `x:$0000` from its writer; (2) the decode-rejection of the
six table words, which the Session resolved as data on three independent grounds; (3) the **completeness
argument** — the packet's `PROVEN` bar is a full PC-indexed CFG/def-use slice, and what exists is a targeted
dependency trace. Whether the targeted trace suffices, or the full slice is required, is a **Planner**
judgment.

**Not reopened:** `A4b1-r4` stays accepted and closed; no toolkit file outside the discovery packet's
declared scope was modified (one out-of-scope declaration was made and **reverted** before building).

---

## Previous packet — `A4b2-NR-r2` (**discovery**, promoted 2026-09-27; executed → `O-INCONCLUSIVE`)

- **Packet:** `docs/packets/a4b2-nonreliance-discovery.md`, revision **`A4b2-NR-r2`**, class **discovery**,
  frozen SHA-256 **`7EF30508CCC2588748EAA92E9B56B12D038D425CA925CA360AA9059DFAF33E38`** (**33 lines**).
  **This is the packet to execute.** Promotion was byte-identical with no revision (§5.3).
- **Adequacy:** **`VERDICT: ADEQUATE`**, `BLOCKING: NONE` — the **writing Planner's own** review, as §5.8
  requires for a discovery packet (child `2565775c-9aa9-436b-b917-1efb2d01784a`, `codex/gpt-6-sol` @
  `high`). No second Planner was spawned; no Muse shape preflight was repeated, because the mechanism is
  the one the Advisor itself mandated.
- **Question:** are the `B+0x810` GP DMA doorbell write's **payload, address and executed-path write guards
  independent of both** VP-produced MIXBUF samples **and** placeholder `0xFFFFB3` reads? Output is a
  two-leg **discovery finding**, never a strict `A4b2` verdict.
- **The Advisor's two-leg standard** (`docs/reviews/a4b2-r7-expl-input-ruling.md`): **Leg 1** static
  mechanism from image `I`; **Leg 2** empirical invariance under adversarial perturbation. **Either leg
  failing REFUTES non-reliance.**
- **Outcome rows:** **`O-REFUTED`** (the death branch — strict `A4b2` **BLOCKED** until real VP
  implementation plus `0xFFFFB3` resolution; next `A4b-VP-real-implementation` and
  `A4b-B3-register-resolution`); **`O-TWO-LEG`** (both legs valid → `A4b2-r8` may cite both and the
  Advisor's input-specific conditional ruling); **`O-INCONCLUSIVE`** (→ `A4b2-NR-followup`). An unresolved
  leg **never rounds up**.
- **Bounded toolkit write scope:** `src/apu/dsp/interp/dsp_cpu.c`, `src/apu/dsp/dsp.c`,
  `src/apu/dsp/gp_ep.c`, `src/apu/apu_watch.c`, `src/apu/apu_watch.h`,
  `tests/apu_watch_fixture_test.c` (selector-control assertions only). **Bounded game write scope:**
  `CMakeLists.txt` (only new process-isolated CTest registrations for that existing fixture target). No
  other toolkit or game source. **`A4b1-r4` is NOT reopened** — this is new packet work.
- **Session verification:** `docs/reviews/a4b2-nr-r2-session-verification.md` — every command verified to
  run; one **blocking** scope defect found in `r1` and repaired in `r2`.
- **Closure:** instrumentation off by default; the `bad-output` control arm removed at closure; final
  absent-env control run verifies inertness. Evidence record:
  `docs/reviews/a4b2-nonreliance-discovery-evidence.md`. **Exploratory/diagnostic evidence is knowledge
  only, never acceptance of a strict criterion.**

---

## Previous packet — `A4b2-r7` **EXECUTED 2026-09-27 → `R2-EXPL-INPUT`** (claim NOT established; Advisor ruled)

**`A4b2-r7` executed under its frozen contract. Row selected: `R2-EXPL-INPUT`.** Evidence:
`docs/reviews/a4b2-r7-execution-evidence.md`. The row was **predicted in advance** by the adequacy
reviewer (`docs/reviews/a4b2-r7-adequacy-review.md`) and independently by the Session from the archived
`r6` bytes, so it is not a surprise.

**Four of five criteria PASS — the GP clear is real and well-attributed:**

| Criterion | Result |
|---|---|
| `AC-DEFAULT2` | **PASS** — R0 `diagnostic_deadline`, `Wf0=3`, `F0=2`, zero `[GP*]` lines, no TRAP/TRACE in env |
| `AC-BOOT` | **PASS** — one complete `n=1` block, 64 pram lines, last counts `boots=1=N`, common `sge0_va = T = 0x803C0000`, anchor cross-check `0x803C0810 − 0x810 = B`, **371/371 image words equal**, control differs |
| `AC-RUN` | **PASS** — `boots=1, gp_frames=768, gp_insns=33120534` |
| `AC-CLEAR` | **PASS** — `GP_CLEAR seq=198852 va=803C0810 observed=00000003 insns=124652 dsp_addr=000800`, `CPU_ANCHOR seq=198851` immediately before, `[GPDMA] watch … cas=ok` |
| `AC-NOCPU` | **PASS** — six-site reconciliation exact (6 found = 6 frozen, 0 missing/extra); no ack in env, source or exe; `CPU_ZERO=0` everywhere, no contested zero |
| **`AC-INPUTS`** | **FAIL** — steps 1–2 valid; step 3 finds named stub reads |

**Why `AC-INPUTS` fails — three named stub conditions, all in the `at_clear` freeze:** PERIPH index
**`0x33` (`0xFFFFB3`) reads = 1017** (the placeholder `v = 0; // core->num_inst; // ??`); MIXBUF
**`reads_while_stub` nonzero in 26 of 32 bins** (each `7648`); and **`mixbuf_stub_read = 1`**. The only
other nonzero PERIPH offsets (`0x45`, `0x56`) are inside the five-offset modelled set. DMA's nonzero class
is `CONTIG`, guest-written by region.

**So: the `3→0` GP memory-write-path transition at `B+0x810` is observed and attributed, but it is
EXPLORATORY for the named stub inputs — the packet's *Establishes* claim is NOT satisfied.** No `R2-PASS`.

**Next action (the row's own direction): the Advisor has ruled.** Verbatim:
`docs/reviews/a4b2-r7-expl-input-ruling.md`.

> **Next packet: ONE discovery packet** (§5.8) proving or refuting **non-reliance** of the doorbell exchange
> on **both** stub inputs, to a **two-leg standard**: **Leg 1 (static mechanism)** — from image `I`
> (`0x173` words), identify the doorbell write(s) and show its payload, address and every branch guarding
> the write on the executed path are independent of mixbin and `0xFFFFB3` values; **Leg 2 (empirical
> invariance)** — perturbation runs varying consumed stub values adversarially (all-zero, all-ones/max,
> pseudorandom) must reproduce the **identical** doorbell latch (`va`, `observed=3`, `payload=0`,
> `dsp_addr`) with frames advancing. **Either leg failing REFUTES non-reliance.**
>
> **The discovery's outcome table must carry an explicit death branch:** if non-reliance is refuted, the
> strict `A4b2` claim is **BLOCKED** until real VP implementation (not a model) plus `0xFFFFB3` resolution,
> and `A4b2` retires or waits.
>
> **A MIXBUF value-model is RULED OUT** — inadmissible under `docs/jsrf-run-profiles.md` criterion 4
> (mixbin contents are the *result* of VP voice-mixing work consumed by the GP whose execution is being
> certified, so supplying them without the VP is synthetic completion). The "idle bins read 0" escape is
> **closed by measurement**: the `MIXBUF_STUB_READ` latch reads `observed=1` (a VP voice was genuinely
> active). **A `0xFFFFB3` model is PREMATURE** until the register is identified. **No Planner time goes
> there.** `0xFFFFB3` register identification rides along as a parallel thread.
>
> **Conditional case ruling (confined to these two inputs):** Q1 condition 2's *purpose* — per its own
> "Why" and its per-value "each value it **relies on**" rule — is satisfied for these two inputs by a
> **completed two-leg non-reliance proof**; the follow-up `A4b2` revision may then cite the discovery and
> restate its input qualifier accordingly, with **no model packet** for them. This substitutes a **higher**
> bar (proof of non-reliance) for a blanket proxy (zero reads); it waives **no** evidence, sets **no**
> precedent beyond inputs with a completed proof, and does not touch Q1's text for any other input.
>
> **The perturbation hook is discovery instrumentation** (§5.8: reversible, env-gated, off by default at
> closure) — a **new packet's toolkit scope**, **not** a reopening of closed `A4b1`. **`PIO_FREE` ordering
> is unconstrained**; natural order is this discovery first, `PIO_FREE` after, but **sequencing is the
> Planner's call**.

**`r7`'s `R2-EXPL-INPUT` is terminal for `r7`** — the strict claim is **not established**, and no part of
the ruling converts it. `r6`'s `R2-UNKNOWN` remains terminal for `r6`.

**Also recorded:** the A2h displacement did **not** block this decision. Under `r7` no PASS-path claim reads
the R1 dump, so the R1 `content-mismatch: 1` was correctly **not** a gate; the displaced-dump `Wf` was not
read, and the no-`GP_CLEAR` branches that would have required it were not reached. That is the `r7` repair
working as designed. `r6`'s artifacts discharged nothing.

**Open lead, unchanged:** the **A2h displacement writer** (characterized, never identified;
`docs/jsrf-operating-history.md:1000-1045`). It no longer gates this claim but still gates later
progress-past-spin claims — the trapped run dies at ~4.8 s.

---

## Previous packet — `A4b2-r7` (promoted 2026-09-27, `ADEQUATE`; executed → `R2-EXPL-INPUT`)

- **Packet:** `docs/packets/a4b2-gp-clears-pending-word.md`, revision **`A4b2-r7`**, class **change**,
  frozen SHA-256 **`93E410C3742D0E290A0C0BBA71DCFEC547A77ECAC497E389BA5D58026E551F95`** (**136 lines**).
  **This is the packet to execute.** Promotion was **byte-identical with no revision**, as §5.3 requires —
  the file still reads `Status: draft` in its own text, exactly as `A4b1-r4`'s and `A4b2-r6`'s frozen files do.
- **Adequacy:** **`VERDICT: ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED` — recorded
  **verbatim** in `docs/reviews/a4b2-r7-adequacy-review.md` (fresh Planner child
  `7c6fd178-49cf-4f00-af5f-3401f3b6d85c`, `codex/gpt-6-sol` @ `high`). The authoring child
  `b430c48f-42ea-463f-99e6-97da5989e73a` did not review it.
- **Why `r7` exists — §5.4(1):** `A4b2-r6` was executed and selected **`R2-UNKNOWN`** because gate **G2**
  failed for R1. A **no-A4b2-edit control** reproduced that failure identically, proving the cause is a
  **pre-existing guest defect** (the A2h dump displacement, exactly `0x37608`), not this packet's edits.
  The Advisor ruled `AC-CLEAR`'s binding of `B` to the **post-mortem dump** a blocking defect with a
  concretely realized **false UNKNOWN**, and mandated four repairs, all implemented:
  **`B` rebound to log evidence** (common `sge0_va`, cross-checked against `anchor va − 0x810`); a new
  **independent title-page oracle `T = 0x803C0000`** so `AC-BOOT` is not circular; **G2 deleted** as a
  global PASS gate (no PASS-path claim reads the R1 dump); **`Wf`** meaningful only under a passing R1
  mapping check, and the no-`GP_CLEAR` branches guarded so a displaced dump yields UNKNOWN rather than a
  false `R2-UNATTRIBUTED`/`R2-CPU`/`R2-NOCLEAR`. Ruling:
  `docs/reviews/a4b2-r6-execution-ruling.md`; execution record: `docs/reviews/a4b2-execution-evidence.md`.
- **`r6`'s `R2-UNKNOWN` is terminal for `r6`** — not reopened, not rescored. **`r6`'s run archives are
  STALE for `r7`** (`packet:42`, `:135`): `r7` re-executes **fresh** R1/R0 under this contract.
- **Claim (Establishes, on `R2-PASS` only):** in one STRICT run with `RECOMP_GPU_ACK=0` and
  `RECOMP_APU_TRAP=1` plus observation-only APU trace, the **`3→0` transition at `B+0x810` was performed by
  the GP engine's memory-write path while executing validated image `I`**, after the anchored store was
  recorded, with no synthetic ack, no instrumented competing CPU zero, and all pre-exchange GP inputs
  guest-written by region or modelled. **It does not establish** that the guest observed the `0`, exited the
  spin at `loc_001A18D0`, or progressed past it.
- **Baseline:** game `a000662aef3bb4d7088f3fa367d19e7ce7c157df`, toolkit
  `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d` (clean). **Step 1's game-only edits are already applied** in
  the working tree (40 insertions, 0 deletions) and `r7` says to verify and preserve them, not duplicate them.
- **Write scope (game only):** `src/recomp/gen/recomp_0000.c`, `src/recomp/gen/recomp_0005.c` (step-1 calls
  and their `extern` declarations only), `src/diagnostics.c` (the `jsrf_watch_store` forwarder only),
  `docs/reviews/a4b2-*.md`. **No toolkit file.** Build/run owner: the Session.
- **Expected row, stated in advance by the adequacy reviewer:** a faithful fresh repetition is predicted to
  select **`R2-EXPL-INPUT` or `R2-UNKNOWN`, not `R2-PASS`** — the archived `r6` `at_clear` block already
  prints `mixbuf_stub_read=1` with **26 of 32** MIXBUF bins showing nonzero `reads_while_stub` (Session
  independently reproduced). `R2-EXPL-INPUT` routes to the **Advisor** to decide whether that named stub
  input needs its own model packet first. This prediction is **not** a defect in `r7` and is **not**
  permission to rescore `r6`.
- **Next after `R2-PASS`:** the `PIO_FREE` model packet, **before** any strict liveness/boot claim past the
  spin. `R2-NOBOOT`/`R2-NOFRAMES`/`R2-NOEXEC`/`R2-NOCLEAR` route to `A4c` discovery.
- **Open lead, cross-referenced (not newly mandated):** the **A2h displacement writer** was characterized
  (`docs/jsrf-operating-history.md:1000-1045`) but never identified; `A2h-r6` was retired on a *different*
  refuted premise, so the writer investigation **stays open**. It is on the critical path for later
  progress-past-spin claims — the trapped run dies at ~4.9 s — but it **does not gate** this claim.
- **Planning history (non-authoritative):** `docs/reviews/a4b2-revision-history.md`. `r1`–`r3`
  `INADEQUATE` on `[GPIN]` accounting; `r4`–`r5` `INADEQUATE` on `AC-BOOT`; `r6` `ADEQUATE` then executed
  → `R2-UNKNOWN`; `r7` `ADEQUATE` on the §5.4(1) evidence-binding ground.

**Route note:** the old `claude` route stays unusable and is not re-probed. Current DSH staffing is
`docs/agent-workflow.md` §1: Session/Workers `workbuddy-ai/deepseek-v4.1-flash` @ `max`; Planner and final
adjudicator **GPT-6 Sol** @ `high` (`provider: codex`); Advisor **Muse Spark 1.3** @ `max`
(`muse-worker`, handle `muse_FkNhGaXtV9P5`); acceptance stage 1 `hy4-preview-f` @ `high`; stage 2
`deepseek-v4.1-flash` @ `max`.

---

## Previous packet — `A4b2-r6` (promoted 2026-09-27, `ADEQUATE`; executed → `R2-UNKNOWN`; superseded by `r7`)

**`A4b2-r6` was executed on 2026-09-27 and selected row `R2-UNKNOWN`** because gate **G2** failed for R1.
**That UNKNOWN is terminal for `r6`** — failure stays failure, no rescoring, and no PASS survives from its
artifacts. Evidence: `docs/reviews/a4b2-execution-evidence.md`. Ruling:
`docs/reviews/a4b2-r6-execution-ruling.md`.

**What execution established (observed, positive — the packet's own subject):** the ported GP engine's DMA
write path took the `3→0` CAS at `0x803C0810` while executing image `I` — `GP_CLEAR seq=198852
observed=00000003 insns=124652 dsp_addr=000800`, with `CPU_ANCHOR seq=198851` immediately before it,
`[GPDMA] watch … cas=ok`, `CPU_ZERO_OVERFLOW=0`, no synthetic ack, and **`AC-BOOT`'s image comparison
passing 371/371 words** with a differing control. The rerun reproduced `seq=198852` identically.
`AC-DEFAULT2` is **PASS** (R0 `diagnostic_deadline`, `Wf=3`, `F=2`, zero `[GP*]` lines).

**Why G2 failed — a pre-existing guest defect, not this packet's edits.** A **no-A4b2-edit control**
(all three edits reverted, rebuilt, identical command) crashes **identically**: same
`unhandled_exception`, same `[ICALL] invalid target 0x00000000 return=0014982E`, same
`requested 598869040`, same `content-mismatch: 1`. The displacement is **exactly `0x37608`** — the A2h
separation — and the same signature exists in a **pre-`A4b1`** archive (`20260922-224429-003-a2g-304f0-span`,
2026-09-22). **`A4b1-r4` is not implicated and remains accepted and closed.**

**Next packet: `A4b2-r7` — a §5.4(1) revision** (a blocking finding from execution with a concrete
false-UNKNOWN scenario; **not** §5.4(2) — no load-bearing premise was invalidated). The Advisor's mandated
repair shape:

1. **Rebind `B` to validated log evidence** — e.g. `sge0_va` from the every-instance-validated `[GPBOOT]`
   block(s), cross-checked against `anchor va − 0x810`; multi-boot `sge0_va` disagreement → UNKNOWN.
   **`AC-BOOT`'s `sge0_va = B` conjunct must be restructured with it**, or it becomes tautological.
2. **No PASS-path dependence on dump integrity** — **G2 as a PASS-gate goes with it** (delete or demote;
   Planner's choice, one-line reason recorded).
3. **Displaced-dump `Wf` (reads 0) must not be recorded as meaningful corroboration** — qualify by mapping
   integrity or drop it.
4. **`r6`'s run artifacts are STALE for the revision** (§2.2.5, §2.4.7): the revision **re-executes** new
   R1/R0 runs under the new contract; the `r6` logs are leads and premise evidence only.

**The Planner writes the predicates.** Next: revision → **full fresh §5.3 adequacy review by a
non-authoring Planner** → re-execution → decision from log evidence.

**Open lead, cross-referenced (not newly mandated):** the **A2h displacement writer** was characterized
(`docs/jsrf-operating-history.md:1000-1045`) but never identified; `A2h-r6` was retired on a *different*
refuted premise, so the writer investigation **stays open**. It remains on the critical path for later
progress-past-spin claims — the trapped run dies at ~4.9 s — but it **does not gate** this claim.

**Route note:** the old `claude` route stays unusable and is not re-probed. Current DSH staffing is
`docs/agent-workflow.md` §1: Session/Workers `workbuddy-ai/deepseek-v4.1-flash` @ `max`; Planner and final
adjudicator **GPT-6 Sol** @ `high` (`provider: codex`); Advisor **Muse Spark 1.3** @ `max`
(`muse-worker`, handle `muse_FkNhGaXtV9P5`); acceptance stage 1 `hy4-preview-f` @ `high`; stage 2
`deepseek-v4.1-flash` @ `max`.

---

## Previous packet — `A4b2-r6` (promoted 2026-09-27, `ADEQUATE`; executed → `R2-UNKNOWN`)

- **Packet:** `docs/packets/a4b2-gp-clears-pending-word.md`, revision **`A4b2-r6`**, class **change**,
  frozen SHA-256 **`3273409262437D42A500A1E988E39CB25234C2E5E2A96007A9BE74FEB6B250E5`** (**134 lines**).
  **This is the packet to execute.** Promotion was **byte-identical with no revision**, as §5.3 requires —
  the file still reads `Status: draft` in its own text, exactly as `A4b1-r4`'s frozen file does.
- **Adequacy:** **`VERDICT: ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED` — recorded
  **verbatim** with the reviewer's child ID and route in `docs/reviews/a4b2-r6-adequacy-review.md` (fresh
  Planner child `36e21ac4-ffcf-49b6-a3ae-80abe39da281`, `codex/gpt-6-sol` @ `high`). The authoring child
  `636dc591-2e30-41fb-8cf6-46d108135165` did not review it.
- **Claim (Establishes, on `R2-PASS` only):** in one STRICT run with `RECOMP_GPU_ACK=0` and
  `RECOMP_APU_TRAP=1`, the **`3→0` transition at `B+0x810` was performed by the GP engine's memory-write
  path while executing validated image `I`**, after the anchored store was recorded, with no synthetic ack,
  no instrumented competing CPU zero, and all pre-exchange GP inputs guest-written by region or modelled.
  Every recorded GPRST bootstrap loaded words equal to `I`. **It does not establish** that the guest
  observed the `0`, exited the spin at `loc_001A18D0`, or progressed past it.
- **Baseline:** game `a000662aef3bb4d7088f3fa367d19e7ce7c157df`, toolkit
  `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d` (clean).
- **Write scope (game only):** `src/recomp/gen/recomp_0000.c`, `src/recomp/gen/recomp_0005.c` (step-1
  calls and their `extern` declarations only), `src/diagnostics.c` (the `jsrf_watch_store` forwarder only),
  `docs/reviews/a4b2-*.md`. **No toolkit file.** Build/run owner: the Session.
- **Runs:** R1 (trap+trace) and R0 (default), 30 s each, `--profile strict`, archived under `logs/runs/`.
- **Next after `R2-PASS`:** the `PIO_FREE` model packet, **before** any strict liveness/boot claim past the
  spin. Rows `R2-NOBOOT`/`R2-NOFRAMES`/`R2-NOEXEC`/`R2-NOCLEAR` route to `A4c` discovery.
- **Planning history (non-authoritative):** `docs/reviews/a4b2-revision-history.md`. `r1`–`r3` were
  `INADEQUATE` on `[GPIN]` accounting; `r4` and `r5` were `INADEQUATE` on `AC-BOOT`. The binding rulings
  that shaped `r6` are `docs/reviews/a4b2-r4-planning-rulings.md` and
  `docs/reviews/a4b2-r4-advisor-blocker-ruling.md`; the two verdicts are
  `docs/reviews/a4b2-r4-adequacy-review.md` and `docs/reviews/a4b2-r5-adequacy-review.md`.

---

## Last closed packet — `A4b1-r4` (GP/DSP core port) — **ACCEPTED 2026-09-26, `R1-PASS`, PUSHED (complete)**

**`A4b1-r4` is accepted and its closure push is done. It must not be reopened.** `A4b1` is accepted with
exactly its "Establishes" claim: the pinned xemu GP core (interpreter only), GP MMIO routing and one GP
DMA write choke point are in the toolkit with per-file provenance; the synthetic ack is gone from source
and exe; a thread-safe watched-word ledger is exported and fixture-exercised; the combined work's licence
is recorded; the tree builds and every ctest passes; one STRICT default run matches the `A4s` baseline
stop. **The core is reachable only through GP MMIO, which requires `RECOMP_APU_TRAP`.**

**Next packet: `A4b2` → `A4b2-r4` — ✅ PLANNING RESUMED (owner decision, 2026-09-27).**

> **RESUMED: the staffing authority was replaced by owner decision.** The `§1` roster no longer assigns
> the Planner or the persistent Advisor to `claude/claude-opus-5-5`; that route's failure and the
> eight-probe ladder remain recorded in `docs/reviews/blocker-20260926-claude-route-down.md`, and the
> suspension it produced is **lifted**. The owner instruction that replaced the staffing, the Advisor
> ruling confirming the resumption, and the Session's re-verification that no load-bearing premise
> changed are recorded together in **`docs/reviews/owner-staffing-resumption-20260927.md`**.
>
> **Current DSH staffing** (`docs/agent-workflow.md` §1): Session and Workers
> `workbuddy-ai/deepseek-v4.1-flash` @ `max`; **Planner and final acceptance adjudicator GPT-6 Sol** @
> `high` (`provider: codex`, `LIVE_RESOLVE`); **persistent Advisor Muse Spark 1.3** @ `max`
> (`muse-worker`, handle `muse_FkNhGaXtV9P5`); acceptance stage 1 `hy4-preview-f` @ `high`; stage 2
> `deepseek-v4.1-flash` @ `max`.
>
> **Preserved and still valid — nothing was lost:** the Planner's **complete sketch** (packet lines 1–25)
> over the **intact `A4b2-r3` body** (27–334); the Advisor's **`SHAPE: PROCEED`** ruling recorded verbatim
> in `docs/reviews/a4b2-r4-planning-rulings.md`; the Session's pre-preflight verification
> (`a4b2-r4-sketch-verification.md`); and the `0xFFFFB3` decision-class note applied to
> `a4b1-r4-acport-step4-enumeration.md`. **Work resumed at "write the `A4b2-r4` body" — no re-planning
> and no re-preflight**, because the sketch was already cleared and the ruling is recorded. The old
> Planner child `cee46374-…` is retired with the old configuration and is not reused.
>
> **What the `A4b2` planning already settled, so it is not re-litigated on resume:**
> grounds are **§5.4(2) + §5.4 After-INADEQUATE + §5.4(3)** (not "only a re-bind"); five edits approved
> in substance; the `803CC000` GPSADDR conjunct is demoted to a recorded value (the Advisor confirmed
> **its own boundary note missed this**); `AC-INPUTS` decides from the **first `[GPIN] at_clear` block**
> **on a stated inference plus two can-fail checks**; **`0xFFFFB3` is STUB, not modelled** — the modelled
> set is **five** offsets (`0x45`, `0x54`–`0x57`) with a claim limit on `0x56`; **P3** (EP unreachability)
> is bound to the **build-identity commit**, not "HEAD"; and the **NDEBUG** coverage claim is narrowed to
> a claim limit on `R2-PASS`.

**Route-availability note (unchanged in effect):** the old `claude` route stays unusable and is not
re-probed; the current roster above is what §1 requires. A required route that is unavailable is still
`BLOCKED` under §1 — never a silent fallback.

| Result | Value |
|---|---|
| Gates | **G1** `STRICT`; **G2** `matches 1, content-mismatch 0`; **G3** readable; **G4** `L = 6748`, `goto` at `6751`; **`F = 2`** (matches the `A4s` baseline) |
| `AC-PORT` | **PASS** — stage 1 `DISAGREED` (correctly), corrected, **stage 2 `AGREED`** |
| `AC-LIC` | **PASS** |
| `AC-FIX` | **PASS** — **14/14 ctest**, 17 cases, both can-fail twins **mutation-proven** |
| `AC-DEFAULT` | **PASS** — `diagnostic_deadline`, `W = 3`, `F = 2`, all counts 0, **positive control 954 vs 0** |
| Row | **`R1-PASS`** |
| R0 | `logs/runs/20260926-010303-411-a4b1-default` |
| Acceptance | stage 1 `NOT ACCEPTED` (`fb109c49-…`, Hy4 @ `high`) → stage 2 **`ACCEPT`** (`f3f02108-…`, DeepSeek @ `max`) |
| **Pushed** | **`origin` (the fork), `main`, `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d`**, fast-forward `3f8bf67..3a3c7c1`, 8 commits. **`upstream` untouched** at `766ecef`. No force used. |
| Toolkit | HEAD `3a3c7c1…`, clean |
| exe | `B13521858A344919731E73F6E602186E951A49D7E57CA4E929BEA15CC2736ADD` |

**Two Advisor rulings were needed during execution**, both recorded verbatim in
`docs/reviews/a4b1-r4-execution-rulings.md`: the `FIFO_READ` ruling **(C)** — `AC-FIX (viii)` found a
**genuine hook-completeness gap** (the pinned DMA read arm falls through because its `assert` is
`NDEBUG`-elided, consuming stale bytes as a GP input, unrecorded) — and its **addendum**, which caught a
defect in the Session's own fix (`dsp_dma.c` is shared by the GP and the EP, so the hook needed an
`is_gp` gate).

**Stage-1 acceptance then blocked on `AC-PORT`**, and the block was correct: the step-4 enumeration's
write-callback row was factually wrong (`ep_scratch_rw` recorded as not reaching the choke point, citing
an `is_gp` gate that does not exist). The Session verified it and found the error **larger than reported —
three false cells, not one**; `gp_fifo_rw` and `ep_fifo_rw` also reach it via
`circular_scatter_gather_rw` and were not listed. **All four pinned write callbacks reach the choke
point.** Root cause: the generating script computed no reachability, so that column was hand-written.

> **Pattern recorded for the next packet.** **Three** completeness/correctness claims in this packet were
> wrong and **all three were caught by adversarial review, not by the Session's own checking**: the
> missing `FIFO_READ` hook, the enumeration, and (twice) the Session's first reading of a premise being
> reversed by the Advisor. Two were "every X" claims. **For `A4b2`, derive enumerations and reachability
> from source rather than writing them by hand, and treat any "every"/"all" claim as the highest-risk
> sentence in the document.**

**New `A4b2` decision input, carried forward (do not silently absorb).** Because `ep_scratch_rw` and
`ep_fifo_rw` reach the choke point, **an enabled EP can land an exchange on `W_va` and take
`GP_CLEAR`**. The EP is gated on `EPRST` (`gp_ep.c:650`) and runs every 8th frame (`:652`), but the path
is real. The Session does **not** rule on whether it is benign — that is a Planner/Advisor judgment, and
`DS5`'s text ("from every pinned write callback") already requires the shared choke point, which holds.

**Follow-up leads carried into `A4b2` planning (recorded, not done):**
1. **`NDEBUG`-elided asserts** — enumerate those in `src/apu/dsp/` that change GP state or inputs. Known:
   `unk2`/`unk13`, the `format` default, `dsp_offset` out of range. *(Advisor follow-up lead.)*
2. The classifier's treatment of the inert `RECOMP_APU_DSP_ACK`.
3. The game repository has no licence file.
4. EP routing.
5. `gp_ep.c:382-387` (the `GPBOOT` trace block) reads guest memory via `apu_guest_dma_ptr` **without**
   `apu_gp_dma_read` — trace-gated, feeds only the printed line, not a consumed GP input. *(Stage-2
   advisory 2.)*

Evidence: `docs/reviews/a4b1-execution-evidence.md` (the index), `a4b1-r4-implementation-verification.md`,
`a4b1-r4-fixture-green.md`, `a4b1-r4-acport-step4-enumeration.md`, `a4b1-r4-execution-rulings.md`,
`a4b1-r4-stage1-acceptance-review.md`, `a4b1-r4-stage2-acceptance-review.md`.

- **Packet:** `docs/packets/a4b1-gp-core-port.md`, revision **`A4b1-r4`**, class **change**, frozen
  SHA-256 **`6DD62A57E87445F5C12085210146204AA4E77D26FD316071FD41CAEC76835C38`** (**445 lines**).
  **This is the packet to execute.** Promotion was **byte-identical with no revision**, as §5.3
  requires. The superseded `A4b1-r3` (`321ABCF7…`) remains recoverable from git
  (`git cat-file blob 3f05c0f95205bfef06c0d75a0ba1038b670e9550`).
- **Adequacy:** **`VERDICT: ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS: PASS` — recorded
  **verbatim** with the reviewer's child ID and route in `docs/reviews/a4b1-r4-adequacy-review.md`
  (fresh Planner child `5db5fd71-b1db-479d-9e5c-14043d54e211`, `workbuddy-ai/kimi-k3`). It was the
  **full formal §5.3 review** the Advisor mandated, not a delta re-review, and it checked **all six**
  mandated scope items individually. It independently re-verified the packet SHA, reproduced the
  load-bearing toolkit citations at `M`, confirmed the **8 live `0x03FFFFFF` sites**, and confirmed the
  packet uses the **Advisor's** DS3 rather than the Session's superseded formula.
- **Why this packet is larger than a re-baseline — and correctly so.** It carries **two** revision
  grounds: **(1)** a **§5.4 After-INADEQUATE** repair of `A4b1-r3`'s blocking defects **B1** (the
  `[GPIN]` 256-entry table's key space exceeded the MIXBUF universe → false `R2-UNKNOWN`) and **B2**
  (`sge0` and the counts-line `seq` undefined and unasserted → false `R2-NOBOOT`); and **(2)** a
  **§5.4(2) `PREMISE_CHANGED`** re-bind to `M`. **No `A4b1` revision had ever been `ADEQUATE`; `A4b1-r3`
  is superseded.** The Planner found the `[GPIN]` lineage conflict; the Advisor ruled the redesign **in
  scope**, because it is caused by the inadequate verdict, **not** by A4s changing files.
- **Advisor rulings (binding, verbatim):** `docs/reviews/a4b1-r4-planning-rulings.md` — shape
  **`PROCEED`**; `[GPIN]` = the **finite-universe/provenance-class design in full**; identifier
  **`A4b1-r4`**; and **part 4 — P-F is load-bearing**, the forward map is now **non-injective**, so DS3's
  inverse is rewritten (the Session's earlier formula was **wrong** and is superseded).
- **What the packet carries:** DS3's four-case inverse (window-VA **first**, then high-water
  `XBOX_CONTIG_BASE + P`, then mapped low-RAM identity, then fail closed), citing `dma_resolve` **and**
  `xbox_memory_layout.c:843`, with `GPDMA_AMBIGUOUS` and the aliasing claim limit; DS6's **full**
  finite-universe `[GPIN]` design (MIXBUF `[NUM_MIXBINS = 32]`, PERIPH `[128]`, FIFO `[6]`, DMA region
  classes `[4]`, `BOOT_SCRATCH_READ`, `GPIN_OUT_OF_UNIVERSE` as a bug detector, `at_clear` freeze at
  `GP_CLEAR`, capped observation, fixed-count emission) with the r3 table / `GPIN_OVERFLOW` / per-key
  lines / cut-off **retired and named as must-not-reappear**; DS5's CPU-site table sized
  one-per-enumerated-`jsrf_watch_store`-site with `CPU_ZERO_OVERFLOW` a bug detector; DS7's `[GPBOOT]`
  printing **both** `sge0` (raw) and `sge0_va` (translated); and AC-FIX `(viii)` kept with
  `(ix′)/(x)/(xi)` replacing `(ix)`, and `(vii)` in **two** forms that can fail in **both** directions.
- **`A4b2` boundary note is in the packet** — `A4b2-r3`'s precondition P2 and its `AC-INPUTS` become
  **stale**; the `A4b2-r4` re-brief items are recorded there. **`A4b2` is not revised here.**
- **Regeneration is a non-goal**, so the pytest gated lead **stays gated and unexercised** — no `pytest`
  work is manufactured.
- **Deferred advisories (recorded, not reopening the packet):** AC-FIX `(v)`'s `CPU_ZERO_OVERFLOW ≥ 1`
  is a deliberate lower bound under finite-universe sizing (the can-fail property is preserved by `(x)`);
  a soft "where the snapshot records them" qualifier carried from r3; and two observation-only subfields
  that **no row reads**.
- **Next action: execute `A4b1-r4` literally**, beginning with its **P0/Readiness** gates. `A4s` is
  complete; `A4b1` reaching a durable disposition is the precondition for `A4b2`.

### Predecessor — `A4s-r6` (toolkit sync) — **ACCEPTED 2026-09-25, `R-SAME`, PUSHED** (complete)

> **RESULT: the merge succeeded, the strict stop is UNCHANGED, and `M` is ACCEPTED and PUSHED.**
> Every gate passed. Toolkit `main` is at **`M` = `3f8bf67c450861aefcbc376698750bc1446bc9dd`** (parents
> `0d7929c`, `766ecef`), clean, and now **published to the owner's fork**.
>
> - **Acceptance:** **stage-1 `ACCEPT`**, `BLOCKING: NONE`, **all ten mandatory criteria `AGREED`**
>   (`docs/reviews/a4s-r6-acceptance-review-stage1.md`; reviewer child
>   `f6ba7864-07ae-4388-9583-9a265e63c935`, `workbuddy-ai/hy4-preview-f` @ `high`, route verified live).
>   Per §2.2 a first-stage `ACCEPT` is **final** and is not passed to a second stage. State: **accepted**.
> - **Closure push — DONE** (owner policy, all five checks): `main` → `origin`, fast-forward
>   `766ecef..3f8bf67`. Verified: `git rev-parse origin/main` = `M` and `git ls-remote origin
>   refs/heads/main` = `M`. **No force, no `upstream` push** (`upstream/main` still `766ecef`, untouched).
>   Tracking moved to `origin/main` (recorded, not reverted). Log:
>   `docs/reviews/owner-push-policy-xboxrecomp-fork.md`.
>
> **The reviewer independently reproduced more than the Session measured** — worth keeping: positive
> controls for `AC-MERGE` (b)/(c) (**143** and **28** excess on the two parents, so those checks *can*
> fail); a positive control for the `|= MCPX_AC97_CODEC_READY` grep (**1** on `766ecef`, so the zero at
> `M` is meaningful); **link falsification** for `AC-BUILD` (all **147** sources in the run's
> `build-source.json` hash-match the `M` worktree); an independent conflict-set guard via
> `git merge-tree --write-tree`; and **AST-based** verification of hunks 5/6/9 instead of text
> comparison.
>
> **One documentation error it caught was CORRECTED:** `docs/reviews/a4s-r6-ac-merge.md` had named the
> hunk-8 function `test_seeds_align_16`, which does not exist; the hunk sits in
> `test_seeds_drops_unaligned_targets` (L96), where HA-LOCAL is correct. A record error, not a resolution
> defect.
>
> **Deferred advisories (recorded, not reopening the packet):** step-4 evidence-table labelling (raw
> preview **15** vs resolved-all-ours **3** should be labelled separately); the `AC-STRUCT`
> duplicates-not-omissions limit; the pre-existing set-G failure; the 13-byte PE-timestamp exe delta; and
> the reviewer's own disclosed instrument errors.
>
> **Two claim limits and one gated lead carry forward** — see the block at the end of this section.
>
> - **Step 1** — recovery point discharged as **verify-and-reuse** of the pre-existing `a4s-pre-sync`
>   (per **Q-B**); nothing created, deleted, or force-moved.
> - **Step 2** — controls: build OK; `ctest` 12/12; K4 27 tests OK; KX 33 modules / Ran sum 133 with zero
>   per-module differences; set G 11 files / 10 pass / 1 fail (carve-out by identity).
> - **Step 3** — the **9-hunk guard passed exactly** (5 files, 9 hunks, every section and line span
>   matching); all 9 hunks resolved **by exact text** per the pre-ruled table.
> - **Step 4 `AC-STRUCT` PASS** (0 findings, 0 UNKNOWN; all controls green) — and it **caught a real
>   defect in the Session's own first resolution**: edit (b) implemented as a blanket text delete left
>   switch 4 with a duplicate `case 138` and **switch 5 with none at all**, silently dropping ordinal-138
>   dispatch, which the build would never have caught (`docs/reviews/a4s-r6-ac-struct-catch.md`). The
>   merge was aborted, re-run deterministically, and the rule corrected to *keep the first occurrence per
>   switch* — the **operative reading of edit (b)**, confirmed by the Advisor.
> - **Step 5 build PASS** (relinked; no structural diagnostics, so the `R-BUILD` backstop never arose).
> - **Step 6 `AC-TEST`** — C **12/12** incl. the named witness `jsrf_inplace_event_bridge`; G same
>   failing identity as step 2; K4 30 tests OK; KX 56 modules with **0 regressions, 0 lost coverage**.
> - **`AC-MERGE` PASS** (16/16 targeted checks; the `(d-twin)` deletion witness took four attempts and
>   all three failures were mine — *"credited"* is load-bearing, `docs/reviews/a4s-r6-ac-merge.md`).
> - **`AC-KEEP` (iv)/(v)/(vi) PASS** — three model ranges byte-identical (anchor counts 1/1/1, working
>   can-fail controls); deleted names **0** in `SCOPE`; all four controls match **2/2/0/0**; and the
>   **(vi) `[A3A]` witness on the run: `gc=0x00000002 gs=0x00000100`** — GC bit1 and GS bit8 both set,
>   matching the reference control exactly.
> - **`AC-INV` PASS** — new env names exactly `RECOMP_APU_MIXDOWN_ALL` + `RECOMP_USB_PORT`, none removed.
> - **`AC-GEN` PASS**, **`AC-NOPUSH` PASS** (`ahead 25`, remotes exact, no push/fetch during execution).
> - **`M` conforms to Appendix A verbatim** — HA-COMBINED-1/-2 and Expected-5/6/9 byte-for-byte, plus
>   10/10 structural checks (`docs/reviews/a4s-r6-appendix-a-conformance.md`).
> - **Step 7 — the one strict run:** `logs/runs/20260925-210315-113-a4s-sync-strict`. **V holds**
>   (STRICT; dump `matches: 1, content-mismatch: 0`); **`outcome = diagnostic_deadline`**;
>   **`B = 0x803C0000`**; **`W = 3`**; **`F = 2`** (identical to the `A4a-r2` R0 control). **No rerun.**
> - **Row `R-SAME`:** V holds; `outcome = diagnostic_deadline`; `B` usable and `= 0x803C0000`; `W = 3`;
>   `F ≥ 1`. All six gate rows and `R-INVALID`/`R-MOVED` were evaluated first and are unmatched. **The
>   stop is unchanged — still the DSP pending-word spin** at `loc_001A18D0` (`recomp_0005.c`), pending
>   word `MEM32(0x803C0810) = 3`.
>
> **Next packet:** Planner revises **`A4b1` → `A4b1-r4`** (`PREMISE_CHANGED`, §5.4(2)) — new baseline =
> toolkit **`M`**, exe
> `E45026C3DF5AACAF3D66FCC1E17D9C6C1A12247864D58D0911CDBA8435A2A3C7`, this run as the reference R0, and
> upstream's `src/apu/apu_dsp.c`/`CMakeLists.txt` as the starting state; then `A4b2`. **`A4b1`/`A4b2` are
> no longer parked behind `A4s`** — `A4s` now has a durable final disposition.
>
> **Closure push (owner policy) — after ACCEPT only.** `R-SAME` leaves `main` at `M`, so
> `git push -u origin main` is permitted once acceptance returns `ACCEPT` and the five pre-push checks
> hold. **The acceptance reviewer must be shown the Q-C claim limit explicitly**; acceptance with that
> recorded limit satisfies the policy's "active packet's tests/acceptance passed" — not all 56 KX modules
> passing.
>
> **CLAIM LIMIT (Q-C) — must appear in the acceptance review.** *KX does not exercise 7 pytest-dependent
> upstream modules under `unittest`* (`test_block_dispatch`, `test_incdec_carry`, `test_incdec_result`,
> `test_lifter_double_shift`, `test_lifter_result_clobber`, `test_sar_width`, `test_x87_classification`).
> *The lifter/translator conflict resolutions (hunks 5–7 and 9) are therefore witnessed only by K4 and the
> existing KX modules, not by upstream's own tests of those paths.* Harmless **for this packet only**
> because `AC-GEN` holds — no regeneration, so `M`'s lifter produced none of the linked code. The 7 are
> **not a FAIL**: per Q-C a module whose only error is an absent third-party import ran no test, and all
> four fail-closed conditions were verified. **No `pytest` was installed and none was borrowed.**
>
> **GATED LEAD (binding on later planning).** Before **any** packet regenerates or relifts with the
> toolkit at `M` or later, all **56+** KX modules — including these 7 — must run under **real `pytest`**
> in an **owner-authorized** environment; parametrized and fixture tests included.
>
> **`AC-STRUCT` claim limit (Q-C).** It **detects duplicates, not omissions** — the switch-5 damage was
> invisible to it, and is excluded only by the explicit post-condition (one `case 138` per switch). For
> any future revision, **each HA edit should carry an expected-count post-condition enforced as an
> `AC-MERGE` check**.

- **Packet:** `docs/packets/a4s-toolkit-sync.md`, revision **`A4s-r6`**, class **change**, frozen
  SHA-256 **`75207C41B8E9964D3E1467F5E980D1B8DE4CB75A1E0AFE954F59D40FA27D86E3`** (**374 lines**).
  **This is the packet that was executed.** It replaced `A4s-r5` at the canonical path **byte-identically
  to the reviewed revision**; `A4s-r5` remains recoverable from git
  (`git show HEAD:docs/packets/a4s-toolkit-sync.md`, blob `f918b998c20a5be2bcc832fbde527a5754634183`).
- **Status: EXECUTED (see the result block above).** Promotion was **byte-identical with no revision**,
  exactly as §5.3 requires: `ADEQUATE` ends plan iteration, so the packet was **not** edited — the stale
  "358 lines" note in the packet and its tool record (the scanner is really 355 lines) is a **recorded
  advisory only** and is used in **no** predicate. Editing it would have changed the SHA and invalidated
  the review.
- **Adequacy:** **`VERDICT: ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED` — recorded
  **verbatim** with the reviewer's child ID and route in `docs/reviews/a4s-r6-adequacy-review.md`
  (fresh Planner child `95429607-3d77-44a9-8f38-47e978ece759`, `workbuddy-ai/kimi-k3`, effort omitted).
  It independently re-verified both pinned tool hashes and found the hunk-1/2 C text **byte-identical**
  to the Advisor's ruling, the 9-hunk table complete, and H2 to be the Advisor's **edit-application**
  version rather than the rejected precondition.
- **The review's one persistence risk is CLOSED** (`DEFERRED 1`). The reviewer asked the Session to
  confirm `logs/a4s/conflicted-*` survive until the `AC-STRUCT` positive control runs. Rather than merely
  confirming existence — they are in **gitignored** `logs/`, so nothing guarantees them — the Session
  tested whether the risk is real, and **it is not**:
  - the fidelity source is **durable in git**: the raw preview **`75083476` is a git tree object**,
    recoverable with `git show 75083476:<path>`;
  - the saved files are **not** byte-identical to the preview blobs, and the reason matters: the saved
    files are **diff3** (3 `|||||||` base markers) while `git merge-tree --write-tree` emits **2-way**
    markers (0 base markers) — the same 2-way/diff3 distinction that caused an earlier Session error;
  - **it does not change the control**: resolving **both** sources to all-ours gives **byte-identical
    content for all five conflicted files**, and both give **exactly 3 findings, 0 UNKNOWN** over `SCOPE`
    (`dup-case case 138 [8019,8322]`, `dup-case case 138 [8481,8847]`, `dup-def bridge_KeResetEvent
    [1377,6713]`) — precisely the 2 + 1 the criterion expects.
  - **A Session error found and corrected while closing it:** the first attempt resolved **all five**
    conflicted files, including three under `tools/recomp/` that are **outside `SCOPE`** (toolkit tooling,
    not build inputs of `jsrf_recomp.exe`). Feeding Python to a C-oriented scanner produced **79 spurious
    `dup-def` findings and one UNKNOWN** in `lifter.py`. The corrected control scans only the two in-scope
    files. Recorded because it is exactly the mistake the `SCOPE` guard exists to prevent.
- **Readiness gates the Session has already verified:** both pinned gating tools match their pins —
  `scripts/check-merge-structure.py` = `A1FDCE26755DAB3A0B6AA6E23D380A8F95E097439C2BE91D6318BF4402AA1AFF`
  and `tests/test_merge_structure.py` = `A42F4A1E2C473892DC28A0F5B21923F4BC0AEFCFFE859C7FC20006F49DB0744C`
  — and the scanner's four controls reproduce (`0d7929c` 87/109/**0**, `766ecef` 87/111/**0**,
  `75083476` raw 90/111/**15**, worktree 87/109/**0**; fixtures **14/14 OK**).
- **Set-G baseline at this revision: 11 files, 10 pass / 1 fail** (the one failure is `E2 =
  `tests/test_ac2_provenance.py`, pre-existing and pristine-HEAD-reproduced). It has moved **three times
  this session** (8/2 → 9/1 → 10/1), which is why the packet requires the carve-out to be **re-derived
  from each execution's own step-2 run by test identity, never carried forward**.
- **Next action: Planner revises `A4b1` → `A4b1-r4`** (`PREMISE_CHANGED`, §5.4(2)), then `A4b2`.
  `A4s` now has a durable final disposition, so **`A4b1`/`A4b2` are no longer parked behind it**.

### `A4s-r6` execution — **COMPLETE**, steps 1–8 all run

The full narrative is in `docs/reviews/a4s-r6-execution-evidence.md`; the summary is in the result block
at the top of this `CURRENT PACKET` section. Points worth keeping here:

- **P0 passed under the Advisor's interpretation rulings** — **Q-A** (a pin on a tracked file identifies
  the **committed blob**; P0.10 passed by **branch (a)**, the literal working-tree hash, which equals the
  pin) and **Q-B** (proceed, discharging P0.7/step 1 as **verify-and-reuse** of the pre-existing
  `a4s-pre-sync` under four fail-closed conditions, all of which held). **Neither required a packet
  revision** (§5.4 interpretation rulings), and **no `.gitattributes` was added** (owner/packet-scope
  work, per the Advisor).
- **`AC-STRUCT` earned its place.** It caught a real defect in the **Session's own** first resolution —
  a blanket text delete of `case 138` that left switch 4 with a duplicate (compile error) and **switch 5
  with none at all**, silently dropping ordinal-138 dispatch. **The build would never have caught the
  second one.** Direct evidence for the Advisor's ruling that `AC-STRUCT` must run **before** the build.
- **The `AC-MERGE` `(d-twin)` deletion witness took four attempts and all three failures were mine** —
  the word *"credited"* is load-bearing. Recorded in full, including the two false alarms caused by
  over-broad file-wide text searches (`docs/reviews/a4s-r6-ac-merge.md`,
  `docs/reviews/a4s-r6-appendix-a-conformance.md`).
- **A recurring Session failure mode, recorded because it repeated:** every verification failure in this
  execution was an **over-broad text match** — a file-wide search used where the criterion was
  function-scoped, or a substring test matching a longer identifier. **No defect in `M` was ever found
  by these.** The lesson is that each criterion's scope must be implemented literally, and the targeted
  per-region checks are what make the results meaningful.
- **The KX/`pytest` question was resolved by the Advisor as `Q-C`: NOT `R-TEST`.** The 7 modules are
  *"not exercised: new at M, dependency absent (pytest)"* — **not a FAIL and not a PASS witness** — under
  four fail-closed conditions, **all verified**. **No `pytest` was installed, and none was borrowed**
  from the unrelated venv the Advisor identified. The claim limit and the gated lead are in the result
  block above and **must be shown to the acceptance reviewer**.

**Current blocker / next action.** **`A4s` is COMPLETE** — `A4s-r6` is executed, **accepted** (`ACCEPT`,
all ten criteria `AGREED`), and **pushed** to the owner's fork; the toolkit has a durable final
disposition at `M`. **The current work is the Planner's revision of `A4b1` → `A4b1-r4`**
(`PREMISE_CHANGED`, §5.4(2)), then `A4b2`.

**`A4b1-r4` — ADEQUATE, PROMOTED, EXECUTING (2026-09-25).** The Advisor ruled **`SHAPE: PROCEED`** on
the Planner's sketch; the packet body was written; a **full formal §5.3 review** by a fresh Planner
returned **`ADEQUATE`** (`BLOCKING: NONE`, `PREMISE_FRESHNESS: PASS`); the packet was **promoted
byte-identically** and **execution is in progress**. (Both the packet and its verdict were authored by
Kimi K3 Planners, which was the roster in force at the time — recorded as **provenance**; see the
staffing-state block below, which supersedes that arrangement for *future* Planner work.)

- **Packet:** `docs/packets/a4b1-gp-core-port-r4.md`, revision **`A4b1-r4`**, class **change**, SHA-256
  **`6DD62A57E87445F5C12085210146204AA4E77D26FD316071FD41CAEC76835C38`**, **445 lines**. The canonical
  `docs/packets/a4b1-gp-core-port.md` (`A4b1-r3`, `321ABCF7…`) is **untouched** — promotion is the
  Session's step.
- **Advisor rulings (binding, verbatim):** `docs/reviews/a4b1-r4-planning-rulings.md`. Four parts:
  **shape `PROCEED`**; **`[GPIN]` = the finite-universe/provenance-class design in full** (part 2);
  **identifier = `A4b1-r4`** (part 3); and **part 4 — P-F is load-bearing, not a wording fix.**
- **The packet is LARGER than a pure re-baseline, and the class statement says so:** it is *both* a
  **§5.4 After-INADEQUATE repair** of `A4b1-r3`'s blocking defects B1/B2 *and* a **§5.4(2)
  `PREMISE_CHANGED`** re-bind to `M`. **No `A4b1` revision has ever been `ADEQUATE`; `A4b1-r3` is
  superseded.** Hence a **full** adequacy review, not a delta re-review.
- **What the packet carries:** DS3 rewritten for the **non-injective** forward map (four-case inverse —
  window-VA first, then high-water `XBOX_CONTIG_BASE + P`, then mapped low-RAM identity, then fail closed
  — citing `dma_resolve` **and** `xbox_memory_layout.c:843`, with `GPDMA_AMBIGUOUS` and the aliasing claim
  limit); DS6 with the **full** finite-universe `[GPIN]` design (MIXBUF `[NUM_MIXBINS = 32]`, PERIPH
  `[128]`, FIFO `[6]`, DMA region classes `[4]`, `BOOT_SCRATCH_READ`, `GPIN_OUT_OF_UNIVERSE` as a bug
  detector, `at_clear` freeze at `GP_CLEAR`, capped observation, fixed-count emission) with the r3
  table/`GPIN_OVERFLOW`/per-key lines/cut-off **retired and named as must-not-reappear**; DS5's CPU-site
  table sized one-per-enumerated-`jsrf_watch_store`-site with `CPU_ZERO_OVERFLOW` a bug detector; DS7's
  `[GPBOOT]` printing **both** `sge0` (raw) and `sge0_va` (translated); and AC-FIX `(viii)` kept with
  `(ix′)/(x)/(xi)` replacing `(ix)`, `(vii)` in **two** forms that can fail in both directions.
- **`A4b2` boundary note is in the packet** (`A4b2-r3`'s P2 and `AC-INPUTS` become stale; `A4b2-r4`
  re-brief items recorded). **`A4b2` is not revised here.**
- **Regeneration is a non-goal**, so the pytest gated lead **stays gated and unexercised** — no `pytest`
  work is manufactured.
- **Session verifications supporting the packet:** the Advisor's one **inferred** claim is now
  **OBSERVED** (slot `0x001C40E8` holds `0x800000AD` = ordinal 173 `MmGetPhysicalAddress`, with exactly
  **10** calls through it and exactly **6** returning into the DSOUND range
  `0x1A4E42–0x1A70C6`); and the DS3 citations are verified with the **ordering nuance** that DS3 must test
  window-VA **before** the high-water mark (`docs/reviews/a4b1-r4-ds3-citations-verified.md`).

**The premise reconnaissance (below) remains the measured basis for the re-bind.**

- **Frozen brief:** `docs/reviews/a4b1-r4-planning-brief.md` (the task, the verified baseline, the
  ordered reading list, the measured premise delta, the four premise answers, the pytest lead, and the
  mandatory sketch → Opus shape-preflight workflow).
- **Premise re-check:** `docs/reviews/a4b1-r4-premise-recheck.md`. **A4s changed exactly two of the files
  `A4b1` touches** — `src/apu/apu_dsp.c` (+56/−3) and `src/apu/CMakeLists.txt` (+7/−1);
  `apu_core.c`, `apu_state.h`, `apu.h` and `apu_mmio_hook.c` are **byte-identical**. So the APU surface
  is almost untouched, which is why this is a premise re-check and not a redesign.
  - **P-F is CORRECTED by the Advisor (part 4):** the Session's original reading — *"premise
    strengthened, a wording correction"* — **understated the change.** The forward map went from
    **identity** to **non-injective**, so low-RAM VA `X` and window VA `0x80000000+X` both map to physical
    `X`; the Session's proposed inverse formula was **wrong** and is superseded by the Advisor's four-case
    rule. The record carries a correction banner.
  - The other premises **hold or are unchanged**, including the strict stop.
  - **The inverse the packet needs is documented in-tree** (`docs/reviews/a4b1-r4-inverse-precision.md`,
    now superseded in its *formula* by the Advisor's part 4): `XBOX_CONTIG_BASE = 0x80000000` **is** the
    contiguous window, and `xbox_memory_layout.c:843-845` states the round trip — *"The contiguous window
    IS the physical-address view, so OR-ing its base is the documented round trip, not a guess."*
    **The Advisor's four-case inverse replaces the Session's `XBOX_CONTIG_BASE | (P & 0x0FFFFFFF)` form,
    which is wrong for any identity-passed low-RAM address.**
  - **The `& 0x03FFFFFF` hazard is real and pre-existing:** 8 live sites, including `apu_shim.h:101-123`
    and `apu_vp.c:846` — both **byte-identical** across the baseline move. Note the masks differ
    (`0x03FFFFFF` = 26 bits vs the round trip's `0x0FFFFFFF` = 28 bits), which is exactly the distinction
    Device semantics 3 exists to prevent.
  - **The `MIXDOWN_ALL` question is RESOLVED by source read** — it was the plan's flagged *uncertain and
    load-bearing* item. `monitor.frame_buf` is `int16_t[256][2]` at `apu_state.h:500` inside the **host**
    `MCPXAPUState` struct, its instances are host allocations, and there are **zero** guest-mapping
    references to it. So the default-on path writes only host monitor storage: **not new device
    behaviour, no new criterion.** It remains a `jsrf-run-profiles.md` classification item.
- **Everything else holds:** P-B, P-C, P-G (byte-identical), P-E, P-D's gate, and the strict stop. All
  `A4a` R0/R1 observations, the watch-ledger ruling, the Q1/Q2 rulings, the checkpoint-40 constraints
  and the `[GPIN]` redesign remain **admissible and un-reopened** — a baseline change alone does not
  reopen a technical ruling.

The new baseline for that revision: toolkit **`M`** = `3f8bf67c450861aefcbc376698750bc1446bc9dd`; exe
`E45026C3DF5AACAF3D66FCC1E17D9C6C1A12247864D58D0911CDBA8435A2A3C7`; the run
`logs/runs/20260925-210315-113-a4s-sync-strict` as the reference R0.

**Identifier correction (Advisor ruling, part 3; 2026-09-25).** This revision is **`A4b1-r4`**, not
`A4b1-r2`. The Session named it `A4b1-r2` in error and that label propagated into the frozen `A4s-r6`
packet's `R-SAME` cell. `A4b1-r2` is already taken: `r1` (commit `8735165`), `r2` (`127d203`, itself
adequacy-reviewed at `839E9BEC…D2BB7`) and `r3` (`fea49f1`) all exist, and an identifier that names a
reviewed document must never be reused. The frozen `A4s` packet is a closed record and owes nothing
(§5.4: record pointers never reopen a frozen packet); the label is corrected here. Full ruling:
`docs/reviews/a4b1-r4-planning-rulings.md`; the Session's error record:
`docs/reviews/a4b1-revision-identifier-collision.md`.

**Two claim limits and one gated lead carry forward into that planning:**

1. **KX does not exercise 7 pytest-dependent upstream modules under `unittest`** — `test_block_dispatch`,
   `test_incdec_carry`, `test_incdec_result`, `test_lifter_double_shift`, `test_lifter_result_clobber`,
   `test_sar_width`, `test_x87_classification`. *The lifter/translator conflict resolutions (hunks 5–7 and
   9) are therefore witnessed only by K4 and the existing KX modules, not by upstream's own tests of those
   paths.* Harmless for `A4s-r6` only because `AC-GEN` held (no regeneration). **`A4b1-r4` does not
   regenerate either** (regeneration is already one of its non-goals), so this lead stays **gated and
   unexercised** — no `pytest` work is manufactured to clear it early.
2. **`AC-STRUCT` detects duplicates, not omissions** — it did not see that one switch had lost `case 138`
   entirely; that class is excluded only by an explicit expected-count post-condition. **For any future
   revision, each HA edit should carry such a post-condition, enforced as an `AC-MERGE` check.**
3. **GATED LEAD (binding):** before **any** packet regenerates or relifts with the toolkit at `M` or
   later, all **56+** KX modules — including these 7 — must run under **real `pytest`** in an
   **owner-authorized** environment; parametrized and fixture tests included. **Do not install, borrow, or
   otherwise introduce `pytest` without owner authorization.**

### Predecessor — `A4s-r5` — **EXECUTED 2026-09-25, selected `R-CONFLICT`** (superseded by `A4s-r6`)

- **Packet (superseded):** `docs/packets/a4s-toolkit-sync.md`, revision **`A4s-r5`**, class **change**,
  frozen SHA-256 **`09DA9413C028D61BD28D9E4007AF6DDE3474ED6869F77B0095D03DB2C4FB86FB`** (263 lines).
  Recoverable from git; no longer the file at that path.
- **Status: executed. Selected row `R-CONFLICT`** (fail-closed, as predicted). **Toolkit `main` is left
  at `0d7929c`** (rolled back; working tree clean). **No push was performed at execution** — `main` at
  `0d7929c` is not a descendant of `origin/main`, so per the Closure rule: **"no push: main at
  `0d7929c`"**. (The accepted **baseline** was later published to the fork's `jsrf/integration` branch
  under the owner's standing push policy — see the push log above; `main` itself is still unpushed.)
- **Execution evidence:** `docs/reviews/a4s-execution-evidence.md`. **Conflict inventory:**
  `docs/reviews/a4s-r5-conflict-inventory.md` + raw hunk text in `logs/a4s/conflict-hunk-inventory.txt`.
- **Result:** the merge attempt produced **5 conflicted files, all inside the packet's 10-file set**
  (`kernel_bridge.c`, `xbox_memory_layout.c`, `lifter.py`, `test_icall_feedback.py`, `translator.py`)
  and **9 conflict hunks: 1 × HA, 1 × H1, 2 × H2, 5 × UNDECIDED**. Five UNDECIDED hunks select
  `R-CONFLICT`; the packet does not authorize choosing a side by judgment for them. `AC-MERGE`,
  `AC-KEEP`, `AC-INV`, `AC-BUILD`, `AC-TEST` and the strict run were **not reached** (row 2 precedes
  them); their read-only pre-checks are recorded as non-binding supporting evidence.
- **Pre-merge controls (step 2, all on `0d7929c`):** build OK; pre-merge exe
  `9597FF7C2A377265ABA8DBB90B461EBE763E02D65432E9DFA13ACD925539C553` (matches the recorded
  expectation); `ctest` **12/12**; set K4 **27 tests OK**; set KX **33 modules, Ran sum = 133**;
  set G **8 pass / 2 fail**, both failures **measured pre-existing** (see the `E1`/`E2` follow-ups
  below — `E1` now passes, so the current set-G baseline is **9 pass / 1 fail**).
- **Rollback rebuild exe SHA-256:** `AEC1F0FF7FB944DA44487EA15C3E94FF342751D4168E699F544A58895CF083C3`
  — **differs** from step 2; the packet says record, not gate, so no row turns on it. Cause **measured**:
  `/Zi` + `/DEBUG:FULL` with no `/Brepro`, so a **relink** stamps fresh PE timestamps. Step 2's build
  found the tree already up to date and **did not relink** (which is why it matched the archived
  `9597FF7C…`); the merge attempt and `merge --abort` then rewrote **58 toolkit files** (mtimes only —
  `git status` clean, `git diff HEAD` empty), forcing the rollback build to relink. A **no-op build was
  measured not to relink**. The two exes are identical in size, layout and all but **13 bytes** (four
  PE timestamp fields plus one stamp byte).
  Analysis: `docs/reviews/a4s-r5-rollback-exe-hash.md`. *(An earlier note here called the rebuild
  "deterministic"; that inference was wrong and is retracted — see that record.)*
- **Adequacy:** **`ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED`, `DEFERRED: NONE`
  — `docs/reviews/a4s-r5-adequacy-review.md`, fresh Planner child `f339b306-0473-4777-a211-78271453f2b9`.
- **Baseline recorded at execution:** game **`c1cdb91`** (the commit that adds the promotion;
  the plan previously named `b7d6af5`, which is the preceding startup-receipt commit — advisory,
  not revised), toolkit `main` **`0d7929c`**, `upstream/main` = `origin/main` = `v0.11.0^{commit}` =
  `766ecef`, merge base `051a128`, no `a4s-*` branch existed before step 1 (`a4s-pre-sync` now exists
  at `0d7929c` and still resolves to it).
- **Revision log:** `docs/reviews/a4s-revision-history.md` (non-authoritative).

**Push log — first durable-checkpoint push (owner-authorized, 2026-09-25).**

```text
PUSHED_TO:  https://github.com/danillogical/xboxrecomp.git   (remote `origin` = the owner's fork)
BRANCH:     jsrf/integration   (NEW branch; `main` cannot fast-forward yet)
COMMIT:     0d7929c86771dd0b971941592fd4f15436116e82
REMOTE_URL: https://github.com/danillogical/xboxrecomp.git
RESULT:     SUCCESS — verified independently with `git ls-remote origin`, which reports
            0d7929c…  refs/heads/jsrf/integration and 766ecef…  refs/heads/main (unchanged)
```

The 24 commits published are the **accepted toolkit baseline** (`A4a-r2`, `A3a-r25`, `A4p-r1` all
accepted on `0d7929c`; baseline tests re-measured this session: ctest 12/12, K4 27 OK, KX 133 OK). The
`A4s-r5` `R-CONFLICT` attempt produced **no commit**, so none of its output is in this push. **No force,
no `upstream` push, `main` untouched.** `main` stays at `766ecef` because it cannot be fast-forwarded
until the `A4s` merge lands `766ecef` in local `main`'s history; the owner's branch-handling rule covers
exactly this case. Full record and all five pre-push checks:
`docs/reviews/owner-push-policy-xboxrecomp-fork.md`.

**`A4s-r6` packet delivered; adequacy review completed and superseded by execution.** (This paragraph and
the ones that follow are the **planning-phase record**; the outcome is in the `CURRENT PACKET` result
block at the top of this file.) `docs/packets/a4s-r6-toolkit-sync.md`,
revision **`A4s-r6`**, SHA-256 **`75207C41B8E9964D3E1467F5E980D1B8DE4CB75A1E0AFE954F59D40FA27D86E3`**,
**374 lines** (the Planner's reported "325" was a non-empty-line miscount; the project convention is
total lines — `A4s-r5` is cited as 263, its LF count). The frozen `A4s-r5` is **untouched**
(`09DA9413…86FB`). A **fresh Kimi Planner** performed the binding §5.3 adequacy review of that exact
SHA, returning **`ADEQUATE`** (`BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED`) — recorded in
`docs/reviews/a4s-r6-adequacy-review.md`. Per §5.3, `ADEQUATE` requires only `BLOCKING = NONE` and
`PREMISE_FRESHNESS` not `FAIL`; on `ADEQUATE` the revision was frozen and promoted in the same step,
with no polishing pass.

**Current blocker / next action.** The Advisor's hunk rulings are **recorded and binding**
(`docs/reviews/a4s-r6-advisor-hunk-ruling.md`). The frozen `A4s-r6` planning brief was dispatched to the
Planner (`workbuddy-ai/kimi-k3`, effort omitted per the owner's instruction). The Planner wrote its
**sketch** (`docs/packets/a4s-r6-toolkit-sync.md`, 14 lines, ≤20 tool calls) and the Session relayed it
for the **mandatory shape preflight**. The Advisor returned **`SHAPE: PROCEED`** with three bounded
policy items to write into the packet:

1. **Gating tools must be tracked and pinned.** `AC-STRUCT`'s scanner, and any verifier the packet
   relies on, must live at a **tracked** path (e.g. the game repo's `scripts/`) pinned by SHA-256 —
   **not** in `logs/a4s/`, which is gitignored and which `AGENTS.md` forbids for durable helper source.
   Its controls must be packet criteria (positive on the merge preview; zero on both parents; UNKNOWN
   for an unparseable merge-changed file).
2. **The pre-ruled table does not exempt run-profiles rules 1–5.** Keep `AC-INV` and the `SCOPE` guard
   exactly as they are; and if the real merge produces a hunk outside the recorded 9, **or** a recorded
   hunk whose base/ours/theirs text differs from `logs/a4s/conflict-hunk-inventory.txt`, the result is
   **`R-CONFLICT`** — never resolved by analogy.
3. **"Expected `R-SAME`" is a forecast only** — no criterion may assume it. `R-PUSH` follows the owner
   push policy: all five pre-push checks, and **never** after `R-CONFLICT`, a rollback, or `INADEQUATE`.

The Advisor's stated **REVERSED_BY** conditions, which return the packet to sketch: the real merge's
conflict set differing from the 9 recorded hunks; the verifier showing edit-application H2 changes any
recorded classification other than hunk 5's; or the scanner failing its positive/negative control on the
pinned trees.

**Session support already delivered to the Planner** (`docs/reviews/a4s-r6-ac-struct-controls.md`):
measured control numbers for the scanner over `SCOPE` — `0d7929c` and `766ecef` both **0 findings**;
the raw preview **15** (12 marker lines + 3 structural); the resolved all-ours preview **3 structural,
0 markers**; resolved all-theirs **2** (resolving hunk 1 to upstream removes the duplicate
`bridge_KeResetEvent` that lives inside it). Two wording traps are recorded there: the criterion must
say **which tree** each number belongs to, and must name the **resolution** the control uses.

**The Advisor's rulings, in one line each** (full text and BASIS in the ruling record):

1. **Hunks 1–2 — COMBINED form**, fixed precedence: (1) in-place KEVENT if `guest_va_is_inplace_kevent`
   accepts, (2) `ke_shadow_lookup` handle if non-NULL, (3) `XBOX_TO_NATIVE` fallback. **Upstream's
   `bridge_resolve_handle` tier is NOT carried over** (for a nonzero VA it returns the token itself,
   which would pass a guest VA as a host HANDLE and make tier 3 unreachable). The ruling supplies the
   **exact C text** for all three functions; **exactly one** `bridge_KeResetEvent` survives, with the
   same three tiers. Two further edits are pre-ruled HA: delete upstream's duplicate clean-hunk
   `bridge_KeResetEvent`, and keep **one** `case 138` per switch (local's positions), located **by exact
   text, not line number**.
   - *Why not upstream alone:* the accepted, gating ctest `jsrf_inplace_event_bridge` asserts guest
     `SignalState` and previous-state values that only local's bodies write.
   - *Why not local alone:* upstream's timer model arrives in a **clean hunk** and registers timers only
     in the shadow table; ordinals 113 and 159 are both declared, so a wait on a timer would never see
     the event that fires it.
   - *Why in-place first:* `ke_shadow_remove` has **zero callers**, so shadow entries are never retired;
     and every reachable shadow populator writes type 8/9, which the sniffer rejects — so in-place-first
     never takes an object away from the shadow tier. It also defuses the `Size` hazard without deciding
     the `Size` question.
2. **Hunk 5 — per-line combined form** (derived by rule): `_FLAGS_UNDEFINED` loses `"lock xadd"`, keeps
   upstream's `"popfd"` and its comment; `_EFLAGS_SETTERS` keeps local's line; hunk 6's H2 result stands.
3. **Hunk 8 — LOCAL** (`"--functions", fns,`), pinned.
4. **Hunk 9 — full union**, local's members then upstream's, upstream's `_RESULT_SNAPSHOT_SETTERS` clause
   kept.
5. **H1 — accepted as proposed**: containment must include deletions.
   **H2 — the Advisor REJECTED the Session's added delete-vs-keep precondition** and kept the original
   one; H2's **action** must be defined as **EDIT APPLICATION** (apply each side's per-line edit to the
   base, then insert each side's insertions, local first), **not** a union of line presence. Ambiguous
   attribution → UNDECIDED. The Session **verified the Advisor's prediction exactly**: hunk 5 changes
   from UNDECIDED to H2 applies, every other hunk keeps its classification, and edit application
   produces the ruled `_FLAGS_UNDEFINED` text.
6. **`AC-MERGE`(d) gains a twin witness:** every base line a credited side deleted is absent from the
   result unless the other side replaced it.
7. **Post-resolution structural check required** (conflict markers; duplicate `case` values per switch
   per branch path; duplicate file-scope definitions per branch path), on the **resolved** tree
   **before** the build, over `SCOPE`, with positive and negative controls, selecting **`R-CONFLICT`,
   never `R-BUILD`**, plus a C2084/C2196/C2371 backstop. The Advisor ruled **against** a broader semantic
   review as unbounded. **H3 is not audited** (no corpus) — the packet must say so.

**Follow-up leads the Advisor recorded (NOT `A4s-r6` obligations):** the correct `Size` convention
(4 vs 16/40 — needs a primary source); tier 3's handle-typed treatment of a non-handle Ke* object
(pre-existing); `ke_shadow_remove` having no callers; the upstream timer model's observable behaviour
for JSRF; an indirect-thunk-call search behind "not declared ⇒ unreachable"; case 207's argument count
(local 36 vs upstream 40 — the merge silently took upstream's 40). Prior leads carry forward:
`MIXDOWN_ALL`/`USB_PORT` classification, E2, the function-style KX modules, and the exe-relink advisory.

**Staffing state — PLANNER CHANGED TO CLAUDE OPUS 5.5 @ `medium` (owner §3.4 decision, 2026-09-25).**
The owner **replaced `docs/agent-workflow.md`** and directed a staffing change. The file was **re-read
from disk**: its SHA-256 is now **`973CDDEF0C5206349E66DE15A6C0E830F85ACAD488231D3867F25B583E911748`**
(previously `CBEF9041…`), so it genuinely changed. Full record:
`docs/reviews/owner-staffing-update-20260925-planner-opus.md`.

```text
WORKFLOW_RELOADED: YES
WORKFLOW_SHA256:   973CDDEF0C5206349E66DE15A6C0E830F85ACAD488231D3867F25B583E911748
PLANNER_ROUTE:     claude/claude-opus-5-5
PLANNER_EFFORT:    medium
ADVISOR_CHILD:     5c555969-dea9-4b47-be05-62aa0835cde2
ADVISOR_ROUTE:     claude/claude-opus-5-5 @ high
```

**SECOND RELOAD, same day — implementation-worker progress gate (2026-09-26).** The owner replaced
`docs/agent-workflow.md` **again**; re-read from disk, its SHA-256 is now
**`F4F5D029D45A96CE5E4CA61ED106C0EFDF7CFD3FC99EB5F331E4D22D12F04A0C`** (was `973CDDEF…`). Full record:
`docs/reviews/owner-workflow-update-20260926-worker-gate.md`.

```text
WORKFLOW_RELOADED: YES
WORKFLOW_SHA256:   F4F5D029D45A96CE5E4CA61ED106C0EFDF7CFD3FC99EB5F331E4D22D12F04A0C
PLANNER_ROUTE:     claude/claude-opus-5-5 @ medium
ADVISOR_CHILD:     5c555969-dea9-4b47-be05-62aa0835cde2
ACTIVE_WORKER_PROGRESS_GATE_APPLIED: YES
```

- **Staffing is unchanged** — the Session read the new `§1` roster rather than assuming; all six DSH
  rows are identical to the previous reload, so **no route re-resolution was needed** and the existing
  Planner resolution (`claude/claude-opus-5-5`, `medium`) and Advisor child stand.
- **The only substantive addition** is the `§2.2` **implementation-worker anti-loop progress gate**: by
  **15 tool calls** a bounded implementation worker must have produced a concrete execution artifact
  (edit, compile/build attempt, test run, generated fixture, or bounded blocker report); after the first
  attempt further reads must be tied to a **specific observed** compiler/linker/test/runtime failure;
  **two consecutive 10-call stretches** with no new artifact/measurement/narrowed blocker mean the worker
  stops; re-reading the same files or re-litigating settled alternatives counts as **no progress**; and
  a blocker returns the fixed `STATUS: BLOCKED` form. **A no-progress or blocker return is an escalation
  signal — the Session does not auto-spawn an identical replacement**, but routes it through the
  escalation ladder first.
- **Applied immediately to the active worker** (`1fe1b4f2-…`, rewriting the `AC-FIX` fixture for the
  `FIFO_READ` ruling (C)). It had **already exceeded the new first-artifact budget**, so per the owner it
  was **not** given a fresh 15-call window; the owner's bounded correction
  (*"STOP RESEARCH CHURN AND EXECUTE OR REPORT A BLOCKER"*, next 3 tool calls: produce the smallest safe
  artifact and compile/test it, **or** return the bounded `BLOCKED` report) was sent, with the production
  changes listed factually so no re-derivation was needed. **The correction was recorded as issued under
  the new workflow.**
- **Packet, baseline, Advisor child and every durable ruling are preserved**; scope is unchanged. The
  workflow change alone reopens nothing.

- **The Planner route resolves live and exactly** (§1 `LIVE_RESOLVE`): `list_subagent_models` returns
  **one** canonical match, `claude/claude-opus-5-5`, and it advertises **`medium`** among its efforts
  (`low, medium, high, xhigh, max`). **No fallback was used.**
- **The §1 roster changed in three rows, not one** — found by diffing, not assumed. The **Planner** moved
  to Opus 5.5 @ `medium`; and the **acceptance reviewers swapped** to stage 1 `workbuddy-ai/hy4-preview-f`
  @ `high` and stage 2 `workbuddy-ai/deepseek-v4.1-flash` @ `max`, which is exactly the pairing the
  session had already been using, so the revision **aligns the document with practice**.
- **No in-flight Planner work existed to abandon.** Every Kimi Planner child had **finished** and its
  output is durable: `3fbfef10` (`A4s-r6`), `7332c25d` (the `A4b1-r4` packet), `95429607` and `5db5fd71`
  (the two `ADEQUATE` verdicts). The only running child, `f9d0e439`, is a **Worker** — a contract role
  whose route (`workbuddy-ai/deepseek-v4.1-flash` @ `max`) is **unchanged** — so it continues.
- **Durable work is preserved with its original model identity as provenance.** `A4b1-r4` remains the
  promoted, authorized packet and **execution continues**; the owner explicitly forbids retroactive
  invalidation of work completed before the change.
- **Planner/Advisor separation (same model family, different roles):** the **Opus 5.5 Medium** child is
  **Planner authority only**; the existing **Opus 5.5 High** persistent child
  (`5c555969-…`) is **Advisor authority only**; **neither is reused for the other role in the same
  decision**, and the Advisor child is **kept, not replaced or reprobed**. The **mandatory Advisor shape
  preflight remains in force** for new change packets/material redesigns, and it is **not** the formal
  adequacy review. For a change packet materially authored by an **Opus 5.5 Medium** Planner, the
  **binding adequacy review goes to a fresh Opus 5.5 Medium Planner child** — not the authoring child and
  not the Opus High Advisor.
- **Going forward, no new Planner work goes to Kimi K3.** The next Planner dispatch — the `A4b2-r4`
  revision the `A4b1-r4` boundary note calls for — goes to a **fresh `claude/claude-opus-5-5` @ `medium`**
  child. The Session route is unchanged (`workbuddy-ai/deepseek-v4.1-flash` @ `max`).
- **Superseded (provenance only):** the earlier Planner arrangement in which `docs/agent-workflow.md` §1
  named **Kimi K3** with a `max` qualifier, and the owner's then-instruction *"kimi k3 doesn't take an
  effort, so don't worry about effort for kimi k3"*. That arrangement is **no longer in force**; it is
  retained here only because the packets and verdicts authored under it are durable and cite it.
  `docs/agent-workflow.md` §1 was **not** edited by the Session (§3.4).

**Execution rulings (binding, recorded).** `docs/reviews/a4s-r5-execution-startup-ruling.md`:
**Q1** Planner effort (superseded by the owner instruction above); **Q2** P0.8 recorded as a literal
**FAIL** and execution proceeded under a **process exception** (the only dirty path is the owner's
`docs/agent-workflow.md`, a non-build, non-evidence file outside every path-scoped check) — the file was
**not** committed or normalized; **Q3** the `AC-INV` "new environment names" command cannot run as
written on PowerShell 5.1 (embedded `"` is stripped), so an **interpretation ruling** authorized one
substitute form with three controls (42 / 44 / empty-and-exit-1), all of which passed;
**Q4** two pre-existing set-G failures (`E1` `test_agent_docs.py`, `E2` `test_ac2_provenance.py`) are
**named exceptions** — measured byte-identically on a pristine HEAD extraction, hence not
merge-relevant — and the KX carve-out was **not** extended to set G wholesale. `AC-TEST` was not
reached, so Q4 is prospective.

**Follow-ups (recorded, not authorized work):**

- **E1 — RESOLVED as a side effect of this closure; recorded as `PREMISE_CHANGED`.** Four
  current-tense mentions of a since-retired route name tripped `scripts/check-agent-docs.py`'s
  `RETIRED_NAMES` check. They sat in the superseded staffing block that this closure rewrite replaced,
  so they are gone and `tests/test_agent_docs.py` now **passes** (`Ran 30 tests, OK`); the checker
  reports **0 findings**. No test, script, packet or frozen artifact was edited to achieve this.
  **Any re-run of `A4s-r5` must re-measure the set-G baseline** — it is now **9 pass / 1 fail**, not the
  8/2 measured at step 2. Full record: `docs/reviews/a4s-execution-evidence.md`, "Premise change to
  `Q4`'s E1 exception".
- **E2** — `tests/test_ac2_provenance.py` fails on `A0` and `P1` with clause-D reason "the classifier
  was NOT edited; step 3 requires line 391 to be corrected". Cause unconfirmed; diagnose before anyone
  relies on the AC2 provenance tool.
- **KX claim limit** — ≥16 function-style toolkit test modules are not exercised by `unittest`, before
  or after the merge; run them under their own runner (they have `__main__` blocks) or convert them.
- **CRLF working-tree drift** — the merge/abort rewrote 4 toolkit files to CRLF (`core.autocrlf = true`).
  `git status` reports no change and the CRLF→LF bytes hash exactly to the archived `A4a-r2` reference;
  recorded so a future raw-hash comparison is not misread as a regression.

**ROUTE STATE — RESOLVED (2026-09-25, `session-58e86358`); the senior-judgment routes are live.**
The previous state (both senior routes BLOCKED; the substitute unresolvable) is **superseded**, as is
the temporary owner-authorized staffing exception recorded in
`docs/reviews/startup-20260925-session-910703eb.md` — the owner replaced `docs/agent-workflow.md`, and
its §1 roster is now the only persisted staffing policy. This session resolved every §1 DSH row live:
**Claude Opus 5.5 @ `high`** (`claude/claude-opus-5-5`) spawned and passed the two-turn continuity and
read-yourself probe; **Kimi K3** (`workbuddy-ai/kimi-k3`) resolved and answered (effort omitted per the
owner instruction); first-stage reviewer `workbuddy-ai/hy4-preview-f` @ `high` and second-stage
`workbuddy-ai/deepseek-v4.1-flash` @ `max` both answered their probes. **No route was substituted.**
Receipt: `docs/reviews/startup-20260925-session-58e86358.md`.

> **SUPERSEDED FOR FUTURE PLANNER WORK (owner §3.4 decision, 2026-09-25, later the same day).** The
> owner **replaced `docs/agent-workflow.md`** again and moved the **Planner** to
> **`claude/claude-opus-5-5` @ `medium`**. The paragraph above remains an accurate **receipt of the
> startup probes at the time it was written** — including the Kimi K3 Planner resolution — and is
> **retained as provenance**, because the packets and verdicts authored under that roster are durable.
> **For all future Planner dispatches the staffing-state block above governs: Opus 5.5 Medium, no Kimi.**
> The Advisor row (`claude/claude-opus-5-5` @ `high`) is **unchanged**, and its existing child
> `5c555969-…` is **kept, not reprobed**.

**Preserved binding rulings.** The `A4s` AC'97 hunk rulings (`docs/reviews/a4s-ac97-hunk-ruling.md`,
including "Interpretation ruling 2: scope") and the `A4b` rulings remain **binding** and were
**preserved, not reopened** — the packet *cites* them rather than restating them, and the model/provider
change does not reopen a ruling.

**Superseded route state (historical).** The earlier `ROUTE STATE` and temporary-staffing blocks
recorded in this plan and in `docs/reviews/startup-20260925-session-910703eb.md` are **superseded**.
They described a since-replaced staffing arrangement under the previous workflow revision, including a
now-retired route that ran out of quota during the `A4s-r5` review. The owner has since replaced
`docs/agent-workflow.md`, whose §1 roster is the only persisted staffing policy. Prior route-failure
diagnoses are retained as provenance only:
`docs/reviews/route-failure-20260924-workbuddy-substitute.md`,
`docs/reviews/route-failure-20260924-claude-pool.md`. **`docs/agent-workflow.md` §1 was not edited**
by this session — §1 is an owner-reserved decision (§3.4).

**Planner research-sufficiency stopping rule (unchanged):** the Planner stops investigating as soon as
§5.1's planning test is met and leaves remaining unknowns to the packet.

**Queued in order:** (1) **DONE** — the `A4s-r5` re-review returned `ADEQUATE`, the revision was
promoted, and it has now been **executed** (row `R-CONFLICT`, above); (2) **`A4s-r6`** — the Planner
revision that resolves the five UNDECIDED hunks from the inventory above, with hunks 1, 2, 5, 8 and 9
going to the Advisor first and hunk 5's `H1` rule gap clarified; (3) the `A4b1-r4`/`A4b2-r4` revision
per the binding `[GPIN]` redesign. The existing Advisor rulings are **binding** and are not reopened by
a provider or staffing change.

**Revision history of this packet** (each round's blocker was in the same criterion, so the
Session ruled the last fix a **simplification** under §5.5, not a patch):

| Rev | Verdict | Blocker |
|---|---|---|
| `r1` | INADEQUATE | a false `R-MOVED` on a shifted `B` |
| `r2` | **ADEQUATE** | — (but the fork premise then changed) |
| `r3` | INADEQUATE | the new greps matched a **comment** and an **unbuilt scaffold** → guaranteed false FAIL |
| `r4` | INADEQUATE | `AC-KEEP` (iv)'s **end anchors were not unique** (`        }` ×37, `            }` ×20) → false FAIL on a preserved merge |
| `r5` | **ADEQUATE** | — promoted, then **executed** → `R-CONFLICT` (B1-r4 closed: unique-first-line anchor + fixed length 7/56/25 + byte compare; anchors 1/1/1) |

**The sync itself:** merge **`upstream/main`** (**v0.11.0**, `766ecef`) into local `main`,
rebuild **without regenerating `src/recomp/gen`**, run both repositories' tests, rerun **one
strict baseline**, and push nothing during execution. **If the strict stop moves, that is the
next brief.** Verified facts and the conflict surface: `docs/reviews/toolkit-sync-instruction.md`.

**The AC'97 hunk is pre-ruled** (`HA`): it resolves to the **LOCAL** `A3a-r25` model; upstream's
NABM trap enters **unarmed** with zero call sites. The Advisor's **"Interpretation ruling 2:
scope"** fixes the check's scope: `SCOPE` = the toolkit's **`src/` and `include/`**, deleted
names match only as **quoted string literals**, and comments/docs/tests/unbuilt scaffolds are
**inventoried, never failed**. See `docs/reviews/a4s-ac97-hunk-ruling.md`. In the executed attempt this
was the **only** pre-ruled hunk and it was classified `HA`; the five UNDECIDED hunks are the blocker.

**Push policy (owner) — superseded and broadened 2026-09-25.** The previous wording was *"push toolkit
`main` to `origin` when a packet closes"*. The owner has now made **durable-checkpoint pushes the
standing practice** for toolkit work: push after an accepted toolkit implementation, a completed
sync/merge packet, a materially useful commit later packets depend on, or a clean milestone boundary —
not only at the end of the project. Full text, the verified remote table, and the pre-push checklist:
**`docs/reviews/owner-push-policy-xboxrecomp-fork.md`**; operating summary in `AGENTS.md` "Toolkit
remotes".

**Remotes verified this session with `git remote -v` (names were not assumed):** the owner's fork is
**`origin`** → `https://github.com/danillogical/xboxrecomp.git` (fetch **and** push); `upstream` →
`https://github.com/sp00nznet/xboxrecomp.git` (fetch; push `DISABLED`). **The fork is already
configured, so no remote was added, renamed, or altered, and `upstream` was not touched.** The
instruction's suggested name `fork` was deliberately **not** used — a second name for the same URL is a
way to push to the wrong destination by accident. The game repository has no remote.

**A measured constraint that decides how a future push works.** `origin/main` is `766ecef` (*"Release
v0.11.0"*, i.e. upstream's release line) and is **not** an ancestor of local `main`; the two have
diverged **169 upstream-only / 24 local-only**. So `git push origin main` is currently **non-fast-forward
and would be rejected**. It becomes a valid fast-forward only once the `A4s` merge lands `766ecef` in
local `main`'s history — which is exactly what `A4s` is for, and what the packet's Closure clause
already assumes. Nothing is manufactured to force it; the workflow decides when a merge is valid.

**No push occurred in this execution** — `main` is at `0d7929c`, which is not a descendant of
`origin/main`, and `R-CONFLICT` is a **no-push state** under the new policy as well.

**Follow-up — two new upstream variables need a `jsrf-run-profiles.md` classification.** The sync brings
in two environment names that did not exist locally (Advisor-observed at the merge tree `75083476`, and
**independently re-measured by this session** with the authorized `-f` pattern-file form: 42 unique
names at `0d7929c`, 44 at `75083476`, difference exactly these two, none removed):
**`RECOMP_APU_MIXDOWN_ALL`** (`apu_dsp.c:91`, **default ON**, sums all 32 mixbins into the host monitor
buffer) and **`RECOMP_USB_PORT`** (`ohci.c:819`, selects the port the virtual pad appears on). Neither is a
classifier-listed or deleted name, so neither fails the merge; they are recorded as **"new unclassified
variable"** and need a classification edit. **Uncertain and load-bearing:** whether `MIXDOWN_ALL`'s
default-on path writes anything the **guest reads back** — the Advisor did not verify that
`monitor.frame_buf` is guest-invisible. **If it is guest-visible, it is new default-on device behaviour**,
and `A4b1` (which rewrites `apu_dsp.c`) must classify it before any strict APU claim.

**Candidate future blocker — the RR wait (Advisor lead, not a packet).** JSRF contains
upstream's AC'97 "Reset Registers" pattern: `0x1A6F6F mov byte [eax+0xFEC0010B],2`, then
`0x1A6F7F mov cl,[eax+0xFEC0010B]` / `and cl,2` / `0x1A6F88 test cl,cl` / `jne 0x1A6F88` — a
hoisted single read that spins on the RR bit self-clearing. The same RR write occurs at
`0x1A7406`. **Uncertain whether current runs reach it**; the A3a/A4a runs stop at the DSP spin
first, which is consistent with not reaching it. If a later run stalls at `0x1A6F88`, the RR
self-clear becomes its own admission packet (the trap then becomes a precondition of `A4b`
rather than a follow-up). It is **not** a reason to arm the trap in `A4s`.

**Then:** `A4b1`/`A4b2` (drafted, split by a fresh Planner from `A4b`) resume with
re-established baselines. Their in-flight adequacy verdicts remain useful as design
feedback. `A4b1` = port, licence, fixtures, unchanged default path, no guest-run claim,
toolkit-only writes; `A4b2` = the strict trap+trace run (boot, run, clear, no-CPU, inputs),
game-only writes, with preconditions P1 (`A4p-r1` ACCEPTED `O-GATE`) and P2 (`A4b1`
ACCEPTED).

## Draft packets — `A4b2` only (`A4b1` is **ACCEPTED and PUSHED**; see `CURRENT PACKET` above)

> **Corrected 2026-09-26.** This block previously described **both** packets as drafts at r3. **`A4b1` is
> no longer a draft:** `A4b1-r4` was **accepted (`R1-PASS`)** and **pushed** to the fork at
> `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d`. Its history is preserved below rather than deleted, because
> the `A4b2` revision cites it.

- **`A4b1` — ACCEPTED, PUSHED, COMPLETE (historical entry).** Was `A4b1-r3`, SHA-256
  `321ABCF7C9319B5AC4661B384A21CA2D2CE39C792D9820C9C322B7979EC7AFDA` (373 lines); **superseded by
  `A4b1-r4`** (`6DD62A57E87445F5C12085210146204AA4E77D26FD316071FD41CAEC76835C38`, 445 lines), which
  passed **all four criteria** and every gate and was **accepted by the two-stage review** — stage 1
  `NOT ACCEPTED` on `AC-PORT`, corrected, **stage 2 `ACCEPT`**. `A4b1-r3` remains recoverable from git
  (`git cat-file blob 3f05c0f95205bfef06c0d75a0ba1038b670e9550`). **Do not reopen.**
- **`A4b2`:** `docs/packets/a4b2-gp-clears-pending-word.md`, currently `A4b2-r3`, SHA-256
  `CFB8C0EBFBC9BE3CFB62677C4C756694DDE9663542E239570B639DED9F5BFCE9` (334 lines, including the appended
  r4 sketch). Claim: in one strict run (`RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`) the `loc_001A18D0` wait
  was satisfied by modelled GP execution of the guest's own command. Preconditions P1 (`A4p` `O-GATE`) and
  P2 (`A4b1` ACCEPTED) are **satisfied** — but **P2 pins `A4b1-r3` and its `AC-INPUTS` decides from the
  retired per-key `[GPIN]` lines and `GPIN_OVERFLOW`, so the packet is stale against the accepted
  baseline.** Its revision to **`A4b2-r4`** is planned and its **sketch was cleared by the Advisor
  (`SHAPE: PROCEED`)**; **the body is not yet written because the Claude route is down** — see the
  `CURRENT PACKET` block. Game-only writes.
- **The ledger (ruling 1) is sound and unchanged.** Device semantics 6 is a **write-once
  watched-word ledger** (atomic `seq`, uncapped counters, per-class latches — `GP_CLEAR`,
  `GP_ZERO_OVER_ZERO`, `GP_ZERO_OVER_OTHER`, `GP_NONZERO_OVER`, `GP_PARTIAL`; CPU
  `ANCHOR`/`ZERO`/`ZERO_OVERFLOW`/`OTHER` via an exported `apu_watch_cpu_store`); Device
  semantics 7 is trace-only. Three independent reviews confirmed it faithful to the ruling and
  closed all seven r1 blocking defects.
- **The input accounting (`[GPIN]`) is being redesigned (ruling 3, §5.5 third trigger).** Three
  consecutive blocking verdicts: a lossy once-per-key log decided `AC-INPUTS`; the fix was a
  256-entry table; and that table's key universe is **1024 words** for MIXBUF alone
  (`GP_DSP_MIXBUF_BASE 0x001400` + `DSP_MIXBUFFER_SIZE 1024`, Session-verified), so one frame's
  mixbin sweep overflows it → false `R2-UNKNOWN`. The redesign keys **provenance classes over
  statically enumerated finite universes** (MIXBUF 32 bins, PERIPH 128, FIFO 6, DMA region
  class 4), with counters that **cannot overflow by construction**, a write-once `at_clear`
  freeze at `GP_CLEAR`, and `GPIN_OUT_OF_UNIVERSE` as a **bug detector**. See
  `docs/reviews/a4b-gpin-accounting-ruling.md`.
- **Split decision (Planner, one line):** split, because the port's scale risk is settled by
  build+ctest+one default run and should not wait for, or be reviewed with, the run criteria.
  The Advisor confirmed **do not merge the packets** — the leak was a decision rule placed in a
  packet that cannot change the code producing its input, not the split itself.
- **`AC-PIO` and `R-PIO-DATA` are deleted** from both — `A4p`'s `O-GATE` discharged Q1
  condition 3, so the criterion is retired rather than repaired.
- The former `docs/packets/a4b-gp-dsp-engine.md` (`A4b-r3`) is **superseded** by this split
  and retained as provenance only.
- **Open:** `A4s` is not accepted, so neither packet has a baseline yet. If a pinned xemu write
  path cannot route through the single GP write function, `A4b1` stops and goes to the Advisor.
- **Records:** `docs/reviews/a4b-watch-ledger-ruling.md` (ruling 1, verbatim);
  `docs/reviews/a4b-gpin-accounting-ruling.md` (ruling 3, verbatim);
  `docs/reviews/a4b1-a4b2-r2-adequacy-review.md` and `…-r3-…` (both `INADEQUATE`);
  `docs/reviews/a4b-pio-methodology-ruling.md`; `docs/reviews/a4b-xemu-pin.md`;
  `docs/reviews/a4b-q1-advisor-ruling.md`; `docs/reviews/a4b-q2-owner-decision.md`.

## Last closed packet — `A4p-r1` (discovery, ACCEPTED 2026-09-24)

- **Packet:** `docs/packets/a4p-pio-gate-analysis.md`, revision `A4p-r1`, class
  **discovery**, frozen SHA-256
  `B8BDBFAEC31213AA43D7A2BC8771326A73BE6CC124B6E2993BD2D33C57687A4B`.
- **Question:** are the 28 direct reads of `0xFE820010` in `DSOUND` gate-only under the
  locally-checked calling-convention rule C1–C4?
- **Result — selected outcome row `O-GATE`:** E0–E2 clean, **all 28 sites PASS**, 0 FAIL,
  0 UNKNOWN, E3 clean. Every loop is a threshold re-poll with no store; on every exit path
  `T` empties before any use, by immediate overwrite, by a callee-save `pop`, or by C1 at a
  call. **C3 was invoked at 0 of 28 sites.** The `0xFE800000` table resolves to voice-list
  registers (`0x2054…0x2074`), not `PIO_FREE`, and the two stray constants are unreferenced.
- **This discharges Q1 condition 3**, and therefore **retires `AC-PIO`**: `PIO_FREE` is
  gate-only, so the stub decides whether the guest reaches a point but not the data it
  writes. `A4b` cites this row as a precondition.
- **Claim limits:** a discovery packet never satisfies a strict criterion and never claims
  anything works (§5.8). The result rests on the **inferred** premise that DSOUND follows
  the standard x86 convention, checked at every boundary the analysis relies on but not
  everywhere; it cannot see a custom `edx`-return convention passed on untouched; it covers
  only the 28 direct reads, not register-indirect or computed access beyond E3, timing, or
  whether `0x80` is the true device value.
- **Records:** adequacy `docs/reviews/a4p-r1-adequacy-review.md` (`ADEQUATE`);
  execution `docs/reviews/a4p-execution-evidence.md` (SHA-256
  `3DF5D833E89863E5E9BD0264CC64154C6FEADBD6FE10FC1806F6323FF20981F9`); acceptance
  (`ACCEPT`, first stage, final) `docs/reviews/a4p-r1-acceptance-review.md`; revision log
  `docs/reviews/a4p-revision-history.md`.

**Follow-ups carried forward (not authorized until promoted in a packet):**

- **`A4b` ordinary repairs** (§5.4, different mechanism from the retired `AC-PIO`): the
  `[GPDMA] watch` cap must exempt the deciding `payload=0` line (Advisor ruling (d)), plus
  r3 deferred D1, D2, D4, D5. D3 is superseded by C4 and is discharged by `A4p`.
- **C3 caller-enumeration text** (in the frozen `A4p-r1`, deferred): it searches
  `sub_<ENTRY>(`, which matches only the definition; any future C3 use must enumerate
  callers by value — XBE `call rel32` to the entry, cross-checked by the normalised target
  literal in `RECOMP_ABI_CALL(0x<ENTRY>u, sub_<ENTRY>)`. Second instance of the
  `AGENTS.md` rule against enumerating by one spelling.
- **`PIO_FREE` model packet:** still required before the **first strict liveness or
  boot-progress criterion past the spin**. Finding for it: xemu's own `vp_read` calls its
  `0x80` a pretence, so the obvious secondary source cannot corroborate it.
- **Scope `A4b` to the whole GP block, not just GPRST:** R1 shows five GP offsets written.
- `gp_ep_reads` counts only `[APUMMIO] read` lines, so a GP read-modify-write logged as a
  write would not register. Tighten if the row is reused.
- Archive raw `ctest` output in run directories.
- `P0.1-AC1` re-review; moving `RECOMP_AC97_READY` into the classifier's
  `RETIRED_OVERRIDES` registry; the unmodelled AC'97 registers `0xFEC0017C` / `0xFEC00100`.
- Owner-reserved, unchanged: the `0xFE820010` "GP sample counter" / `PIO_FREE` naming
  conflict (`docs/jsrf-run-profiles.md:288-292`).

## `A4b` — parked, awaiting `A4p`'s outcome

**`A4b`, the GP DSP56300 engine**, is the packet `A4a-r2`'s row `O-6` selects, and both its
gates are answered (Q1 by the Advisor, Q2 by the owner). It is **not** promoted and has had
three revisions and two `INADEQUATE` verdicts, both on its `AC-PIO` criterion — which is why
the §5.5 redesign moved that criterion into `A4p`. When `A4p` returns `O-GATE`, the next
`A4b` revision deletes `AC-PIO` and `R-PIO-DATA`, adds the `A4p` precondition, and folds in
the ordinary repairs (different mechanism, §5.4): the `[GPDMA] watch` cap must exempt the
deciding `payload=0` line, plus r3 deferred D1, D2, D4 and D5. D3 is superseded by C4 and
lives in `A4p`. Its other criteria are otherwise unaffected.

**Records:** `docs/reviews/a4b-r3-adequacy-review.md` (second `INADEQUATE`, verbatim);
`docs/reviews/a4b-r2-adequacy-review.md` (first `INADEQUATE`); `docs/reviews/a4b-r2-session-checks.md`
(the import at `0x1C4004` = kernel ordinal 161 `KfLowerIrql`); `docs/reviews/a4b-r1-adequacy-attempt-1.md`
(`pending — reviewer unavailable`); `docs/reviews/a4b-q1-advisor-ruling.md` (Q1 plus the
PREMISE_CHANGED addendum and the condition-3 handoff to `A4p`); `docs/reviews/a4b-planning-rulings.md`
(checkpoint-40 plus the 3(ii) amendment); `docs/reviews/a4b-xemu-pin.md` (xemu pin
`67cc79e663038d1f55448c0f566b37dde016adf6`); `docs/reviews/a4b-q2-owner-decision.md`;
`docs/reviews/a4b-revision-history.md` (non-authoritative).

**General rules recorded** (Advisor-directed): (`docs/agent-workflow.md` §5.5) a criterion
whose evaluation is itself a multi-step static or dynamic analysis the Planner cannot
complete by reading belongs in a discovery packet that runs first, and the change packet
cites its accepted outcome as a precondition; (§6.1) a criterion claiming something about
*every* access to an address must derive its population from the original XBE with operands
normalised to `uint32`, reconcile by normalised **value** rather than spelling, freeze the
count and generating command, condition PASS on `count == frozen count`, and state what the
method cannot see. The lifter spelling fact is operating knowledge in `AGENTS.md` under
"Generated-source rules".

**General rule recorded** (Advisor-directed, `docs/agent-workflow.md` §6.1): a criterion
claiming something about *every* access to an address must derive its population from the
original XBE with operands normalised to `uint32`, reconcile against the generated code by
normalised **value** rather than spelling, freeze the count and generating command, condition
PASS on `count == frozen count`, and state what the method cannot see. A text search is a
lead, never a completeness witness. The lifter spelling fact (an `A1` moffs load → hex; a
ModRM `disp32` → signed decimal; every address ≥ `0x80000000` exposed) is now operating
knowledge in `AGENTS.md` under "Generated-source rules".

## Last closed packet — `A4a-r2` (discovery, ACCEPTED 2026-09-24)

- **Packet:** `docs/packets/a4a-dsp-pending-word.md`, revision `A4a-r2`, class
  **discovery**, frozen SHA-256
  `2366E18C583B3ED82124F0E5D3AAD14DB38B4D3CA493F1695E44FFE779722FBC`.
- **Question:** is the next packet the GP DSP56300 engine (`A4b`), and what must its scope
  include? Unknowns U1–U4: the last `GPRST` value; whether the title reads GP/EP at all;
  whether scratch page 0 holds the XBE `0x001BA0A0` image with the pending word `3` at
  `+0x810`; whether the stop holds on the instrumented build.
- **Result — selected outcome row `O-6`** ("the start write is the only GP interaction; the
  image is in place"), from two strict runs on game `5b58d13` / toolkit `0d7929c`:
  - `U1` answered: `last_GPRST = 0x00000003` (`&3 = 3`), and the trace now reports the
    values the guest actually wrote. The value oracle matched the original XBE immediates
    exactly: `write 0x3FF14 = 000000FF` (`0x001A582A`) and `write 0x3FFFC = 00000003`
    (`0x001A585E`), where the pre-change build logged `00000000` for both.
  - `U2` answered: **zero** GP/EP reads, with a coverage witness (the once-per-block read
    note is unbounded by the trace cap and never fired).
  - `U3` answered: `G = 803CC000`, `S0 = MEM32(G) = 803C0000 = B`, and all `0x5CC` bytes
    equal the XBE image at file offset `0x1A7D60`.
  - `U4` answered: both runs `diagnostic_deadline` with `F = 2` on the offset-unpinned
    frame pattern, i.e. the stop holds with and without the trap.
- **Instrumentation:** toolkit `0d7929c` (`src/apu/apu_mmio_hook.c` only, 39+/6−). T1
  reports the value the access moved; T2 names the 400-line cap when reached. Both sit
  inside the existing `RECOMP_APU_TRACE` block, off by default; the R0 leak check is their
  witness (0/0/0). exe `87971604…7ef` → `9597ff7c…c553`; `ctest` 12/12.
- **Claim limits:** a discovery packet claims observations only and never satisfies a
  strict criterion (§5.8). R1 reached the spin only after its `0xFE820010` polls were
  answered by the `PIO_FREE` stub constant `0x80`, so **its arrival at the spin is
  exploratory-grade however R1 classifies** (Advisor ruling B). Nothing here shows the GP
  would clear `+0x810` if it ran — only real GP DSP56300 execution can (ruling D).
- **Records:** adequacy `docs/reviews/a4a-r2-adequacy-review.md` (`ADEQUATE`,
  `BLOCKING: NONE`); execution `docs/reviews/a4a-execution-evidence.md`; acceptance
  (`ACCEPT`, first stage, final) `docs/reviews/a4a-r2-acceptance-review.md`; revision log
  `docs/reviews/a4a-revision-history.md` (non-authoritative). Superseded `A4a-r1` is
  preserved at game commit `5e739f6`.

**Follow-ups carried forward (not authorized until promoted in a packet):**

- **Gates `A4b`:** **Q1 — ANSWERED** (Advisor, 2026-09-24): the upstream `PIO_FREE` stub
  dependence does **not** contaminate A4b's strict claim, and a `PIO_FREE` model is neither
  a prerequisite for A4b nor part of it. Case ruling recorded verbatim in
  `docs/reviews/a4b-q1-advisor-ruling.md` (Advisor child
  `407c54a3-6ca4-4a65-835b-faf5355195cd`, route `claude/claude-opus-5-5` @ `high`); its
  general clarification is in `docs/jsrf-run-profiles.md` §"Feature enablement". A4b must
  still establish the ruling's four conditions (who wrote the 0; the GP's input
  provenance; that `PIO_FREE` only gates; the claim limits) and must cite the ruling.
  **Q2 — ANSWERED, owner decision (§3.4), 2026-09-24:** the owner accepts
  **GPL-2.0-or-later** for this open-source project, so porting xemu's DSP56300 core is
  permitted. Recorded in `docs/reviews/a4b-q2-owner-decision.md`. The licensing obstacle is
  removed; whether A4b ports the existing core or implements one independently is now a
  **technical** choice for the Planner. **Mechanical consequence for A4b's closure:** a
  binary linking a GPL-2.0-or-later core is a combined work that must ship under
  GPL-2.0-or-later, so closure must update `NOTICE` and the licence files (verbatim GPL text
  alongside the existing LGPL text). That is bookkeeping following from the decision, not a
  further owner question. A4b should also **pin** the xemu commit it ports; the sources
  fetched this session were `master` and are not pinned.
- **`PIO_FREE` model packet (Advisor-directed prerequisite):** required before the **first**
  strict liveness or boot-progress criterion past the spin (e.g. "the title reaches
  <checkpoint> in a strict run"). It is its own packet, placed before that criterion, not
  inside A4b. **Finding for it:** xemu master `hw/xbox/mcpx/apu/vp/vp.c` `vp_read` returns
  `0x80` for `NV1BA0_PIO_FREE` with the comment *"we don't simulate the queue for now,
  pretend to always be empty"* — the obvious secondary source describes itself as a
  pretence, so it cannot corroborate `0x80` as device state, and our `apu_vp.c` is
  xemu-derived and therefore not independent either. The `jsrf-run-profiles.md` reversal
  condition (ii) most likely needs a real FIFO/free-count model or a primary source, not a
  citation.
- **Scope `A4b` to the whole GP block, not just GPRST:** R1 shows five GP offsets written
  (`0x3FF00`, `0x3FF04`, `0x3FF10`, `0x3FF14`, `0x3FFFC`×3). Recorded as an acceptance
  advisory on `A4a-r2`.
- `gp_ep_reads` (as defined in `A4a-r2`) counts only `[APUMMIO] read` lines, so a GP
  read-modify-write logged as a write would not register. It did not bite here. Tighten if
  the row is reused.
- Archive raw `ctest` output in run directories; the packet's Closure named it but only
  `CTestCostData.txt` survives.
- `P0.1-AC1` re-review; moving `RECOMP_AC97_READY` into the classifier's
  `RETIRED_OVERRIDES` registry; the unmodelled AC'97 registers `0xFEC0017C` / `0xFEC00100`.
- Owner-reserved, unchanged: the `0xFE820010` "GP sample counter" / `PIO_FREE` naming
  conflict (`docs/jsrf-run-profiles.md:288-292`).

**Measured blocker (unchanged):** the guest spins at `loc_001A18D0`
(`recomp_0005.c:6748-6751`, live frame in `sub_001A1769`) on a DSP pending word at
`+0x810` that nothing clears. The run ends `diagnostic_deadline`.
`RECOMP_APU_DSP_ACK` stays forbidden as acceptance evidence. **Caution:** A3a's predicted
next stop (`div@0x001A2BFC`) came from an exploratory run with `RECOMP_GPU_ACK` enabled,
whose synthetic completion cleared this word; it is not a prediction of strict behaviour.

**Baseline (2026-09-24).** The accepted A3a work is committed: toolkit `c97ce2c`
(`src/kernel/xbox_memory_layout.c`) and game `a16350f` (`scripts/jsrf_run_profile.py`,
`scripts/ac2-provenance.py`, `tests/test_ac2_provenance.py`). Both trees were clean
after the baseline commits; any later dirty file is new work.

## Last closed packet — `A3a-r25` (ACCEPTED 2026-09-24)

- **Packet:** `docs/packets/a3a-ac97-codec-model.md`, revision `A3a-r25`, SHA-256
  `6F907A42EAC6FD02392E11EADEF127AE840D97A245E771CD15B63A387F0E5BDC`.
- **Change:** the environment-gated `RECOMP_AC97_READY` override was replaced by an
  always-on model in the `nv2a_ack_thread` loop:
  `GS(0xFEC00130).bit8 := GC(0xFEC0012C).bit1`, level-evaluated each tick, atomic,
  outside the `g_apu_mmio_trapped` gate; `GC` is read only.
- **Result:** one **strict** run selected `R-PASS`; `AC1`–`AC6` all PASS. The codec
  poll succeeds, the vector-6 ISR is connected, and the guest proceeds into DSP
  initialisation, where it now spins (the blocker above). The strict baseline
  previously self-relaunched before reaching the poll.
- **Claim limits:** one strict run's codec-presence wait was satisfied by modelled
  device state. This does not establish faithful hardware behaviour (secondary
  sources only), audio, a complete DSP handshake, liveness, or strict boot.
- **Records:** adequacy `docs/reviews/a3a-r25-adequacy-review.md`; execution
  `docs/reviews/a3a-execution-evidence.md`; acceptance (`ACCEPT`, no disagreements)
  `docs/reviews/a3a-r25-acceptance-review.md`.

**Follow-ups carried forward (not authorized until promoted in a packet):**

- `P0.1-AC1` is reopened by A3a (see P0 below).
- Move `RECOMP_AC97_READY` into the classifier's `RETIRED_OVERRIDES` registry and
  update `tests/test_run_profiles.py:560`. Its `removed_commit` boundary now exists
  (toolkit `c97ce2c`); the `added_commit` must be found in toolkit history.
- `RECOMP_APU_TRAP` belongs with the DSP packet.
- The AC'97 registers A3a does not model (`0xFEC0017C`, `0xFEC00100`) wait for a
  later packet.

## Retired packet — `A2h-r6` (premise refuted, 2026-09-23)

`docs/packets/a2h-writer-investigation.md` was retired at adequacy review, never
executed. Record: `docs/reviews/a2h-r6-adequacy-review.md`. Its premise — an invalid
target `0x00700010` dispatched from `0x0017DC23` — was refuted: that failure came
through the static-initializer walker's `tail_jump_alias` entry (`0x0017E58F`) and
was already fixed by commit `cb7cae2`; every run that logged it predates the fix.
`sub_0017DBBD`'s execution status is simply uninstrumented (UNKNOWN). Do not revive
this area without new evidence. Two lessons stand for any future packet that must
observe **inside** a generated function: it first needs a trace seam outside the
provenance-guarded files and a trace mode that keeps the **last** N records; and the
collector's call history plus the frozen dump should be tried first, since they
distinguish call sites with no code change.

## P0 — ACCEPTED

**P0.S and P0.1–P0.7 are accepted.** They are completed prerequisites, not active
implementation instructions. Do not reopen them unless a later edit invalidates an
affected accepted criterion; then reopen only that criterion and re-review it.

Durable evidence:

- `docs/reviews/p0-1-execution.md`
- `docs/reviews/p0-1-vblank-adjudication.md`
- `docs/reviews/p0-2-acceptance.md`
- `docs/reviews/p0-3-to-p0-7-acceptance.md`

The original P0 contracts under `docs/packets/` are frozen provenance; their old
`proposed`, `pending`, `next packet` and `TOOLING REQUIRED` wording is not current
work authorization.

### Reopened: `P0.1-AC1` (by `A3a-r25`, 2026-09-24)

Re-review only `P0.1-AC1` against the new runtime.

1. **Cited sites moved.** Its accepted evidence names `xbox_memory_layout.c:684-686,1613`
   (`docs/reviews/p0-1-vblank-adjudication.md:265`,
   `docs/packets/p0-acceptance-contract.md:78`). A3a changed that file, so both ranges
   shifted. The re-review should cite **symbols**, not line numbers.
2. **`P0.1-AC3` is affected in principle.** `_valid_new_archive` compares recorded
   reasons verbatim (`scripts/jsrf_run_profile.py:585-586`) while `CLASSIFIER_VERSION`
   stays `jsrf-run-profile/1`, so editing the reason string at `:391` makes an archive
   recorded under the old string unreclassifiable.
3. **Measured impact today is zero.** Of 650 archived runs, exactly one carries a
   `run_profile`, and it holds no AC97 reason. The classifier still treats the retired
   name as exploratory, which can never produce a false `STRICT`.

## Evidence and decision rules

- `MEASURED` means inspected source/artifact evidence with an identity/procedure.
  `INFERRED` means a hypothesis or expected consequence.
- Missing, malformed, stale, unexercised or `CANNOT VERIFY` evidence is never PASS.
- Only a verified **strict** run can support strict integration/boot/liveness/device
  claims. Exploratory/fixture evidence remains bounded to what it actually measured.
- A reviewer verifies or refutes each mandatory criterion.
- A disagreement goes to the Persistent advisor, whose ruling is final within the
  evidence invariants of `docs/agent-workflow.md` §2.4.
- A failed measurement cannot be converted to PASS by Planner, Session, reviewer or
  advisor interpretation.
- A post-review code/evidence change reopens the affected criterion.
- Original assets and existing saves remain unchanged.

## Roadmap policy

Older A1–A5 sequences, GPU/audio milestone diaries, historical `next packet`
statements, old model assignments and legacy status tables live in
`docs/jsrf-operating-history.md`. They are provenance only.

New executable work follows the packet lifecycle in `docs/agent-workflow.md` §5: the
Planner designs the packet, an adequacy review of the exact revision returns
`ADEQUATE`, and that frozen revision is promoted into **CURRENT PACKET** in the same
step. Historical evidence may motivate a packet, but a historical failure does not
imply the same failure exists in the target revision.

If no packet is explicitly promoted in this file, implementation is **BLOCKED**.
