# Turn start plan — title-005

Turn Planner, `claude/claude-opus-5-5` @ `high`. Baseline for the turn; do not rewrite it after execution starts.

## Objective (user)

"Explain and clear the present ceiling, then reach the title screen (M15)." M15 = a title-frame BMP whose hash is
neither `5bdaea576b8509f5` nor `87683a748e27d071`, the run record's ledger IDs, an xemu comparison, and Turn
Reviewer reproduction.

## State and blocker (game `5e7a1a3`, toolkit `dc04dc0`; nothing yet run on Windows)

Plan "Current work" and TR §19 hold the state: the boot reaches the disclaimer, then presents stop at 1000/888. The
working hypothesis is a rejected walk (L39/L40), and it is not yet established. **Three facts the Planner read from
the archive with `Select-String` (unreviewed; W2 verifies them):**

1. **Every title-path run since `20261003-205636-600-f4b-fcmov-fix-150` (49 of 50 through f13) rejects at
   submit #12:** `diag=unsupported_method method=1810 subch=0 param=03000000 at=00008EF0`. That is within the
   first ~10 s, before most flips; logging stops at #63 with GET still `0x8EF0`. `0x1810` is NV097 `DRAW_ARRAYS`.
   It is **not** in `nv2a_method_table.c` (only `0x1818` is), yet `nv2a_pb_exec.c` already handles it.
2. **All five target dumps end at the same GET `0x4DDBC`**, each with a different PUT. The decode at that GET
   differs from run to run (three different `reserved_opcode` headers and one `invalid_target`): **the same ring
   address holds different bytes in each run.** `budget_exhausted` appears 0 times in all five.
3. Flips kept rising after #12 (f13: 9 → 840). So the walk committed again after #63 and GET left `0x8EF0`, but
   there is no log line for it.

**Reading (inference).** The old live mirror let D3D wrap the ring and overwrite the stuck bytes at `0x8EF0`, and
PUT-triggered re-walks then parsed later frames. So everything after SEGA may have rendered from a ring that was
overwritten and possibly mis-parsed. The 1000/888 ceiling would be where that rescue failed (`0x4DDBC`), which makes
TR §19's count change a timing property. **Prediction:** the new mirror stops at #12 with the guest in `0x1914F0`,
and the `RECOMP_FENCE_MIRROR_LIVE=1` A/B reproduces about 888.

## Proposed execution order (changes from the user's order are marked)

0. **Sync, build and test, as prescribed.** Report the counts. `kernel_file_apc_test` runs for the first time;
   `jsrf_nv2a_registers` runs its USER-unreadable-page assertions for the first time. A failure stops the turn.
1. **Fence-ordering audit; the Planner did a first pass.** `0x1912A0` (`CDevice_KickOff`) has 5 direct callers
   and no `jmp`/immediate references:
   - `0x191431`: in `D3D_SetFence`, after `[esi+0x30] += 2` at `0x191429`. Safe.
   - `0x19167C`/`0x191694`: in `MakeRequestedSpace`. The second follows `SetFence(1)`.
   - `0x192440`/`0x1925C3`: init and idle paths.
   `D3D_SetFence` has 9 callers; with flag `2` it skips the kick. W1 completes the audit. Also check KickOff's
   IsBusy spin at `0x191311`, which runs when `[0x19DED0] != 0` (it is 0 in f13).
2. **Trimmed (changed).** These dumps were made with the old mirror (fact 2), so do not decode `0x4DDBC`. One row
   per run: mapping gate, `budget_exhausted` count, first reject line, final GET/PUT, `gpu-report` status, and the
   main thread's top frames. The table confirms facts 1–3; it does not locate the blocker.
3. **`just title-run title005-ceiling 300`, then the live-mirror A/B.** If the new mirror stops at #12, judge the
   `title.adx`/APC check and stop 28 (`0x81860`) from the **A/B** run, and record them NOT EXERCISED in the first.
   On the new-mirror dump, the first reject line, `g_nv2a_submit_state` and the `gpu-report` decode should all
   agree, because GET now holds the original bytes.
4. **Branch.** For `unsupported_method 0x1810`:
   - Regenerate from the new-mirror dump's genuine ring with `gen-nv2a-method-inventory.py --put=`.
   - In parallel, make one `RECOMP_NV2A_ADMIT_UNKNOWN=1` run with the live mirror **off**.
   Classify each method as state or action. `DRAW_ARRAYS` acts through the executor, which already handles it.
   Rerun with **neither switch**. Expect 2–4 cycles, one scene at a time.
