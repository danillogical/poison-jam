# `A2h-null-slot-triage-r1` — **stage-1 acceptance review**

**Reviewer:** acceptance reviewer (stage 1), 2026-09-28.
**Packet:** `docs/packets/a2h-null-slot-triage.md`, revision **`A2h-null-slot-triage-r1`**, **80 lines**
(32704 bytes). **Under review:** `docs/reviews/a2h-null-slot-triage-execution-evidence.md`.
**Authority:** `docs/agent-workflow.md` §5.8 (discovery acceptance = artifacts exist, match the commands,
correct outcome row selected), §2.2 (contract roles), §2.4 (evidence invariants), §6.1.6 (decision inputs
lossless and bounded by construction), plus the relayed shape preflight `SHAPE: REDIRECT`
(`docs/reviews/a2h-critical-path-advisor-ruling.md:81-149`).

**DISPOSITION: ACCEPT**

A discovery packet never satisfies a strict criterion and this review does not read it as one. Everything
below is the reviewer's own recomputation from the named bytes and logs; where a claim could not be checked
directly it is marked `CANNOT VERIFY`, never ASSUMED.

---

## Criterion 1 — the named artifacts exist: **AGREED**

| Artifact | Observed |
|---|---|
| `logs/runs/20260928-001502-101-a2h-null-slot-inert-off/` | present; `jsrf_run.log` 1124225 B, **`EC2DD672F1E6B89C81CE86060683523A3F1B8A518A588DC2832B935E53388029`**; `metadata.json`, `result.json`, `stacks.txt`, `process.dmp`, `build-source.json`, exe/pdb/map, `save-root` |
| `logs/runs/20260928-001520-474-a2h-null-slot-authoritative-on/` | present; `jsrf_run.log` 2576320 B, **`689B2AB2D510397A472A48CFFA2AD60A8CF55D4A9B77C3B6BF8A0B92671E0E09`**; same artifact set |
| `scripts/a2h-slot-triage-classify.py` | present; the committed revision (`0d7bed6`) self-test reports **`SELF-TEST OK`, 8 checks**; the current working-tree revision reports `SELF-TEST OK`, **11 checks** |
| `docs/reviews/a2h-null-slot-triage-execution-evidence.md` | present, 148 lines |

Both `metadata.json` files record `run_log_sha256` equal to the hashes I computed
(`ec2dd672…` / `689b2ab2…`), so each log is bound to its own archive.

**Exactly two runs** named in this packet exist under `logs/runs/` matching `a2h-null-slot*` — no third.

## Criterion 2 — the artifacts match the commands the packet specified: **AGREED**

Packet lines 49-59 prescribe Run 1 / Run 2 with `--profile strict --seconds 8` and the named labels.

