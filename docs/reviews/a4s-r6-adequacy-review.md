# A4s-r6 adequacy review (§5.3) — VERBATIM, by a fresh Planner

**Reviewer:** child `95429607-3d77-44a9-8f38-47e978ece759`, route `workbuddy-ai` / `kimi-k3`,
`reasoning_effort` omitted per the owner's instruction. A **fresh** Planner that did **not** author the
packet, as `docs/agent-workflow.md` §5.1.5 requires.
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Revision reviewed:** `A4s-r6`, SHA-256
`75207C41B8E9964D3E1467F5E980D1B8DE4CB75A1E0AFE954F59D40FA27D86E3`.

---

```
REVISION:          A4s-r6  |  SHA-256 OBSERVED: 75207C41B8E9964D3E1467F5E980D1B8DE4CB75A1E0AFE954F59D40FA27D86E3
                   (matches expected exactly)  |  line count OBSERVED: 374 LF lines, 0 CRLF, trailing
                   newline, 63462 bytes. The "325 lines" in the original brief was a NON-EMPTY-line
                   miscount by the writing Planner; your correction (374) is confirmed. SHA is the
                   binding identity check and it matches, so the bytes reviewed are the frozen revision.
READ:              docs/agent-workflow.md (all 833 lines; §5.3, §5.1.5, §5.4, §3.1, §6.1, §6.2 in full);
                   docs/packets/a4s-r6-toolkit-sync.md (all 374 lines);
                   docs/reviews/a4s-r6-advisor-hunk-ruling.md (all 268 lines);
                   docs/reviews/a4s-r5-conflict-inventory.md (all 60 lines);
                   logs/a4s/conflict-hunk-inventory.txt (all 185 lines);
                   docs/reviews/a4s-execution-evidence.md (all 348 lines);
                   docs/reviews/a4s-r6-ac-struct-tool.md (all 129 lines);
                   docs/reviews/a4s-r6-ac-struct-controls.md (all 68 lines);
                   docs/reviews/a4s-r6-h1-repair-proposal.md (all 231 lines);
                   docs/packets/a4s-toolkit-sync.md (all 263 lines, A4s-r5);
                   scripts/check-merge-structure.py (hashed + grepped; CLI confirmed: only --rev/--json/--expect);
                   tests/test_merge_structure.py (hashed).
                   INDEPENDENTLY VERIFIED this session: scanner SHA-256 = A1FDCE26…AFF (matches pin);
                   fixtures SHA-256 = A42F4A1E…C4C (matches pin); C text Appendix A vs ruling §1 = 0 content
                   diffs (normalizing only the ruling's "> " quote prefix + header comment lines);
                   Expected-5 comment block = upstream's 6 lines verbatim from the inventory.
PREMISE_FRESHNESS: BOUNDED — the conflict inventory, the binding Advisor ruling, the structural findings,
                   the pinned tool hashes, and the scanner controls are all current-session records produced
                   against this exact conflict set; the merge base/tags/remotes/baseline SHAs are unchanged
                   from the executed A4s-r5; the reference stop is A4a-r2 R0. Every stale-premise path fails
                   closed (P0 fail→R-PRE; conflict-set/text divergence→R-CONFLICT; scanner-hash mismatch→
                   R-CONFLICT; V fail→R-INVALID; F=0/B-shift→R-UNKNOWN), never to a false PASS. Not PASS
                   only because the resolved-all-ours positive control depends on an executor construction
                   step (see DEFERRED 1) whose fidelity rests on the recorded conflicted-* files; that
                   residual is stated and fail-closed.
BLOCKING:          NONE
DEFERRED:          (1) RESOLVE-TO-OURS CONSTRUCTION (flagged item 2): the scanner has NO built-in
                   resolve-to-ours mode (confirmed — only --rev/--json/--expect). The positive control's
                   construction (take every conflict hunk to ours into a scratch tree outside both repos,
                   use logs/a4s/conflicted-* for fidelity, do not touch main) is external to the pinned tool.
                   I judge it MECHANICAL ENOUGH for a literal executor: the resolution is uniform (all-ours,
                   no per-hunk judgment), the fidelity source is named, the exact expected output (exactly 3
                   structural findings, 0 markers) and both resolved/raw line-position sets are pinned, and
                   the Session independently reproduced the number (a4s-r6-ac-struct-controls.md). Borderline,
                   not blocking — the fidelity files live in gitignored logs/; if they are lost the control is
                   unverifiable. Recommend the Session confirm conflicted-* persist until execution.
                   (2) VERIFIERS AS PROVENANCE (flagged item 1): consistent with POLICY_ISSUE (1). That policy
                   requires GATING tools be tracked+pinned; verify-advisor-h2.py / verify-hunk5-output.py are
                   NOT gating — the H1/H2 rule text is fully specified in-packet and AC-TEST is the independent
                   gate, so no criterion invokes them. Keeping them as non-gating provenance is correct.
                   (3) SCANNER LINE COUNT: packet line 42 and a4s-r6-ac-struct-tool.md both say "358 lines";
                   the pinned file is 355 lines (355 LF, trailing newline). The SHA-256 pin — the operative
                   identity — matches exactly, and the line count is descriptive metadata used in NO
                   PASS/FAIL/UNKNOWN predicate. A wrong count in a note (§3.1: not blocking). Correct to 355.
                   (4) BRIEF/PACKET LINE-COUNT MISCOUNT: the "325" figure was the writing Planner's
                   non-empty-line count; the project convention (confirmed against A4s-r5 = 263 total/242
                   non-empty) is total lines. Record-keeping note only; the SHA bound the revision correctly.
DECISIONS:         Packet class = change (A4s-r5 revision per §5.4(3), Advisor ruling changed a depended-on
                   policy) — OBSERVED (ruling recorded, marked BINDING); reverse if the Advisor retracts.
                   All 9 hunks pre-ruled, none UNDECIDED — OBSERVED (table covers all 9; attributions match
                   the inventory; hunk-6 local-vs-upstream edit attribution verified against base/ours/theirs);
                   reverse if the real conflict set/text diverges (then R-CONFLICT, never analogy).
                   H1 repaired to multiset containment over additions AND deletions — OBSERVED matches the
                   Advisor-accepted proposal; reverse if a hunk shows it loses an edit.
                   H2 action = edit application, original precondition, "kept unchanged" not an edit,
                   presence-union forbidden, ambiguous attribution→UNDECIDED→R-CONFLICT — OBSERVED this is the
                   Advisor's ruling §2, NOT the rejected delta-12/corrected precondition (confirmed against
                   a4s-r6-h1-repair-proposal.md's supersession note); reverse per ruling REVERSED-BY.
                   Hunks 1-2 HA-COMBINED (in-place→ke_shadow_lookup→XBOX_TO_NATIVE, NO bridge_resolve_handle,
                   exactly one bridge_KeResetEvent) + edits (a)/(b) — OBSERVED byte-identical to ruling §1;
                   reverse per ruling REVERSED-BY (jsrf_inplace_event_bridge failing, etc.).
                   AC-STRUCT before build, over SCOPE, FAIL→R-CONFLICT never R-BUILD, C2084/C2196/C2371
                   backstop, positive control on RESOLVED all-ours=3 (all-theirs=2 named), negatives on both
                   parents=0, raw preview 15 a separate expected observation, fail-closed UNKNOWN — OBSERVED
                   decidable at command level; predicates match the ruling and the controls doc; reverse per
                   ruling §3 REVERSED-BY.
                   AC-MERGE(d-twin) deletion twin witness — OBSERVED present and correctly conditioned
                   (absent unless replaced or HA-restored); matches ruling §2.
                   AC-TEST set-G carve-out naming E2=tests/test_ac2_provenance.py by test identity, re-derived
                   per execution, no count hard-coded — OBSERVED; "same failing test identity" is literally
                   decidable (E2's failing checks are named: A0/P1); the 10/1-over-11 figure is the current
                   measurement, explicitly re-derived, matching the tool doc's recorded 8/2→9/1→10/1 drift.
                   Gating scanner+fixtures tracked and SHA-256-pinned, verifiers provenance — OBSERVED both
                   hashes match; satisfies POLICY_ISSUE (1).
                   No push during execution; Closure push to origin only after ACCEPT with 5 pre-push checks;
                   R-CONFLICT/rollback/INADEQUATE no-push — OBSERVED; matches owner push policy.
                   "Expected R-SAME" is a forecast only, no criterion assumes it — OBSERVED (decision-row
                   header states it explicitly); satisfies POLICY_ISSUE (3).
VERDICT:           ADEQUATE
```

