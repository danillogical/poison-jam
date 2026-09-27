# Advisor ruling — `A4b2-NR-epoch-slice-followup-r1`: terminal `L1` decision, V2 order, radix correction

**Advisor:** Persistent Advisor, `docs/agent-workflow.md` §2.3. **Handle:** `muse_FkNhGaXtV9P5`
(`.muse-workers.md`). **Model:** `muse-spark-1.3-contributor`. **Effort:** `max` (plugin default).

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Trigger:** the terminal `L1` referral the Advisor's own method ruling reserved to itself, after the packet
measured which DMA descriptor produces the exchange.
**Basis:** `docs/reviews/a4b2-nr-epoch-slice-execution-evidence.md` (whole),
`docs/reviews/a4b2-nr-epoch-slice-disjointness-preparation.md`, `plan-jsrf-bare-minimum.md`,
`docs/reviews/a4b2-nr-epoch-slice-followup-r1-session-verification.md`.

**Route note.** The referral was attempted **four times** before succeeding — three `MODEL_UNAVAILABLE`
(model stream idle timeout, `retryable: false`) and a fourth that timed out at 600 s and completed
server-side, collected via `muse_session_read` per §4.4. **No substitute model was used.** Recorded in
`docs/reviews/a4b2-epoch-slice-advisor-route-outage.md`.

---

## RULING (verbatim)

