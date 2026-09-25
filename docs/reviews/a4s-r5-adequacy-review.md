# A4s-r5 adequacy review — ADEQUATE

**Packet:** `docs/packets/a4s-toolkit-sync.md`, `A4s-r5`, SHA-256
`09DA9413C028D61BD28D9E4007AF6DDE3474ED6869F77B0095D03DB2C4FB86FB` (263 lines).
**Reviewer:** Planner child `f339b306-0473-4777-a211-78271453f2b9`, **fresh** (§5.1.5 — it did not
write this packet), route `workbuddy-ai/gpt-5.6-sol` @ `high` (temporary owner-authorized staffing,
`docs/reviews/startup-20260925-session-910703eb.md`).
**Verdict:** `ADEQUATE`, `BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED`, `DEFERRED: NONE`.

**Post-review byte check (Session, before promotion):** the packet's SHA-256 re-read after the
verdict is still `09DA9413…` and `git status --porcelain docs/packets/a4s-toolkit-sync.md` is empty,
so **no post-review edit occurred** and the review binds to the promoted bytes (§2.4.7).

## Reviewer's returned block (verbatim)

```text
REVISION:          A4s-r5 + SHA-256 09DA9413C028D61BD28D9E4007AF6DDE3474ED6869F77B0095D03DB2C4FB86FB (observed; 263 lines)
READ:              Observed directly: docs/packets/a4s-toolkit-sync.md 1-263; docs/reviews/a4s-r4-adequacy-review.md 1-97; docs/reviews/a4s-r3-adequacy-review.md 1-58; docs/reviews/a4s-ac97-hunk-ruling.md 1-203; docs/agent-workflow.md 1-777, especially §3.1/§5.3-§5.6/§6.1; plan-jsrf-bare-minimum.md 1-416, especially CURRENT PACKET 9-81 and A4s table 33-42; docs/jsrf-run-profiles.md 45-114, 270-374. Toolkit commits/trees read or measured: 0d7929c, 766ecef, 051a128, merge tree 75083476d3277f5c6d91eb7040f7a3ce5dc25335; xbox_memory_layout.c ranges 261-267, 664-719, 1716-1740 and merge-tree conflict 2103-2138; current refs/status/remotes/divergence. Run read/measured: logs/runs/20260924-191906-091-a4a-r2-default (STRICT; dump matches 1/content-mismatch 0; diagnostic_deadline; A3A gc=0x2 gs=0x100; F=2). Game HEAD b7d6af5 clean; changes since 3ec4563 affecting the packet's named build/run/generated inputs were checked and none observed.
PREMISE_FRESHNESS: BOUNDED
BLOCKING:          NONE
DEFERRED:          NONE
DECISIONS:         Observed — r4 B1 is closed and the HA-resolved comparison is unambiguous: AC-KEEP (iv) says both that the source is M and that the third comparison is on the "committed, HA-resolved M"; execution defines M only after conflict resolution and commit. Raw 75083476 fails the third comparison because markers occupy 2123-2138, while a read-only LOCAL-resolution simulation makes all three comparisons equal; reverse if operative wording permits the raw conflict-marked tree to be called M.
                   Observed — fixed lengths 7/56/25 are exactly the inclusive lengths and actual line counts of 0d7929c ranges 261-267/664-719/1716-1740. Exact first-line counts at 75083476 are 1/1/1 at 442/890/2103; after simulated HA LOCAL resolution all three fixed-length sequences equal local and each +1 shifted extraction differs. Thus the can-fail control is discriminating and its instruction is executable as written; reverse if a frozen input changes an anchor/count or a shifted extraction compares equal.
                   Inferred from the observed fixed inputs and complete contract — dropping end anchors and hunk-overlap creates no plausible false PASS: uniqueness fixes one start, fixed length covers every byte of each defined model range, any changed/shortened/shifted range differs or fails uniqueness, and AC-MERGE, the c97ce2c added-line/ancestry checks, semantic absence/call-site checks, plus the strict A3A witness cover preservation outside and behavior. A merge that genuinely alters any byte in a defined region cannot pass (iv); reverse with a concrete same-parent/H-rule result that changes or disables the model while all these checks pass.
                   Observed — r3 B1/B2 remain closed: exact quoted-name controls reproduce 2/2/0/0 at 766ecef/051a128/0d7929c/75083476; the only whole-tree quoted out-of-scope hit is templates/new-game/src/main.c:318; scoped T2 hits are mechanically dispositionable by the aperture/call-site tests; the boundary guard has zero hits and classifier lines remain 2/2. Reverse if final M produces an undispositioned trigger or a control mismatch, which the packet already routes fail-closed.
                   Inferred — premise freshness is BOUNDED, not PASS, because the merge/build/tests/new strict run are intentionally unexecuted; observed pins, clean toolkit main, remotes, merge base, 24/169 divergence, 153/38/10 file counts, archived strict reference validity, and docs-only later game HEAD support the premise, while P0/V and mandatory criteria fail closed if it has gone stale. Reverse to FAIL if P0 or the reference identity/profile is refuted in a way a row could misclassify; execution evidence satisfying the pins removes the bound.
VERDICT:           ADEQUATE
```

