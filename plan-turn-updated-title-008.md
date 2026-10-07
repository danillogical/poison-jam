# plan-turn-updated-title-008

**Living execution plan.** Owned by the Orchestrator. Initialized from
`plan-turn-start-title-008.md` (Turn Planner, Claude Opus 5.5 @ high). Changes recorded with
`PLAN_CHANGE`.

## Objective (unchanged)

**Primary:** a non-perturbing, observational same-flip draw→present trace on the `NV097_FLIP_STALL`
path, keyed on `(flip_stalls, present serial)`.
**Secondary:** admit the 29-method class from a runtime witness.
**Then:** continue toward M15 — the title screen itself, by content, by eye, image preserved.

## Status: the primary measurement is DONE and it discriminates

### Instrumentation (landed, toolkit working tree)

`RECOMP_FLIP_TRACE=<n>` (off by default) in the toolkit:

- `src/video/fb_present.c` — `xbox_FramebufferPresentSerial()` and
  `xbox_FramebufferPresentHash()`; `fb_hash_rgb` refactored onto `fb_hash_words`.
- `src/kernel/nv2a_pb_exec.c` — `trace_note_surface` (records every
  `SET_SURFACE_COLOR_OFFSET` value), `trace_hash_surface` (FNV-1a-64 over the surface converted
  exactly as `fb_present.c` converts a published frame, so hashes are comparable with `[FBPRESENT]`
  and with the archived surface hashes), `trace_reason` (names which of the four
  `present_track_flip` branches won), `trace_flip` (emits at `NV097_FLIP_STALL`).
- Per-frame flags `drawn_this_frame` / `targeted_this_frame` are read **before**
  `present_track_flip` clears them; the published hash is read **after** the present call, so it
  names the bytes that call just handed the window.
- Gated by `RECOMP_FLIP_TRACE` (budget), `RECOMP_FLIP_TRACE_FROM` (first flip), and
  `RECOMP_FLIP_TRACE_CHANGE` (only when the decision key moves).

**Observational only.** It does not choose, redirect, force a resolve, write guest memory, or alter
the present call. `present_track_flip`'s arguments and result, `xbox_FramebufferWindowSet/Present`,
`s_gpu`/`s_present`, the walk, PFIFO and GET/PUT are untouched. `RECOMP_FB_VA` is not used.

### The deciding run

`logs/runs/20261007-110933-718-title008-trace-noadmit` — exploratory, no admit switch, 620 s,
`RECOMP_FLIP_TRACE=400 FROM=2200 CHANGE=1`. 242 flips traced (2200→2441), covering
disclaimer → `e886cadf72766a64` → black.

Verified with `logs/workers/title008/verify_sameflip.py` and `show_transition.py`:

| Check | Result |
|---|---|
| `published hash == selected-surface hash`, same flip | **242 / 242** |
| selection reason | `drawn_this_frame` **242 / 242** (never stale, never fallback, never targeted-only) |
| selected surface present in the enumerated candidate set | **242 / 242** |
| published matched **no** candidate | **0** |
| `flip_stalls == present serial` (the keying holds) | **242 / 242** |

The candidate set is every value the guest ever wrote to `SET_SURFACE_COLOR_OFFSET` — three
surfaces: `0x00084000`, `0x0011C000`, `0x001B2000` (and `flip_modulo=3`, i.e. a **three-entry flip
index ring**, not three proven scanout-buffer roles: `0x84000` is an offscreen render target). The
presenter can only ever publish `drawn_offset`, `targeted_offset` or
`color_offset`, and each of those is set by that method, so the set is complete for what the
presenter can publish.

### What this establishes (OBSERVED, same-event)

**The copy/publication mismatch is eliminated for the traced window; semantic source selection is
UNQUALIFIED.** `present_track_flip` selected a surface, and the
bytes published at that same flip were *that surface's bytes*, 242 times out of 242, by the
`drawn_this_frame` branch — so the presenter did not publish a stale or wrong buffer and did not
fall back. **That is copy/publication consistency, not proof of correct guest-intended selection:**
`reason` restates the same heuristic, and a concrete unresolved alternative exists (at flip 2425 an
earlier composite writes `0x11C000`, then a later `self=1` batch writes `0x84000`, and the tracker picks
`0x84000` because it prefers the last surface drawn). **No selection bug is claimed.**

