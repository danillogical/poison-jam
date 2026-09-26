# A4s-r6: H1 repair proposal, and the Advisor's ruling that supersedes my H2 proposal

> ## ADVISOR RULING — READ THIS FIRST
>
> The Advisor (`docs/reviews/a4s-r6-advisor-hunk-ruling.md` §2) **accepted my H1 proposal** and
> **rejected my H2 proposals** (both the delta-12 version and the "corrected" one). Its words:
>
> > **H2 (REJECT the delta-12 and corrected-delta precondition (a); KEEP the existing
> > precondition):** H2 applies iff no base line is changed by both sides. Changed means deleted or
> > replaced. H2's action must be defined as **EDIT APPLICATION**: start from base, apply each side's
> > per-line edit (keep, delete, or replace), then insert each side's insertions (local first at a
> > shared point). **A union by line presence is forbidden.** That presence-union is the true H2
> > analogue of the vacuity defect, because it resurrects deleted lines.
> > If the per-line attribution is ambiguous (repeated identical lines, or several alignments of equal
> > cost), H2 does not apply and the hunk is UNDECIDED.
>
> And on why my version was wrong:
>
> > **OBSERVED, and this is where I disagree with your H2 repair: "kept unchanged" is not an edit.**
> > On hunk 5, your own attribution is: changed by ours = [0], changed by theirs = []. H2's
> > precondition holds. Edit application gives: delete "lock xadd", then add upstream's 6 comment
> > lines and "popfd". That is exactly the correct semantic form. Git flagged the hunk only because
> > the two edits are adjacent.
> > So repaired H1 plus edit-application H2 decides hunk 5 correctly and mechanically. Your
> > precondition (a) would send a correctly decidable hunk to UNDECIDED: it errs in the safe
> > direction, but it is wrong.
> > The repair was never "H2 must refuse delete-vs-keep". It was "H1 must not grab the hunk first",
> > plus "H2 must not be implemented as a union of line presence".
>
> **The Advisor's expectation was verified by the Session and confirmed exactly**
> (`logs/a4s/verify-advisor-h2.py`, `logs/a4s/verify-hunk5-output.py`):
>
> | Claim | Measured |
> |---|---|
> | hunk 5 changes from UNDECIDED to **H2 applies** | **True** |
> | every other hunk keeps its classification | **True** (hunk 3 → `H1 → OURS`; lifter 6/7 → H2 applies; hunks 1, 2, 8, 9 → H2 does not apply) |
> | edit application produces the ruled `_FLAGS_UNDEFINED` text | **True** — no `lock xadd`, `popfd` present, exactly 6 comment lines |
>
> **The sections below are my proposals and are retained only as the reasoning trail. Where they
> conflict with the ruling, the ruling wins.** In particular, the "Proposed H2 repair" section and its
> retraction are **superseded**: the Advisor's edit-application definition is the operative rule, and
> its precondition is the *original* one ("no base line changed by both sides"), not my added
> delete-vs-keep clause.

---

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** a **tested proposal** for the Planner, offered as input to the Advisor's rule ruling. Not a
ruling, and not yet a packet criterion.
**Script:** `logs/a4s/h1-repair3.py` (runs against the **real diff3 hunks** saved from the `A4s-r5`
merge, not the 2-way preview tree).

## The defect, restated

`A4s-r5` step 3's **H1** is:

> *"Every non-blank line one side added in the hunk is also present in the other side's version of the
> hunk → take the containing side."*

It tests **additions only**. When one side added nothing — it only **deleted** — the antecedent is
**vacuously true**, so H1 "succeeds" and returns the *other* side, silently discarding the deletion.
Measured instance: `lifter.py` hunk 5, where local deleted `"lock xadd"` from `_FLAGS_UNDEFINED`
(having **moved** it to `_EFLAGS_SETTERS` in commit `b3de85c`) and upstream kept it.

## The proposed repair

Make the comparison account for **both** kinds of change. For side *X*, against the hunk's base
section:

- `added(X)` = non-blank trimmed lines in *X*'s version, **minus** those in the base section
- `deleted(X)` = non-blank trimmed lines in the base section, **minus** those in *X*'s version

Then:

```
X contains Y   iff   added(Y) ⊆ added(X)   AND   deleted(Y) ⊆ deleted(X)
```

and H1 applies as before:

| Condition | Result |
|---|---|
| versions identical | H1 does not decide (not its job) |
| `X contains Y` and **not** `Y contains X` | **H1 → take X** |
| neither contains the other | **H1 does not apply** → fall through to H2 / H3 / UNDECIDED |

A **deletion is work**: if *Y* deleted a base line and *X* did not, *X* does not contain *Y*, so H1
refuses instead of silently restoring the line. Comparison is on non-blank **trimmed** lines as
multisets, matching the packet's existing "non-blank line" language and tolerating whitespace-only
differences.

