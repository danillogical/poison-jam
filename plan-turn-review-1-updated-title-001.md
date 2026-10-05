# Remediation plan — turn `title-001` (Review 1 returned `FIX`)

Turn Planner artifact per `docs/agent-workflow.md` §2. **Guidance, not an approval gate.**
The Orchestrator owns remediation and may modify this file with the same `PLAN_CHANGE` form
when evidence requires.

Written against HEAD `24d6970`. **Remediation is already partly done**: `4c61042` and
`24d6970` landed while this plan was being written, and B2, B3, B6 and most of B1 and B4
are already corrected in the tree. This plan therefore records (a) what is genuinely
outstanding, and (b) the verification each correction needs so the turn can close on
evidence rather than on the claim that it closed.

## 0. Scope and the one thing that must not happen

**Preserve all landed engineering.** The review is explicit and I confirm it: every span
touched this turn is byte-correct, and **no verified span is to be reopened**. That
includes the ten dispatch entries, the twelve-span batch (65 trap sites), `0x00054750`
plus its `remove-54750-stub` patch, `0x00089A60`, and the two self-found corrections in
`4cac35c`. Nothing below asks for a span to be reverted.

**Everything still outstanding is a record/evidence correction, except §3.** The classes are
marked `[RECORD]` and `[CODE]` throughout. The only behaviour item left is a decision about a
gate's baseline, not a fix to a verified span.

## 1. Already remediated — verify, do not redo

These were corrected in `4c61042`/`24d6970` while this plan was in flight. I reproduced
each; they need no further work, only the confirmation named.

| # | Status | Smallest confirming measurement |
|---|---|---|
| B2 | **Done.** `0x32610` is now recorded as slot 1 of `0x1EC200`; `0x1EC0F0` ends at the `0x1EC1FC` zero terminator. | `python -X utf8 scripts/inspect-jsrf.py disasm 0x1EC1F0 0x1EC210` → `0x1EC1FC = 0`, `0x1EC200 = 0x30660`, `0x1EC204 = 0x32610`. Also a whole-file dword scan for `0x00032610` returns **exactly one** hit, at VA `0x1EC204` (file `0x1DCAA4`, `.data`). |
| B3 | **Done.** `f27` credited; the "run-confirmed" wording no longer rests on `f25`. | `Select-String -Path logs\runs\*f25*\jsrf_run.log -Pattern '0x00032610 returned'` → **0 hits**; the same against `*f27*` → 1 hit at L104350. |
| B6 | **Done.** `plan-turn-updated-title-001.md` regenerated from the committed manifest. | The file now reads `0x32610..0x3275F`, twelve `ret 0xc`, and credits `f27`. |

