# Turn plan (review-1 remediation): `title-004`

Turn Planner remediation plan for `plan-turn-review-1-title-004.md` (`TURN_REVIEW: FIX`).
Guidance, not a gate (`docs/agent-workflow.md` §2). The Orchestrator may revise it with
`PLAN_CHANGE` if new evidence makes it stale.

**Tree checked:** game `79f9f8f` (= `origin/master`, clean), toolkit `2cee914` (clean). The review
was taken at `dd85d13`. Since then: `02d190e`, `8454b81`, `cee0c27` (claimed remediation), then
`46f5c48` (stop 26 confirmed, stop 27 `0x96B60` recovered) and `79f9f8f` (TR §21). Line numbers
below are as of `79f9f8f`. §21 was appended at the end of the file, so the §19/§20 line numbers are
the same as at `cee0c27`.

## Verdict on the claimed remediation

Most of the review's text is addressed. **It is not complete.** Both blocking findings leave
residues of the same overclaim. One residue in Blocking 1 is a **new false statement introduced by
`cee0c27` itself**. Every correction below is a **record/wording correction**. No code, manifest
extent, `stack_args`, or runtime behaviour has to change, and none of the 159 repairs is in
question.

| finding | status | one-line reason |
|---|---|---|
| B1: deleted replay promoted / dominance bound | **PARTIAL: NOT RESOLVED** | dominance withdrawn and replay marked NOT VERIFIED, but the replacement "verifiable" evidence is a **three-binary** series cited as same-binary, the "byte-for-byte at any time" claim survives, and two sentences still lean on unverified or false premises |
| B2: all-path depth-0 overstatement | **PARTIAL: NOT RESOLVED** | living plan and the two p0-7 amendments are fixed; the same phrase survives in TR §20's own method line, the baseline JSON, a test comment and 159 manifest evidence strings; and the "81 vs 76+5" explanation describes a difference that does not exist |
| A: alias run count | **RESOLVED** | 44 lines / 32 runs, re-measured |
| A: stale fields | **RESOLVED, then STALE AGAIN** | fixed at `cee0c27`; `46f5c48` reintroduced the identical stop-row pattern and the plan is now behind f12 |
| A: residual scope | **RESOLVED (named), counts moved** | now 1981 PROVED / 1113 UNKNOWN / 67 SUSPICIOUS, 2894 CLEAN / 265 OVER_RUN / 2 UNQUALIFIED, 25 `KNOWN_OPEN` |

## Preserved: valid completed work, do not reopen

- **All 159 F7c/F7d repairs.** Re-measured here with `Analyzer.walk` from
  `scripts/check-stack-depth.py` over the committed manifest. **152/152 F7c and 7/7 F7d** satisfy the
  certificate: no non-`ret` exit, no fall-off, no truncation, exactly one distinct immediate, and at
  least one depth-0 `ret`. This matches the Reviewer's independent reproduction. Nothing is rolled
  back.
- **The depth-0-witness rule itself** (`stack_args = N` licensed by *any* depth-0 `ret`) is the
  established TR §10 identity. The review only objects to describing it as an all-path proof.
- **Alias latent-hazard conclusion.** Re-measured: 44 `[ALIAS-ICALL]` lines naming the ten targets
  across **32** run directories. f11 and f12 add **0** firings.
- **Stop chain.** `check-stop-chain.py` PASS on 11 rows (1 REPAIRED, 8 RUNTIME_CONFIRMED,
  2 STATIC_ONLY). Stop 26 `0x96560` is now genuinely confirmed: f12
  (`20261006-022404-045-f12-exercise-stop26`) log line 106227
  `[RECOVERED] 0x00096560 returned; ABI verified`, the log SHA256 matches `metadata.json`
  (`9c9559ba…`), and the revision `cee0c27` descends from `fc6f2d4`. Do not reopen it.
- **The withdrawn dominance language** (TR §19 2400–2407, 2415–2419) and the **NOT VERIFIED marking**
  of the replay row (2383–2390, 2395–2397, 2421–2427) are correct and stay.
- **No new replay is required.** The Reviewer accepts an honest downgrade, and I agree.

## R1: Blocking 1 residue (record correction; do first, highest diagnostic value)

**Measured contrary evidence** (archived `metadata.json` `exe_sha256`, retained logs):

