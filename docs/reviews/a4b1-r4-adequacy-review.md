# A4b1-r4 adequacy review — VERBATIM (fresh Planner, full formal §5.3)

**Reviewer:** child `5db5fd71-b1db-479d-9e5c-14043d54e211`, route `workbuddy-ai` / `kimi-k3`,
`reasoning_effort` omitted per the owner's instruction. A **fresh** Planner that did **not** author the
packet, as §5.1.5 requires.
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Revision reviewed:** `A4b1-r4`, SHA-256
`6DD62A57E87445F5C12085210146204AA4E77D26FD316071FD41CAEC76835C38`, **445 lines**.
**Scope:** the **full formal** §5.3 review the Advisor mandated (part 2) — *not* a delta re-review —
covering r3 B1/B2 closure, DS3, DS5, DS6, AC-FIX `(vi)/(vii)/(viii)/(ix′)/(x)/(xi)`, and contract
completeness.

---

```
REVISION:          A4b1-r4 — SHA-256 OBSERVED: 6DD62A57E87445F5C12085210146204AA4E77D26FD316071FD41CAEC76835C38 (matches expected exactly). Line count 445 total LF (writer's "403" was non-empty lines; project convention is total LF, consistent with A4b1-r3's cited 373 — no mismatch).

READ:
- docs/packets/a4b1-gp-core-port-r4.md — ALL 445 lines (the packet, reviewed directly, SHA verified myself via python hashlib).
- docs/agent-workflow.md — §5.3 in full (L533-561: verdict format + exact blocking rule), §5.1.5 (L501-513), §5.4 (L563-582), §3.1 (L258-275), §5.6 (L604-619), §6.1 (L651-721, incl. item 6(b) L699-711), §6.2 (L730-777), §2.3 (L172-226), §3.2, §2.4.
- docs/reviews/a4b1-r4-planning-rulings.md — ALL 202 lines (binding Advisor rulings: part 1 SHAPE PROCEED; part 2 [GPIN] lineage = finite-universe design + required-content list; part 3 identifier A4b1-r4; part 4 DS3/P-F non-injective ruling with four-case inverse, GPDMA_AMBIGUOUS, sge0/sge0_va, two-direction fixture; Session ordinal-173 verification L143-161).
- docs/reviews/a4b-gpin-accounting-ruling.md — ALL 101 lines (the [GPIN] redesign authority, (a) required content, (d) rule 6(b), retroactive DS5 note L53; Session constant verification L80-93).
- docs/reviews/a4b1-a4b2-r3-adequacy-review.md — ALL 77 lines (the INADEQUATE verdict: B1 = [GPIN] key space 1024-word MIXBUF universe vs 256-entry table → false R2-UNKNOWN; B2 = sge0 and counts-line seq undefined/unasserted → false R2-NOBOOT).
- docs/reviews/a4b-watch-ledger-ruling.md — ALL 106 lines (Device semantics 6/7 authority, finiteness premise L28, retroactive CPU-site-table note).
- docs/reviews/a4b1-r4-premise-recheck.md — ALL 168 lines (P-F section superseded by Advisor part 4 per correction banner L51-67; confirmed packet uses the ADVISOR's DS3, not the recheck's wrong XBOX_CONTIG_BASE|(P&0x0FFFFFFF) formula).
- docs/reviews/a4b1-r4-inverse-precision.md — ALL 94 lines (inverse documented in-tree at :843; 0x03FFFFFF sites real and in scope).
- docs/reviews/a4b1-r4-ds3-citations-verified.md — ALL 81 lines (dma_resolve L88, xbox_ContiguousAllocatedBytes L2694, :843 precedent, ordering nuance).
- docs/packets/a4b1-gp-core-port.md — ALL 373 lines (superseded A4b1-r3, for delta comparison).
- TOOLKIT (read-only verification, repo C:\Users\logic\Repos\xboxrecomp at M = 3f8bf67c450861aefcbc376698750bc1446bc9dd, tree CLEAN, origin=fork / upstream push DISABLED): nv2a_pb_exec.c L88 dma_resolve body L67-100 (CONFIRMED high-water test FIRST, input always a physical offset); xbox_memory_layout.c L2694 xbox_ContiguousAllocatedBytes (CONFIRMED = g_contig_next − XBOX_CONTIG_BASE); xbox_memory_layout.c L843-848 (CONFIRMED XBOX_CONTIG_BASE | (x & 0x0FFFFFFF) round trip); apu_shim.h L101/105/109/115/119/123 and apu_vp.c L846 (CONFIRMED 8 live 0x03FFFFFF occurrences). Game repo: only docs/agent-workflow.md modified — the §1 roster (Kimi K3 Planner) + shape-preflight/§5.1.5 policy edit, separately owned, NOT part of the packet; §6.1.6(b) already present in-tree.

PREMISE_FRESHNESS: PASS — The motivating A4a R0/R1 runs are baseline-independent (the loc_001A18D0 spin, GPSADDR writes, zero GP/EP reads); the toolkit premises were re-measured at M by the Session (premise-recheck); the load-bearing P-F form change (identity → non-injective forward map) is folded in as DS3's rewrite per the binding Advisor part 4; the ordinal-173 identification is upgraded to OBSERVED (Session XBE verification in the rulings record). I independently confirmed the toolkit is at M with a clean tree and that dma_resolve / xbox_ContiguousAllocatedBytes / :843 exist and behave as the ruling assumes. The packet fails closed if the premise is wrong (Stop-if bullets route premise changes to the Planner).

BLOCKING: NONE

DEFERRED:
1. AC-FIX (v) states CPU_ZERO_OVERFLOW ≥ 1 (uncapped counter) while r3 pinned =1 — the change is deliberate (finite-universe N_SITES vs r3's fixed 16), but a reviewer should note the assertion is intentionally a lower bound, not exact; non-blocking since the can-fail property (overflow detector reachable) is preserved via (x) and the per-site latch count assertion.
2. AC-FIX line-vs-snapshot rule retains the soft qualifier "frame, insns, dsp_addr are also compared where the snapshot records them" (carried from r3, flagged in r3 DEFERRED); (i) pins insns and dsp_addr explicitly, so the soft phrase protects nothing extra. Cosmetic.
3. DS6 PERIPH "first value, first seq" and FIFO "reads, words" array shapes differ slightly from the accounting ruling's literal "{reads, first value, first seq}" / "{reads, words}" — packet matches the ruling; noting only that the fixture (viii) asserts reads under correct offset/FIFO, not the first-value subfield. Observation-only subfields; no row reads them.

DECISIONS (each the packet makes):
1. Base [GPIN] on the finite-universe design (Advisor part 2), not the r3 table — OBSERVED at DS6 L125-135; re-binding r3's DS6 would resurrect a ruled-against mechanism. Reversed by the ruling's own reversal conditions (GP threads other than APU frame thread, non-bin-major mix buffer, owner writer-level provenance requirement).
2. Fold P-F in as a DS3 rewrite (non-injective inverse), not a wording fix — OBSERVED at DS3 L72-82 and AC-FIX (vii) two placements; the Advisor ruled it load-bearing. Reversed by JSRF shown never to reach ordinal 173 on the DSOUND path (then the M-form fixture becomes a guard, not a witness).
3. Carry identifier A4b1-r4 — OBSERVED at header L3 and governing-requirement L4. Reversed by owner explicitly choosing the literal label after being told of the r2 collision.
4. Record A4b2 P2/AC-INPUTS as a stale boundary note, do not revise A4b2 here — OBSERVED at Closure L384; successor A4b2-r4's job. Reversed by an Advisor ruling that A4b2 must be co-revised.
5. Retire r3's 256-entry table / GPIN_OVERFLOW latch / per-key lines / cut-off, appearing ONLY in retired framing — OBSERVED at DS6 L125 and AC-FIX (ix′) L328 (known-bad framing). Reversed by a ruling that reinstates a bounded-table mechanism.
6. CPU-site table sized ≥ enumerated jsrf_watch_store count, count stated, CPU_ZERO_OVERFLOW as bug detector — OBSERVED at DS5-adjacent DS6 L118-119. Reversed by an enumerated site count that grows with run length.
7. Name regeneration a non-goal so the pytest gated lead stays gated — OBSERVED at non-goals L58; no regeneration this revision. Reversed by a packet that scopes regeneration.

VERDICT:           ADEQUATE

SCOPE CONFIRMATION (all six mandated items checked): (1) r3 B1 and B2 genuinely closed — B1 by the finite-universe redesign (32/128/6/4 universes, uncapped, keyed by classification property), B2 by DS7 sge0/sge0_va definitions + (vii) assertions and DS6 counts-seq + (vi)/(ix′) assertions. (2) DS3 as rewritten for the non-injective forward map — four-case inverse in the ADVISOR's order (window-VA first, high-water, low-RAM identity, fail-closed), correct for BOTH address forms, citing dma_resolve and :843, GPDMA_AMBIGUOUS uncapped/no-row, claim limit present, & 0x03FFFFFF prohibition retained, shim/vp sites a lead not scope. (3) DS5 table sizing — ≥ enumerated site count, count stated, overflow a bug detector. (4) DS6 — full required-content list present; retired mechanism cannot reappear (framed as retired/must-not-reappear). (5) AC-FIX (vi)/(vii)/(viii)/(ix′)/(x)/(xi) — (viii) kept, (ix′)/(x)/(xi) replace (ix), (vi)/(ix′) assert counts-line seq, (vii) two placements with SGE entry and page at different VAs both asserting sge0_va = page VA (can-fail both directions). (6) contract-completeness — governing-requirement names both revision grounds (After-INADEQUATE + PREMISE_CHANGED to M=3f8bf67c), non-goals name regeneration, A4b2 boundary note present, decision rows complete. No criterion is itself an unbounded analysis; every AC-FIX case can fail.
```

