# A4s-r6: `AC-STRUCT` scanner — tracked tool, pinned hash, verified controls

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** Session-delivered tooling satisfying Advisor shape-preflight POLICY_ISSUE (1). **Not** a
ruling. The Planner owns the criterion wording; this record supplies the pinned artifact and its
measured controls.

## The tracked tools

| Artifact | Path | SHA-256 | Lines |
|---|---|---|---|
| Structural scanner | `scripts/check-merge-structure.py` | `A1FDCE26755DAB3A0B6AA6E23D380A8F95E097439C2BE91D6318BF4402AA1AFF` | 358 |
| Its fixtures | `tests/test_merge_structure.py` | `A42F4A1E2C473892DC28A0F5B21923F4BC0AEFCFFE859C7FC20006F49DB0744C` | — |

**Superseded hash (do not pin):** `E3A7E7EC…` was the scanner before the worktree-mode hardening below;
`8F7641DD…` was the version before the one-line-definition fix. Both are recorded only so a reader who
sees them elsewhere knows they are stale.

Both live at **tracked** paths. This satisfies the Advisor's requirement that gating tools *"must live
at a tracked path, such as the game repo's `scripts/`, pinned by SHA-256 in the packet. They must not be
run from `logs/a4s/`, which is gitignored"*. The earlier prototypes
(`logs/a4s/ac-struct-prototype.py`, `logs/a4s/ac-struct-resolved.py`) remain as **provenance only** and
are explicitly **not** packet-referenced.

## Verified controls — exactly the three the Advisor specified

| Control | Required | Measured |
|---|---|---|
| **Positive** — reports the known defects on the merge preview | `case 138` in both switches + `bridge_KeResetEvent` | **2 `dup-case` + 1 `dup-def`** (plus 12 marker lines, expected on an unresolved tree) |
| **Negative** — zero hits on `0d7929c` | 0 | **0 findings, 0 UNKNOWN** |
| **Negative** — zero hits on `766ecef` | 0 | **0 findings, 0 UNKNOWN** |
| **UNKNOWN** — a merge-changed file it cannot parse | reported, not skipped | **yes** — `scan_file` returns a reason for unbalanced braces or an unmatched switch, and `scan()` collects these into `unknown[]`; **any UNKNOWN makes the run fail closed** |

Reproduce:

```powershell
python -X utf8 scripts\check-merge-structure.py --rev 0d7929c --expect 0      # exit 0
python -X utf8 scripts\check-merge-structure.py --rev 766ecef --expect 0      # exit 0
python -X utf8 scripts\check-merge-structure.py --rev 75083476 --expect 15    # reports 15; exit 1
python -X utf8 scripts\check-merge-structure.py                              # scans the worktree
python -X utf8 tests\test_merge_structure.py                                 # 14 tests, OK
```

The scanner also reports its **file and switch counts** (`files=`, `switches=`), which the Advisor's
control list requires.

## Tree / resolution precision the criterion must carry

Measured counts, over `SCOPE` (`src/`, `include/`; `.c` and `.h`):

| Tree | files | switches | findings |
|---|---|---|---|
| `0d7929c` (local) | 87 | 109 | **0** |
| `766ecef` (upstream) | 87 | 111 | **0** |
| `75083476` raw (unresolved) | 90 | 111 | **15** = 12 markers + 3 structural |
| `75083476` resolved ALL-OURS | 90 | 111 | **3** structural, 0 markers |
| `75083476` resolved ALL-THEIRS | 90 | 111 | **2** structural (dup-def disappears — local's copy is inside hunk 1) |

**The criterion must say which tree each number belongs to.** The Advisor's rule is that the check runs
on the **resolved** tree before the build; its positive control names `75083476`. Those reconcile only
if the positive control is stated against the **resolved** preview (3 structural, 0 markers) and the raw
tree's 12 markers are a **separate, expected** observation. Stating it loosely would make a correct
implementation fail its own control, or pass while ignoring markers.

**The criterion must also name the resolution the control uses** — ALL-OURS gives 3, ALL-THEIRS gives 2.

## A real false negative the fixtures caught

Writing `tests/test_merge_structure.py` **found a defect in the scanner**: it missed **one-line function
definitions** (`static void a(void) { return; }`), which are real in this tree
(`static void bridge_XeLoadSection(void) { bridge_XeSection(1); }`). The first version therefore
under-counted definitions. Fixed by adding `DEFN_INLINE_RE`; the hash above is the **fixed** artifact,
and the test that caught it is retained. Recorded because it is direct evidence that the fixtures earn
their place — and because it is the third time in this work that a tool needed testing against real
input before it could be trusted.

## Worktree-mode hardening (second change, after the hash was first sent)

Worktree mode originally walked the directories, which would have scanned untracked scratch files and
could diverge from revision mode's file set. It now uses
`git ls-files --cached --others --exclude-standard`, so:

- untracked scratch files are **not** scanned;
- the file set is **consistent** between revision mode (`git ls-tree`) and worktree mode;
- during a conflicted merge the conflicted paths are still listed — which is what the criterion wants,
  because the **resolved** tree is what it checks.

All four controls were re-verified after this change and are unchanged (`0d7929c` 87/109/0, `766ecef`
87/111/0, `75083476` 90/111/15, worktree 87/109/0, fixtures 14/14 OK).

## Side effect: the set-G baseline moved (recorded, and it is why re-measurement is the rule)

Adding `tests/test_merge_structure.py` puts a **new file into set G**, so the baseline the brief
carried is now stale. Re-measured immediately:

| Set G | Files | Pass | Fail |
|---|---|---|---|
| before this tool (as briefed) | 10 | 9 | 1 (`test_ac2_provenance.py` = E2) |
| **after this tool** | **11** | **10** | **1** (`test_ac2_provenance.py` = E2) |

**The one failure is unchanged and is still E2** — the pre-existing, pristine-HEAD-reproduced
`test_ac2_provenance.py` failure (`docs/reviews/a4s-r5-premise-baseline-failures.md`). The new file
passes.

**Consequences the packet must carry:**

1. Any `AC-TEST` criterion enumerating set G must expect **11 files at this revision**, and must state
   its carve-out **by test identity** (`E2 = test_ac2_provenance.py`), not by count — a count-based
   criterion would fail on a correct tree.
2. The Advisor's rule stands: the exception set is **re-derived from each execution's own step-2 run
   and never carried forward**. The packet must say so rather than hard-code any number.

This is a live illustration of why the Advisor required re-measurement: the set-G baseline moved
**three times in one session** — 8/2 (as first measured), 9/1 (after the staffing-cleanup plan rewrite),
10/1 (after this tool). Any criterion that copied a number forward would have been wrong twice.

## Limits, stated rather than left implicit

- **Textual, no preprocessing.** It cannot resolve `#include` graphs, macro-expanded code, or `#if`
  conditions that depend on defined values. It treats each `#if` as an opaque branch and compares only
  within the same branch path, so a duplicate the preprocessor would produce from two *different* branch
  paths is **missed**. This is exactly why the Advisor's `R-BUILD` backstop (C2084 / C2196 / C2371) is
  required alongside it — the scanner cannot replace a compiler.
- **Legal constructs deliberately excluded**, each measured on the real merge: declaration + one
  definition; tentative definition completed later (C11 6.9.2p2 — the `g_device_vtbl` case); identical
  macro redefinition (C11 6.10.3p3 — the `apu_regs.h` case).
- **Fixture coverage** is 14 tests over synthetic shapes plus the real pinned-tree controls. It does not
  prove the scanner finds *every* structural defect class; it proves it finds the two measured classes
  and does not fire on the four measured legal constructs.
