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
surfaces: `0x00084000`, `0x0011C000`, `0x001B2000` (and `flip_modulo=3`, so the title is
**triple-buffered**). The presenter can only ever publish `drawn_offset`, `targeted_offset` or
`color_offset`, and each of those is set by that method, so the set is complete for what the
presenter can publish.

### What this establishes (OBSERVED, same-event)

**Case A is eliminated for the traced window.** `present_track_flip` selected a surface, and the
bytes published at that same flip were *that surface's bytes*, 242 times out of 242, by the
`drawn_this_frame` branch. The presenter did not publish a stale or wrong buffer, and it did not
fall back.

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
  (above). Selection and presentation are measured correct, so instrumenting them further cannot
  answer the remaining question.
- **Why:** the user's decision tree assigns Case A to a source-selection bug. The measurement puts
  the defect in the guest's own drawing/composite, so the batch ring (Turn Planner §1.1, §7 Case B)
  is the next discriminator rather than more selection tracing.

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

## Next steps, in order

1. **Per-frame batch ring** (Turn Planner §1.1): the last N batches since the previous flip —
   target `color_offset`, stage-0 texture offset/format/dims/pitch/addr, blend enable+factors, draw
   path, `idx_count`, and the UVs/texel actually sampled. Emitted only on traced flips. This is what
   connects "the guest drew black into `0x84000`" to *which batch did it and what it sampled*.
2. Re-run the deciding experiment with the ring and classify Case B vs Case C.
3. Retry the 29-method witness (a longer run, or the reject-point witness Worker B described) —
   recorded as NOT EXERCISED so far.
4. Records: plan **Current work**, TR §23.6, ledger as needed.
5. Commit + push toolkit first, then game.
6. Fresh Turn Reviewer (`codex/gpt-6.1-sol` @ high).

## The ring measurement is DONE: Case C, and the composite is FAITHFUL

**Run `20261007-115809-590-title008-ring-admitted`** (620 s, no admit switch, **29 methods now in the
table**, `RECOMP_FLIP_TRACE=900 FROM=2400 CHANGE=1`; 42 flips traced; `diagnostic_deadline`). Analysed
by `logs/workers/title008/composite_source.py` and `analyze_ring.py`.

**The composite is a full-screen textured pass, and it works.** Every traced flip carries a ring. The
frame structure at the black flips is:

```
frame batches=16 drew=1 textured=16 self_sample=15 untransformed=0
  b[0]  target=0x001B2000  tex=0x80084000  fmt=0x11 640x480 self=0 tris=1 px=307200
  b[1..15] target=0x00084000 tex=0x80084000 fmt=0x11 640x480 self=1 tris=0 px=0
```

`px=307200` is exactly `640*480`: **the composite covers the whole frame**. `tris=1` for a
full-screen quad. So the pass that turns `0x80084000` into the swap buffer **runs and writes every
pixel**.

**And it publishes exactly what its source held.** For all **17** black flips, the composite's source
(the bound texture, `0x80084000`) was **itself black** — `0 of 17` had a non-black source:

| flip | selected | sampled source | candidate 0x84000 | candidate 0x1B2000 | candidate 0x11C000 |
|---|---|---|---|---|---|
| 2425 | `0x84000` | `156e…` black | `156e…` | **`8205…`** | **`e886…`** |
| 2426 | `0x1B2000` | `156e…` black | `156e…` | `156e…` | **`e886…`** |
| 2427–2441 | alternating | `156e…` black | `156e…` | `156e…` | `156e…` |

**This is Case C, and it is a sharp result: the defect is UPSTREAM of the composite.** The composite
samples `0x80084000` and publishes it faithfully; at the black flips `0x80084000` *is* black. So the
question is no longer "why is the composite black" (it is not — it is a correct copy) but **"why did
`0x80084000` stop holding the scene?"**.

Note the contrast with the earlier archived run (`§23.5`), where `0x80084000` held a rich 3D city
scene at the dump instant: that was a *different* run whose walk had stopped. Here, at the flips that
publish black, the source is genuinely black.

**A NEW missing-method class appeared, and the stop moved.** With the 29 admitted, the run stops at

```
[PFIFO] reject diag=unsupported_method method=1A30 subch=0 param=00000000 at=00059420 get=00059420 put=0005C7B4 successes=3560 rejections=1
```

i.e. **`0x1A30`**, not `0x1964` — the 29-method admission *did* move the stop, which is itself
evidence the admitted methods were exercised. This is a new class and needs its own runtime witness
before admission (never a decode). A witness run is in flight.

**The `self=1` batches are not the defect.** 15 of 16 batches per frame sample the surface they write
(`self=1`) — that is a feedback read, and on hardware the sampled copy is the pre-pass contents. But
all 15 have `tris=0 px=0`: they are **counted but rasterise nothing**, so they write nothing and
cannot be the cause. The one batch that does write (`tris=1 px=307200`) is `self=0` — it samples
`0x84000` while writing a *different* surface (`0x1B2000`/`0x11C000`), which is the correct,
non-feedback composite. **So the composite is clean and the feedback reads are inert.**

## PLAN_CHANGE (second)

- **Changed:** the plan's §7 branch selection. The ring measurement selects **Case C**, not Case B:
  the composite is faithful and its source is black, so no composite/UV/blend fix is indicated.
- **Evidence:** `composite_source.py` — 17/17 black flips had a black composite source, and the
  writing batch is `self=0` with `px=307200` (full frame).
- **Why:** the user's tree assigns Case B to "another candidate surface has the content / the pass
  produced black". Neither holds: the pass produced black *because its input was black*. The critical
  path moves upstream to whatever should have filled `0x80084000`, and to the newly exposed
  `0x1A30` missing-method stop.

## Completion criteria (unchanged from the start plan)

Trace committed and off by default; a run past presents 2424 with keyed lines and inspected dumps;
the result classified as Case A/B/C with OBSERVED/PROVED/INFERRED labels; the 29 admitted from a
witness or recorded NOT EXERCISED; records updated; M15 only on title content confirmed by eye.
