# A4s-r6 Advisor hunk ruling (VERBATIM)

**Advisor:** child `5c555969-dea9-4b47-be05-62aa0835cde2`, route `claude` / `claude-opus-5-5` @ `high`
— the persistent Advisor probed at startup in
`docs/reviews/startup-20260925-session-58e86358.md`, reused per `docs/agent-workflow.md` §4.4.
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Scope:** the original brief plus fourteen Session deltas; one consolidated ruling covering the five
UNDECIDED hunks, the H1/H2 rule property, and the post-resolution check.
**Status:** **BINDING** from the moment of recording (`docs/agent-workflow.md` §3.3). A4s-r6 drafting
proceeds on this basis.

The Advisor's own disclosure, recorded verbatim: *"What I did: read-only `git show`/`git grep` in the
toolkit, nothing else. I also ran one AST parse of lifter set literals on copies placed in
%TEMP%\adv-sets. It ran outside both repositories and produced no evidence anyone needs to rely on."*

---

## 1. HUNKS 1–2 (+ A: KeResetEvent, F: case 138, G/K)

> RULING: a COMBINED form. The precedence is fixed:
>   (1) in-place KEVENT, if `guest_va_is_inplace_kevent(va)` accepts it
>   (2) the shadow handle, if `ke_shadow_lookup(va)` is non-NULL
>   (3) the base/local fallback `XBOX_TO_NATIVE(va)`
> Upstream's `bridge_resolve_handle` tier is NOT carried over. Exactly one `bridge_KeResetEvent` survives, and it has the same three tiers.
>
> The executor replaces each whole function with the exact text below. The replacement covers everything from `static void …(void)` through its closing brace, including the non-conflicted header lines inside the function, because upstream's header renamed `event_ptr` to `guest_va`.
>
> ```c
> static void bridge_KeSetEvent(void)
> {
>     uint32_t event_ptr = STACK_ARG(0);
>     uint32_t increment = STACK_ARG(1);
>     uint32_t wait = STACK_ARG(2);
>     HANDLE h;
>
>     recomp_diag_record(10, event_ptr, g_xbox_kernel_caller, 0);
>     if (guest_va_is_inplace_kevent(event_ptr)) {
>         g_eax = (uint32_t)xbox_KeSetInplaceEvent(
>             event_ptr, XBOX_TO_NATIVE(event_ptr), BRIDGE_MEM8(event_ptr),
>             increment, (BOOLEAN)wait);
>     } else if ((h = ke_shadow_lookup(event_ptr)) != NULL) {
>         g_eax = (uint32_t)xbox_KeSetEvent(h, increment, (BOOLEAN)wait);
>     } else {
>         g_eax = (uint32_t)xbox_KeSetEvent(
>             XBOX_TO_NATIVE(event_ptr), increment, (BOOLEAN)wait);
>     }
>     recomp_diag_record(10, event_ptr, g_xbox_kernel_caller, g_eax);
> }
>
> static void bridge_KeResetEvent(void)
> {
>     uint32_t event_ptr = STACK_ARG(0);
>     HANDLE h;
>
>     recomp_diag_record(11, event_ptr, g_xbox_kernel_caller, 0);
>     if (guest_va_is_inplace_kevent(event_ptr)) {
>         g_eax = (uint32_t)xbox_KeResetInplaceEvent(
>             event_ptr, XBOX_TO_NATIVE(event_ptr), BRIDGE_MEM8(event_ptr));
>     } else if ((h = ke_shadow_lookup(event_ptr)) != NULL) {
>         g_eax = (uint32_t)xbox_KeResetEvent(h);
>     } else {
>         g_eax = (uint32_t)xbox_KeResetEvent(XBOX_TO_NATIVE(event_ptr));
>     }
>     recomp_diag_record(11, event_ptr, g_xbox_kernel_caller, g_eax);
> }
>
> /* ── KeWaitForSingleObject (ordinal 159) ─────────────────── */
> static void bridge_KeWaitForSingleObject(void)
> {
>     uint32_t diag_object = STACK_ARG(0);
>     recomp_diag_record(8, diag_object, g_xbox_kernel_caller, 0);
>     uint32_t object = STACK_ARG(0);
>     uint32_t wait_reason = STACK_ARG(1);
>     uint32_t wait_mode = STACK_ARG(2);
>     uint32_t alertable = STACK_ARG(3);
>     uint32_t timeout_ptr = STACK_ARG(4);
>     HANDLE h;
>
>     if (guest_va_is_inplace_kevent(object)) {
>         g_eax = (uint32_t)xbox_KeWaitInplaceEvent(
>             object, XBOX_TO_NATIVE(object), BRIDGE_MEM8(object),
>             (BOOLEAN)alertable, XBOX_TO_NATIVE(timeout_ptr));
>     } else if ((h = ke_shadow_lookup(object)) != NULL) {
>         g_eax = (uint32_t)xbox_KeWaitForSingleObject(
>             h, wait_reason, wait_mode,
>             (BOOLEAN)alertable, XBOX_TO_NATIVE(timeout_ptr));
>     } else {
>         g_eax = (uint32_t)xbox_KeWaitForSingleObject(
>             XBOX_TO_NATIVE(object), wait_reason, wait_mode,
>             (BOOLEAN)alertable, XBOX_TO_NATIVE(timeout_ptr));
>     }
>
>     recomp_diag_record(9, diag_object, g_xbox_kernel_caller, g_eax);
> }
> ```
>
> The executor also makes these edits outside the conflict hunks. A4s-r6 must authorize each one explicitly as a pre-ruled HA edit:
>   (a) Delete upstream's clean-hunk KeResetEvent at merge-tree lines 6733–6746: the `/* --- KeResetEvent (ordinal 138 …) --- */` comment through the closing brace of that function.
>   (b) Keep exactly one `case 138` per switch:
>       - delete `case 138: return  4;  /* KeResetEvent (1) */` at 8343
>       - delete `case 138: return bridge_KeResetEvent;` at 8868
>       - keep 8040 and 8502, which are local's positions
>      The line numbers refer to 75083476. The executor must locate each edit by its exact text, not by line number.
>
> BASIS:
> - OBSERVED — the in-place tier is required: the accepted, gating ctest `jsrf_inplace_event_bridge` asserts guest SignalState and previous-state values at test lines 81, 86, 91 and 92. Your delta 7 shows it reads the same address that local's model writes. Upstream's bodies write no guest memory. So UPSTREAM alone is ruled out from source, and no experiment is needed.
> - OBSERVED — the merged event family would be split: in the merge tree, NtClearEvent, NtSetEvent, NtPulseEvent and NtWaitForSingleObject all dispatch in-place first (for example, 75083476:1476). Taking upstream for the Ke* functions would split one guest KEVENT between two object models.
> - OBSERVED — why the shadow tier is required, and why LOCAL alone is wrong in the merged tree: upstream's timer model arrives in a clean hunk. At 75083476:2328–2358, `KeInitializeTimerEx` does `ke_shadow_insert(timer_va, ev)`, and the timer thread at 2451–2454 signals that shadow event. JSRF declares ordinals 113 and 159.
>   - Under LOCAL-only, a KeWaitForSingleObject on a KTIMER fails the sniffer on type (8/9) and waits on `XBOX_TO_NATIVE(timer)` cast to a HANDLE. The event the merged timer machinery actually signals is never consulted.
>   - This decides the question: a reachable clean-hunk populator plus a reachable consumer. It is not the latent Size scenario.
>   - Whether JSRF actually waits on a timer is not measured, and the ruling does not depend on it. The combined form is a strict superset for every object that reaches the Ke* bridge.
> - OBSERVED — why in-place comes first:
>   - `ke_shadow_remove` has zero callers in 75083476 (only its definition at 5900), so shadow entries are never retired. With shadow first, a guest VA reused as an in-place KEVENT after holding a timer would route to a dead event.
>   - Every reachable shadow populator writes type 8/9, which the sniffer rejects. So in-place-first never takes an object away from the shadow tier.
>   - As a side effect, the delta-8 Size hazard is defused without deciding the Size question: an upstream-initialised event (Size 16) fails the sniffer and is served correctly by the shadow tier.
> - OBSERVED — why no `bridge_resolve_handle` tier:
>   - In the merge tree, `bridge_resolve_handle` returns the token itself for any untagged value (2709–2710). For every nonzero VA it would therefore pass a guest VA as a host HANDLE and make tier 3 unreachable.
>   - Ke* take object pointers, not handles.
>   - Tier 3 is exactly the base/local behaviour, so an object no model knows keeps today's behaviour. That behaviour is itself a pre-existing type confusion; that is a lead, not something this merge introduced.
> - OBSERVED — `bridge_KeResetEvent` is not low-stakes: ordinal 138 is unreachable in JSRF, but the gating ctest calls it directly through `xbox_test_bridge_KeResetEvent` (merge 9383–9389) and asserts "previous 1" and "stores 0". Only the in-place body satisfies that. JSRF reachability does not decide which body survives; the test gate does.
> - OBSERVED — the two `case 138` sites in each switch are byte-identical (8040 = 8343; 8502 = 8868). They are the same logical addition, and both give 4 bytes for one argument. Both switches must stay consistent: one 138 in each, and the args value must match the single surviving bridge's arity.
> - INFERRED: tier 2 of KeSetEvent uses `xbox_KeSetEvent(h, …)`, which returns 0 ("previous state unknown"), instead of upstream's `SetEvent` BOOL. That matches local's handle-typed contract and upstream's own KePulseEvent and KeResetEvent. This is a small technical choice; no evidence depends on it.
> - UNCERTAIN (lead only): the correct Size convention. From NT convention, Size = sizeof/sizeof(LONG), i.e. 4 for KEVENT and 10 for KTIMER. That would make upstream's 16 and 40 wrong. I have no primary source this session, and the combined form makes the question non-blocking.
>
> REVERSED BY:
> - ctest `jsrf_inplace_event_bridge` failing on the merged tree with this form. That would falsify "in-place tier unchanged".
> - A measured guest Ke* call that passes a tagged NtCreateEvent handle. That would argue for a `bridge_resolve_handle` tier.
> - A reachable object that is both sniffer-accepted and shadow-registered at the same VA, where shadow must win.
>
> RECORD IN: docs/reviews/a4s-r6-advisor-hunk-ruling.md (verbatim). A4s-r6 should carry hunks 1 and 2 and edits (a)–(b) as HA pre-rulings with this exact text. AC-TEST must name `jsrf_inplace_event_bridge` as the witness for this ruling.

