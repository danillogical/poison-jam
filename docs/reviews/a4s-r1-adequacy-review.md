# A4s-r1 adequacy review — INADEQUATE

**Packet:** `docs/packets/a4s-toolkit-sync.md`, `A4s-r1`, SHA-256
`05CFD5A42715B31E3DD6FB4CAE99ABB289F7E24C8EB17E5198F587C5CF386E3A`
**Reviewer:** Planner child `3295835b-70df-4ffa-81cc-ea21f039013a`, route
`claude` / `claude-opus-5-5` @ `high` — fresh, wrote nothing (§5.1.5).

## Verdict block (verbatim, abridged to the load-bearing parts)

```text
REVISION:          A4s-r1, SHA-256 05CFD5A42715B31E3DD6FB4CAE99ABB289F7E24C8EB17E5198F587C5CF386E3A (matches)
PREMISE_FRESHNESS: BOUNDED — all git/remote facts re-observed identical to the packet; provenance checker ok today;
                   the reference stop is A4a-r2 R0 on 0d7929c, not re-observed, but stale premises route to
                   R-PRE/R-INVALID/R-UNKNOWN — except the defect below.
BLOCKING:
 (1) Decision rows R-MOVED vs the AC-RUN W definition. AC-RUN reads W = MEM32(0x803C0810) at a FIXED VA, but A4a
     defined W = MEM32(B+0x810); the packet's "A4a's predicate, unchanged" is false. R-MOVED's second branch
     (diagnostic_deadline, W readable, W≠3) does not require B = 0x803C0000, and the B≠0x803C0000 clause sits only
     in R-UNKNOWN, which is evaluated AFTER R-MOVED.
     Scenario: upstream adds a pinned-physical path to MmAllocateContiguousMemoryEx and +556 lines in
     xbox_memory_layout.c (observed), so any change in prior allocation order/size shifts the bump allocator and B
     becomes e.g. 0x803C1000. The guest still spins at loc_001A18D0 (F=2) with the pending word 3 at B+0x810, but
     MEM32(0x803C0810) reads unrelated memory ≠3 → first match R-MOVED → a false "the strict stop moved", a wrong
     next brief, and A4b1/A4b2 stay parked although the stop is unchanged. The same false R-MOVED arises whenever
     W≠3 but nonzero with F≥1.
     Required: the W≠3 branch of R-MOVED requires B readable and = 0x803C0000 (or define W = MEM32(B+0x810) with
     B≠0, as A4a did); W≠3 with a live spin frame (F≥1) routes to R-UNKNOWN, not R-MOVED; B≠0x803C0000 must reach
     R-UNKNOWN before any W-based R-MOVED. outcome≠diagnostic_deadline may stay a positive R-MOVED.
DEFERRED:
 (a) Likely outcome is R-CONFLICT: a read-only merge-tree shows conflict hunks in kernel_bridge.c (7), lifter.py (3),
     xbox_memory_layout.c, translator.py and test_icall_feedback.py (1 each). kernel_bridge.c hunks 1 (KeSetEvent)
     and 2 (KeWaitForSingleObject) replace the same base line differently on both sides → UNDECIDED under H1–H3
     (inferred). Fail-closed, but execution will probably stop at step 3; the revision could pre-rule those hunks.
 (b) R-REGEN's "Means" overclaims — an upstream runtime API change surfacing as an error inside src/recomp/gen is
     labelled "merge requires regeneration". Reword to "build error in generated code; regeneration is one
     candidate". Both routes stop and roll back, so no unsafe result.
 (c) R-PUSH is ordered before R-PRE with no "P0 passed" guard: a P0 failure caused by origin/main moving before
     execution could be labelled "breached during execution". Both stop.
 (d) `git branch -f a4s-merge-attempt` silently overwrites; add "no a4s-* branch exists" to P0 (none exist today).
 (e) The Closure `ls-remote` is a network op; permitted post-ACCEPT, just note it.
 (f) Upstream's C tests do not enter the game CTest at 766ecef as far as grep shows; the packet's non-assertion
     bound is consistent.
DECISIONS:         H1/H2/H3 + UNDECIDED→rollback is decidable (the "syntactically valid" clause is a judgment, but
                   it only fails closed). AC-NOPUSH is correct. The no-regeneration bound holds: build-jsrf
                   regenerates only with --allow-regeneration (observed), and provenance --check compares generated
                   bytes, not lifter hashes, so no false AC-GEN FAIL. AC-TEST is decidable. Claim limits adequate.
VERDICT:           INADEQUATE
```

## Next

Returned to the authoring Planner (§5.4): fix **only** `R-MOVED`/`W` as stated, and correct the
false "unchanged" claim. Re-review covers the changed rows and `AC-RUN` only. The deferred items
are advisories except that (a) is a prediction of what execution will hit — the revision may
pre-rule the two `kernel_bridge.c` hunks.
