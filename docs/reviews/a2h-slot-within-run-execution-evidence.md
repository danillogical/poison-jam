# `A2h-slot-within-run-attribution-r1` — execution evidence: **`O-COVERAGE`**, with a sharpened writer bound

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-slot-within-run-attribution-r1`, frozen
**`3865FACC776BC64C6B0CFE8DF6C6BD0287DBD6EFC339C4E7FFAA7406667BEC27`** (33 lines).
**Runs:** **five** pre-specified ON launches, same build, `--profile strict --seconds 8`,
`JSRF_TRACE_A2H_DR=1` + `JSRF_TRACE_A2H_SLOT=1`, non-gate env identical
(`RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, `RECOMP_KERNEL_LOG_BUDGET=100000`).

| # | Run directory |
|---|---|
| 1 | `logs/runs/20260928-020421-240-a2h-within-run-on-1` |
| 2 | `logs/runs/20260928-020448-975-a2h-within-run-on-2` |
| 3 | `logs/runs/20260928-020456-595-a2h-within-run-on-3` |
| 4 | `logs/runs/20260928-020504-433-a2h-within-run-on-4` |
| 5 | `logs/runs/20260928-020512-239-a2h-within-run-on-5` |

**All five share the same `jsrf_recomp.exe` (`a7e324642a41…`), the same OOM, and the same identity-1
prefix — so the OFF control carries and every realization passes the deterministic anchors.**

---

## Classification — by terminal-event set, per the packet

| # | Terminal set | Anchors | Install trap | Class |
|---|---|---|---|---|
| 1 | **`0x00000000@0014982E`** | PASS | PASS | **TARGET** |
| 2 | **`0x00000000@0014982E`** | PASS | PASS | **TARGET** |
| 3 | *(none observed)* | PASS | PASS | **NON-TARGET** |
| 4 | **`0x00000000@0014982E`** | PASS | PASS | **TARGET** |
| 5 | `0x00000001@0018CE73` | PASS | PASS | **NON-TARGET** |

**Observed targets: 3 of 5. Complete-coverage TARGET realizations: K = 3** — **exceeds the packet's
K ≥ 2 requirement.** Coverage-disqualified targets: **0**.

**Every run's deterministic anchors are identical:** OOM `598869040` / `12715008/50855936`, status
`0xC0000017`, **identity-1 dispatch prefix 5555**. **Terminal events vary** (as the packet anticipates), and
**no target realization carried a competing terminal** — the packet's strict multi-terminal rule discarded
**zero** target data, exactly as the Session's pre-run measurement predicted (0 of 26 archived target runs).

**The install positive control PASSED in all five runs**, read from the frozen latch:
`install_seen=1 install_raw=80000115 install_value=FE000104 install_ok=1`, index 65, plus
`[A2HSLOT] install … raw=80000115 installed=FE000104`.

---

## The row: **`O-COVERAGE`** → `A2h-slot-write-coverage-provenance`

**The all-thread-arming leg FAILS, and the packet makes that fail closed.** Every run reports (these lines
live in **`stacks.txt`, not `jsrf_run.log`** — there are **zero** `GUEST_DR` lines in any of the five logs,
stated per acceptance finding C4):

```
GUEST_DR_ARM why=handshake ok=0 armed=10 failed=9 collision=0 canonical=00000000001D4064 aliases=28
GUEST_DR_ARM_FAIL tid=… why=create_thread reason=no_mapping_offset      (×9)
GUEST_DR_HIT …                                                          (ZERO, all five runs)
```

**Nine of ten threads failed to arm**, so the canonical DR0 write watch **did not cover the process**. Per
the packet's predecessor line 45 and the Advisor's correction 3(i) — *"any unarmed observed tid → `UNKNOWN`,
fail closed"* — **no absence or attribution claim is available**, and the target realizations select
**`O-COVERAGE`**.

**This is the Advisor's flagged uncertainty #1 and #3 arriving exactly as predicted.** The ruling and the
implementation record both said the real DR0 arm path was **unverified against the live game**, and that a
missing trap must select `O-COVERAGE` and **never** be read as "no write occurred." **That is what happened,
and the instrument reported it honestly rather than silently under-covering** — `ok=0` is printed, the nine
failures are individually named with a reason, and the tid list is archived.

---

## What the run nevertheless establishes — a SHARPENED writer bound

**These are positive measurements, and they narrow the writer substantially even though the row is
`O-COVERAGE`.**