## The r4 blocking defect is CLOSED (reviewer-reproduced)

`docs/reviews/a4s-r4-adequacy-review.md` **B1-r4** was a guaranteed false FAIL: `AC-KEEP` (iv)
located each model range by **first *and* last** line, but the end anchors `0d7929c:719` (`        }`,
×37) and `:1740` (`            }`, ×20) are not unique, so a **correctly preserved** merge would be
ruled UNKNOWN → FAIL → rollback and `R-CONFLICT` blaming the wrong thing.

The reviewer reproduced the r5 replacement on the frozen inputs:

| Check | Reviewer's measurement |
|---|---|
| Fixed lengths 7/56/25 | **exact** inclusive lengths of `0d7929c` 261–267 / 664–719 / 1716–1740 |
| First-line anchor counts at `75083476` | **1/1/1**, at merge-tree lines **442 / 890 / 2103** |
| Byte compare after simulated `HA` LOCAL resolution | **all three equal** local |
| Can-fail (each `+1` shifted extraction) | **differs in every case** — the control is discriminating |
| Raw `75083476` third comparison | **fails** (conflict markers at 2123–2138) — hence the `HA`-resolved clause is load-bearing |

This directly answers the question r4's review left open — *"whether the packet's wording makes the
`HA`-resolved comparison unambiguous"* — in the **affirmative**: the operative text says the source
is `M` and that the third comparison is on the *committed, HA-resolved* `M`, and execution defines
`M` only after conflict resolution and commit.

## Why this is a §5.5 simplification, not a third patch

r3's blocker was in `AC-KEEP` **(v)** (name-grep scope); r4's was in `AC-KEEP` **(iv)** (line-anchor
uniqueness). The criterion is the same but the **mechanisms differ**, and r4's own reviewer
recommended treating (iv) as *"a redesign or simplification, not a third patch of the same shape."*
r5 **removes the failing shape entirely** — no end anchors, no hunk-overlap — rather than patching
it. That is the §5.5 **redesign branch**, so no Advisor consult was required, and the Advisor (live
on the `[GPIN]` redesign) was not burdened twice.

## Session verification of the load-bearing claims (independent, read-only)

Run before dispatch and again after the verdict; both agree with the reviewer:

| Claim | Check | Result |
|---|---|---|
| Merge tree identity | `git merge-tree --write-tree 0d7929c 766ecef` | **`75083476d3277f5c6d91eb7040f7a3ce5dc25335`** — matches r3/r4 and the reviewer |
| Anchors unique | exact whole-line counts of `0d7929c:261/664/1716` at `75083476` | **1/1/1** — **Confirmed** |
| Packet unedited | SHA-256 re-read + `git status --porcelain` | `09DA9413…`, empty — **Confirmed** |

## Deferred

**NONE.** r4's advisories D1–D4 were folded into r5 as wording and disposition changes (the
`main.c:318`-only expectation; the "admitted model text unchanged" disposition; the boundary-guard
limits; the `MIXDOWN_ALL` and macro/concatenation limits kept as claim limits). Per §5.4 a deferred
advisory never reopens a frozen packet, and none was raised here.

## Disposition

`VERDICT: ADEQUATE` **exactly** when `BLOCKING` is `NONE` and `PREMISE_FRESHNESS` is not `FAIL`
(§5.3). Both conditions hold: `BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED`. `BOUNDED` is
correct and expected — the merge, build, tests and the strict run are **intentionally unexecuted**;
that is what the packet is *for*. It is not `FAIL`, because the premise is not refuted and every
gate that depends on it (`P0`, `V`, the mandatory criteria) **fails closed** if it has gone stale:
a stale premise routes to `R-PRE`/`R-INVALID`/`R-UNKNOWN`, **never** to a false `R-SAME` or
`R-MOVED`.

**Per §5.3, `ADEQUATE` ends plan iteration.** The Session freezes this exact revision, records this
review, and **promotes it into `CURRENT PACKET` in the same step**, with **no discretion to revise
first**. Deferred advisories stay deferred. The revision is promoted as **`A4s-r5`** at SHA-256
`09DA9413C028D61BD28D9E4007AF6DDE3474ED6869F77B0095D03DB2C4FB86FB`.

**Evidence-freshness note for execution.** The review's `READ` line cites the game tree at
`b7d6af5` and the plan at 416 lines. Promotion **necessarily** edits the plan's `CURRENT PACKET`
block, which moves both. That is the promotion act itself, not a post-review edit to a reviewed
artifact: the **packet bytes are unchanged** (verified above), and the packet's own `Baseline`
clause already delegates the game revision to promotion time — *"game = `3ec4563` or the later
commit that adds this packet (Session records it at promotion; clean tree)"*. The game revision is
recorded at promotion below. The Planner's own reverse condition — *"reverse if a frozen input
changes an anchor/count or a shifted extraction compares equal"* — is unaffected: no toolkit input
changed, and the anchors remain 1/1/1.