**One residual `[RECORD]` — B4 is not fully closed.** The regenerated loss list
(`plan-turn-updated-title-001.md` §Status, the sentence beginning "`recovery-unresolved.json`
losses `d77474d` -> HEAD, measured") is now the measured set *except* that it omits
**`0x00089A60`**, which is a genuine `d77474d`→HEAD loss (present at `d77474d`, absent at
HEAD, removed by the `0x89A60` recovery this turn). It is the one entry the plan's own
blocker table row 12 records as a new recovery, so the two statements in the same file
disagree. The list also still contains `0x00032610`, `0x00033800` and `0xFFC00000`, which
are *not* keys of that file — harmless as prose, but they make the list stop being a
set-equality claim.

- **Fix:** add `0x00089A60` to the list; either drop the three non-keys or label the list
  as "losses, plus the addresses discussed in this section".
- **Smallest measurement:** `python -c "import json,subprocess;
  a=json.loads(subprocess.check_output(['git','show','d77474d:config/recovery-unresolved.json']));
  b=json.loads(subprocess.check_output(['git','show','HEAD:config/recovery-unresolved.json']));
  print(sorted(set(a)-set(b)))"` → must equal the plan's list, as sets.

## 2. B1 — `[RECORD]` the disputed finding, and the one part of it that is not in dispute

**I side with the Orchestrator on the factual core, and I reproduced it independently.**
The review's B1 tested *committed* revisions; the `sol-6.1` spelling lived in the
**uncommitted working tree** at session start. I recovered that tree state from the session
record itself (the Orchestrator's first read of the file) and ran the real checker against
it: `MISS codex/sol-6.1` / `roster_route_not_allowed`, **exit 1**. The working-tree
document also differs from `d77474d` in exactly the way B1's own diff describes — it is an
owner-prepared §1–§7 rewrite carrying the Orchestrator and Workers rows. So:

- **The checker did fail on the session-start roster** (exit 1, reproduced). B1's core
  claim, as worded ("the checker exits 0 on the revision it blames"), is true of the
  *commit* and false of the *tree the session actually had*. Both statements are correct
  about different objects, which is the whole substance of the disagreement.
- **The two narrower points in B1 stand and are already recorded**: the checker is not in
  the `check:` recipe (it has its own `route-check`), so "so `just check` was RED" was
  inaccurate; and `7e683a3` published pre-existing owner-prepared Orchestrator and Workers
  rows under a "typo fix" message.

**What is still owed is not more argument — it is the record of the disagreement.** The
Advisor ruling was requested and (per `24d6970`) has been applied. Two `[RECORD]` items:

1. **The remediation must not record B1 as "refuted".** `4c61042`'s message says
   "PARTIALLY UPHELD" and `NOT UPHELD: 'the route never existed'…", which is the right
   shape. Keep it. A finding whose factual core was reproduced must not be closed as
   refuted just because the review's *test* was aimed at the wrong object.
2. **The session-start tree state is the load-bearing artifact and it is not durable.**
   Nothing in the repository proves the dirty tree existed; a future reader repeating B1's
   reasoning will reach B1's conclusion. Record the proof where it survives: cite that the
   session-start `git status` showed ` M docs/agent-workflow.md` against `d77474d`, and
   that restoring the `sol-6.1` spelling into the committed document yields exit 1. That
   is already in `plan-jsrf-bare-minimum.md`; make sure the same sentence, or a pointer to
   it, is in the turn plan, since `docs/agent-workflow.md` §2 makes the turn plan the file
   the next session reads first.

- **Smallest measurement:** reconstruct the checker failure from the committed document —
  substitute `sol-6.1` into §1 of a scratch copy of `docs/agent-workflow.md`, run
  `python scripts/check-route-allowlist.py` against it, and record `exit 1` plus the exact
  `MISS` line. That is reproducible by any later reader without the session record.
- **Owner action, not Orchestrator action:** the §1 roster rewrite is owner-reserved
  (§7). The disclosure is now present; it needs the owner's confirmation, and that is
  outside the Orchestrator's authority to close.

## 3. B7 — `[CODE]` the gate now exists; its baseline needs one more decision

*(The gate wiring is `[CODE]` and is done. What remains is a `[RECORD]` question about what
the baseline claims, plus an optional `[CODE]` follow-up if the Orchestrator chooses (i).)*

The Orchestrator has wired `check-entry-extents.py` into `check:` with a
`--baseline config/entry-extent-baseline.json` allow-list (`4c61042`). I reproduced
`just check` → exit 0, `PASS: no new TRUNCATED entries (19 known and reviewed)`. That
resolves the *mechanical* half of B7: the detector is now in the gate, and a **new**
`TRUNCATED` entry fails.

**The remaining question is whether the 19 baselined entries are all genuinely benign.**
The review's framing was "19 pre-existing `TRUNCATED` entries, none of them a turn entry",
and the baseline was built on that. I tested it, and the class is **not homogeneous**:

- All 19 declared ends are themselves function-entry starts (the repo's `end` == next
  entry's start convention holds), and the checker's *suggested* end is an entry start in
  **0 of 19** — so blindly applying the checker's "fix end to 0x…" advice would move every
  one of them into the middle of the next function. The baseline is right to suppress the
  suggestions.
- **The 19 are not one class, and at least one is a live defect.** I ran two independent
  tests and they agree on the strongest case and disagree on the rest — so I report both
  rather than a single number.
  - **Test A (decisive): the generated body.** The lifter emits the body's last block as
    `g_seh_ebp = ebp; sub_<end>(); return; /* fallthrough 0x<end> */`. That block is
    reachable exactly when the statement before it does not terminate control (an
    `if (c) { …; return; }` with no `else` still falls through when `c` is false). This test
    has jump tables resolved, so it is the better oracle. It flags **7** of the 19.
  - **Test B: x86 recursive descent** from the entry over the real bytes. It flags **1**
    (`0x000307A0`). It under-reports by construction — it cannot follow
    `jmp dword [eax*4+0x2d224]`-style jump tables, which is exactly how `0x2D080` reaches
    its cut.
  - **`0x000307A0` is confirmed live by both.** Declared end `0x307F8` cuts
    `test esi, esi` (`85 F6`) at `0x307F7`; the `je 0x30800` at `0x307F2` falls through to
    `0x307F4 → 0x307F7`, and the body emits `sub_000307F8(); return;` there — a symbol the
    analysis DB records as `detection_method=imm_ref_target`, decoded from mid-instruction.
    That is the `0x00178F40`/`0x32610` failure mode at a live site.
  - The other 6 Test A flags are `0x2D080`, `0x37480`, `0x5CB90`, `0x64E30`, `0x121980`,
    `0x127810`. **The Orchestrator should arbitrate these per entry** before acting; I would
    not treat my count as final. The 12 Test A does *not* flag are benign: 11 emit their
    fallthrough only after an unconditional `esp += N; return;`, and `0x000EB740` emits no
    fallthrough block at all.
- **All 19 are pre-existing and unchanged this turn.** At `d77474d` and at HEAD each has the
  same `end` and the same `stack_args`. So the review's sentence is literally right: no turn
  entry is among them. They are nonetheless now *frozen as "known and reviewed"* by a gate,
  which is a stronger claim than "pre-existing" — and at least `0x000307A0` does not support
  it.

- **Fix (choose one; the tradeoff is genuine and I am not picking for you):**
  - **(i)** Fix the live entries the way the Orchestrator has been fixing this class all
    turn — correct `end` to the real instruction boundary — then re-baseline to those that
    remain benign. Highest value, and each needs its own decode before landing. Note the
    checker's suggestion is safe for an entry with a live fallthrough *because* the cut
    instruction belongs to it; for the benign ones the suggestion would swallow the next
    function, so a blanket "apply the suggestion" pass would be wrong.
  - **(ii)** Keep the baseline but **annotate it**: record per entry whether the cut is
    live or trailing, and register the live ones as known open defects (the same shape as
    `tests/test_recovery_span_ownership.py`'s `KNOWN_OPEN`, which already tracks this class
    and shrank 80 → 66 this turn). Cheap, honest, and stops the file from reading as
    "19 reviewed and fine".
  - **(iii)** Ship the gate as a **non-failing report** and say so in the records, which
    was the review's own fallback option.
  - **My recommendation, weakly held:** (ii) now, with the live entries named — preferably
    by adding them to `KNOWN_OPEN` rather than inventing a second list, so the repository has
    one register of open boundary defects instead of two. It is the smallest change that
    removes the false claim ("known and reviewed") without spending this turn's remaining
    budget on a batch of fresh span fixes, and it leaves the fix decision open on evidence.
    (i) supersedes it whenever the Orchestrator takes it.
- **Smallest measurement:** the live entries are identifiable in one command — for each
  baseline entry, read its generated body in `src/recomp/recovered/recovered.c` and check
  whether the statement before the `/* fallthrough 0x… */` line ends in `return;`. Helper
  scripts are in `%TEMP%\revplan\` (`final.py` for Test A, `x86_reach.py` for Test B); they
  print the two disagreeing sets, which is the honest starting point. For the single
  confirmed case: `python -X utf8 scripts/inspect-jsrf.py disasm 0x307A0 0x30800` shows
  `test esi, esi` starting at `0x307F7` with the declared end cutting it at `0x307F8`, and
  the generated body shows `sub_000307F8(); return;`.
- **Note:** `check-span-exits.py` is still in no gate. It exits 0 today with 362
  `CUT-TARGET` findings, so it is a report by construction — that is a legitimate choice,
  but it should be *stated* in the records rather than left implicit, since B7's point was
  that an ungated detector makes a green gate mean less than the record implies.

## 4. B5 — `[RECORD]` residual in the regenerated turn plan

`plan-jsrf-bare-minimum.md` carries the B5 correction, but the **regenerated
`plan-turn-updated-title-001.md` reintroduced the original claim**: at L91-92 it still
says "f24 on the **same binary** ran 520 s", and at L331 the Blocker 11 table is still
headed "Three runs on the **same binary**" over f23/f25/f26 — which is false for f23
(`44c39546…`) against f25/f26 (`ca867957…`). The genuine same-binary pair is f25/f26.

- **Fix:** apply the same correction in the turn plan that the bare-minimum plan already
  carries; make the Blocker 11 header name the actual pair.
- **Smallest measurement:** `python -X utf8 -c "import json; [print(r, json.load(open(
  'logs/runs/%s/metadata.json'%r))['exe_sha256'][:16]) for r in
  ('20261005-011634-413-f23-7da30','20261005-020708-928-f24-repro',
  '20261005-034216-016-f25-32610','20261005-034914-266-f26-long')]"` → four hashes, two
  distinct.

