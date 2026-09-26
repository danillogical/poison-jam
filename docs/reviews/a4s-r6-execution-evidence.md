# A4s-r6 execution evidence

**Revision:** `A4s-r6`, frozen SHA-256
`75207C41B8E9964D3E1467F5E980D1B8DE4CB75A1E0AFE954F59D40FA27D86E3` (374 lines).
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Governing rulings:** `docs/reviews/a4s-r6-execution-rulings.md` (Q-A pinned bytes; Q-B `a4s-pre-sync`
verify-and-reuse) and `docs/reviews/a4s-r6-advisor-hunk-ruling.md` (the hunk resolutions).

---

## P0 preconditions — **all now PASS**

| # | Precondition | Result |
|---|---|---|
| P0.1 | toolkit `status --porcelain` empty | **PASS** |
| P0.2 | toolkit `HEAD` = `0d7929c86771dd0b971941592fd4f15436116e82` | **PASS** |
| P0.3 | branch `main`, `@{u}` = `upstream/main` | **PASS** |
| P0.4 | `upstream/main` = `origin/main` = `v0.11.0^{commit}` = `766ecef…` | **PASS** |
| P0.5 | `remote -v` exactly the four required lines; `upstream` push `DISABLED` | **PASS** |
| P0.6 | game `git remote` prints nothing | **PASS** |
| P0.7 | `git branch --list "a4s-*"` prints nothing | **literal FAIL → PASS by Advisor Q-B** (verify-and-reuse; see below) |
| P0.8 | game tree clean except the owner's uncommitted `docs/agent-workflow.md` | **PASS** — SHA-256 still `CBEF90414E78D14BBF66FFA25D9BC7854D312C84AED9605246F305BE7AB8E6AB`, unchanged |
| P0.9 | `recomp_0005.c` lines 6748 / 6751 | **PASS** — `loc_001A18D0: ;` / `if (CMP_NE(_fa, _fb)) goto loc_001A18D0; /* jne: not equal / not zero */` |
| P0.10 | `Get-FileHash -Algorithm SHA256 scripts\check-merge-structure.py` = the pin | **PASS by branch (a)** — see below |

### P0.10 — which branch passed (recorded per Advisor Q-A)

**Branch (a)** — the literal working-tree hash — is the branch that passed:

| Artifact | `Get-FileHash` (working tree) | Pin | Match |
|---|---|---|---|
| `scripts/check-merge-structure.py` | `A1FDCE26755DAB3A0B6AA6E23D380A8F95E097439C2BE91D6318BF4402AA1AFF` | same | **yes** |
| `tests/test_merge_structure.py` | `A42F4A1E2C473892DC28A0F5B21923F4BC0AEFCFFE859C7FC20006F49DB0744C` | same | **yes** |