---

## 2. H1/H2 RULE PROPERTY (B2, L, M)

> RULING — the property is: a deletion is an edit. Every rule must represent deletions both in its comparison and in its application, and every witness must check them. Concretely:
>   H1 (ACCEPT your proposal): X contains Y iff multiset added(Y) ⊆ added(X) AND deleted(Y) ⊆ deleted(X). If neither direction holds, H1 does not apply. If the two versions are identical, take either.
>   H2 (REJECT the delta-12 and corrected-delta precondition (a); KEEP the existing precondition): H2 applies iff no base line is changed by both sides. Changed means deleted or replaced. H2's action must be defined as EDIT APPLICATION: start from base, apply each side's per-line edit (keep, delete, or replace), then insert each side's insertions (local first at a shared point). A union by line presence is forbidden. That presence-union is the true H2 analogue of the vacuity defect, because it resurrects deleted lines.
>      If the per-line attribution is ambiguous (repeated identical lines, or several alignments of equal cost), H2 does not apply and the hunk is UNDECIDED.
>   AC-MERGE(d) witness: the existing check that "added lines are present" gets a twin — every base line a credited side deleted is ABSENT from the result, unless the other side replaced it.
>
> BASIS:
> - OBSERVED, and this is where I disagree with your H2 repair: "kept unchanged" is not an edit.
>   - On hunk 5, your own attribution is: changed by ours = [0], changed by theirs = []. H2's precondition holds.
>   - Edit application gives: delete "lock xadd", then add upstream's 6 comment lines and "popfd".
>   - That is exactly the correct semantic form (see §4). Git flagged the hunk only because the two edits are adjacent.
> - So repaired H1 plus edit-application H2 decides hunk 5 correctly and mechanically. Your precondition (a) would send a correctly decidable hunk to UNDECIDED: it errs in the safe direction, but it is wrong.
> - The repair was never "H2 must refuse delete-vs-keep". It was "H1 must not grab the hunk first", plus "H2 must not be implemented as a union of line presence".
> - To your question M: repairing H1 alone IS sufficient for hunk 5, provided H2 is specified as edit application. If H2 is left as the loose "keep both sides' edits" read as a union of lines, it is not sufficient. Pin the edit-application definition.
> - INFERRED: a cross-set contradiction check ("token in two exclusive tables") is NOT required. Once H1 and H2 represent deletions, the contradiction does not arise from this merge, and you retracted the coupling premise in delta 6. A generic checker cannot know which tables are order-sensitive anyway.
> - H3 is not audited (no corpus). Say so in the packet.
>
> REVERSED BY: a real or synthetic hunk where the repaired H1 plus edit-application H2 yields a result that loses one side's edit, or keeps a line a credited side deleted. Re-run your 9-hunk corpus and your 14 synthetic shapes under this H2 definition. I expect hunk 5 to change from UNDECIDED to H2 and every other hunk's classification to stay the same.
>
> RECORD IN: the A4s-r6 step-3 rule text (Planner wording) and docs/reviews/a4s-r6-h1-repair-proposal.md. Add an addendum that supersedes the delta-12 and corrected H2 precondition.

