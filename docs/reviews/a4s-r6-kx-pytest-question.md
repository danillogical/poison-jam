# A4s-r6 step 6: the KX pytest question (measured, awaiting an interpretation ruling)

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** **open interpretation question** sent to the persistent Advisor. `R-TEST` (row 5) gates the
strict run — *"the run is launched only if rows 2–6 do not match"* — so this must be resolved **before**
step 7. Recorded now so the measurement cannot be lost.

**Merge commit `M`** = `3f8bf67c450861aefcbc376698750bc1446bc9dd` (parents `0d7929c`, `766ecef`).

## What was measured

Every `AC-TEST` set on `M`:

| Set | Step-2 control | At `M` | Verdict |
|---|---|---|---|
| **C** (game ctest) | 12/12 | **12/12 passed**, incl. `jsrf_inplace_event_bridge` | **PASS** |
| **G** (game Python) | 11 files, 10 pass / 1 fail (`test_ac2_provenance.py`) | **11 files, 10 pass / 1 fail — same identity** | **PASS** |
| **K4** (toolkit baseline) | 27 tests, OK | **30 tests, OK** (grew; still all pass) | **PASS** |
| **KX** (toolkit, per module) | 33 modules, Ran sum 133 | **56 modules, Ran sum 183** | **see below** |

**Every step-2 module still passes, and none lost coverage:**

- all **33** step-2 modules are present at `M`;
- **0** modules that passed at step 2 regressed;
- **0** modules lost coverage (`Ran` count did not decrease).

**23 modules are new at `M`** (they arrive from upstream, which this merge brings in). Their breakdown:
**9 exit 0**, **7 exit 5** (`Ran 0 tests / NO TESTS RAN` — the known function-style case), **7 exit 1**.

**The 7 exit-1 modules all fail for one reason, verified individually:**

```
ModuleNotFoundError: No module named 'pytest'
```

| Module | Cause |
|---|---|
| `tools.recomp.test_block_dispatch` | `pytest` missing |
| `tools.recomp.test_incdec_carry` | `pytest` missing |
| `tools.recomp.test_incdec_result` | `pytest` missing |
| `tools.recomp.test_lifter_double_shift` | `pytest` missing |
| `tools.recomp.test_lifter_result_clobber` | `pytest` missing |
| `tools.recomp.test_sar_width` | `pytest` missing |
| `tools.recomp.test_x87_classification` | `pytest` missing |

**All 7 are byte-identical to the upstream parent `766ecef`** (verified blob-by-blob), so they are
**not** merge artifacts and no resolution touched them. `pytest` is **not installed** for either Python on
this machine (`pip list` shows no pytest), and the owner's standing directive is *"Do not install or
modify any system tooling for this investigation."*

**Upstream documents `pytest` — not `unittest` — as its test runner:**

- `README.md:409` — `pytest          # test suite only  (pip install pytest)`
- `README.md:417` — `py -3 -m pytest tools/       # unit tests`
- `CONTRIBUTING.md:136` — `py -3 -m pytest tools/       # unit tests`

So `AC-TEST`'s KX procedure (`python -m unittest tools.<dir>.<module>`) is **not** upstream's documented
runner, and these 7 modules were written for pytest. Under `unittest` they cannot even be collected.

## Why this is a real conflict between two `AC-TEST` clauses

The packet says both of these, and they point opposite ways here:

> **FAIL:** any test in (i) fails; a K4 test fails; a G test outside the carve-out set fails; a KX module
> that passed in step 2 fails; **a new KX module fails**; a baseline ctest name is missing → `R-TEST`.

> **UNKNOWN:** **a set cannot be run (environment, not code — e.g. `capstone` missing**, confined
> `tempfile` denial) → record; **one retry after fixing only the environment**; persists → `R-TEST`.

- **Literally, clause FAIL applies:** 7 new KX modules fail → `R-TEST`.
- **Literally, clause UNKNOWN applies too:** these modules *cannot be run* for an **environment** reason —
  a missing Python dependency — which is precisely the `capstone missing` example the clause names.

**Why `R-TEST` looks like a misattribution.** `R-TEST` *"means a resolution or upstream change breaks a
relied-on or upstream test"*. Nothing is broken: the modules are byte-identical to upstream, no resolution
touched them, they would fail identically on a pristine `766ecef` checkout, and every module that ran
before still passes. The failure is the **absence of a dependency**, not a defect in the merge. This is the
same §3.1 misattribution shape the Advisor already ruled against for the set-G carve-out (*"a literal
`R-TEST` would misattribute a pre-merge game-repository defect to the merge"*).

**Why the UNKNOWN clause's remedy is not available.** It says *"one retry after fixing only the
environment"* — but "fixing the environment" here means **installing `pytest`**, which the owner's
directive forbids. The Session will not install it unilaterally.

## The question put to the Advisor

1. Do these 7 new modules select **`R-TEST`** (literal FAIL clause), or are they **UNKNOWN → recorded as
   "cannot be run: environment (pytest absent)"** and **not** a `FAIL`, given they are byte-identical to
   upstream, no resolution touched them, and no step-2 module regressed?
2. Does the UNKNOWN clause's *"one retry after fixing only the environment"* authorize installing
   `pytest`, or does the owner's *"do not install or modify any system tooling"* directive override it?
   If installing is not authorized, what is the correct disposition?
3. Should these 7 be recorded as a **claim limit** (like the ≥16 function-style modules the `A4s-r5` KX
   addendum already records as not exercised under `unittest`), and carried as a follow-up lead rather
   than a row selection?
4. Does this select a row at all, or is it a **recorded environment limit** that leaves rows 2–6
   unmatched so the strict run proceeds?

## What was deliberately NOT done

- **`pytest` was not installed**, and no environment was modified.
- **No module was skipped, patched, or excluded** to make the set look green.
- **No `__pycache__`/import shim** was written to fake collection.
- **The strict run was not launched**, because row 5 (`R-TEST`) gates it.
- **`M` was not rolled back**, because the question is unresolved and rolling back is itself a row
  outcome (`R-TEST` → `0d7929c`).