## Tested against the real hunks

Run over the five saved conflicted files (`logs/a4s/conflicted-*`, diff3 style, 9 hunks):

| File | Hunk | lines | ours added/deleted | theirs added/deleted | Repaired H1 | Recorded |
|---|---|---|---|---|---|---|
| `kernel_bridge.c` | 1 | 1365–1404 | 18 / 1 | 10 / 1 | does not apply | UNDECIDED ✓ |
| `kernel_bridge.c` | 2 | 1425–1445 | 7 / 0 | 1 / 1 | does not apply | UNDECIDED ✓ |
| **`kernel_bridge.c`** | **3** | **8981–8988** | **2 / 1** | **1 / 1** | **H1 → take OURS** | **H1 ✓** |
| `xbox_memory_layout.c` | 1 | 2123–2145 | 4 / 6 | 3 / 0 | does not apply | HA (pre-ruled) ✓ |
| **`lifter.py`** | **1 (hunk 5)** | **400–412** | **0 / 1** | **7 / 0** | **does not apply** | **UNDECIDED ✓ (was the bug)** |
| `lifter.py` | 2 (hunk 6) | 426–437 | 1 / 1 | 3 / 1 | does not apply | H2 ✓ |
| `lifter.py` | 3 (hunk 7) | 1276–1297 | 15 / 0 | 3 / 0 | does not apply | H2 ✓ |
| `test_icall_feedback.py` | 1 (hunk 8) | 116–124 | 2 / 1 | 2 / 1 | does not apply | UNDECIDED ✓ |
| `translator.py` | 1 (hunk 9) | 987–994 | 1 / 1 | 2 / 1 | does not apply | UNDECIDED ✓ |

**The two decisive cases both come out right:**

- **hunk 3** — the one hunk H1 legitimately decides — still yields **"H1 → take OURS"**, exactly the
  recorded classification. Local added 2 lines (a comment and the `RECOMP_TLS` line), upstream added 1
  (the same `RECOMP_TLS` line without the comment), and both deleted the same base line. Upstream's
  work is a subset of local's, so local is the containing side. **The repair does not disturb the
  classification that was already correct.**
- **hunk 5** — the defect — now yields **"H1 does not apply"**. Under the old rule it was vacuously
  contained and would have taken upstream, resurrecting `"lock xadd"` into `_FLAGS_UNDEFINED` while
  local's `_EFLAGS_SETTERS` still lists it.

## Properties the user's §5 requires

| Requirement | How the repair satisfies it |
|---|---|
| **Mechanical** | multiset differences of trimmed non-blank lines; no semantics, no call graph, no judgment |
| **Bounded** | scoped to one hunk's three text sections; work is linear in hunk size |
| **Safe for a literal executor** | the executor computes four sets per hunk and tests two subset relations; no step says "decide" |
| **Capable of PASS/FAIL/UNKNOWN** | each hunk yields exactly one of: H1-take-X, does-not-apply (→ H2/H3), or UNDECIDED; the packet's row logic already handles the third |
| **No unbounded semantic analysis** | nothing inspects program meaning; a hunk that cannot be decided mechanically stays **UNDECIDED**, which the user explicitly prefers over a mechanically wrong merge |

## H2 has the SAME defect class — measured, and it is not hypothetical

The user's §5 warned: *"Do not merely patch this observed hunk if the same H1 defect can produce a
wrong merge elsewhere."* The same defect **does** exist in **H2**, and it bites on **hunk 5 itself**.

`H2` is: *"Each base line in the hunk was changed by at most one side (the sides edited different
lines, or both only inserted at the same point) → keep both sides' edits; for pure same-point
insertions, local lines first, then upstream lines."*

Measured on `lifter.py` hunk 5 (script `logs/a4s/h2-deletion-check.py`), with `difflib` alignment of
each side against the base:

```
BASE  : ['    "lock xadd",           # Lock prefix - complex flag behavior']
OURS  : []                                        <-- the line DELETED
THEIRS: [ same line, + 6 comment lines, + '"popfd",' ]   <-- the line KEPT, 7 lines inserted after
```

| Base line | changed by ours | changed by theirs |
|---|---|---|
| the `"lock xadd"` line | **yes** (deleted) | **no** (kept; only inserted after it) |

`changed_by_ours ∩ changed_by_theirs = ∅`, so **H2's precondition HOLDS**. But H2's action — *"keep
both sides' edits"* — is **undefined** for that base line: ours' edit *is* the deletion, theirs' edit
is to *keep* it. You cannot both delete and keep a line. So:

> **H2's precondition does not exclude a base line that one side deletes and the other keeps, and
> H2's action is undefined in exactly that case.** It is the same defect class as H1's vacuous
> containment: **a deletion is not represented in the comparison, so the rule cannot see that work
> would be lost.**

This matters because hunk 5 is the hunk the user singled out, and under the packet's rule order the
executor reaching H2 after a repaired H1 refuses would find H2 apparently **satisfied** — and H2 is a
*mechanical* rule, so an executor would apply it rather than stop. That is the "mechanically wrong
merge" the user's §5 says is worse than `UNDECIDED`.

### Proposed H2 repair (same shape as H1)

**Corrected after edge-case testing — my first attempt was too crude and is recorded below as a
retraction.** The distinction that matters is between an **outright deletion** and a **replacement**:

| difflib opcode | Meaning |
|---|---|
| `delete` (or `replace` with an empty replacement) | the side **removed** the base line and put nothing in its place |
| `replace` (non-empty replacement) | the side **swapped** the base line for different text |

H2's action — *"keep both sides' edits"* — is **well defined** when the other side **replaced** the
line (both edits compose), and **ill-defined** only when one side **outright deletes** a base line
while the other **keeps it unchanged**. So:

> H2 applies only if (a) no base line was **outright deleted** by one side while **kept unchanged**
> by the other, and (b) no base line was **changed by both** sides. Otherwise H2 does **not** apply →
> `UNDECIDED`.

**Tested against the real corpus** (`logs/a4s/h2-corrected.py`):

| Hunk | Recorded | Corrected H2 | |
|---|---|---|---|
| `lifter.py` 5 (400–412) | UNDECIDED | **does not apply** — one side deleted a base line outright, the other kept it | ✓ the defect is fixed |
| `lifter.py` 6 (426–437) | **H2** | **applies** | ✓ unchanged |
| `lifter.py` 7 (1276–1297) | **H2** | **applies** (no base section; pure insertion) | ✓ unchanged |
| `kernel_bridge.c` 1, 2 | UNDECIDED | does not apply (same base line changed by both) | ✓ |
| `kernel_bridge.c` 3 | H1 | does not apply | ✓ (H1 decides it first) |
| `xbox_memory_layout.c` 1 | HA | applies | ✓ (pre-ruled; H2 never consulted) |
| `test_icall_feedback.py` 1, `translator.py` 1 | UNDECIDED | does not apply | ✓ |

**Both hunks that were correctly classified H2 remain H2, and hunk 5 stops being mechanically
mis-merged.**

#### Retraction: my first H2 proposal was wrong

My first version said *"no base line deleted by one side and kept by the other"* where "deleted" meant
**any change**. Edge-case testing (`logs/a4s/h1h2-edge-cases.py`) showed that fires on **hunk 6**,
which is a **correct** H2 case: upstream **replaced** `"pushfd", "popfd", "pushal",` with
`"pushfd", "pushal",` while local left that line alone. There is a replacement to take, so "keep both
sides' edits" is perfectly well defined there. The crude version would have routed a correct H2 hunk to
`UNDECIDED` — an over-strict rule that loses work in the other direction. **The corrected version above
distinguishes outright deletion from replacement and passes both recorded H2 hunks.** Recorded rather
than silently fixed, because it is the second time in this work that a proposed rule needed testing
against the corpus before it could be trusted.

**Edge cases now covered** (`logs/a4s/h2-corrected.py`, 14 synthetic shapes): outright deletion vs
replacement, both sides deleting the same line, both sides making the *same* edit, one side replacing
while the other appends, no-base-section pure insertion, and both sides replacing the same line
differently. Every case lands on `applies` or `does not apply` with the expected reason, and none
produces a silent side choice.

## Honest limits

1. **Tested on 9 hunks from one merge.** That is the entire real corpus available and it includes both
   the correct case and the defect case, but it is a small sample. A hunk where both sides delete
   different base lines and add different lines will simply be `does not apply` → H2/H3/UNDECIDED,
   which is the intended fail-closed direction.
2. **H2 is repaired above** (see "H2 has the SAME defect class"), and H3 is **not** examined here. H3
   applies only when the sole base-line changes on both sides are a version string or comment text; it
   does not touch deletions of code, and no hunk in this merge was classified H3, so there is no corpus
   to test it against. If the Planner wants H3 audited for the same class, that needs a different
   corpus and is a separate question.
3. **The rule is a proposal.** Whether it becomes `A4s-r6`'s operative H1/H2 — and its exact wording —
   is the Planner's design decision within the Advisor's rule ruling.
4. **Whitespace handling** uses trimmed comparison, matching the packet's "non-blank line" language.
   If the Planner prefers byte-exact comparison, the same structure works; it would classify hunk 3
   identically here because the differing lines differ in content, not spacing.