### 1. NO WRITE CAME THROUGH ANY OF THE 28 MIRRORS — the census is complete and found zero touches

Every run: `[A2HSLOT] alias census armed=1 mapped=28 protected=28 mask=0FFFFFFF/0FFFFFFF`, and the frozen
latch records **`touched_count = 0`, `publish_failed = 0`** in **all five runs**.

**28 of 28 mirrors were mapped and protected** (the mask `0x0FFFFFFF` is 28 bits), so the census covered
**every** alias page, and **not one was touched.** By the corrected ruling's first-touch argument — a page's
first touch necessarily precedes its first single-step window — **a mirror write could not have been
missed.** **So the zero did NOT arrive through an alias.**

### 2. THE FAULTING THREAD WAS ARMED IN THREE OF FIVE RUNS — and produced no DR hit

| # | Faulting tid | Was it armed? | DR hits |
|---|---|---|---|
| 1 | 50616 | **ARMED** | **0** |
| 2 | 56204 | **ARMED** | **0** |
| 3 | *(none)* | — | 0 |
| 4 | 67512 | **ARMED** | **0** |
| 5 | 66468 | **NOT ATTEMPTED** | 0 |

**In runs 1, 2 and 4 the thread that faulted was the one thread that armed successfully — and it recorded
zero canonical-slot writes.** So the faulting thread did not write the slot through the canonical address.

### 3. **Run 5 POSITIVELY WITNESSED the slot becoming zero**

**This is the strongest single observation in the A2h line so far, and it is new.** Run 5's log:

```
[A2HSLOT] tid=66468 call=#245 ordinal=159 slot=001C4064 value=FE000104 phase=after
[A2HSLOT] tid=66468 call=#246 ordinal=224 slot=001C4064 value=00000000 phase=before
[A2HSLOT] tid=66468 call=#246 ordinal=224 slot=001C4064 value=00000000 phase=after
[A2HSLOT] write tid=66468 slot=001C4064 before=FE000104 after=00000000 phase=boundary
[ICALL] invalid target 0x00000001 tid=66468 esp=0126BF88 return=0018CE73
[A2HSLOT] terminal tid=66468 target=00000001 slot=001C4064 live=00000000 call=#246 observed=0
```

*(**Corrected per the post-review confirmation's D2 finding.** This block first attributed the
`FE000104 phase=after` sample to call `#246`; **it is call `#245`'s**, and no `#246` line with that value
exists. The sequence is now transcribed exactly as the log carries it, which is also what makes the bracket
below legible.)*

**Within one bridge call** (ordinal 224, call `#246`) the slot went **`FE000104 → 00000000`**, and the very
next event was a fatal call through a **different** slot with a **non-NULL** target.

**The emitter is honest about what it can and cannot say** (`kernel_bridge.c:9021-9055`): it runs in host
bridge code, the write happened **somewhere between the previous boundary and this one**, and **a native RIP
would be needed to name the writer** — so it records **`JSRF_PROV_UNKNOWN`** and **does not guess**. **That is
the packet's rule applied correctly.**

**Is it real, or a per-thread sampling artifact?** The sampler compares the **live** slot against its own
last read and reports **only on inequality**, so it **cannot** report a transition when the value is
unchanged — **the memory genuinely changed.** *(The Session could not cross-check with post-boundary samples
from other threads: the run terminates immediately after, so there are none. Recorded as a limit, not
papered over.)*

**The acceptance reviewer SHARPENED the bracket, and it is tighter than this record first stated.** The two
`value=00000000` samples are **not** the record's endpoints: call `#246`'s **`phase=before` already read
`00000000`**, so the record's `before=FE000104` is the sampler's **previous** read — call `#245`'s
`phase=after`. **The write is therefore bracketed strictly BETWEEN `#245`'s after-sample and `#246`'s
before-sample — i.e. OUTSIDE any bridge body for `#246`**, not merely "within one bridge call" as this record
originally said. **That is a materially tighter constraint on where the write happened, and the reviewer's
reading is the correct one.**

