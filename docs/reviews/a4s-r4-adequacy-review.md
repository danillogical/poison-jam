# A4s-r4 adequacy review — INADEQUATE (one new blocker; r3's B1/B2 CLOSED)

**Packet:** `docs/packets/a4s-toolkit-sync.md`, `A4s-r4`, SHA-256
`C194B92E7507295A7D3D8B35B96378407EB376CADDC9E894B23CEB5A9545603D`
**Reviewer:** Planner child `d8a797a0-b782-4507-9097-2beb8377b7d4`, fresh (§5.1.5).
**Verdict:** `INADEQUATE`, `PREMISE_FRESHNESS: BOUNDED`.

## r3's blockers are CLOSED (reviewer-reproduced)

- **B1 closed.** `git merge-tree --write-tree 0d7929c 766ecef` reproduces as
  `75083476d327…`. The **quoted** deleted-names grep over `src include` gives **0** at `75083476`
  and **0** at `0d7929c`, and the controls can still fail: **2** at `766ecef`
  (`kernel_bridge.c:1983`, `xbox_memory_layout.c:1960`) and **2** at `051a128` (`:1945`, `:1572`).
  Whole tree: only `templates/new-game/src/main.c:318`, recorded out of scope. The raw-text hits
  (`:166`, `:203`, `:409`) are comments.
- **B2 closed.** T2 hits at `75083476` are the `kernel_bridge` `NtProtectVirtualMemory` line and the
  `xbox_memory_layout` `PAGE_NOACCESS` guard → **host mechanism** (aperture false). The AC'97 lines
  (`:386`, `:397` in `ac97_write_veh`; `:420`, `:422` in `ac97_arm_write_trap`) are aperture-true with
  **zero call sites** → **dormant**. `win32_compat` is not even a T2 hit; `tools/` and `tests/` are
  outside the diff paths. **The aperture and call-site tests are plain greps — no call graph needed.**
- **Boundary guard** is fail-closed and decidable: added lines only, covers the game `CMakeLists.txt`,
  **zero hits** at `75083476`. Classifier-names control reproduced 2/2 with no difference. The new
  `getenv` set difference is exactly `RECOMP_APU_MIXDOWN_ALL` and `RECOMP_USB_PORT`.

## BLOCKING (B1-r4) — `AC-KEEP` (iv), the D2 extracted-range diff, false-FAILs on a preserved `M`

The rule locates each range at `M` by the text of its **first and last** line at `0d7929c`, each of
which must occur **exactly once**. **Measured at `75083476`:** the first lines (261, 664, 1716) each
occur exactly once, but the last lines are `0d7929c:719` = `        }` (occurs **37** times; 168
trimmed) and `0d7929c:1740` = `            }` (occurs **20** times; 168 trimmed).

**Scenario:** a faithful executor on a correctly preserved `M` (the model region is byte-identical
once `HA` takes LOCAL) finds the end anchors are not unique → the rule gives UNKNOWN → FAIL →
`AC-KEEP` fails **before the build** → rollback and `R-CONFLICT` with the brief *"the merge did not
preserve accepted work"*. A false FAIL and a misattribution, **every time**. The obvious relaxation
also fails: taking the nearest following occurrence extracts `M` 890–896 instead of 890–945, because
`        }` occurs *inside* the range.

**Required (measured, ready to apply):** anchor each range by its **unique first line** and extract
the **same line count** (7, 56, 25), then compare byte-for-byte. All three first lines occur exactly
once at `75083476`, found at `M` lines **442, 890 and 2103**; ranges 261–267 and 664–719 are
byte-identical there, and 1716–1740 differs only inside the `HA` conflict markers (which `HA` resolves
to LOCAL). Record **anchor-uniqueness counts as a control**.

## §5.5 disposition — treat (iv) as a simplification, not a third patch

r3's blocker was also in `AC-KEEP` (in (v)); r4's is in (iv). The **mechanisms differ** (name-grep
scope vs line-anchor uniqueness) but the **criterion is the same**. The reviewer's own recommendation:
*"r5 should treat (iv) as a redesign or simplification, not a third patch of the same shape."*
**Session decision:** `A4s-r5` **drops the hunk-overlap anchoring form entirely** and keeps only
unique-start-anchor + fixed line count — a simplification that removes the failing shape rather than
patching it. This satisfies §5.5 by the redesign branch, so no Advisor consult is needed and the
Advisor (already engaged on the `[GPIN]` question) is not burdened twice.

## DEFERRED (advisories)

1. The `(v)` "outside SCOPE" expected list names `main.c:166/203/318`, but the **quoted-literal** grep
   prints only `:318` — the other two are comments. Record-only, nothing fails; the wording should
   match the grep.
2. A T1 code hit **outside** the `HA` region that does not change an admitted model's text has no
   named disposition — it fails closed (UNKNOWN → FAIL) rather than false-PASS. No such hit at
   `75083476`.
3. Boundary-guard edge cases, both hypothetical: a `templates` reference without a trailing slash or
   via a variable would be missed; a **moved** existing line could trip it spuriously (UNKNOWN, so
   fail-closed). Zero hits at `75083476`.
4. The `MIXDOWN_ALL` guest-visibility uncertainty and the macro/concatenation limit stay as claim
   limits, consistent with making no APU claim.
