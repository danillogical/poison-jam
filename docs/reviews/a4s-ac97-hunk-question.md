# A4s merge — AC'97 hunk is a policy question about accepted work

**Found by:** the A4s Planner (`edf308d2`) while previewing the merge with a read-only
`git merge-tree`. **Verified by the Session** against base, local and upstream.

## The clash

Both sides model the AC'97 codec-ready bit, but differently, and they touch the same region
of `src/kernel/xbox_memory_layout.c`:

| | Codec-ready mechanism |
|---|---|
| **Merge base** `051a128` | a plain `GS |= MCPX_AC97_CODEC_READY;` (set-only, unconditional) |
| **Local** `0d7929c` (`c97ce2c`, the accepted **`A3a-r25`** model) | **level-evaluated, atomic, bidirectional**: `InterlockedOr(...)` and `InterlockedAnd(...)`, `GS.bit8 := GC.bit1`, evaluated each tick outside the `g_apu_mmio_trapped` gate |
| **Upstream** `766ecef` | keeps the base's unconditional `|= MCPX_AC97_CODEC_READY` **and adds a VEH write trap** on the AC'97 NABM page: `AC97_NABM_OFFSET 0x400000` (`0xFEC00000`), `AC97_TRAP_BYTES 0x1000`, `ac97_clear_reset_bits()`, `ac97_write_veh`, `ac97_arm_write_trap()` |

So upstream did **not** remove the codec model — it kept the base's set-only form and added a
different mechanism (a write trap on the NABM page) alongside it.

## Why this is not the executor's call

`c97ce2c` is the **accepted** change of packet `A3a-r25` (record
`docs/reviews/a3a-r25-acceptance-review.md`, `ACCEPT`, no disagreements). Its model was
admitted as an **unconditional modeled hardware cause** under
`docs/jsrf-run-profiles.md` §"Unconditional modeled hardware causes", on the
secondary-source evidence in `docs/reviews/ac97-codec-ready-evidence.md`.

Resolving the hunk one way or the other therefore decides **whether accepted work survives**,
and whether upstream's trap machinery supersedes, complements, or contradicts the admitted
model. That is a technical-policy question for the **Advisor** — it bears on the admissibility
of strict evidence, which is exactly what `docs/jsrf-run-profiles.md` owns. The Planner
reached the same conclusion and routed it there.

## The other unresolvable hunks (Planner-observed, not re-verified by the Session)

`kernel_bridge.c` hunk 1 also carries local's `bridge_KeResetEvent`, which upstream defines
elsewhere (a duplicate definition if kept). `lifter.py` hunk 2 (`popfd`), `translator.py`
hunk 1 (`xadd` vs `inc`/`dec`) and `test_icall_feedback.py` hunk 1 each change one base line
differently on both sides. Together these mean the first merge attempt is expected to end at
`R-CONFLICT`, which is a legitimate and informative outcome: it hands `A4s-r3` the complete
inventory from a real attempt.

## Session decision on the Planner's efficiency question

The Planner asked whether to skip the execution and hand `A4s-r3` the preview inventory
instead. **The Session is executing the packet as written**, because:

1. it is the owner's instruction, and a real merge attempt produces the **authoritative**
   inventory — the Planner itself noted a preview may differ;
2. `R-CONFLICT` is a decidable, fail-closed row whose next step is `A4s-r3` with a complete
   per-hunk list, which is strictly more information than a preview;
3. substituting a preview for a measurement is the pattern this project has been burned by
   repeatedly (the `#200` truncation, the 10-of-28 spelling).

The AC'97 question is escalated to the Advisor **in parallel**, because any resolution of that
hunk needs it regardless of when it is asked.