| run | revision | `exe_sha256` | max `presents=` | ABI-verified returns | outcome |
|---|---|---|---|---|---|
| f9 | `b1aca89` | `43e639d7…` | 1000 | 550 | `unhandled_exception` |
| f10 | `59f3ebf` | `6c2eed73…` | 888 | 553 | `unhandled_exception` |
| f11 | `8cd1e08` | `f2c67aec…` | 888 | 440 | `diagnostic_deadline` |
| f12 | `cee0c27` | `f2c67aec…` (= f11) | 888 | 556 | `unhandled_exception` |

The overrides are the same (`RECOMP_APU_TRAP`, `RECOMP_PB_EXEC`, `RECOMP_FB_WINDOW`,
`RECOMP_FB_PRESENT_DUMP_EVERY`).

Corrections, in order:

1. **TR 2397–2399 is false and must change.** It says the retained f9/f10/f11 series
   "is enough to show same-binary variation exists". Those are **three different binaries**. This
   sentence was added by `cee0c27` as the remediation's replacement evidence, so it repeats the
   finding's failure mode: an unsupported premise standing in for a deleted one. **Consequence:** the
   only on-disk support the section cites for its "Recorded" bullet does not support it.
   Retained same-binary pairs that *do* exist:
   - **f11/f12** (`f2c67aec`): presents 888/888, returns 440/556, different outcomes. f12 reached
     `0x96560`/`0x96B60` and f11 did not.
   - **f25/f26** (`ca867957`, already in `plan-jsrf-bare-minimum.md` 603–605): presents 960/193.

   Cite one or both, or drop the clause.
2. **TR 2358–2360**: "g03 through g08 reached demonstrably different addresses on **identical
   binaries**". This came from `67bc013`, and it is false. The g03–g08 archives carry distinct hashes
   (`b670e0f8`, `d1078ee2`, `deaeac13`, `9b6eb76f`, `80861c65`, `6bc429e0`/`0188f9a1`). Re-word it as
   different binaries, or substitute the genuine pairs above. The main plan's own Blocker 11 text
   already made this same correction for f23/f24.
3. **TR 2443–2450**: "the replay can be reproduced byte-for-byte at any time". The review explicitly
   requires removing this, and it contradicts the section's own caveat at 2423–2427. Keep the input
   identity (2378–2380 is fine). Withdraw the reproducibility claim.
4. **TR 2432–2434**: the deleted-log hash `87683a748e27d071` "independently confirms" that the
   disclaimer hash is a path witness. A NOT VERIFIED value cannot independently confirm anything.
   Downgrade it to "would suggest, if verified", or remove the inference.
5. **TR 2400–2401**: "an uncontrolled variable of **at least the same magnitude** is present". This
   is still a magnitude bound, and its only basis is the unverified 394. Either drop the magnitude, or
   attribute it to a retained pair (f25/f26, 960 vs 193, a different binary) and say so.
6. **TR 2356 and the §19 heading name the wrong confounder set.** The f9→f10 delta is `0445a80`
   (**F7b**, which re-ended `0xAE560`, the function f9 trapped in) plus `0763de8` plus `59f3ebf` (F7c).
   Checked: `0445a80` is an ancestor of `59f3ebf` and not of `b1aca89`. f11 adds `fc6f2d4`
   (stop 26) and `8cd1e08` (F7d). So the text "f10 and f11 differ from f9 by F7c *and* F7d together"
   is wrong for f10. Likewise, TR 2368's "a run at `b1aca89` (pre-F7c)" is a same-binary replay of f9,
   not an F7c isolation. The F7c single-variable pair would be `0763de8` vs `59f3ebf`.
7. **Record the plateau observation neutrally.** I measured max `presents=` across every retained
   `2026100[56]` archive. Exactly **1000** in 22 pre-F7b runs on many binaries. Exactly **888** in
   **all three** post-`0445a80` runs (two binaries). No pre-F7b run ended at 888; the other pre-F7b
   values are scattered (9, 113, 193, 730, 960). This is a correlation only, and causality stays
   *unresolved*. It is relevant because it argues against the section's residual lean toward
   "variance", and the review's stated consequence was misdirecting the next critical-path decision.

**Smallest proof of R1:** the hash table above (already measured, reproducible from the archives'
`metadata.json`), plus a grep showing no surviving `identical binaries`, `same-binary variation
exists`, `byte-for-byte at any`, or `independently confirms` in §19. **No run is needed.** An
optional run is useful only if the 888 plateau becomes the critical path: one archived run of a
`0763de8` build with the canonical overrides, with outputs and environment retained. That is the
Orchestrator's call.

## R2: Blocking 2 residue (record correction)

