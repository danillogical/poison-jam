# A4s-r6 execution rulings — VERBATIM (Advisor §5.4 interpretation rulings)

**Advisor:** child `5c555969-dea9-4b47-be05-62aa0835cde2`, route `claude` / `claude-opus-5-5` @ `high`
— the persistent Advisor, reused per `docs/agent-workflow.md` §4.4.
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Revision:** `A4s-r6`, frozen SHA-256
`75207C41B8E9964D3E1467F5E980D1B8DE4CB75A1E0AFE954F59D40FA27D86E3`.
**Status:** **BINDING — execution follows them, and neither requires a packet revision.** Both are §5.4
interpretation rulings: the criterion text is not wrong for its purpose, it is over-strict for a re-run.

The Advisor's own read-only checks, recorded verbatim: *"In the game repo: `git ls-files --eol` shows both
pinned files as i/lf w/lf with no attributes. Working-tree Get-FileHash equals the pin for both files
today. A binary-safe blob hash of HEAD:<path> also equals the pin for both. `git status --porcelain` for
both paths is empty. core.autocrlf=true. In the toolkit: `a4s-pre-sync` = 0d7929c…, its reflog has a
single entry 'branch: Created from 0d7929c…', `branch --list "a4s-*"` prints only that branch, HEAD =
0d7929c, and the tree is clean."*

---

## Q-A. P0.10 / `AC-STRUCT` pinned SHA-256: which bytes

> RULING:
> (1) A pin on a TRACKED game-repo file identifies the committed blob. The working tree is not the identity.
> - P0.10 PASSES when either of the following holds:
>   (a) the literal command, `Get-FileHash -Algorithm SHA256 <path>`, equals the pin. This is sufficient, and it holds today.
>   (b) if (a) differs, both of these hold:
>       - (i) the SHA-256 of the blob bytes of HEAD:<path> equals the pin;
>       - (ii) `git status --porcelain -- <path>` prints nothing.
>   Record which branch, (a) or (b), was used. If (b) was used, also record the working-tree hash and "eol-converted checkout".
> - Anything else is a P0.10 FAIL, and the packet's existing row applies.
> - The command for (b)(i) must be binary-safe. On PS 5.1, piping `git show`/`git cat-file` output into Get-FileHash re-encodes the bytes, and `git hash-object` is SHA-1, so neither may be used. Use exactly this, which I ran and which reproduces both pins:
>   `python -X utf8 -c "import subprocess,hashlib,sys;print(hashlib.sha256(subprocess.check_output(['git','cat-file','blob','HEAD:'+sys.argv[1]])).hexdigest().upper())" <path>`
> (2) This is not specific to AC-STRUCT. The same reading applies to every SHA-256 pin in this packet whose subject is a file tracked and committed in the game repo, including the fixtures pin.
> - It does NOT apply to pins whose subject is deliberately the working-tree bytes. Those keep literal Get-FileHash:
>   - the owner's uncommitted `docs/agent-workflow.md` (CBEF9041…), where the P0.8 exception is about the dirty file itself;
>   - untracked records;
>   - build outputs (the exe);
>   - run artifacts.
> - If a pinned subject's tracked/untracked status is unclear, use the literal working-tree hash.
>
> BASIS:
> - OBSERVED: your measurements (blob = pin; fresh CRLF checkout ≠ pin) and mine above. Today, branch (a) passes, so there is no live false FAIL; this ruling is prospective.
> - INFERRED: the CRLF and LF forms of these two .py files execute identically, because Python reads source in universal-newline mode, including inside string literals. So a clean eol-converted checkout is the same tool. Condition (ii) is what excludes a genuine content edit, because it is empty only if the working tree normalises back to the committed blob.
> - A §2.3 purpose reading: the pin exists to detect a changed tool, not a checkout's newline policy. A reading that fails on the second is a false FAIL.
> - A .gitattributes change would be a repo-wide checkout change. It is owner- and packet-scope work, not an interpretation, and must not be made.
>
> REVERSED BY: evidence that the scanner's or the fixtures' behaviour depends on line endings, for example a byte-level test of CRLF inside a fixture. Branch (b) would then be invalid, and the tracked content would need a .gitattributes pin under a revision.
>
> RECORD IN: docs/reviews/a4s-r6-execution-rulings.md (verbatim), plus the P0.10 line of docs/reviews/a4s-execution-evidence.md, stating which branch passed.

