# A4b1-r4 step 8 — the AC-FIX fixture is GREEN, and its guards are can-fail

**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-26.
**Worker:** child `1fe1b4f2-6e2d-45dc-8741-1359389432c0`, `workbuddy-ai/deepseek-v4.1-flash` @ `max` (§2.2),
reported **COMPLETE** (not blocked).
**Fixture:** `xboxrecomp/tests/apu_watch_fixture_test.c`. Supersedes the review in
`a4b1-r4-fixture-review.md`, whose one open finding this closes.

## Result — verified by the Session, not taken from the report

| Check | Result |
|---|---|
| `python -X utf8 scripts\build-jsrf.py` | **succeeded** |
| `ctest --test-dir build -C Release` | **100% tests passed out of 14** — baseline 12 + both fixture registrations |
| `jsrf_apu_watch_fixture_trace_on` | **Passed** (0.37 s) |
| `jsrf_apu_watch_fixture_trace_off` | **Passed** (0.02 s) |
| direct, `RECOMP_APU_TRACE=1` | **715 checks, 0 failed** |
| direct, trace unset | **156 checks, 0 failed** |
| **clean rebuild** (`--clean-first` on `xbox_apu`, then relink) | **14/14 again** — the result does not depend on stale objects |

## The fixture is honest — re-checked after the rewrite

| Check | Result |
|---|---|
| Case functions defined **and called** | **17 of 17** |
| Skips / early returns that could hide a failure | **none** |
| Test-local copies of ported logic | **none** (the one candidate, `gp_write` at `:826`, is a three-line wrapper over `mcpx_apu_mmio_write`) |
| Production entry points driven | `mcpx_apu_mmio_write`, `mcpx_apu_dsp_frame`, `apu_gp_dma_write`, `apu_guest_dma_ptr`, `apu_watch_snapshot`, `apu_watch_reset`, `apu_watch_cpu_store`, `dsp_write_memory`, `dsp_run`, `xbox_MemoryLayoutInit`, `xbox_ContiguousAlloc`, … |

## Mutation testing — the Session proved the new guards can actually fail

**A passing test that cannot fail is not evidence.** The ruling's two can-fail twins exist to catch
specific production mistakes, so the Session **broke production deliberately and confirmed each twin
catches it**. This is the strongest available check on the fixture, and it is the check that a green
result alone cannot give.

**Mutation 1 — remove the `is_gp` gate** (`if (s->is_gp) {` → `if (1) {`):

```
FAIL: (viii) EP twin: the EP's DMA leaves every gpin.fifo count at 0 (the is_gp gate)
FAIL: 715 checks, 1 failed
```

→ **Caught, by the exact twin and the exact message the Advisor required.** The EP twin is real.

**Mutation 2 — drop the universe test** (`if (buf_id < GP_INPUT_FIFO_COUNT) {` → `if (1) {`):

```
FAIL: (x) buf_id=5 read-direction: out_of_universe == 1
FAIL: (x) buf_id=5 read-direction: GPIN_OUT_OF_UNIVERSE counted once
FAIL: (x) buf_id=5 read-direction: the latch fired
FAIL: (x) buf_id=5 read-direction: the latch records the index
FAIL: (x) buf_id=5 read-direction: the latch records kind GPIN_FIFO
FAIL: (x) buf_id=5 read-direction: fifo[5] unchanged (got reads=1 words=64)
```

→ **Caught, six ways.** The `buf_id=5` twin is real, and `fifo[5] unchanged` is precisely the
reserved-slot guard the Advisor's addendum §3 asked for.

Both mutations were reverted, the source byte-verified against `HEAD` (`git diff` empty), and the tree
returned to the committed state.

## A build trap found while reverting — recorded because it nearly produced a false result

After reverting mutation 2 the suite **still failed**, which looked like a bad revert. It was not:
`Copy-Item` **preserves the source file's original mtime**, so the restored `dsp_dma.c` (mtime
`00:40:12`) appeared **older** than the object compiled from the mutated source (mtime `00:56:36`).
**MSBuild therefore skipped recompiling it and kept the mutated object.**

The fix is to touch the source (content unchanged, so git stays clean), which forced the rebuild and
restored 14/14. **A clean `--clean-first` rebuild was then run to confirm the result does not rest on
incremental state at all.**

