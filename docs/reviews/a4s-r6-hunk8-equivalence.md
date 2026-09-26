# A4s-r6: hunk 8 — the two forms are behaviourally equivalent (measured by execution)

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** measured evidence for the Advisor's hunk-8 ruling. Not a ruling.
**Script:** `logs/a4s/hunk8-equivalence.py` — it extracts `load_function_bodies` **verbatim from the
pinned revision `0d7929c`** and executes it, rather than paraphrasing it.

## The hunk

`tools/recomp/test_icall_feedback.py:116-124`, inside `test_seeds_drops_unaligned_targets`, within
`with tempfile.TemporaryDirectory() as tmp:`. Preceding lines (unconflicted, identical on both sides)
create a real empty file and state why an explicit database is supplied:

```python
fns = os.path.join(tmp, "functions.json")
with open(fns, "w") as f:
    json.dump([], f)
```

- **LOCAL:** `assert main(["--db", db, "--functions", fns,`
- **UPSTREAM:** `assert main(["--db", db, "--functions", os.path.join(tmp, "none.json"),`
- shared tail: `"seeds", "--out", out, "--align", "16"]) == 0`

## Measured equivalence

`args.functions` is consumed in `cmd_seeds` (the `seeds` subcommand, defined at
`icall_feedback.py:220`) at exactly one place:

```python
273:     bodies = load_function_bodies(args.functions)
274:     kept, dropped = {}, []
275:     for va, flags in sorted(db.items()):
276:         if bodies:
277:             inside = _interior_of(va, bodies)
```

Executing the pinned `load_function_bodies` on both inputs:

| Form | Return value | `bool(...)` |
|---|---|---|
| LOCAL — existing empty JSON file | `[]` | **False** |
| UPSTREAM — nonexistent path | `None` | **False** |

Because the consumer is `if bodies:`, **both forms take the same branch**: `_interior_of` is never
called, the alignment filter is exercised in both, and the test's asserted `kept` set is identical.
`cmd_seeds` only *reads* `args.functions`; its only write is `save_db(args.out, kept)` (line 290), so
neither form can affect any other state.

Corroborating facts, measured earlier: `icall_feedback.py` is **byte-identical** between `0d7929c` and
`766ecef` (empty `git diff`), and the *next* test in the same file — unconflicted — already uses
`none.json` (`75083476:131`), which is upstream's style.

## Consequence for the ruling

For **this test**, the two forms are interchangeable; neither changes an outcome. That makes hunk 8 a
candidate for a **mechanical rule** rather than a semantic one — but the choice still needs stating,
because the forms differ in a way a future reader could be misled by:

- upstream's form relies on the *fallback* (`path` does not exist → `None`);
- local's form relies on the *empty-database* path (`[]`), and its preceding lines deliberately
  create that file.

A merged tree that keeps local's `fns = …` / `json.dump([], f)` lines **and** upstream's
`none.json` argument would leave `fns` written but unused — harmless, but dead code that reads like an
oversight. Conversely, keeping local's argument with upstream's surrounding text would be consistent.
**The two lines of the hunk (the argument) and the unconflicted setup lines above it are coupled**, so
whichever side's argument is chosen should match the side whose setup intent is kept.
