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

---

## Step 3 — merge, guard, resolution

**Merge command** (exactly as the packet specifies):
`git -C $T -c merge.conflictStyle=diff3 merge --no-ff --no-commit 766ecefcd7fb2a9b344de8ec891f6fe9ea14261b`

**Conflicted paths — exactly the 5 recorded:** `src/kernel/kernel_bridge.c`,
`src/kernel/xbox_memory_layout.c`, `tools/recomp/lifter.py`,
`tools/recomp/test_icall_feedback.py`, `tools/recomp/translator.py`. All within the 10-file set.

**The 9-hunk guard: PASS.** The live merge reproduced the recorded inventory **exactly** — all 5
files, all **9** hunks, every `ours`/`base`/`theirs` section and every recorded line span matching
(`1365–1404`, `1425–1445`, `8981–8988`, `2123–2145`, `400–412`, `426–437`, `1276–1297`, `116–124`,
`987–994`), all with diff3 markers. Comparison method and its justification:
`docs/reviews/a4s-r6-guard-decidability.md` (27/27 sections, 0 mismatches).

**Per-hunk resolution** (all 9 by **exact text**, never by line number; each pre-image asserted to
occur exactly once):

| # | File | Rule | Resolution applied |
|---|---|---|---|
| 1 | `kernel_bridge.c` 1365–1404 | **HA-COMBINED** | `bridge_KeSetEvent` + the single surviving `bridge_KeResetEvent` replaced with **HA-COMBINED-1** verbatim |
| 2 | `kernel_bridge.c` 1425–1445 | **HA-COMBINED** | `bridge_KeWaitForSingleObject` replaced with **HA-COMBINED-2** verbatim |
| 3 | `kernel_bridge.c` 8981–8988 | **H1 → LOCAL** | local's comment + TLS declaration kept |
| 4 | `xbox_memory_layout.c` 2123–2145 | **HA → LOCAL** | local's `VirtualProtect`-failure `else` kept; upstream's `\|= MCPX_AC97_CODEC_READY` and `ac97_arm_write_trap()` omitted |
| 5 | `lifter.py` 400–412 | **H2 (edit application)** | deletion applied, then upstream's insertions → matches **Expected-5** |
| 6 | `lifter.py` 426–437 | **H2 (edit application)** | both disjoint edits applied → matches **Expected-6** |
| 7 | `lifter.py` 1276–1297 | **H2 (pure insertion)** | local's block first, then upstream's |
| 8 | `test_icall_feedback.py` 116–124 | **HA-LOCAL** | local's `--functions fns` form |
| 9 | `translator.py` 987–994 | **HA-UNION** | full union → matches **Expected-9** |

**HA-COMBINED edits outside the conflict hunks** (located by exact text): **(a)** upstream's
clean-hunk duplicate `bridge_KeResetEvent` deleted; **(b)** exactly one `case 138` per dispatch
switch, keeping the **first** occurrence in each.

**Post-conditions:** `bridge_KeResetEvent` definitions = **1**; `case 138` sites = **2**, one in
switch 4 and one in switch 5. `git diff --check` clean; **0** conflict markers remain.

**`M` = `3f8bf67c450861aefcbc376698750bc1446bc9dd`**, parents `0d7929c…` and `766ecef…`.

**A Session error found by `AC-STRUCT`, corrected, and recorded** —
`docs/reviews/a4s-r6-ac-struct-catch.md`. My first edit (b) was a blanket text delete; measured from
the raw tree, **both copies within a switch are the same form**, so text cannot separate local's from
upstream's. That left switch 4 with a duplicate case (compile error) and **switch 5 with no
`case 138` at all** — silently dropping ordinal-138 dispatch, which the **build would never have
caught**. The merge was aborted, re-run deterministically, and the rule replaced with "keep the first
occurrence per switch", which reproduces the packet's named positions (`8040`, `8502`) exactly.

---