---

## Q-B. P0.7 literal FAIL: the existing `a4s-pre-sync`

> RULING:
> (1) YES, proceed. Treat P0.7 and step 1 as satisfied by verify-and-reuse, under all of these fail-closed conditions:
>   - `git -C $T branch --list "a4s-*"` prints exactly one branch, `a4s-pre-sync`;
>   - in particular, `a4s-merge-attempt` is ABSENT;
>   - `git -C $T rev-parse a4s-pre-sync` = 0d7929c86771dd0b971941592fd4f15436116e82;
>   - `git -C $T reflog show a4s-pre-sync` shows no move after its creation from 0d7929c.
>   Record the step as "pre-existing from A4s-r5 step 1, verified and reused, not created", with the three outputs.
>   - Do not run `branch -f`, `-D`, or re-create the branch. Do not create any other ref.
>   - AC at packet line 113 then applies unchanged: a4s-pre-sync = 0d7929c after step 1 and at closure.
>   - The rollback procedure is unchanged. Its `branch -f a4s-merge-attempt HEAD` cannot clobber anything, because that branch is absent.
> (2) If any of those conditions fails, it is a P0 FAIL, and the row is R-PRE. It is never R-CONFLICT, because no merge has been attempted.
> (3) No revision is required.
> - This is an interpretation ruling: the criterion's text is not wrong for its purpose, it is over-strict for a re-run.
> - P0.7 exists "so step 1 and the rollback cannot overwrite an existing branch". Nothing is overwritten, and the recovery ref already sits at the exact rollback target.
> - For any future revision, note the text change: "prints nothing, or exactly `a4s-pre-sync` resolving to 0d7929c and no other a4s-* branch". This is a revision note, not a reopening.
>
> BASIS:
> - OBSERVED: branch state, reflog, HEAD, and a clean tree, as listed above.
> - OBSERVED: `git branch` exits 128 when the branch already exists (your dry test).
> - INFERRED: R-PRE would report "baseline not as stated" when the baseline is exactly as stated. That misattribution is the §3.1 blocking shape. I may grant this under §2.3/§2.4 because it is a process reading, not an evidence exception: every evidence criterion is unchanged.
>
> REVERSED BY:
> - a4s-pre-sync not resolving to 0d7929c, or its reflog showing it was moved and then restored;
> - any other a4s-* branch present;
> - evidence that a4s-pre-sync was pushed, which would make it no longer local only.
> Any of these → R-PRE.
>
> RECORD IN: docs/reviews/a4s-r6-execution-rulings.md (the same file as Q-A), plus the step-1 entry in docs/reviews/a4s-execution-evidence.md.

---

## Q-C. Step-6 KX / `pytest` — NOT `R-TEST` (added 2026-09-25, after steps 1–6)

The Advisor's own read-only checks, verbatim: *"All 7 modules do `import pytest` at module level. None
contains a TestCase or load_tests. `py -0p` lists 3.13, 3.12 and 3.11. find_spec('pytest') is False on
all three, and capstone is present only on 3.13. The step-2 interpreter is the same C:\Python313, so
pytest was absent at step 2 as well. `git diff --stat 766ecef 3f8bf67c` shows lifter.py and translator.py
changed relative to upstream. These are the resolved hunks 5, 6, 7 and 9."*

