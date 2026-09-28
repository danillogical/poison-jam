# ⚠ `A2h` — the "pre-existing save-root flake" was **DISK EXHAUSTION**, not a race

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Corrects:** the diagnosis recorded in `a2h-live-slot-write-implementation-record.md` and repeated in the
implementation Worker's reports — **that `test-harness.py`'s save-root `winerror=5` failures were *"a
transient `CreateFileW` failure on a freshly-created temp file (racy against an AV/indexer)"*** and that the
Worker had *"PROVEN [it] pre-existing by stashing EVERY change and reproducing it on the clean baseline (1 of
4 runs)."*
**Why:** the owner's repository-durability session ran the mandated harness verification and it failed **twice
consecutively**, which is not what a 1-in-4 race looks like. Investigating found the real cause.

---

## THE MEASUREMENT

| Drive state | Harness result |
|---|---|
| **0.51 GB free of 931.8 GB** | **FAILED — twice in a row, on two DIFFERENT probes** (`test-healthy-0`, then `test-gpu-ptimer-runtime-0`) |
| **~62 GB free** | **FULL PASS — 19 probes + 9 A2h delivery fixtures + 1 native-`#DB` control** |

**The failure message in both cases:**

```
[SAVE] root rejected: requested directory is not writable (winerror=5)
```

> ## **`winerror=5` here is the surface of a FULL VOLUME, not a permissions or race problem.**

**And the two consecutive failures are the tell:** **a genuine 1-in-4 race fails twice consecutively with
probability ~1/16 and, more importantly, the Worker's own baseline reproduction (1 of 4) is ALSO consistent
with a disk that was already nearly full during that measurement.** **The Worker's stash-and-reproduce control
was methodologically sound in form but could not distinguish *"pre-existing code-independent flake"* from
*"pre-existing ENVIRONMENTAL condition"* — and it was the latter.** ✓

## ⚠ Why the Worker's conclusion was nonetheless reasonable, and where it fell short

**The Worker's evidence was:** *"~50 lines BEFORE any code I touched"*, and reproducible on a stashed clean
baseline. **Both are true and both are consistent with the disk explanation** — **the save-root preflight is
early in startup, and a full disk affects the clean baseline identically.**

**What the control could NOT rule out:** **an environmental precondition shared by every build.** **A
stash-based control varies the CODE; it holds the ENVIRONMENT fixed.** **So it can prove *"not caused by the
change"* but never *"not caused by the environment."***

> **The Session records that as the general lesson: a clean-baseline reproduction excludes the diff, not the
> machine.** ✓

## The corroborating measurement the Session made

**The Session measured REAL disk allocation (sparse-aware) rather than logical size, because this project's
logical numbers are wildly misleading:**

| Tree | logical | **REAL on disk** |
|---|---|---|
| **`logs/`** | **2 579.6 GB** | **167.3 GB** |
| **`logs/runs/`** | **2 579.5 GB** | **167.1 GB** |
| **`game/`** | **2.3 GB** | **2.3 GB** |

> **2 412 GB of the logical figure is SPARSE.** **The `Partition1.img` files are 2.36 GB logical with
> `Valid Data Length = 0` — they cost essentially nothing.** **So the consumer is the 167 GB of run EVIDENCE,
> not the partition images.**

**⚠ And the Session records that it first misread this:** **it saw `logs/` at *"2579.6 GB"* on a 932 GB drive
and briefly treated that as impossible.** **The `disk-usage.py` tool it wrote is what corrected it — logical
size is the wrong instrument for a sparse-heavy tree.** ✓

## The tools this produced, now checked in

| Tool | Purpose |
|---|---|
| **`scripts/disk-usage.py`** | **sparse-aware REAL allocation** (`GetCompressedFileSizeW`), with a failure path that counts logical and FLAGS it, so a failed query cannot silently understate |
| **`scripts/logs-reclaim-plan.py`** | **classifies run dirs into `save-root` vs `evidence` and `test-*` vs named**, so any reclamation can be scoped to regenerable scratch |
| **`scripts/secret-audit.py`** | **scans every reachable blob for secret shapes, with redacted samples** |

## What this changes operationally

1. **The harness is NOT flaky.** **A green harness requires adequate free space; below that it fails
   deterministically-ish at the save-root preflight.**
2. **A successor should check free space BEFORE treating a save-root failure as a race** — **and the Session
   recommends the preflight report a distinguishable code for a full volume rather than collapsing to
   `winerror=5`.** **⚠ That is a CODE CHANGE and the Session does NOT make it — it is outside this session's
   repository-durability scope and would need its own packet.** **Recorded as a recommendation.**
3. **⚠ The disk remains tight (~62 GB free).** **`logs/runs/` holds 167 GB of run evidence across 1 172 run
   directories, and nothing prunes it.** **The Session did NOT delete any of it** — **it is evidence, the owner
   reclaimed space separately, and pruning evidence is a decision the Session is not authorized to make
   unilaterally.**

## Prohibitions and status

**The Session deleted NO run evidence.** **No `git` history was rewritten.** **No frozen packet was edited.**
**The correction is recorded, not applied retroactively to the Worker's record** — **that record stands as
written, and this note supersedes its DIAGNOSIS while leaving its measurements intact.** ✓
