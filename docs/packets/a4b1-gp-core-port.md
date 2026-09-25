## A4b1 — the pinned GP DSP56300 core is ported, licensed, fixture-tested, and the default strict path is unchanged

**Class:** change   **Contract revision:** `A4b1-r1`   **Status:** draft
**Split decision:** `A4b` is split into `A4b1` (port, licence, fixtures, unchanged default path; no guest-run claim) and `A4b2` (the strict trap+trace run: boot, run, clear, no-CPU, inputs), because the port's scale risk is settled by build, ctest and one default run, and should not wait for, or be reviewed with, the run criteria.
**Governing requirement:** `A4a-r2` row `O-6` (`docs/packets/a4a-dsp-pending-word.md`); Q1 ruling `docs/reviews/a4b-q1-advisor-ruling.md`; owner Q2 decision `docs/reviews/a4b-q2-owner-decision.md`; checkpoint-40 constraints `docs/reviews/a4b-planning-rulings.md`; `docs/reviews/a4b-pio-methodology-ruling.md` §(d) (watch-cap repair).
**Depends on:** `A4a-r2` (ACCEPTED); `A3a-r25` (ACCEPTED); `A4p-r1` (ACCEPTED, row `O-GATE`, `docs/reviews/a4p-r1-acceptance-review.md`).
**Baseline:** game = the commit that adds this packet (the Session records it at promotion; since `825a62a` only `AGENTS.md` and docs changed, so the expected baseline exe SHA-256 is `9597ff7c2a377265aba8dbb90b461ebe763e02d65432e9dfa13acd925539c553`, which the Session confirms by building); toolkit `0d7929c86771dd0b971941592fd4f15436116e82`; both trees clean at promotion.
**Pin record:** `docs/reviews/a4b-xemu-pin.md` (xemu `67cc79e663038d1f55448c0f566b37dde016adf6`; per-file SHA-256 and the licence taken from each file's own header).
**Revision log:** `docs/reviews/a4b-revision-history.md` (non-authoritative).

### Motivating evidence

- `logs/runs/20260924-191833-331-a4a-r2-trap-trace` (A4a R1). STRICT, trap and trace on, toolkit `0d7929c`. The guest writes GPSADDR `0x02040 = 803CC000`, GPSMAXSGE `0x020D4 = 8`, and GPRST `0x3FFFC = 1` and then `= 3`. It makes zero GP/EP reads. The run ends `diagnostic_deadline`, spinning at `loc_001A18D0`.
- `logs/runs/20260924-191906-091-a4a-r2-default` (A4a R0). STRICT, default path. Same stop; zero `[APUMMIO]` lines. This is AC-DEFAULT's oracle.
- Toolkit source at `0d7929c` (as read by the `A4b-r3` author):
  - `apu_dsp.c` is a 166-line passthrough with no DSP core. It carries the synthetic `dsp_ack_frame`.
  - `apu_core.c:602-619` drops GP (`0x30000`) and EP (`0x50000`) writes, and `apu_core.c:626-634` answers 0 for their reads.
  - `apu_state.h:43-122` carries stale `dsp_core_t`/`DSPDMAState`/`DSPState` layouts.
- Address model (read): the `0x80000000` contiguous window is separate storage, not an alias of low RAM (`xbox_memory_layout.c:1539-1543`), and `MmGetPhysicalAddress` returns the VA (`kernel_bridge.c:1700-1707`). So xemu's `addr & 0x03FFFFFF` would read the wrong bytes for `0x803CC000`.

### Claim and boundaries

- **Establishes (on R1-PASS only):**
  - the pinned xemu GP core (interpreter backend only), GP MMIO routing and the GP DMA path are in the toolkit tree, with provenance recorded per file;
  - the synthetic DSP acknowledgement is gone from source and exe;
  - the licence of the combined work is recorded;
  - the tree builds and every ctest passes, including a fixture that exercises:
    - bootstrap through a VA SGE entry, on the pinned GPRST transition;
    - fail-closed address translation;
    - the compare-exchange on the watched word;
    - the `[GPDMA] watch` cap (the first zero-payload line always emitted);
    - trace-off silence;
  - one STRICT default (untrapped) run matches the accepted A4a R0.
- **Does not establish:** anything about the title's GP behaviour. No trapped guest run is made or claimed here. Specifically, nothing about bootstrap from the title's image, GP retirement in APU frames, who clears `+0x810`, or where the GP's inputs come from. Those are `A4b2`. It does not establish DSP56300 instruction-level correctness either: there is no independent opcode oracle, and trust in the core is inherited from the pinned commit. The fixture is a positive control, not acceptance evidence for the title.
- **Non-goals:** `dsp_jit.c`; EP routing (EP stays unrouted: writes dropped, reads 0); GP→CPU interrupts; a `PIO_FREE`/FIFO model; audio output; any game-side generated-chunk edit (those are `A4b2`); changing `docs/jsrf-run-profiles.md`; any regeneration.

### Device semantics and runtime adaptations (operative)

1. **Bootstrap and reset on the GPRST write**, as the pinned `gp_ep.c` `proc_rst_write` does (quoted in the pin record):
   - **reset** whenever either `GPRST`/`GPDSPRST` bit is clear in the new value;
   - **bootstrap** when either bit was clear in the old value and both are set in the new one.
   Execution happens only in `se_frame` frames. The GP never runs at GPRST time.
2. **GP per frame** follows the pinned `mcpx_apu_dsp_frame`: `dsp_start_frame`, then `dsp_run` in chunks until halt is requested. `dsp_c_init` is called unconditionally; there is no new environment variable. The EP keeps the existing mixbin-0/1 passthrough.
3. **One translation function**, `apu_guest_dma_ptr(uint32_t addr, uint32_t len)` (it may be named differently, but there is exactly one):
   - Its comment describes it as the inverse of `bridge_MmGetPhysicalAddress`.
   - It returns a host pointer only when the whole of `[addr, addr+len)` lies inside a mapped guest region.
   - Otherwise it logs `[GPDMA] unmapped addr=%08X len=%X` once per address, returns NULL, and the caller makes no access (fail closed).
   - xemu's `& 0x03FFFFFF` is not used on this path; a comment says why.
   - VP code is untouched.
4. **Synthetic ack removed:** `dsp_ack_frame`, `dsp_ack_init` and every read of `RECOMP_APU_DSP_ACK` are deleted.
5. **Atomic store to the watched word.** Suppose a GP DMA write covers the aligned dword `W_va = MEM32(0x001BA858)+0x810` (read at write time) and that dword's payload is `0`. The path then stores that dword with `InterlockedCompareExchange(host(W_va), 0, 3)` and writes the rest of the transfer as usual. If the exchange fails, it makes an ordinary store of the payload. Guest memory ends up identical either way. This behaviour is always on.
6. **Observation-only trace.** Everything below sits inside `if (getenv("RECOMP_APU_TRACE"))`, read once and cached. Every line is `fflush`ed, and none of it changes any value, register, store or control flow.
   - `[GPBOOT] n=%u sge0=%08X gprst=%08X` at each bootstrap. After it, the first `0x200` PRAM words as `[GPBOOT] pram %03X: w0 … w7`: 24-bit hex, 8 words per line, 64 lines.
   - `[GPRUN] frame=%u se_frame_after_boot=%u cycles=%u insns=%llu pc=%06X halt=%d tone=%d`, **one line carrying all fields**. It is emitted for the first 8 frames after each bootstrap and then every 256th frame. `se_frame_after_boot` counts `se_frame` calls since the latest bootstrap; `insns` is the core's retired count since that bootstrap.
   - `[GPDMA] frame=%u reads=%u writes=%u rbytes=%llu wbytes=%llu watch=%u watch_zero=%u` every 256th frame. `watch` and `watch_zero` count covering writes and zero-payload covering writes, and are never capped.
   - For a GP DMA write whose range covers `W_va`: `[GPDMA] watch va=%08X before=%08X payload=%08X after=%08X cas=<ok|fail|n/a> observed=%08X dsp_addr=%06X frame=%u insns=%llu`. `cas` is `n/a` when the payload is not 0; `observed` is the value the exchange found.
     - **Cap:** at most 16 such lines, counted separately from the exempt line below.
     - **Exempt line:** the **first zero-payload covering write in the process** is always logged, whatever the cap.
     - **Cap line:** when the 16-line cap is first hit, emit exactly one `[GPDMA] watch cap reached frame=%u`.
   - `[GPIN] kind=<PERIPH|DMA_READ|FIFO_READ|MIXBUF> addr=%08X first=%08X frame=%u`, once per distinct (kind, addr), until the first zero-payload watch line. `MIXBUF` lines also carry `vp_active_voices=%u`.

### Readiness

- **Tools (verified at baseline, 2026-09-24):** `python -X utf8 scripts\build-jsrf.py`; `ctest --test-dir build -C Release --output-on-failure` (12/12); `python -X utf8 scripts\run-jsrf.py … --profile strict`; `scripts\check-run-profile.py`; `scripts\check-dump-mapping.py`; `scripts\inspect-jsrf.py memory|disasm`.
- **Prerequisites:**
  - The Session vendors the files from the pin record's table only, at the pinned SHA. Each file's SHA-256 must match the record before any local edit.
  - The emulated disk images must be accessible (a non-confined run).
- **Stop if:**
  - any of `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED`, `JSRF_ABI_CONTINUE` is in R0's environment;
  - the runner refuses `--profile strict`;
  - a vendored file's hash does not match the pin record;
  - the port needs a change outside the write scope;
  - the same build/ctest root cause survives two attempts (→ R1-PORT-FAIL);
  - any code path would let the APU write guest memory other than through the GP DMA path, `FEMEMDATA` or VP (both existing);
  - the pre-check fails. The pre-check: `recomp_0005.c` lines 6748 and 6751 read `loc_001A18D0: ;` and `… goto loc_001A18D0; …`.

  A session that ends mid-execution selects no row.

### Execution

**Steps**

1. **Vendor** the pin record's files into toolkit `src/apu/dsp/`, keeping upstream headers intact:
   - `dsp.c`, `dsp.h`, `dsp_c.c`, `dsp_internal.h`, `debug.h`;
   - `interp/dsp_cpu.c`, `interp/dsp_cpu.h`, `interp/dsp_cpu_regs.h`, `interp/dsp_emu.c.inc`;
   - `interp/dsp_dis.c.inc` and `interp/debug.c`, only if needed to compile;
   - `gp_ep.c`, `gp_ep.h`, `dsp_dma.c`, `dsp_dma.h`, `dsp_dma_regs.h`, `trace.h`.

   Exclude `dsp_jit.*`. List every local modification in `a4b-xemu-pin.md`.
2. **One layout:**
   - Replace the stale state definitions in `apu_state.h` with the pinned headers.
   - Fix the `pram_opcache` reset in `apu_core.c` to the pinned API.
   - Shims go in `apu_shim.h` as no-ops or pass-throughs, never as value-producing stand-ins.
3. **Route GP MMIO** (`0x30000..0x3FFFF` → the pinned `gp_read`/`gp_write`) from `mcpx_apu_dispatch_mmio` and `mcpx_apu_mmio_read_quiet`. EP stays unrouted. The existing read note keeps firing for EP and for unmodelled GP offsets.
4. **DMA** through Device semantics 3, with the atomic store of Device semantics 5.
5. **Frames** per Device semantics 2; delete the synthetic ack (Device semantics 4).
6. **Trace** per Device semantics 6.
7. **Licence bookkeeping** (toolkit):
   - Add a verbatim `LICENSES/GPL-2.0.txt` (`https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt`).
   - `NOTICE` lists each ported file **under its own header's licence**, with its copyright lines and the pinned SHA:
     - GPL-2.0-or-later: `dsp.c`, `dsp.h`, `dsp_c.c`, `dsp_internal.h`, `debug.h`, `interp/*`;
     - LGPL-2.1-or-later: `gp_ep.*`, `dsp_dma*`.
   - `NOTICE` states that a binary linking `xbox_apu` is a combined work under GPL-2.0-or-later, and its MIT paragraph is amended to say so.
   - `LICENSES/README.md` names both texts.
8. **Fixture** (AC-FIX), registered in ctest **twice**: once with `RECOMP_APU_TRACE=1` through the test's `ENVIRONMENT` property, once without it. The fixture captures its own trace output, for example by redirecting `stderr` to a file it then reads.
9. Build, run ctest, make R0, and record everything in `docs/reviews/a4b1-execution-evidence.md`.

**Run** (from the game root, 30 s, archived under `logs/runs/`):

```powershell
Remove-Item Env:\RECOMP_APU_DSP_ACK,Env:\RECOMP_AC97_READY,Env:\JSRF_ALLOW_UNRESOLVED,Env:\JSRF_ABI_CONTINUE,Env:\RECOMP_APU_TRAP,Env:\RECOMP_APU_TRACE -ErrorAction SilentlyContinue
$env:RECOMP_GPU_ACK='0'; $env:RECOMP_KERNEL_LOG_BUDGET='100000'
python -X utf8 scripts\run-jsrf.py --seconds 30 --profile strict --label a4b1-default   # R0
```

- **Write scope:**
  - toolkit: `src/apu/**`, `NOTICE`, `LICENSES/**`, plus `tests/` and `CMakeLists.txt` for the fixture only;
  - game: `CMakeLists.txt` and `tests/`, for the fixture only if it is hosted there;
  - docs: `docs/reviews/a4b1-*.md` and `docs/reviews/a4b-xemu-pin.md` (the local-modification list only).

  Nothing else. **No game `src/` edit.**
- **Build/run owner:** the Session.

### Gates (checked before any AC)

- **G1:** `check-run-profile.py` prints `STRICT` for R0.
- **G2:** `check-dump-mapping.py` gives `matches ≥ 1` and `content-mismatch: 0`.
- **G3:** `result.json` is readable.
- **G4:** the pre-check passes.

Any gate failing → R1-UNKNOWN.

### AC-PORT — pinned core ported with recorded provenance; tree builds and tests green

- Mandatory: yes.
- Guards against: an unpinned or silently modified core; a port that breaks the build or existing tests; a surviving synthetic ack.
- Evidence profile: structural.
- Procedure:
  1. Check `a4b-xemu-pin.md`: SHA, per-file SHA-256, licence and a local-modification list.
  2. `Get-FileHash` each vendored file at its pre-edit commit, and compare it with the record.
  3. `python -X utf8 scripts\build-jsrf.py`.
  4. `ctest --test-dir build -C Release --output-on-failure > <R0>\ctest.txt`.
  5. `Select-String -Path <toolkit>\src\apu -Pattern 'RECOMP_APU_DSP_ACK|dsp_ack_' -Recurse`.
  6. Search the strings of `build\Release\jsrf_recomp.exe` for `RECOMP_APU_DSP_ACK`.
- Artifact: pin record; build output; `ctest.txt`; exe SHA-256; toolkit commit.
- Oracle: upstream file hashes at the pinned SHA.
- PASS: the record is complete, the hashes match, the build succeeds, every ctest passes (baseline 12 plus both fixture registrations), and both searches return zero hits.
- FAIL: any of those is not met.
- UNKNOWN: a record field is missing.
- Controls: the baseline exe `9597ff7c…` is the known-good for the string search (the string is present).
- Claim limits: structural only; nothing about DSP semantics.

### AC-LIC — the combined work's licensing is recorded

- Mandatory: yes, whenever the ported code remains in the tree.
- Guards against: shipping a GPL-2.0-or-later combined work described as MIT-only, or listing a file under the wrong licence.
- Evidence profile: manual/structural.
- Procedure: `Get-FileHash LICENSES\GPL-2.0.txt` and compare it with the gnu.org text; read `NOTICE` and `LICENSES/README.md` against the pin record's licence column.
- PASS: all of the following hold:
  - the GPL text is verbatim;
  - `NOTICE` lists every ported file under its header's licence, with its copyright lines and the pinned SHA;
  - `NOTICE` states the combined-work licence;
  - `LICENSES/README.md` names both texts.
- FAIL: otherwise.
- Controls: waived; this is a document check against a canonical text.
- Claim limits: bookkeeping only; not legal advice.

### AC-FIX — fixture: VA-SGE bootstrap, fail-closed translation, compare-exchange, watch cap, trace-off silence

- Mandatory: yes.
- Guards against:
  - a bootstrap that masks addresses, reads the wrong page, fires on the wrong transition, or reads unmapped memory silently;
  - a lost zero-payload watch line (r3 B2);
  - trace code that changes state.
- Evidence profile: **fixture — a positive control only, not acceptance evidence for the title.**
- Procedure: the fixture maps a guest window (or uses the runtime layout) as follows:
  - it places a scratch page at a `0x80xxxxxx` VA and fills it with a known pattern;
  - it writes an SGE entry pointing at that page at another `0x80xxxxxx` VA;
  - it programs GPSADDR/GPSMAXSGE;
  - it drives GPRST through `0→1` (does not bootstrap) and `1→3` (bootstraps);
  - it then performs the GP DMA writes in (e) and (f).
- Oracle: the pattern itself, independent of the core: `PRAM[i] == LE32(page + 4i) & 0xFFFFFF`.
- PASS, in the trace-on registration:
  - (a) after `1→3`, PRAM matches the pattern for `i < 0x800`, as far as the pattern covers;
  - (b) known-bad: after `0→1` alone, PRAM is unchanged;
  - (c) known-bad: an SGE entry at an unmapped VA produces the `unmapped` line, and leaves PRAM unchanged with no crash;
  - (d) known-bad: the pattern placed only at `addr & 0x03FFFFFF` is not loaded;
  - (e) a zero-payload write covering a watched dword holding `3` reports `cas=ok observed=00000003`, while the same write over a dword holding `0` reports `cas=fail observed=00000000`, and the final memory is identical in both cases;
  - (f) cap check:
    - 20 covering writes with a non-zero payload, then one with a zero payload over a dword holding `3`;
    - the log has exactly 16 non-zero-payload watch lines, exactly one `watch cap reached` line, and the zero-payload line with `cas=ok`;
    - the next `[GPDMA]` summary reports `watch=21 watch_zero=1`, or the fixture calls the summary emitter directly;
  - (g) one `[GPBOOT]` header, followed by 64 `pram` lines that match (a).
- PASS, in the trace-off registration: (a)–(e) hold as memory/PRAM checks, and the captured output contains zero `[GP` lines.
- FAIL: any item is not met.
- Claim limits: fixture only; nothing about the title's image or run.

### AC-DEFAULT — the default (untrapped) strict path is unchanged

- Mandatory: yes.
- Guards against: the port leaking into the default run.
- Evidence profile: strict (R0).
- Procedure:
  1. Take R0's `result.json`.
  2. Read `W = MEM32(MEM32(0x001BA858)+0x810)` with `inspect-jsrf.py memory`.
  3. Count `F`: matches of `sub_001A1769\+0x[0-9A-Fa-f]+ .*recomp_0005\.c:67(4[89]|5[01])\b` in `stacks.txt`.
  4. In `jsrf_run.log`, count lines matching `\[APUMMIO\]`, `\[GP(BOOT|RUN|DMA|IN)\]` and `\[A4BSTORE\]`.
- Oracle: the accepted A4a R0 (`20260924-191906-091`): `diagnostic_deadline`, `W = 3`, `F ≥ 1`, zero `[APUMMIO]`.
- PASS: `outcome = diagnostic_deadline`, `W = 3`, `F ≥ 1`, and all three counts are 0.
- FAIL: any of these differs.
- Controls:
  - known-good: the A4a R0 artifact;
  - known-bad for the `[APUMMIO]` count: the A4a R1 artifact (non-zero);
  - known-bad for the `[GP*]` counts: AC-FIX's trace-on registration (non-zero).
- Claim limits: one run; not a proof of determinism; says nothing about the trapped path.

### Decision rows (evaluate in order; first match wins)

- **R1-PORT-FAIL:** AC-PORT, AC-LIC or AC-FIX cannot be made to pass within two attempts on the same root cause. → Revert the toolkit and game to baseline, leaving no GPL code in the tree. → Planner re-plans.
- **R1-DEFAULT-REGRESS:** AC-DEFAULT FAIL. → Revert. → Planner.
- **R1-UNKNOWN:** a gate fails, or any AC is UNKNOWN. → Planner re-plans with the failing gate/AC as its brief. The code is reverted unless the Planner rules otherwise.
- **R1-PASS:** every AC PASS. → `A4b1` accepted with exactly its "Establishes" claim. The core stays in the tree; it is reachable only through GP MMIO, which the guest reaches only under `RECOMP_APU_TRAP`. → Next: promote `A4b2`.

### Closure

- Evidence index (`docs/reviews/a4b1-execution-evidence.md`): each AC → artifact path + SHA-256 (R0's `result.json`, `stacks.txt`, `jsrf_run.log`, `process.dmp`; `ctest.txt`; pin record; exe SHA-256; toolkit and game commits) → result → selected row.
- Post-review edits reopen the affected criteria. An unrelated next stop is recorded as a follow-up; the scope does not expand.
- Follow-ups to record, not to do:
  - the classifier's treatment of the now-inert `RECOMP_APU_DSP_ACK` name;
  - the game repository has no licence file of its own;
  - EP routing;
  - GP→CPU interrupts.