> RULING:
> (1) NOT R-TEST.
> - A module whose only error is an import failure for an absent third-party dependency has not failed a test. No test in it ran. That is the same disposition as exit 5 / Ran 0 in my A4s-r5 KX ruling (item 2).
> - Record each as "not exercised: new at M, dependency absent (pytest)". Such a module is not a FAIL and not a PASS witness.
> - This applies per module only if all of the following hold. If any fails, the module is a FAIL under AC-TEST and the row is R-TEST:
>   (a) its unittest output contains exactly one error, `ModuleNotFoundError: No module named 'pytest'`, raised by the module's own top-level import. Any other missing name, especially tools.* or a module in either repo, is a FAIL;
>   (b) its blob at M equals its blob at 766ecef;
>   (c) the step-2 interpreter also lacks pytest. Record the find_spec result;
>   (d) every step-2 module still passes with Ran ≥ its step-2 count, which you measured.
> - The 9 new exit-0 modules are PASS. The 7 new exit-5 modules are "not exercised (new, recorded)", as already ruled.
> (2) The UNKNOWN clause does not trigger, and it authorizes no install.
> - It covers "a set cannot be run". KX ran: 49 of 56 modules executed, and every control module passed. So its "persists → R-TEST" escalation does not apply.
> - Installing pytest is an environment change. The owner's directive outranks the packet's retry remedy, so the Session may not install it.
> - Do not borrow a pytest from any other interpreter or venv for this criterion either; one exists in an unrelated project venv. That also changes the environment.
> - Only the owner can authorize an install.
> (3) YES, it is a claim limit plus a gated lead, and it selects no row. Record this claim limit: "KX does not exercise 7 pytest-dependent upstream modules (list them) under unittest. The lifter/translator conflict resolutions (hunks 5–7 and 9) are therefore witnessed only by K4 and the existing KX modules, not by upstream's own tests of those paths."
> - This matters, and it is exactly why the lead is gated. test_incdec_result, test_incdec_carry and test_lifter_result_clobber test the inc/dec/_fa result-snapshot family that hunk 9 resolved by union.
> - The gap is harmless for THIS packet only because AC-GEN holds. There is no regeneration, so M's lifter produced none of the linked code, and the exe and the strict run cannot depend on it.
> - GATED LEAD (binding on later planning): before ANY packet regenerates or relifts with the toolkit at M or later, all 56+ KX modules, including these 7, must run under real pytest in an owner-authorized environment. Parametrized and fixture tests are included.
> (4) Rows 2–6 are unmatched on this evidence. Proceed to step 7, the one strict run, then step 8.
> - The shim probe (14 pass / 0 fail / 8 fixture-blocked) is admissible only as a recorded diagnostic that supports "environment, not code". It witnesses no criterion. The Session framed it correctly.
>
> BASIS:
> - OBSERVED: your per-module errors and blob identity; my import and interpreter checks above; the 49/56 executed count; the 33/33 control pass.
> - INFERRED: the purpose of R-TEST ("breaks a relied-on or upstream test") is not met, because nothing ran and failed. A literal R-TEST would repeat the §3.1 misattribution already ruled against for set G. It would also roll back a merge whose every exercised gate passed.
> - UNCERTAIN: whether the 8 fixture/parametrize-dependent tests pass. The shim probe cannot settle that, which is why the lead is gated rather than closed.
>
> REVERSED BY:
> - any of the 7 showing a different or additional error;
> - a blob differing from 766ecef;
> - pytest being found importable by the step-2 interpreter, which would make it a real FAIL;
> - a later real-pytest run of these modules failing. That would not retroactively change this row, but it blocks any regeneration packet and becomes that packet's brief.
>
> RECORD IN: docs/reviews/a4s-r6-execution-rulings.md (as Q-C, verbatim), plus the AC-TEST KX lines of docs/reviews/a4s-execution-evidence.md with the per-module list and the four fail-closed conditions. Carry the gated lead into the plan's leads list.

### Q-C on `AC-STRUCT`'s catch

