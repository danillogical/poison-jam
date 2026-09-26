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
