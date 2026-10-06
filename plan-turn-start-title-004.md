# title-004 — independent starting strategy

Immutable Turn Planner baseline (`docs/agent-workflow.md` §2/§4). It guides; it does not approve, freeze
scope or gate anything. The Orchestrator owns `plan-turn-updated-title-004.md` and may depart on evidence.

## 1. Objective

Continue down the critical path toward the **title screen (M15)** for an ~8-hour turn. Do not stop at
the first repaired defect, the first run, the first commit, or the first progression. The user's
starting order: **A** safety invariants (rename, dispatch count/array guard, `0xB06E0`, a
certified-continuation record gate); **B** a structural misdispatch census; **C** exercised evidence for
stop 21 `0xB5EB0`; **D** stop 22 `0x48DB0` and beyond. Evidence may change that order, and §3 argues it
should.

## 2. Measured state at plan time (game `df8ea02` + the staffing edit; toolkit `6e6e056`)

The Planner re-measured these before writing. They are leads for the Orchestrator to re-check, not proof.

| Fact | Measurement | Status |
|---|---|---|
| `just check` | green in 38 s; stack-depth **1946 PROVED / 1073 UNKNOWN / 138 SUSPICIOUS** (70 CUT_EPILOGUE, 25 FALL_OFF_END, 39 RET_DEPTH, 2 TAIL_CLEANUP, 2 TRUNCATED) over 3157 entries; hidden-entry 2890 CLEAN / 265 OVER_RUN / 2 UNQUALIFIED; table targets 68 (4 swallowed, 64 uncovered) | observed |
| **The rename broke run-profile classification, and no gate noticed** | `check-run-profile.py --all`: **0 strict, 34 exploratory, 99 UNKNOWN** out of 133. Every newer archive fails with `save-root identity is missing, conflicting, or not verified`, because `metadata.json` stores absolute `C:\Users\logic\Repos\my_xbox_game\...` values (`save_root.*`, `xbe_path`, `cwd`, `command`) and `jsrf_run_profile.py:686-697` and `:233` compare them with the relocated `archive_dir.resolve()`. If the comparison is made with the old root mapped to the new one, the result is 101 exploratory, **26 strict**, 3 fixture and 3 UNKNOWN. **All 26 strict archives are currently unreadable as strict.** | observed (simulation run in a scratch interpreter; no file changed) |
| Other rename leftovers | `df8ea02` already made the scripts root-relative. What remains is historical (`docs/reviews/**`, packets, operating history), untracked root scratch logs (`jsrf_run.log`, `xbox_kernel.log`, `step5_regen.log`), and stale CTest dashboard `.vcxproj` files in `build/` dated 2026-09-12. `CMakeCache.txt` already points at `poison-jam`. The toolkit's `docs/GETTING_STARTED.md` uses `my_xbox_game/` as a **generic template name**, not as this checkout. | observed |
| Dispatch count | toolkit `translator.py:3508` emits the literal `static const size_t g_recomp_table_size = {len(translations)};` and the array is a complete static array in the same TU. The same literal-count shape occurs a second time: `g_recomp_alias_icall_entries = 134u`, `g_recomp_alias_icall_hits[134]` and `g_recomp_alias_seen[134]` against `g_recomp_alias_icall_map[][2]`. | observed |
| Stop 22 `0x48DB0` | Manifest `[0x48DB0, 0x48FBB)`, `stack_args` **absent (0)**. `0x48FBB` is the **middle of an instruction stream** (`jge` at `0x48FBB`, a `tail_jump_alias` database start). The real body runs to its single `ret 8` at `0x496B4`. A guarded jump table follows at `0x496B8`: `and ebx,7; cmp ebx,7; ja; jmp [ebx*4+0x496B8]`, giving **8 dwords then `0x90909090`**. The prologue (`sub esp,0x98` plus four pushes) accounts for exactly the observed `-0xA8`. The emitted body calls abort stubs `sub_00049669` and `sub_000496AC`. **So it is both an extent error and a `stack_args` error, and on this evidence `stack_args` should be 8.** | byte-observed; candidate repair `INFERRED` until validated by the walk/gates |
| Same vtable as stop 22 | `.data` table `0x1F97B4` (the `0x496E0` family, TR §12): slot `0x1F97B8` = **`0x48690`**, a real-looking prologue with **no span and no dispatch** (also in table-target UNCOVERED). One aligned dword. | observed; a strong next-stop candidate |
| `0xB06E0` | Manifest `[0xB06E0, 0xB0811)` with `stack_args 4`. It is already `SUSPICIOUS/CUT_EPILOGUE` in the stack-depth report. Database starts inside its real body: `0xB0811` (only reference is a dword in the `DSOUND` section) and `0xB090B` (only reference is in `$$XTIMAGE`; dispatched to the **alias shim** `recomp_alias_000B090B → sub_000B0A20`). The real epilogue is `0xB09DC`, then a `0xB09E0..0xB09F8` data/pad run, and the next entry is `0xB0A00`. | observed |
| **`0xB06E0` sits in the same virtual slot as live code** | Sibling vtables at `.rdata 0x1CD7F8 / 0x1CD838 / 0x1CD878 / 0x1CD910` all share slots 4–6 (`0xAECC0`, `0xAEE80`, `0xAE660`). Slot 3 is **`0xB06E0` / `0xB2D20` / `0xB3C30` / `0xB5EB0`**. `0xB2D20` returned in g07 and g08 just before both terminal events, and `0xB5EB0` was g07's stop. Shared slot 5 `0xAEE80` also calls an abort stub (`0xAF067`). | observed; the reachability of `0xB06E0` is `INFERRED`, not proved |
| **Why `0xB5EB0` did not run in g08** | The last 15 first-time `[RECOVERED] … returned` lines of g07 and g08 are identical in order, ending `0x000B2D20`. Then g07 calls `0xB5EB0` (ICALL history: `0x153C50` ×7 → `0xB5EB0`) and g08 hits the `0x48DB0` ABI failure, which **aborts**. So g08 may have died *before* it would have reached `0xB5EB0`. | observed sequence; the causal reading is `INFERRED` |
| Trap-call population (rough, Planner's own, **not for citation**) | 139 recovered bodies call 209 distinct `recomp_stubs_recovery.c` abort stubs. In 115 of the 218 caller/target pairs, the target lies **just past the caller's declared end and before the next manifest start** (65 bodies: 42 CUT_EPILOGUE, 6 FALL_OFF_END, 11 UNKNOWN, 5 PROVED, 1 TRUNCATED). Of the 95 CUT_EPILOGUE + FALL_OFF_END entries, **51 call an abort stub**. This is a different population from TR §17's 27/50 (ITAIL-body ∩ trap definitions). | `UNQUALIFIED` — a regex sweep over generated text; it must be reproduced structurally before use |

**Blocker.** Presents freeze at exactly 1000 with the disclaimer hash `5bdaea576b8509f5`. The live stop is
`0x48DB0`. Stop 21 is **NOT EXERCISED**. M15 is not claimed. Run-to-run reachability is highly variable
(g03–g08).

## 3. Where the stated priority order should change

1. **A1 is a real defect, not housekeeping.** The rename silently demoted every strict archive to UNKNOWN,
   and `just check` passed anyway: the same "green suite, uncovered class" shape as the dispatch crash.
   Fix it by **relocation-aware comparison in the classifier**. **Never rewrite archived `metadata.json`**,
   which is evidence that is hashed and cross-checked. Include a control: a fixture archive moved to
   another root must classify the same, and a *mixed*-root archive must still fail.
2. **Stops 21, 22 and `0xB06E0` are one class: an under-wide span that cuts its own continuation or
   epilogue.** That is the mirror of F6, already reported as non-gating CUT_EPILOGUE/FALL_OFF_END. Treat
   A3 and D7 as **one repair batch**, plus `0x48690` from stop 22's own vtable, and point census B5 **at
   this class first**. The alias-shim misdispatch census (TR §12) comes second.
3. **C6 should not be a separate early run.** The evidence suggests that g08 aborted at `0x48DB0` before it
   would have reached `0xB5EB0`. A run on the pre-repair binary mostly re-measures the same abort. Run
   **after** the batch, where one run can test stop 22, stop 21, and possibly `0xB06E0` and `0x48690`.
4. **A4 is ambiguous; name it before building it.** TR §17's *certified lost continuation* is a
   **structural** gate (guard → jump proof, with reachable slots read from file bytes). The user's item 4 is
   a **record** gate (the stop chain must not claim progression its archives do not show). Both are
   worthwhile, but they are different tools. The record gate is cheaper, and its soundness is clearer (§6).
   The structural gate should follow the B5 census, not precede it.

## 4. Proposed execution order (dependency and diagnostic value)

1. **Open and identity (≤15 min).** Commit the staffing edit. Record the hashes of the current
   `build\Release\jsrf_recomp.exe` and of the g07/g08 archived executables, to use as known-good controls.
2. **A1, the rename.** Make the classifier relocation-aware, with the controls above. Re-run `--all` and
   expect roughly 26 strict / 101 exploratory / 3 fixture / 3 UNKNOWN; the Orchestrator must explain any
   difference. Check `check-horizon-ledger.py`, `check-run-exercised.py` (a bare run name resolves to
   `ROOT/<name>`, not `logs/runs/<name>`) and the strict-horizon ledger for path-dependent comparisons.
   Optionally reconfigure `build/` to drop the stale dashboard projects. Leave historical prose alone.
3. **A2, the dispatch guard.** Two independent layers, plus a control:
   - *Derive*: the toolkit template emits `sizeof(g_recomp_table)/sizeof(g_recomp_table[0])`, and does the
     same for the alias arrays. **Forward-compatibility trick:** change the game patch
     `fix-dispatch-table-size` so that its `after` text is **byte-identical** to the new template line.
     `patch-generated.py` then applies it to today's tree, and after a regeneration it reports "already
     applied" (`after`×1, `before`×0) instead of failing. The tuple-removal patch then needs no count
     companion at all.
   - *Check*: a deterministic game-side checker in `just check` that parses `recomp_dispatch.c` and
     compares every literal count or bound with the rows it describes (dispatch tuples; alias map rows
     against `entries` and both `[N]` arrays). It should also check that tuple VAs are strictly
     increasing and fit inside `g_flat_span`, because the binary search and the flat index both assume it.
     This catches old trees, partial regenerations and hand patches.
   - *Control*: a fixture recreating **exactly 8928 declared / 8927 rows** must make the real checker exit
     nonzero. Add a sibling for the alias-count shape and a CTest entry.
4. **Repair batch: the under-wide class (stop 22, `0xB06E0`, `0x48690`).** Derive from the bytes with
   `Analyzer.walk`. For `0x48DB0`: the end covers the `ret 8` at `0x496B4` (decide by recipe precedent
   whether the end includes the `0x496B8` table), and `stack_args 8`. For `0xB06E0`: the end is past
   `0xB09DD`. Prove from the widened walk that `0xB0811` and `0xB090B` are internal labels reached by its
   own control flow; their only "evidence" is dwords in non-`.data`/`.rdata` sections. Decide the fate of
   the `0xB090B` alias tuple, with any dispatch-table edit going through the A2-guarded mechanism.
   `0x48690`: recover it only if it passes standalone liftability and its own walk. For each repair, keep
   a **negative old-span control** and confirm that the stack-depth and hidden-entry gates move as
   expected (no new OVERLAP or SHADOWED findings; the CUT_EPILOGUE/FALL_OFF_END count drops). Then
   `relift-selected.py boundaries`, `recover-functions.py`, `gen-abi-deltas.py`, `just check`, `just test`.
5. **Runtime, in the background while static work continues.** Use the bounded exploratory profile with
   the same settings as g07/g08 and the run's ledger IDs. If the new build dies **before known guest
   progress**, first re-run g08's archived executable in the same environment. Judge each run with
   `check-run-exercised.py` for `0x48DB0 0xB5EB0 0xB06E0 0x48690 0x00154540`. Same-binary repeats are
   worth more than one longer run. A run that does not reach an address leaves it **NOT EXERCISED**.
6. **A4, the record gate (§6), while runs execute.** It uses the existing g03–g08 archives as positive and
   negative controls.
7. **B5, the structural census.** Begin with the under-wide/lost-continuation class, then the alias-shim
   misdispatch class. Keep classes separate (§5). Only after the measurement, decide whether a
   zero-baseline structural gate exists (for example, a cut epilogue **∩** an abort-stub target **∩** a
   fully enumerated in-span guard). Repair batches go by **live-path proximity first**: the sibling
   vtables `0x1CD7F8…`/`0x1F97B4`, including `0xAEE80 → 0xAF067`.
8. **Follow the next stop** with the §4 D7 discipline: preserve the evidence, localize the guest path,
   classify the defect, strengthen a reusable gate if it is an instance of a known class, fix narrowly,
   run static gates first, then do exercised verification. Repeat while budget remains.
9. **Close.** Update the records (Current work, TR, ledger L02 and others as needed, provenance with
   history preserved), commit and push the **toolkit first**, then the game, with receipts. Then the Turn
   Review.

## 5. Useful worker delegations (disjoint files; the Orchestrator owns manifests, builds and runs)

| Worker | Bounded objective | Files | Returns |
|---|---|---|---|
| W-reloc | Relocation-aware classifier plus tests (moved-root passes, mixed-root fails); audit other archive path comparisons | `scripts/jsrf_run_profile.py`, `tests/test_run_profiles.py` | before/after `--all` tallies, observed vs inferred |
| W-dispatch | Game-side generated-table checker plus the 8928/8927 fixture and alias fixture; draft the toolkit template change separately | new `scripts/check-*.py`, `tests/`, fixtures; toolkit `translator.py` on a branch | a failing-control transcript |
| W-bytes (read-only) | Independently re-derive the ends, `stack_args`, table slots and internal-label status for `0x48DB0`, `0xB06E0`, `0x48690`, `0xB090B`, `0xB0811` from `default.xbe` with its own capstone walk | none | a per-address table: `PROVED` / `INFERRED` |
| W-census (read-only) | A structural census of abort-stub targets and alias shims: caller span relation, walk reachability, guard/table proof, independent-entry evidence, vtable membership; separate classes | scratch outside the repo, or a new report script | counts produced by tooling, never by transcription |
| W-record | Prototype the record gate (§6) and its controls on archived g03–g08 | new script, test, structured ledger file | pass/fail on the controls |

Do not run builds or game launches in parallel. Workers report observed vs inferred, and the Orchestrator
re-checks every load-bearing fact.

## 6. Record-gate sketch (A4): evidence, not text coincidence

- Make the stop chain **structured** (for example `config/stop-chain.json`): the stop number, VA, kind
  (`FOUND`, `RUNTIME_CONFIRMED`, `NOT_EXERCISED`), the cited run directory or directories, and the repair
  commit. The plan's prose table cites it and is not parsed.
- A `FOUND` row requires that the cited archive's `jsrf_run.log` hash matches `metadata.run_log_sha256` and
  that the log contains the failure line for that VA (`ABI FAILURE 0x…` or `ICALL] Failed to resolve VA
  0x…`).
- A `RUNTIME_CONFIRMED` row requires the same integrity check plus the `[RECOVERED] 0x… returned; ABI
  verified` line (reusing `check-run-exercised.py`'s parser). The archive's recorded project revision must
  be a **descendant of the repair commit** (one `git merge-base --is-ancestor`), and the archived
  executable hash must match its `build-source.json`. That ties the confirmation to a binary built with
  the repair, not to a filename.
- `NOT_EXERCISED` is legal and explicit. Zero baseline: about 22 rows today, all checkable.
- Controls: g06 cited as confirming stop 20 must FAIL (not exercised); g08 cited for stop 21 must FAIL; g07
  cited for stop 20 must PASS; a pre-repair archive cited as a confirmation must FAIL on ancestry.
- Stated limit: the line is logged once per address and proves the wrapper returned with a correct ABI. It
  does not prove that a *specific* arm ran (for example `0xB5EB0`'s `0xB5F82` case). Record that limit,
  and do not overclaim.

## 7. Deciding measurements and tests

| Question | Deciding measurement |
|---|---|
| Is the rename fully handled? | `check-run-profile.py --all` restored to the per-run classification the archives support; a moved-root fixture passes and a mixed-root one fails; `just check` and CTest stay green |
| Is the dispatch-count class closed? | The real checker exits nonzero on the 8928/8927 fixture and on the alias-count fixture; the current tree passes; after derivation, `patch-generated.py --check` reports the count patch as applied |
| Is stop 22 correct statically? | The widened walk is fully enumerated to `ret 8` at depth 0 (PROVED, or explicitly INFERRED); the emitted body contains no `sub_00049669`/`sub_000496AC` call and has a resolved 8-arm switch; the old span replays as SUSPICIOUS |
| Is `0xB06E0` repaired rather than suppressed? | The emitted body has no `sub_000B09DC`/`sub_000B09D9`; `0xB0811` and `0xB090B` are proved reachable from the widened entry; the hidden-entry gate shows no new OVERLAP/SHADOWED; CUT_EPILOGUE drops by one; an old-span control is kept |
| Was stop 21 or 22 runtime-confirmed? | `check-run-exercised.py <run> 0xB5EB0 0x48DB0 …` PASS on an archive whose executable hash and revision include the repair; otherwise NOT EXERCISED, stated per address |
| Did boot advance? | The present count and frame hash change, or a later stop appears, in matched-profile runs; a new ABI return is **not** title evidence |

## 8. Competing hypotheses to keep alive

- **Dispatch fix location:** derive in the toolkit (it removes the class at its source, but needs a
  regeneration to land) versus a game-side checker (covers today's tree, partial regenerations and
  patches). Recommendation: both, with the forward-compatible patch. A C `static const` is not an integer
  constant expression, so a `_Static_assert` against it would need the sizeof form anyway.
- **`0xB06E0`:** a gap in the hidden-entry detector *or* a different class. On the bytes it is **neither**
  an over-wide span nor the detector's concern: it is under-wide (a cut epilogue), already visible as
  stack-depth SUSPICIOUS. Keep the alternative alive: `0xB090B`/`0xB0811` might be genuine secondary
  entries (dual-entry bodies). Only the widened walk plus an evidence check decides that.
- **Stop 22:** a `stack_args` error, an extent error, or both. The bytes favour **both** (no `ret` within
  the span; the real `ret 8` versus the recorded 0). Keep alive that `0x48FBB` or `0x49520` (database
  starts inside) could be separate entries; the walk decides.
- **Why `0xB5EB0` did not run in g08:** an early abort at `0x48DB0` (favoured) versus genuine
  nondeterminism versus unreachability. Decide with post-repair runs. Optionally inspect the g08 dump for
  `0x48DB0`'s caller and compare it with g07's `0x153C50 → 0xB5EB0`.
- **Record gate soundness:** rows and archives are structured and integrity-checked, so it does not
  degrade into text matching. It would degrade if allowed to cite prose or unhashed logs.
- **The 1000-present freeze:** a symptom of the boot transition (the Advisor ruling of 2026-10-05) versus
  an independent present-path blocker. Do not build a present-path fix on an independence assumption.

## 9. Where Advisor input would help

- The relocation rule for archived evidence. It changes evidence semantics in the classifier, so ask
  which cross-field consistency must survive a root move.
- Whether *cut epilogue ∩ abort-stub target ∩ fully enumerated walk* is a sound **zero-baseline**
  structural gate, or needs the full TR §17 guard proof. Do this after the census numbers exist.
- `0xB090B` and other alias tuples that become internal labels after widening: delete the tuple, redirect
  it to the owner, or keep it.
- Any runtime divergence that contradicts §2's causal reading (for example, the post-repair run still never
  reaches `0xB5EB0`).

## 10. Useful completion criteria for the turn

- **Safety:** archive classification works again after the rename, with relocation controls. The dispatch
  and alias literal-count class is guarded by a checker in `just check` with the exact 8928/8927 negative
  fixture, and is derived at the source or forward-compatibly patched.
- **Static:** stop 22 and `0xB06E0` repaired from the bytes, each with an old-span negative control.
  `0x48690` either recovered or classified with a reason. The census produced by tooling, with classes
  preserved. Either a zero-baseline gate for a proven sub-class, or a written reason why none is gateable.
- **Records:** a record gate exists and fails on the known bad citations (g06 for stop 20, g08 for stop
  21), or a written reason it cannot be made sound.
- **Runtime:** at least one post-repair run archived and judged per address. Stop 21 and stop 22 are
  RUNTIME_CONFIRMED or NOT EXERCISED, never assumed. The next stop is localized and classified, and pursued
  while budget remains.
- **Always:** `just check`, CTest (≥ 38 plus new tests), stack-depth `--selfcheck`, hidden-entry
  `--selfcheck` and the extents gate are green on the final tree. Stack-depth gating stays `d == 0`.
  Hidden-entry proof still needs both halves. No caller-absence reasoning. Pushes go toolkit first with
  receipts. No title or M15 claim without milestone evidence.