## 5. Ordering

By dependency, then diagnostic value:

1. **§4 (B5 residual)** — one-line, no dependency, and it is in the file the next session
   reads first.
2. **§1 (B4 residual `0x00089A60`)** — one-line, same file, closes B4 completely.
3. **§2 (B1 record)** — mostly done; add the durable pointer and keep "partially upheld".
   The owner confirmation is the only genuinely open part and it is not the Orchestrator's
   to close.
4. **§3 (B7 baseline)** — decide (i)/(ii)/(iii), then annotate or fix. Do this last because
   it is the only item that can turn into new engineering.
5. **Re-run the load-bearing validation on the final tree** (§6) and commit/push per
   `AGENTS.md`.

## 6. Closing the turn on evidence

Remediation is not complete because the findings were edited; it is complete when the
corrected tree is re-measured. Run these against the **final committed tree**:

- `just check` → exit 0, "all checkers passed" (now including the entry-extent gate).
- `just test` / `ctest --test-dir build -C Release` → 35/35 on an idle host (the
  load-artifact caveat is already recorded and reproduced).
- `python -X utf8 scripts/check-span-exits.py` → record the count actually observed
  (362 at HEAD, not the 363 in the plan; the difference is the `0x89A60` recovery).
- `python -X utf8 scripts/check-generation-provenance.py --check` → `ok : True`, and the
  measured `recovered.c` sha256 equals the committed manifest value.