| Packet requirement | Verified |
|---|---|
| labels `a2h-null-slot-inert-off` / `a2h-null-slot-authoritative-on` | both directories carry exactly those labels |
| `--seconds 8` | `metadata.json` `seconds: 8` in both |
| `--profile strict` | `metadata.json` `profile_source: "explicit"`; re-running `scripts/check-run-profile.py` on both directories gives **STRICT / STRICT** (`checked 1; strict 1`) |
| Run 1 gates **absent** | `metadata.json` settings list contains **no** `RECOMP_KERNEL_WATCH`, `RECOMP_KERNEL_WATCH_ALL` or `JSRF_TRACE_A2H_SLOT`; log holds **0** `[A2HSLOT]` and **0** `[KWATCH]` lines |
| Run 2 gates **exactly** `RECOMP_KERNEL_WATCH=0x1C4064`, `RECOMP_KERNEL_WATCH_ALL=1`, `JSRF_TRACE_A2H_SLOT=1` | metadata effective settings list all three with those values, and nothing else beyond the shared set |
| shared strict env `RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, `RECOMP_KERNEL_LOG_BUDGET=100000` | present and **identical** in both; no `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED`, `JSRF_ABI_CONTINUE` |
| same build | `exe_sha256`, `pdb_sha256`, `map_sha256`, `collector_sha256`, `build_source_sha256`, `xbe_sha256` **all MATCH** across the two runs; `repository_identities` identical (`project 009f624…`, `toolkit c151d4e…`) |
| disposable save root | both `save_root` point into their own run directory |
| Exp1 tool and suites still green | `python -X utf8 -m unittest scripts.test_a2h_null_slot_triage scripts.test_a2h_frame_audit scripts.test_a2h_oom_slice scripts.test_pio_free_demand` → **Ran 106 tests … OK** |

Run 2's `result.json` and Run 1's are the same class (`unhandled_exception`, exit code `3762440515`,
`dump_ok: true`); durations differ (`5.13 s` vs `7.93 s`) — expected, since Run 2 prints ~15.5k extra lines
inside the same 8-second bound. That is a capture-bound difference, not a profile or build difference.

## Criterion 3 — the correct outcome row was selected: **AGREED**

I re-ran the classifier myself, with both the committed revision and the current working-tree revision:

```
python -X utf8 <classify.py> logs/runs/20260928-001520-474-a2h-null-slot-authoritative-on
row = O-NO-BOUNDARY-TRANSITION
install_control.ok = True ; coverage.series_complete = True
terminal = {tid 44768, live 0x00000000, call #5555, observed 0}
any_sample_saw_zero = False ; kwatch_change_lines = 0
```

Against the packet's own row definitions (lines 64-70), independently:

| Row | Applicable | Basis |
|---|---|---|
| `O-IDENTITY` | no | XBE SHA `FD190557…` matches the packet's baseline pin; exe/pdb/map/collector/build-source identical across runs; toolkit pin `c151d4e` matches; both runs STRICT; install control passed |
| `O-OPEN` | no | see the adjudication below — the "index gap/duplicate within a thread" clause does **not** fire |
| `O-BRIDGE` | no | requires a first `nonzero→0` **inside** a bridge. Independently: **0 of 15498** samples read zero (distinct value set is `{0xFE000104}`), and the latch recorded **no** transition (`observed=0`) |
| `O-GUEST` | no | requires an ordered `previous-after != 0 → next-before == 0`. No sample ever read zero, so no such pair exists |
| **`O-NO-BOUNDARY-TRANSITION`** | **YES** | valid install control; last-nonzero samples on every thread; complete per-thread boundary series; raw-zero terminal ICALL (`[ICALL] invalid target 0x00000000 … return=0014982E`); no first-zero boundary transition before it |

The row's own successor (`A2h-slot-read-path-displacement`) is named, and the evidence record states
explicitly what the row does **not** establish — see "over-reading an absence" below.

---

## Install control — **independently verified**

Claim: `[A2HSLOT] install tid=44768 slot=001C4064 raw=80000115 installed=FE000104 index=65`.

I read the original XBE directly (`game/default.xbe`, 2281472 B, SHA-256
`FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` — matches the packet's baseline pin):

- section table: `.rdata` at **VA `0x001C3F60`**, virtual size `0x00027800`, file raw offset `0x001B4000`;
- the packet's table base `0x001C3F60` is therefore the first byte of `.rdata`;
- `0x001C4064` → file offset `0x001B4104` → **value `0x80000115`** ✓ (claimed `raw=80000115`);
- `(0x001C4064 − 0x001C3F60)/4 = 0x104/4 = 65`, exactly ✓ (claimed `index=65`);
- toolkit `kernel_bridge.c:75`: `#define KERNEL_VA_BASE 0xFE000000u`, and `:9317`
  `uint32_t synthetic = KERNEL_VA_BASE + i * 4`; `0xFE000000 + 65*4 = 0xFE000104` ✓ (claimed
  `installed=FE000104`);
- `0x80000115 & 0x7FFFFFFF = 277`, so the image value is the ordinal-277 marker as the packet states.

The install line occurs exactly **once** in Run 2, and `jsrf_slot_latch_install` (`src/diagnostics.c:37-51`)
independently compares both values and records `install_ok` rather than assuming the match. **AGREED.**

## Run 1 inertness — **independently verified**

| Check | My measurement |
|---|---|
| `[A2HSLOT]` lines, Run 1 | **0** |
| `[KWATCH]` lines, Run 1 | **0** |
| non-gate env | `RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, `RECOMP_KERNEL_LOG_BUDGET=100000` — identical in both; Run 1's settings list contains no gate var |
| OOM | `NtAllocateVirtualMemory … size=598869040` → `xbox_HeapAlloc: out of memory (requested 598869040, used 12715008/50855936)` → `[KERNEL] → returned 0xC0000017` — **identical text in both** |
| terminal ICALL | `[ICALL] invalid target 0x00000000 tid=… esp=00F7FD00 return=0014982E` — identical (only `tid` differs: 65896 vs 44768, a host handle) |
| exception | `[EXCEPTION] … code=0xE0424943 RIP=0x7FFA6EB441CA`, identical register block (`eax=0x4C000020 ecx=0x000001B8 edx=0x00000000 esp=0x00F7FD00 …`) — identical in both |

**Prefix comparison — done myself.** The guest is genuinely multi-threaded in both runs (Run 1: 6 threads
with dispatch counts 5555/872/180/143/131/2; Run 2: 5555/1385/328/259/222/2), and the two logs interleave
threads differently, so a naive line-by-line diff diverges almost immediately. That is expected and is not
itself a prefix difference. The meaningful test is per-thread, and the packet requires the *terminating
thread's* numbered prefix through the OOM:

- terminating thread = the one reaching `#5555`: Run 1 `tid 65896`, Run 2 `tid 44768`;
- compared all **5555** dispatch records on that thread by `(call #, ordinal, slot, esp, return PC)`:
  **0 differences**;
- last seven records identical in both: `#5549 ord 129`, `#5550 ord 161`, `#5551 ord 277`,
  `#5552 ord 294`, `#5553 ord 277`, `#5554 ord 184 ret 0x00149E50 → 0xC0000017`, `#5555 ord 294`.
- including the `[KERNEL] → returned` value there are 187 differences, all of them an artifact of my own
  stitching (a bare `→ returned` line carries no `tid`, so it cannot be attributed to a thread); none of
  them is a difference in the dispatch tuple.

**The claimed identical numbered prefix through `#5554` is confirmed, on the terminating thread, by
measurement.** Both runs terminate with the same event on the same guest frame. Run 1 therefore reproduces
the intended fault and the comparable prefix, so the packet's `O-OPEN` stop ("Run 1 … differs at a required
comparable guest prefix or lacks the target event") does **not** fire. **AGREED.**

Advisory (not blocking): the evidence record says "identical numbered guest prefix" without saying the
comparison is per-thread. A reader who diffs the two logs line-by-line will find divergence at once and may
conclude the claim is false. The underlying claim is true; the wording should be tightened to
"identical per-thread prefix on the terminating thread".

## Counts — **independently recomputed**

| Quantity (Run 2) | Evidence record | My recomputation |
|---|---|---|
| `[A2HSLOT]` before-samples | 7751 | **7751** |
| `[A2HSLOT]` after-samples | 7743 | **7747** |
| total samples | 15498 | **15498** (7751 + 7747) |
| distinct sampled values | `0xFE000104` only | **`{0xFE000104: 15498}`** |
| samples reading zero | 0 | **0** |
| `[KWATCH]` change lines | 0 | **0** (1 `[KWATCH]` line total, the initial `before` print) |
| latch first-zero transitions | 0 | **0** (`terminal … observed=0`) |
| terminal read | `0x00000000` | **`0x00000000`**, call `#5555` |
| per-thread dispatch counts | 5555/259/2/328/1385/222 | **5555/259/2/328/1385/222** (tids 44768/48528/60860/65292/65888/65956) |
| per-thread before-samples | 5555/259/2/328/1385/222 | **5555/259/2/328/1385/222** — exact match on all six |

One quantity in the record is off by four: **after-samples are 7747, not 7743** (and 7751 − 7747 = 4
outstanding `before`s at capture, matching my stack-depth computation: end depth 1 on four threads, 0 on
two). Total samples (15498) and every per-thread before/dispatch figure are correct. This is a
transcription error in a non-decision quantity and does not touch the row; **advisory**, listed under
Deferred.

Note also that "15498 sampled bridge boundaries" is the count of *samples*, not of distinct boundaries:
7751 boundaries were each sampled twice (before/after) minus four whose after never printed. The
substantive claim — every sampled value was `0xFE000104` — holds either way.

## Nesting explanation — **real; corrected completeness test passes**

*(a) Is nesting real?* Yes, and I confirmed it from the log rather than from the record's prose. The
decisive context in Run 2 is around `jsrf_run.log:9130-9156`:

```
[KERNEL] #1757: ordinal 219 … tid=44768
[A2HSLOT] tid=44768 call=#1757 ordinal=219 … phase=before
[KERNEL] #1758: ordinal 232 … tid=44768
[A2HSLOT] tid=44768 call=#1758 ordinal=232 … phase=before
[KERNEL] #1759: ordinal 225 … tid=44768
[A2HSLOT] tid=44768 call=#1759 ordinal=225 … phase=before
[A2HSLOT] tid=44768 call=#1759 ordinal=225 … phase=after
[A2HSLOT] tid=44768 call=#1759 ordinal=232 … phase=after
[A2HSLOT] tid=44768 call=#1759 ordinal=219 … phase=after
```

Three `before`s, then three `after`s all stamped `#1759` — the counter had advanced to 1759 before the
outer calls unwound. The `after` line is printed after `bridge()` returns, at which point
`g_kernel_call_count` (RECOMP_TLS, per-thread, incremented on entry) already reflects any nested dispatch.
The `ordinal` field changes on each `after` (225 → 232 → 219), which is exactly the unwinding order.

I reproduced the Session's first (wrong) test and its numbers: pairing `after` to `before` **by call
number** leaves `before`-numbers 1, 1757, 1758, 1762, 1763 with no matching `after` on tid 44768 — **the
claimed 5 gaps**, plus duplicated `after` numbers 1759 and 1764. My LIFO re-pairing shows only **4**
`after` lines whose number differs from the `before` they actually match (the fifth "gap", call #1, is the
outermost call still on the stack at capture — my depth walk leaves end depth 1 on tid 44768 with ordinal
255 outstanding). So the mechanism is confirmed and the "5 gaps" figure is the right shape for the wrong
test; the details differ by one and are immaterial.

*(b) The "595 nested adjacencies" figure is NOT reproduced.* I count **5** same-thread `before→before`
adjacencies in Run 2 (all on tid 44768), not 595. My per-thread maximum stack depth is **4**, not the
claimed 7. 595 is plausibly a count from a different (global, cross-thread) definition, but as printed it
does not match the artifact. This is a **numerical overstatement of a corroborating detail**; the nesting
mechanism itself is real and the conclusion drawn from it is unaffected. Advisory — but it should be
corrected or re-derived before it is cited anywhere else, because an inflated number that a reviewer can
falsify damages the credibility of the correct parts.

*(c) Does the corrected test pass?* Yes. The correct predicate — every `[KERNEL]` dispatch has a `before`
sample on that thread — holds on all six threads with **zero** missing, and each thread's index set is
exactly `1..N` with no gap and no duplicate (verified independently of the classifier). Per-thread:
5555/5555, 259/259, 2/2, 328/328, 1385/1385, 222/222. No unsampled thread. Both real failure modes
(unsampled thread; dispatch lacking a before-sample) are pinned by fixtures (cases 5 and 6) and fail closed
to `O-OPEN`.

## The crux — does `O-OPEN`'s "index gap/duplicate **within a thread**" clause apply? **NO**

Packet line 67 and line 72 both make per-thread index integrity an explicit `O-OPEN` trigger:
*"cap or index gap/duplicate within a thread"*, and *"whose indices must form `1..last_call_index` with no
gap or duplicate"*. The Session's evidence record (as committed at `0d7bed6`) did **not** test this clause
directly — it argued past it via the nesting explanation.

**My adjudication: the clause does not fire, but the original evidence did not establish that, and this is
the one place where the record as written was incomplete.**

- The clause tests the **dispatch index series** (`[KERNEL] #N`), not the before/after pairing. Nesting
  perturbs the `after` stamps; it does not perturb the `before` stamps or the dispatch indices, because a
  `before` is emitted (and the index is incremented) at dispatch entry, before any nested call can occur.
  So the nesting artifact is irrelevant to this clause — which is the right answer, but for a reason the
  record did not state.
- Measured directly: on every one of the six threads the parsed `[KERNEL]` `#N` series is exactly
  `1..N`, `min=1`, `max=N`, `distinct=N`, no duplicates, and `N` (max 5555) is far below the per-thread
  `RECOMP_KERNEL_LOG_BUDGET` of 100000. No cap, no gap, no duplicate. **The clause is not triggered.**
- The packet's `O-OPEN` clause is therefore discharged **by measurement**, not by the nesting argument.

**Post-review correction observed.** During this review the working tree carried an **uncommitted** addition
to `scripts/a2h-slot-triage-classify.py` (`git status`: `M scripts/a2h-slot-triage-classify.py`) adding
`index_integrity()` — an explicit per-thread contiguity/duplicate/cap test with three new fixtures
(cases 9-11) — and wiring it into `classify()` so a dirty thread fails closed to `O-OPEN`. I ran it: 11
self-tests OK, and on Run 2 all six threads report `clean: true`, `all_threads_clean: true`. That closes the
gap properly. **The change has since been committed** (the tree is now clean against `HEAD`; `git diff HEAD
-- scripts/a2h-slot-triage-classify.py` is empty) and the current committed tool still selects
`O-NO-BOUNDARY-TRANSITION` on Run 2. One caveat I record rather than resolve:

1. §2.4.7 ("reviews bind to bytes") — the row as recorded at `0d7bed6` was produced by the revision that
   lacked this test. I independently supplied the missing test myself and the committed tool now carries
   it, so the row stands on both; stage 2 should confirm it re-runs the current committed tool.
2. `index_integrity()` is called with `budget=None`, so `cap_reached` is always `False`; the "cap" half of
   the packet's clause is not actually evaluated against the 100000 budget. Fixture-pinned but not
   exercised on real data. Advisory — the measured headroom (5555 vs 100000) makes the cap moot here, but
   the parameter should be wired for the check to be honest.

One further note on fixture case 11: it is written as `#1, #1, B(1), #2, B(2)`, i.e. the duplicate line is
**adjacent** and the sequence is otherwise contiguous — that is a genuinely re-printed event, and the
tool's "identical content ⇒ not an index defect" rule is sound. The fixture's own comment records that an
earlier version was wrong and the tool was right to reject it.

## Toolkit scope — the three test-file stubs: **WITHIN SCOPE**

The packet (line 43) restricts toolkit advance to `kernel_bridge.c` (install loop `:9198-9254`, dispatch/
watch seam `:8944-9094`), `src/kernel/recomp_diagnostics.h` "if the already established toolkit→game
callback needs a declaration", and game `src/diagnostics.c`/`.h` plus `src/recomp_manual.c:35-43`; it says
to stop *"if a different source outside this scope"* is needed. It also authorizes *"named A2h tools/tests
if needed"*.

The Session added two-line inert stubs to **three existing test files**:
toolkit `tests/apu_watch_fixture_test.c`, `tests/kernel_inplace_event_test.c`, and game
`tests/test_nv2a_hal.c`. I read all three diffs.

**Ruling: WITHIN SCOPE.** Basis:

1. **The stop clause is about diagnostic logic, not about link closure.** Its operative phrase is
   *"needed for the toolkit diagnostic to **compile**"* — read against the packet's own authorization for
   tools/tests, the natural reading is "do not put the instrument in a new place". These stubs contain no
   diagnostic logic at all: each is `void jsrf_slot_latch_install(...) { (void)a; (void)b; }` plus the same
   for `jsrf_slot_latch_sample`. They observe nothing, store nothing, and cannot change any observation.
2. **They are mechanically forced.** Those three targets link the toolkit without the game's
   `diagnostics.c`, and the bridge calls the callbacks unconditionally when armed, so the build the packet
   itself mandates cannot link without them. Stopping would have aborted an authorized change over a link
   symbol — the opposite of proportionate.
3. **They follow an existing in-file convention.** All three files already stub the sibling
   `recomp_diag_record` / `recomp_diag_thread_start` / `recomp_diag_thread_end` for exactly this reason.
   The Session extended an established pattern rather than inventing one.
4. **No guest behaviour is touched.** Test-only files; the production path is unaffected.
5. **Session flagged it, did not hide it.** The commit message for `009f624` records it under
   "SCOPE NOTE FOR ACCEPTANCE (flagged, not hidden)" and invites adjudication — compliant with §2.2.3/§2.4.1.

**Verified state of both trees:**
- Toolkit: `git status --porcelain` shows exactly four modified files — `src/kernel/kernel_bridge.c`,
  `src/kernel/recomp_diagnostics.h`, `tests/apu_watch_fixture_test.c`, `tests/kernel_inplace_event_test.c`
  — and `HEAD` is still **`c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`**, the packet's pin. No other source
  touched; nothing committed or pushed (correct: pending-acceptance state must not be pushed).
- Game: `009f624` touched `src/diagnostics.c`, `src/diagnostics.h`, `src/recomp_manual.c`,
  `tests/test_nv2a_hal.c` — all inside the packet's enumerated scope (plus the same stub question).

**Adjudication recorded:** the stubs are within scope. A future packet should not treat this as precedent
for adding *logic* outside scope; the sanctioned category is inert link-satisfaction stubs for an
authorized callback, following an existing in-file convention.

## §6.1.6 — latch authoritative, version bump: **CONFIRMED (with one gap to close)**

- **The latch is the authoritative record.** `recomp_diagnostics.h` declares the two callbacks and, under
  `#else`, reduces both to `((void)0)` — so with the gate off they vanish at compile time. The row's
  decisive negative (no first-zero transition) comes from the latch's `observed=0` field, printed as the
  terminal `[A2HSLOT] … observed=0`, not from the absence of a log line. The `[ICALL]` hook in
  `src/recomp_manual.c:41-57` is explicitly labelled corroboration and appends **two fields only**
  (live slot value, latest per-thread call index) before the **unchanged**
  `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)`. Verified in source.
- **Version bump present.** `src/diagnostics.h:28` `#define JSRF_REGISTRY_VERSION 2`;
  `src/diagnostics.c:11` `JsrfRegistry g_jsrf_debug = {JSRF_REGISTRY_VERSION};`.
- **Collector's archived `version` reads 2 in BOTH runs.** `stacks.txt` in each archive:
  `GUEST_REGISTRY address=00007FF7193D9000 version=2 claimed=5 overflow=0 qpc_frequency=10000000` (Run 1)
  and `…address=00007FF745DF9000 version=2 claimed=5 overflow=0 …` (Run 2).
  `tools/harness/collect.c:207-220` resolves `g_jsrf_debug` by symbol and reads `sizeof(*registry)`, so the
  latch is archived by existing machinery — §6.1.6's "lossless by construction".
- **Bounded by construction.** `JSRF_LATCH_CAPACITY 128`, fixed array, claimed by
  `InterlockedIncrement`, never recycled or overwritten, `slot >= capacity → overflow++` and UNKNOWN
  (`src/diagnostics.c:67-72`). `overflow=0` in both archives.

**One gap (advisory, not blocking, but it should be closed):** the `GUEST_REGISTRY` line's `claimed=5` is
the **thread registry's** claim count, not the latch's — `collect.c` prints `registry->claimed`, and
`JsrfSlotLatch.claimed` is a different field that the collector never prints. So the archived record does
not expose `latch.claimed == 0` or `latch.install_ok == 1`. The latch's contribution to this row is carried
by the terminal print's `observed=0` field, which *is* derived from the latch
(`jsrf_slot_latch_self_observed()` returns `self_slot ? 1 : 0`, `src/diagnostics.c:95-100`). That is
sufficient for this row — the latch is what decides — but a reader cannot read `install_ok` or the
zero-count back out of the archived registry, and the unlocked `shared` fields
(`install_raw/install_value/install_ok`, `sequence`) are written once at init, so no torn-publication risk
arises in practice. Recommend the successor packet add a versioned latch readout; not a defect in this
execution.

## Synthetic completion: **confirmed absent**

- `RECOMP_GPU_ACK=0` (disabled) in both; `RECOMP_APU_TRAP=1` in both; no `RECOMP_APU_DSP_ACK`,
  `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED`, `JSRF_ABI_CONTINUE` in either.
- The APU trap and `PIO_FREE`'s `0x80` were not touched; the allocation was not faked; the arena was not
  widened; the NULL call was not bypassed; the run still ends in `unhandled_exception` `0xE0424943`.
- No file under `src/recomp/gen/*.c` was modified (game commits `009f624`, `0d7bed6` touch
  `src/diagnostics.*`, `src/recomp_manual.c`, `tests/test_nv2a_hal.c`, the classifier, and the evidence
  record only).
- `PIO_FREE` remains DEFERRED; `A4b2-r7`, `A4b2-r8`, `A4b1-r4` were not reopened; `0xFFFFB3` stays
  UNRESOLVED.
- **No guest behaviour changed.** The gated code paths are compiled-in but inert without
  `JSRF_TRACE_A2H_SLOT`; Run 1 (gates absent) reproduces the same event on the same frame with the same
  ordinal sequence and the same register block, which is the live proof that the instrumentation does not
  perturb the guest.

## Frozen packet unchanged: **CONFIRMED**

`docs/packets/a2h-null-slot-triage.md`: **SHA-256 `F9A6522E8579AD756701C150A0AF60275DCFF4158705CE5331BE3BF2EA7A9F20`**,
**80 lines**, 32704 bytes — matches the hash in the evidence record, in the plan's `CURRENT PACKET` block
(`plan-jsrf-bare-minimum.md:107`) and in the packet's own header. Not modified during execution (no game
commit touches it).