**The reviewer also independently corroborated the witness from the frozen registry**, which this record had
not done: run 5's `classes[4]` reads `valid=1 class_id=4 tid=66468 call_index=246 ordinal=224
before=0xFE000104 after=0x00000000 count=1 class_witness_count=1`, while **runs 1–4 record 0**. **So the
witness survives in the archived decision record, not only in a log line** — which is the §6.1.6 standard
this line has been held to throughout.

### 4. The deduction the three measurements support together

**No alias touch** (census complete, 28/28) **+ no canonical write by the armed faulting thread** (DR0 armed,
zero hits) **+ the slot demonstrably reached zero** ⇒ **the zero was written through the CANONICAL address by
one of the threads the instrument did NOT arm — the nine failed attempts AND the never-attempted guest
threads** (corrected per acceptance finding C2; see below).

**That is a bounded, evidence-backed narrowing — and it is precisely the region the coverage gap leaves
open.** It is **not** an attribution: **no writer is named, no site is named, and the row stays
`O-COVERAGE`.** But it converts "somebody zeroed it" into **"a canonical-address write from a thread the
instrument failed to arm"**, which is a materially stronger statement than the predecessor row could make.

**Recorded as a finding, explicitly NOT as a row.** The packet's rule is that an unarmed thread prevents
absence claims; **this finding is consistent with that rule rather than an exception to it.**

---

## The coverage defect, precisely characterised for the successor

> **⚠ SUPERSEDED — read the correction at line 182 first.** This section's *counts* are right, but its
> conclusion that guest threads were **"never attempted"** is **withdrawn**. `GUEST_DR_DISARM cleared=17`
> proves **17** threads were armed (not 10), all nine failed attempts were **recovered** by the handshake
> sweep, and a **successful post-handshake arm prints nothing** — so the tid list is a **handshake-time
> snapshot** and cannot show which threads were armed. **The defect is a REPORTING gap, not a proven coverage
> gap.**

**Root cause:** `GUEST_DR_ARM_FAIL … why=create_thread reason=no_mapping_offset`. The collector arms at
`CREATE_THREAD` **before `ContinueDebugEvent`**, which is the correct **ordering** — but at that moment
`g_xbox_mem_offset` is **not yet resolvable**, so **every thread created before the mapping is established
fails to arm.** Only the thread that armed **via the install handshake** (after the mapping existed) succeeded.

**So the defect is a TIMING dependency, not an ordering error:** the `CREATE_THREAD` arm path needs the
mapping offset, and for early threads it does not yet exist. **The handshake path works; the birth path does
not.** The successor should arm at `CREATE_THREAD` **when possible** and **re-arm at the handshake** for any
thread that could not be armed at birth — and must **fail closed** if any observed tid remains unarmed, which
the instrument already does.

**Nothing here is a reason to distrust the census**, which is page-protection based, process-wide, and
**independent of per-thread DR arming** — which is exactly why it delivered a complete, zero-touch result
while the DR watch covered one thread.

---

# ⚠ SESSION ERROR CORRECTED — the "never attempted" claim was WRONG; the defect is a REPORTING gap

**I claimed "at least five of at least six guest-dispatching threads were never attempted at all," and
sections below were written on that basis. The claim OUTRAN THE EVIDENCE and is withdrawn.** I found the
error by reading the collector's own source rather than defending the claim.

## What the source actually does

| Fact | Source |
|---|---|
| `dr_arm_all()` **is a full sweep** — it walks `CreateToolhelp32Snapshot`/`Thread32Next` over every thread in the target and arms each | `collect.c:157-181` |
| The `GUEST_DR_ARM …` + `GUEST_DR_ARM_TID` summary is printed by `dr_print_arm_summary()`, called **at the handshake** | `collect.c:217` |
| In the `CREATE_THREAD` branch, the summary prints **only on failure** | `collect.c:713` |
| **A successful arm after the handshake prints NOTHING** | consequence of the two above |

**So the tid list in `stacks.txt` is a HANDSHAKE-TIME SNAPSHOT. Threads armed later are invisible in it.**

## What the artifacts prove

| Evidence | Value, **identical in all five runs** |
|---|---|
| Summary at handshake | `armed=10 failed=9` |
| Tids **listed** | 10 |
| **`GUEST_DR_DISARM ok=1 cleared=17 …`** | **17** |
| ⇒ armed **after** the summary, unreported | **7** |

**`cleared=17` is the decisive number:** seventeen threads had DR7 cleared at teardown, so **seventeen were
armed** — not ten. **And all nine `failed=9` attempts were subsequently RECOVERED:** every one of the nine
failed tids also appears in the armed list.

## The corrected conclusion

**`failed=9` counts failed ATTEMPTS, not unarmed threads.** The handshake sweep recovered all nine. **The
four guest threads absent from the tid list are NOT PROVEN UNARMED** — they were created **after** the
snapshot, and a successful post-handshake arm **prints nothing**. **The record cannot distinguish:**

- **(i) armed successfully, never reported** — from
- **(ii) never attempted at all.**

**So the defect is a REPORTING gap, not a proven coverage gap.** My "100% miss over the guest threads that
matter" framing was **wrong**, and the sections below that assert it are **superseded by this correction**.

## Why the row is nevertheless unchanged — `O-COVERAGE`

**The row does not depend on the withdrawn claim.** The packet's rule is that **any unarmed observed tid
yields `UNKNOWN`, and a coverage claim requires positive continuous coverage.** **A record that cannot show
which threads were armed cannot establish coverage** — and that is a **sufficient** basis for `O-COVERAGE`
on its own. **What changes is the REASON: the instrument's *record* is inadequate, not (as I claimed) its
*coverage*.**

**This makes the Advisor's correction 2 exactly right, and for a sharper reason than I gave.** I asked
whether the never-attempted population needed a positive control. **It does — because without one I
mistook a reporting gap for a coverage gap**, which is precisely the absent-record-as-negative error this
line keeps meeting. **I committed it myself while documenting it.**

**The corrected deduction, stated at its true strength:** the census result (28/28 mirrors, zero touches) and
the run-5 witness (`FE000104 → 00000000`, outside any bridge body) **stand unchanged**. **What cannot be
stated is that the DR0 watch missed the writer** — only that **the archive does not show whether it did.**

---

# Superseded — the following sections were written on the withdrawn claim

*Retained rather than deleted, per this line's rule that corrections name what they supersede. **Read them
with the correction above applied: the counts are right, the "never attempted" conclusion is not.***



**The Session dug one level further, and the failure is not a retry problem. The collector armed the WRONG
THREADS ENTIRELY.**

**Measured in run 1:**

| tid | guest identity | attempted? | dispatched? | state |
|---|---|---|---|---|
| 50616 | **1** | yes | **yes** | **ARMED** |
| 58620 | **3** | **NO** | **yes** | **NEVER ATTEMPTED** |
| 60872 | **4** | **NO** | **yes** | **NEVER ATTEMPTED** |
| 66492 | **2** | **NO** | **yes** | **NEVER ATTEMPTED** |
| 67988 | **5** | **NO** | **yes** | **NEVER ATTEMPTED** |
| 10904, 30120, 32108, 38516, 45168, 46648, 53916, 60208, 65644 | — | yes | **no** | arm FAILED |

**Every one of the nine "attempts" was a thread that NEVER DISPATCHED a single guest kernel call** — transient
or toolkit host threads. **And all FOUR of the other real guest threads recorded in the registry (identities
2, 3, 4, 5) were never attempted at all**, yet **each dispatched guest code.**

**So the coverage hole is total for guest threads: the DR0 watch covered exactly ONE of the guest threads that
ran guest code — out of AT LEAST SIX.** *(This figure was first written here as "one of the five" and "4 of 5
never attempted", taking the frozen registry's five threads as the population. **The post-review confirmation
established that is wrong:** a **sixth** guest-dispatching thread, `tid 57376`, also ran guest code and was
never attempted — see the C3 correction below. **The superseded figure is corrected here rather than only in
the appended corrections, because a reader who stops before those would otherwise carry the wrong number.**)*
**This is not "9 of 10 arms failed" — it is "at least 5 of at least 6 guest threads were never even
attempted."**

**Run 5 shows the same shape:** the one armed thread (`66428`, **guest identity 1** — corrected, see below)
plus **four guest threads never attempted** (`59716`, `62152`, `63812`, `66468`), and **`66468` — guest
identity 4 — is the thread that witnessed the transition and then faulted.**

**Why the earlier characterisation was incomplete, and why that matters:** `GUEST_DR_ARM … armed=10 failed=9`
reads as a 90% failure rate over a ten-thread population. **It is actually a 100% miss over the guest-thread
population that matters**, plus nine wasted attempts on threads that never ran guest code. **The summary
counts were true and the conclusion they invited was wrong** — the same absent-record-read-as-negative shape
this project keeps meeting, and worth recording as such.

**The fix shape is therefore a SWEEP, not a retry:** at the install handshake — the one point where the
mapping offset is known to exist — the collector must **enumerate every live thread in the target and arm
each**, rather than arming only the handshaking thread. **The fail-closed half is already correct** (`ok=0`,
named failures, archived tid list); **only the enumeration is missing.**

---

# Corrections applied after stage-1 acceptance — **`ACCEPT-WITH-CORRECTIONS`, `BLOCKING: NONE`**

**Reviewer:** `a25dafb3-52c9-4d2d-86e1-cfe4dd8f1d1f`, review
`docs/reviews/a2h-slot-within-run-acceptance-review.md`. **No acceptance criterion was falsified**; all four
findings were **defects in this record's prose**, not in the execution. **The Session verified each and
applied all four.**

**C1 — a factual mislabel, corrected.** This record said run 5's armed thread was *"`66428`, guest identity
4."* **Verified: `66428` is guest identity 1** (start `0x00148023`, the main thread) and **`66468` is
identity 4.** The argument is unaffected — *the armed thread is not the witnessing thread* — **but the label
was wrong and is now corrected above.**

**C2 — an imprecise writer set, corrected.** §4 said the zero came from *"one of the nine unarmed threads."*
**The nine are the nine failed `create_thread` attempts**, whereas the witnessing thread `66468` **was never
attempted at all** (it is absent from `GUEST_DR_ARM_TID`). **The correct phrasing is "one of the threads the
instrument did not arm — the nine failed attempts AND the never-attempted guest threads."** **This is the
more important correction of the two**, because it is precisely the distinction the section above establishes.

**C3 — the guest-thread census was incomplete, and the gap is BIGGER than recorded.** This record counted
*"4 of 5 guest threads never attempted"* for run 1, taking the frozen registry's five threads as the
population. **Verified: there is a SIXTH guest-dispatching thread, `tid 57376`**, which reaches
`[KERNEL] #286`, appears **1149 times** in `stacks.txt`, and was **never armed**. **So the true statement is
at least 5 of at least 6 guest-dispatching threads never attempted** — **which strengthens this record's
point rather than weakening it.**

