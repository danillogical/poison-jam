# `PIO_FREE-model-r2` — Session verification and promotion

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/pio-free-model.md`.
**Class:** **discovery** (§5.8) — so the **writing Planner** reviews its own packet against §5.3's two
questions and **no second Planner is spawned** (`docs/agent-workflow.md:615-617`, `:743`).
**Authoring Planner:** child `d468ee62-a5c5-4fe0-8141-de543be151ce`, `codex/gpt-6-sol` @ `high`.
**Adequacy:** the writing Planner's own review — **`ADEQUATE`**, `BLOCKING: NONE`,
`PREMISE_FRESHNESS: BOUNDED`.
**No shape preflight** — the owner's instruction and §5.1 both trigger a Muse shape preflight only for a
**new change packet**, and this is a discovery.

| Item | Value |
|---|---|
| Revision | **`PIO_FREE-model-r2`** |
| SHA-256 (frozen) | **`11D6ECB195D51159785D6E94C98439BD4AA6979FDEE6B8EC79B28A80961DDA9E`** |
| Lines | **36** |

**Hash verified 3× over ~10 s immediately before promotion**, all identical.

## The class decision, and why it is right

The Planner chose **discovery** on the grounds that the trapped register's meaning, the truthful free-space
value, and the queue transitions are **not established**, so a safe **change** cannot be specified from
`A4p`'s gate-only finding. **The Session agrees, and independently confirmed the load-bearing premises**
(see `docs/reviews/pio-free-device-boundary.md`):

- `0xFE820010` resolves to **APU `+0x20010` = VP `+0x10` = `NV1BA0_PIO_FREE`** — so `A4p`'s "PIO" and the
  VP route are the **same address**, not two candidate devices;
- the current stub is `return 0x80; /* Always pretend queue is empty */` with **every other VP offset
  reading `0`** — a **pretence**, not a model, exactly as the packet says;
- VP writes dispatch **synchronously** through `fe_method` — **there is no queue**, so the model implicitly
  claims free space never decreases;
- the guest's two threshold forms are one constant (`val & ~3` vs `4`) and one **variable**
  (`val >> 2` vs a register), so `0x80` is **not neutral** — under the shift form it asserts **32 free
  units**;
- the untrapped route is a **separate** `MCPX_COUNTERS[0x020010]` tick counter, whose increment at
  `xbox_memory_layout.c:890` is gated on `!g_apu_mmio_trapped` — **which is precisely why** the trapped
  route falls through to the VP stub.

**So the device semantics genuinely are unresolved, and a model cannot yet be specified without guessing.**
Discovery is the correct class.

## Bounded revisions requested, and applied

The Session required **three** bounded revisions before promotion, none of which changed the class, scope,
question or outcome rows:

1. **Stale citations.** The packet cited `pio-free-strict-horizon.md` by line number **after the Session
   corrected that record**, so the citations had shifted. Fixed.
2. **The stale "untrapped alternative" framing.** The packet had said a default untrapped claim is *"not
   universally A2h-blocked"* and could be *"evaluated against its own archive."* **The Session's own
   measurement falsified the premise that made that sound benign** — the untrapped run's 31.71 s is a
   **hang**, not budget — so the packet now requires any untrapped claim to *"establish a valid modelled
   cause **and explain why the guest is not merely hung**."*
3. **A retracted witness.** An intermediate revision listed **"no GP bootstrap"** as evidence that the
   untrapped GP never booted. **That is an absent record, not a negative measurement**: the untrapped run
   has `RECOMP_APU_TRACE` absent, so it emits **zero `[GP*]` lines by design** — `AC-DEFAULT2` *requires*
   zero. The Session introduced that error, caught it, and required its removal. The packet now rests on
   **`F=2` + `Wf0=3`**, which are **tracing-independent**.

## The Session's own errors, recorded rather than hidden

**The Session made three errors in the horizon analysis and corrected all three**, and the record
(`docs/reviews/pio-free-strict-horizon.md`) carries a correction banner rather than a silent rewrite:

1. **The conclusion was backwards.** The first version read the untrapped run's 31.71 s as *"reaching the
   full requested deadline"* and concluded a default strict run had ~31.7 s of usable time. **Duration is
   not progress** — the untrapped guest is hung in the pending-word spin, and the **trapped** run is the one
   that makes real progress and then dies at 4.77 s.
2. **The evidence list was wrong.** Four "zero" witnesses were cited for the untrapped run, but all four are
   `[GP*]` lines that run **cannot emit by configuration**.
3. **A `Wf` cell was wrong.** The trapped run's `Wf` was listed as "cleared"; its dump is actually
   `CONTENT_MISMATCH` (`matches: 0, content-mismatch: 1`), so per the packet's own rule its `Wf` is **not
   reportable**.

**All three are the same class — reading an absent or displaced record as a measurement** — and all three
are recorded because the packet's Planner had already begun citing the wrong version. **The correction
mattered**: revision 2 above exists only because the error was caught.

## Promotion

`PIO_FREE-model-r2` at **`11D6ECB1…1DDA9E`** was **frozen and promoted into `CURRENT PACKET` in the same
step**, byte-identical with no revision (§5.3). The packet was **never edited by the Session**.

**What the Session now executes:** the packet's four Experiments, offline — pinned source reads,
XBE disassembly, and bounded external sourcing — producing
`docs/reviews/pio-free-model-execution-evidence.md` and a first-match row. **No guest run, no build, no
toolkit change, no instrumentation.** `A2h` is named as prerequisite **only** for a later claim needing
trapped strict observation beyond the captured pre-OOM prefix.
