## A4b1 — the pinned GP DSP56300 core, and its watched-word ledger, are ported, licensed and fixture-tested, and the default strict path is unchanged

**Class:** change   **Contract revision:** `A4b1-r3`   **Status:** draft
**r3 delta (not a redesign; `docs/reviews/a4b1-a4b2-r2-adequacy-review.md`):** DS6 defines every latch field; `AC-FIX` asserts each latch line and the counts line equal the snapshot; `[GPIN]` becomes lossless ledger accounting (bounded table + uncapped overflow counter + write-once `GPIN_OVERFLOW` latch), cleared by `apu_watch_reset()`, with an `AC-FIX` case per kind and for overflow; the `[GPDMA] watch` count is reset too; advisory D1 (DS5 CAS race) taken; D5–D7 wording fixed.
**Split decision:** `A4b` is split into `A4b1` (port, ledger, licence, fixtures, unchanged default path; no guest-run claim) and `A4b2` (the strict trap+trace run: boot, run, clear, no-CPU, inputs), because build, ctest and one default run settle the port's scale risk, so it should not wait for the run criteria or be reviewed with them.
**Governing requirement:**
- `A4a-r2` row `O-6` (`docs/packets/a4a-dsp-pending-word.md`);
- the Q1 ruling (`docs/reviews/a4b-q1-advisor-ruling.md`);
- the owner Q2 decision (`docs/reviews/a4b-q2-owner-decision.md`);
- the checkpoint-40 constraints (`docs/reviews/a4b-planning-rulings.md`);
- **the watch-ledger ruling (`docs/reviews/a4b-watch-ledger-ruling.md`), the authority for Device semantics 6 and 7.**

**Depends on:** `A4a-r2` (ACCEPTED); `A3a-r25` (ACCEPTED); `A4p-r1` (ACCEPTED, `O-GATE`); **`A4s` (toolkit sync) ACCEPTED, with its strict baseline still stopping at the `loc_001A18D0` spin.**
**Baseline:** game and toolkit at `A4s`'s accepted commits, plus the game commit that adds this packet. Both trees are clean. At promotion the Session records the SHAs, the baseline exe SHA-256 and the baseline ctest count.
**Pin record:** `docs/reviews/a4b-xemu-pin.md`. It records the xemu commit `67cc79e663038d1f55448c0f566b37dde016adf6`, the per-file SHA-256 of the upstream LF bytes, and each file's licence as taken from its own header.
**Revision log:** `docs/reviews/a4b-revision-history.md` (non-authoritative).

### Motivating evidence

- **A4a R1** — `logs/runs/20260924-191833-331-a4a-r2-trap-trace` (STRICT, trap+trace, toolkit `0d7929c`):
  - The guest writes GPSADDR `0x02040 = 803CC000`, GPSMAXSGE `0x020D4 = 8`, and GPRST `0x3FFFC = 1`, then `= 3`.
  - There are zero GP/EP reads.
  - The run ends in `diagnostic_deadline` at the `loc_001A18D0` spin.
  - The guest's store of `3` comes long after GPRST=3: frames open at L3286, and the store is first traced at L5226. So GP write-backs can reach `+0x810` while it still holds 0 (`docs/reviews/a4b1-a4b2-adequacy-review.md`, A4b1 B3).
- **A4a R0** — `logs/runs/20260924-191906-091-a4a-r2-default` (STRICT, default): the same stop, and zero `[APUMMIO]` lines.
- **Toolkit at `0d7929c`.** `A4s` changes `apu_dsp.c` and `apu/CMakeLists.txt`, so Readiness re-checks these facts at the baseline:
  - `apu_dsp.c` has no DSP core, and it carries the synthetic `dsp_ack_frame`.
  - GP/EP writes are dropped and GP/EP reads return 0 (`apu_core.c:618, 637`).
  - `apu_state.h` carries stale `DSPState` layouts.
  - Guest MMIO reaches the APU only under `RECOMP_APU_TRAP` (`xbox_memory_layout.c:1728`).
- **Address model.** The `0x80000000` window is separate storage, not an alias of low RAM (`xbox_memory_layout.c:1539-1543`). `MmGetPhysicalAddress` returns the VA (`kernel_bridge.c:1700-1707`). So xemu's `addr & 0x03FFFFFF` reads the wrong bytes.
- **Threads.** The GP runs on the APU frame thread (`apu_core.c:557`), and guest stores come from guest threads (watch-ledger ruling).

### Claim and boundaries

- **Establishes (on R1-PASS only):**
  - The pinned xemu GP core (interpreter only), GP MMIO routing and one GP DMA write choke point are in the toolkit, with per-file provenance.
  - The synthetic DSP acknowledgement is gone from both source and exe.
  - A thread-safe watched-word ledger is exported. A fixture exercises its classes, latches, counters and emission through a snapshot accessor.
  - The licence of the combined work is recorded.
  - The tree builds, and every ctest passes.
  - One STRICT default run matches the `A4s` baseline stop.