**Cross-check by branch (b)(i)** (the Advisor's binary-safe committed-blob hash), which also equals both
pins — so the pins are confirmed as identifying the committed content, not merely today's checkout:

```
python -X utf8 -c "import subprocess,hashlib,sys;print(hashlib.sha256(subprocess.check_output(['git','cat-file','blob','HEAD:'+sys.argv[1]])).hexdigest().upper())" <path>
  scripts/check-merge-structure.py  -> A1FDCE26755DAB3A0B6AA6E23D380A8F95E097439C2BE91D6318BF4402AA1AFF
  tests/test_merge_structure.py     -> A42F4A1E2C473892DC28A0F5B21923F4BC0AEFCFFE859C7FC20006F49DB0744C
```

`git status --porcelain -- <both paths>` is empty (branch (b)(ii)). No `.gitattributes` was added; the
Advisor ruled that is owner/packet-scope work, not an interpretation.

### P0.7 — the literal FAIL and its resolution (recorded per Advisor Q-B)

The precondition failed literally: `git branch --list "a4s-*"` prints `a4s-pre-sync`, and step 1's
`git branch a4s-pre-sync 0d7929c…` exits **128** (`fatal: a branch named 'a4s-pre-sync' already
exists`). The Advisor ruled this is a **§5.4 interpretation** matter, not a revision: the criterion's
text is not wrong for its purpose, it is over-strict for a re-run.

All four of Q-B's fail-closed conditions were re-verified immediately before step 1 and **all hold**:

1. `git branch --list "a4s-*"` prints **exactly one** branch, `a4s-pre-sync`;
2. `a4s-merge-attempt` is **ABSENT**;
3. `git rev-parse a4s-pre-sync` = `0d7929c86771dd0b971941592fd4f15436116e82`;
4. `git reflog show a4s-pre-sync` has a **single** entry, `branch: Created from 0d7929c…` — no move.

---

## Step 1 — recovery point: **verify-and-reuse** (not created)

Recorded as: **"pre-existing from A4s-r5 step 1, verified and reused, not created."**

| Output | Value |
|---|---|
| `branch --list "a4s-*"` | `a4s-pre-sync` |
| `rev-parse a4s-pre-sync` | `0d7929c86771dd0b971941592fd4f15436116e82` |
| `reflog show a4s-pre-sync` | `0d7929c a4s-pre-sync@{0}: branch: Created from 0d7929c86771dd0b971941592fd4f15436116e82` |

Raw outputs: `logs/a4s/step1-branches.txt`, `step1-revparse.txt`, `step1-reflog.txt`.
Per Q-B: **no `branch -f`, no `-D`, no re-creation, and no other ref created.**
`AC` (packet line 113): `a4s-pre-sync` = `0d7929c` after step 1 — **PASS**.

---

## Step 2 — pre-merge controls on `0d7929c`

| Control | Expected | Measured | Verdict |
|---|---|---|---|
| `build-jsrf.py` | succeeds | **succeeded**, 8 s, regeneration disabled | **PASS** |
| exe SHA-256 | record, **not a gate** | `AEC1F0FF7FB944DA44487EA15C3E94FF342751D4168E699F544A58895CF083C3` (mtime `17:58:10.059`) | recorded |
| `ctest -N` | 12 | **12** | **PASS** |
| `ctest` | all pass | **12/12 passed** (incl. `jsrf_inplace_event_bridge`) | **PASS** |
| Set **K4** | all pass | **27 tests, OK** | **PASS** |
| Set **KX** | 33 modules, Ran sum 133 | **33 modules, Ran sum 133, 0 per-module differences** | **PASS** |
| Set **G** | re-measured, carve-out by identity | **11 files, 10 pass / 1 fail**; carve-out `test_ac2_provenance.py` | recorded |

**The exe hash is the known no-op-build behaviour, not a regression.** `AEC1F0FF…` with mtime
`17:58:10.059` is exactly the `A4s-r5` *rollback* build's artifact, which the plan already records. The
build found the tree up to date and **did not relink** (measured in
`docs/reviews/a4s-r5-rollback-exe-hash.md`: no `/Brepro`, so a relink stamps fresh PE timestamps; a no-op
build does not relink). It differs from the archived `A4a-r2` R0 exe `9597FF7C…` only in the **13 bytes**
already measured (four PE timestamp fields plus one stamp byte). The packet says **record, not gate**.

**Set-G carve-out, derived from THIS step-2 run by test identity** (never carried forward):
`test_ac2_provenance.py` (**E2**) is the only failure — the pre-existing, pristine-HEAD-reproduced defect.

**KX methodology, and a Session error worth recording.** KX is run per module, exactly as the packet
says. 16 of the 33 modules are **function-style** files that unittest reports as
`Ran 0 tests / NO TESTS RAN` (exit 5); per the `A4s-r5` KX addendum that is **"not exercised
(pre-existing, recorded)"**, explicitly *not* a FAIL and *not* a pass witness. My first KX pass reported
**Ran sum = 132** because my own regex looked for `"Ran N tests"` and unittest prints **`"Ran 1 test"`**
(singular) for a single test — so `test_translator_xmm_state` was scored 0 instead of 1. Re-measured with
the corrected regex: **Ran sum = 133, zero per-module differences from the recorded baseline.** The
discrepancy was mine, not the toolkit's.

**Raw logs:** `logs/a4s/step2-build.log`, `step2-ctest.log`, `step2-K4.log`,
`logs/a4s/kx-permodule-0d7929c-rerun.csv`.