**And `57376` carries something this record had not examined: 1148 first-chance `0xC0000005` access
violations.** **Disclosed as unexamined contrastive data.** It does not change the row — `O-COVERAGE` either
way — but **a thread raising ~1148 access violations is a fact a successor should not have to rediscover**,
and this record should have surfaced it.

**C4 — provenance of the quoted DR lines, now stated.** The `GUEST_DR_ARM` / `GUEST_DR_ARM_FAIL` /
`GUEST_DR_HIT` lines live in **`stacks.txt`, not `jsrf_run.log`** — there are **zero** `GUEST_DR` lines in any
of the five logs. **Stated so a reader checking the log alone does not wrongly conclude the claim is
unsupported.**

## What the reviewer could NOT verify — recorded, not glossed

- **The read-path leg:** it could not exclude that `BRIDGE_MEM32(0x001C4064)` and the guest's own read resolve
  to different host storage. **The row is `O-COVERAGE` regardless**, so this does not move the verdict.
- **The OFF-control identity claim:** it verified the five ON runs share one exe SHA but **did not compare it
  against the archived Run-1 OFF exe SHA.** **The Session did make that comparison** before the runs
  (`A7E324642A41DF3F…ACDEAD9`, byte-identical, recorded in
  `docs/reviews/a2h-slot-within-run-r1-session-validation.md`), **so the claim holds — but the reviewer is
  right that its own review did not independently establish it, and that is stated here rather than papered
  over.**
- **`PIO_FREE` / `A4b2-*` / `0xFFFFB3` status** was taken from this record's assertion rather than re-derived.
- **No `check-dump-mapping.py` gate was run**; no claim here rests on XBE-backed dump content beyond the
  registry extraction, which the reviewer **did** re-derive.

---

## Prohibitions and status

**No synthetic completion.** No guest semantics changed; the APU trap and `0x80` untouched; no allocation
faked; arena not widened; NULL call not bypassed; guest error handling not edited. **No `src/recomp/gen/*.c`
edit.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**. The toolkit diagnostic at
`5528d00` remains **unpushed** (pending acceptance). **No sixth run** — `N = 5` was pre-specified and
exhausted; extension requires Advisor re-referral.

**K = 3 ≥ 2, so the packet's repetition requirement is MET** — but **all three targets select `O-COVERAGE`**
on the arming leg, so **no attribution row is selected and generality is not the binding limit here; the
coverage leg is.**