## Step 4 — `AC-STRUCT` (run **before** the build, as required)

| Check | Result |
|---|---|
| resolved `M` | **0 findings, 0 UNKNOWN** (exit 0 with `--expect 0`), 90 files / 111 switches |
| positive control, raw `75083476` | **15** = 12 marker lines + the 3 structural findings |
| negative control `0d7929c` | **0 findings, 0 UNKNOWN** (87 files / 109 switches) |
| negative control `766ecef` | **0 findings, 0 UNKNOWN** (87 files / 111 switches) |
| fixtures `tests/test_merge_structure.py` | **Ran 14 tests, OK** |
| scanner bytes vs pin | **match** (`A1FDCE26…A1AFF`), re-verified before use |

**PASS.**

---

## Step 5 — build (no regeneration)

**Succeeded**, 16 s, regeneration disabled. **A relink occurred** (unlike step 2): exe
`E45026C3DF5AACAF3D66FCC1E17D9C6C1A12247864D58D0911CDBA8435A2A3C7`, mtime `20:49:20.815`, versus
step 2's `AEC1F0FF…` at `17:58:10.059`.

**`AC-BUILD` backstop satisfied:** the build **succeeded**, so the `C2084`/`C2196`/`C2371` question does
not arise; in particular there was **no** duplicate-case or redefinition diagnostic — which is direct
evidence that the corrected edit (b) is right, since the earlier bad version would have produced one.

---

## Step 6 — `AC-TEST`

| Set | Step-2 control | At `M` | Verdict |
|---|---|---|---|
| **C** (ctest) | 12/12 | **12/12 passed**, incl. the named witness `jsrf_inplace_event_bridge` | **PASS** |
| **G** (game Python) | 11 files, 10 pass / 1 fail (`test_ac2_provenance.py`) | **11 files, 10 pass / 1 fail — same identity** | **PASS** |
| **K4** | 27 tests, OK | **30 tests, OK** | **PASS** |
| **KX** | 33 modules, Ran sum 133 | **56 modules, Ran sum 183**; all 33 step-2 modules present, **0 regressed, 0 lost coverage** | **open** |

**KX is the one open item.** 23 modules are **new** at `M` (arriving from upstream): 9 exit 0, 7 exit 5
(function-style, the known case), and **7 exit 1** — all seven failing solely with
`ModuleNotFoundError: No module named 'pytest'`. All seven are **byte-identical to the upstream
parent**, `pytest` is **not installed**, and upstream documents `pytest` (not `unittest`) as its runner.
`AC-TEST`'s FAIL clause ("a new KX module fails → `R-TEST`") and its UNKNOWN clause ("a set cannot be
run (environment, not code — e.g. `capstone` missing) → record") point opposite ways, and the UNKNOWN
clause's remedy ("retry after fixing only the environment") would mean **installing** `pytest`, which
the owner's directive forbids. **Referred to the Advisor**:
`docs/reviews/a4s-r6-kx-pytest-question.md`.

---

## `AC-MERGE` — PASS (16/16 targeted checks)

| Sub-check | Result |
|---|---|
| (a) parents `M^1`, `M^2` | **PASS** |
| (b) `theirs→M` ⊆ ours-changed (38) | **PASS** — excess empty |
| (c) `ours→M` ⊆ theirs-changed (153) | **PASS** — excess empty |
| (d) presence witness | **PASS** |
| (d-twin) deletion witness | **PASS** — 16/16 targeted checks |

The (d-twin) witness took **four attempts**, and **all three failures were mine** — the word
*"credited"* is load-bearing. Full analysis: `docs/reviews/a4s-r6-ac-merge.md`.

---

## `AC-KEEP` (iv) and (v) — PASS (evaluated before the build)

