# A4s-r5 execution startup — Advisor ruling (Q1, Q2) and owner override

**Advisor:** child `5c555969-dea9-4b47-be05-62aa0835cde2`, route `claude/claude-opus-5-5` @ `high`
(the persistent Advisor probed at startup in `docs/reviews/startup-20260925-session-58e86358.md`).
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Questions:** Q1 — the scope of the Kimi K3 Planner-route effort finding; Q2 — whether P0.8's literal
failure stops execution and how it interacts with §0.6's receipt write and the owner's
`docs/agent-workflow.md` replacement.

---

## Q1 — Planner route: **SUPERSEDED BY DIRECT OWNER INSTRUCTION**

The Advisor returned a full ruling (recorded verbatim in "Advisor's returned block" below) holding that
the Kimi K3 row fails live verification, that the blocker covers Planner-role work, and that execution
of the promoted `A4s-r5` is nevertheless authorized.

**Before that ruling arrived, the owner answered the same question directly:**

> "kimi k3 doesn't take an effort, so don't worry about effort for kimi k3"

Under `docs/agent-workflow.md` §4.1 — *"The owner may message any agent directly, including a child.
Such a message is an owner instruction, not an injection: follow it, record it... It outranks every
role's ruling."* — **the owner's instruction governs.** Its effect:

1. The §1 Planner row `Kimi K3` is **satisfied** by `workbuddy-ai/kimi-k3` **with `reasoning_effort`
   omitted**. The `@ max` qualifier is read as the model's own default behaviour, not a selectable
   tier, because the model exposes no tiers.