## Over-reading an absence as a negative measurement: **checked — the record does NOT over-read**

This is the failure mode I looked hardest for, because the row is a negative about 15498 samples. The
evidence record states it correctly and repeatedly:

- *"Zero was unobserved **at those bridge boundaries**, not proof of never-zeroed"* (packet line 70) is
  mirrored at evidence line 138: *"Does NOT establish — and the row's own wording forbids claiming: that
  the slot was **never** zero. A transient zero and recovery between the last sample and the raw read is
  precisely what the successor packet tests."*
- The classifier's own `O-NO-BOUNDARY-TRANSITION` reason string carries the same disclaimer.
- The record does not name a writer, a guest RIP, a bridge, or a causal link to the OOM, and says so.

The absence is also **coverage-backed**, which §2.4.5 requires: every dispatch has a before-sample on every
thread, so the boundaries really were observed; the claim is "nonzero at every observed boundary", not
"never zeroed". **No over-reading found.**

## Correction banner with an unswept body: **checked — none**

The `⚠` block at evidence lines 91-119 is a *self-corrected* methodological error (the wrong completeness
test), and the body has been swept: both the classifier (`coverage()` docstring, and the fixtures) and the
record's own table state the corrected test. The rows I verified are all post-correction. There is no
banner claiming a correction whose fix was not applied.

