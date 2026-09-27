# `A4b2-r7` adequacy review — `ADEQUATE` (with a predictive caveat on the expected row)

**Reviewer:** fresh Planner child `7c6fd178-49cf-4f00-af5f-3401f3b6d85c`, `codex/gpt-6-sol` @ `high`
(§1, §5.1.5(3)). Not the authoring child `b430c48f-42ea-463f-99e6-97da5989e73a`.
**Revision reviewed:** `docs/packets/a4b2-gp-clears-pending-word.md`, `A4b2-r7`, SHA-256
**`93E410C3742D0E290A0C0BBA71DCFEC547A77ECAC497E389BA5D58026E551F95`** (independently recomputed by the
reviewer, 136 lines).
**Review type:** §5.4 re-review after execution, on a **§5.4(1)** ground.
**Recorded verbatim** from the reviewer's own response, as §2.2.10 requires. Written to disk **before**
promotion.

---

## Verdict block (verbatim)

```text
REVISION:          A4b2-r7; SHA-256 93E410C3742D0E290A0C0BBA71DCFEC547A77ECAC497E389BA5D58026E551F95 (I recomputed Get-FileHash on packet bytes).
READ:              docs/agent-workflow.md:239-336,338-365,499-724,755-833; plan-jsrf-bare-minimum.md:9-88; docs/jsrf-run-profiles.md:11-74,137-179,208-243; docs/packets/a4b2-gp-clears-pending-word.md:1-136; docs/reviews/a4b2-execution-evidence.md:1-251; docs/reviews/a4b2-r6-execution-ruling.md:1-137; docs/packets/a4a-dsp-pending-word.md:1-61; docs/reviews/a4a-execution-evidence.md:1-81; docs/reviews/a4a-r2-acceptance-review.md:1-38; logs/runs/20260924-191833-331-a4a-r2-trap-trace/{metadata.json:1-85,result.json:1-14,jsrf_run.log:3188-3201 (grep)}; logs/runs/20260927-103258-133-a4b2-gp-trap-trace-rerun/{result.json:1-14,jsrf_run.log:3196-3210,5787-5788,6881-6901,13166-13210,16300-16324,16518-16615 (read/grep)}; logs/runs/20260927-103003-050-a4b2-gp-trap-trace/jsrf_run.log:3190-3197,5812-5813,6887-6888,16505 (grep); logs/runs/20260927-104618-057-a4b2-control-no-edits/jsrf_run.log:3192-3199,6894-6895,16137 (grep); docs/jsrf-operating-history.md:1000-1044; scripts/check-dump-mapping.py:1-57; src/recomp/gen/recomp_0005.c:6748-6761; src/diagnostics.c:108-145; toolkit src/apu/dsp/gp_ep.c:340-407; toolkit src/apu/apu_watch.c:695-855,970-1021; docs/reviews/a4b2-r4-session-mechanical-verification.md:1-316 (lead only). Git identities and toolkit-clean check and XBE SHA independently rechecked.
PREMISE_FRESHNESS: BOUNDED — observed accepted A4a oracle and unchanged original XBE, current game/toolkit identities, and archived r6 live clear and pre-existing dump displacement; actual r7 fresh run outcome is uncertain. In particular observed old r6 [GPIN] at_clear mixbuf_stub_read=1 and nonzero reads_while_stub (rerun log:6881-6886), with interleaved later log blocks: do NOT promise R2-PASS. r7 requires new R1/R0 and fails closed to EXPL-INPUT/UNKNOWN if reproduced.
BLOCKING:          NONE
DEFERRED:          NONE
DECISIONS:         Accept T=0x803C0000 as independent can-fail page oracle; directly observed accepted A4a strict O-6 with mapping-verified B=S0=T, 1484-byte original-XBE image, and independent fresh-header source; reverse if A4a evidence or page identity is contradicted by verified original title evidence.
DECISIONS:         Accept log-bound common B and anchor cross-check as fail-closed; :61,:75,:77 impose complete all-instance blocks, last counts, common sge0_va, nonwrapping anchor arithmetic before FAIL/PASS, so divergence from T is NOBOOT whereas malformed binding is UNKNOWN; reverse with concrete valid-source output that violates that partition.
DECISIONS:         Delete global R1 G2 PASS gate, keep per-branch dump boundary; :60,:62,:88-98,:129 require no R1 dump on PASS, R1 mapping/readable Wf on every no-clear dump-based branch, R0 independent mapping for AC-DEFAULT2; reverse if any decision path consumes an unverified R1 dump.
DECISIONS:         Treat r6 runs only as premise leads, never r7 acceptance; :6,:42,:60,:135 require new archives/source identities for every gate/AC; reverse if stale archive is allowed to discharge one.
VERDICT:           ADEQUATE

Observation caveat: r6's actually printed mixbuf stub reads predict R2-EXPL-INPUT or UNKNOWN on faithful fresh repetition, not PASS; this is not an adequacy block or permission to rescore r6.
```

`VERDICT` is `ADEQUATE` because `BLOCKING` is `NONE` and `PREMISE_FRESHNESS` is not `FAIL` (§5.3).

## The reviewer's predictive caveat — Session-verified

The reviewer observed, in the archived `r6` run, that the printed `at_clear` block already shows the
`AC-INPUTS` stub condition. **The Session independently reproduced this from the same bytes:**