---

## 3. POST-RESOLUTION CHECK (E, N)

> RULING: YES, a structural check that is not conflict-driven — (i) only. It runs on the RESOLVED tree before the build, over SCOPE (the build inputs of the evidence binary, per the existing scope ruling). No generic semantic "every reachable difference" review.
>
> Property it must establish: every translation unit in SCOPE that the merge changed contains
>   (a) no conflict markers;
>   (b) no two `case` labels with the same constant value in one switch, on the same preprocessor branch path;
>   (c) no two file-scope definitions of the same identifier on the same branch path. Declarations plus one definition, tentative definitions, and identical macro redefinitions are excluded, per your Finding 3.
>
> Predicates:
> - PASS: zero hits, AND the coverage witness holds:
>   - positive control: the scanner reports the known three defects on 75083476 — `case 138` in both switches and `bridge_KeResetEvent`;
>   - negative control: it reports zero on 0d7929c and 766ecef;
>   - it reports its file, switch and definition counts.
> - FAIL: any hit. This selects the conflict-class row (R-CONFLICT, reason "structural"), never R-BUILD.
> - UNKNOWN: the scanner cannot assign braces or branch paths in a changed file. That file is listed, and the check does not PASS.
> - Backstop so the check cannot be bypassed by attribution: R-BUILD may be selected only if the first compiler error is not a redefinition or duplicate-case diagnostic (C2084 / C2196 / C2371 class) in a merge-changed file. Otherwise the row is conflict-class.
>
> (ii) NOT required. Semantic clean-hunk coverage is already bounded by existing policy: run-profiles rules 1–5 (a by-content inventory for mechanisms relevant to devices and profiles) plus the test gates. A generic reachable-difference review is an unbounded re-review of the upstream diff.
>
> BASIS:
> - OBSERVED: the three structural defects exist only in clean hunks, and all four existing rules are blind to them. §3.1 forbids the resulting misattribution to R-BUILD.
> - INFERRED: the timer change is coherent, and no A4s-r6 criterion depends on it beyond what hunks 1–2 already absorb. That makes it a follow-up lead, as you proposed. It did bear on this ruling (it is why the shadow tier exists), and that link is recorded above.
>
> REVERSED BY:
> - A merge-introduced compile-class defect that (a)–(c) cannot express. Extend the list; do not switch to (ii).
> - An owner decision to make the clean-hunk semantic inventory broader than run-profiles rule 4.
>
> RECORD IN: A4s-r6 as a new AC before AC-BUILD, with its own row or its own reason under R-CONFLICT.

