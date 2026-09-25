# A4s-r2 §5.4 re-review — ADEQUATE

**Packet:** `docs/packets/a4s-toolkit-sync.md`, `A4s-r2`, SHA-256
`DBF114BEC0FF6BB32B4E97CDB5156995013278EE4BE18AFFD67545021691238C`
**Reviewer:** Planner child `ae48fcf5-6f74-4ad2-b24c-85a0e4f91625`, route
`claude` / `claude-opus-5-5` @ `high` — fresh, wrote nothing (§5.1.5).
**Scope:** §5.4 re-review — the changed regions, the repaired blocking defect, and a
contract-completeness check.

## Verdict block (verbatim, abridged to the load-bearing parts)

```text
REVISION:          A4s-r2  SHA-256 DBF114BEC0FF6BB32B4E97CDB5156995013278EE4BE18AFFD67545021691238C (matches)
PREMISE_FRESHNESS: BOUNDED — every P0 fact re-observed today and holding (HEAD 0d7929c, upstream/main = origin/main =
                   v0.11.0 = 766ecef, @{u} = upstream/main, four remote lines exact, no a4s-* branch, both trees clean,
                   no game remote, 6748/6751 = the loc_001A18D0 label/jne). The reference stop is not re-observed, but
                   a stale stop now fails closed (R-UNKNOWN/R-INVALID), never a false R-SAME or R-MOVED.
BLOCKING:          NONE
DEFERRED:          (1) The rerun count is not capped when two rows apply: R-INVALID → rerun → R-UNKNOWN could be read as
                   allowing a second rerun. It still fails toward A4s-r3. Suggested wording: "at most one rerun total".
                   (2) The self-check's PREMISE_FRESHNESS says "F=0 routes to R-UNKNOWN, never R-MOVED". Imprecise: with
                   B = 0x803C0000 and W ≠ 3, F=0 legitimately gives R-MOVED(ii). Not operative text.
                   (3) An H2 union in kernel_bridge.c could produce the duplicate bridge_KeResetEvent the adequacy block
                   itself notes; the build would then fail and route to R-BUILD rather than R-CONFLICT. Still fails closed.
                   (4) The Session may prefer the Advisor's AC'97 ruling before executing, saving a run expected to end in
                   R-CONFLICT. Scheduling, not a packet defect.
DECISIONS:         False R-MOVED is closed. R-MOVED(ii) requires all of: outcome = diagnostic_deadline, B usable and
                   = 0x803C0000, W readable and ≠ 3, and F = 0. A shifted B cannot get through any branch because (ii)
                   tests B itself and does not rely on R-UNKNOWN being evaluated later. A live spin frame (F ≥ 1) with
                   W ≠ 3 cannot match (ii). R-MOVED(i) stays a positive observation. Would reverse if a V-holding strict
                   outcome other than diagnostic_deadline could come from the harness rather than the guest.
                   AC-RUN's W = MEM32(B+0x810), read only when B is usable, agrees with A4a line 33, R-SAME and
                   R-UNKNOWN. The false "unchanged" sentence is gone.
                   Rows are ordered and exhaustive: R-MOVED(ii) and R-SAME are disjoint on W; R-MOVED(i) and R-SAME on
                   outcome; R-UNKNOWN is a true catch-all. Every row names its next step.
                   Step 3 is sound: the merge runs with --no-commit, so the rollback is merge --abort and an UNDECIDED
                   hunk cannot be committed. One residual risk remains — an executor could mislabel a hunk H1–H3 when
                   it should be UNDECIDED, and AC-MERGE (d) only checks that each side's added lines appear in the
                   result. Build, tests, or the reviewer's check of the resolution table would catch it. That is an
                   execution-fidelity risk, not a contract defect.
VERDICT:           ADEQUATE
```

**On not pre-ruling the conflict hunks: the reviewer agreed.** At least six undecidable hunks
are expected; one is the AC'97 clash, which the Advisor owns. Pre-ruling the two
`kernel_bridge.c` hunks would not avoid `R-CONFLICT`, and ruling the rest would mean designing
the merge by judgment inside a change packet. Collecting the complete inventory from one
attempt and handing it to `A4s-r3` is the cheapest path that stays safe.

## Status

`A4s-r2` is **ADEQUATE**, but it is **not promoted**: the Advisor's AC'97 ruling
(`docs/reviews/a4s-ac97-hunk-ruling.md`) adds criterion obligations that `A4s-r2` does not
carry — the hunk disposition, three preservation checks including the `[A3A]` witness, the
zero-call-site grep for `ac97_arm_write_trap(`, and a rule-(d) inventory requirement covering
clean hunks. Those are criterion changes, so the packet goes back to a Planner for `A4s-r3`
before freezing (§5.3, §2.2.6).

**Deferred advisories** (recorded, not acted on): the four above. (1) is a cheap wording fix
for the revision.
