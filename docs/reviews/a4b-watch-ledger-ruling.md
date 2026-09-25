# A4b watch-ledger ruling (§5.5, second trigger)

**Advisor:** child `407c54a3-6ca4-4a65-835b-faf5355195cd`, route `claude` / `claude-opus-5-5` @ `high`.
**Trigger:** §5.5 — two consecutive blocking verdicts in one mechanism: `A4b-r3` B2 (the `[GPDMA] watch` cap could lose the deciding `payload=0` line) and `A4b1` B3 / `A4b2` B3 (the exemption can be consumed by a pre-command GP write-back, so the real 3→0 write is never logged).

## Ruling (verbatim)

> RULING, in brief: Your reading is correct. The defect class is one artifact doing two jobs: a capped, human-readable log line was both the observation record and the decision input. Separate them. The decision comes from a write-once event ledger kept by the write paths themselves, lossless by construction. The watch lines become observation only, and no row may depend on whether a capped line is present or absent. The ledger and its fixture live in A4b1, the toolkit. A4b2 only calls its CPU-side entry point and reads its output. No third patch of the cap/exemption shape.
>
> (a) What is recorded, and where: a toolkit "watched-word ledger" (A4b1).
> 1. State. A small device-side struct, not a log:
>    - an atomic 32-bit sequence counter `seq`, incremented with InterlockedIncrement for every ledger event;
>    - uncapped counters for each event class;
>    - one write-once latch for the first event of each class. Set it with InterlockedCompareExchange on a "latched" word; the latch stores seq, va, before/observed, payload, frame, insns and dsp_addr.
>    It is computed always (a few atomics, no guest-visible effect). Its lines are printed under RECOMP_APU_TRACE.
>    It must be thread-safe. The GP runs on the APU frame thread (`apu_core.c:557`, qemu_thread_create "mcpx.apu_thread"), and guest CPU stores come from guest threads.
> 2. GP-side classes, recorded on the GP DMA write path at the same point as the Device semantics 5 compare-exchange. W_va = MEM32(0x001BA858)+0x810, read at write time and recorded in every latch.
>    - GP_CLEAR: zero payload, CAS(host(W_va), 0, 3) succeeded, observed=3. This is the attribution witness.
>    - GP_ZERO_OVER_ZERO: zero payload, CAS failed, observed=0. This is the pre-command write-back case (the B3 scenario). Counted and latched; it never decides.
>    - GP_ZERO_OVER_OTHER: zero payload, CAS failed, observed not in {0,3}.
>    - GP_NONZERO_OVER: a non-zero payload covering W_va. The latch records before/payload.
>    - GP_PARTIAL: a GP DMA write that overlaps W_va's 4 bytes without covering the aligned dword. It is not CAS-protected, so it is an unattributable writer.
> 3. CPU-side classes, through one exported toolkit function `apu_watch_cpu_store(uint32_t site_va, uint32_t target_va, uint32_t value)`. It checks `target_va == MEM32(0x001BA858)+0x810` itself, and the game calls it before each instrumented store.
>    - CPU_ANCHOR: the first store of 3 from the control site (recomp_0005.c:6746). A4b2 passes the control-site VA; A4b1 does not hard-code it.
>    - CPU_ZERO: the first store of 0, latched per site. Use a bounded site table (say 16 entries) plus one CPU_ZERO_OVERFLOW latch. Overflow is itself a recorded event, never silent.
>    - CPU_OTHER: counters only.
>
>    **Premise (added 2026-09-24):** the bounded site table is safe **only because** the site universe is finite, enumerated in source, and independent of run length — the instrumented `jsrf_watch_store` call sites. Applying this same "bounded table + overflow latch" pattern to a key whose universe is *not* so bounded produced a third `INADEQUATE` verdict; see `docs/reviews/a4b-gpin-accounting-ruling.md` and `docs/agent-workflow.md` §6.1.6b.
> 4. Emission, under RECOMP_APU_TRACE:
>    - exactly one `[GPWATCH] latch class=<C> seq=%u va=%08X observed=%08X payload=%08X site=%08X frame=%u insns=%llu dsp_addr=%06X` at the moment each latch fires, fflush'd. The number of classes is fixed, so this needs no cap and cannot lose a line;
>    - `[GPWATCH] counts seq=%u <class>=%u …` every 256th frame, and again immediately after GP_CLEAR latches.
>    The existing `[GPDMA] watch` lines and their cap stay as observation only. Delete the r3/A4b1 "exempt line" and "cap reached" rules: nothing depends on them any more.
> 5. Test accessor: `apu_watch_snapshot(struct *)` and `apu_watch_reset()`, used only by the fixture, which then checks the struct and not log text. This also removes A4b1 B1's fixture-order and cumulative-count problem.
>
> Why this cannot lose the witness:
> - The GP_CLEAR latch fires only on a successful 3→0 CAS, so no earlier zero-over-zero write-back, cap or ordering can consume it.
> - Its line is emitted exactly once, when the latch fires.
> - The CAS is the ordering authority. A CPU record is written before its store, so CPU seq order is only approximate. But if GP_CLEAR's CAS saw 3, no CPU 0 had landed by then, whatever the seq order says.
>
> (b) What A4b2 decides on. Let B = MEM32(0x001BA858) at freeze and Wf = the watched word at freeze (the existing W).
> AC-CLEAR:
> - PASS requires all of:
>   - a GP_CLEAR latch with va = B+0x810, observed=3, insns > 0 and dsp_addr recorded;
>   - a CPU_ANCHOR latch with seq < GP_CLEAR.seq. The anchor is corroboration of which 3 was exchanged; CAS observed=3 already proves the word held 3.
> - FAIL → R2-CPU, only with a positive witness: no GP_CLEAR; Wf = 0 (or the spin was exited, F = 0); and a CPU_ZERO latch at a non-control site with seq > CPU_ANCHOR.seq.
> - FAIL → R2-NOCLEAR: no GP_CLEAR, a CPU_ANCHOR is present, and Wf = 3.
> - R2-UNATTRIBUTED, a new row: not PASS, not blamed on the CPU; goes to the Advisor. Selected when:
>   - there is no GP_CLEAR, Wf = 0, and there is no CPU_ZERO after the anchor; or
>   - GP_PARTIAL or GP_ZERO_OVER_OTHER latched while Wf = 0 and there is no GP_CLEAR.
>   This replaces my r3 (d) rule "no deciding line + W=0 + path alive → R-CPU". Absence of a witness is never a positive attribution.
> - UNKNOWN when:
>   - no CPU_ANCHOR latch although Wf ∈ {0,3} and the run reached the spin (instrumentation broken);
>   - GP_CLEAR.seq < CPU_ANCHOR.seq (the exchanged 3 came from an uninstrumented store);
>   - CPU_ZERO_OVERFLOW latched;
>   - there is no `[GPWATCH] counts` line at all (the ledger is not alive).
> AC-NOCPU:
> - Keep "no synthetic ack; zero AC-PORT search hits".
> - Replace "no other instrumented site stored 0 before the first cas=ok (log order)" with a ledger rule: a non-control CPU_ZERO latched with seq < GP_CLEAR.seq is "contested". AC-NOCPU is then UNKNOWN, not FAIL (the one-rerun rule applies). Record-before-store makes that order indeterminate, so neither PASS nor a CPU FAIL can be claimed.
> - A CPU_ZERO after GP_CLEAR is recorded only (for example, the stop path clearing an already-zero word).
> - Claim limit, unchanged: the CPU instrumentation is a text-matched lead, not a complete enumeration. Attribution rests on the CAS alone.
> The GP_ZERO_OVER_ZERO count is reported and decides nothing. A large count is the expected pre-command write-back and is exactly what the old design tripped on.
> [GPIN] cut-off: "until the first zero-payload watch line" has the same defect. Change it to "until GP_CLEAR latches, or the run ends".
>
> (c) Where it lives.
> - The ledger, the CAS, the exported `apu_watch_cpu_store`, the accessor, and all class and decision-relevant semantics go in A4b1, the toolkit write scope, with an AC-FIX extension checked through apu_watch_snapshot on fresh apu_watch_reset state. Required fixture cases:
>   - (i) the B3 scenario as the known-bad for the old design: 20+ zero-payload write-backs over 0, then an anchor store of 3, then a GP zero write gives GP_CLEAR latched, GP_ZERO_OVER_ZERO = 20+, and exactly one GP_CLEAR line;
>   - (ii) a zero write over 7 gives GP_ZERO_OVER_OTHER;
>   - (iii) a partial overlap gives GP_PARTIAL;
>   - (iv) a CPU zero store before a GP write gives CPU_ZERO latched, and the GP write gives cas fail observed=0 with no GP_CLEAR;
>   - (v) CPU-site table overflow gives CPU_ZERO_OVERFLOW.
> - A4b2 edits the game only:
>   - `jsrf_watch_store` becomes a thin forwarder to `apu_watch_cpu_store`, or the edits call the toolkit function directly. Either way there is no game-side decision logic and no game-side cap.
>   - The old `[A4BSTORE]` capped lines may stay as observation.
>   - A4b2 reads the `[GPWATCH]` latch and counts lines and the dump.
> - The seam rule: A4b2 must not depend on any toolkit behaviour that A4b1's fixture does not exercise. Every A4b2 decision input must be a ledger class that A4b1 AC-FIX covers.
> - Do not merge the packets. The leak was a decision rule placed in a packet that cannot change the code producing its input, not the split itself.
>
> (d) General rule to record: yes.
> "Decision inputs are lossless by construction. A criterion may select a row only from a record that cannot drop the deciding event: a write-once latch or an uncapped counter, updated at the event by the code that performs it. Capped, sampled, rate-limited or first-N logs are observation only, and no row may depend on the presence or absence of such a line. Absence of a witness is never a positive attribution: it selects UNKNOWN or an explicit unattributed row, never a row that blames a specific agent. The packet that owns the code producing a decision input also owns and fixture-tests that input's semantics, and a consuming packet only reads it."
>
> BASIS:
> - Observed (read myself this turn): the a4b1-a4b2 adequacy review blocks (B1–B4); A4b1 Device semantics 5–6 (lines 51–60); A4b2 steps 1 and 74, the log definitions (lines 111–118) and AC-CLEAR/AC-NOCPU (lines 164–205).
> - Observed: A4a R1 jsrf_run.log L3190 GPRST=1, L3201 GPRST=3, L3286 `[APU] started by the title`.
> - Observed: toolkit apu_core.c 435–470 (se_frame runs on the APU frame thread's loop) and 557 (qemu_thread_create "mcpx.apu_thread"), so the GP and guest writers are on different threads.
> - Not verified by me: the reviewer's "sub_001A1769 first traces at L5226" (my grep found no 1A1769 line in that log). It is not load-bearing. The design is correct whatever the order of pre-command write-backs and the guest's store of 3, which is the point.
> - Inferred: record-before-store makes CPU seq order approximate, while the CAS is exact. Follows from the store order in the A4b2 step-1 edits (call before the store).
> - Uncertain: whether the ported GP ever makes partial or byte-granular DMA writes over +0x810. GP_PARTIAL exists to make that visible rather than assume it away.
>
> REVERSED BY:
> - A GP DMA path that can write W_va without passing through the recording point (for example a second write path in the pinned core). The ledger would then not be the complete GP writer, and the design needs one choke point first.
> - Evidence that the guest's pending value is not always 3 for this command. CAS(…,0,3) would then miss real clears (they show as GP_ZERO_OVER_OTHER), and the expected value would need to become "the value the anchor stored".
> - An owner decision to accept the old capped-log attribution. It is a fidelity/evidence tradeoff under §3.4, but I do not recommend it.
>
> RECORD IN:
> - This ruling, verbatim with this child's ID and route: new docs/reviews/a4b-watch-ledger-ruling.md, cross-referenced from a4b1-a4b2-adequacy-review.md "Next", and from a4b-pio-methodology-ruling.md §(d) marked "superseded by a4b-watch-ledger-ruling.md".
> - A4b1 revision: Device semantics 5 kept; 6 split into a new "6 ledger (decision)" and "7 trace (observation)"; AC-FIX gains cases (i)–(v); write scope adds the exported header entry.
> - A4b2 revision: AC-CLEAR, AC-NOCPU and the log definitions per (b); new R2-UNATTRIBUTED row; the [GPIN] cut-off changed.
> - Fold the cheap fixes (A4b1 B1/B2/B4, A4b2 B1/B2) into the same revisions. B1 is largely dissolved by the snapshot/reset accessor.
> - The general rule (d): docs/agent-workflow.md §6.1 drafting checklist, plus one line in docs/jsrf-run-profiles.md's evidence rule. This is a policy edit, separate from the packets.

## Session cross-reference

The r3 (d) rule is **superseded** by this ruling: see the note appended to
`docs/reviews/a4b-pio-methodology-ruling.md`. The next step is a Planner revision of **both**
`A4b1` and `A4b2` together (the reviewer found the split leaks at exactly this seam), folding
in the cheap fixes: `A4b1` B1/B2/B4 and `A4b2` B1/B2.
