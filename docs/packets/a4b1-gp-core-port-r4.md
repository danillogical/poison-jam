# A4b1-r2 — DRAFT (planning sketch stage)

## Sketch (planning record, §5.1.4; not the contract)

1. **Bounded claim:** re-bind the *premises, baseline and anchors* of the GP/DSP56300-core port (pinned core + watched-word ledger + licence + fixtures + unchanged default strict path) to toolkit `M` (`3f8bf67c`), incorporating **only** the A4s premise changes that can move the implementation/evidence decision. It establishes nothing about the title's GP behaviour (that is `A4b2`).
2. **Packet class:** change (`PREMISE_CHANGED`, §5.4(2)); the port edits source by hand, **no regeneration** — the pytest gated lead stays gated and unexercised.
3. **Preserved rulings (not reopened by the baseline move):** watch-ledger (DS5/DS6/DS7), Q1, owner Q2, checkpoint-40, xemu pin, the `[GPIN]` losslessness rule (§6.1.6 / 6b).
4. **Material premise deltas (measured, `a4b1-r2-premise-recheck.md`):** P-F form strengthened (bridge now delegates; inverse = `XBOX_CONTIG_BASE | (P & 0x0FFFFFFF)`, documented at `xbox_memory_layout.c:843-845`); `apu_dsp.c`/`apu/CMakeLists.txt` re-baselined to upstream `M` (anchors re-derived from text, not line numbers); `MIXDOWN_ALL` default-ON recorded host-only (no new criterion); `RECOMP_USB_PORT` out of scope (inventory only).
5. **Material unknowns (deferred to the packet, never pre-executed):** JSRF GP/audio runtime behaviour; whether the port still builds/tests/runs green at `M`.
6. **Proposed procedure:** same 9-step port (vendor → layout → route GP MMIO → DMA/choke → frames/ack-removal → trace → licence → fixture → record), re-baselined; then build + ctest + one strict default run vs the `M` reference R0.
7. **CONFLICT (escalated):** the on-disk `A4b1-r3` (256-entry `[GPIN]` table) was ruled **INADEQUATE** (third §5.5); the Advisor's `a4b-gpin-accounting-ruling.md` mandates a provenance-class/finite-universe `[GPIN]` redesign targeting `A4b1-r4` — never written. Is `A4b1-r2`'s base the ruled-against r3 DS6 text, or must it incorporate the r4 redesign?
8. **Outcome rows:** R1-PORT-FAIL / R1-DEFAULT-REGRESS / R1-UNKNOWN / R1-PASS (unchanged).

## Checkpoint forecast (if I continue past preflight)
- Read `A4b-r3-adequacy-review.md` and the r3 B2 fix text to confirm the exact r3→r4 `[GPIN]` DS6 delta, **only if** the Advisor rules the redesign is in scope.
- Re-derive the two `apu_dsp.c` text anchors (`dsp_ack_frame`, the frame hook) at `M` for the baseline block.
- Everything else is mechanical re-binding already specified by the brief.

## Checkpoint trail (§5.1.4)
- **≤20 calls:** sketch 1 written (above); SHAPE PREFLIGHT sent to the Advisor via the Session, together with an ADVISOR-CLASS QUESTION — the `[GPIN]` lineage conflict (on-disk `A4b1-r3` 256-entry table vs the `A4b1-r4` provenance-class redesign that was ruled but never written). Recorded by the Session in `docs/reviews/a4b1-r2-gpin-lineage-conflict.md`.
- **Status at 20 calls:** PAUSED, blocked on the Advisor's shape verdict and conflict ruling. The Session instructed me not to expand the body before the ruling lands. All mandatory reading done; load-bearing facts verified read-only (`apu_dsp.c`/`apu_core.c` anchors at `M`, A4s APU diff, git lineage).

## Adequacy self-check (§5.3) — to be completed after the packet body is written
PENDING — awaiting shape preflight and the conflict ruling.
