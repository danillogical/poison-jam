# A4s-r6: P0 precondition run — P0.7 FAILS (execution halted before any write)

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** **execution halted at P0**, as the packet mandates (*"Preconditions P0 (all must hold, else stop
before any write)"*). **No toolkit write has occurred**; the toolkit is still clean at `0d7929c`.
An interpretation ruling is with the persistent Advisor.

**Packet:** `A4s-r6`, frozen SHA-256
`75207C41B8E9964D3E1467F5E980D1B8DE4CB75A1E0AFE954F59D40FA27D86E3`.

## Results

| # | Precondition | Result |
|---|---|---|
| P0.1 | toolkit `status --porcelain` empty | **PASS** |
| P0.2 | toolkit `HEAD` = `0d7929c86771dd0b971941592fd4f15436116e82` | **PASS** |
| P0.3 | branch `main`, `@{u}` = `upstream/main` | **PASS** |
| P0.4 | `upstream/main` = `origin/main` = `v0.11.0^{commit}` = `766ecef…` | **PASS** |
| P0.5 | `remote -v` prints exactly the four required lines, `upstream` push `DISABLED` | **PASS** |
| P0.6 | game `git remote` prints nothing | **PASS** |
| **P0.7** | **`git branch --list "a4s-*"` prints nothing** | **FAIL** — prints `a4s-pre-sync` |
| P0.8 | game tree clean except the owner's uncommitted `docs/agent-workflow.md` | **PASS** — only ` M docs/agent-workflow.md` plus Session-created records; its SHA-256 is still `CBEF90414E78D14BBF66FFA25D9BC7854D312C84AED9605246F305BE7AB8E6AB`, unchanged |
| P0.9 | `recomp_0005.c` lines 6748 / 6751 | **PASS** — `loc_001A18D0: ;` and `if (CMP_NE(_fa, _fb)) goto loc_001A18D0; /* jne: not equal / not zero */` |
| P0.10 | `Get-FileHash scripts\check-merge-structure.py` = the pin | **PASS** — equals `A1FDCE26…A1AFF` (but see the separate line-ending hazard record) |

## P0.7 — the finding, and why it is a real blocker rather than cosmetic

**Measured.** `git branch --list 'a4s-*'` prints `a4s-pre-sync`. Step 1 is:

```powershell
git -C $T branch a4s-pre-sync 0d7929c86771dd0b971941592fd4f15436116e82
```

Run as a dry test (it cannot mutate anything if it succeeds; it failed): **exit 128**,
`fatal: a branch named 'a4s-pre-sync' already exists`. Plain `git branch` refuses to overwrite, so
**step 1 cannot execute literally**, and every failure row's rollback procedure names `a4s-pre-sync` as
its target.

**Cause — a leftover from `A4s-r5`, not a divergence:**

- `git reflog show a4s-pre-sync` → `branch: Created from 0d7929c86771dd0b971941592fd4f15436116e82`
  (i.e. `A4s-r5` step 1);
- `a4s-pre-sync` = `0d7929c86771dd0b971941592fd4f15436116e82` = **exactly the `A4s-r6` rollback target**;
- the plan already anticipated this: *"no `a4s-*` branch existed before step 1 (`a4s-pre-sync` now exists
  at `0d7929c` and still resolves to it)"*;
- `A4s-r5`'s own execution evidence records its step 1 as `exit 0` and the branch intact at closure.

**Is the hazard P0.7 exists to prevent actually present?** Measured, and apparently **not**:

- P0.7's stated purpose in `A4s-r5` was *"so step 1 and the rollback cannot overwrite an existing
  branch"*;
- the branch step 1 wants to create **already exists at precisely the commit step 1 would give it**, so
  the recovery point is already correct and durable;
- the rollback's other branch, `a4s-merge-attempt`, is created with `git branch -f` (force, so it *can*
  clobber) — measured **ABSENT**, so that hazard is not present either;
- net: the state is *semantically* what P0.7 and step 1 are trying to establish; only the literal
  command's exit code differs.

## What was deliberately NOT done

- **No `git branch -f a4s-pre-sync`** and **no deletion/recreation.** `git branch -D` + re-create would
  also "work" but destroys and recreates the recovery ref for no reason; not done without a ruling.
- **No silent skip of step 1**, and **no packet edit** — `A4s-r6` is frozen; §5.4 permits revision only on
  a §3.1 blocking finding, a `PREMISE_CHANGED`, an Advisor policy ruling, or an owner change, and this is
  an interpretation question.
- **Nothing mutated.** After the dry test: toolkit clean, `HEAD` still `0d7929c`, `a4s-pre-sync` still
  `0d7929c`, branches `a4s-pre-sync` + `main` only, no `MERGE_HEAD`.

## The question put to the Advisor

1. May execution proceed, with step 1 discharged as **verify-and-reuse** of the existing `a4s-pre-sync`
   (assert `git rev-parse a4s-pre-sync` = `0d7929c`, fail closed otherwise) — recording that the branch
   **pre-existed and was reused**, not created?
2. Or does a failed P0.7 select `R-PRE` / `R-CONFLICT` and end this attempt?
3. Is a packet revision required (§5.4), or is this an interpretation ruling execution follows? The
   Session's reading is the latter: the criterion's intent is met and no criterion *text* is wrong — it is
   only over-strict for a re-run whose predecessor left the ref behind.

A second, independent interpretation question (the pinned-hash / line-ending hazard) is recorded in
`docs/reviews/a4s-r6-pinned-hash-eol-hazard.md` and is awaiting the same ruling.