**Sweep width:** adequate. The corrected predicate (dispatches vs before-samples, per thread) is broader
than the original (call-number pairing) and detects both real failure modes, pinned by fixtures. It is not
a narrowed sweep.

## Evidence outrunning claims

1. **`scripts/a2h-slot-triage-classify.py` is modified but uncommitted** (`index_integrity()`, fixtures
   9-11). The row was selected by the **committed** revision (8 tests) which did not test the packet's
   index clause; I supplied that test independently. Commit and re-run before stage 2 (§2.4.7).
2. **After-sample count is 7747, not 7743** in the record (total 15498 is correct).
3. **"595 nested adjacencies" and "max depth 7" are not reproduced** — I measure 5 adjacencies and max
   depth 4. The nesting mechanism is real; these two numbers are wrong.
4. **"identical numbered guest prefix" is per-thread**, not whole-log; the record should say so.
5. **`index_integrity()` is called with `budget=None`**, so the cap half of the `O-OPEN` clause is never
   evaluated against 100000.
6. **The collector does not print the latch's own `claimed`/`install_ok`** — only the thread registry's.

## Blocking findings: **NONE**

No failure mode in §3.1's sense was found. Specifically: the artifacts exist and are bound to their
archives; the commands match the packet exactly; the row is the one the packet's own row definitions
select; the install control is exact; Run 1 is a valid live inertness control with a verified identical
terminating-thread prefix; the series is complete on all six threads; the packet's per-thread index clause
is discharged by measurement; the toolkit change is inside the authorized scope with the three stubs
adjudicated in scope; the version bump is present and reads 2 in both archives; the latch is authoritative
and the log is corroboration; there is no synthetic completion; the frozen packet is unchanged; and the
negative is not over-read. The surviving items are record-accuracy and tooling-hygiene issues that do not
change the selected row.