## Adversarial check summary (the six the Session named)

> (a) hunk-1/2 C text byte-identical to the ruling — **PASS**;
> (b) 9-hunk table complete, none UNDECIDED, attributions correct — **PASS**;
> (c) H2 is the Advisor's edit-application version, not the rejected precondition — **PASS**;
> (d) AC-STRUCT PASS/FAIL/UNKNOWN and controls decidable by a literal executor — **PASS**
> (positive-control construction is external but specified, see DEFERRED 1);
> (e) no criterion silently assumes R-SAME — **PASS**;
> (f) R-CONFLICT reachable from every fail-closed path (AC-STRUCT FAIL/UNKNOWN, conflict-set divergence,
> ambiguous attribution, build backstop) — **PASS**.
>
> No blocking defect found. The two items the writing Planner flagged are both acceptable as designed
> (DEFERRED 1 and 2). One factual correction for the record (scanner is 355 lines, not 358) — advisory
> only. Per §5.3, ADEQUATE ends plan iteration: freeze this exact revision, record this review, and
> promote it into CURRENT PACKET in the same step. The one persistence risk worth your attention before
> execution: confirm logs/a4s/conflicted-* survive until the AC-STRUCT positive control runs (DEFERRED 1).

---

## Session disposition of the review

**`VERDICT: ADEQUATE` with `BLOCKING: NONE`.** Per `docs/agent-workflow.md` §5.3, `ADEQUATE` requires
`BLOCKING = NONE` **and** `PREMISE_FRESHNESS` not `FAIL` — both hold — so **plan iteration ends**. The
revision is **frozen at SHA-256 `75207C41…86E3`** and **promoted in the same step**, with **no polishing
pass**: editing the packet now would change its SHA and invalidate this review.

