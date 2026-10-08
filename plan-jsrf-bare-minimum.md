# Jet Set Radio Future: Windows port plan

**Status: execution authority** (adopted by the owner 2026-09-29; condensed 2026-10-06). The text
before condensation — every per-run narrative, the Phase 0/1/2 tables and the TTD line — is
`git show dcc93ab:plan-jsrf-bare-minimum.md`; the condensation and the turns it summarises are
recorded in `docs/jsrf-operating-history.md` ("2026-10-06 — plan condensed").

**Authorities, so this file does not repeat them.** `docs/agent-workflow.md` owns roles and turns;
`docs/jsrf-run-profiles.md` owns evidence profiles; `docs/jsrf-technical-record.md` ("TR §n") owns
established facts; `docs/jsrf-compatibility-ledger.md` owns every shortcut (ledger IDs `Lnn`/`Dn`);
`config/stop-chain.json` owns the runtime stop chain; `AGENTS.md` owns build and repository
discipline. This plan owns the objective, the milestones and what to do next.

---

## Direction (owner decision, 2026-09-30): the bare minimum is pragmatic

- **Take the path of least resistance to the title screen** (DoD-BOOT, M15), then to the rest of the
  slice. The fast path below supersedes any older ordering until the title screen is reached.
- **Every departure from running the original code on real hardware is recorded** in the ledger,
  classed as reimplemented, translated, emulated, wrapped, stubbed, patched, approximated or
  intentionally ignored. A shortcut is allowed; an unrecorded shortcut is not.
- **Exploratory runs may satisfy bare-minimum milestones** when the run record lists the ledger IDs
  it relied on (`docs/jsrf-run-profiles.md` §"Pragmatic bare minimum"). Strict runs remain the
  diagnostic for fidelity questions.
- **The Orchestrator runs the fast path**; the Turn Reviewer reproduces a milestone claim, with its
  ledger IDs, rather than accepting the description.

## Current work (state 2026-10-07, turn title-008)