**The black is guest-drawn content.** At the transition the guest *drew black into the surface it
then selected and published*, while the other two buffers still held the previous
disclaimer/fade content:

| flip | selected → published VA | published | cand 0x84000 | cand 0x1B2000 | cand 0x11C000 | bound texture |
|---|---|---|---|---|---|---|
| 2423 | `0x84000` | `8205f3a6d2e48df5` | `8205…` | `8205…` | `8205…` | `8205…` |
| 2424 | `0x84000` | `e886cadf72766a64` | `e886…` | `8205…` | `8205…` | `e886…` |
| 2425 | `0x84000` | `156ed4086987e325` | `156e…` | **`8205…`** | **`e886…`** | `156e…` |
| 2426 | `0x1B2000` | `156ed4086987e325` | `156e…` | `156e…` | **`e886…`** | `156e…` |
| 2427–2441 | alternating `0x1B2000`/`0x11C000`/`0x84000` | `156ed4086987e325` | `156e…` | `156e…` | `156e…` | `156e…` |

So at flip 2425 the selected surface (`0x84000`, `drawn_this_frame=1`) held **black**, while
`0x1B2000` still held the disclaimer and `0x11C000` held the fade — leftover content from *earlier*
frames, not new content. **The guest drew black into the buffer it presented.** That is downstream
of selection and downstream of the presenter.

**The disclaimer renders into all three buffers with the same content** (`8205f3a6d2e48df5` in all
three candidates for flips 2200–2423), and the fade step is visible as `e886cadf72766a64` appearing
in `0x84000` at 2424 and in `0x11C000` at 2425–2426.

### What this does NOT establish

- **Not** that the guest "never drew the title". The scene exists: this run's own dump holds a rich
  3D city scene in `0x80084000` (`1e971149626031f6`, 52.4 % non-black, mapping gate
  `matches 1 / content-mismatch 0`), and the same surface is bound as a texture
  (`[TEXUSE] 0x80084000 640x480 fmt 0x11 lin`, 52 report blocks, up to 2474 batches).
- **Not** which operation should have turned the drawn content into the xemu-visible title.
- **Not** whether the black is a fade the guest asked for, a composite that produced black, or a
  missing pass. The per-frame batch ring is needed for that, and it is not implemented yet.

## PLAN_CHANGE

- **Changed:** the primary objective is re-scoped from "build the same-flip trace" to "use the
  same-flip trace to identify the composite/draw operation that produces black". The trace is built
  and has answered its question; the remaining work is the per-frame batch ring that links the
  drawing of the swap surface to the texture it sampled.
- **Evidence:** the 242/242 same-flip equality and the `drawn_this_frame` reason at 242/242
  (above). That is copy/publication consistency; it does **not** establish correct guest-intended
  selection, so this PLAN_CHANGE is **superseded** by the third one below.
- **Why:** the user's decision tree assigns Case A to a source-selection bug. The measurement
  narrowed it to copy/publication consistency rather than eliminating the semantic question, so the
  batch ring (Turn Planner §1.1) was the next discriminator. **Superseded:** the ring did not settle it
  either, and the actual cause turned out to be the missing viewport constants (third PLAN_CHANGE).

## Second finding: the black does NOT depend on the `0x1964` stall

`logs/runs/20261007-112201-379-title008-admit-witness` — `RECOMP_NV2A_ADMIT_UNKNOWN=1`, 620 s.
It reached **presents 2850** with **zero** `[PFIFO] reject` lines and **zero** `admit-unknown`
lines, and its dump has all three surfaces black (`156ed4086987e325`), mapping gate
`matches 1 / content-mismatch 0`.

Two consequences:

1. **The 29-method witness is NOT EXERCISED** by this run (no `admit-unknown` line), exactly the
   outcome the Turn Planner warned about. The 29 cannot be admitted from it. (Worker B's code
   reading said the witness fires at unit commit, so an admit run *can* witness; this run simply
   never met those methods in a committed unit.)
2. **The black survives the walk continuing.** So the persistent black is not merely the
   `0x1964` reject freezing the last frame — the guest kept flipping black frames for 400 more
   presents. That strengthens the reading that the guest itself is producing black.

**Correction to a premise carried into this turn:** the earlier statement that the black interval
"reproduces **without** the admit switch, stopping at `unsupported_method 0x1964`" is true of the
no-admit run, but the black is *also* present in the admit run where nothing rejects. The reject is
therefore not the cause of the black; it only ends the run.