---

## Deferred (advisory; do not reopen the frozen packet)

1. Correct `7743` → `7747` after-samples in the evidence record.
2. Correct or re-derive "595 nested adjacencies" and "max nesting depth 7" (measured: 5 and 4).
3. State explicitly that the identical-prefix comparison is per-thread on the terminating thread.
4. Wire `index_integrity(budget=…)` to the metadata budget so the cap half of the clause is exercised.
5. Add a versioned latch readout (expose `install_ok` and `latch.claimed`) in the successor packet, so the
   archived registry is independently checkable rather than only the terminal print.
6. ~~Commit the classifier's `index_integrity()` addition and re-run it before stage 2.~~ **DONE during
   this review** — the tree is clean and the committed tool (11 self-tests) selects the same row.

## UNCERTAIN / CANNOT VERIFY

- **Cross-thread interleaving in the two logs cannot be made identical**, and I did not try to. The
  per-thread comparison is the meaningful one and it is exact; I did **not** verify that the *relative
  ordering* of the six threads' events matches between runs, and no claim in the record depends on it.
- **The latch's own in-memory fields** (`install_ok`, `slots[]`, `overflow`) are **CANNOT VERIFY from the
  archived artifacts**: the collector prints only the thread registry's `version/claimed/overflow`, and
  `stacks.txt` carries no latch dump. The latch's verdict reaches the record only through the terminal
  print's `observed=0`. Sufficient for this row; not independently checkable.
- **The `→ returned` value comparison across the two logs is CANNOT VERIFY per-thread** on my method,
  because that line carries no `tid`. My 187 "differences including retv" are an artifact of attribution,
  not evidence of divergence; the dispatch tuple comparison (which is attributable) is exact.
- **Whether any additional guest thread existed that never dispatched** is CANNOT VERIFY from these
  artifacts: only threads that entered the bridge are sampled. The packet anticipates this (N+1 → UNKNOWN)
  and `latch.overflow` is not printed, so I cannot close it here. It does not affect the selected row,
  which is about observed boundaries.