| Measurement (r6 R1 rerun) | Value |
|---|---|
| `[GPIN] at_clear flags …` | **`out_of_universe=0 mixbuf_stub_read=1 boot_scratch_read=1`** |
| `at_clear` MIXBUF `reads_while_stub` nonzero bins | **26** of 32, each reading `7648` |

Under `AC-INPUTS` step 3 (`packet:115`), *"MIXBUF any bin's `reads_while_stub>0` or `mixbuf_stub_read=1`
→ **FAIL**"*, and step 2 (`:114`) accepts `boot_scratch_read=1` and `out_of_universe=0`. So on a faithful
fresh repetition the expected selection is **`R2-EXPL-INPUT`** (steps 1–2 valid, step 3 stub nonzero), or
**`R2-UNKNOWN`** if the step-1/2 integrity checks fail — **not `R2-PASS`.**

**This is recorded so it is not later mistaken for a surprise.** It is **not** a defect in `r7`, not an
adequacy block, and **not permission to rescore `r6`** — `r6`'s `R2-UNKNOWN` stands as selected. It is a
prediction about a fresh run under a different contract, and it is consistent with the Advisor's own
framing: `R2-EXPL-INPUT` routes to the Advisor to decide whether the named stub input needs its own model
packet first.

## Session actions on this verdict

1. **This record written from the reviewer's own response before promotion** (§2.2.10).
2. **The packet was not edited** on `ADEQUATE` (§5.3: *"no discretion to revise first"*).
3. **`A4b2-r7` at `93E410C3…551F95` was frozen and promoted into `CURRENT PACKET` in the same step**,
   byte-identical with no revision.
4. **Execution re-runs fresh R1/R0** (`packet:42`, `:135`): the `r6` archives are premise leads only and
   cannot discharge any gate or AC.

## Final closing block (verbatim) — the reviewer's authoritative final response

The closing message restates the same verdict with a tightened `DECISIONS` set and repeats the execution
caution. **Verdict unchanged: `ADEQUATE`, `BLOCKING: NONE`.**

```text
REVISION:          `A4b2-r7`; SHA-256 `93E410C3742D0E290A0C0BBA71DCFEC547A77ECAC497E389BA5D58026E551F95` (independently verified).
READ:              `docs/packets/a4b2-gp-clears-pending-word.md:1-136`; `docs/reviews/a4b2-execution-evidence.md:1-251`; `docs/reviews/a4b2-r6-execution-ruling.md:1-137`; `docs/agent-workflow.md:239-365,499-724`; `docs/jsrf-run-profiles.md:11-74,137-179,208-243`; `docs/packets/a4a-dsp-pending-word.md:1-61`; `docs/reviews/a4a-execution-evidence.md:1-81`; `docs/reviews/a4a-r2-acceptance-review.md:1-38`; `docs/jsrf-operating-history.md:1000-1044`; A4a and r6 archived run metadata, results and relevant log ranges; `src/recomp/gen/recomp_0005.c:6748-6761`; `src/diagnostics.c:108-145`; toolkit `gp_ep.c:340-407` and `apu_watch.c:695-855,970-1021`.
PREMISE_FRESHNESS: BOUNDED — the accepted A4a page oracle, current source identities and r6’s observed clear remain supported; fresh r7 runs must establish its result.
BLOCKING:          NONE
DEFERRED:          NONE
DECISIONS:         Accept `T` as an independent, can-fail page oracle; A4a’s accepted strict run independently established `0x803C0000` against the original XBE; reverse with verified contradictory title-page evidence.
DECISIONS:         Accept log-bound `B` and anchor cross-check; missing or inconsistent structure, counts, addresses or arithmetic routes UNKNOWN, while a valid page differing from `T` routes NOBOOT; reverse with a concrete valid-source counterexample.
DECISIONS:         Accept removal of the global R1 dump gate; PASS consumes no R1 dump, and dump-dependent branches require their own mapping checks; reverse if an unverified dump read can determine a row.
DECISIONS:         Require fresh r7 R1/R0 artifacts; r6 artifacts are premise leads only; reverse if a prior-revision artifact can discharge a gate or criterion.
VERDICT:           ADEQUATE

**Execution caution, not an adequacy defect:** the old r6 log records nonzero `at_clear` MIXBUF stub reads. If fresh r7 evidence reproduces them, do not forecast `R2-PASS`; apply `AC-INPUTS` and the ordered rows literally. The verdict has been sent to the parent Session.
```

The reviewer's caution is the same prediction the Session independently reproduced and recorded above.
**The Session will apply `AC-INPUTS` and the ordered rows literally to the fresh run and report whatever
row is actually selected** — no forecasting, no rescoring of `r6`.

## Consequential state

- **`A4b2-r7` is the promoted packet.** It is the only packet authorized to execute.
- **`A4b1-r4` remains accepted, pushed and complete.** Not reopened.
- **`r6`'s `R2-UNKNOWN` is terminal for `r6`** and is not reopened or rescored.
- **Expected outcome of the fresh run, stated in advance:** `R2-EXPL-INPUT` or `R2-UNKNOWN` — not
  `R2-PASS`. The Session will report whatever the run actually selects, by the packet's own rows.