- **Does not establish:**
  - Anything about the title's GP behaviour. No trapped guest run is made; that is `A4b2`.
  - DSP56300 instruction-level correctness. There is no independent opcode oracle; the core is trusted because it comes from the pin.
  - Anything about the title from the fixture, which is a positive control only.
  - General race freedom.
- **Non-goals:**
  - `dsp_jit.*`;
  - EP routing (EP stays unrouted);
  - GP→CPU interrupts;
  - a `PIO_FREE` model;
  - audio;
  - any game `src/` edit;
  - regeneration;
  - edits to `docs/jsrf-run-profiles.md`.

### Device semantics and runtime adaptations (operative)

1. **GPRST write: bootstrap and reset,** following the pinned `proc_rst_write` (quoted in the pin record).
   - **Reset** when either `GPRST|GPDSPRST` bit is clear in the new value.
   - **Bootstrap** when either bit was clear in the old value and both are set in the new value. The bootstrap is synchronous, inside the handling of that write.
   - The GP executes only in frames.
2. **GP per frame,** following the pinned `mcpx_apu_dsp_frame`: `dsp_start_frame`, then `dsp_run` in chunks until halt is requested.
   - `dsp_c_init` is called unconditionally.
   - No new environment variable is added.
   - The EP keeps the existing mixbin passthrough.
3. **One translation function:** `apu_guest_dma_ptr(uint32_t addr, uint32_t len)`, or another name, but exactly one.
   - Its comment names it the inverse of `bridge_MmGetPhysicalAddress`.
   - It returns a host pointer only when the whole range lies in a mapped guest region.
   - Otherwise it logs `[GPDMA] unmapped addr=%08X len=%X` **unconditionally** (not trace-gated), once per address, and returns NULL. The caller then makes no access.
   - It does not use `& 0x03FFFFFF`; a comment explains why.
   - VP code is untouched.
4. **Synthetic ack removed.** `dsp_ack_frame`, `dsp_ack_init` and every read of `RECOMP_APU_DSP_ACK` are deleted.
5. **One GP write choke point, with an atomic store.**
   - Every GP DMA write to guest memory, from every pinned write callback, passes through **one** function.
   - That function reads `W_va = MEM32(0x001BA858)+0x810` at write time.
   - The choke point receives the guest destination start, the length, and the DSP-side address of the transfer's first word.
   - If the write covers the aligned dword `W_va` and the payload for that dword is `0`, that dword is **never** written with an ordinary store. Instead (the rest of the transfer is written as usual):
     1. `o = InterlockedCompareExchange(host(W_va), 0, 3)`. If `o = 3` the exchange succeeded; classify on `observed=3`.
     2. If `o = 0`, the dword already holds the payload: **skip the write**; classify on `observed=0`.
     3. Otherwise `o2 = InterlockedCompareExchange(host(W_va), 0, o)`. If `o2 = o` it succeeded; classify on `observed=o`. Else set `o = o2` and repeat from 1.
   - This closes advisory D1 (a guest store of `3` landing between a failed exchange and an ordinary write of `0` can no longer be overwritten unclassified): every `0` the GP lands on `W_va` is landed by an exchange whose replaced value is the classification.
   - Guest memory ends up identical to a plain write. This is always on.
