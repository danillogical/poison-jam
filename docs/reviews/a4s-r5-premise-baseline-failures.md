# A4s-r5 pre-merge baseline control: two pre-existing game-Python failures

**Date:** 2026-09-25  **Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`
**Packet:** `A4s-r5` (`docs/packets/a4s-toolkit-sync.md`), step 2 pre-merge controls, set **G**.
**Purpose:** determine whether two set-G failures are **pre-existing** or **introduced**, because the
Advisor's Q2 process exception is reversed by "any criterion, script or build step found to require a
whole-tree-clean game repository or to read `docs/agent-workflow.md`".

## Method (read-only; no system tooling installed or modified)

`git archive --output=<temp.tar> HEAD` followed by `tar -xf <temp.tar> -C <temp-dir>`, per the owner's
instruction to avoid piping binary archives through PowerShell. **Not** `git archive | tar`.

| Property | Pristine HEAD extraction | Working tree |
|---|---|---|
| Extraction | `git archive --output=` → `tar -xf` | live |
| `docs/agent-workflow.md` SHA-256 | **`F0A2BD5B81DC7E628AE2CE5D0DC55F3AF244FB22BAEFF0A8945201DEE4D97AEF`** (committed) | `CBEF90414E78D14BBF66FFA25D9BC7854D312C84AED9605246F305BE7AB8E6AB` (owner's edit) |
| Files extracted | 199 | — |
| Required inputs present | plan, workflow, `AGENTS.md`, both tests, the checker — **all present** | — |
| Archive / extract exit | 0 / 0 | — |

The extraction contains every file the checks need, and its workflow is the **committed** version, so
it is a valid pristine-HEAD control.

## Measured result

| Check | Pristine HEAD | Working tree | Classification |
|---|---|---|---|
| `scripts/check-agent-docs.py --check --json` | exit **1**, 4 findings | exit **1**, same 4 findings | **pre-existing** |
| `tests/test_agent_docs.py` | exit **1** — `FAIL: test_real_repository_is_clean` (Ran 30, failures=1) | exit **1** — same single failure | **pre-existing** |
| `tests/test_ac2_provenance.py` | exit **1** — `FAILED: A0 correct run PASSes, P1 differently-cased archive keys still PASS` | exit **1** — same two failures | **pre-existing** |

Both failures reproduce **identically on pristine HEAD**, and the failure sets are **byte-identical**
between the two trees. Applying the owner's classification rule: **reproduces on pristine HEAD →
pre-existing baseline failure.** Neither is introduced by the owner's `docs/agent-workflow.md` edit,
by this session, or by any merge (no merge has been performed yet).

## Root cause of the `test_agent_docs.py` failure — it is NOT repository dirtiness

The test name (`test_real_repository_is_clean`) is misleading; it does **not** check `git status`. It
runs the document checker and asserts zero findings (`tests/test_agent_docs.py:102-110`). The checker
fails on four `retired_names` findings, all in **`plan-jsrf-bare-minimum.md`** (lines 15, 47, 67, 71),
each naming `gpt-5.6-sol` without a historical marker:

```
plan-jsrf-bare-minimum.md:15 names retired route 'gpt-5.6-sol' without a historical marker
plan-jsrf-bare-minimum.md:47 names retired route 'gpt-5.6-sol' without a historical marker
plan-jsrf-bare-minimum.md:67 names retired route 'gpt-5.6-sol' without a historical marker
plan-jsrf-bare-minimum.md:71 names retired route 'gpt-5.6-sol' without a historical marker
```

`scripts/check-agent-docs.py:58-61` lists `gpt-5.6-sol` in `RETIRED_NAMES`; lines 44-49 define the
historical markers that would excuse a mention. The plan's four mentions are written as **current**
staffing statements (the previous session's temporary owner-authorized exception and its pause note),
not as history, so the checker flags them. **This is a documentation-consistency defect in the plan,
entirely independent of the dirty working tree.**

### Why this does not trip the Advisor's reversal condition

The reversal condition is about a step or script **requiring a whole-tree-clean game repository or
reading `docs/agent-workflow.md`**. Measured:

- The failure is identical with the tree dirty **and** pristine — so no step requires a clean tree.
- The findings name only `plan-jsrf-bare-minimum.md`. The checker reads `AGENTS.md`, the workflow and
  the plan (`scripts/check-agent-docs.py:28-30`), but the four findings are plan-only, and the
  **pristine** run used the committed workflow — so the owner's workflow edit contributes **zero**
  findings. The workflow file is read by the checker but is not the cause.
- `docs/` is not a build input; `AC-GEN` is path-scoped to `src/recomp config tools/disasm/output`.

**Conclusion: the reversal condition is not met, and the Advisor's Q2 exception stands.** These are
pre-existing baseline failures that the packet's own `AC-TEST` PASS rule must handle explicitly.

## Consequence for `AC-TEST` (a packet-rule question, referred to the Advisor)

`AC-TEST` PASS requires "(ii) G, K4 all pass" and FAIL is "any test in (i)/(ii) fails". Read literally,
the two pre-existing set-G failures select **`R-TEST`** — which would roll the toolkit back and blame
the merge for a defect that predates it and lives in a **different repository**.

`AC-TEST`'s stated controls are "the step-2 pre-merge run is the known-good control for every set",
and `AC-TEST` already contains a pre-existing-failure carve-out — but **only for set KX**: "(iii) every
KX module passes, **except a module that also failed in step 2 with the same failing test names
(pre-existing, recorded)**". No equivalent carve-out is written for sets G, C or K4.

The Session has **not** applied the KX carve-out to set G by analogy. Extending a carve-out to another
set is a criterion interpretation, which is the judgment layer's call (§2.2.6, §3.3). The measurement
is complete and unambiguous; the interpretation is referred to the Advisor.

**Not done, deliberately:** the plan, the workflow, the tests, and the merge state were **not** edited
to make the baseline green. No system tooling was installed or modified. The investigation was not
broadened beyond deciding pre-existing vs introduced.

## Other step-2 controls (recorded for completeness)

| Control | Expected | Measured | Result |
|---|---|---|---|
| Pre-merge exe SHA-256 | `9597ff7c…` (record) | `9597FF7C2A377265ABA8DBB90B461EBE763E02D65432E9DFA13ACD925539C553` | **matches** |
| `ctest -N` | 12 | **12** | **matches** |
| `ctest` run (set C) | all pass | **12/12 passed**, 100%, 22.01 s | **PASS** |
| Set G | all pass | **8 pass / 2 fail** (both pre-existing) | see above |
| Set K4 | all pass | **27 tests, OK** | **PASS** |
| `AC-REC` branch | resolves to `0d7929c` | `0d7929c86771dd0b971941592fd4f15436116e82` | **PASS** |