1. **The "81 vs 76+5" discrepancy does not exist.** 76 + 5 = 81. Re-measured at the committed
   manifest: **F7c 76/152, F7d 5/7, total 81/159** entries with at least one `UNKNOWN`-depth exit.
   Examples agree with the review:
   - `0x21200`: `ret 16` at `0x2124E` UNKNOWN; at `0x2131A` both UNKNOWN and 0.
   - `0x202A0`: `0x20326` both UNKNOWN and 0.
   - `0x171B50`: `0x171D98` UNKNOWN; `0x171DF5` both UNKNOWN and 0.

   TR 2505–2508 (and the commit messages of `8454b81` and `cee0c27`) explain a "small difference" by
   classification. That explanation describes nothing. Replace it with "the counts agree: 76 + 5 =
   81". It is a classification non-issue, not a real discrepancy.
2. **TR 2459–2460** (§20, added in `ae9c090`): "require a fully enumerated walk whose **every exit is
   one `ret N` at depth 0**". This is the exact phrase §20's own erratum at 2511 declares false.
   Qualify it to the precise certificate.
3. **`docs/reviews/p0-full-generated-baseline.json` 451 and 469** carry the F7c/F7d texts **without**
   the "AT LEAST ONE" qualification that `8454b81` applied to the p0-7 copies. Its "exactly two were
   changed" was true of p0-7 only. Apply the same qualification, then run
   `check-generation-provenance.py --check`.
4. **`tests/test_recovery_span_ownership.py:58`** comment: "whose every exit is one `ret N` at
   depth 0". The comment is false; fix the wording only.
5. **`config/recovered-functions.json`, 159 evidence strings** (152 F7c + 7 F7d) say "exactly one
   distinct immediate, `ret N` -- reached at DEPTH 0". This is the same ambiguity the p0-7
   amendments were changed for. Two reasonable paths; the Orchestrator chooses:
   - **(a)** A mechanical qualification ("…with at least one exit reached at DEPTH 0").
     Verified: this prose does **not** reach `recovered.c` (0 occurrences), but it is a generation
     input, so refresh provenance.
   - **(b)** Leave the prose and add one TR sentence defining the manifest phrase as "at least one".

   Either path is acceptable. Silently leaving 159 copies after correcting 2 is not.

**Smallest proof of R2:** the per-entry UNKNOWN count above (one `Analyzer.walk` loop, about a
minute), plus a repository-wide grep for `every exit is one` and an unqualified `reached at DEPTH 0`,
showing only qualified or defined uses. Then `just check`. No rebuild or run is needed.

## R3: Advisories (record hygiene; after R1/R2)

1. **Stop 27 repeats the exact stale pattern the review flagged for stop 26.** Its `repair_commit` is
   `null` and its note says "The repair is in the working tree", but the recovery landed in
   `46f5c48`. Fill it, keep `REPAIRED` with `discovered` only, and keep the note's
   not-runtime-confirmed sentence.
2. **Stop 26 lost its discovery citation.** `46f5c48` replaced the f10 `discovered` row with the f12
   `confirmed` row. Its note claims "the discovered-run citation is the same run here", which is
   wrong: f10 discovered `0x96560`, and f12 discovered `0x96B60`. Restore f10 as `discovered`
   alongside f12 `confirmed`, as stop 25 does, and correct the note. Re-run `check-stop-chain.py`.
3. **Living plan is behind the tree.** Endpoint at
   `plan-turn-updated-title-004.md` 9–10 says `8454b81`. Remaining-work item 1 at 292–294 still
   schedules the run that f12 has now performed. Stop 27, f12 and §21 are absent. Update the plan, then
   pin the remediation endpoint, which the review asked for.
4. **Residual counts** in the plan's status block should read 1981 / 1113 / 67, 2894 / 265 / 2 and
   25 `KNOWN_OPEN`. The plan's remaining work names 25 `KNOWN_OPEN`, `0x96F80` and the alias classes;
   it does not name 1113 UNKNOWN or 265 OVER_RUN. Add one line. Do not present them as closed.

## Order and closure

R1 → R2 → R3, all records-only, in one or two commits. Then:

1. `just check`.
2. `check-stop-chain.py`.
3. `check-generation-provenance.py --check`.
4. The two grep sweeps above.

CTest is required only if the test file edit is more than a comment. Push per `AGENTS.md`: the game
repository only, because the toolkit has no change. M15 remains **not claimed**. After that, return
the tree to the Turn Reviewer for Review 2.