| Check | Result |
|---|---|
| (iv) range 261–267 (7 lines) | anchor count **1**, M start line 442, **byte-identical**, can-fail control differs |
| (iv) range 664–719 (56 lines) | anchor count **1**, M start line 890, **byte-identical**, can-fail control differs |
| (iv) range 1716–1740 (25 lines) | anchor count **1**, M start line 2103, **byte-identical**, can-fail control differs |
| (v) deleted names in `SCOPE` at `M` | **0 hits** (must be 0) |
| (v) controls | `766ecef` **2**, `051a128` **2**, `0d7929c` **0**, `75083476` **0** — all match the recorded values |
| (v) `\|= MCPX_AC97_CODEC_READY` | **0 code hits** |
| (v) `ac97_arm_write_trap(` | **1 hit, definition line only** — no call site (permitted) |
| `SCOPE` boundary guard | 55 added lines in `CMakeLists`/`cmake`, **0** naming `templates/` or `tools/` — **not tripped** |

**PASS.** (vi) is the `[A3A]` witness on the strict run, read after step 7.

---

## `AC-INV` — PASS (evaluated before the build)

| Check | Result |
|---|---|
| classifier-listed names | `RECOMP_GPU_ACK` 1 code hit (recorded), `RECOMP_APU_DSP_ACK` 1 code hit (recorded), others 0 |
| deleted/retired names | `RECOMP_AC97_READY` **0**, `RECOMP_VBLANK` **0** |
| classifier-listed grep controls | `766ecef` **2**, `051a128` **2**, `0d7929c` **0**, `75083476` **0** — all match |
| admitted-models names | all present in `SCOPE` (recorded per name) |
| **new-environment-names** | `M` has **44** vs `0d7929c`'s **42**; added = **exactly** `RECOMP_APU_MIXDOWN_ALL` and `RECOMP_USB_PORT`; **none removed**; all 42 local names still present |
| new-env controls | literal-pattern control **0** hits (as expected); pattern file 23 bytes |

**PASS.** Note the comparison the criterion requires is **`0d7929c` vs `M`**, not the two parents —
comparing the parents would report upstream's two names that local deliberately deleted plus local's
four additions, which is not the criterion.

---

## `AC-GEN` — PASS

`git diff --quiet HEAD -- src/recomp config tools/disasm/output` → **exit 0**;
`git status --porcelain -- src/recomp config` → **empty**;
`scripts/check-generation-provenance.py --check` → **ok: True**, exit 0.

---

## `AC-NOPUSH` — PASS

| Check | Measured |
|---|---|
| `origin/main` | `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` |
| `upstream/main` | `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` |
| `@{u}` | `upstream/main` |
| `git status -sb` first line | `## main...upstream/main [ahead 25]` — the **success path** (24 baseline + `M`), no `behind` |
| `remote -v` | the four P0 lines exactly |
| game `git remote` | prints nothing |
| Session attestation | no `git push` or `git fetch` during execution (the one push this session was the owner-authorized baseline push to `jsrf/integration`, **before** execution began) |

---

## Step 7 — the one strict run

**Run directory:** `logs/runs/20260925-210315-113-a4s-sync-strict`
**Command:** the packet's exact form — the six synthetic-completion variables cleared,
`RECOMP_GPU_ACK='0'`, `RECOMP_KERNEL_LOG_BUDGET='100000'`,
`python -X utf8 scripts\run-jsrf.py --seconds 30 --profile strict --label a4s-sync-strict`.
**One run. No rerun was needed or made.**

### `AC-RUN` reads

