# A4b1-r1 / A4b2-r1 adequacy review — INADEQUATE (both)

**Reviewer:** Planner child `e0e10ffa-37d0-4f91-bda7-e07c4d5f3b4b`, route
`claude` / `claude-opus-5-5` @ `high` — a fresh Planner that wrote neither packet (§5.1.5).
**Hashes verified by the reviewer:** `A4b1-r1` `90FA4410…7233`; `A4b2-r1` `BD3718E6…CBC1`.

```text
PACKET:            A4b1-r1  SHA-256 90FA44103FAD4077411E9137775C2934E755E392CE6E33B6AC2399F7F4BE7233
PREMISE_FRESHNESS: PASS. Observed: the synthetic ack is still in apu_dsp.c:40,61; GP/EP writes are still dropped (apu_core.c:618) and reads still answer 0 (apu_core.c:637); guest MMIO reaches the APU only under RECOMP_APU_TRAP (xbox_memory_layout.c:1728); the pre-check lines hold; toolkit is at 0d7929c.
BLOCKING:
 B1. AC-FIX (e)/(f) with Device semantics 6 (exemption = "the first zero-payload covering write in the process"; counters cumulative). One trace-on process, (e) before (f): (e)'s zero-over-3 write uses up the exemption and adds to the cap, so (f) logs 15 non-zero lines, its zero line is not logged, and the summary reads watch=23 watch_zero=3, not 21/1. Reversing the order caps (e) instead. A correct implementation fails either way → R1-PORT-FAIL, or the executor "fixes" it wrongly. Required: (f) in a fresh process with no earlier covering writes, or expected counts stated relative to its start; state the fixture order.
 B2. Device semantics 3 logs `[GPDMA] unmapped …` unconditionally; only DS6's lines are inside the RECOMP_APU_TRACE guard. The trace-off registration must still satisfy (c), which requires that line — and it starts with `[GP`, so "zero [GP lines" fails → false FAIL → R1-PORT-FAIL. Required: exclude the unmapped line from the trace-off count, or trace-gate it and have (c) in trace-off check PRAM only.
 B3. DS6's exemption ("the first zero-payload covering write in the process") misses the clearing write. Observed: GPRST=3 at L3201; frames open at L3286; sub_001A1769's store of 3 first traces at L5226 — so the GP is clocked for many frames before the guest writes 3. A GP write-back covering +0x810 while the word is still 0 takes the exemption with cas=fail observed=0; later lines can fill the 16-line cap, so the real 3→0 cas=ok write is never logged. A4b2 then selects R2-CPU, a false FAIL blamed on the guest CPU, and A4b2 cannot change toolkit code. Required: guarantee logging of every zero-payload covering write that observed a non-zero value, plus the first zero-payload write after one. §5.5: second consecutive blocking defect in this mechanism (after A4b-r3 B2) — redesign, not a third patch.
 B4. AC-PORT step 2 ("Get-FileHash each vendored file at its pre-edit commit") gives no method. Observed: core.autocrlf=true and no .gitattributes rule for .c/.h/.inc, so a checkout or `git show >` yields CRLF and every hash differs from the pinned LF hashes (inferred). Required: pin the method, e.g. `src/apu/dsp/.gitattributes` with `* -text` (in scope), or hash byte-exact `git cat-file blob` output.
DEFERRED:          (1) fixture infeasibility routes to R1-PORT-FAIL, reverting a possibly correct port for a harness problem; consider its own row. (2) (d) needs low RAM mapped and seeded; (e)/(f) need MEM32(0x001BA858) seeded — neither stated. (3) a toolkit-hosted add_test runs before the game's include(CTest) (CMakeLists:21 vs :76), so it may not register (uncertain; fails closed). (4) require the fixture to drive the production MMIO→DMA path, not a test-local copy. (5) `ctest > <R0>\ctest.txt` runs before R0's directory exists. (6) state that a G2 failure stops AC evaluation, before R1-DEFAULT-REGRESS. (7) [GPRUN]/[GPIN] formats untested.
DECISIONS:         Split claim clean: R0 is untrapped and the MMIO path is gated at :1728 (observed); reversed by a default-path route into GP MMIO. r3 B2 / D1 / D2 / D4 repairs present and correctly worded, but see B3. Rows ordered and exhaustive, each naming its next packet; R1-UNKNOWN's "Planner rules otherwise" is acceptable because it defers to a named authority. Write scope complete; A4b1 touches no game source.
VERDICT:           INADEQUATE
```

```text
PACKET:            A4b2-r1  SHA-256 BD3718E639A6484216F89E245D185F2245CC5231788953F77B735C6EC528CBC1
PREMISE_FRESHNESS: BOUNDED. P1 observed (XBE hash matches; A4p O-GATE accepted). P2 and the baseline are future commits; P2 fails closed.
BLOCKING:
 B1. AC-BOOT ordering. Observed: apu_hook_handle_mmio prints `[APUMMIO]` after apu_decode_and_handle returns (hook.c:283 then :300) and DS1 bootstraps synchronously inside the GPRST write. So [GPBOOT] precedes `[APUMMIO] write 0x3FFFC = 00000003`, and the GPRST line just before [GPBOOT] is the `=1` reset (L3190). "Follows a GPRST write that bootstraps" never holds; none of FAIL's or UNKNOWN's conditions applies, so AC-BOOT has no outcome — read as FAIL it selects R2-NOBOOT, the wrong row and next packet. Required: key to [GPBOOT]'s `gprst` field plus the prior GPRST value, or to the 0x3FFFC line right after the block; add "otherwise UNKNOWN".
 B2. G4 and F are pinned to recomp_0005.c:6748/6751, but step 1 inserts calls before 6524 and 6746 plus an extern in that same file, so loc_001A18D0 moves down 2–6 lines. G4 fails (R2-UNKNOWN), or F=0 in R0 and R2-DEFAULT-REGRESS reverts the edits — deterministic under a literal execution. Required: take the line numbers from the edited file by content, or require step 1 to keep every line number unchanged. Keep A4a R0's F oracle separate from R0's F.
 B3. AC-CLEAR's deciding line is not anchored after the guest's store of 3. The same pre-command GP zero write as A4b1 B3 gives cas=fail observed=0 → R2-CPU, a false FAIL blamed on the guest CPU. Required: decide on the first zero-payload watch line after the control `[A4BSTORE] … value=00000003` line; if there is none, W=0, and the cap was reached, the result is UNKNOWN. Apply the same anchor to AC-NOCPU's "before the first cas=ok". Depends on A4b1 B3.
DEFERRED:          (1) "Log not truncated" is decidable and fails closed only for the exempt line; stderr is unbuffered (main.c:154), so risk is low. (2) the silent [APUMMIO] 400-line cap: 0x02040 was line 104 in A4a R1, within the cap. (3) no R0 rerun when R0's own gate gives R2-UNKNOWN. (4) inferred: the 1A1FA7 / 1A1751 stores of 0 do not run before the spin; not guaranteed. (5) [A4BSTORE] site list is corroboration only.
DECISIONS:         The cas witness is sound: cas=ok observed=3 can come only from the GP DMA exchange, and CLEAR without BOOT/RUN → UNKNOWN. Reversed by a watch path shared with VP or FEMEMDATA. Coverage table and rows exhaustive and ordered, each naming its next packet; the exception is B1. D1/D2/D4 present; P1 carries A4p's limits in substance. Write scope complete (diagnostics.c already linked, observed).
VERDICT:           INADEQUATE
```

## The split: sound in claim, leaks at one seam

The reviewer judged the **claims** split cleanly — `A4b1` makes no trapped-run claim (its run is
untrapped and guest MMIO is gated off at `xbox_memory_layout.c:1728`), and `A4b2` writes only
game files. **But the seam leaks in one direction:** `A4b1` freezes the trace contract that
`A4b2` must decide from — the watch exemption and cap rules, when `[GPBOOT]` is emitted, and
the `[GPRUN]`/`[GPIN]` formats — while `A4b2` may not change toolkit code. `A4b1` B3 and
`A4b2` B1/B3 all come from that seam. The reviewer's recommendation: revise and review the two
together, keep the trace-contract text only in `A4b1`, and do not freeze `A4b1` until
`A4b2`'s deciding rules have been checked against it.

## §5.5 — second trigger, a different mechanism

Watch logging and the deciding line now have **two consecutive blocking verdicts**:
`A4b-r3` B2 (the `[GPDMA] watch` cap could lose the deciding `payload=0` line) and now
`A4b1` B3 + `A4b2` B3 (the exemption can be consumed by a pre-command GP write-back, so the
real 3→0 write is never logged). Per §5.5 that mechanism is **redesigned or taken to the
Advisor**, not patched a third time. The Session is consulting the Advisor.

**Cheap fixes** (ordinary §5.4 repairs, not the redesign): `A4b1` B1, B2, B4; `A4b2` B1, B2.

## Reviewer's stated coverage limits

It did not read the Q1 ruling in full, the Q2 or planning rulings, `a4p-execution-evidence.md`,
or the run-profile docs, and it did not verify the baseline exe hash itself.

**Next:** Advisor ruling on the watch-logging mechanism (§5.5), then a Planner revision of both
packets together. Neither packet is frozen or promoted; `CURRENT PACKET` remains `none`, and the
**toolkit sync** is still the next executable work.
