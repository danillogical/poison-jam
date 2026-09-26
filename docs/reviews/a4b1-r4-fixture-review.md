# A4b1-r4 step 8 — AC-FIX fixture: Session review and the one open finding

**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Worker:** child `7e467e6c-1c5b-4973-ac54-f519ae29aa8d`, `workbuddy-ai/deepseek-v4.1-flash` @ `max` (§2.2).
**Artifacts:** `xboxrecomp/tests/apu_watch_fixture_test.c` (**1876** lines, new); game `CMakeLists.txt`
(two `add_test` registrations only). Scripts: `logs/a4b1/review-fixture.py`,
`check-fifo-reachability.py`.

## The fixture is honest — checked, not assumed

The packet's central requirement is that the fixture drives **production** code and never a test-local
copy. Verified from the source:

| Check | Result |
|---|---|
| Production entry points called | `mcpx_apu_mmio_write` (3), `mcpx_apu_dsp_frame` (4), `apu_gp_dma_write` (2), `apu_guest_dma_ptr` (2), `apu_watch_snapshot` (23), `apu_watch_reset` (22), `apu_watch_cpu_store` (4), `apu_watch_set_anchor_site`, `apu_gpin_fifo_read`, `apu_watch_boot_scratch_read`, `apu_watch_gp_bootstrap_done` (2), `xbox_MemoryLayoutInit` (2), `xbox_ContiguousAlloc` (2), `mcpx_apu_dsp_init`, `dsp_run` |
| Test-local copies of ported logic | **none.** The one candidate, `gp_write` at `:806`, is a **three-line wrapper** over `mcpx_apu_mmio_write(d, APU_GP_BASE + off, val, 4)` — the real dispatch, not a reimplementation |
| Case functions defined **and called** | **17 of 17** — `case_a, b, c, d, e, i, ii, iii, iv, v, vi, vii_placement, vii, viii, ix, x, xi` |
| Skips / early returns that could hide a failure | **none** — no `skip`/`TODO`/`FIXME`/`unsupported` anywhere |
| The `check()` helper | counts failures and prints `FAIL: <name>`; `main` returns nonzero |

**It also does not weaken anything to get green.** The Worker left both failing assertions in place and
reported them rather than deleting them — which is the behaviour the contract asks for.

## Build and test

`python -X utf8 scripts\build-jsrf.py` → **succeeded**. `ctest` → **14 tests** (baseline 12 + the two
fixture registrations), **12 passed**.

## Finding 1 — the `gpin` summary slots were never written: **FIXED** (commit `2333b6e`)

`apu_watch.h` declares `gpin.mixbuf_stub_read` and `gpin.boot_scratch_read`, and `emit_gpin_block`
prints **both** (`apu_watch.c:895-898`), but neither `gpin` slot was ever written: both classes fired
only their **class** latch (`s.latch[...]`). So the `[GPWATCH] latch class=BOOT_SCRATCH_READ` line
printed while the summary's presence witness stayed **0** forever — and **AC-FIX (vi)** reads that
printed witness.

**Repair:** both class latches are now mirrored into their `gpin` slots under the **same write-once
guard** that fires them (`apu_watch.c:566`, `:641`), so the two views freeze together and the `at_clear`
copy — a `memcpy` of the whole `gpin` struct — stays consistent with the class latches taken alongside.

**Measured effect:** the `(vi)` failures cleared. **628 checks, 2 failed → 1 failed.**

This is a defect in **my own step-4/5 implementation**, not a packet defect: `DS6` says the summary
"carries every fixed-array element … and the `at_clear` block", and it is the summary's own field that
was unreachable. **No revision ground applies** — the packet text needed no change, so this was settled
by repair, not by a `§5.4` reopening.

## Finding 2 — `FIFO_READ` is unreachable through any production path: **OPEN, Advisor consulted**

The fixture's one remaining failure is an assertion of **reachability**, and the Worker was right to
assert it rather than fake the counter. Measured from the pinned source:

| Fact | Source |
|---|---|
| The hook records only `dir == 0` | `gp_ep.c:251` — `if (!dir) { apu_gpin_fifo_read(index, len); }` |
| In `gp_fifo_rw`, `dir == 0` selects the **GP INPUT** fifo registers (`GPIFBASE/END/CUR`, `assert(index < GP_INPUT_FIFO_COUNT)` = 2); `dir == 1` selects the **OUTPUT** registers (4). Upstream's commented debug line reads `dir ? "writing to" : "reading from"` | `gp_ep.c:219-235` |
| **The polarity is therefore correct** — `dir == 0` really is the GP reading an input fifo | — |
| The **only** caller of the `fifo_rw` callback passes a hardcoded **`1`** | `dsp_dma.c:267`, inside the `if (direction)` arm |
| The pinned **read** arm handles only `buf_id` `0xe`/`0xf` and **asserts** on anything else, under the upstream comment *"FIXME: Move to function; then reuse for both directions"* | `dsp_dma.c:280-290` |
| `read_peripheral` models `0xFFFFB3`, `0xFFFFC5`, `0xFFFFD4-D7` and nothing FIFO-related, so a DSP program cannot read an input fifo through peripheral space either | `dsp.c:52-91` |

**Conclusion: in the pinned core as ported, no production path reaches `gp_fifo_rw` with `dir == 0`.**
The hook can never fire, so `FIFO_READ` is structurally 0 — and `AC-FIX (viii)` (packet line 327)
requires the fixture to cause one **through the production code paths**, with the parenthetical
alternative ("or a small DSP program in the bootstrap image run for one frame") not helping, because
that route does not exist either.

**This is a `PREMISE_CHANGED` candidate, not a bug**, so it was **not** settled by editing code. Two
candidate resolutions were put to the Advisor:

- **(A) interpretation** — the array is sized `6 = 2 + 4` deliberately, so the class means FIFO
  **traffic** per FIFO; drop the `if (!dir)` guard so the four output fifos populate. Cost: the class is
  named `FIFO_READ`, its field is `reads`, and `AC-INPUTS` would need to say which of the 6 are inputs.
- **(B) `§5.4(2)` PREMISE_CHANGED** — the premise that a FIFO read is reachable is invalidated by the
  pinned source; the hook is correct as written and `FIFO_READ` is legitimately 0, so either the
  criterion is revised or the unreachability is recorded as a claim limit and the case asserts the
  structural fact instead of a read.

**The Advisor's ruling is pending.** Per `§5.4`, if it is `PREMISE_CHANGED` the revision goes to a fresh
Planner; if it is an interpretation ruling, execution follows it. **The fixture is not committed while
this is open** — the failing assertion stays visible rather than being silenced.

## Worker decisions reviewed and accepted

| Decision | Session assessment |
|---|---|
| `(vi)` bounded with `d->gp.realtime = false` | **Correct.** `realtime` is the production field `mcpx_apu_update_dsp_preference` writes from `g_config.audio.use_dsp` (`gp_ep.c:36-47`), and the loop exits on `!realtime` (`:627`). So `(vi)` **runs** and the packet's `UNKNOWN` clause (line 336) is **not** taken — which is what the Session asked for after measuring both escape paths. |
| Asserts `gp_insns > 0`, which packet line 337 declines to assert | **Accepted, flagged.** It is the fixture's own strength check, not a packet requirement; it holds because the bootstrap image is all-`nop` (2 cycles each) so the core retires 500 instructions in one 1000-cycle chunk. Recorded so no row mistakes it for a criterion. |
| Line-vs-snapshot applied to the **trace-on** arm only | **Correct** — packet line 297 says so. |
| Calls the exported `apu_watch_gp_bootstrap_done(1)` before snapshotting | **Correct** — it is the production emitter and changes no counter (`apu_watch.c:682-689`); no test-local emitter. |
| `qemu_mutex_init`/`qemu_cond_init` rather than bare `calloc` | **Correct** — `calloc` leaves an invalid `CRITICAL_SECTION` and `gp_write` faults. |
| Real `xbox_MemoryLayoutInit` on a synthetic XBE; `xbox_ContiguousAlloc(0x400000)` so `DS3` case 2 is reachable | **Correct** — this is what makes the inverse's second case testable at all. |
