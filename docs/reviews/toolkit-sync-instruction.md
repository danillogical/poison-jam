# Toolkit sync — owner instruction and Session verification

## PREMISE_CHANGED (2026-09-24, owner) — the toolkit was forked and the remotes reconfigured

**The owner forked the toolkit and reconfigured the remotes.** Session-verified:

```text
origin    https://github.com/danillogical/xboxrecomp.git  (fetch + push)   <- the owner's fork
upstream  https://github.com/sp00nznet/xboxrecomp.git     (fetch)
upstream  DISABLED                                        (push)
```

| Ref | Commit | Note |
|---|---|---|
| `origin/main` | `766ecef` | the **fork**, currently equal to v0.11.0 |
| `upstream/main` | `766ecef` | the same commit — **no fetch is needed** |
| `main` | `0d7929c` | now **tracks `upstream/main`**, not `origin/main` |

**Recorded in** `AGENTS.md` "Toolkit remotes", added by game commit **`3ec4563`** ("AGENTS:
record the toolkit fork and push rule"), which is the current game HEAD.

**Push policy (owner, verbatim):** *"push toolkit main to origin when a packet closes; never
push to `upstream`. The game repository has no remote; do not add one."*

**Consequence for `A4s-r1`:** the draft merges from `origin/main` and gates on
`origin/main` staying `766ecef`. Both are now wrong — the merge source is `upstream/main`,
and closure is *supposed* to move `origin/main`. Returned to the authoring Planner to revise
before adequacy review. **Everything else in the draft stands** (owner's words).

---

# Toolkit sync — the original instruction and Session verification

**Instruction (owner, 2026-09-24, direct to the Session):** local `xboxrecomp` `main` has
diverged from `origin/main`. After `A4p` closes and **before any `A4b` code is written**, the
next packet is a change packet that merges `origin/main` (v0.11.0) into local `main`,
resolving conflicts; rebuilds **without regenerating `src/recomp/gen`**; runs both
repositories' tests; and reruns one strict baseline to show whether the stop is still the
DSP pending-word spin. The Planner designs it. If the strict stop moves, that is the next
brief. **Do not push anything to origin.**

> The instruction above predates the fork. Its "`origin/main`" means the commit that is now
> **`upstream/main`** (`766ecef`, v0.11.0), and its "do not push" is superseded by the push
> policy recorded above: push to the fork at closure, never to `upstream`.

## Session verification of the stated facts (all observed, 2026-09-24)

| Fact | Check | Result |
|---|---|---|
| Divergence | `git rev-list --left-right --count origin/main...HEAD` | **169 upstream vs 24 local** — matches the instruction exactly |
| Local branch | `git rev-parse --abbrev-ref HEAD` | `main`, HEAD `0d7929c86771dd0b971941592fd4f15436116e82`, tracking `origin/main` |
| Upstream tip | `git log --oneline -1 origin/main`, `git describe --tags origin/main` | `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b`, tagged **`v0.11.0`** — "Release v0.11.0 -- \"Nothing Said So\"" |
| Genuinely divergent | `git merge-base --is-ancestor HEAD origin/main` | **false** — the 24 local commits are unpushed work, not an ancestor of upstream |
| Merge base | `git merge-base origin/main HEAD` | `051a128df5ec27ef14f1ceaaead11c5457321eef` ("Handle the buffer flip, and list every unhandled method when asked") |
| Overlapping files | `Compare-Object` of the two `git diff --name-only <base>` lists | **exactly 10** — matches the instruction's count |

**The 10 files changed on both sides** (the conflict surface):

```text
CMakeLists.txt
src/kernel/kernel.h
src/kernel/kernel_bridge.c
src/kernel/xbox_memory_layout.c
src/kernel/xbox_memory_layout.h
templates/runtime/recomp_types.h
tools/disasm/functions.py
tools/recomp/lifter.py
tools/recomp/test_icall_feedback.py
tools/recomp/translator.py
```

`src/kernel/xbox_memory_layout.c`, `tools/recomp/lifter.py` and `tools/recomp/translator.py`
are three of the instruction's named examples, and all three are confirmed on the list.

## One fact the Session adds, which makes the ordering essential

**Upstream also changed `src/apu/`** — `src/apu/apu_dsp.c` and `src/apu/CMakeLists.txt`:

| File | Upstream change vs merge base |
|---|---|
| `src/apu/apu_dsp.c` | **+56 lines**; local is 148 lines, upstream is 199 |
| `src/apu/CMakeLists.txt` | +7/-1 |

These are **not** in the 10-file conflict list, so they merge cleanly — but they are
**exactly the files `A4b1` intends to modify** (the packet ports the DSP core into
`src/apu/dsp/`, replaces the state layout, and deletes the synthetic ack from
`apu_dsp.c`). So:

- The sync is a genuine **prerequisite** for `A4b1`, not merely good hygiene: `A4b1`'s
  baseline (`9597ff7c…`) and its `src/apu` starting state are both invalidated by the merge.
- `A4b1`/`A4b2` as drafted are **not executable** until the sync lands and their baselines
  are re-established. Their adequacy review (in flight when this was written) still bears on
  their design, which the sync does not change.
- The 24 local commits include the accepted `A4b`-predecessor work — `c97ce2c` (the AC'97
  codec model, `A3a-r25`'s accepted change) and `0d7929c` (the `A4a-r2` trace fix) — so the
  merge must preserve them. `c97ce2c` is a **dependency of an accepted packet**; losing or
  mangling it would reopen `A3a-r25`.

## What the packet must respect

- **Rebuild without regenerating `src/recomp/gen`.** The merge changes `lifter.py` and
  `translator.py`, so the generated tree becomes *stale relative to the new lifter* by
  construction. The owner has explicitly chosen not to regenerate, which means the packet
  must state that the generated chunks are **not** a product of the merged lifter and that
  this is a deliberate bound, not an oversight. `AGENTS.md` warns that a full pass must use
  the exact command and that `recomp_funcs.h`/`recomp_NNNN.c`/`recomp_dispatch.c`/
  `recomp_stubs_unresolved.c` must come from one pass — so if the merge ever requires
  regeneration, that is a **separate packet**, not a step here.
- **Both repositories' tests:** game `ctest` (12 tests at baseline) and the Python suites;
  toolkit `python -m unittest tools.recomp.test_lifter_atomics
  tools.recomp.test_lifter_string_compare tools.recomp.test_lifter_carry
  tools.recomp.test_seh_frame_owner` (with `PYTHONPATH` per `AGENTS.md`).
- **One strict baseline run** — `RECOMP_GPU_ACK=0`, `--profile strict`, and the *same*
  stop predicate the plan already uses: `diagnostic_deadline` with a live frame at
  `recomp_0005.c:6748-6751` (`sub_001A1769`), and `MEM32(0x803C0810) = 3`. The question is
  whether the stop is **still** the DSP pending-word spin.
- **Do not push.** No `git push` in any form.
- The merge is the owner's instruction, so it is authorized; but it is the kind of
  operation that can lose work, so the packet should make the pre-merge state recoverable
  (the 24 local commits are all reachable from the current `main`, and `git` will keep them
  as a merge parent — the packet should state the recovery point explicitly).

## Consequence for the plan

`A4b1`/`A4b2` remain drafted but are **superseded pending the sync**: their baselines and
their `src/apu` starting state will both change. Their adequacy verdicts stay useful as
design feedback. After the sync closes, the next action returns to `A4b1` with a fresh
baseline — or to whatever the sync's strict run makes the next brief, if the stop moved.