**THE BLACK INTERVAL IS EXPLAINED AND FIXED: the vertex-program viewport constants were never loaded**
(TR §23.8, toolkit this turn). The XDK vertex programs end with the standard viewport step
`MUL o0.xyz = r12 * c[58]` / `MAD o0.xyz = r12 * r1 + c[59]`, where `c[58]`/`c[59]` are the hardware's
viewport **scale**/**offset** slots (`XFCTX_VPSCL = 0x3a = 58`, `XFCTX_VPOFF = 0x3b = 59`). **The
executor never loaded them:** `0x0A20` went only to `s_gpu.vp_offset` (read by the fixed-function path
only) and **`0x0AF0` was not handled at all** — it sat in the report's unhandled top ten at `x28024`.
With `c[58] = c[59] = 0` **every vertex-program vertex collapses to the screen origin**, the triangles
have zero area, `raster_triangle` returns at its `area == 0` test, and the batch draws nothing — which
is why `0x84000` was cleared to black each frame and never refilled, and why the full-screen composite
then faithfully copied black.

**The fix, and its measured effect.** `0x0AF0-0x0AFC` now writes `s_vp.c[58]` and `0x0A20-0x0A2C`
writes `s_vp.c[59]` **in addition to** `s_gpu.vp_offset`, so both consumers get what they read; and
stage 0's enable bit (`SET_TEXTURE_CONTROL0` `0x1B0C` bit 30) is now honoured (default enabled), so the
scene batches that disable stage 0 stop sampling the surface they draw into. Run
`20261007-124149-033-title008-d1-viewport` (620 s, no admit switch, the same trace *settings* as the
pre-fix `…ring-admitted`), against that run:

| measure | before D1 | after D1 |
|---|---|---|
| distinct `[FBPRESENT]` hashes, whole run | **10** | **658** |
| `0x0AF0` in the unhandled list | yes (`x28024`) | **absent** |
| flips 2426–2440 (transition window) | one repeated black hash | **15 distinct hashes** (15 observations; the wider 2425-2441 window has 29 observations and 16) (2425 and 2441 still black) |

At the dump (mapping gate `matches 1 / content-mismatch 0`): `0x80084000` = `efddce3b5bb02ab1`,
**93.5 %** non-black, **2677** colours — rendered, a **full 3D city scene** (towers, sky, clouds, road
markings, the green elevated highway); `0x8011C000` = `eaaa65df05fa3144`, **100 %** non-black — a
**"Now Loading"** screen. Preserved: `logs/workers/title008/surfD1/0x80084000.png` and
`…/0x8011C000.png`. **Classification:** the collapse and the missing `0x0AF0` are OBSERVED; the causal
link to the black is **strongly supported but not proven** by the before/after, which is not a
controlled experiment: the two runs differ in trace budget (400 vs 900, non-binding at 42 events each),
guest path (`ADXIO` 36 vs 6), and the stage-0 change was bundled with the viewport change. Both stop on the
same `0x1A30` reject at the same `at`/`get`/`put`, which is evidence the fix did not move the walk's stop.
It is **not** M15 and **not** a like-for-like comparison with the xemu reference.

**The same-flip trace did its job, and its claims are narrowed** (TR §23.6, §23.7, corrected after Turn
Review). The instrument (`RECOMP_FLIP_TRACE`, off by default) records at one `NV097_FLIP_STALL`, keyed
on `(flip_stalls, present serial)`, every surface the guest named with `SET_SURFACE_COLOR_OFFSET`, a
content hash of each, the hash of the bytes actually handed to the window, the surface
`present_track_flip` selected **and the branch that chose it**, and a ring of the frame's last batches.
**What it establishes:** at **242/242** traced flips the published bytes were the **selected surface's
own bytes**, by the `drawn_this_frame` branch — i.e. **copy/publication consistency**. **What it does
NOT establish:** that the tracker selected the surface the **guest** intended. `reason` restates the
same heuristic rather than validating it, and a concrete unresolved alternative exists — at flip 2425
an earlier composite writes `0x11C000` sampling `0x84000`, then a later `self=1` batch writes
`0x84000`, and the tracker selects `0x84000` because it prefers the last surface drawn. **No selection
bug is claimed**; that branch is an unresolved alternative, not a demonstrated defect. The earlier
"Case A eliminated / the presenter is correct" phrasings are narrowed accordingly, and **timing
non-perturbation is UNQUALIFIED** (no matched trace-on/off control on one binary).

**Two overclaims from this turn were caught by Turn Review and RETRACTED.** (a) The ring's "faithful
copy / Case C" reading: every surface hash — candidate **and** bound texture — is taken at **flip
time, after every batch in the frame has run**, so at flip 2425 the composite reads `0x84000` and a
**later** batch overwrites it; "source was black" and "source was cleared after the read" are
indistinguishable. Sampling/UV/blend/ordering are **reopened** (now largely moot given §23.8, but not
positively established). (b) The "uniform 16-batch frame with a `self=0 px=307200` writer at all 17
black flips" claim is **false for 2 of 17** (measured: `self=0 px=307200` on 15, `self=0 px=306081` on
25, `self=1 px=306081` on 2), and `px` counts **`put_pixel` write calls**, not unique coverage.
Instrument limitations are recorded in TR §23.6/§23.7 rather than fixed: a candidate cap of 8, hashes
using the current clip/pitch rather than each surface's own geometry, the bound-texture hash using
surface geometry, a decision-only `_CHANGE` key, and `flip_modulo=3` proving a **flip-index** ring, not
three scanout-buffer roles.

**The 29-method class is CLOSED — admitted from a genuine runtime witness.** Run
`20261007-113354-706-title008-trace-admit` logged **67** `[PFIFO] admit-unknown` lines including **all
29** (`0580-05AC`, `06C0-06FC`, `1964`) from a committed walk. `config/nv2a-runtime-witnessed-methods.json`
gained 29 entries (run, log SHA-256, line, verbatim witness text); the table regenerated to **NV097
415 → 444, exactly +29, zero removals, no other class changed** (mechanically diffed per class).
`0x1964`'s header word was re-decoded from `20261007-085324-850-title007-blacktrace` at `0x8005DAE0`
(`00041964 FF000000`) because the witness run's ring was reused and its own offset no longer holds it.

**Binary-identity caution.** The no-admit trace, the admit-witness and the trace-admit witness ran the
**same executable** (`9818c346…`) but with **confounded settings** (differing `ADMIT_UNKNOWN` **and**
`FLIP_TRACE` windows), so they are **not** a clean A/B. The `admit-witness` run also took a different
path (no `title.adx` re-reads, no save probe — path divergence OBSERVED, the stalled-read explanation
INFERRED), which is why it logged zero `admit-unknown` lines. Runs after the table regeneration or the
D1 fix use **different executables** and must not be compared without re-checking `exe_sha256`.

**The stop is now `0x1A30`, and it is a render gap — and that class is ALSO runtime-witnessed.** With
the 29 in the table the run rejects on **`0x1A30`** (not `0x1964`), so the admission was exercised.
`0x1A30` is `NV097_SET_VERTEX_DATA4F_M + 0x30` = **attribute 3 (diffuse), component 0** of the 4-float
inline vertex family — a family the executor **already handles** (`nv2a_pb_exec.c:2922-2934`) but only
for `attr==0` (position) and `attr==9` (texcoord 0); every other attribute is accepted and **ignored**,
so inline per-vertex diffuse colour is dropped. **A first claim here that this class was NOT EXERCISED
was WRONG and is retracted:** it rested on one 620 s run with zero `admit-unknown` lines, but the 900 s
run `20261007-122836-322-title008-witness-1A30-long` logged **38 distinct** witnesses
(`log_sha256 db0ec342…`), including `1A30/1A34/1A38/1A3C`. Those 38 are admitted from that witness:
**NV097 444 → 482, exactly +38, zero removals, no other class changed** (mechanically verified). The
witnessed set also names `0700-073C`, `1518-1524`, `1734`, `17F8`, `18C8/18CC`, `1968`, `1B80/1B84`,
`1E20/1E24`, `1E74`. That run then died on the fatal `[ICALL] 0x00159330` after 624 s.

**Other live blockers, none of them presentation.** The fatal
`[ICALL] Failed to resolve VA 0x00159330` (`0xE0424943`; absent from `recomp_dispatch.c`, with NOP
padding then a clean prologue at the address, i.e. an omitted function reached only indirectly) now
ends runs. And `[PFIFO] budget_exhausted … LIMIT=packets(1024)` appeared but **recovered** on the next
walk, so it is a pacing cost rather than the L40 stop — track its count.

**SUPERSEDED by the Current work section above.** The block that stood here claimed the
same-flip trace "ELIMINATES source selection" and that the ring showed a "faithful copy / Case
C". Both were **retracted during Turn Review** (TR §23.6, §23.7): copy/publication consistency
is established, semantic source selection stays **unqualified**, no case is selected from the
ring, and the black is explained instead by the **missing vertex-program viewport constants**
(TR §23.8). What survives is the 242/242 published==selected equality, the `drawn_this_frame`
branch, the three-surface candidate set, and `flip_modulo=3` as a **flip-index** ring (not three
proven scanout-buffer roles). The "uniform 16-batch frame" wording and the `px` "writes every
pixel" reading are retracted: `px` counts `put_pixel` write calls, not unique coverage, and 2 of
the 17 black flips had a `self=1 px=306081` writer.

**The M15 acceptance criterion is UNSOUND as written, and it is now corrected here.** The milestone was
defined as "a title-frame BMP whose hash is **neither** `5bdaea576b8509f5` **nor** `87683a748e27d071`" — a
hash *blacklist*. Measured directly from the archived frames (every distinct frame hashed with the same
FNV-1a the model uses, `fb_hash_rgb`, and classified by pixel content), two things are wrong:

- **`87683a748e27d071` is not a disclaimer at all — it is the blue Dolby card** (94.8% saturated blue with a
  small black box). Blacklisting it excludes a *logo* state, which is arbitrary.
- **The graffiti disclaimer renders in FOUR distinct hashes, only ONE of which is blacklisted**:
  `5bdaea576b8509f5` (listed) plus `089fe3b826bbc18d`, `cf836ec8430ffb6d` and `8205f3a6d2e48df5` (all
  unlisted; each is a red prohibition icon on a ~88% black field). The hash differs because the notice fades
  and shifts between renderings.

**This is not hypothetical: the run that "cleared the ceiling" ended on an UNLISTED disclaimer.**
`20261007-001831-252-…-sixadmitted`'s last `[FBPRESENT]` is `hash=8205f3a6d2e48df5`, which is the graffiti
disclaimer — so under the old wording that run would have **satisfied M15's letter while showing no title
screen**. The criterion must be rewritten to require the *title screen itself*, identified by content, not
by "not one of two hashes". Candidate hashes are recorded in `docs/jsrf-technical-record.md` §23.

**What boots.** With the title-path switches (`just title-run`, ledger L14–L18, L20–L25, L39, L40):
SEGA → Smilebit → ADX → Dolby → the graffiti disclaimer. Then presents stop at a fixed count: **1000** in
30 of 35 runs before game `0445a80`, **888** in all four after it (TR §19). Runs with **zero**
dispatch failures stop there too (g06 `20261005-192455-691-g06-thunk`, 905 s; f9
`20261004-185558-235-f9-frames`, 1203 s). **The title screen is not reached and M15 is not claimed.**

**The "freeze on the disclaimer" was WRONG, and it is corrected here** (TR §23.2). The disclaimer is a
**timed hold**, not a stall, and it ends with **no switch and no input**: it begins at presents **1461** and
is left at presents **2424** (t≈408–420 s), replaced by `e886cadf72766a64` and then black. Confirmed in
three independent runs including one with no admit switch, and reproduced this turn. **Every run that
looked "stuck" simply ended first** — presents advance at only ~6/s, so the 300 s `just title-run` recipe
ends at presents ~1450–1800 and `title005-fixed` ended at 2410, just short of 2424. A run must therefore
reach **past presents 2424** to say anything about the title; the 300 s recipe cannot.

**The `0x00084000` measurement is DONE, and its conclusion is NARROWER than first recorded** (TR §23.2,
corrected after Turn Review). Read offline from archived minidumps at guest VA **`0x80084000`** (never
`0x00084000`, which is XBE `.text`), `0x8011C000` and `0x801B2000` both hash `8205f3a6d2e48df5`, which
**matches that run's last `[FBPRESENT]` sample**, and rendering it shows the disclaimer. **What that
establishes:** at that frozen instant the two surfaces holding content held the disclaimer, and that hash
agrees with the last asynchronously published sample — so the §23.1 composites are not evidence of a
draw/present mismatch. **What it does NOT establish:** the frozen surfaces and the published hash are **not
paired at the same flip**, and the frozen *draw* surface (`0x80084000`) is black while others hold the
disclaimer. So this does **not** globally eliminate the presentation/source-selection branch, and it does
not show the presenter is never stale or mis-selected later. **Presentation/source-role hypotheses stay
open until synchronized per-flip evidence distinguishes them.** This was the plan's stated first decision
point; it resolves to **neither A nor B alone**: nothing title-like was drawn at the 300 s snapshot, and at
the transition a **new 3D scene** appears in `0x80084000` while the walk rejects.

**An xemu reference of the real title now exists** (TR §23.2,
`logs/workers/title007/xemu/deliverable/`): boot → Smilebit → ADX → Dolby → disclaimer (42.5–52.5 s) →
fade to black (57.5 s) → 3D city backdrop (60 s) → JSRF emblem → **"PLEASE PRESS START TO BEGIN" (≈85–95 s)**,
640×480, reached with **no input at all**. The real title is a **perspective street view with a green
elevated highway**, fully presented. The recomp's 3D scene does **not** match it as rendered (47.8 %
pure-black sky vs 1.4–15.1 %; 0.0 % green-dominant vs 8.8–16.1 %), so **it is not claimed as the title**.

**Method admission at the title transition was cleared, then the capacity bound was too** (TR §23.3, §23.4).
The 39-method list was previously decode-derived and "provenance NOT established" because the
`[PFIFO] admit-unknown` witness was truncated at **16** entries by its own cap (`NV2A_ADMIT_PENDING`), so the
log lost the tail. Fixed this turn (witness sized by `NV2A_ADMIT_LOG_MAX`, mutation-validated): one run now
logs **39** methods. **The corroborating decode is the NO-ADMIT run's report, not the witness run's** — the
witness run's own report lists **35** (it had already admitted `0x0420-0x042C` earlier, so they were no
longer missing at its final GET), and the no-admit run's report is the one that decodes exactly the same 39
(Turn Review corrected an earlier citation of the wrong report). Recorded in
`config/nv2a-runtime-witnessed-methods.json`. The table was regenerated from the witness union —
**+39, zero removals** (NV097 376 → 415) — and a run **without** the admit switch moved the stop from
`unsupported_method 0x0420` to **`sink_capacity`** at `get=0x50B1C`. So the ordering is: **method admission
first, then the capacity bound.** The rejected submission is **5732 words** (~1.4 budgets) and, per the
Advisor's replay of the model's own packet rules, splits at **whole-packet boundaries** into exactly two
units. The fix is bounded prefix commit at packet boundaries (L40's own "incremental-commit packet"), **not**
raising the 4096 cap.

**The capacity bound is CLEARED, and it was two real defects, not one** (TR §23.4, toolkit `1f86fbb`). The
walk now commits in **units that end at whole-packet boundaries**, each all-or-nothing (L40 reworded to
per-unit atomicity). The yield predicate is the word term only, so a packet is never split and no in-packet
carry is needed; the 1024-packet cap deliberately stays a **stop**, because 1025 one-word packets is under
the word budget and `tests/test_nv2a_contract.c` pins that it rejects. **The new tests found two silent
defects that inspection had missed**: `sink_count` was reset once per walk, so a second unit overran
`sink[]` (the catching test stages 6138 methods — a 4096-method stream fills the array exactly and hides
it); and the old header check bounded a unit by a **submission-wide** count, so it fired *before* the yield
and the first attempt at this fix still rejected the real stream. **Measured:** with the 39 admitted and no
admit switch, the previously-rejected kick is **consumed** — `GET` advanced `0x50B1C` → `0x5A060`
(**9553 words, 2.33 budgets**), `successes` rose to 3529, and the stop moved to a **new missing-method
class: 29 methods** (`0x0580-0x05AC`, `0x06C0-0x06FC`, `0x1964`).

**BLOCKER: the picture is black while the guest IS drawing — now LOCALIZED to the draw→present path**
(TR §23.5). With the unit fix plus `ADMIT_UNKNOWN=1` the walk reaches **4533 successes with zero
rejections** and presents advance to **3120**, far past the `successes=3523` wall. **The discriminating
measurement has been run** (`20261007-085324-850-title007-blacktrace`, **no** admit switch, stops at
`unsupported_method 0x1964`, presents black 2425→2442, ~170 s): at that dump **`0x80084000` holds a rich 3D
city scene** — `d4fa6747357b9e60`, **52.3 %** non-black, **1417** colours, including the green elevated
highway — while **both swap surfaces read pure black**. So the guest is drawing and the black is
**downstream of the draw**; the earlier "all three surfaces zero" reading came from the admit run, whose
`GET == PUT` made the dump an empty-queue instant. **Cumulative pixel writes and final content are different
measurements**, so "1.7 billion pixels vs black" is not a contradiction. **This is not yet a
presentation-bug claim**: one instant does not show whether `0x84000` is the intended present source,
whether a composite/resolve from it should have run, whether a flip is missing, or whether
`present_track_flip` chose a black surface. **The next step is the same-flip trace** (hash every candidate
surface *and* the published copy at one `NV097_FLIP_STALL`, with `present_track_flip`'s choice and reason,
keyed on `(flip_stalls, present serial)` — never timestamps, never `RECOMP_FB_VA`), which separates
**refusal** (`surface_write_refused` / the `dma_resolve` image redirect), **redirection**, **clearing** and
**source selection** in one run. Note **`0x00084000` and `0x0011C000` both fall inside the XBE `.text` span
`0x11000..0x18CB30`**, the class that guard exists for, though **no `REFUSING` line appeared**. xemu's black
is a **~2.5 s fade**, not a 170 s hold, so this is not xemu's fade being reproduced.

**The present ceiling was one stale method-table entry, and it is cleared** (TR §22). It was the
submission walk rejecting the whole stream with `unsupported_method` on method `0x1810`
(`NV097_DRAW_ARRAYS`), which was missing from the generated admission table (L39) although the
executor already implemented it (`xboxrecomp/src/kernel/nv2a_pb_exec.c`, `case NV097_DRAW_ARRAYS`).
Three independent witnesses named it: the `[PFIFO] reject` line, `g_nv2a_submit_state` in
`just gpu-report`, and the guest's own bytes at GET. **All five archived dumps already logged it as
their first rejection at submit #12**, so it was never a regression. The table was regenerated from a
ring that contains the method (exactly `+0x1810`, nothing removed) and the ceiling is gone: a
no-switch run reaches **presents = 2410** against the old 888 with **zero rejections** and
`last walk ok`, independently reproduced by `20261006-215642-480-title005-confirm`. ("No switch" here
means neither the live-mirror nor the admit-unknown switch; the runs are standard exploratory
`just title-run`s, so this is progress, not a fidelity claim.) **M15 is still not claimed** — the run
still ends on the graffiti disclaimer.

**The next blocker is the same class, and then a different one** (TR §22):

- `unsupported_method method=0BB0` (an `NV097_SET_TRANSFORM_CONSTANT` slot) after 3498 successes, with
  the pending region also carrying `0BB4`, `0BB8`, `0BBC`, `1724`, `1728` — six register-state
  methods, confirmed at runtime by `RECOMP_NV2A_ADMIT_UNKNOWN=1`
  (`20261006-213505-255-title005-admit3`). They need regenerating into the table.
- Once those are admitted the same run hits `budget_exhausted` (L40's per-walk word budget), which is
  a **different** class: its own fix (walk bounds / incremental commit), no switch, and never a
  relaxed rejection (L39).
- **Provenance caution.** Those later rings have **wrapped**, so the generator's decode and the
  model's walk can diverge: decoding `20261006-211635-913-title005-m15` from `0x1000` stops with
  `bad_target 0x00100000` and would inflate the table by a dense `0x1848`–`0x18F8` run the real walk
  never required. Take a new entry only from a decode that **reached PUT**, or from an
  `[PFIFO] admit-unknown` line of a run that exercised it.

**The six are admitted (game `779c6a0`, toolkit `46b3265`), and the next blocker is bigger than the six.** The
admission came from the runtime witness, not a decode: `config/nv2a-runtime-witnessed-methods.json` records each
`[PFIFO] admit-unknown` line with its run, log line and log SHA-256, and the generator unions them
(`--witness=`). The delta is mechanically verified as **exactly `+{0BB0,0BB4,0BB8,0BBC,1724,1728}` on class
`0x97`**, zero removals, no other class changed, `0x1810` retained (381 → 387, NV097 370 → 376). The executor
already implemented all six (`vp_method`'s constant arm and the `0x1720`-range attribute arm), so admission
activates existing behaviour rather than adding any. Ordered delivery is now pinned by
`run_ordered_constant_contract` in `tests/test_nv2a_hal.c`, which reads the real executor through a new narrow
accessor (`nv2a_pb_exec_vp_view`, toolkit `ec98ffe`); it was validated by mutation — a last-value-only constant
handler fails 15 of its assertions.

**`budget_exhausted` is NOT the next blocker, and the method list is bigger than first reported.** An offline
decode of the rejected region (GET `0x494F4`..PUT `0x4E680`, 5219 words) reports **39 distinct NV097 methods
missing from the table** through the repository's own `jsrf_gpu.missing_methods` (which expands incrementing
packets' parameter slots): `0x0420`–`0x042C`, the full `0x0480`–`0x04BC` and `0x0680`–`0x06BC` runs (16 each,
the model-view and composite matrices), `0x1748`, `0x1B40`/`0x1B44`. An earlier "eight" in this plan was
**wrong** — it counted only packet START methods and ignored the incremented slots. They were never witnessed
because `RECOMP_NV2A_ADMIT_UNKNOWN=1` **bypasses** the `unsupported_method` reject, and the witness queue
fills only on a successful commit, which a budget-rejected walk never reaches. So the six-line witness is a
**lower bound**, and a normal walk will reject on the first of these before it ever sees the budget.

**Temporal provenance of that region is NOT established.** The archived log has only **three** pointer samples
for the post-reject phase (the reject at `PUT=0x4E680`, one `still rejecting` at `PUT=0x6D060`, and the final
`0x3E984`), so the distance travelled is `114881 + k·131072` words for unknown `k` and **a full lap cannot be
excluded**. The 39-method list is therefore decoded from the final dump but **not proven to be what the walk
read**, and is conditional. (An earlier draft here said "170 re-reject lines" — that is wrong; `170` is the
`rejections=` counter, not a count of pointer records.) Confirming it needs a contemporaneous first-stop
transcript, which is the next turn's measurement. Do **not** admit these 39 from the decode: they need a
runtime witness, exactly as the six did.

**The budget mechanism is strongly indicated, not proven.** The header-path dump at `nv2a_core.c:1671` is
unconditional and never appeared in the admit3 log, while the parameter path (`:1578`) emits none — so
exhaustion was **inside a packet**, not at the 1024-packet limit, *conditional on complete stderr*. The
archived `nv2a_core.c` hash matches current source. The specific 271-parameter `0x1800` straddling packet is
read from the decode and inherits its provenance limit. **Raising the budget is not the fix** (owner
constraint, L40): the direction is bounded resumable prefix dispatch preserving carry, order and rollback,
and not publishing the fence for the original PUT until it is consumed.

**Whether the post-admission run exercised the changed path is NOT DEMONSTRATED.** The run drained the walk
with zero rejections and `last walk ok`, which shows **no observed harm** from the admission — but even that
is stronger than the evidence, since exercise was not demonstrated, so the run is close to silent about the
changed path either way. Final-ring absence is not proof of absence (the ring is reused; bytes at `0x2FF4C`
demonstrably differ from m15), "zero rejections" is not proof (the pre-admission `fixed` run also had zero),
and the `[PFIFO] submit` log is capped at 64 lines so it cannot establish that `PUT` never wrapped. Nor do
the present counters establish **no regression**: they are 1490 / 2410 / 1680 at **unequal durations**, on
different executable hashes, so they invalidate the old line-count comparison without separating a real
behavioural change from run-to-run variation. Establishing exercise or non-regression needs a matched-duration
A/B on one binary, or a contemporaneous transcript.

**The first-stop instrument now exists (toolkit `4b6cc2f`/`76c76fd`, game `1d71631`).** The previous turn could
only *infer* "inside a packet" from the absence of a dump, because only the header path printed and the submit
log stops at 64 walks. Both budget exits now call one emitter that latches the FIRST stop into the exported
submit state — which limit fired and where, the straddling packet and its remaining count, and a dense
chronological trajectory of the last 64 consumed words (headers *and* parameters). `budget_local_pc` is
separate from `submit_diag_get`, which is the **rollback origin**, not the failure point. The game report
decodes the new fields, with the base fields still decodable so older archives read correctly. Pinned by a
deterministic mid-packet test (counts `2047, 2043, 5` — the shape is forced by the sink guard, since one packet
can never exceed 2047 parameters), validated by mutation. **The next run that reaches a budget stop will now
record where it happened, which is the measurement step 1 below needs.**

Standing facts from this turn's measurements:

- The submission walk commits all-or-nothing (L40), so a rejected walk moves no GET, publishes no flip
  and calls no commit consumer — "flips and presents stop together and the 10 s `[GPU]` report never
  resumes" is what a rejection produces. The earlier reading "both stopped, so the guest stopped
  submitting" does not follow.
- **The pre-2026-10-06 fence mirror (L17) removed back-pressure**: it reported every fence complete
  whether or not the walk consumed the commands, so after a rejection D3D kept writing the ring and an
  archived dump's bytes at GET are a *later* frame's — the first `[PFIFO] reject` line names the
  original blocker, and the `RECOMP_FENCE_MIRROR_LIVE=1` A/B (`20261006-205306-080`) shows the walk
  moving past `0x8EF0` to a later blocker. Since toolkit 2026-10-06 the mirror publishes the fence of
  the last consumed kick and a rejected walk is re-tried every 100 ms; a sticky rejection leaves the
  guest in D3D's ring-space wait at `0x1914F0`, as on hardware.
- `GET ≠ PUT` alone does not mean "unknown method": budget, loop, sink, invalid handle, bad target
  and reserved-opcode rejections also pin GET. Branch on the diagnostic.
- The 1000 → 888 change (TR §19) is **not** fully explained. The overwrite-rescue mechanism above is
  the supported explanation, but the A/B run reaches presents **620**, not 888 or 1000, so it does not
  isolate those exact historical counts, and §19's runs differ in more than one revision. Treat the
  counts as a hypothesis consistent with the mechanism, not a settled causal finding (TR §22).

**The dispatch stop chain (F7) is cleared through stop 28.** Each stop was a missing, swallowed or
mis-sized recovered function found by one run; the class is now mostly found statically.

| Stops | What they were | Record |
|---|---|---|
| 1–16 | unowned gaps, spans that swallowed a later function, spans cut at a folded-alias start, a shared tail epilogue, the alias-folded job handler `0x32610`, `0x80340`/`0x80BD0` | TR §9–§10; `git show dcc93ab:plan-jsrf-bare-minimum.md` "Current work" |
| 17–28 | missing entries (`0x94AB0` class), this-adjusting thunks, under-wide spans (`0xB5EB0`, `0x48DB0`), the `0x96xxx` vtable family | `config/stop-chain.json` (gate `check-stop-chain.py`, L43): 9 runtime-confirmed (17–22, 25–27), 23 and 24 static-only, **28 (`0x81860`) repaired, not yet exercised**; TR §11–§21 |

**Static gates in `just check`:** entry extents, stack depth (TR §10; gates `stack_args = N` at a
depth-0 `ret N`), hidden entries (TR §14), dispatch-table count = array (TR §16), the stop-chain
record (L43). Measured residue (TR §18–§20): stack-depth SUSPICIOUS **67**, span-exit CUT-TARGET
**210**, span-ownership `KNOWN_OPEN` **25**, zero fatal trap-call sites in `recovered.c`.

**Run-to-run variation is first-class.** Same-binary runs take different paths (g03–g06, f25/f26);
a run that does not reach a changed address proves nothing about it (`scripts/check-run-exercised.py`),
and a run without the four title-path switches is not comparable (f8 of 2026-10-06).

## Fast path to the title screen

| Step | State | Acceptance / pointer |
|---|---|---|
| F0–F0b | done | 15 GB disk gate; copy `src/recomp/` aside before pulling an untracking commit; rebuild and test on a new toolkit before a run |
| F1 | done 2026-09-30 | `RECOMP_RDATA_GUARD=1` named the kernel-thunk-table writer (D5) |
| F2, F2b | done 2026-09-30 | D1 (DMA_PUT bit 16), ML3 (file flags) |
| F3 | done 2026-09-30 | strict horizon closed: two wrong `tail_jump_alias` folds recovered |
| F4 | met (exploratory) 2026-10-01 | SEGA card renders through the owner's NV097 consumer (toolkit `a71f937`) |
| F4b | done 2026-10-04 | the no-op `fcmove`/`fcmovne` lift held the SEGA fade; toolkit `671ab0a` translates them |
| F5 | parked | intro movies: if Sofdec blocks, skip and ledger it (*patched*/*intentionally ignored*); decoding is M29 |
| F7 | done through stop 28 | the dispatch stop chain above; residue in the backlog |
| **F8** | **cleared 2026-10-06** (toolkit `505cda5`) | the present ceiling: `unsupported_method 0x1810`, one stale table entry; the table now carries it and a run with neither the live-mirror nor the admit-unknown switch reaches presents 2410 with zero rejections (TR §22) |
| **F8b** | **done 2026-10-07** (toolkit `46b3265`) | the same class: the six runtime-witnessed methods `0BB0`/`0BB4`/`0BB8`/`0BBC`/`1724`/`1728`, admitted from the `[PFIFO] admit-unknown` witness (+6 on class `0x97`, zero removals). Ordered delivery pinned by a mutation-validated contract. Whether the post-admission run exercised them is **not demonstrated** — see "Current work" |
| **F8c** | **admission CLEARED 2026-10-07** (toolkit witness-capacity fix) | the 39 NV097 methods are now **runtime-witnessed**, not decode-derived: the `[PFIFO] admit-unknown` witness was truncated at 16 by its own cap and is fixed, so one run logs 39. Table regenerated **+39, zero removals** (NV097 376 → 415). A no-switch run then moved the stop from `unsupported_method 0x0420` to **`sink_capacity`** (TR §23.3). **Corroborating decode is the NO-ADMIT run's report (39), not the witness run's (35)** — Turn Review corrected the earlier citation |
| **F8d** | **capacity bound CLEARED 2026-10-07** (toolkit `1f86fbb`, `e85f331`) | the submission walk now commits in **units at whole-packet boundaries** (per-unit atomicity, L40), so a 5732-word submission is consumed instead of rejected. **Measured:** GET advanced `0x50B1C` → `0x5A060` (9553 words, 2.33 budgets) and the stop moved off `sink_capacity`. **Three real defects were found by tests/review, not by inspection:** `sink_count` was reset once per walk so a second unit overran `sink[]`; the old header check bounded a unit by a submission-wide count so it fired before the yield; and `admitted_unknown` re-added every earlier unit at each unit commit (published 10227 for 6137 — found by Turn Review, fixed in `e85f331`). Raising the 4096 cap was NOT the fix |
| **F8e** | **CLEARED — the black was the missing vertex-program viewport constants** (toolkit this turn, TR §23.8) | the XDK vertex programs end with `MUL o0.xyz = r12 * c[58]` / `MAD o0.xyz = r12 * r1 + c[59]`, and **the executor never loaded `c[58]`/`c[59]`**: `0x0A20` reached only the fixed-function `s_gpu.vp_offset`, and **`0x0AF0` was unhandled entirely** (`x28024` in the report's unhandled top ten). With both slots zero, **every vertex-program vertex collapses to the screen origin**, the triangles have zero area, and the batch draws nothing — so `0x84000` was cleared to black each frame and never refilled, and the composite then faithfully copied black. **Fix:** `0x0AF0-0x0AFC` → `s_vp.c[58]` and `0x0A20-0x0A2C` → `s_vp.c[59]` as well as `vp_offset`; plus stage 0's `SET_TEXTURE_CONTROL0` enable bit honoured. **Measured** (`20261007-124149-033-title008-d1-viewport` vs the pre-fix `…ring-admitted`; the two also differ in trace budget and guest path, so this is a before/after with confounders): distinct `[FBPRESENT]` hashes **10 → 658**, `0x0AF0` gone from the unhandled list, the transition window (flips 2426–2440) going from one repeated black hash to **15 distinct hashes** (15 observations; 2425 and 2441 remain black); `0x80084000` = `efddce3b5bb02ab1` (93.5 % non-black, 2677 colours) renders a **full 3D city scene**, and `0x8011C000` = `eaaa65df05fa3144` (100 % non-black) a **"Now Loading"** screen. **Not M15** |
| F6 = M15 | open | **criterion corrected 2026-10-07** (TR §23): the title screen itself, identified by CONTENT and confirmed by eye, with its hash recorded — **not** "a hash that is neither of two listed ones". The old blacklist was unsound: `87683a748e27d071` is the blue Dolby card, and the graffiti disclaimer renders in **four** hashes, three of them unlisted, so a run ending on the disclaimer (as the ceiling-clearing run did) satisfied the old letter. **An xemu reference of the real title now exists** (`logs/workers/title007/xemu/deliverable/`: emblem + "PLEASE PRESS START TO BEGIN" over a perspective city street, 640×480, reached with no input) and is the comparator. Also needs the run record with ledger IDs and Turn Reviewer reproduction |

## Next actions, in order

1. **The black interval is CLEARED — the missing vertex-program viewport constants were the cause** (TR
   §23.8). Do not re-investigate it as a presentation or composite defect. `0x0AF0` now feeds
   `s_vp.c[58]` and `0x0A20` feeds `s_vp.c[59]` as well as `vp_offset`, and stage 0's enable bit is
   honoured. Measured against the pre-fix run: distinct `[FBPRESENT]` hashes **10 → 658**, `0x0AF0` gone
   from the unhandled list, the transition window (flips 2426–2440) going from one repeated black hash
   to **15 distinct hashes** (15 observations; 2425 and 2441 are still black), `0x80084000` a full 3D city scene and `0x8011C000` a
   "Now Loading" screen. **Next: look for the title screen itself** — the emblem and
   "PLEASE PRESS START TO BEGIN" — and compare it by content with the xemu reference. Note the guest
   currently stops on the **`0x1A30`** missing method, so the run must get past that to progress.
2. **`0x1A30` and 37 more methods are ADMITTED from a runtime witness** (NV097 444 → 482, +38, zero
   removals). The remaining work in this class is the **render gap**: `0x1A30` is attribute 3
   (diffuse) component 0 of the 4-float inline vertex family, which the executor decodes at
   `nv2a_pb_exec.c:2922-2934` but acts on only for `attr==0` and `attr==9`. Inline per-vertex diffuse
   colour is currently dropped.
3. **The 29-method class is CLOSED** — admitted from a genuine runtime witness (`0580-05AC`,
   `06C0-06FC`, `1964`; +29, zero removals, mechanically verified per class). Do not re-admit, and do
   not take any of them from a decode.
4. **`0x00159330` is RECOVERED** — the fatal `[ICALL]` (`0xE0424943`) that ended runs. It had **no
   analysis-database entry at all**, because it is reached **only by an indirect call**, so no direct
   call site ever put it there. The address is a complete function, not an internal label:
   `0x159321-0x15932F` is NOP padding, `0x159330` begins a clean prologue (`push ecx; push esi;
   mov esi,[esp+0xC]`), the body runs to `0x159416 ret 0xc`, and `0x159419-0x15941F` is further NOP
   padding before the next function at `0x159420`. Added to `config/recovered-functions.json` with
   **`stack_args: 12`** — `check-stack-depth.py` caught the omission as `DEFECT/STACK_ARGS`, which is
   the gate working as intended. Stop 29 in `config/stop-chain.json`; the preservation baseline and
   provenance manifest were refreshed (they exist to make exactly this kind of regeneration
   auditable). Game CTest 45/45 after it.
   **A run now completes without the failure but does NOT exercise the address:** run
   `20261007-200329-265-title008-recovered-159330` (900 s, table at 482) reached the **full 900 s**,
   presents **2808**, **1023 distinct** hashes, with **zero** `[ICALL] Failed`, `[EXCEPTION]` and
   `[PFIFO] reject` lines — where the previous branch died at ~624 s. But
   `scripts/check-run-exercised.py` reports `0x00159330` **NOT exercised**, so that run takes a path
   that never reaches it and proves nothing about the repair; the stop-chain row stays `REPAIRED` with
   no `repair_commit`. At the dump all three surfaces are a flat **"Now Loading"** screen
   (`66e8421869c27824` / `b6c0b88aa933bc64`) — a real game UI state, further than any previous run on
   this branch, but **not** the title and **not** M15.
   The 1024-packet `budget_exhausted` that **recovered** is a pacing cost; track its count and revisit
   L40's "the 1024-packet cap stays a stop" only if it starts recurring every kick.
5. **Correct the two eliminated premises so they are not re-run.** The `sub_0019E438` "spin" is ordinary
   DirectSound lock traffic (all 16356 calls return 1; enter/leave balanced 24702/24702). The pinned
   composites from `RECOMP_FB_VA=0x8011C000` are **accumulation artifacts, not a title**, and pinned vs
   unpinned hash counts are **not comparable** because `RECOMP_FB_VA` makes `xbox_FramebufferWindowPresent`
   early-return (`fb_present.c:66-69`). **One premise here was also wrong and is corrected:** the claim that
   `0x11C000`/`0x1B2000` are "never cleared" is **FALSE** — across the archive the `clear surface` field
   takes `0x0011C000` 45× and `0x001B2000` 43×. The "only `0x84000` and `0x0` are ever cleared" reading came
   from the clear-colour line, which prints only the first 8 **distinct clear colours** (`nv2a_pb_exec.c`
   `seen[8]`), not the first 8 clears.
3. **Input is NOT the current blocker, and this is now measured, not assumed.** The xemu reference reaches
   the full title — emblem, backdrop and "PLEASE PRESS START TO BEGIN" — with **no input at all**, and the
   recomp's own boot reaches the disclaimer and the transition with no input. Input cannot currently be
   delivered (both `xbox_OhciInit` and `xbox_InputInit` are defined and declared but **never called**), but
   wiring it up is **not** on the critical path to M15; it is M16's subject. Do not spend the turn on it
   while the walk is blocked.
4. **Do not spend runs on the dispatch stop chain** until the ceiling path is finished; stop 28
   (`0x81860`) is still **not exercised** by any run (`scripts/check-run-exercised.py` reports NOT EXERCISED
   for every post-fix run), so `config/stop-chain.json` is unchanged.
5. **When a new scene appears, read the present lines before trusting the window.** Until 2026-10-06
   the window was handed `drawn_offset`, which only the software rasteriser's triangles update, so a
   frame of clears or untransformed (vertex-program) batches presented the previous buffer. The present
   choice now falls back to the buffer the frame cleared or targeted, and logs
   `[FBPRESENT] presenting targeted 0x…` the first 8 times it does. Untransformed batches are still not
   rasterised (`batches_untransformed` in the `[GPU]` report), so a targeted frame can show only its
   clear colour: an unchanged or blank picture after the ceiling clears is not proof the guest did not
   advance. **Watch the `0x0680` `composite_set` latch** (TR §23.3): it is never cleared, so a later 2D
   overlay sent as screen-space vertices could be transformed as 3D and go missing — a render risk now that
   the composite range is admitted, not a reason to withhold admission.
6. **A run must reach past presents 2424 to be informative.** The 300 s `just title-run` recipe ends at
   presents ~1450–1800, before the transition; use ≥540 s.
7. **M15** as F6 states — the title screen by content against the captured xemu reference — then M16 onward.

## Backlog (not on the critical path until it blocks)

- **Stop-chain residue:** 25 `KNOWN_OPEN` (`tests/test_recovery_span_ownership.py`), 67 SUSPICIOUS,
  `0x96F80` (`UNQUALIFIED`, named proof gap, TR §18), the 10 dead alias shims (a latent hazard, not a
  live defect, TR §18), the alias-shim census's actionable classes (12 `SWALLOWED_FUNCTION`, 59
  outside-owner shims; census commit `0cc6d5d`, counts in `git show dcc93ab:plan-turn-updated-title-004.md`
  "B5"), and the `gap_prologue` root cause (needs a full regeneration; batch the 12 sibling spans, each
  verified by decode before landing).