---

## 4. HUNK 5 (B1, H)

> RULING: a per-line combined form. It equals the output of repaired H1 plus edit-application H2, so it is mechanical. The final text of `_FLAGS_UNDEFINED` is:
> ```
> _FLAGS_UNDEFINED = frozenset({
>     "mul", "div", "idiv",  # Flags partially undefined
>     "rdtsc", "cpuid",      # Special instructions
>     # popfd REPLACES every flag … (upstream's 6 comment lines, verbatim)
>     "popfd",
> })
> ```
> No "lock xadd" appears in it. `_EFLAGS_SETTERS` keeps local's `"xadd", "lock xadd", "lock cmpxchg",` line, which is already the merged text. Hunk 6's H2 result (`"pushfd", "pushal",` / `"sgdt", "ljmp", "sfence", "wbinvd",`) stands.
>
> BASIS:
> - OBSERVED: b3de85c is a move, and in the merge tree it is corroborated by `_EFLAGS_SETTERS`, translator.py, the `_fa = _old + _add` snapshot in the xadd lift (lifter 1706), and the `_fa`-based xadd condition (963).
> - OBSERVED: the `lock ` prefix is stripped in `_make_condition` (504), so the tracked setter resolves.
> - OBSERVED: popfd has upstream tests (`test_lifter_popfd_flags.py`, `test_lifter_carry.py:161`) that the merge brings in.
>
> REVERSED BY: `test_lifter_atomics` or `test_lifter_popfd_flags` failing on the merged tree.
>
> RECORD IN: the ruling file. A4s-r6 hunk 5 is then derived by rule, not pre-ruled. The ruling serves as the expected output the executor's result must match.