**This is worth remembering for any future mutation test in this tree: restoring a file by copy does not
restore its build state.** A silent stale-object reuse can make a correct revert look broken — or, worse,
make a broken tree look fixed.

## Ruling compliance, item by item

| Ruling item | Implementation |
|---|---|
| **(C)(a)** drive the pinned DMA with a read-direction descriptor, `buf_id = 0` | `dma_read_transfer()` at `:1550`; `(viii)` at `:1684`. Registers go through `write_peripheral()` (the pinned peripheral path, `dsp.c:107-118` → `dsp_dma_write`); the descriptor is the 7-word block `dsp_dma_run()` reads from DSP X memory (`dsp_dma.c:137-144`). Asserts `fifo[0].reads == 1`, `words == 0x40`, `out_of_universe == 0`, and the `"Unhandled DSP DMA buffer: 0x0"` line present |
| **(C)(b)** keep the `gp_fifo_rw` hook | untouched, as ruled |
| **(C)(c)** per-FIFO classification of all 6 indices | written into `apu_watch.h` beside the universe definition, including the stated **inference** |
| **(C)(d)** the two can-fail twins | EP twin at `:1746`; `buf_id = 5` twin folded into `(x)` at `:1888` — **both mutation-proven above** |
| **(C)(d)** reserved slots 2..5 stay 0 | asserted at `:1721` |
| **(C)(e)** line-vs-snapshot covers **both** mirrored slots | `(vi)` printed `boot_scratch_read == class latch` at `:1394`; `(viii)` printed `mixbuf_stub_read == MIXBUF_STUB_READ` class latch at `:1610-1618`; `check_gpin_summary` compares both slots at `:801`, `:806` |
| **(C)(d)** remove the obsolete reachability assertion | deleted |

## Worker decisions reviewed

| Decision | Session assessment |
|---|---|
| `buf_id = 5` twin folded into `(x)`, in its own reset+snapshot block | **Correct** — the ruling permits folding, and its own block keeps it independently decisive |
| Asserts `latch.observed == APU_WATCH_GPIN_FIFO` on the twin | **Correct** — matches the hook's own call |
| EP twin deliberately **skips** `check_gpin_summary` | **Correct and well-reasoned.** `emit_now()` is `apu_watch_gp_bootstrap_done(1)`, so asserting GP summary lines while driving the *EP's* DMA would assert an unrelated emission. The snapshot counts are the decisive check |
| Precedes START with `DMA_CONTROL_ACTION_STOP` | **Correct** — `dsp_reset` does not clear `dsp->dma`, so a prior case could leave RUNNING set |
| **INFERRED:** that `buf_id = 5` landing in the reserved output range is the mistake the reserved-slot guard exists to catch | **Accepted as stated.** The ruling fixes slots 2..5 as output places and requires the twin but does not spell out this interaction; the Worker connected them and marked it INFERRED. Mutation 2 confirms the guard does catch it |

## Advisory — the fixture leaves a stderr file in its working directory

The fixture captures its own `stderr` by `freopen`-ing to **`apu_watch_fixture_stderr.txt`** in the
**current working directory**, which the packet's step 8 requires (*"It captures its own `stderr`, for
example by redirecting it to a file that it then reads"*).

- Under `ctest` the CWD is the build directory, so the file lands in `build/` — harmless, and gitignored
  by location.
- Run **manually from the game root** it lands in the game root as an **untracked** file. The Session's
  own positive-control run did exactly that; the stray file was removed.

**Advisory, not a defect:** the criterion does not say where the capture goes, and the file is a
legitimate part of the mechanism. Recorded so a future reader does not mistake it for a stray artifact,
and so a future revision can direct it to a temp path if the litter matters. **No effect on the
disposition.**

## The Worker's one open item was already closed

The Worker reported that `docs/reviews/a4b-xemu-pin.md` still needed the `dsp_dma.c` read-arm entry. **It
was already recorded** — `dsp_dma.h` and `dsp_dma.c` rows at `:110-111` and the ruling (C) narrative at
`:117`, added when the Session recorded the ruling. The Worker could not see policy edits outside its
write scope, so it flagged a real obligation and correctly left it to the Session.