- **Toolkit review findings (2026-10-06): exercised on Windows this turn.** The four `dc04dc0`
  behaviours were judged: the **fence mirror** holds back-pressure as designed (a rejected walk leaves
  GET pinned and the guest in the ring-space wait, with a `[PFIFO] reject` line present) and the
  `RECOMP_FENCE_MIRROR_LIVE=1` A/B reproduces the overwrite-rescue mechanism, so the 1000/888 count
  is **not** evidence of a regression in the new mirror (TR §22). CTest
  `kernel_file_apc_test` (its `STATUS_USER_APC` assertions), `kmem`, `fence_snapshot`,
  `nv2a_present_track`, `nv2a_submit_diag` all pass; `just test` 44/44 and `just check` clean. Still
  open: the `[APUWAIT]` line cap, and **stop 28 (`0x81860`) is still not exercised** by any run.
  **Fence-ordering audit done (static, read-only):** `0x1912A0` has exactly 5 direct callers, found by
  two independent methods; no raw dword spelling of the address was found in the image, which is **not**
  proof that a computed or indirect call cannot exist.
  The literal comment "D3D advances the counter before it writes PUT" is **false for two of them**
  (`0x19167C` on its `0x19165C` path, and the `0x192440` bring-up kick), but neither can manufacture a
  false ceiling: `0x191390` returns the **pre**-advance counter as the fence and then advances by 2, so
  the live counter is always ≥ any recorded fence. A real staleness path exists in principle —
  `0x191390` with arg bit 1 advances *without* kicking, and `nv2a_retry_stalled_walk` can commit with no
  new kick — but it is not what happened here, and it is distinguishable at runtime (a lagging mirror
  produces no `[PFIFO] reject` and no rise in `consecutive_rejections`; a rejected walk does both).