| Read | Value |
|---|---|
| **V** (validity) | **HOLDS** — `check-run-profile.py` prints **STRICT** (checked 1; strict 1, exploratory 0, fixture 0, unknown 0, missing 0); `result.json` readable; `check-dump-mapping.py` → **matches: 1, content-mismatch: 0** |
| **outcome** | `diagnostic_deadline` |
| **B** = `MEM32(0x001BA858)` | `0x803C0000` — **usable** (readable, ≠ 0) and **equal to the expected value** |
| **W** = `MEM32(B+0x810)` = `MEM32(0x803C0810)` | **`3`** |
| **F** = `stacks.txt` `sub_001A1769` frames on `recomp_0005.c:67(48\|49\|50\|51)` | **2** — **identical to the `A4a-r2` R0 reference control (2)**; both runs have exactly 2 `sub_001A1769` lines, at `recomp_0005.c:6749` |
| **A3A** (`AC-KEEP` (vi)) | **PASS** — `[A3A] ac97 witness: gc=0x00000002 gs=0x00000100`; GC bit1 set (`gc & 0x2`) and GS bit8 set (`gs & 0x100`), **matching the reference control exactly** |

**Artifact SHA-256:**

| Artifact | SHA-256 |
|---|---|
| `result.json` | `C662254D150E4778CE44CC7F83072413BD47D71583BFD75590C0F2C8AADE0152` |
| `stacks.txt` | `D7791C014EBDE39F5D7A5E762265BC929346BE6841592DEA3CD8E03BB17BB83B` |
| `jsrf_run.log` | `DFA320441DEDEB7E0DC7693DC1FC4B82821CE5EAFA03EEFE4E5969FE75747B5B` |
| `process.dmp` | `3898C72C107F1A86EC55E7E0E9C247815B34F4D399BC8D11BC0D20D69F3C9E1C` |

`outcome = diagnostic_deadline` means capture was bounded, **not** that the guest was live; `exit_code`
was 3 and `dump_ok` true.

---

## Row selection — **`R-SAME`**

Evaluated in order; the first match wins. All six gate rows are unmatched:

| Row | Condition | Result |
|---|---|---|
| `R-PUSH` | P0 passed **and** `AC-NOPUSH` FAIL/UNKNOWN | **no** — `AC-NOPUSH` PASS |
| `R-CONFLICT` | conflict outside the 10 / set differs / text differs / UNDECIDED / `AC-STRUCT`/`AC-MERGE`/`AC-KEEP`/`AC-INV` FAIL / build backstop | **no** — all PASS |
| `R-REGEN` | build failure meeting the regeneration test, or `AC-GEN` FAIL | **no** — build OK, `AC-GEN` PASS |
| `R-BUILD` | `AC-BUILD` FAIL | **no** — build succeeded |
| `R-TEST` | `AC-TEST` FAIL | **no** — C 12/12; G same identity; K4 OK; KX disposed by **Q-C** |
| `R-PRE` | P0 fails | **no** — P0 passed under **Q-A**/**Q-B** |

| Run row | Condition | Result |
|---|---|---|
| `R-INVALID` | V fails, or `result.json` unreadable | **no** — V holds |
| `R-MOVED` | (i) `outcome ≠ diagnostic_deadline`; or (ii) `outcome = diagnostic_deadline` **and** `B` usable **and** `B = 0x803C0000` **and** `W` readable **and** `W ≠ 3` **and** `F = 0` | **no** — (i) it *is* `diagnostic_deadline`; (ii) `W = 3` (not ≠ 3) and `F = 2` (not 0) |
| **`R-SAME`** | V holds; `outcome = diagnostic_deadline`; `B` usable and `B = 0x803C0000`; `W = 3`; `F ≥ 1` | **MATCH — all five hold** |

**Selected row: `R-SAME`.** The strict stop is **unchanged**: still the DSP pending-word spin at
`loc_001A18D0` (`recomp_0005.c`), pending word `MEM32(0x803C0810) = 3`.

| Field | Value |
|---|---|
| **Toolkit `main` left at** | **`M` = `3f8bf67c450861aefcbc376698750bc1446bc9dd`** |
| **Next packet** | Planner revises **`A4b1` → `A4b1-r2`** (`PREMISE_CHANGED`, §5.4(2)): new baseline = toolkit `M`, this exe SHA, this run as the reference R0, and upstream's `src/apu/apu_dsp.c`/`CMakeLists.txt` as the starting state; then `A4b2` |
| **Push** | **not an execution step.** `R-SAME` leaves `main` at `M`, so a Closure push to `origin` is permitted **only after ACCEPT** and after the five pre-push checks — and the acceptance reviewer **must see the Q-C claim limit explicitly** |

---

## `AC-GEN` and `AC-NOPUSH` re-checked at closure (step 8)

| Check | Result |
|---|---|
| `git diff --quiet HEAD -- src/recomp config tools/disasm/output` | **exit 0** |
| `git status --porcelain -- src/recomp config` | **empty** |
| `check-generation-provenance.py --check` | **ok: True**, exit 0 |
| `origin/main` / `upstream/main` | both `766ecef…` |
| `@{u}` | `upstream/main` |
| `git status -sb` | `## main...upstream/main [ahead 25]` — the **success path**, no `behind` |
| game `git remote` | prints nothing |
| toolkit final | `HEAD` = `M`, **clean**, `a4s-pre-sync` = `0d7929c` |

---

## `AC-TEST` — KX detail (per Q-C)

**C:** all 12 baseline names discovered and passing, including the named hunk-1/2 witness
`jsrf_inplace_event_bridge`. **G:** 11 files, 10 pass / 1 fail — `test_ac2_provenance.py`, the **same
failing identity** as step 2 (carve-out derived by identity, never carried forward). **K4:** 30 tests, OK.
**KX:** 56 modules; all **33** step-2 modules present with **0** regressions and **0** lost coverage.

**Per-module disposition at `M`:**

| Class | Count | Disposition |
|---|---|---|
| step-2 modules | 33 | **PASS** (all present; `Ran ≥` step-2 count) |
| new, exit 0 | 9 | **PASS** |
| new, exit 5 (`Ran 0`) | 7 | **not exercised (new, recorded)** |
| new, exit 1 (`pytest` absent) | 7 | **not exercised: new at M, dependency absent (pytest)** — per **Q-C** |

**The 7 pytest-dependent modules** (all four Q-C conditions verified — see
`docs/reviews/a4s-r6-execution-rulings.md`): `test_block_dispatch`, `test_incdec_carry`,
`test_incdec_result`, `test_lifter_double_shift`, `test_lifter_result_clobber`, `test_sar_width`,
`test_x87_classification`.

**CLAIM LIMIT (per Q-C, and the acceptance reviewer must see it):** *KX does not exercise 7
pytest-dependent upstream modules (listed above) under `unittest`. The lifter/translator conflict
resolutions (hunks 5–7 and 9) are therefore witnessed only by K4 and the existing KX modules, not by
upstream's own tests of those paths.* This is harmless **for this packet only** because `AC-GEN` holds —
there is no regeneration, so `M`'s lifter produced none of the linked code.

**GATED LEAD (binding on later planning):** before **any** packet regenerates or relifts with the toolkit
at `M` or later, all **56+** KX modules — including these 7 — must run under **real `pytest`** in an
**owner-authorized** environment; parametrized and fixture tests included.

---

## `AC-STRUCT` claim limit (per Q-C)

**`AC-STRUCT` detects duplicates, not omissions.** The switch-5 damage (ordinal 138 reduced to **zero**
cases) was **invisible** to the scanner; it was found only because the duplicate in switch 4 prompted a
fuller look. It is now excluded for this merge by the explicit **post-condition count** — exactly one
`case 138` site per switch — not by the scanner. **For any future revision, each HA edit should carry an
expected-count post-condition enforced as an `AC-MERGE` check.**

**Operative reading of HA edit (b)** (confirmed by the Advisor): *within each switch containing
`case 138`, keep the FIRST occurrence and delete the rest* — which reproduces the positions the ruling
named (`8040`, `8502`). The packet's original *"locate each by text"* was insufficient because both
copies within a switch are byte-identical.