> All verification complete. Two decisive findings from my own checks: (1) the `block_addr` print is `%04X` — hex, not decimal, which confirms the block-24 identification while invalidating the record's radix exposition; (2) the 39-write artifact closes the block-24 alias question by inspection. Ruling:
>
> ## RULING
>
> ### Q1 — `O-TWO-LEG`, contingent on one bounded verification (V2). Row stays unselected until it lands
>
> **Decision: SELECT `O-TWO-LEG` upon V2 clean (Session records it without re-referral); if V2 finds anything live, SELECT `O-INCONCLUSIVE` and re-refer for scope/defer/retire. No fifth packet either way.**
>
> The closure ledger, against the packet's bar, with the corrected descriptor:
>
> - **Block-24 fields all immediates** (control/count/dsp_offset/scratch_offset from `P 00F1`/`000D`/`000A`/`000B`), measured producing the exchange (latch line + ordinal 765/766 + exact `0x800` match). **Closed.**
> - **Trigger guards + shared-state `a`**: immediate chain, ×1. **Closed** (descriptor-independent).
> - **B3 walks**: terminate in scratch/loop-control, no slice reach. **Closed.**
> - **Computed readers/writers**: single computed read (`P 00B9`, `r1` immediate `0x24`); computed writes all outside every descriptor region **including [24..28]**. **Closed.**
> - **Direct-write alias for [24..28] (V1): CLOSED BY ME NOW.** The prep artifact classifies all 39 direct writes as `{x:$0004, x:$007C–$007F, x:$FFFF*}`, disjoint from `[0x18..0x1C]` by inspection. Cited so r8's adequacy reviewer can re-verify.
> - **Entry set, interrupts/faults, image-I watch stability, L2**: carried, all green.
> - **V2 (ordered, the one open leaf): cross-boundary enumeration over at-exchange second-image bytes, BOTH directions** — (out) image-I transfers targeting ≥`0x173` (falls on doorbell path? returns clobbering slice state?); (in) second-image transfers targeting slice nodes **or the dead-block PCs** (a second-image→dead-block edge plus dead-block writes would be a two-hop chain — the dead blocks' image-I unreachability does not cover it); (data) second-image X-write destinations vs slice-read words (`[24..28]`dec, guard/shared words). Static artifacts only — **no new runs, no new packet**; if V2 needs a run, STOP and re-refer (scope breach). Deliverable: method + full lists (or "none" with universe + coverage proof) + artifact hashes, appended to the verification record. Any live edge/write, or any indirect/unresolvable → precise `UNKNOWN` → row does not stand, back to me.
>
> F-D is not breached: this is Advisor-ordered verification of a referred row (§2.3/§2.4: order the measurement that closes the named gap), not an L1 attempt round. This ruling is not acceptance — discovery acceptance (artifacts + row selection) still applies.
>
> ### Q2 — The tally does not overturn the closure; your calibration instinct is half-right, and the correction matters
>
> **Direct answer: trustworthy AS A MEASURED CLOSURE; no further re-derivation round required.**
>
> - Your inference ("high error rate → lower confidence") is miscalibrated in a specific way: five errors are static-tooling errors **all caught by measurement or cross-check, never by re-reading**; the sixth (ungated trace) was caught by a can-fail count anomaly. The correct update is not uniform discounting but **discount by evidence class**: static-only claims from this work carry demonstrated risk; measured claims — especially multiply-pinned ones (B9 addresses + watch counts + decode compare + DMA-desc latch + ordinal + L2 + tuple) — carry the weight. Every load-bearing fact in this closure is now in the second class. That is why the closure stands despite the tally.
> - The fresh-code re-derivation covers the highest-risk single point (post-misidentification block identity) for tooling artifacts — but it is currently **reported, not recorded**. Require: script + inputs + outputs + hashes archived before `A4b2-r8` cites it. No new re-derivation packet; r8's mandatory fresh-Planner adequacy review is the independent gate (with teeth: `INADEQUATE` blocks r8).
> - **Radix correction REQUIRED (record hygiene, not row-blocking):** `block_addr` prints `%04X` (`apu_watch.c:657`) — `0018` is **hex 0x18 = block 24**, not "decimal". The identification is right; the word "decimal" in the evidence (§"decisive measurement"), the erratum, and the "trap" note is wrong and **must be corrected before r8 opens** — a future reader doing decimal arithmetic re-derives block 18 → region [18..22] → the wrong descriptor again, the exact failure mode. (Corroboration: the whole system closes under hex-print — chain `0x06→0x25→0x1E`, `r0=#$18`, region `[24..28]`dec, `#$000012=0x12`→`[18..22]`; DSP `$` = hex throughout.) Also: **forbid** citing region-counter tallies (`in_doorbell_region` etc.) for block identity — labels predate the correction and bounds are unverified here; cite only the latch-attributed line + ordinal analysis. DMA chain-restart semantics remain unmodeled and r8 must not depend on chain theory — attribution is measured, not derived.
>
> ### Q3 — r8 carry list; yes to the correction; yes, `PIO_FREE` next
>
> **`A4b2-r8` carries:** (1) the two-leg outcome (L1 per this ruling incl. V2 result; L2) + the conditional-ruling citation; r7 stays `R2-EXPL-INPUT`, no retroactivity; (2) the block-24 identity **with corrected radix**, citing erratum + latch line (yes — record the correction explicitly in r8 so the wrong identifier dies here); (3) the leaf table as the input-qualifier restatement basis; (4) boundary statements: second-image INTERFACE enumerated per V2, INTERNALS unanalyzed and unneeded (demand-driven discipline — state so a future reader doesn't mistake scope for oversight); B3-ID still deferred. **`PIO_FREE` is genuinely next** — nothing since r2 changed the sequencing (loader is GP-internal DMA; no new PIO_FREE dependency discovered).
>
> ### Q4 — No §5.4(2). The erratum (once radix-corrected) is sufficient
>
> **Agree, for the recorded reason plus one check I ran:** no frozen contract premised the descriptor's *identity* — criteria concerned the exchange-producing descriptor functionally, now correctly bound by measurement. My check: no executed row depended on the identifier — all four prior rows were `O-INCONCLUSIVE` on independent grounds (window/bytes/epochs/scope); an `INCONCLUSIVE` row asserts no closure and inherits no misidentification. L2, tuple, watch, entry, interrupts never touched the identifier. Frozen packets listed-not-edited is correct (§5.7 provenance). Require: radix fix in erratum + evidence record + a V2 pointer; no packet action, no re-review.
>
> **BASIS:** observed — evidence file whole (slice tables, consumer chain §, corrections, row referral); prep artifact (5 sites, 10-pair disjointness, 39-write address classes, computed-form census); print format `%04X` (`apu_watch.c:657`); verification record (method/elements/promotion); plan CURRENT block; prior ruling obligations. Inferred — V1 closure by address inspection; V2 scope (both directions + dead blocks + X-writes) as the complete residual; measured-closure trust calibration. Uncertain — V2's outcome (the ordered check, not an assumption); re-derivation contents (reported, archival required).
>
> **REVERSED BY:** V2 finding a live cross-boundary edge/write into slice state (row flips to `O-INCONCLUSIVE`, re-refer); re-derivation archive contradicting the latch attribution; a P-write watch bypass (contained by F3).
>
> **RECORD IN:** this ruling verbatim with handle/model/effort in the epoch-slice review record; V1 close + V2 order + radix correction bind the Session before r8 opens; r8 brief carries Q3's list. — Note, not ruling: the referral itself was correct conduct — measuring instead of arguing, and refusing to self-select with known concerns outstanding, is exactly what the terminality clause exists to reward.

---

## Session obligations arising from this ruling

| # | Obligation | Status |
|---|---|---|
| **V1** | Direct-write alias for `[24..28]`dec | **CLOSED BY THE ADVISOR** — the 39 direct writes are `{x:$0004, x:$007C–$007F, x:$FFFF*}`, disjoint from `[0x18..0x1C]` |
| **V2** | **Cross-boundary enumeration over at-exchange second-image bytes, BOTH directions** — (out) image-I transfers targeting ≥`0x173`; (in) second-image transfers targeting slice nodes **or the dead-block PCs**; (data) second-image X-write destinations vs slice-read words. **Static artifacts only — no new runs, no new packet.** Any live edge/write or unresolvable → precise `UNKNOWN` → re-refer | **ORDERED — this is the one open leaf** |
| **R1** | **Radix correction**: `block_addr` prints `%04X` (**hex**), so `0018` = block **24**. The word "decimal" in the evidence, the erratum and the trap note is **wrong** and must be fixed **before `A4b2-r8` opens** | **ORDERED** |
| **R2** | **FORBID** citing region-counter tallies (`in_doorbell_region` etc.) for block identity — cite only the latch-attributed line + ordinal analysis | **Recorded** |
| **R3** | **Archive** the fresh-code re-derivation: script + inputs + outputs + hashes, before `A4b2-r8` cites it | **ORDERED** |
| **R4** | DMA chain-restart semantics stay unmodeled; **`A4b2-r8` must not depend on chain theory** | **Recorded** |
| **Q1** | **SELECT `O-TWO-LEG` upon V2 clean** — no re-referral. If V2 finds anything live → `O-INCONCLUSIVE` + re-refer | **Conditional** |
| **Q3** | `A4b2-r8` carry list (4 items, incl. the radix-corrected block-24 identity); **`PIO_FREE` is genuinely next** | **Brief for Planner** |
| **Q4** | **No `§5.4(2)`**; erratum sufficient once radix-corrected | **Settled** |
| **F-D** | Not breached — V2 is Advisor-ordered verification, not a fifth `L1` attempt | **Recorded** |

**The Advisor also noted, not as a ruling:** *"the referral itself was correct conduct — measuring instead of
arguing, and refusing to self-select with known concerns outstanding, is exactly what the terminality clause
exists to reward."*