6. **Watched-word ledger (the decision record).** It is always computed: a few atomics per event, with no effect visible to the guest. It lives in a toolkit struct, written only by the choke point (5) and by `apu_watch_cpu_store`.
   - **Shared state**
     - `seq` is a 32-bit counter, incremented with `InterlockedIncrement` once per ledger event.
     - Each class has one **uncapped** counter.
     - Each latched class has one **write-once latch** (`CPU_OTHER` has none). The thread that wins `InterlockedCompareExchange` on the latch's `latched` word fills in the fields below, then emits the latch line.
     - **Latch fields (every one defined; the latch line prints exactly these values):**
       - `seq`: the value **that event's own** `InterlockedIncrement(&seq)` returned, taken once per event before classification. It is never re-read when the latch is filled or the line printed.
       - `va`: for GP classes, **`W_va` as read by the choke point at that event** (never the DMA destination start); for CPU classes, the call's `target_va` (which equals `W_va`, since other targets are ignored).
       - `observed`: GP — the value the classifying exchange replaced (DS5), or for `GP_NONZERO_OVER`/`GP_PARTIAL` the dword at `W_va` read immediately before the write; CPU — `0` (the store has not happened yet: record-before-store).
       - `payload`: GP — the dword the transfer writes at `W_va` (for `GP_PARTIAL`, the dword `W_va` would hold after the write); CPU — the call's `value`.
       - `site`: CPU — the call's `site_va`; GP — `0`.
       - `frame`: the global `se_frame` count at the event.
       - `insns`: GP — `gp_insns` at the event; CPU — `0`.
       - `dsp_addr`: GP — the DSP-side address (24-bit, in the DSP memory space the DMA reads from) of the word that lands on the dword at `W_va`, or for `GP_PARTIAL` of the first overlapping word; CPU — `0`.
   - **GP classes,** classified at the choke point, from the `observed` of the exchange that landed the zero (DS5):
     - `GP_CLEAR`: zero payload, `observed=3`.
     - `GP_ZERO_OVER_ZERO`: zero payload, `observed=0` (write skipped). It is counted and latched, but never decides anything.
     - `GP_ZERO_OVER_OTHER`: zero payload, `observed ∉ {0,3}`.
     - `GP_NONZERO_OVER`: a non-zero payload covering the dword; `observed` is the value before the write.
     - `GP_PARTIAL`: the write overlaps `W_va`'s 4 bytes without covering the aligned dword.
   - **CPU classes,** recorded through the exported `void apu_watch_cpu_store(uint32_t site_va, uint32_t target_va, uint32_t value)`.
     - The function ignores any call with `target_va != MEM32(0x001BA858)+0x810`.
     - The anchor site is set through the exported `void apu_watch_set_anchor_site(uint32_t site_va)`; the toolkit hard-codes no site.
     - `CPU_ANCHOR`: a store of `3` from the anchor site.
     - `CPU_ZERO`: a store of `0`, latched **per site** in a 16-entry site table.
     - `CPU_ZERO_OVERFLOW`: a store of `0` from a 17th distinct site. It is latched.
     - `CPU_OTHER`: any other store. It is counted only.
   - **Run counters** (uncapped):
     - `boots`: the total number of bootstraps;
     - `gp_frames`: GP frame-function calls since the latest bootstrap;
     - `gp_insns`: instructions the core retired since the latest bootstrap.
   - **GP input accounting (`[GPIN]`), lossless by construction.** This is the watched-word rule applied to inputs: no first-N or first-per-key record decides anything unless its loss is itself counted.
     - Every GP input hook calls one exported recording function, `apu_watch_gp_input(kind, addr, value)`, with `kind ∈ {PERIPH, DMA_READ, FIFO_READ, MIXBUF}`:
       - `PERIPH`: a core read of a GP peripheral register; `addr` = the DSP peripheral address.
       - `DMA_READ`: a GP DMA read of guest memory, including the bootstrap's; `addr` = the guest VA of each 4 KiB page read (`va & ~0xFFF`); `value` = the first dword read from that page.
       - `FIFO_READ`: a GP read of a FIFO; `addr` = the FIFO's register/index as the pinned source names it.
       - `MIXBUF`: a GP read of the VP mix buffer; `addr` = the DSP address; the entry also records `vp_active_voices` at the read.
     - Recording is always computed (so both registrations can check it) and stops once `GP_CLEAR` is latched; the cut-off is **derived from the `GP_CLEAR` latch**, so `apu_watch_reset()` clears it.
     - A **fixed 256-entry table** of distinct `(kind, addr)` keys holds, per entry: `kind`, `addr`, `first` (value at first read), `frame`, `seq` (that read's own `InterlockedIncrement` value) and, for `MIXBUF`, `vp_active_voices`. The counter `gpin` (uncapped) counts inserted entries.
     - A read of a key **not** in the full table increments the **uncapped** counter `GPIN_OVERFLOW` and, the first time, fires the write-once **`GPIN_OVERFLOW` latch** (fields as DS6 latches: `seq`, `frame`, `va` = `addr`, `observed` = `value`, `payload` = kind index 0–3, `site=0`, `insns` = `gp_insns`, `dsp_addr=0`). No key is ever dropped silently: each distinct key read before the cut-off is either an entry or counted in `GPIN_OVERFLOW`.
   - **Accessors,** exported for fixtures:
     - `apu_watch_snapshot(struct apu_watch_snapshot *)` copies every counter, latch, CPU-site record, run counter, the `[GPIN]` table, `gpin` and `GPIN_OVERFLOW`.
     - `apu_watch_reset()` returns all of them to their initial state. It **also** clears the `[GPIN]` table (and hence its cut-off, which follows `GP_CLEAR`), the process-scoped `[GPDMA] watch` line count (DS7), and the `[GPDMA] unmapped` once-per-address set. After it, no trace or ledger state from an earlier case survives. (GP core state is not reset by it; see follow-up D4.)
   - **Emission,** only under `RECOMP_APU_TRACE` (read once and cached). Every line is `fflush`ed.
     - `[GPWATCH] latch class=<C> seq=%u va=%08X observed=%08X payload=%08X site=%08X frame=%u insns=%llu dsp_addr=%06X` is emitted exactly once, when a latch fires, with the latch's own field values. The number of latches is fixed (16 `CPU_ZERO` site latches, plus one each for `CPU_ANCHOR`, `CPU_ZERO_OVERFLOW`, the five GP classes and `GPIN_OVERFLOW`; `CPU_OTHER` is never latched), so this line has no cap and none can be lost.
     - `[GPIN] kind=<PERIPH|DMA_READ|FIFO_READ|MIXBUF> seq=%u addr=%08X first=%08X frame=%u` (plus `vp_active_voices=%u` for `MIXBUF`) is emitted exactly once per table insertion, with the entry's values. It is lossless because every non-inserted key is counted in `GPIN_OVERFLOW`.
     - `[GPWATCH] counts seq=%u boots=%u gp_frames=%u gp_insns=%llu GP_CLEAR=%u GP_ZERO_OVER_ZERO=%u GP_ZERO_OVER_OTHER=%u GP_NONZERO_OVER=%u GP_PARTIAL=%u CPU_ANCHOR=%u CPU_ZERO=%u CPU_ZERO_OVERFLOW=%u CPU_OTHER=%u gpin=%u GPIN_OVERFLOW=%u frame=%u` is emitted:
       - at each bootstrap, after the run counters reset;
       - at the first GP frame after each bootstrap;
       - immediately after any latch fires;
       - every 256th `se_frame`.
7. **Trace (observation only).** Only under `RECOMP_APU_TRACE`. Every line is `fflush`ed, and none changes any value, store or control flow. **No row in `A4b1` or `A4b2` depends on whether a capped or sampled line is present or absent.**
   - `[GPBOOT] n=%u sge0=%08X gprst=%08X prev=%08X` is emitted **inside** the handling of the GPRST write that bootstraps.
     - `gprst` is the value written; `prev` is the value before it.
     - It is followed by the first `0x200` PRAM words, as `[GPBOOT] pram %03X: w0 … w7`: 24-bit hex, 8 words per line, 64 lines.
     - The existing hook prints the `[APUMMIO]` line for that write afterwards.
   - `[GPRUN] frame=%u se_frame_after_boot=%u cycles=%u insns=%llu pc=%06X halt=%d tone=%d` is one line carrying all fields.
     - It is emitted for the first 8 GP frames after each bootstrap, then every 256th frame.
     - `se_frame_after_boot` is equal to `gp_frames`.
   - `[GPDMA] watch …` is emitted per covering write, for the first 16 since the latest `apu_watch_reset()` (process start in a run). It is observation only, with no exemption rule and no cap line.
   - `[GPDMA] frame=%u reads=%u writes=%u rbytes=%llu wbytes=%llu` is emitted every 256th frame.
   - `[GPIN]` is **not** a trace line of this section: it is the ledger's lossless input accounting, defined and emitted under Device semantics 6. A reader decides from `[GPIN]` lines only together with the counts line's `gpin` and `GPIN_OVERFLOW`.

### Readiness

- **Tools:** `python -X utf8 scripts\build-jsrf.py`; `ctest --test-dir build -C Release --output-on-failure`; `scripts\run-jsrf.py … --profile strict`; `scripts\check-run-profile.py`; `scripts\check-dump-mapping.py`; `scripts\inspect-jsrf.py memory|disasm`. All are verified by `A4s` at the baseline.
- **Prerequisites:** the pin record's files at the pinned SHA. The emulated disk images must be accessible, so the run is non-confined.
- **Stop if** any of these holds (select no row, except where a bullet names a row explicitly):
  - `A4s` is not accepted, or its strict baseline stop is not the `loc_001A18D0` spin.
  - At the baseline, `dsp_ack_frame`/`RECOMP_APU_DSP_ACK` is absent from `src/apu`, or GP writes are no longer dropped. The premise has changed → Planner.
  - Any of `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED` or `JSRF_ABI_CONTINUE` is in a launch environment.
  - The runner refuses `--profile strict`.
  - The port needs a change outside the write scope.
  - A GP write path to guest memory cannot be routed through the choke point (5) → Advisor.
  - The same build or ctest root cause survives two attempts: this is the one bullet that selects a row, R1-PORT-FAIL.
  - Any code path would let the APU write guest memory other than through the choke point, `FEMEMDATA` or VP.

### Execution

**Steps**

1. **Vendor.**
   - First commit toolkit `src/apu/dsp/.gitattributes`, containing `* -text`.
   - Then make the **vendor commit**, containing only unmodified files, copied byte-for-byte as listed in the pin record: `dsp.c`, `dsp.h`, `dsp_c.c`, `dsp_internal.h`, `debug.h`, `interp/dsp_cpu.c`, `interp/dsp_cpu.h`, `interp/dsp_cpu_regs.h`, `interp/dsp_emu.c.inc`, `gp_ep.c`, `gp_ep.h`, `dsp_dma.c`, `dsp_dma.h`, `dsp_dma_regs.h` and `trace.h`.
   - Add `interp/dsp_dis.c.inc` and `interp/debug.c` only if they are needed to compile. Exclude `dsp_jit.*`.
   - Local modifications go in later commits, and each one is listed in `a4b-xemu-pin.md`.
2. **One layout.**
   - The pinned headers replace the stale state definitions in `apu_state.h`.
   - The `pram_opcache` reset moves to the pinned API.
   - Shims go in `apu_shim.h` and are only no-ops or pass-throughs.
3. **Route GP MMIO.** Route `0x30000..0x3FFFF` to the pinned `gp_read`/`gp_write`, from both `mcpx_apu_dispatch_mmio` and `mcpx_apu_mmio_read_quiet`.
   - EP stays unrouted.
   - The existing read note keeps firing for EP and for unmodelled GP offsets.
4. **DMA and the choke point** (Device semantics 3 and 5). The ledger, its exports and its accessors (Device semantics 6) go in an exported toolkit header, for example `src/apu/apu_watch.h`.
5. **Frames** (Device semantics 2) and removal of the synthetic ack (Device semantics 4).
6. **Trace** (Device semantics 7).
7. **Licence bookkeeping** (toolkit).
   - Add a verbatim `LICENSES/GPL-2.0.txt`, from `https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt`.
   - In `NOTICE`, list each ported file under its own header's licence, with its copyright lines and the pinned SHA:
     - GPL-2.0-or-later: `dsp.c`, `dsp.h`, `dsp_c.c`, `dsp_internal.h`, `debug.h`, `interp/*`;
     - LGPL-2.1-or-later: `gp_ep.*`, `dsp_dma*`.
   - `NOTICE` must also state that a binary linking `xbox_apu` is a GPL-2.0-or-later combined work, and its MIT paragraph must be amended to match.
   - `LICENSES/README.md` names both texts.
8. **Fixture (AC-FIX).**
   - It is registered in ctest twice: once with `RECOMP_APU_TRACE=1` through the test's `ENVIRONMENT` property, and once without.
   - It drives the **production** GPRST write handler, frame function and DMA choke point. It never uses test-local copies of them.
   - It maps guest memory for low RAM (below `0x04000000`) and for the `0x80000000` window.
   - It seeds `MEM32(0x001BA858)` so that `W_va` falls in mapped memory.
   - It captures its own `stderr`, for example by redirecting it to a file that it then reads.
   - **Every case begins with `apu_watch_reset()` and decides on `apu_watch_snapshot()`, so no case depends on the order of other cases or on cumulative counts.** Because the reset also clears the `[GPIN]` table, its cut-off, the `[GPDMA] watch` count and the unmapped set (DS6), this now holds for trace state too. Where a case checks log text, it reads only the output emitted during that case. Cases that bootstrap or run a frame first reset GP state the way the production reset path does (GPRST with a bit clear).
9. **Record.**
   - Run `ctest --test-dir build -C Release --output-on-failure > logs\a4b1-ctest.txt`.
   - Then run R0 and copy `a4b1-ctest.txt` into R0's run directory.
   - Record everything in `docs/reviews/a4b1-execution-evidence.md`.

**Run** (from the game root, 30 s, archived under `logs/runs/`):

```powershell
Remove-Item Env:\RECOMP_APU_DSP_ACK,Env:\RECOMP_AC97_READY,Env:\JSRF_ALLOW_UNRESOLVED,Env:\JSRF_ABI_CONTINUE,Env:\RECOMP_APU_TRAP,Env:\RECOMP_APU_TRACE -ErrorAction SilentlyContinue
$env:RECOMP_GPU_ACK='0'; $env:RECOMP_KERNEL_LOG_BUDGET='100000'
python -X utf8 scripts\run-jsrf.py --seconds 30 --profile strict --label a4b1-default   # R0
```

- **Write scope:**
  - toolkit: `src/apu/**` (including `src/apu/dsp/.gitattributes` and **the exported ledger header**), `NOTICE`, `LICENSES/**`, and `tests/` and `CMakeLists.txt` for the fixture only;
  - game: `CMakeLists.txt` and `tests/`, only if the fixture is hosted there;
  - docs: `docs/reviews/a4b1-*.md`, and the local-modification list in `docs/reviews/a4b-xemu-pin.md`.

  Nothing else is in scope; **there is no game `src/` edit.**
- **Build/run owner:** the Session.

### Gates

Gates are checked before any AC. A failing gate stops evaluation and selects R1-UNKNOWN.

- **G1:** `check-run-profile.py` prints `STRICT` for R0.
- **G2:** `check-dump-mapping.py` reports `matches ≥ 1` and `content-mismatch: 0`.
- **G3:** `result.json` is readable.
- **G4:** in `src/recomp/gen/recomp_0005.c` at the build commit, `loc_001A18D0: ;` occurs exactly once, at line `L`, and a line in `L+1..L+3` contains `goto loc_001A18D0;`.

`F` is the count of matches in `stacks.txt` of `sub_001A1769\+0x[0-9A-Fa-f]+ .*recomp_0005\.c:(L|L+1|L+2|L+3)\b`, with `L` taken from G4.

### AC-PORT — pinned core ported with recorded provenance; tree builds and tests green

- **Mandatory:** yes.
- **Guards against:** an unpinned or modified core; a second GP write path; a broken build or broken tests; a surviving synthetic ack.
- **Evidence profile:** structural.
- **Procedure:**
  1. Check that `a4b-xemu-pin.md` is complete: SHA, per-file SHA-256, licence, and the local-modification list.
  2. Run `git -C <toolkit> ls-files --eol src/apu/dsp`. It must show `attr/-text` for every vendored file.
  3. Hash each vendored file **byte-exact** at the vendor commit. Either pipe `git -C <toolkit> cat-file blob <vendor-commit>:src/apu/dsp/<f>` to a binary file (for example `cmd /c "git … > tmp"`) and run `Get-FileHash`, or hash a clean checkout of that commit under the `-text` rule.
  4. The Session records the choke-point function and every pinned write callback that calls it, with source lines; and every GP input path (peripheral read, DMA read, FIFO read, mix-buffer read) with the line where it calls `apu_watch_gp_input`.
  5. Run `python -X utf8 scripts\build-jsrf.py`, then ctest.
  6. Run `Select-String -Path <toolkit>\src\apu -Pattern 'RECOMP_APU_DSP_ACK|dsp_ack_' -Recurse`, and search the strings of `build\Release\jsrf_recomp.exe` for `RECOMP_APU_DSP_ACK`.
- **Oracle:** the upstream LF hashes at the pinned SHA.
- **PASS:** all of the following:
  - the record is complete;
  - every vendored file shows `-text`, and every hash matches;
  - there is exactly one choke point, and every write callback reaches it;
  - the build succeeds;
  - every ctest passes: the baseline count plus both fixture registrations;
  - both searches return zero hits.
- **FAIL:** any of the above is not met.
- **UNKNOWN:** a record field is missing.
- **Controls:** the `A4s` baseline exe is the known-good for the string search, since the string is present in it.
- **Claim limits:** structural only.

### AC-LIC — the combined work's licensing is recorded

- **Mandatory:** yes, while the ported code is in the tree.
- **Guards against:** a GPL-2.0-or-later combined work described as MIT-only, or a file listed under the wrong licence.
- **Evidence profile:** manual/structural.
- **Procedure:**
  - Run `Get-FileHash LICENSES\GPL-2.0.txt` and compare it with the gnu.org text.
  - Read `NOTICE` and `LICENSES/README.md` against the pin record's licence column.
- **PASS:** all of the following:
  - the GPL text is verbatim;
  - `NOTICE` lists each ported file under its header's licence, with its copyright lines and the pinned SHA;
  - `NOTICE` states the combined-work licence;
  - `README.md` names both texts.
- **FAIL:** otherwise.
- **Controls:** waived; this is a document check against a canonical text.
- **Claim limits:** bookkeeping only; not legal advice.

### AC-FIX — fixture: bootstrap, translation, compare-exchange, ledger, run counters, emission

- **Mandatory:** yes.
- **Guards against:**
  - a bootstrap that masks addresses, reads the wrong page, fires on the wrong transition, or reads unmapped memory silently;
  - a ledger that loses, misclassifies or double-latches the deciding event;
  - trace code that changes state;
  - any `A4b2` decision input that no test has exercised, including a latch or counts line whose printed fields differ from the ledger, a `[GPIN]` kind with no hook, and a `[GPIN]` loss that is not counted.
- **Evidence profile:** **fixture — a positive control, not acceptance evidence for the title.**
- **Setup:**
  - A scratch page at a `0x80xxxxxx` VA holds a known pattern, and an SGE entry pointing to it sits at another `0x80xxxxxx` VA.
  - The fixture programs GPSADDR/GPSMAXSGE.
  - GPRST goes `0→1` (no bootstrap), then `1→3` (bootstrap).
  - The anchor site is set to a fixture constant `S_A`; `S_1…S_17` are other site VAs.
  - Every case starts from `apu_watch_reset()`, and PRAM is re-zeroed wherever it is checked.
- **Oracle:** independent of the core. PRAM is checked against the pattern, `PRAM[i] == LE32(page + 4i) & 0xFFFFFF`, and the ledger is checked against the writes and stores the fixture made.
- **Line-vs-snapshot rule (trace-on registration, applies to every latch that fires in (e), (i)–(v) and (ix)).** For each latch in the case's snapshot, the case's output contains **exactly one** `[GPWATCH] latch class=<C>` line for it, and that line's `class`, `seq`, `va`, `observed`, `payload` and `site` equal the snapshot's latch record, field by field (`frame`, `insns`, `dsp_addr` are also compared where the snapshot records them). No latch line appears for a latch the snapshot does not hold. Likewise each `[GPIN]` line in (viii)–(ix) equals its snapshot table entry, and there is exactly one line per entry.
- **PASS (trace-on registration), every item:**
  - **(a)** After `1→3`, PRAM matches the pattern for `i < 0x800`, as far as the pattern covers.
  - **(b)** Known-bad: after `0→1` alone, PRAM is unchanged.
  - **(c)** Known-bad: an SGE entry at an unmapped VA produces `[GPDMA] unmapped`, leaves PRAM unchanged, and does not crash.
  - **(d)** Known-bad: when the pattern is placed only at `addr & 0x03FFFFFF`, in mapped and seeded low RAM, it is not loaded.
  - **(e)** A zero write over `3` gives `GP_CLEAR=1`, latched with `observed=00000003`. After a reset, the same write over `0` gives `GP_ZERO_OVER_ZERO=1` and `GP_CLEAR=0`. Final memory is identical in both cases.
  - **(i)** Make 20 or more zero writes over `0`. Then call `apu_watch_cpu_store(S_A, W_va, 3)` and store `3`. Then make one GP zero write **whose destination starts at `W_va − 0x810` (= `B`) and whose length exceeds `0x814`**, so the destination start differs from `W_va`. Expect:
    - `GP_CLEAR` latched, with `observed=3`, **`va = W_va`** (not `B`), `payload=0`, `site=0`, `insns` = the snapshot's `gp_insns` at the write, and `dsp_addr` = the DSP address of the word the fixture placed at offset `0x810`;
    - `GP_ZERO_OVER_ZERO ≥ 20`;
    - `CPU_ANCHOR` latched with `va = W_va`, `payload=3`, `site = S_A`;
    - `CPU_ANCHOR.seq < GP_CLEAR.seq`, and both `seq` values equal those printed on their latch lines;
    - exactly one `[GPWATCH] latch class=GP_CLEAR` line in the case's output, equal to the snapshot (line-vs-snapshot rule).

    A further zero write leaves `GP_CLEAR=1` and its latch unchanged, and emits no second `GP_CLEAR` line.
  - **(ii)** A zero write over `7` gives `GP_ZERO_OVER_OTHER`, latched with `observed=7`, and no `GP_CLEAR`.
  - **(iii)** A 2-byte write at `W_va+2` gives `GP_PARTIAL` latched, and neither `GP_CLEAR` nor any `GP_ZERO_*`.
  - **(iv)** With the word at `3`, call `apu_watch_cpu_store(S_1, W_va, 0)` and store `0`, then make a GP zero write. Expect:
    - `CPU_ZERO` latched for `S_1`, with **`site = S_1`**, `va = W_va`, `payload=0`, and its latch line equal to the snapshot;
    - `GP_ZERO_OVER_ZERO` latched with `observed=0`;
    - `GP_CLEAR=0`.
  - **(v)** Zero stores from `S_1…S_17` give 16 site latches (sites `S_1…S_16`) plus a `CPU_ZERO_OVERFLOW` latch with `site = S_17`, and counters `CPU_ZERO=17`, **`CPU_ZERO_OVERFLOW=1`**. The output holds 17 latch lines, each equal to its snapshot record. A separate call, `apu_watch_cpu_store(S_1, W_va+4, 0)`, changes no counter and emits no line.
  - **(vi)** After a bootstrap, one call to the production GP frame function gives, in the snapshot, `boots=1`, `gp_frames=1`, and `gp_insns` equal to the core's own counter. The case's output contains:
    - a `[GPWATCH] counts` line whose **`boots`, `gp_frames` and `gp_insns` equal the snapshot's values** (so `boots=1 gp_frames=1`);
    - a `[GPRUN]` line with all seven fields;
    - a `[GPIN] kind=DMA_READ` line for the scratch page, equal to its snapshot entry — although (a) read the same page earlier in the process (the reset cleared the table).
  - **(viii)** **Every `[GPIN]` kind, through its production hook.** After a reset and a bootstrap, the fixture causes, through the production code paths (the pinned read callbacks, or a small DSP program in the bootstrap image run for one frame), one read of each kind: a GP peripheral register (`PERIPH`), a guest-memory DMA read (`DMA_READ`), a FIFO read (`FIFO_READ`), and a mix-buffer read (`MIXBUF`) once with the VP's active-voice count `0` and, after another reset, once with it `> 0`. Expect: the snapshot table holds exactly one entry per `(kind, addr)` with the fixture-known `addr` and `first`; the `MIXBUF` entries record `vp_active_voices` `0` and `> 0` respectively; `gpin` equals the entry count; `GPIN_OVERFLOW=0`; repeating a read adds no entry. Then latch `GP_CLEAR` (as in (e)) and repeat one read with a new `addr`: no entry is added (cut-off). After `apu_watch_reset()` the same key is recorded again. AC-PORT step 4 records each hook call site; a kind with no hook fails this case.
  - **(ix)** **Overflow.** After a reset, call `apu_watch_gp_input` with 258 distinct `DMA_READ` keys. Expect: `gpin=256`, 256 `[GPIN]` lines each equal to its entry, counter `GPIN_OVERFLOW=2`, the `GPIN_OVERFLOW` latch fired once with `va` = the 257th key, its latch line equal to the snapshot, and a counts line showing `gpin=256 GPIN_OVERFLOW=2`. After `apu_watch_reset()`, `gpin=0` and `GPIN_OVERFLOW=0`.
  - **(vii)** The `1→3` write emits exactly one `[GPBOOT]` header with `gprst=00000003 prev=00000001`, plus 64 `pram` lines that match (a). All of this output is captured before the handler returns. The `0→1` write emits none.
- **PASS (trace-off registration):**
  - (a)–(e), (i)–(vi), (viii) and (ix) hold as snapshot, PRAM and memory checks (the ledger and the `[GPIN]` table are always computed; only their emission is trace-gated).
  - (c)'s `[GPDMA] unmapped` line is present, because it is unconditional.
  - The case output contains zero lines matching `\[GP(BOOT|RUN|IN|WATCH)\]|\[GPDMA\] (watch|frame=)`. The `unmapped` line is excluded from this count.
- **FAIL:** any item is not met.
- **UNKNOWN:** the frame function cannot be bounded in the fixture, so (vi) cannot run. The Session records why.
- **Claim limits:** fixture only, single-threaded. `gp_insns > 0` is not asserted, because it depends on the program.

### AC-DEFAULT — the default (untrapped) strict path is unchanged

- **Mandatory:** yes.
- **Guards against:** the port or the ledger leaking into the default run.
- **Evidence profile:** strict (R0).
- **Procedure:**
  - Read R0's `result.json`.
  - Read `W = MEM32(MEM32(0x001BA858)+0x810)` with `inspect-jsrf.py memory`.
  - Compute `F` (see Gates).
  - Count the lines in `jsrf_run.log` that match `\[APUMMIO\]`, `\[GP(BOOT|RUN|DMA|IN|WATCH)\]` and `\[A4BSTORE\]`.
- **Oracle:** `A4s`'s accepted strict baseline run: `diagnostic_deadline`, `W = 3`, `F ≥ 1`, zero `[APUMMIO]`. A4a R0 (`20260924-191906-091`) shows the same stop.
- **PASS:** `outcome = diagnostic_deadline`, `W = 3`, `F ≥ 1`, and all counts are 0.
- **FAIL:** any of these differs.
- **Controls:**
  - known-good: the `A4s` run;
  - known-bad for `[APUMMIO]`: A4a R1;
  - known-bad for `[GP*]`: AC-FIX's trace-on output.
- **Claim limits:** one run.

### Decision rows (evaluate in order; first match wins)

- **R1-PORT-FAIL:** AC-PORT, AC-LIC or AC-FIX cannot be made to PASS within two attempts on the same root cause.
  - Action: revert the toolkit and game to baseline, leaving no GPL code.
  - Next: Planner.
- **R1-DEFAULT-REGRESS:** AC-DEFAULT FAIL.
  - Action: revert.
  - Next: Planner.
- **R1-UNKNOWN:** a gate fails, or any AC is UNKNOWN.
  - Action: revert, unless the Planner rules otherwise.
  - Next: Planner, with that gate or AC as the brief.
- **R1-PASS:** every AC PASS.
  - Outcome: `A4b1` is accepted with exactly its "Establishes" claim. The core stays in the tree, reachable only through GP MMIO, which requires `RECOMP_APU_TRAP`.
  - Next: promote `A4b2`.

### Closure

- **Evidence index,** in `docs/reviews/a4b1-execution-evidence.md`: for each AC, the artifact path + SHA-256 → result → row. The artifacts are:
  - R0's `result.json`, `stacks.txt`, `jsrf_run.log` and `process.dmp`;
  - `a4b1-ctest.txt`;
  - the `ls-files --eol` output;
  - the pin record;
  - the exe SHA-256;
  - the toolkit vendor and head commits, and the game commit.
- Post-review edits reopen the affected criteria.
- After ACCEPT, push toolkit `main` to `origin`, per `AGENTS.md` "Toolkit remotes". Never push to `upstream`.
- **Follow-ups (record, do not do):**
  - the classifier's treatment of the inert `RECOMP_APU_DSP_ACK`;
  - the game repository has no licence file;
  - EP routing;
  - GP→CPU interrupts;
  - the reviewer's deferred advisories: fixture infeasibility routes to R1-PORT-FAIL, and the ordering of `add_test` relative to `include(CTest)`.
  - r2 review (`docs/reviews/a4b1-a4b2-r2-adequacy-review.md`): D1 (DS5 CAS race) **taken in r3** (skip on `0`, retry the exchange otherwise); D5–D7 fixed in r3 wording. Recorded, not fixed: D2 `GP_NONZERO_OVER` is never fixture-exercised (nothing decides on it); D4 `apu_watch_reset()` does not reset GP core state — cases that bootstrap reset it through GPRST.