- **Undiagnosed:** the XBE `DOLBY` section being written although marked read-only (section protection
  is not enforced; seen with the `0xFFC00000` fill, which TR §9 explains as the `0x32610` misdispatch,
  closed by its recovery), and the second cause of the disc-error dialog at ~950 s with no OOM
  (f5-long; TR §8) — the L41 caller logs are in place for the next long run.
- **Carried chores:** re-cite `P0.1-AC1` by symbol (its line numbers moved); the DSP provenance record
  (the A4b2-NR instrumentation is not listed).
- **Carried technical items:** C2 kernel-memory follow-ups, C4 D3D resource release, C6 `/we4013` on
  Windows, T16's `--coalesce-functions` half, T19 retail-byte oracles, ML4 APU interrupt delivery,
  ML7 D3D11 renderer (cel shading for M19–M20), GP port follow-ups and the AC'97 registers
  `0xFEC0017C`/`0xFEC00100` before M23, `PIO_FREE` (deferred at `O-OPEN`, TR §4).
- **Owner items:** removing the lifted code from history needs a force-push
  (`docs/reviews/lifted-code-history-scrub.md`); the repository licence; W8 fallback routes.

## Definition of done — the minimum playable slice

All of the following under **any** run profile, provided every path the result relies on that is not
*emulated* or *translated* is in the ledger and the run record lists those ledger IDs.