---

## 5. HUNK 9 (D, I)

> RULING: the full union, with upstream's clause kept:
> ```
>         if any(insn.mnemonic in ("cmp", "test", "bsf", "bsr", "cmpxchg",
>                                  "lock cmpxchg", "xadd", "lock xadd", "inc", "dec")
>                or insn.mnemonic in _RESULT_SNAPSHOT_SETTERS
>                for insn in instructions):
> ```
> Order: local's members, then upstream's.
>
> BASIS:
> - OBSERVED: this is a DECLARATION guard for `_fa`, `_fb`, `_fas` and `_fbs`. `uint32_t _fa` is declared only at translator 994.
> - In the merged lifter, the lifts that write `_fa` are: bsr (1668), xadd (1706, local), cmpxchg (1720), `_result_snapshot` (1866, upstream, the `_RESULT_SNAPSHOT_SETTERS` family), inc/dec (1913–1914, upstream), and cmp/test (2166).
> - Both sides' writers are therefore present. Dropping either side's mnemonics leaves generated C that references an undeclared `_fa`. Over-declaration is harmless because of the `(void)` casts.
> - The deciding fact was "which lifts emit `_fa =` in the merged lifter". It is a source read, and it is done. Your delta-4 correction was right; the question is consistency, and it resolves to the union.
>
> REVERSED BY: a lift in the merged lifter that writes `_fa` for a mnemonic outside this guard (then add it), or evidence that one of these lifts no longer writes `_fa`.

---

## 6. HUNK 8 (C, J)

> RULING: LOCAL, meaning `"--functions", fns,`. Pin it; do not leave it to judgment.
>
> BASIS:
> - OBSERVED (your execution): both forms give the same outcome for this test.
> - OBSERVED: the surviving non-conflicted lines 106–114 (the comment plus the `fns` creation) exist only because local added them (9568f29) and describe the local form. LOCAL keeps the test coherent; UPSTREAM leaves dead setup.
>
> REVERSED BY: nothing material. This is a coherence choice.

---

## FOLLOW-UP LEADS (not A4s-r6 obligations)

> - The Size convention (4 vs 16/40): needs a primary source.
> - Tier 3's handle-typed treatment of a non-handle Ke* object (pre-existing in base and local).
> - `ke_shadow_remove` has no callers, so shadow entries are never retired.
> - The upstream timer model's observable behaviour for JSRF (113/149 reachability witness).
> - An indirect-thunk-call search behind "not declared ⇒ unreachable".
> - Case 207, as you listed it.
>
> Prior leads carry forward unchanged: MIXDOWN_ALL/USB_PORT classification, E2, the function-style KX modules, and the exe-relink advisory.

---

## Consequence: `R-CONFLICT`'s next step is unblocked

The `R-CONFLICT` row's next action was *"Planner `A4s-r6` with the full hunk inventory as its brief,
naming a resolution per hunk; hunks that choose between local accepted runtime and an upstream model
(…) go to the Advisor first."* **That Advisor step is now complete and recorded.** All five UNDECIDED
hunks have rulings; hunks 3, 4, 6 and 7 keep their existing classifications; the H1/H2 rule defect has
a ruled property; and the post-resolution check has a ruled shape. The Planner brief
(`docs/reviews/a4s-r6-planning-brief-draft.md`) can now be frozen and sent.
