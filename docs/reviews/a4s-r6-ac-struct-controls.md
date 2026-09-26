# A4s-r6: AC-STRUCT prototype and its verified control numbers

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** Session measurement supporting the Planner's `AC-STRUCT` criterion. Not a ruling.
**Scripts:** `logs/a4s/ac-struct-prototype.py` (raw tree), `logs/a4s/ac-struct-resolved.py` (resolved trees).
**Both scripts are in the gitignored `logs/` tree and are PROVENANCE ONLY** — the Advisor ruled
(`a4s-r6-advisor-hunk-ruling.md`, shape preflight POLICY_ISSUE (1)) that the packet's gating scanner must
live at a **tracked** path and be pinned by SHA-256.

## The three checks, implemented

Over `SCOPE` (toolkit `src/`, `include/`; `.c` and `.h`), with comments and string literals stripped
**while preserving line count** (so reported line numbers stay true), and each line assigned a
preprocessor branch path:

- **(a)** conflict markers — any line matching `^(<<<<<<<|\|\|\|\|\|\|\||=======|>>>>>>>)(\s|$)`;
- **(b)** duplicate `case` labels with the same constant value in one switch **on the same branch path**;
- **(c)** duplicate file-scope definitions of the same identifier **on the same branch path**.

A file with unbalanced braces, or a switch whose braces do not match, is returned as **UNKNOWN** rather
than silently skipped.

## Verified control numbers (measured, not predicted)

| Tree | files | switches | definitions | UNKNOWN | findings |
|---|---|---|---|---|---|
| `0d7929c` (local) — **negative control** | 87 | 109 | 1176 | 0 | **0** |
| `766ecef` (upstream) — **negative control** | 87 | 111 | 1302 | 0 | **0** |
| `75083476` **raw** (unresolved preview) | 90 | 111 | 1399 | 0 | **15** = 12 markers + 3 structural |
| `75083476` **resolved ALL-OURS** | 90 | 111 | 1399 | 0 | **3** structural, 0 markers |
| `75083476` **resolved ALL-THEIRS** | 90 | 111 | 1399 | 0 | **2** structural, 0 markers |

**The three structural findings on the resolved ALL-OURS tree:**

```
[dup-case] src/kernel/kernel_bridge.c:[8019, 8322]  case 138 branch=()
[dup-case] src/kernel/kernel_bridge.c:[8481, 8847]  case 138 branch=()
[dup-def]  src/kernel/kernel_bridge.c:[1377, 6713]  bridge_KeResetEvent branch=()
```

On the **raw** tree the same three appear at `[8040, 8343]`, `[8502, 8868]` and `[1378, 6734]`
(line numbers shift when markers are removed).

## Two precision points the criterion wording must carry

1. **Which tree is scanned.** The Advisor's criterion says the check runs on the **resolved** tree
   before the build; its positive control names `75083476`. Those reconcile only if the criterion
   states which number belongs to which tree. **Recommended wording:** the positive control runs on the
   **resolved** preview and expects **exactly the 3 structural findings with 0 markers**; if the raw
   tree is also cited, its 12 markers are a **separate, expected** observation, not a defect.
2. **ALL-THEIRS yields 2, not 3.** Resolving hunk 1 to upstream removes local's `bridge_KeResetEvent`
   (which lives *inside* that hunk), leaving only upstream's clean copy — so the duplicate definition
   disappears while both `case 138` duplicates remain. This is consistent with
   `a4s-r6-structural-findings.md` Finding 2 and is a good illustration that the count depends on the
   resolution. **The control should therefore name the resolution it uses** (ALL-OURS gives 3).

## Limits (stated, not glossed)

- The scanner is **textual**: it does not preprocess, so it cannot resolve `#include` graphs,
  macro-expanded code, or `#if` conditions that depend on defined values. It treats each `#if` as an
  opaque branch and compares only within the same branch path. A duplicate the preprocessor would
  produce from two *different* branch paths (e.g. two `#elif` arms that both evaluate true) is
  **missed**. That is why the criterion also needs the Advisor's `R-BUILD` backstop
  (C2084 / C2196 / C2371).
- Earlier iterations of these scripts produced **false positives** that had to be adjudicated by hand
  (member names read as typedef names; function parameters read as declarators; one-line definitions
  missed). That history is the evidence for the limit above — the scanner is a **lead generator**, and
  the positive/negative controls are what make it trustworthy for the specific defect classes it claims.