| DoD | Criterion | Measured by |
|---|---|---|
| DoD-BOOT | Launch to the title screen; the title frame is presented. | M15 |
| DoD-INPUT | A host controller drives menu navigation through the guest's own XAPI input path. | M16–M17 |
| DoD-PLAY | New game loads the opening area; the player skates, turns, jumps and the camera follows. | M18–M21 |
| DoD-GRAFFITI | One graffiti interaction completes and the game's own progression state records it. | M22 |
| DoD-AUDIO | Sound effects and music are audible at the correct pitch. | M23–M24 |
| DoD-SAVE | Save, exit, restart and resume to the saved location/progression; a missing save is handled. | M25 |
| DoD-STABLE | A 15-minute soak with no invalid indirect call, no ABI violation and bounded memory. | M26 |

A window opening is not the slice; one playable scene is not the game.

## Milestones (M00–M06 done; 06b closed into the fork fixes)

Any profile is acceptable for these; each acceptance record lists its ledger IDs. "xemu ref" is a T3
capture of the same checkpoint. SSIM/correlation thresholds guide the judgement; they are not a strict
gate.

| M | Milestone | Acceptance (artifact → PASS) | Status 2026-10-06 |
|---|---|---|---|
| 07 | CRT and game initialization | `verify-initializers.py` passes; the main-loop entry is reached; zero invalid indirect calls / ABI violations | boot reaches the logo loop (exploratory); not accepted |
| 08 | Paths and first real asset read | a `Media` read whose byte count and hash equal the retail file; a missing file returns `STATUS_OBJECT_NAME_NOT_FOUND` | reads succeed (`title.adx`, cache files); criterion not measured |
| 09 | Allocation and ownership | `[KMEM] summary` over a boot: `commit_rejected=0`, `release_failed=0`, `region_table_full=0` | heap bookkeeping fixed (toolkit `dbeb284`), zero OOM since; not measured |
| 10 | Timers, threads, synchronization | `[GMETER] anomalies=0`; every boot-path wait satisfied by a modelled cause | open |
| 11 | Graphics interception decided | the decision recorded; a fixture feeds committed methods to the back end | **decided**: executor path (C5, L16/L18) |
| 12 | Window and clear | ≥ 60 presented frames whose clear follows the guest's own clear methods | logos presented (exploratory); not accepted |
| 13 | One game-owned UI primitive | first UI frame vs xemu ref, SSIM ≥ 0.95 on its bounding box | open |
| 14 | One menu texture | decoded texture bytes equal the xemu ref dump or an independent decode | open |
| **15** | **Title screen** | 60 consecutive frames vs xemu ref (SSIM ≥ 0.90); intro FMV handling stated | **open — F8d (capacity), then F6.** The xemu reference is captured (emblem + "PLEASE PRESS START TO BEGIN" over a perspective city street, 640×480, no input needed); the recomp's 3D scene does **not** match it yet |
| 16 | Controller input | with `RECOMP_USB`, a host pad press produces the guest's own XID report and the title's input state changes; `RECOMP_PAD_PRESS` is not admissible | open (toolkit-provided: verify, not build). **Not on M15's critical path:** xemu reaches the full title with no input |
| 17 | Main menu navigation | host pad drives start/options/back; the menu-state global changes | open |
| 18 | New game loads the opening area | loader completes; files read archived with sizes and hashes; object count > 0 | open |
| 19 | Opening scene and character | spawn frame vs xemu ref SSIM ≥ 0.90; depth/transforms at three camera positions | open (needs ML7 or executor shading) |
| 20 | Distinctive rendering | cel shading and outlines at three views, SSIM ≥ 0.90 | open |
| 21 | Skating and camera | position follows a held stick; jump and landing; frame-time p99 ≤ 2× median | open |
| 22 | One graffiti interaction | tag-completion flag set and paint count decrements (named globals) | open |
| 23 | Sound effects | APU output non-silent at rate; one effect's PCM correlates ≥ 0.9 with an xemu capture | open |
| 24 | Music and streaming | 5 minutes of music, underrun counter unchanged, memory bounded | open (JSRF streams ADX) |
| 25 | Save and resume | isolated save root; restart resumes; missing save handled; a regression test covers save enumeration | open |
| 26 | Stability | 15-minute soak: zero invalid indirect calls / ABI violations, bounded `[KMEM]`, `[GMETER] anomalies=0` | open (DoD-STABLE) |

