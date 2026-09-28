# A2h — discover the ~571 MB allocation's causal input

**Class:** discovery   **Contract revision:** `A2h-oom-causal-slice-r1`   **Status:** draft

## Sketch
- Claim: identify the failing trapped strict `NtAllocateVirtualMemory` invocation, its guest size-input chain, and the first unresolved or contradicted producer; do not fix or claim progress.
- Known: the archived trapped strict log has ordinal 184, `size=598869040`, `ret=00149E50`, then heap OOM and invalid ICALL; the older A2h dump-displacement investigation is not a current causal proof.
- Unknowns: which guest definition supplied that size, whether it depends on a particular trapped APU value or is independent, and whether the archived prefix/dump can bind the chain without a new run.
- Experiment: identity-check archived failing run; decode the original XBE at the kernel call and backward-slice only its size definition, with actual invocation values anchored by the log and trustworthy frozen evidence; if a value/branch is not recoverable, use one short diagnostic-only trapped strict run instrumented at the proven producer, bounded by the existing ~4.77 s horizon.
- Outcomes: `O-MODEL-LEAF` → narrow evidence-grounded model investigation; `O-GUEST-LEAF` → targeted guest/translation defect packet; `O-OPEN` → a single missing-witness discovery; `O-IDENTITY` → provenance repair. No outcome authorizes suppression, arena widening, or allocation fakery.
