# Turn updated plan — title-005

Orchestrator-owned living plan. Start plan: `plan-turn-start-title-005.md`.

## Objective (unchanged)

"Explain and clear the present ceiling, then reach the title screen (M15)."

## Status: the ceiling is EXPLAINED and CLEARED

**Root cause (measured, three-way agreement).** The NV2A submission walk rejected the whole
stream at guest `0x00008EF0` on `unsupported_method method=0x1810`. `0x1810` is
`NV097_DRAW_ARRAYS`; it was absent from the generated admission table (L39) between `0x1808`
and `0x1818`, although the toolkit's executor **already implements it**
(`xboxrecomp/src/kernel/nv2a_pb_exec.c:2759`, `case NV097_DRAW_ARRAYS`). The table was stale
relative to the executor — the `0x1720` class of defect (L39).

Evidence, all on Windows, toolkit `dc04dc0`:

1. **New-mirror run** `20261006-203929-286-title005-ceiling` (`just title-run title005-ceiling 300`):
   `[PFIFO] reject diag=unsupported_method method=1810 subch=0 param=03000000 at=00008EF0 get=00008EF0 put=0000A6D4 successes=15 rejections=1`,
   then `still rejecting n=16`, `n=256`. GET pinned at `0x8EF0`; `[FBPHASE] presents=9 (stalled 1s)`.
2. **`just gpu-report` on the same dump**: `Submit state: last walk unsupported_method; 2695 consecutive
   rejection(s); method 0x00001810`; predicted diagnostic `unsupported_method`; the pending-stream
   table lists exactly one missing method, `class 0x97 method 0x1810 first VA 0x80009ADC`.
3. **The archived old-mirror logs already carried it**: all five target runs log
   `[PFIFO] submit #12 diag=unsupported_method ... method=1810 at=00008EF0` as the first rejection,
   with GET never moving again in the log. This was always the first blocker, not a regression.

**The hypothesis in the brief is CONFIRMED** (`unsupported_method`), and the `GET ≠ PUT` reading
that the guest had stopped submitting is refuted for these runs.

**`RECOMP_FENCE_MIRROR_LIVE=1` A/B** (`20261006-205306-080-title005-ceiling-ab-live`) reproduces the
old behaviour: 24 reject lines, 8 recoveries, walks past `0x8EF0` to a later blocker
(`at=00033D04`, `unsupported_method` then `reserved_opcode`), presents=620 by 296 s. This confirms
the planner's inference: the old live mirror let D3D overwrite the stuck bytes and later re-walks
parsed whatever was there, so an archived dump's ring at GET is **not** the original blocker.

**`RECOMP_NV2A_ADMIT_UNKNOWN=1`** run `20261006-205823-760-title005-admit-unknown` listed **exactly
one** missing method for the whole next scene — `[PFIFO] admit-unknown class=97 method=1810` — with
zero reject lines, `Submit state: last walk ok; 0 consecutive rejection(s)`, reaching
**presents=1760** by 297 s and 10 distinct frame hashes. So the ceiling is exactly this one method.

## The fix

Regenerated the admission table through the sanctioned generator, as the union of the two runs the
inventory already used **plus** the new-mirror ceiling run, each decoded only to **its own**
log-derived ring top:

```text
python -X utf8 scripts/gen-nv2a-method-inventory.py \
  20260922-110235-244-spanfix-1185b0 \
  20260930-230206-594-f4-frames-after-horizon-fix \
  20261006-203929-286-title005-ceiling
```

Set difference vs the committed table: **exactly one method added, `0x1810`; none removed**
(380 → 381; NV097 370). The rejection itself was **not** relaxed (L39).

A determinism control (regenerating from the two original runs alone) reproduced the committed
table byte-for-byte, so the change is attributable to the new run alone.

## PLAN_CHANGE

- **Changed:** the fix is "regenerate the table to include `0x1810`", not "implement an action
  method". The start plan (step 4) expected state/action classification and possibly a new
  implementation.
- **Evidence:** `nv2a_pb_exec.c:2759` already implements `NV097_DRAW_ARRAYS`, including the
  index-run expansion, and says in its own comment that it is "the method this title actually
  draws with". The executor handles it; only the walk's admission table lacked it.
- **Why:** the defect is a stale generated table, not a missing implementation. Classifying it as
  an action method needing implementation would have produced redundant work and obscured that the
  table is generated from measured rings (L39) and must be regenerated when the title submits more.

- **Changed:** step 2 was trimmed per the planner — the archived dumps' ring at GET was not decoded
  as the blocker, because the old mirror had overwritten it.
- **Evidence:** the A/B run shows the walk moving past `0x8EF0` under the live mirror, and the
  planner's finding that the five dumps end at the same GET `0x4DDBC` with different PUTs and
  different decodes.
- **Why:** decoding overwritten bytes would have named a later, non-original blocker.

## Remaining work

- Post-fix run with **neither** switch (`20261006-210559-268-title005-fixed`, 420 s) — **done**:
  0 rejections, 0 admit-unknown, `last walk ok`, presents **2410**. Independently reproduced by
  `20261006-215642-480-title005-confirm` (presents 1680 at 297 s).
- Judge the four `dc04dc0` behaviours on Windows — **done**: the fence mirror holds back-pressure as
  designed (a rejected walk pins GET and leaves the guest in the `0x1914F0` ring-space wait — the
  stack frame at `recovered.c:399957` is inside `loc_001914F0` — with a `[PFIFO] reject` line
  present), and the `RECOMP_FENCE_MIRROR_LIVE=1` A/B reproduces the old behaviour, so the 1000/888
  count was the old mirror's property, not a regression. CTest `kernel_file_apc_test` (its
  `STATUS_USER_APC` assertions), `kmem`, `fence_snapshot`, `nv2a_present_track`, `nv2a_submit_diag`
  all pass; `just test` 44/44; `just check` clean.
- Stop 28 (`0x81860`) exercised? **NO — NOT EXERCISED** by every post-fix run
  (`scripts/check-run-exercised.py`); `config/stop-chain.json` left unchanged.
- M15: **not reached.** The run still ends on the graffiti disclaimer; the BMPs
  (`logs/workers/title005/`) and the `[FBPRESENT]` hashes show no title screen.

## Next blocker (recorded, not fixed this turn)

Two, in order, both now in the plan's "Next actions":

1. Same class as this turn's fix — `unsupported_method 0x0BB0` plus `0BB4`, `0BB8`, `0BBC`, `1724`,
   `1728`, all runtime-confirmed by `[PFIFO] admit-unknown` in `20261006-213505-255-title005-admit3`.
2. A different class — `budget_exhausted` (L40's per-walk word budget), which that run hits once the
   six are admitted; needs walk bounds / incremental commit, no switch.

**Provenance caution for #1:** those later rings have wrapped, so the generator's decode and the
model's walk can diverge (decoding `20261006-211635-913-title005-m15` from `0x1000` stops with
`bad_target 0x00100000` and would inflate the table by a dense `0x1848`–`0x18F8` run the real walk
never required). Take a new entry only from a decode that **reached PUT** or from an
`admit-unknown` line. The committed `+0x1810` fix satisfies this: its source run reached PUT.

## Completion criteria (from the start plan, unchanged)

Step 0 counts recorded; the ceiling explained in the TR with three-way agreement and the A/B;
each `dc04dc0` behaviour judged on Windows; the ceiling cleared with no exploratory switch and
every admitted method listed; either M15 or the next blocker recorded. Both repositories committed
and pushed, toolkit first.