Graphics, input and audio may be reordered when the boot path demands it. **After the slice
(27–36, scope unchanged):** area transition, second character and challenge, cutscenes and FMV
(Sofdec), all areas, missions, full playthrough, optional content, Windows hardening, performance,
reproducible package — each gets criteria in the same form when it becomes next.

## IDs other files cite

Records, scripts and the TR cite these IDs; their full text is in
`git show dcc93ab:plan-jsrf-bare-minimum.md` (old section numbers §0–§14 likewise).

| IDs | What | State |
|---|---|---|
| V1–V5 | Phase 0 re-baseline on Windows (build, regenerate, strict re-baseline, read-only checks, recovered-functions audit) | done 2026-09-29/30 |
| T1–T14 | Tooling: TTD (T1), XbSymbolDatabase (T2), xemu oracle (T3), Windows CI (T4), clang-cl//analyze (T5), `just` (T6), pre-commit (T7), DuckDB `logq` (T8), access enumerator (T9), citation lint (T10), review capture (T11), per-run doctor (T12), startup receipts (T13), disk gate and retention (T14) | done; T11/T13's scripts retired with the packet lifecycle (2026-10-03); T8 gained the present/PFIFO tables 2026-10-06 |
| T15–T19 | Mercenaries-derived tools: generated-body parity/overlay (T15), `just regen` inputs (T16), content-sniffing asset guard (T17), post-generation patches (T18), retail-byte function oracles (T19) | T15, T17, T18 done; T16 half done (`--coalesce-functions` needs a Windows regeneration); T19 not started |
| W1–W16 | Workflow checks tied to measured failure patterns | done, each with its check; W8 (fallback routes) owner-reserved; the packet-only checks (W1, W3/W10, W6, W7, W13) retired 2026-10-03; W15 gained the reverse direction 2026-10-06 |
| C1 | Attribute the kernel-thunk-table write | superseded: F1 found the writer (D5) |
| C2, C4 | Kernel-memory follow-ups; D3D resource-release re-check | open, backlog |
| C3 | NV2A action methods (L19) | parked until after the slice |
| C5 | Rendering architecture | decided for the bare minimum: the executor path (L16, L18) |
| C6 | Implicit declarations | declarations in; `/we4013` and its MSVC build on Windows remain |
| C7, C8 | A4b2 P4 transfer bridge; upstream/fork cadence | carried; chore on each upstream release |
| ML1–ML10 | Lifts from Mercenaries-Recompiled (MIT root; its `src/apu`, `src/nv2a` are xemu-derived LGPL; never its xemu DSP oracle) | ML1 serial guest mode done, opt-in (L34); ML3 file I/O done (L30, L31); ML4 mostly done (APU interrupt delivery open); ML2 heap replacement, ML5/ML6 fallbacks, ML7 D3D11 renderer (for M19–M20), ML8 ISO tooling, ML9 input/options (after M15), ML10 unified physical memory — not started |