### DEFERRED 1 — the one persistence risk — CLOSED with stronger evidence than requested

The reviewer asked the Session to *"confirm `logs/a4s/conflicted-*` persist until the AC-STRUCT positive
control runs"*. Confirming the files exist would have been weak: they are in **gitignored** `logs/`
(`.gitignore:3:/logs/`), so their survival is not guaranteed by anything. The Session therefore tested
whether the risk is real at all, and **it is not**:

1. **The fidelity source is durable in git.** The raw preview **`75083476` is a git tree object**, and
   its conflicted files are recoverable at any time with `git show 75083476:<path>`. It is not
   `logs/`-dependent.
2. **The saved files are NOT byte-identical to the preview blobs** — and the reason matters. Measured:
   the saved files are **diff3** (3 `<<<<<<<`, **3** `|||||||`, 3 `>>>>>>>`) while `git merge-tree
   --write-tree` emits **2-way** markers (3 `<<<<<<<`, **0** `|||||||`, 3 `>>>>>>>`); the preview blob of
   `kernel_bridge.c` is 9 lines shorter, consistent with the 3 omitted base sections. This is the same
   2-way/diff3 distinction that already caused one Session error earlier in this work.
3. **The distinction does not change the control.** Resolving **both** sources to all-ours yields
   **byte-identical content for all five conflicted files** (`verify-scope-positive-control.py`,
   section 2), so the control cannot depend on which source the executor uses.
4. **The positive control reproduces exactly as the packet states.** Over `SCOPE`, resolved all-ours
   from the 2-way preview **and** from the diff3 saved files both give **3 findings, 0 UNKNOWN**:
   `dup-case case 138 [8019,8322]`, `dup-case case 138 [8481,8847]`, `dup-def bridge_KeResetEvent
   [1377,6713]` — i.e. exactly the 2 + 1 the criterion expects, with 0 markers.

**A Session error found and corrected while closing this.** The first attempt at this verification
resolved **all five** conflicted files, including the three under `tools/recomp/` — which are **outside
`SCOPE`** (they are toolkit tooling, not build inputs of `jsrf_recomp.exe`). Feeding Python to a
C-oriented scanner produced **79 spurious `dup-def` findings and one UNKNOWN** in `lifter.py`. The
corrected run restricts the control to the two in-scope files. Recorded because it is precisely the
class of mistake `AC-STRUCT`'s `SCOPE` guard exists to prevent, and because a reader who repeats it will
see a failing control that is not a real defect.

### DEFERRED 3 — accepted as an advisory, deliberately NOT fixed

The reviewer is right that the scanner is **355 lines**, not the 358 stated in packet line 42 and in
`a4s-r6-ac-struct-tool.md`. Verified. It is **not** corrected in the packet, because the line count is
descriptive metadata used in **no** PASS/FAIL/UNKNOWN predicate, and editing the packet would change its
SHA-256 and invalidate this review — the §5.3 rule is explicit that `ADEQUATE` ends iteration. The
correction is recorded here instead, and in the tool record.

### DEFERRED 2 and 4 — accepted

Both are consistent with the Session's reading and with the Advisor's `POLICY_ISSUE` items. No action.
