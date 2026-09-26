# A4s-r6: structural defects in the merge tree that conflict-only rules cannot see

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** measured evidence for the Advisor's hunk rulings and the `A4s-r6` design. Not a ruling.
**Method:** read-only `git show` of the merge tree `75083476` and its parents, with the conflict
markers resolved to each side in turn, then brace-matched structural analysis. Scripts under
`logs/a4s/`: `case138-origin.py`, `case138-verify.py`, `structural-resolved.py`, `structural-pp.py`,
`dup-defs.py`, `switch-dup2.py`.

## Finding 1 — the merge produces DUPLICATE `case 138` labels in both dispatch switches

Both parents **added** `case 138` (KeResetEvent) to the same two switches, but at **different
positions**, and the base `051a128` had **no** `case 138` at all:

| Revision | `stdcall_args_for_ordinal` | `bridge_for_ordinal` |
|---|---|---|
| `051a128` (base) | **no `case 138`** | **no `case 138`** |
| `0d7929c` (local) | line 3548 — label **#67 of 194** | line 3811 — label **#65 of 174** |
| `766ecef` (upstream) | line 6744 — label **#306 of 348** | line 7172 — label **#290 of 347** |
| `75083476` (merge) | `case 138` at **8040 and 8343** | `case 138` at **8502 and 8868** |

Each parent has exactly **one** `case 138` per switch. The merge has **two** per switch, because the
two additions sit in different hunks and neither side's diff touches the other's location, so the
3-way merge keeps **both**.

**These lines carry no conflict markers.** They are clean-hunk content. Every site is unconditional —
verified by assigning each line a preprocessor branch path: all four `case 138` sites report
`branch=()` (no enclosing `#if`), so they are not mutually-exclusive platform alternatives.

**This is a hard C compile error** ("duplicate case value"), and it survives **both** all-local and
all-upstream resolution of the three conflicted hunks:

| Resolution | Duplicate labels found |
|---|---|
| ALL-OURS | `switch 7944-8383`: case 138 at 8019, 8322; `switch 8388-8919`: case 138 at 8481, 8847 |
| ALL-THEIRS | `switch 7925-8364`: case 138 at 8000, 8303; `switch 8369-8900`: case 138 at 8462, 8828 |

**Consequence for the packet design.** `A4s-r5`'s merge procedure is entirely conflict-driven: `HA`
pre-rules one conflicted hunk, `H1`–`H3` classify conflicted hunks, and anything else is `UNDECIDED`.
A defect that lives in **clean** hunks is therefore invisible to it. `A4s-r5`'s own
`AC-MERGE`(d) union witness checks that both sides' added lines are *present*; it cannot notice that
the same logical case label is now present **twice**. Left as is, this would surface only as a build
failure and be attributed to `R-BUILD` ("the merged runtime does not build with the unchanged game")
— a **misattribution**, which is the §3.1 blocking shape. It is also exactly the class of defect
`docs/jsrf-run-profiles.md` rule 4 warns about: *"Clean hunks are not exempt. A mechanical merge that
applies cleanly can arm behaviour as silently as a conflict can hide it."*

### The general form — measured, and bounded

The defect class is *"both sides independently added the same symbol/label in different places"*,
which a 3-way merge cannot detect because neither side's diff touches the other's location. Measured
across the two files the merge touched that carry these constructs:

| Population | Result |
|---|---|
| Top-level function definitions in `kernel_bridge.c` | base 200; local added 14; upstream added 184; **both added exactly 1** → `bridge_KeResetEvent` (2 definitions in the merge tree) |
| Labelled switches in `kernel_bridge.c` | 4 per revision. Switch #1 (24 labels) and #2 (7→16): **no** label added by both. Switch #3 (193→194 / →348) and #4 (173→174 / →347): **both added exactly `138`** |
| `kernel.h` | no function definitions in any revision |

So in this merge the class has exactly **two members** — one duplicate definition and one duplicated
case label in each of two switches — and both are now known. That does **not** mean the class is
exhaustively covered by these measurements: the scan is textual (see the method note below), and it
was applied to the merge tree for `kernel_bridge.c` and `kernel.h`, not to every construct in all 153
upstream-changed files.

## Finding 2 — resolving conflicted hunk 1 to LOCAL creates a duplicate function DEFINITION

`bridge_KeResetEvent` does **not exist** at the base `051a128`. Both sides added it, in different
places:

| Revision | definition |
|---|---|
| `051a128` | **absent** |
| `0d7929c` (local) | line 1359, **inside conflicted hunk 1** |
| `766ecef` (upstream) | line 6573, clean hunk |
| `75083476` (merge) | line 1378 (local's, in hunk 1) **and** line 6734 (upstream's, clean) |

Measured consequence of the hunk-1 resolution:

| Resolution of hunk 1 | `bridge_KeResetEvent` definitions | Result |
|---|---|---|
| **LOCAL** | 2 (local's + upstream's clean copy) | **duplicate definition → compile error** |
| **UPSTREAM** | 1 (upstream's clean copy) | compiles |

So hunk 1 is **not** a free textual choice: taking local's text brings a definition that collides with
a clean arrival from upstream. Whichever side wins, **exactly one** `bridge_KeResetEvent` definition
must remain, and the surviving one must be the side whose `KeResetEvent` semantics are chosen — the
local in-place form calls `xbox_KeResetInplaceEvent`, the upstream form calls
`ke_shadow_lookup`/`ResetEvent`. A merged file with both, or with local's `KeSetEvent` calling
`xbox_KeSetInplaceEvent` while `KeResetEvent` resets a *different* object model, would be internally
inconsistent even if it compiled.

## Finding 3 — the other structural candidates are false positives (recorded so they are not mistaken for defects)

A preprocessor-unaware scan flags duplicate definitions in `src/input/xinput_device.c`,
`src/audio/wma_decoder.c`, `src/kernel/kernel_file.c`, `src/kernel/kernel_hal.c`,
`src/kernel/kernel_path.c` and `src/nv2a/nv2a_mmio_hook.c`. **All are `#if defined(_WIN32)` / `#else`
platform alternatives** (confirmed by reading `xinput_device.c` around lines 17 and 132), and the
preprocessor-aware scan clears every one of them.

Two further candidates were checked and are **legal C, not defects**:

| Candidate | Verdict |
|---|---|
| `src/d3d/d3d8_device.c`: `static const IDirect3DDevice8Vtbl g_device_vtbl;` (line 120) and `… = {` (line 1406) | **Legal.** A file-scope `static const T x;` is a tentative definition (C11 6.9.2p2); the later initialised definition completes it. Present in **all four** revisions (base, local, upstream, merge) — pre-existing style, not merge-introduced. |
| `src/apu/apu_regs.h`: `#define NV1BA0_PIO_SET_SSL_SEGMENT_OFFSET` at 154 and 201; `…_LENGTH` at 155 and 202 | **Legal.** C11 6.10.3p2 permits redefinition when the replacement lists are **identical** — verified identical — and MSVC accepts it silently. Present in **all four** revisions. |

**Only `kernel_bridge.c` has same-branch structural duplicates introduced by this merge.**

### Additional structural classes checked (all clean)

Beyond duplicate functions and duplicate `case` labels, the merge tree was scanned for other
same-branch duplicate-definition classes. All came back clean, and two apparent hits were adjudicated
as regex false positives rather than defects:

| Class checked | Result |
|---|---|
| Duplicate file-scope variable definitions | one apparent hit, `src/d3d/d3d8_device.c` `g_device_vtbl` — **legal C** (tentative definition completed later; present in all four revisions) |
| Duplicate `#define` in headers | two apparent hits in `src/apu/apu_regs.h` — **legal C** (C11 6.10.3p2 permits redefinition with an identical replacement list; verified identical; present in all four revisions) |
| Duplicate `typedef` / `struct` / `union` / `enum` definitions | apparent hits in `src/platform/xbox_winnt.h` (`Ptr`) and `src/platform/mmio_decode.h` (`dev`, `off`) — **regex false positives**: the pattern captured a *member* name and *parameter* names rather than the typedef declarator. Both files define distinct types. No real hits. |
| Functions called but never defined | two apparent hits, `bridge_XeLoadSection` / `bridge_XeUnloadSection` — **regex false positives**: they are one-line definitions (`static void bridge_XeLoadSection(void) { bridge_XeSection(1); }`) that the "definition" pattern missed. Both are defined in all four revisions. |
| Do the two `switch (ordinal)` blocks carry consistent label sets? | **No, by design, and unchanged by the merge.** The args-count switch holds **24** labels and the dispatch switch holds **348** in the merge tree; the parents differ too (base 193, local 194, upstream 348), and their label sets already differ by ~23 / ~347 entries. A known ordinal may carry an arg count without an implementation, so a set difference is **not** a defect. The merge introduces no new difference between the two switches. |
| Duplicate tokens inside a frozenset literal (`_EFLAGS_PRESERVE`) | The **raw** merge tree appears to list `pushfd`, `pushal`, `sgdt`, `ljmp`, `sfence` twice — but that is an **artifact of reading the conflict-marked file**, where both sides' lines are present. On both resolutions the merge tree has **174** (all-ours) / **172** (all-theirs) tokens with only `movsd` doubled, and `movsd` is doubled in **every** parent too (base, local, upstream). `movsd` is legitimately **two distinct x86 instructions** — the SSE scalar move and the string move — as upstream's own comment says (*"movsd is TWO instructions"*). **Pre-existing and benign; not a merge defect.** |

**Method note (honest limits).** The scan is textual: it strips comments and string literals, matches
braces, and assigns each line a preprocessor branch path. It does **not** preprocess, so it cannot
resolve `#include` graphs, macro-expanded code, or `#if` conditions that depend on defined values — it
treats each `#if` as an opaque branch and only compares occurrences within the *same* branch path.
A genuine duplicate that the preprocessor would produce from two *different* branch paths (e.g. two
`#elif` arms that both evaluate true) would therefore be missed. The regex-based type/variable scans
are also heuristic and produced three false positives that had to be adjudicated by hand, which is
direct evidence of that limit. **The scan is therefore a lead generator, not a completeness witness.**
It does not affect Findings 1 and 2, whose sites are all `branch=()` — unconditional — and were
confirmed by reading the raw text.

## What this implies for `A4s-r6` (for the Planner; the Advisor rules the hunks)

1. A **structural** post-resolution criterion is needed that is not conflict-driven — at minimum
   duplicate `case` labels within one switch, and duplicate function definitions at file scope — and
   it must be evaluated on the **resolved** tree before the build, so a clean-hunk structural defect
   selects a conflict-class row rather than being misattributed to `R-BUILD`.
2. Hunk 1's ruling must state which `bridge_KeResetEvent` survives, not merely which side's
   `KeSetEvent` text is taken.
3. The two `case 138` duplicates need an explicit disposition: which of the two additions is kept in
   each switch (they are the *same* logical addition by both sides — a genuine duplicate, not two
   different behaviours), and whether the two switches must stay consistent with each other.