---

## Session disposition

**`VERDICT: ADEQUATE`, `BLOCKING: NONE`, `PREMISE_FRESHNESS: PASS`.** Per `docs/agent-workflow.md` §5.3,
`ADEQUATE` requires `BLOCKING = NONE` **and** `PREMISE_FRESHNESS` not `FAIL` — both hold — so **plan
iteration ends**. The revision is **frozen at SHA-256 `6DD62A57…35C38`** and **promoted in the same
step**, with **no polishing pass**: editing the packet now would change its SHA and invalidate this
review.

**The reviewer's three `DEFERRED` advisories are recorded and deferred.** None changes the disposition,
and none reopens the frozen packet (§5.4). Advisory 1 (AC-FIX `(v)` asserting `CPU_ZERO_OVERFLOW ≥ 1`
rather than an exact count) is a **deliberate** consequence of the finite-universe sizing the Advisor
mandated, and the can-fail property is preserved by `(x)`; advisories 2 and 3 are cosmetic or
observation-only subfields that **no row reads**.

**Independent verification the reviewer added beyond the Session's own work:**

- it re-verified the packet SHA-256 itself before reading;
- it independently confirmed the toolkit is at `M` with a clean tree, and that `dma_resolve`,
  `xbox_ContiguousAllocatedBytes` and the `:843` round trip **exist and behave as the ruling assumes**;
- it confirmed the **8 live `0x03FFFFFF` sites** at their recorded locations;
- it confirmed the packet uses the **Advisor's** DS3, **not** the Session's superseded formula from the
  premise-recheck — the specific trap the correction banner exists to prevent;
- it checked **all six** mandated scope items individually and recorded each.

**Note on the line count:** the reviewer independently reached the same conclusion the Session did —
the writing Planner's "403" was the non-empty-line count, and the project convention is total lines
(consistent with `A4b1-r3`'s cited 373). No mismatch, and no false FAIL was raised.
