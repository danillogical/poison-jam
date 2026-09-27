## A4b2-NR-next-edge — discover full-program doorbell input dependencies

### Sketch
- **Class/claim:** §5.8 discovery, not strict acceptance; decide whether a full-program feasible-route slice proves or refutes non-reliance of the first named GP doorbell on MIXBUF and `0xFFFFB3`.
- **Blocker:** followup selected `O-INCONCLUSIVE`: 380 of 2 340 executed PCs were in the earlier 512-word window; `P 00B9` is measured closed for the bounded exchange.
- **Unknowns:** full 3 881-word control flow, indirect edges, input def-use, candidate writes/guards, descriptor aliases and alternative reaching definitions; seven suspended completeness claims require re-derivation.
- **Experiment:** validate full P-memory image/decode, construct PC-indexed feasible CFG/def-use with explicit UNKNOWN edges and static no-propagation or concrete causal chain; use the histogram only as a lower-bound cross-check.
- **Controls/closure:** preserve r2 L2, one fresh absent-gate baseline at pinned identity, conditional same-exe mini-series on absent-gate GP-path changes; remove `bad-output`, add two-header `is_gp` guardrails, keep diagnostic gates off.
- **Rows:** `O-REFUTED` → real VP + B3 resolution; `O-TWO-LEG` → `A4b2-r8`; `O-INCONCLUSIVE` → bounded next discovery targeted to the recorded unresolved full-program edge.
