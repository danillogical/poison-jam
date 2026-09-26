# A4s-r5 rollback rebuild: exe SHA-256 differs from step 2 — cause measured

**Date:** 2026-09-25  **Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`
**Packet:** `A4s-r5` step 1 / `Closure`, under the selected row **`R-CONFLICT`**.
**Packet instruction being satisfied:** *"then `python -X utf8 scripts\build-jsrf.py` and record the
exe SHA-256 (it must equal the step-2 pre-merge SHA; if not, record — the restored tree is still
`0d7929c`)."*

## Measurement

| Build | Revision | exe SHA-256 |
|---|---|---|
| Step 2 pre-merge control | `0d7929c` | `9597FF7C2A377265ABA8DBB90B461EBE763E02D65432E9DFA13ACD925539C553` |
| Post-rollback rebuild | `0d7929c` | `AEC1F0FF7FB944DA44487EA15C3E94FF342751D4168E699F544A58895CF083C3` |
| Post-rollback rebuild, repeated | `0d7929c` | `AEC1F0FF7FB944DA44487EA15C3E94FF342751D4168E699F544A58895CF083C3` |

**CORRECTION (2026-09-25, after Advisor review).** An earlier version of this record called the
rollback rebuild "deterministic" on the strength of the two identical hashes above. **That inference was
wrong, and the Advisor identified the flaw:** if a fresh link always stamped a new timestamp, step 2's
exe could not have equalled a **days-old archived** hash. The repeated identical hash is a **no-relink
witness, not a reproducibility witness.** Both claims are now measured rather than inferred — see
"Cause" below. The two hashes and the packet's disposition are unchanged; only the explanation is.

## Cause: PE build-time stamps (13 bytes) — and *why* only one build relinked

Step 2's exe is byte-identical to the archived `A4a-r2` R0 exe. Comparing the rollback exe against
that archived exe (`logs/a4s/pe-diff.py`):

- both are **15,068,160 bytes**; 6 sections; debug directory at RVA `13959840`, size 84 — **identical layout**
- exactly **13 bytes differ**, in 5 runs:

| Offset | Current (`AEC1F0FF…`) | Archived (`9597FF7C…`) | Field |
|---|---|---|---|
| 280–282 | `a118b7` | `ced9b5` | PE `TimeDateStamp` (offset 280 = `e_lfanew+8`) |
| 13954724–13954726 | `a118b7` | `ced9b5` | debug-directory timestamp |
| 13954752–13954754 | `a118b7` | `ced9b5` | debug-directory timestamp |
| 13954780–13954782 | `a118b7` | `ced9b5` | debug-directory timestamp |
| 13955276 | `dc` | `db` | trailing byte of a 4-byte stamp (value 220 vs 219) |

Decoded (`logs/a4s/timestamps.py`): `0x6AB718A1` = **2026-09-26 00:58:09 UTC** (the rollback build,
local mtime 2026-09-25 17:58:10) vs `0x6AB5D9CE` = **2026-09-25 02:17:50 UTC** (the archived exe, local
mtime 2026-09-24 19:17:50). The archived run directory is `20260924-191906-091-…`. `CMakeLists.txt:9-10`
sets `/Zi` and `/DEBUG:FULL` and the project passes **no `/Brepro`**, so a link writes a fresh timestamp
into each image. **No code or data byte differs.**

### Why the rollback build relinked and step 2 did not — measured

| Measurement | Result |
|---|---|
| **No-op build relinks?** ran `build-jsrf.py` twice with no source change | **No.** exe mtime `17:58:10.059` and SHA `AEC1F0FF…` **unchanged**; the build log contains no compile of `recomp_0005.c`/`main.c` and no `Generating Code` |
| **What did the merge/abort rewrite?** | **58 toolkit files** carry mtime `2026-09-25 17:57:51`, including `src/apu/apu_dsp.c`, `src/kernel/kernel.h`, `CMakeLists.txt` and all four CRLF files |
| **Rollback build time** | exe mtime `17:58:10` — **19 s after** the rewrite, i.e. it saw changed source mtimes and relinked |
| **Content identity** | `git status --porcelain` empty; `git diff --stat HEAD` empty — the 58 rewritten files are content-identical to `0d7929c` |

**Conclusion (observed, not inferred):** the step-2 build found the tree already up to date from the
prior session and **did not relink**, so it kept the archived binary and its old timestamp — which is
exactly why `9597FF7C…` matched. The merge attempt and `git merge --abort` then rewrote 58 files
(changing mtimes, not content), which forced the rollback build to **relink** and stamp a new timestamp,
producing `AEC1F0FF…`. The Advisor's explanation is confirmed in both directions.

**Consequence for future work:** an "exe equals control" check is only meaningful if it states whether a
relink occurred. Where binary equality matters, compare with the PE timestamp fields masked, or build
with `/Brepro`. Recorded as a follow-up; nothing in this packet depends on it.

## Source identity is intact

- Both repositories are git-clean at the packet's revisions: game `c1cdb91`, toolkit `0d7929c`
  (`git status --porcelain` shows only the owner's `docs/agent-workflow.md` and this session's records).
- The game-side `build-source.json` source set has **144 entries, 0 added, 0 removed** versus the
  archived `A4a-r2` reference; the only content differences are line-ending artefacts of the
  merge/abort (below), which do not alter compiled output.

## Secondary finding: the merge/abort rewrote 4 toolkit files to CRLF

After the merge attempt and `git merge --abort`, four toolkit files that the merge had touched are
written in CRLF where the archived reference recorded LF:

`CMakeLists.txt`, `src/kernel/kernel_bridge.c`, `src/kernel/xbox_memory_layout.c`,
`src/kernel/xbox_memory_layout.h`.

**This is a git checkout artefact, not a content change.** Toolkit `core.autocrlf = true`, so git
re-materialises files with CRLF on checkout. Measured:

- For the three `.c`/`.h` files, `sha256(current bytes with CRLF→LF)` **equals** the archived raw hash
  **exactly**, and the worktree blob equals both the `HEAD` and `0d7929c` blobs.
- `git status --porcelain` reports **no change** for all four files.
- The archived `A4a-r2` `CMakeLists.txt` hash matches no revision in history in either LF or CRLF form
  (507 revisions scanned), so that one archived entry reflects a locally-modified build-time state;
  it is **not** evidence of a change introduced here. The current `CMakeLists.txt` is exactly the
  `0d7929c` blob in CRLF.

**Consequence:** none for this packet's criteria. `AC-GEN` is path-scoped to the **game** repository's
`src/recomp config tools/disasm/output`; the game repository had **0** raw source changes. Recorded
because it is a real change in working-tree bytes that a future session comparing raw hashes against
the `A4a-r2` archive would otherwise misread as a regression.

## Disposition

The packet anticipates exactly this case and instructs **record, do not gate**. Both hashes are
recorded; `R-CONFLICT` is selected by the five UNDECIDED hunks, not by this hash. No escalation is
required, and no evidence rule is bent: the rollback exe is `AEC1F0FF…`, and the restored tree is
`0d7929c` as the packet says.