5. **New scene, then M15.** Read `[FBPRESENT]` and `[FBPHASE]` before trusting the window. Get the xemu reference
   early (W4).

## Worker delegations (disjoint; the Orchestrator owns every game run, one at a time)

- **W1, static (read-only).** An ordering table for every `SetFence`/`KickOff`/`BlockOnTime` caller and each wait
  site (`0x1914F0`, `0x191311`, `0x191710`): which word each wait polls, and whether the new mirror can hold it
  below target while GET == PUT.
- **W2, archive (read-only).** The step-2 table plus verification of facts 1–3, including the first evidence of
  GET leaving `0x8EF0` (the flips series against time).
- **W3, once the missing-method list exists.** State/action classification from xemu `pgraph` and the nv2a docs,
  plus a minimal implementation and fixture for each action method. No table edits.
- **W4, any time.** An xemu title screenshot from the owner's local assets. Nothing enters either repository.

## Deciding measurements

- **The sharpest discriminator is the new-mirror run's first `[PFIFO] reject` line, checked against
  `g_nv2a_submit_state` and the `gpu-report` decode at `at=` in the same dump.** Back-pressure keeps the rejected
  bytes at GET. Three-way agreement on `unsupported_method`/`0x1810` at `0x8EF0` establishes the hypothesis. A
  different diagnostic, or disagreement, sends the turn down that diagnostic's own branch.
- **False stall against true idle** (no reject line, GET == PUT, guest at `0x1914F0`): compare the published
  `[[ring+0x34]]` with the live `[ring+0x30]` and the BlockOnTime target (ring = `MEM32[0x19DCE0]`). Published
  below the target while live is at or above it means the mirror manufactured the stall.
- **A/B:** the live mirror at about 888 and the new mirror stopping at #12 confirm the overwrite-rescue reading.
- **Cleared:** with no switches, GET passes `0x8EF0` and presents exceed 888.

## Competing hypotheses

- **H1 (leading):** missing methods, starting with `0x1810`.
- **H2:** `0x4DDBC` is a mid-packet re-walk artifact of the overwritten ring, which vanishes once #12 is fixed.
- **H3:** a guest wait unrelated to the GPU (f9: `NtDelayExecution` under `0x13F80` via `0x6FA3C`).
- **H4:** a stall made by the mirror, or a new GPU-completion wait (vblank or flip-stall) exposed once ring
  consumption is faithful. `RECOMP_NV2A_ACTIONS` is not set by `title-run`.
- **H5:** run-to-run variation (f25/f26). Never conclude from one count.

## Advisor consultation points

- Whether the live mirror is acceptable as a ledgered stepping stone while methods are admitted, if the faithful
  mirror stops at #12.
- The state/action classification of each method, and whether admission needs behaviour beyond capture.
- Any `0x1914F0` stall with no reject line (H3 against H4).
- Before claiming M15, if the frame shows only a clear colour.

## Risks the prescribed order under-weights

- **The new mirror will look like a regression:** the predicted stop at #12 gives far fewer presents than 888. It
  is a faithful rejection that was always there. Do not revert `dc04dc0` on the count; revert only on H4 evidence.
- **Step 1 is a false-ceiling check that may leave no reject line.** A flag-`2` fence with no later PUT, a kick
  that samples `[dev+0x30]` before the increment, or the IsBusy spin would each look like "the guest stopped
  submitting". The 9 `SetFence` sites have not all been read.
- **The step-2 dumps cannot show the original rejection** (fact 2). The #12/`0x1810` rejection is in the logs
  only, and the final one at `0x4DDBC` cannot be recovered from any archived dump.
- **The step-3 regression checks need the A/B run**, because an early stop never reaches `title.adx` or stop 28.
  Do not touch `config/stop-chain.json` without a run that reaches `0x81860`.
- **A table regenerated from any old-mirror dump** would admit methods taken from overwritten, mis-parsed bytes.
- **The M15 hash test is weaker now:** `presenting targeted` can change the hash without any guest progress. Pair
  the hash with the xemu comparison and guest-phase evidence.

## Completion criteria

- Step 0 counts recorded. The ceiling **explained** in the TR, with the three-way agreement from a new-mirror run
  and the A/B result.
- Each `dc04dc0` behaviour judged on Windows with evidence: the mirror and retry, present fallback,
  `STATUS_USER_APC`, heap merging.
- The ceiling cleared with no exploratory switch, and every admitted method listed with its class.
- Either M15 (hash, ledger IDs, xemu, Reviewer reproduction), or the next blocker in the plan's "Current work" and
  "Next actions". Both repositories committed and pushed, toolkit first.