## Tooling for the fast path

`just --list` is the authority on recipes; these are the ones the next actions use.

| Question | Tool |
|---|---|
| Build, test, check | `just build`, `just test`, `just check` |
| A comparable title-path run | `just title-run <label> [seconds]` (sets `RECOMP_APU_TRAP`, `RECOMP_PB_EXEC`, `RECOMP_FB_WINDOW`, `RECOMP_FB_PRESENT_DUMP_EVERY`) |
| Why the GPU stopped consuming | `just gpu-report <run>` (pending stream, missing methods, predicted diagnostic, `g_nv2a_submit_state`); `[PFIFO] reject`/`still rejecting`/`recovered`/`admit-unknown` log lines |
| Present counts and PFIFO history without hand-grepping | `python -X utf8 scripts/logq.py <run> --saved present-ceiling` (tables `presents`, `pfifo`, `gpu_flips`; `just logq <run> "<sql>"` for ad-hoc SQL) |
| Did a run exercise an address | `scripts/check-run-exercised.py` |
| Span and ABI defects from the bytes | `just stack-depth`, `just hidden-entries`, `scripts/check-entry-extents.py`, `scripts/check-table-targets.py`, `scripts/check-span-exits.py` |
| Reading a dump | `scripts/check-dump-mapping.py` first, then `scripts/inspect-jsrf.py`, `scripts/read-host-symbol.py` |
| The xemu oracle | `scripts/xemu-oracle.py`, `scripts/xemu-gdbstub.py`, `scripts/xemu-diff.py` (T3) |
| Admitting new NV2A methods | `scripts/gen-nv2a-method-inventory.py` (pass `--put=`) |