## PLAN_CHANGE (third): the root cause was found and fixed, and Case C is withdrawn

- **Changed:** the critical path. The black interval is no longer attributed to the composite, the
  presenter, or an unknown upstream draw. It was the **missing vertex-program viewport constants**:
  `0x0AF0`/`0x0A20` never reached `s_vp.c[58]`/`c[59]`, so every vertex-program vertex collapsed to the
  screen origin and every scene batch drew zero-area triangles. `0x0AF0` was **unhandled entirely**.
- **Evidence:** TR §23.8 — the toolkit's own `nv2a_regs.h` names `XFCTX_VPSCL = 0x3a = 58` /
  `XFCTX_VPOFF = 0x3b = 59`; the executor's `vp_method` never writes them; `0x0AF0` appears in the
  report's unhandled top ten at `x28024`. After the fix, against the pre-fix run, distinct
  `[FBPRESENT]` hashes go **10 → 658**, `0x0AF0` disappears from the unhandled list, and the transition
  window (flips 2426–2440) goes from one repeated black hash to **16 distinct hashes** — with flips 2425
  and 2441 **still black**, so the fix is not total.
  `0x80084000` (93.5 % non-black, 2677 colours) renders a full 3D city scene and `0x8011C000` a
  "Now Loading" screen.
- **Why:** this is a measured cause with a before/after on a controlled configuration, so it supersedes
  the earlier speculation. The sampling/UV/blend branches are no longer the critical path — though they
  remain **not positively established**, only made moot for this symptom.

## Case classification (final, corrected)

- **Case A (source selection):** the **copy/publication mismatch is eliminated** in the observed
  window (242/242 published == selected, `drawn_this_frame`). **Semantic source selection is
  UNQUALIFIED** — `reason` restates the same heuristic, and the flip-2425 ordering (earlier composite
  writes `0x11C000`, later `self=1` batch writes `0x84000`, tracker picks `0x84000`) is a concrete
  unresolved alternative. **No selection bug is claimed.**
- **Case B (composite produced black):** **withdrawn.** The ring's flip-time hashes cannot distinguish
  a black source from a source cleared after the read, so this was never established; and the actual
  cause was upstream in the vertex path.
- **Case C (upstream draw):** **CONFIRMED by the fix.** The scene batches produced no pixels because
  their vertices collapsed. This is the measured answer.
- **M15:** **not reached.** The title screen itself — emblem, "PLEASE PRESS START TO BEGIN" — has not
  been observed on the recomp, and the `0x80084000` city scene has not been compared like-for-like with
  the xemu reference.

## Next steps, in order

1. **Get past `0x1A30`** — the new missing-method stop (`SET_VERTEX_DATA4F_M + 0x30`, attribute 3
   diffuse, which the executor accepts and ignores). It needs a **runtime witness** before admission;
   two attempts are NOT EXERCISED. The witness run also names `0x0700-0x073C`, `0x18C8/0x18CC`,
   `0x1518-0x1524`, `0x1B80/0x1B84`, `0x17F8`, `0x1E20/0x1E24`, `0x1E74`, `0x1734`, `0x1968`.
2. **Look for the title screen** on the D1 build once the walk gets past that stop, and compare it by
   **content** with `logs/workers/title007/xemu/deliverable/`.
3. **Recover `0x00159330`** (the fatal `[ICALL]`, an omitted function reached only indirectly).
4. **Consider the presenter rule** — present the swap buffer the `FLIP_INC` closed rather than the last
   surface drawn — as a separate correctness question, not as the cause of the black.
5. Records: TR §23.6–§23.8, plan Current work, ledger L46/L47/L48, this plan.
6. Commit + push toolkit first, then game; then a fresh Turn Reviewer.

## The ring measurement is DONE, and it REOPENED the composite branch (Turn Review correction)

**Run `20261007-115809-590-title008-ring-admitted`** (620 s, no admit switch, **29 methods now in the
table**, `RECOMP_FLIP_TRACE=900 FROM=2400 CHANGE=1`; 42 flips traced; `diagnostic_deadline`). Analysed
by `logs/workers/title008/composite_source.py`, `analyze_ring.py`, `writer_shape.py`.

**What the ring shows (OBSERVED).** Every traced frame contains a full-screen textured batch that
writes a swap surface while sampling `0x80084000`:

```
flip 2400 (published 8205f3a6d2e48df5, NON-black):
  b[0] target=0x001b2000  tex=0x80084000  self=0 px=307200
  b[1..3] target=0x00084000 tex=0x806b6000 self=0 px=159037/306081
flip 2425 (published 156ed4086987e325, black):
  b[0] target=0x0011c000  tex=0x80084000  self=0 tris=1 px=307200
  b[1] target=0x00084000  tex=0x80084000  self=1 tris=2 px=306081
```

`px=307200` is exactly `640*480`. **`px` counts `put_pixel` write calls (`nv2a_pb_exec.c:1867`), not
unique pixel coverage**, so this is "307200 raster write calls, consistent with a full-screen pass" —
**not** proof of correct copy or full coverage, and not what the ring establishes about the composite.

**RETRACTED: the "faithful copy / Case C" conclusion was an OVERCLAIM.** The Turn Reviewer challenged
it and I verified the challenge independently against the raw ring:

- **Every surface hash — candidates and the bound texture — is taken at flip time, after every batch in
  the frame has run.** At flip 2425 the composite `b[0]` samples `0x84000` and writes `0x11C000`, and
  **then `b[1]` overwrites `0x84000`** (`tris=2 px=306081`, sampling `0x84000` itself). So the recorded
  source hash is the source's *post-frame* content, not what the composite read. "The source was black
  at the flip" and "the source held content when the composite sampled it, and was cleared afterwards"
  are **indistinguishable** from this evidence.
- **The "uniform 16-batch frame with a `self=0 px=307200` writer at all 17 black flips" claim is FALSE
  for 2 of the 17.** Measured writer shapes: `self=0 px=307200` on 15 flips, `self=0 px=306081` on 25,
  `self=1 px=306081` on 2.

**So the sampling / UV / blend / ordering branches are REOPENED, not closed.** What survives: the frame
*shape*, and that the `tris=0 px=0` feedback batches cannot be the cause. The next instrument must
**hash each batch's sampled source at the moment that batch executes**.

**A NEW missing-method class appeared, and the stop moved.** With the 29 admitted the run rejects on

```
[PFIFO] reject diag=unsupported_method method=1A30 subch=0 param=00000000 at=00059420 get=00059420 put=0005C7B4 successes=3560 rejections=1
```

i.e. **`0x1A30`**, not `0x1964` — so the admitted methods *were* exercised. `0x1A30` is
**`NV097_SET_VERTEX_DATA4F_M + 0x30`** = **attribute 3 (diffuse), component 0** of the 4-float inline
vertex family, which the executor already handles at `nv2a_pb_exec.c:2922-2934` but only for `attr==0`
and `attr==9`; other attributes are accepted and **ignored**. Admitting it activates an existing arm,
and it is a **render gap**: inline per-vertex diffuse colour is dropped. Its witness is **NOT
EXERCISED** (two runs, 620 s and 900 s, zero `admit-unknown`), so the class stays open.

**Instrumentation gaps recorded, not fixed** (Turn Planner §1.3 items not implemented): per-surface
geometry for each candidate; the bound-texture hash using the texture's own geometry; and a
content-aware `_CHANGE` key. The first two did not affect this run's conclusion (all three candidates
share `pitch=1280 clip=640x480`), but a future run with mixed geometry must not reuse this instrument
unchanged.

## PLAN_CHANGE (second, corrected)

- **Changed:** the plan's §7 branch selection. The first version of this record selected **Case C** on
  the strength of "the composite's source was black at every black flip". **That selection is
  withdrawn.** The trace's flip-time hashes cannot distinguish a black source from a source cleared
  after it was read, so **no case is selected** and the sampling/UV/blend/ordering branches are
  reopened.
- **Evidence:** the raw ring at flip 2425 (`b[0]` composite, then `b[1]` overwrites the source), and
  the writer-shape histogram (`logs/workers/title008/writer_shape.py`) which contradicts the "uniform"
  wording.
- **Why:** the user's standard is that a conclusion must follow from the evidence, and the same-event
  requirement extends to *when* a hash is taken. The remaining work is a stronger instrument plus the
  newly exposed `0x1A30` stop.

## Completion criteria (unchanged from the start plan)

Trace committed and off by default; a run past presents 2424 with keyed lines and inspected dumps;
the result classified as Case A/B/C with OBSERVED/PROVED/INFERRED labels; the 29 admitted from a
witness or recorded NOT EXERCISED; records updated; M15 only on title content confirmed by eye.