- Set-equality check for the `recovery-unresolved.json` loss list (§1) and the exe-hash
  check (§4).
- **No verified span reopened:** `git diff d77474d..HEAD -- config/recovered-functions.json`
  should show only additions, end corrections and `stack_args` corrections. Measured at
  HEAD: **9 added, 0 removed, 18 modified** — every modification is an end correction or a
  `stack_args` correction, and the single *tightening* (`0x47820` `0x47970 → 0x47849`) is
  the intentional split that gives `0x47850` its own entry, not a revert. No entry holds a
  value it held at an earlier commit. That is the one assertion this whole remediation rests
  on and it is cheap to check.

## 7. Findings I think the Orchestrator should re-examine

1. **B1 is right that the finding was reproduced; the Orchestrator should not let
   "partially upheld" shade into "the reviewer was wrong".** The review's test was aimed at
   committed revisions and the route lived in the dirty tree — but the failure was real,
   the checker did exit 1, and the commit *did* publish owner-reserved rows under a
   "typo fix" message. That last part is the substantive finding and it is not weakened by
   the tree-state correction. The current wording handles this; the risk is drift if it is
   summarised later as "B1 refuted".
2. **B7's baseline was built on a premise I could not reproduce in full.** "19 pre-existing
   `TRUNCATED` entries" is accurate as a count, but they are not one class. At minimum
   `0x000307A0` has a live fallthrough into a symbol decoded from a mid-instruction address,
   which is the very defect the detector exists to find; my two tests disagree on how many
   others share that property (7 vs 1), and that disagreement is itself the finding — nobody
   has established that these 19 are benign. Baselining them as "known and reviewed"
   overstates what was established, and the gate will now *enforce* that overstatement.
3. **The review's own framing understated B7's importance and the Orchestrator's fix
   understates its risk.** The review called it "a green gate that does not include the
   relevant checker"; the deeper point is that wiring a detector in with a baseline
   converts 19 unaudited entries into 19 *audited* ones. The distinction between
   "suppressed so the gate can pass" and "reviewed and found benign" needs to survive in
   the artifact.
4. **B4 is not closed.** The regenerated list is closer but still wrong by one key
   (`0x00089A60`), and it mixes losses with addresses that are not keys of the file. Given
   B4's original defect was exactly "names a key that never existed and omits keys that
   were removed", this is the same class recurring in the correction.
5. **Minor and not worth a finding, but worth one line:** the plan still cites
   `check-span-exits.py` findings as 431 → 363; the measured value at HEAD is **362**. The
   difference is the `0x89A60` recovery. Re-measure rather than carry the number forward.

## 8. What must not change

- No verified span reopened; all ten dispatch fixes, the twelve-span batch, `0x00054750`
  and `0x00089A60` stay as landed.
- No `JSRF_ALLOW_UNRESOLVED`, no guessed stack corrections, no synthetic advancement.
- The `0x13FAD0` split (`d77474d`) and the toolkit `title.adx` APC fixes stay untouched.
- The title screen is **not** claimed: no title-frame BMP exists, so M15 is not reached,
  and no disclaimer-cleared claim is made from a present count or a hash change.