## Owner decisions in force

- The bare minimum is pragmatic, every shortcut ledgered (2026-09-30).
- Lifted code (`src/recomp/gen/`, `recovered.c`) is untracked and rebuilt locally (2026-09-30).
- Ordinary runs need 15 GB free (`scripts/check-disk-gate.py`); TTD recordings keep the 50 GB
  expectation; deleting old runs is an owner decision.
- The strict-horizon ledger's scope is from 2026-09-29 onward (`--since all` for the archive).
- C3 is parked and C5 decided (the executor) for the bare minimum.
- The packet lifecycle and its tooling were retired (2026-10-03); existing packets, reviews and rulings
  stay as history.
- Commit and push policy, and the public-repository rules, are in `AGENTS.md`.

## Evidence rules (kept)

- `MEASURED` = inspected source or artifact with an identity and procedure; `INFERRED` = a hypothesis.
- Missing, malformed, stale, unexercised or `CANNOT VERIFY` evidence is never PASS; a failed
  measurement cannot be converted to PASS by any role; a post-review change reopens the criterion.
- Bare-minimum milestones accept exploratory runs whose records list their ledger IDs; a *fidelity*
  claim still needs a strict run, and exploratory or fixture evidence stays bounded to what it measured.
- A run that does not reach the changed address is NOT EXERCISED, not a pass.
- Values transcribed by hand into a record are re-checked by a second worker against the artifact
  (W5) until tool citation (T10) is the only path; TTD query output is a lossless decision input only
  under the W11 conditions in `docs/jsrf-run-profiles.md`.
- Original assets and existing saves remain unchanged.

## Closed packets

| Packet | Result | Claim (limits in TR) | TR |
|---|---|---|---|
| P0.S, P0.1–P0.7 | ACCEPTED | evidence loop: runner, profiles, dumps, review records, provenance | `docs/reviews/p0-3-to-p0-7-acceptance.md` |
| `A3a-r25` | ACCEPTED | AC'97 codec-ready modelled as `GS.bit8 := GC.bit1` | §3 |
| `A4a-r2` | ACCEPTED (discovery) | GP start handshake observed; row `O-6` | §4 |
| `A4p-r1` | ACCEPTED (discovery) | `PIO_FREE` gate-only at 28 direct reads (`O-GATE`) | §4 |
| `A4s-r6` | ACCEPTED | toolkit synced to v0.11.0 | §1 |
| `A4b1-r4` | ACCEPTED (stage 2) | GP DSP56300 core ported, GPL-2.0-or-later | §4 |
| `A4b2-r8` | ACCEPTED | the GP engine's DMA write clears the DSP pending word in a strict run | §4 |
| `A4b2-NR` (discovery) | `O-TWO-LEG` | the stub inputs do not reach the clearing descriptor | §4 |
| `A2h-slot-writer-attribution-r2` | ACCEPTED, `O-OPEN` | writer found, row withheld | §5 |

Owner-direct work outside packets (toolkit syncs to v0.12+, CRT divide helpers, the v0.12
regeneration, the fork audit and fixes) is in TR §1, §2 and §7. `A2h-r6` was retired (its premise was
refuted; the failure was already fixed by `cb7cae2`).
