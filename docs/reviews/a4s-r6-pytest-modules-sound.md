# A4s-r6: are the 7 pytest modules' tests sound? — measured YES

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** Session measurement supplementing `docs/reviews/a4s-r6-kx-pytest-question.md`, which is with
the Advisor. **Nothing was installed and nothing was written into either repository.**

## Why this was measured

The open question is whether 7 **new** KX modules failing with
`ModuleNotFoundError: No module named 'pytest'` select **`R-TEST`** (*"a resolution or upstream change
breaks a relied-on or upstream test"*) or are **UNKNOWN** (*"a set cannot be run (environment, not code —
e.g. `capstone` missing)"*).

A fact that bears directly on it: **is the test logic itself sound, or is something genuinely broken?**

The modules do `import pytest` **at module level**, so they cannot even be imported without it — which is
why the ordinary run reports only the import error and says nothing about the tests. To separate "the
dependency is absent" from "the code is broken", the Session supplied a **minimal in-memory `pytest`
module** covering only the surface these 7 modules actually use, then imported each module and called its
`test_*` functions directly.

**The shim exists only in that one process's `sys.modules` and died with it.** It is a diagnostic, not a
workaround: the packet's KX procedure still cannot run these modules, and **no criterion is satisfied by
it**. Recorded explicitly so nobody mistakes it for a fix.

## What pytest surface do the 7 modules use?

Discovered by scanning them, not assumed:

```
['mark', 'skip']
```

That is the whole surface — no `pytest.raises`, no `approx`, no `fixture` decorators.

## Result

| Module | Test functions | Direct-invocation result |
|---|---|---|
| `test_block_dispatch` | 11 | **7 PASS**, 3 need the `monkeypatch` fixture, 1 needs fixtures |
| `test_incdec_carry` | 1 | needs `parametrize` |
| `test_incdec_result` | 1 | **PASS** |
| `test_lifter_double_shift` | 3 | all 3 need `parametrize` |
| `test_lifter_result_clobber` | 4 | **3 PASS**, 1 needs `parametrize` |
| `test_sar_width` | 1 | **PASS** |
| `test_x87_classification` | 1 | **PASS** |

```
PASS                 : 14
FAIL                 : 0
SKIP                 : 0
needs pytest fixture :  8
```

**14 tests pass, 0 fail.** The 8 that could not be invoked directly need pytest's `monkeypatch` or
`parametrize` machinery — i.e. they are blocked by the **same absent dependency**, not by a defect.

## What this establishes

- **The test logic is sound.** Nothing in these modules is broken by the merge, and nothing would fail
  if pytest were present.
- **The obstacle is exactly one missing dependency.** The failure is environmental, not a defect in a
  relied-on or upstream test.
- **It supports the UNKNOWN reading** (*"a set cannot be run (environment, not code)"*) over `R-TEST`
  (*"a resolution or upstream change breaks a relied-on or upstream test"*), because **nothing is
  broken** — the modules are byte-identical to the upstream parent, no resolution touched them, every
  step-2 module still passes, and their own tests pass when invoked.

## What this does NOT establish

- **It does not satisfy `AC-TEST`.** The packet's KX procedure is `python -m unittest <module>`, which
  still cannot run these modules. This measurement is evidence about the *cause*, not a substitute for
  the set.
- **It does not cover the 8 fixture-dependent tests.** Those need real pytest machinery; the shim cannot
  honestly stand in for it, so they are reported as blocked rather than as passing.
- **It does not authorize installing pytest.** That remains the owner's decision.
- **It is not a packet criterion.** No criterion in `A4s-r6` references this probe.

## Reproduction

```powershell
cd C:\Users\logic\Repos\my_xbox_game
$env:PYTHONPATH='C:\Users\logic\AppData\Roaming\Python\Python313\site-packages'
& C:\Python313\python.exe -X utf8 logs\a4s\pytest-shim-probe.py
```

Script: `logs/a4s/pytest-shim-probe.py` (gitignored `logs/` — **provenance only**, not a gating tool, and
deliberately not referenced by any criterion). A companion `logs/a4s/pytest-direct-invoke.py` shows the
unshimmed result (all 7 fail to import), which is the control for the shim's necessity.
