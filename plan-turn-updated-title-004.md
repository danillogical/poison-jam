# Turn plan (updated) — `title-004`

Living execution plan. Owner: the Orchestrator (`workbuddy-ai/deepseek-v4.1-flash` @ high).
Baseline: `plan-turn-start-title-004.md` (immutable). This file records how execution actually
evolved and why.

## Status at last update

Turn `title-004`. Toolkit `2cee914`; game `59f3ebf`. `just check` green; CTest **40/40**;
`test_run_profiles.py` 46 OK; `test_generation_provenance.py` 36 OK;
`test_dispatch_table_size.py` 15 OK; `test_stop_chain.py` 25 OK;
`test_recovery_span_ownership.py` 10 OK. Both repositories pushed and clean.

**The title screen is NOT reached and M15 is NOT claimed.** Presents still freeze at exactly 1000
with the disclaimer hash `5bdaea576b8509f5` unchanged.

| # | Item | State |
|---|---|---|
| A1 | checkout rename, operational | **DONE** — `2a1dbc9`. 26 strict archives recovered from 0. |
| A2 | dispatch count/array invariant | **DONE** — `8e5b93a` + toolkit `2cee914`. Derived count, gate, 15 controls. |
| A2b | `--write` erasing the provenance history | **DONE** — fixed inside `8e5b93a`; 4 controls + byte-identical round-trip controls. |
| A3 | `0xB06E0` | **DONE** — `892dd1e`. End `0xB09E0`; zero trap stubs; counts erratum in TR §18. |
| A4 | certified-continuation record gate | **DONE** — `b1aca89`, ledger L43, 25 controls, both review holes closed in `0763de8`. |
| D7 | stop 22 `0x48DB0` | **DONE and RUNTIME-CONFIRMED** — `892dd1e`, confirmed by f9 (`0445a80`). |
| C6 | exercised evidence for `0xB5EB0` | **DONE — CONFIRMED by f9.** Two runs had missed it; the third executed it. |
| F7b | stop 25 `0xAE655` (census-predicted) | **DONE** — `0445a80`. First stop the census called in advance. |
| F7c | the certified under-wide class | **DONE** — `59f3ebf`. 152 spans widened; SUSPICIOUS 135→74. |
| B5 | structural misdispatch census | open — the alias-shim class, kept separate |
| — | the 7 entries whose `stack_args` disagrees with certified `N` | open — a separate finding, not folded into an extent repair |

## PLAN_CHANGE

- **Changed:** A4 (the certified-continuation gate) is **deferred behind the B5 census**, and the
  user's item 4 was read as **two different gates**, not one.
- **Evidence:** the Turn Planner's independent reading, which the Orchestrator's own census agrees
  with. TR §17's *certified lost continuation* is a **structural** gate: it proves a guard-to-jump
  path from the bytes, reading the reachable slots from file-backed memory. The user's item 4 is a
  **record** gate: it stops the stop-chain record claiming progression the archived runs do not
  show. They are different tools with different proofs, and the structural one is the census's
  output rather than its precondition.
- **Why:** building the structural gate before the census would freeze a rule before the population
  is measured — the mistake the plan already records twice (the 192/234 spelling count, and the
  `N + d` gate that had to stay narrow). The record gate is cheap and its invariant is sound; it is
  the next item after the batch.

## PLAN_CHANGE

- **Changed:** A3 (`0xB06E0`) and D7 (stop 22 `0x48DB0`) are executed as **one repair batch** with
  `0x48690`, rather than as separate units.
- **Evidence:** measured independently by both agents, from the bytes. `0x48DB0`'s declared end
  `0x48FBB` falls **mid-instruction**, and its own walk reaches its real `ret 8` at `0x496B4`; the
  prologue (`sub esp,0x98` + 4 pushes) nets exactly the observed `-0xA8`, so it is both an extent
  error and a missing `stack_args 8`. `0xB06E0`'s declared end `0xB0811` is likewise a
  `tail_jump_alias` start **inside** its real body, and its own epilogue at `0xB09DC` is outside the
  span, so its emitted body calls the fatal stub `sub_000B09DC` four times. Both are the
  **under-wide** class — the mirror of F6 — and `0x48690` is an uncovered pointer in `0x48DB0`'s
  own `.data` table `0x1F97B4`.
- **Why:** one batch derives the extents by the same method and is validated by the same gates,
  and one run afterwards can test all three. Three separate runs would spend the same evidence
  three times.

## PLAN_CHANGE

- **Changed:** C6 (a run to exercise `0xB5EB0`) is **deferred until after the repair batch**.
- **Evidence:** g07 and g08 log the **same last 15 first-time returns in the same order**, both
  ending at `0xB2D20`. g07 then calls `0xB5EB0`; g08 instead aborts on the `0x48DB0` ABI failure. So
  g08 very likely died *before* it would have reached `0xB5EB0`, and a run on the pre-repair binary
  would mostly re-measure the same abort.