2. **Planner-role work is therefore NOT blocked.** The Advisor's Q1 conclusion that the row fails
   verification — and its consequence that Planner work is blocked pending an owner repair — is
   **superseded**, because the owner has supplied exactly the repair it escalated for ("the owner
   chooses the repair: omit the effort in §1, name another route, or wait for the provider"). The
   owner chose to omit the effort.
3. **No substitution occurred.** The route used is the canonical `workbuddy-ai/kimi-k3` named in §1.
   No other Kimi model, Grok, GPT-5.6, another Claude model, or Hy3 was used.
4. The Advisor's Q1 ruling is **retained verbatim below as provenance**, not as operative policy. Its
   factual basis is not contradicted: Kimi K3 genuinely advertises no selectable efforts, and both
   `max` and `high` are genuinely rejected. Only its **consequence** (Planner work blocked) is
   superseded by the owner.
5. **`docs/agent-workflow.md` §1 was NOT edited.** Staffing/model assignments in §1 are an
   owner-reserved decision (§3.4). The owner's instruction is the authority for invoking the row
   without an effort in this session; if it is meant to persist in the text, §1 is the place to change
   it.

**Q1 and Q2 agree on the operative point that matters for this session:** executing the promoted
`A4s-r5` is authorized. That authorization does not depend on the Q1 disagreement.

---

## Q2 — P0.8 deviation: **PROCEED UNDER A RECORDED PROCESS EXCEPTION**

### Advisor's ruling (verbatim)

> (a) Record P0.8 as FAIL exactly as measured. Do not rewrite it as PASS. This literal failure does not
> select R-PRE. I grant a process exception, not an evidence exception (§2.3, §2.4.8), and execution
> proceeds past P0.8. P0.8 guards the game-side baseline: the build inputs and generated tree are the
> recorded commit, and the evidence binds to that commit. The only dirty path is
> docs/agent-workflow.md, which the owner edited deliberately. It is not a build input. It is outside
> every path AC-GEN inspects (`src/recomp config tools/disasm/output`, path-scoped). It is in a
> different repository from AC-MERGE, AC-KEEP and AC-INV, which inspect the toolkit. The run classifier
> records project patch/status hashes but does not require a clean tree. The packet's own steps also
> dirty the game tree (logs\a4s, the evidence file), so no criterion depends on the whole tree being
> clean. Sending this to R-PRE would produce a false BLOCKED on a premise that holds (§3.1). It would
> also route to a Planner that is currently unavailable, which stalls the project and protects nothing.
> Your request is the escalation §2.2.9 requires, so continuing is not the Session acting on its own
> judgment.
>
> CONDITIONS (the exception holds only while all of these are true):
> - Immediately before step 1, `git status --porcelain` in the game repository shows exactly
>   ` M docs/agent-workflow.md`, plus only paths the Session itself created under this ruling (receipt,
>   plan/evidence records). docs/agent-workflow.md must hash to SHA-256
>   CBEF90414E78D14BBF66FFA25D9BC7854D312C84AED9605246F305BE7AB8E6AB (observed by me just now).
>   Re-record both values at step 7. If any other path is dirty, or the file's hash changes during
>   execution, P0 has really failed → R-PRE.
> - Every other P0 item must still hold as you measured it (toolkit clean at 0d7929c, @{u}, the three
>   refs at 766ecef, remotes, no game remote, no a4s-* branch, lines 6748/6751).
> - Record in the evidence file's P0 section: "P0.8 FAIL (literal): ` M docs/agent-workflow.md`
>   [hash]; proceeded under Advisor exception [ruling ref]". Do not write "P0 PASS".
>
> (b) Do NOT commit the owner's docs/agent-workflow.md, and do not stage, stash, checkout, revert or
> line-ending-normalize it. That rules out `git add -A`, `git commit -a` and `git stash`. Committing it
> would make P0.8 pass literally by rewriting the state the precondition measures, which is the
> normalization the owner forbade. It would also fix the owner's roster in history while its Planner
> row is known to be broken and waiting on the owner's repair. The file remains the owner's until the
> owner commits or revises it. The startup receipt (§0.6) may be committed ALONE with a path-limited
> commit (stage only docs/reviews/startup-<date>-<session>.md and any Session-owned record; verify
> `git diff --cached --name-only` lists nothing else). You may also leave it uncommitted. Either way,
> record the resulting game HEAD as the execution's game revision.
>
> (c) Plan-record discrepancy (b7d6af5 recorded vs c1cdb91 actual promotion commit): advisory,
> non-blocking. A commit cannot contain its own hash. The packet's Baseline delegates the game revision
> to the Session's record, and §5.4 says record pointers do not reopen a frozen packet. Record the
> actual HEAD in the evidence file. Do not revise the packet or the plan pin because of it.

### Conditions accepted and how they are met

| Advisor condition | Session action |
|---|---|
| Status exactly ` M docs/agent-workflow.md` + Session-created paths | **Recorded immediately before step 1** in `docs/reviews/a4s-execution-evidence.md` (P0 section) |
| `docs/agent-workflow.md` SHA-256 = `CBEF9041…E6AB` | **Re-measured and confirmed** at step 1 and again at step 7 |
| Every other P0 item still holds | **Confirmed** — toolkit clean at `0d7929c`, `@{u}` = `upstream/main`, all three refs `766ecef`, four remote lines exact, no game remote, no `a4s-*` branch, `recomp_0005.c:6748/:6751` as stated |
| Write "P0.8 FAIL (literal)… proceeded under Advisor exception" | **Done** — never written as "P0 PASS" |
| Do not commit/stage/stash/revert/normalize the owner's file | **Observed** — path-limited commit only; `git diff --cached --name-only` verified |
| Record resulting game HEAD as execution's game revision | **Done** in the evidence file |

### Accountability entries (§2.3)

1. **Decision:** override §0's literal "startup BLOCKED" reading, scoped to Planner-free execution of
   an already-`ADEQUATE`, promoted packet. **Reason:** the blocker's purpose — no work resting on an
   unverified or substituted route — is not engaged by contract execution that uses no Planner.
   **Reversed by:** the owner stating that any failed roster row halts all execution, or evidence that
   an execution or acceptance step needs a Planner verdict. *(Q1: the owner has since resolved the
   route itself, so this override is no longer load-bearing for `A4s-r5`; it is retained because it
   would govern any future Planner outage.)*
2. **Decision:** proceed past P0.8's literal failure under a process exception. **Reason:** the single
   dirty path is a non-build, non-evidence documentation file the owner intentionally edited, outside
   every path-scoped check in the packet. **Reversed by:** any criterion, script or build step found to
   read `docs/agent-workflow.md` or to require a whole-tree-clean game repository — in particular a
   `check-generation-provenance.py --check` problem citing project dirtiness, which is to be treated as
   `UNKNOWN` and escalated, **not waived**.

### Advisor's noted uncertainty (recorded, not waived)

The Advisor did not trace every consumer of the provenance checker's `dirty` identity field. If
`scripts/check-generation-provenance.py --check` reports a problem attributable to project dirtiness,
that is **UNKNOWN → escalate**, not a PASS. This is recorded as a live condition on the exception.

---

## Q3 — `AC-INV` "New environment names" command: **PROCEED WITH A CONTROLLED SUBSTITUTE**

### Advisor's ruling (verbatim)

> RULING: (b). This is an Advisor interpretation ruling that execution follows. It is not an A4s-r6
> revision, and execution may proceed now.
>
> 1. Classification. The literal command does meet the §3.1 test on this host. Run as written, it
> silently returns an empty set, and an executor could record "no new names" and drop
> RECOMP_APU_MIXDOWN_ALL from a load-bearing follow-up premise. The defect is in shell transport only:
> Windows PowerShell 5.1 strips the embedded `"` when it builds git's command line. The criterion
> itself is unaffected: its claim, regex, revisions, SCOPE, dispositions, expected set and decision
> logic are all correct and unchanged. Under §5.4, a post-ADEQUATE revision is PERMITTED ("may be
> revised only when"), not REQUIRED, when a blocking finding exists. Revising a frozen, adequate packet
> for a quoting artifact would force an A4s-r6 adequacy round that protects nothing, because the ruling
> below delivers the identical regex to git. So I remedy it by interpretation ruling: the operative
> content of the step is the ERE `getenv\("[A-Za-z0-9_]+"` as git receives it, and any transport that
> delivers those exact bytes to `git grep -h -o -E` is the command as written.
>
> 2. Authorized substitute (the ONLY authorized form):
>    - Write logs\a4s\getenv-pattern.txt containing exactly the ASCII bytes
>      `getenv\("[A-Za-z0-9_]+"`, with no BOM and no trailing newline. Use
>      `Set-Content -NoNewline -Encoding ascii` (or `[IO.File]::WriteAllBytes`). WARNING: on PS 5.1,
>      `-Encoding utf8` writes a BOM and `Out-File` writes UTF-16LE. Either would corrupt the first
>      pattern and silently break matching, which is the same defect class again. Record the file's
>      byte length and SHA-256.
>    - Run `git -C $T grep -h -o -E -f logs\a4s\getenv-pattern.txt <rev> -- src include`, then sort
>      unique, for <rev> = 0d7929c and M. Record the difference both ways, verbatim, in logs\a4s\.
>    - Controls, all required, or the step is UNKNOWN → FAIL per AC-INV's own rule: (i) 0d7929c yields
>      exactly 42 unique names; (ii) 75083476 yields 44, with difference {RECOMP_APU_MIXDOWN_ALL,
>      RECOMP_USB_PORT} added and none removed. This known-good control proves the transport delivers
>      the quote-bearing pattern. (iii) Also record the literal packet command's output on 0d7929c
>      (expected: empty, exit 1) as the known-bad witness showing why the substitute is needed.
>    - At M, whatever the set difference is gets dispositioned exactly as line 115 says. Any new name
>      that is classifier-listed or deleted is FAIL. Other new names are "new unclassified variable".
>      A difference that does not match the 75083476 expectation is recorded as it is, not reconciled
>      to the expectation.
>
> 3. Scope of the authorization: THIS ONE COMMAND ONLY, at every place it is executed: both revisions,
>    and the D1 re-run on an amended M. It does not authorize substituting any other command. If any
>    other command fails as written, stop that line of work and escalate it separately (§2.2.2).
> 4. Acceptance. Reviewers bind to the frozen contract plus recorded Advisor rulings (§3.3: a ruling
>    "binds every role from the moment it is recorded"). A reviewer checks this step against the
>    substitute form and its controls, not against the literal quoting.

### Session note on one stated number

The Advisor's ruling says "Record the file's byte length (24)". The Session **measured** the byte
length of the authorized byte sequence `getenv\("[A-Za-z0-9_]+"` and records the **measured** value in
`docs/reviews/a4s-command-defect.md`'s companion evidence and in the execution evidence file. The
ruling's operative requirement is that the file contain **exactly those bytes** with no BOM and no
trailing newline; the byte length is recorded as measured, not as predicted. Any difference from the
predicted number is a note about the prediction, not a deviation from the ruling.

### Accountability entry (§2.3)

**Decision:** interpretation ruling instead of an `A4s-r6` revision; **reason:** transport defect only,
and the identical regex is delivered with known-good and known-bad controls; **reversed by:** a control
failure or a matched-language difference.

---

## Q4 — `AC-TEST` set G: two pre-existing failures: **NAMED EXCEPTIONS (ruling (c))**

Raised after the step-2 pre-merge controls produced two set-G failures. Measurement record:
`docs/reviews/a4s-r5-premise-baseline-failures.md`.

### Advisor's ruling (verbatim)

> RULING: (c). The two named step-2 failures are recorded baseline exceptions and do not select
> R-TEST. Every other G result is held to the literal PASS rule. The KX carve-out is NOT extended to G
> wholesale, so (a) is rejected as too broad. (b) is rejected because it produces a false FAIL.
>
> 1. Why this is an interpretation ruling, not a revision. AC-TEST's own "Controls" line makes the
>    step-2 run "the known-good control for every set", and its "Guards against" line targets a merge
>    resolution that breaks what the project relies on. Read literally, (ii) would select R-TEST, roll
>    toolkit main back and blame the merge for two failures that you measured on pristine game HEAD
>    before any merge. Those failures sit in the game repository, which this packet may not edit
>    (Non-goals). That outcome is a false FAIL and a misattribution (§3.1). The criterion's two clauses
>    conflict for G only because its known-good premise does not hold for G. I resolve that conflict in
>    favour of the criterion's stated purpose. The ruling is made BEFORE any measurement at M exists,
>    so it is prospective and fixed in advance (§2.4.4). It converts no failure into success, because
>    both tests stay recorded as FAIL (§2.4.3).
> 2. Exact exception set (closed; nothing else qualifies):
>    **E1: tests/test_agent_docs.py.** Failing test set exactly `{test_real_repository_is_clean}`
>    (Ran 30, failures=1). The underlying scripts/check-agent-docs.py finding set is exactly the four
>    `retired_names` findings for 'gpt-5.6-sol' in plan-jsrf-bare-minimum.md. The step-2 lines are 15,
>    47, 67 and 71.
>    **E2: tests/test_ac2_provenance.py.** Failing check set exactly
>    `{"A0 correct run PASSes", "P1 differently-cased archive keys still PASS"}`, with the clause-D
>    reason line "the classifier was NOT edited; step 3 requires line 391 to be corrected" (I observed
>    that reason line myself just now).
> 3. The G rule at M (step 5):
>    - The other 8 G files must pass exactly as in step 2. Any failure → R-TEST.
>    - E1 and E2 may fail at M only with IDENTICAL failure sets. A new failing test, a new failing
>      check or a changed reason in either file → R-TEST. An additional checker finding in E1, or any
>      change to the finding set other than a line-number shift caused solely by the Session's own
>      record edits to the plan (record both listings and the edit that shifted them) → R-TEST. If
>      either file now passes at M, record it; that is not a FAIL, but note it as unexplained, because
>      the merge should not affect it.
>    - A G file that is new at M, or missing at M → R-TEST. (The game tree should not change; if it
>      does, the Stop-if on game src/config applies first.)
>    - K4 and C have no exception. They passed at step 2 and must pass at M under the literal (i)/(ii).
>      KX keeps its own written carve-out, unchanged.
>    - UNKNOWN (a G file cannot run) follows the packet's existing UNKNOWN clause.
> 4. Keep the baseline fixed during execution. Do NOT fix the plan's gpt-5.6-sol lines or either test
>    before step 5 has run. Doing so would change the control mid-packet. Avoid adding any retired
>    route name to AGENTS.md, docs/agent-workflow.md or the plan during execution. (The checker reads
>    those files. Review records are not among them.)
> 5. AC-TEST's recorded result must say so in the same sentence: "AC-TEST PASS with two recorded
>    pre-existing G failures (E1, E2) excepted by Advisor ruling Q4; G is not all-green". The claim
>    limits add that E1/E2 are unresolved defects in the game repository.
> 6. Follow-ups (plan, after closure; not in this packet): (i) E1 — the plan's four current-tense
>    gpt-5.6-sol lines need historical markers or rewording. That is a record correction and needs no
>    re-review (§3.2). (ii) E2 — cause unconfirmed. Lead: the test's clause D expects an
>    unedited-classifier state from an older packet (A3a-era "line 391"). Diagnose before anyone relies
>    on the AC2 provenance tool.

### Applicability to this execution

**`AC-TEST` was not reached.** `R-CONFLICT` is row 2 and was selected at step 3 on five UNDECIDED
hunks, before any build or test at `M`; there is no `M` and no step-5 test run. The ruling is therefore
**prospective and unexercised** here. It is recorded because it binds any future execution of this
packet revision and because its two named exceptions are measured facts about the game repository.

### Accountability entry (§2.3)

**Decision:** named pre-existing G failures excepted instead of an `A4s-r6` revision; **reason:** a
literal `R-TEST` would misattribute a pre-merge game-repository defect to the merge, and the ruling
precedes any `M` measurement; **reversed by:** toolkit dependence of E1/E2 or an unfixed baseline.

---

## Q4 addendum (KX) — per-module invocation governs

### Advisor's disclosure (verbatim)

> DISCLOSURE FIRST: to check your KX note, I ran the 33 KX modules one per invocation in
> C:\Users\logic\Repos\xboxrecomp. The merge was already in progress, so my run hit the working tree
> with conflict markers, not clean 0d7929c: 27 modules exit 1 with SyntaxError at lifter.py:401
> `||||||| 051a128`. Those results are NOT a baseline measurement and must not be recorded as
> evidence. The only side effect was ignored `__pycache__/*.pyc` (check-ignore: .gitignore:2). I
> observed no tracked or untracked-unignored change. `git diff --name-only --diff-filter=U` showed 5
> conflicted files: kernel_bridge.c, xbox_memory_layout.c, lifter.py, test_icall_feedback.py,
> translator.py. Please re-confirm `git status --porcelain` shows nothing of mine before you continue.
> From here on I will not run anything in the toolkit tree while execution is live.

**Session verification of the disclosure (measured, before continuing):** toolkit
`status --porcelain` = 153 lines, **all** merge-staged (`M `/`A `) or the 5 expected `UU` conflicts;
**0 unstaged modifications**; **0 untracked non-ignored files**; the 15 ignored entries are
`__pycache__`, `build/`, `output/` and `.pytest_cache/` directories already covered by the toolkit's
`.gitignore`. `MERGE_HEAD` intact, `HEAD` = `0d7929c`. **The Advisor's run left no tracked or
untracked-unignored change.** Its 27-module syntax-error results are recorded as **not evidence** and
were not used. The clean-tree KX table was then re-run by the Session on the restored `0d7929c`.

### Advisor's ruling (verbatim, operative points)

> 1. The per-module invocation is the operative KX procedure, because it is the packet's literal
>    wording. The combined "Ran 133 … OK" run is recorded as reproducing the packet's control line. It
>    is not the per-module control.
> 2. Exit 5 / "Ran 0 tests / NO TESTS RAN" is NOT a failing test. It is also not a pass witness,
>    because the module was not exercised. Step 2 has 16 such modules. […] At M, per module:
>    - exit 0 with the same or higher Ran count as step 2 → pass;
>    - exit 5 / Ran 0 where step 2 was also exit 5 / Ran 0 → "not exercised (pre-existing, recorded)",
>      not a FAIL;
>    - a module that ran ≥1 test at step 2 and runs 0 at M, or runs fewer tests → FAIL (lost coverage)
>      → R-TEST;
>    - any failing or erroring test, or an import error → FAIL → R-TEST;
>    - a module new at M: exit 0 → pass; exit 5 → "not exercised (new, recorded)", listed in claim
>      limits; any failure → R-TEST.
> 3. Record the step-2 per-module table (module, exit, Ran) and check that its Ran sum equals 133. If
>    it does not, record the sum as the control and flag it.
> 4. Claim limit to add: KX does not exercise the ≥16 function-style test modules under unittest. […]
>    Plan follow-up after closure: run them under their own runner (they have `__main__` blocks) or
>    convert them. Not in this packet.

### Session measurement against the addendum

The per-module table was re-run on the **clean, restored** `0d7929c` (not on a conflicted tree):
**33 modules, Ran sum = 133**, exactly matching the packet's recorded control — so item 3's flag
condition is not triggered. Table: `logs/a4s/kx-permodule-0d7929c.csv`. 17 modules exit 0 with tests;
**16** exit 5 / `Ran 0 tests`. Applicability: `AC-TEST` was not reached in this execution (see Q4
above), so the at-`M` comparison is not performed.

---

## Q4 closure note — premise change acknowledged; E1 withdrawn, E2 stands

Recorded after the Session reported the plan-rewrite `PREMISE_CHANGED` (see
`docs/reviews/a4s-execution-evidence.md`, "Premise change to `Q4`'s E1 exception").

### Advisor's ruling (verbatim, operative points)

> 1. Q4 is NOT reversed. E1's exception is MOOT and is withdrawn: its subject (the four plan lines) no
>    longer exists. E2's exception stands unchanged, with the failing set {A0, P1} and the clause-D
>    reason. Your classification is correct. Q4 item 4 protected a step-5 comparison that never
>    happened, because R-CONFLICT is row 2 and AC-TEST was never reached, so no measurement was
>    altered. The plan's CURRENT PACKET rewrite is closure bookkeeping the plan owns. It is not
>    baseline tampering, because no measurement depended on those lines at that moment. You correctly
>    caught and fixed your own new line-74 finding.
> 2. **Prospective rule for any re-execution** (A4s-r6 or a rerun). The Q4 exception set is re-derived
>    from THAT execution's own step-2 G run. It is never carried forward. With today's tree that means
>    E2 only, and G expected at 9/1. If G at step 2 differs from 9/1, record it; any new pre-existing
>    failure needs its own Advisor exception before step 5. That replaces "re-measure 8/2 → 9/1" with a
>    general rule. The A4s-r6 Planner should state this in AC-TEST itself (a G carve-out modelled on
>    KX's, naming failures by test identity). That closes the literal-clause gap that produced Q4 in
>    the first place.
> 3. **Rollback exe hash:** I accept that it is recorded and selects no row, since the packet says
>    record, not gate. **ADVISORY, one weakness in the explanation:** step 2's exe matched 9597FF7C,
>    the ARCHIVED A4a-r2 hash, exactly. If every link stamps a fresh timestamp, a fresh relink at step 2
>    could not match a days-old archive hash. So the step-2 build most likely did NOT relink (an
>    up-to-date incremental build), while the rollback rebuild did, because the merge/abort touched
>    sources (your CRLF rewrite of 4 files fits). By the same logic, "two consecutive rollback rebuilds
>    gave the same hash" is probably a no-relink witness, not a reproducibility witness. None of this
>    changes the row or any claim. It does mean any future "exe equals control" check must say whether a
>    relink happened, and should use a timestamp-masked or /Brepro comparison where equality matters.
> 4. **CRLF rewrite:** git status is clean and the normalized bytes match the reference, so it is
>    non-blocking. Because the conflicted files were rewritten on disk, any future build comparison
>    should treat the rollback tree as "same content, new mtimes".

### Session response — the advisory is CONFIRMED by measurement, and the earlier inference is retracted

The Advisor's stated discriminator (the step-2 build log's link line) was **unavailable**: step 2's
`logs/build-current.log` was overwritten by the rollback rebuild, and `build-jsrf.py` does not archive
per-build logs. The mechanism was therefore tested directly instead:

| Measurement | Result |
|---|---|
| Does a **no-op** build relink? (ran `build-jsrf.py` twice, no source change) | **No** — exe mtime `17:58:10.059` and SHA `AEC1F0FF…` unchanged; log shows no compile of `recomp_0005.c`/`main.c`, no `Generating Code` |
| What did the merge/abort rewrite? | **58 toolkit files** with mtime `2026-09-25 17:57:51`, incl. `src/apu/apu_dsp.c`, `src/kernel/kernel.h`, `CMakeLists.txt` |
| Rollback build time | exe mtime `17:58:10` — **19 s after** that rewrite → it relinked |
| Content identity | `git status --porcelain` empty; `git diff --stat HEAD` empty |

**The Advisor's explanation is confirmed in both directions.** Step 2's build did not relink (keeping the
archived binary and its old timestamp, which is why `9597FF7C…` matched); the merge/abort rewrote 58
files' mtimes, forcing the rollback build to relink and stamp `AEC1F0FF…`.

**Retraction.** The earlier claim in `docs/reviews/a4s-r5-rollback-exe-hash.md` that the rollback rebuild
was "deterministic" — inferred from two identical hashes — was **wrong**, exactly as the Advisor
suspected: those identical hashes were a **no-relink witness, not a reproducibility witness**. That
record is corrected, with the correction stated in place rather than silently edited. The measured
hashes, the row selection and every packet claim are unaffected.

### Routing for `A4s-r6` (Advisor-directed, per the `R-CONFLICT` row)

- **Hunks 1–2** (`kernel_bridge.c`, the `KeSetEvent` / `KeWaitForSingleObject` event paths) go to the
  **Advisor FIRST**, before the Planner drafts `A4s-r6` — they choose between accepted local runtime
  code and an upstream model. The Advisor has **not** read them and is **not** pre-ruling them; the
  Session must supply the hunk text (base/ours/theirs) plus the local accepted-packet provenance.
- **Hunk 5** (`lifter.py`, the `H1` vacuous-containment gap) is a **rule defect the `A4s-r6` Planner
  must close**: `H1` must require that neither side **deleted** a base line the other kept, or the rule
  must be redesigned.
- **Hunks 8–9** and any non-runtime hunks go straight to the **Planner**.
- **Planner route:** `workbuddy-ai/kimi-k3`, **no effort setting** (owner ruling, Q1).

### Accountability entry (§2.3)

**Decision:** withdraw E1's exception as moot while keeping E2, and convert the re-execution rule from a
one-off re-measurement into a general per-execution derivation; **reason:** E1's subject no longer
exists, and `AC-TEST`'s literal clause gap should be closed in the packet rather than by case ruling;
**reversed by:** E1's future pass being found to depend on the toolkit revision.

---

## Advisor's returned block (verbatim, Q1 + Q2)

```text
ADVISOR RULING — consolidated Q1 + Q2 (Persistent advisor, this probed child; route per your live
resolution: Claude Opus 5.5 @ high). Case ruling under docs/agent-workflow.md §2.3/§3.3.

========== Q1 — Kimi Planner row, unsupported `max` ==========
RULING:
1. The Planner row FAILS live verification. It is a real staffing blocker, not a near-miss. §1 requires
   "exactly one advertised route ... which supports the required effort". workbuddy-ai/kimi-k3 is the
   only canonical match but advertises no efforts, and both `max` and `high` were rejected at spawn.
2. Kimi without an effort (the `KIMI_NO_EFFORT_ALIVE` spawn) is NOT an authorized Planner. The row
   names `max`, not an omitted effort, so dropping the effort is a substitution, and §0/§1 forbid
   silent fallback. Repairing the row means editing §1 (for example to omit effort, or to name another
   route). That is owner-reserved under §3.4, so I am escalating it to the owner. Neither you nor I
   choose the repair.
3. SCOPE: the blocker covers Planner-role work only. That means no adequacy review, no drafting or
   revision of A4s-r6, A4b1-r2 or the next brief, and no freeze or promotion that needs a Planner
   verdict. It does not block contract execution of an already-ADEQUATE, promoted packet. §0 says a
   missing route makes startup "BLOCKED for accepted game work". The purpose of that line is to stop
   work that would depend on, or substitute for, an unverified route. A4s-r5 execution (steps 1-7) and
   its acceptance use only the Session, both reviewer stages and the Advisor, and all of those routes
   answered probes. The Planner route is used nowhere in execution. Reading §0 to halt this execution
   protects nothing, so under §2.3 I override that literal reading for this case only. §4.1's own
   precedent points the same way: when one judgment role is missing, the others continue within their
   authority. This agrees with the owner's instruction that the failed route "is a staffing blocker";
   it only rules on how far that blocker reaches.
4. EXECUTING A4s-r5 NOW IS AUTHORIZED, subject to Q2's conditions. Hard stop: once a decision row is
   selected whose next step is a Planner (R-CONFLICT, the expected result; R-BUILD; R-TEST; R-PRE;
   R-INVALID/R-UNKNOWN after the single rerun; R-SAME; R-MOVED's brief), do the row's mechanical
   actions (rollback, rollback-rebuild SHA, evidence file, AC-NOPUSH), then stop. Record next-step
   state as "pending — Planner route BLOCKED (§1 Kimi effort)". Do not hand the row to a substitute
   Planner. R-REGEN and R-PUSH route to me and the owner and are not blocked. Acceptance review of a
   delivered packet may run: Hy4 first, DeepSeek second.
BASIS:
- observed: agent-workflow.md §0 lines 43-46, §1 lines 56 and 77-84, §3.4 line 308, §4.1 lines 331-332
  (read directly).
- observed: I re-ran list_subagent_models myself. workbuddy-ai has exactly six routes and one Kimi
  (`workbuddy-ai/kimi-k3`), which advertises "(no advertised reasoning efforts)".
- observed: the packet at C:\...\docs\packets\a4s-toolkit-sync.md hashes to SHA-256 09DA9413…FB86FB,
  263 lines, which equals the CURRENT PACKET pin. Its execution steps 1-7 invoke no Planner (read lines
  22-60 and 154-184).
- observed (from your brief): the spawn rejections at max/high and the no-effort spawn. These are your
  measurements; my catalog read agrees with them.
- inferred: the purpose of the §0 block (no work resting on an unverified or substituted route) is not
  engaged by Planner-free contract execution.
- uncertain: whether the owner meant "staffing blocker" to halt all work. If so, see REVERSED BY.
REVERSED BY: the owner stating that any failed roster row halts all execution; or evidence that some
execution or acceptance step needs a Planner verdict. Separately, once the owner repairs §1 and the
Planner resolves live at the listed effort, the "pending — Planner route BLOCKED" state clears and the
routed follow-up proceeds normally.

========== Q2 — P0.8 dirty game tree + receipt commit ==========
RULING:
(a) Record P0.8 as FAIL exactly as measured. Do not rewrite it as PASS. This literal failure does not
select R-PRE. I grant a process exception, not an evidence exception (§2.3, §2.4.8), and execution
proceeds past P0.8. [conditions and basis as recorded above]
...
REVERSED BY: any criterion, script or build step found to read docs/agent-workflow.md or to require a
whole-tree clean game repository (for example check-generation-provenance.py --check failing on project
dirtiness), which would make the deviation load-bearing → stop, R-PRE; any additional dirty path or a
hash change in that file; or an owner instruction to commit or revert the file first (then follow the
owner and re-measure P0).
```

*(The Q2 basis lines and the full "RECORD IN" block are in the Advisor's message; the operative
conditions and accountability entries are reproduced in full above. Q1 items 1–2 are superseded by the
owner instruction recorded at the top of this file.)*

---

## Consequential records required by the ruling

- **Plan `CURRENT PACKET` blocker line:** the Advisor directed that the plan's blocker line read
  "Planner route BLOCKED — §1 Kimi K3 @ max unsupported (no advertised efforts); owner decision (§3.4)
  pending". **The owner has now decided** (omit the effort), so the accurate current blocker line is
  recorded in the plan as: the Planner route is resolved by owner instruction; no Planner blocker is
  in force. The plan owns the current blocker and is updated accordingly.
- **Owner escalation (§3.4, staffing):** raised by the Advisor and **answered directly by the owner**
  during this session. No owner decision remains pending.
- **This ruling does not reopen any packet.** Per §5.4, P0.8 is a precondition outcome, not a packet
  criterion, and the recorded exception changes no criterion, threshold, or evidence rule.