> ON AC-STRUCT'S CATCH:
> - The HA-(b) wording was mine: "locate each by text" was insufficient, because both copies in each switch have identical text. Your "keep the first occurrence per switch" reproduces the positions I named (8040 and 8502). Record it as the operative reading of edit (b).
> - Record one further claim limit on AC-STRUCT itself. It detects duplicates, not omissions. The switch-5 damage, where ordinal 138 had zero cases, was invisible to the scanner. It was caught only because the duplicate in switch 4 prompted a fuller look, and it is now excluded by your post-condition count of one case-138 site per switch.
> - Any future revision should make an expected-count post-condition for each HA edit an AC-MERGE check, rather than relying on the scanner to find deletions.
>
> PUSH NOTE: after R-SAME or R-MOVED, a Closure push of M still requires ACCEPT. The owner push policy's condition "active packet's tests/acceptance passed" is satisfied by acceptance with this recorded claim limit, not by all 56 modules passing. The reviewer should see the claim limit explicitly.

### Session verification of Q-C's four fail-closed conditions — ALL PASS

Measured immediately after the ruling (`logs/a4s/verify-qc-conditions.py`):

| Condition | Result |
|---|---|
| **(a)** exactly one error per module, `ModuleNotFoundError: No module named 'pytest'`, own top-level import | **PASS** for all 7 — `errors=1`, names `['pytest']`, `Ran=1` each; no `tools.*` or in-repo name among them |
| **(b)** blob at `M` equals blob at `766ecef` | **PASS** for all 7 (identical blob hashes recorded) |
| **(c)** the step-2 interpreter also lacks pytest | **PASS** — `C:\Python313` 3.13.2, `find_spec('pytest')` is `None` |
| **(d)** every step-2 module still passes with `Ran ≥` its step-2 count | **PASS** — 33/33 present, **0** regressions, **0** lost coverage |

**Disposition: all 7 are "not exercised: new at M, dependency absent (pytest)" — NOT a FAIL.** Rows 2–6
are unmatched; step 7 proceeds.

**Session compliance:** no `pytest` was installed; none was borrowed from the unrelated ComfyUI venv the
Advisor identified; no other interpreter or venv was used for the criterion. The shim probe
(`docs/reviews/a4s-r6-pytest-modules-sound.md`) is recorded **only** as a diagnostic supporting
"environment, not code" and **witnesses no criterion**, as the ruling requires.

### Operative reading of HA edit (b) — recorded per the Advisor

**Edit (b) is discharged as: "within each switch containing `case 138`, keep the FIRST occurrence and
delete the rest."** The packet's original wording (*"locate each by text"*) was insufficient because
**both copies within a switch are byte-identical**; the Advisor confirms this reading reproduces the
positions it named (`8040`, `8502`). The post-condition is **exactly one `case 138` site per switch**.

### New claim limit on `AC-STRUCT` itself — recorded per the Advisor

**`AC-STRUCT` detects duplicates, not omissions.** The switch-5 damage (ordinal 138 reduced to **zero**
cases) was **invisible** to the scanner — it was caught only because the duplicate in switch 4 prompted a
fuller look. The gap is now closed for this merge by the explicit post-condition count (one `case 138`
per switch), not by the scanner. **For any future revision, each HA edit should carry an
expected-count post-condition enforced as an `AC-MERGE` check**, rather than relying on the scanner to
find deletions.

### Gated lead (binding on later planning) — carried to the plan

**Before ANY packet regenerates or relifts with the toolkit at `M` or later, all 56+ KX modules —
including the 7 pytest-dependent ones — must run under real `pytest` in an owner-authorized environment.
Parametrized and fixture tests are included.**

---

## Net effect

**Resume at P0. P0.7 is now PASS under Q-B, and P0.10 is PASS by branch (a). Continue to step 1 as
verify-and-reuse.**

### Session compliance notes

- **Q-A:** today branch **(a)** holds for both pinned files, so it is the branch recorded. The binary-safe
  branch-(b) command is recorded above for use if a future checkout converts line endings. **No
  `.gitattributes` was added and none will be** — the Advisor ruled that is owner/packet-scope work.
- **Q-B:** the four fail-closed conditions are re-verified immediately before step 1 and their outputs
  recorded. **No `branch -f`, no `-D`, no re-creation, and no other ref is created.**
- The Advisor's suggested future-revision wording is recorded here as a **revision note, not a reopening**;
  §5.4 governs when a frozen packet may actually be revised.