- **Why:** one run on the post-batch binary tests stop 21, stop 22 and `0xB06E0` together. The
  reading is `INFERRED`, not proved, and is recorded as such — it changes ordering, not any claim.

## A1 — the rename silently demoted every strict archive

Measured before and after the fix, on the real archive:

```text
before:  checked 133; strict 0,   exploratory 34,  fixture 0, unknown 99
after :  checked 133; strict 26,  exploratory 101, fixture 3, unknown 3
```

All 133 archives record the pre-rename absolute root in `save_root.*`, `xbe_path` and every element
of the recorded collector `command`, because those are the paths the run really used. The classifier
compared them literally with the new location, so **all 26 strict archives read UNKNOWN** and
`just check` stayed green — the project's entire strict-evidence base, lost silently. The Turn
Planner measured the same 26/101/3/3 independently, from a scratch interpreter, before this fix.

The fix is **relocation-aware comparison, never rewriting the archives**: a recorded path is a fact
about the machine the run happened on, and editing it would fabricate provenance. `RELOCATED_ROOTS`
maps exactly this one recorded move; a different root, a prefix-sharing sibling
(`my_xbox_game_backup`) and an embedded-not-leading old root all still fail. Both sides are
canonicalised, so old-vs-new and old-vs-old both compare equal.

Also fixed: `check-run-exercised.py` given a bare run name looked in the repository root, found
nothing, and reported "cannot establish coverage" — a FAIL for a run that exists.

The 3 remaining UNKNOWN are genuinely unverifiable and stay that way: g01 was killed mid-run, g02
failed before `guest_entry` with `[SAVE] root rejected ... not writable`. Both are already recorded
as environment losses, not code results.

## A2 — the dispatch count is derived, and gated

`sizeof(g_recomp_table) / sizeof(g_recomp_table[0])` cannot disagree with the array it measures, so
the 8928-vs-8927 class is structurally impossible rather than merely detected. Three coordinated
pieces: the toolkit emitter (`2cee914`), the game patch `fix-dispatch-table-size` whose `after` is
byte-identical to the new template text (so it reports `already applied` after a regeneration and
still repairs an old tree), and `scripts/check-dispatch-table.py` as the independent gate for trees
from any other route. `tests/test_dispatch_table_size.py` re-injects the exact shipped mismatch.

Also fixed, because it blocked the record update: `write_manifest()` erased the hand-maintained
`amendments`/`regenerations` history on every `--write` (measured four times in one session). It now
carries them over verbatim, refuses a malformed list rather than emptying it, and renders the whole
record **byte-identically** when nothing changed — verified on both real records. Without that, a
two-line generated change would have rewritten 3,200 lines of record.

## The under-wide class: measured population

The Orchestrator's structural census (tooling, not a text sweep) over the 3,157 manifest entries
finds **128 entries whose own fully-enumerated walk reaches a fatal abort stub**, and **150 where a
*conditional* branch leaves the span to an address that is neither a manifest start nor inside any
genuine span, and extending to the next manifest start certifies a single `ret N`** (150 PROVED, 4
INFERRED). 10 of the certified set disagree with their declared `stack_args`.

This is the same class the Planner's independent text sweep pointed at, and it is the mirror of F6:
an **under-wide** span that cuts its own continuation or epilogue. It is **not yet a gate**: the
discriminator between "this function's own continuation" and "a legitimate tail call to a separate
function" is not established for the unconditional-`jmp` subset, which is why the census keeps the
conditional and unconditional populations separate rather than forcing one label.

## Next actions, in order

1. The under-wide repair batch: `0x48DB0` (+`0x48690`), `0xB06E0`. Derive each extent from the
   bytes with `Analyzer.walk`, repair, and re-run the stack-depth, hidden-entry, entry-extent and
   span-exit gates before any run.
2. The certified-continuation **record** gate (A4), with g06 and g08 as citations that must fail.
3. The structural misdispatch census (B5), keeping the alias-shim class separate from the
   under-wide class.
4. One bounded run to test stops 21 and 22 together.

## A4 — the certified-continuation record gate, and the batch's runtime state

`config/stop-chain.json` (8 rows) plus `scripts/check-stop-chain.py` and
`tests/test_stop_chain.py` (21 controls, CTest `jsrf_stop_chain`, `just check`, ledger L43). Each
row cites the archived runs that establish it, and the gate refuses a row that claims more than they
show: a confirming role needs the address's ABI-verified return line **and** a run revision
descending from the row's `repair_commit`; a `not_exercised` role needs that line to be absent; a
`discovered` role needs a defect line or an ICALL-history frame; and a `RUNTIME_CONFIRMED` state
needs at least one confirming role. Every citation also re-checks its run's log hash against that
run's own `metadata.json`, so a citation cannot be satisfied by editing a log afterwards.

**The ancestry half immediately caught a real distinction rather than a hypothetical one.** g05
*discovered* `0x00154540` and does **not** descend from its own repair commit (`f603e1e`), so that
row cites g05 as `discovered` and not as a confirmation — the return line alone would have been read
as one. The same check shows g07 does not descend from `0xB5EB0`'s repair either, so stop 21 could
only ever have been `NOT_EXERCISED` from that run. An unprovable revision is reported UNKNOWN, never
as refuted.

