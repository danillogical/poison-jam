# A4s-r3 adequacy review — INADEQUATE

**Packet:** `docs/packets/a4s-toolkit-sync.md`, `A4s-r3`, SHA-256
`62138278C4AFEB19AFB9A1C818178D9CEE68CFC8F36360AFC225D52CF82314AD`
**Reviewer:** Planner child `21e4be92-0573-4bf1-a461-f095e8575f15`, fresh (§5.1.5).
**Verdict:** `INADEQUATE`, `PREMISE_FRESHNESS: BOUNDED`. **The `HA` resolution itself is correct** — the
defect is that the new grep checks are too broad, so a faithful run **false-FAILs into `R-CONFLICT` even
when the AC'97 model is preserved exactly**.

```text
BLOCKING:
 (B1) AC-KEEP (v) and the AC-INV merged-tree grep cannot pass under the packet's own rules.
      Observed at the conflict-free merge result (read-only `git merge-tree --write-tree 0d7929c 766ecef`,
      tree 75083476 — the same tree r2 recorded): `git grep RECOMP_AC97_READY` has 4 hits, all outside the
      HA conflict hunk (markers at 2123–2138):
        (a) xbox_memory_layout.c:409 — upstream's comment above the ac97_arm_write_trap definition. It sits in
            a CLEAN hunk (@@ -254 +304) that HA lets in, and HA allows only one authored comment line, so the
            executor cannot remove it.
        (b) templates/new-game/src/main.c:166, 203, 318 (318 = `if (getenv("RECOMP_AC97_READY")) {`). That file
            is upstream-only, so AC-MERGE (b) requires it byte-exact upstream. The game build does not compile
            it (766ecef's root CMake adds only src/*).
      Result: AC-KEEP (v) FAILs (the getenv grep matches main.c:318) and AC-INV FAILs (all four lines) before
      the build, every time → rollback, R-CONFLICT, with the model preserved exactly: a false FAIL whose brief
      blames the wrong thing. The known-bad control cannot distinguish, because the expected-good M also matches.
      Required: set the grep scope and comment handling so HA's permitted output passes — restrict to built
      paths (e.g. src/) and code rather than comments, or authorize removing the upstream comment at 409 as a
      recorded HA omission. For templates/, whether the ruling's "anywhere" covers an unbuilt scaffold is an
      Advisor interpretation, since AC-MERGE (b) forbids editing it. Add a known-good control.
 (B2) The AC-INV inventory has no valid disposition for many hunks it will pick up. The triggers
      VirtualProtect / AddVectoredExceptionHandler / PAGE_READONLY match clean upstream hunks in kernel_bridge.c
      (NtProtectVirtualMemory, @@ -4618 +4867), xbox_memory_layout.c @@ -1198 (the PAGE_NOACCESS guard page),
      tools/recomp/data/win32_api_names.txt, tests/kernel_bridge/{CMakeLists.txt,README.md} and
      tests/memory_layout_posix/test_main.c. PASS requires each to be "local admitted form kept" or "dormant",
      and none is either → false FAIL, or judgment. Separately "any upstream-new function containing
      arm_/_trap/_veh … reachable from runtime init" is call-graph analysis, not a grep.
      Required: add an objective third disposition with a mechanical test, or narrow the triggers and paths;
      replace "reachable from runtime init" with a concrete grep.
DEFERRED: (1) the HA-amend re-check gap — advisory, not blocking (an amend only removes lines, so checks on the
      pre-amend M cannot yield a false PASS). Make it explicit in r4. (2) AC-KEEP (iv) hunk-overlap depends on
      git's hunk boundaries; also diff the extracted line ranges. (3) name the RR-wait lead in R-MOVED's guidance.
      (4) the self-check still carries r2 text ("hunks are NOT pre-ruled"), now partly stale.
DECISIONS: HA faithfully implements ruling (b); AC-KEEP (vi) correctly reuses the step-6 run (control confirmed
      at line 2943: gc=0x00000002 gs=0x00000100); the rerun cap is consistent; rows stay ordered and exhaustive
      with correct A4s-r4 targets. B1/B2 are blocking because a faithful execution gives a false FAIL.
```

**Advisor question raised:** does the ruling's "anywhere" (zero `getenv("RECOMP_AC97_READY")` in the merged
tree) reach an **unbuilt upstream scaffold** (`templates/new-game/src/main.c`), which `AC-MERGE` (b) also
requires to be byte-exact upstream? The Session escalated this rather than narrowing a ruling's scope itself.