### The batch's runtime state: both stops CONFIRMED by f9

Run **f8** (`20261006-002553-667-f8-underwide-batch`, exploratory, 423 s, `diagnostic_deadline`)
reached `guest_entry` with **zero** `Failed to resolve`, **zero** ABI failures and **zero**
alias-misdispatch lines, and exercised **none** of the repaired addresses. On its own that would have
been NOT EXERCISED.

**But f8 is not a comparable run, and that was my error, not the code's.** It produced only **195**
ABI-verified returns against g07's and g08's **546**, never reached a single `[FBPRESENT]` line, and
its last returns are an entirely different set. The cause is measured, not inferred: g07 and g08
carry `RECOMP_APU_TRAP=1`, `RECOMP_PB_EXEC=1`, `RECOMP_FB_WINDOW=1` and
`RECOMP_FB_PRESENT_DUMP_EVERY=10` in their recorded settings, and **f8 carries none of them** —
`just explore-run` adds no override, and these are inherited from the environment. So f8 is not a
regression and not evidence against the repair; it is a different, much earlier path. This is the
differential-debugging rule applied to a *run* rather than a binary: compare against what the earlier
run actually had, not against what it is remembered to have had.

**The comparable run resolves it.** f9 (`20261006-003520-133-f9-underwide-batch-pb`, exploratory,
257 s, `unhandled_exception`, 550 returns, presents frozen at exactly 1000 with the disclaimer hash
`5bdaea576b8509f5` unchanged) is the first run ever to execute either repaired address:

```text
[RECOVERED] 0x00048DB0 returned; ABI verified (ESP/EBX/ESI/EDI)
[RECOVERED] 0x000B5EB0 returned; ABI verified (ESP/EBX/ESI/EDI)
```

So **stop 21 `0xB5EB0` and stop 22 `0x48DB0` are RUNTIME-CONFIRMED**, and `0xB5EB0` — repaired from
the bytes and then missed by two consecutive runs — is finally discharged. f9 then stopped on
`[ICALL] Failed to resolve VA 0x000AE655`, which the census had already predicted (F7b).

## PLAN_CHANGE

- **Changed:** the runtime verification was re-run with the canonical exploratory override set after
  f8 proved non-comparable, and f8 is recorded as methodologically non-comparable rather than as a
  clean result.
- **Evidence:** the four overrides are present in g07's and g08's recorded `settings` and absent from
  f8's, measured from `metadata.json`; f8 produced 195 returns and no presents against 546 and 1000.
  f9 with those four set produced 550 returns and confirmed both stops.
- **Why:** recording f8 as evidence for or against the batch would compare two different
  configurations, which is exactly the single-variable error `docs/jsrf-run-profiles.md` warns about.
  No claim is made from f8 in either direction, and the run that *is* comparable is cited instead.

## F7c — the certified under-wide class

152 spans widened to their real end in one validated batch (`59f3ebf`), each certified by a fully
enumerated walk whose every exit is one `ret N` at depth 0. Five assertions guarded every write:
widening only; instruction-boundary end; declared `stack_args` equals certified `N`; no other
manifest start crossed; extended walk fully enumerated.

**Eight candidates were skipped rather than guessed at**, and one of them is the strongest evidence
that the assertions matter: `0x43EC0` would have swallowed the manifest start `0x44000` — the
`0xB5EB0` lesson — and assertion 4 caught it mechanically. The other seven have a declared
`stack_args` that disagrees with their certified `N`; that is a separate finding and is tracked as
one rather than folded into an extent repair.

| gate | before | after |
|---|---|---|
| stack-depth SUSPICIOUS | 135 | **74** |
| — CUT_EPILOGUE | 68 | **19** |
| — FALL_OFF_END | 24 | **12** |
| check-span-exits CUT-TARGET | 308 | **225** |
| fatal trap-call sites in `recovered.c` | — | **0** |

`KNOWN_OPEN` shrank 60 → 31, again exactly the measured set — and the stale-member control added
earlier this turn is what caught the 29 that this batch repaired, by failing before the list was
updated. That control has now paid for itself twice in one turn.

## Remaining work, in order

1. **The 7 `stack_args`/`N` disagreements** (`0x202A0`, `0x20420`, `0x204D0`, `0x34070`, `0x38460`,
   `0x705E0`, `0x171B50`): a separate ABI finding, each needing its own evidence.
2. **B5, the structural misdispatch census** of the 134 alias shims, on the Advisor's four
   independent byte-derived axes (position, reachability, shape, evidence), keeping the
   `OUTSIDE_OWNER` class separate — all four named cases (`0xE9A40`, `0x100AB0`, `0x1199C0`,
   `0x13A340`) fall in it, so TR §12's "harmless mid-body label" reasoning covers at most the 75
   `INSIDE` shims.
3. **One more bounded run** to test F7c and reach the next stop.

