## A4b — the GP DSP56300 engine clears the title's pending word in a strict run

**Class:** change   **Contract revision:** `A4b-r3`   **Status:** draft
**Governing requirement:** `A4a-r2` outcome row `O-6` (`docs/packets/a4a-dsp-pending-word.md`, frozen SHA-256 `2366E18C…22FBC`); Advisor Q1 ruling `docs/reviews/a4b-q1-advisor-ruling.md` (binding, four conditions); owner Q2 decision `docs/reviews/a4b-q2-owner-decision.md`; Advisor checkpoint-40 ruling to the A4b Planner (seven constraints; Session records it verbatim in `docs/reviews/a4b-planning-rulings.md`, child `407c54a3-6ca4-4a65-835b-faf5355195cd`, route `claude/claude-opus-5-5` @ `high`).
**Depends on:** `A4a-r2` (ACCEPTED), `A3a-r25` (ACCEPTED).
**Baseline:** game `825a62a` (the commit that adds this packet; supersedes `60df196`, which the draft named); toolkit `0d7929c86771dd0b971941592fd4f15436116e82`; both trees clean at promotion. Baseline exe SHA-256 `9597ff7c2a377265aba8dbb90b461ebe763e02d65432e9dfa13acd925539c553`.
**Revision log:** `docs/reviews/a4b-revision-history.md` (non-authoritative).
**Pin record:** `docs/reviews/a4b-xemu-pin.md` (xemu commit `67cc79e663038d1f55448c0f566b37dde016adf6`; per-file SHA-256 and the licence read from each file's own header).
**Advisor rulings:** `docs/reviews/a4b-q1-advisor-ruling.md` (Q1, four conditions) and `docs/reviews/a4b-planning-rulings.md` (checkpoint-40, seven constraints, verbatim).

### Motivating evidence

- `logs/runs/20260924-191833-331-a4a-r2-trap-trace` (R1) — STRICT, trap+trace, toolkit `0d7929c` — the guest wrote GPSADDR `0x02040 = 803CC000` (L3188), GPSMAXSGE `0x020D4 = 8` (L3189), GPRST `0x3FFFC = 1` (L3190) then `= 3` (L3201); FECTL `0x0100F` / SECTL `0x0F` leave the APU frame gate open from L3286-3287 (`[APU] started by the title`), i.e. **after** GPRST=3; zero GP/EP reads (coverage witness); `MEM32(803CC000) = 803C0000 = B`; `[B, B+0x5CC)` equals XBE file offset `0x1A7D60`; `MEM32(B+0x810) = 3`; `diagnostic_deadline`, spin at `loc_001A18D0` (`recomp_0005.c:6748-6751`).
- `logs/runs/20260924-191906-091-a4a-r2-default` (R0) — STRICT, default path — same stop; zero `[APUMMIO]` lines.
- Toolkit source (read at `0d7929c`): `apu_dsp.c` is a 166-line passthrough with no DSP core and the synthetic `dsp_ack_frame`; `apu_core.c:602-619` drops GP (`0x30000`) and EP (`0x50000`) accesses; `apu_core.c:626-634` returns 0 for them; `apu_core.c:454-468` runs `se_frame` only when `SECTL & 0x18 != 0` and `FECTL & 0xE0 == 0` (any method-mode bit counts as not-running) and the test tone is off.
- Address model (read): the APU is created with `g_apu_ram_ptr = xbox_GetMemoryBase()` (game `src/main.c:250`); guest VA → host is `+g_memory_offset` for every mapped VA (`XBOX_MAP_START = 0`); the `0x80000000` contiguous window is **separate storage, not an alias of low RAM** (`xbox_memory_layout.c:1539-1543`); `MmGetPhysicalAddress` returns the VA (`kernel_bridge.c:1700-1707`). xemu's `ldl_le_phys(addr & 0x03FFFFFF)` would therefore read `0x003CC000` for `G = 0x803CC000` — the wrong bytes.
- Guest stop-path stores (XBE disassembly, read): `0x001A1751 and dword [edi+0x810], 0` in `sub_001A1747` (`recomp_0005.c:6524`), called from `0x001A1FBF`; `0x001A1FA7 mov [edi+0x10], ebp` with `edi = MEM32(0x1BA858)+0x800` (`recomp_0005.c:8088`). The guest's own write of `3` is `recomp_0005.c:6746` (`MEM32(ebx) = eax`, `ebx = B+0x810`).

### Claim and boundaries

- **Establishes (on R-PASS only):** in one STRICT run with `RECOMP_GPU_ACK=0` and `RECOMP_APU_TRAP=1`, the wait at `loc_001A18D0` was satisfied by modelled GP execution of the guest's own command: the guest's GPRST write bootstrapped a DSP56300 GP core (ported from a pinned xemu commit) from the scratch pages named by the guest's SGE table at `G`; the GP retired instructions in APU frames; and the GP's own DMA write path atomically exchanged the `3` at `MEM32(0x001BA858)+0x810` for `0` (compare-exchange succeeded, so no store intervened), with no synthetic acknowledgement. This is exactly the Q1 ruling's one sentence ("this wait was satisfied by modelled GP execution of the guest's own command").
- **Does not establish (Q1 ruling condition 4, cited verbatim in substance):** the guest's arrival at the spin, and everything after it, goes through `PIO_FREE` stub gates (`0xFE820010` → constant `0x80`) and is **exploratory-grade**; A4b therefore does not establish boot progress, liveness, DirectSound init success, audio output, or the title's behaviour under a modelled VP FIFO; nor that the title reaches the spin in the same state or with the same timing as real hardware. "The guest leaves `loc_001A18D0`" is an observation only, and any progress after it is exploratory-grade because it passes through `PIO_FREE` stub gates.
- **Also not established:** DSP56300 instruction-level correctness — there is **no independent opcode oracle**; trust in the core is inherited from the pinned xemu commit (recorded by SHA, paths and line ranges), and no DSP behaviour beyond the one witnessed store is claimed. Physical/VA aliasing is **not modelled**: SGE/PRD entries are resolved as guest VAs (runtime adaptation, below). EP (`0x50000`) stays unrouted (writes dropped, reads 0). GP→CPU interrupts are not delivered (PCI IRQ is a stub). Nothing about the `PIO_FREE` value itself.
- **Non-goals:** a `PIO_FREE`/FIFO model (its own packet, placed before the first strict liveness/boot criterion past the spin — plan follow-up); EP execution; audio output; `dsp_jit.c`; the disassembler beyond what the interpreter needs to compile; changing `docs/jsrf-run-profiles.md`; any regeneration.

### Device semantics and runtime adaptations (operative)

1. **Bootstrap** happens on the guest's GPRST write, as the pinned `gp_ep.c` / `proc_rst_write` does. The exact transition condition is recorded from the pinned source in `docs/reviews/a4b-xemu-pin.md`: **reset** whenever either `GPRST|GPDSPRST` bit is clear in the new value; **bootstrap** on the transition where either bit was clear in the old value and both are set in the new one. So a write of `3` from `1` bootstraps. **Execution** happens only in `se_frame` frames, which R1 shows are gated open *after* GPRST=3; the packet never assumes the GP runs at GPRST time.
2. **GP execution per frame** follows the pinned `mcpx_apu_dsp_frame` for the GP (start frame, `dsp_run` in chunks until halt requested), with the pinned commit's GP-enabling setting applied **unconditionally** in `mcpx_apu_update_dsp_preference` (no new environment variable). The EP keeps the existing mixbin-0/1 passthrough to the monitor.
3. **SGE/PRD address translation (Advisor constraint 2).** One named function, `apu_guest_dma_ptr(uint32_t addr, uint32_t len)` (name may differ; exactly one), documented in its comment as the **inverse of `bridge_MmGetPhysicalAddress`** (this kernel reports physical == VA, so the address the title wrote into an SGE entry is the VA our kernel told it was physical). It returns the host pointer for `[addr, addr+len)` only when the whole range is inside a mapped guest region; otherwise it logs `[GPDMA] unmapped addr=%08X len=%X` once per address and returns NULL, and the caller performs **no** access (fail closed; the run then selects R-NOBOOT/R-NOCLEAR/R-UNKNOWN via the criteria). xemu's `& 0x03FFFFFF` masking is left unused on this path, with a comment saying why. Existing VP code is untouched.
4. **Synthetic acknowledgement removed.** `dsp_ack_frame`, `dsp_ack_init` and every read of `RECOMP_APU_DSP_ACK` are deleted from `apu_dsp.c`.
5. **Atomic store to the watched word (Advisor amendment to constraint 3(ii), `docs/reviews/a4b-planning-rulings.md`).** When a GP DMA write to guest memory covers the aligned dword `W_va = MEM32(0x001BA858)+0x810` (read at write time) and that word's payload is `0`, the write path stores that one dword with `InterlockedCompareExchange(host(W_va), 0, 3)`. It writes the rest of the transfer as usual. If the exchange fails, it does an ordinary store of the payload and logs the observed value. Guest memory ends up identical either way, so device semantics do not change. This is always on (not tied to `RECOMP_APU_TRACE`); only the log line is traced.

### Readiness

- **Tools (verified to run at baseline, 2026-09-24):** `python -X utf8 scripts\build-jsrf.py` (exit 0); `ctest --test-dir build -C Release --output-on-failure` (12/12 at baseline); `python -X utf8 scripts\run-jsrf.py … --profile strict` (exit 0); `python -X utf8 scripts\check-run-profile.py <run>`; `python -X utf8 scripts\check-dump-mapping.py <run>`; `python -X utf8 scripts\inspect-jsrf.py memory|disasm …`. Each was run by the Session before freezing.
- **Prerequisites:** the Session fetches the xemu sources at **one pinned commit SHA** (a commit, not `master`), and records in `docs/reviews/a4b-xemu-pin.md`: the SHA, the upstream path and SHA-256 of every file ported, and each file's licence header. Emulated disk images accessible (non-confined run).
- **Stop if:** any of `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED`, `JSRF_ABI_CONTINUE` is in the launch environment; the runner refuses `--profile strict`; the port needs a change outside the write scope; the same build/ctest root cause survives two attempts (→ R-PORT-FAIL); anything would write guest memory from the APU other than through the ported GP DMA path, `FEMEMDATA` (existing) or VP (existing); any step would set or clear `+0x810` other than by GP execution; the pre-check that `recomp_0005.c` lines 6748 and 6751 read `loc_001A18D0: ;` and `… goto loc_001A18D0; …` fails. A session that ends mid-execution selects no row; the packet stays promoted.

### Execution

**Steps**

1. **Pin and vendor.** Copy the pinned interpreter core into toolkit `src/apu/dsp/` (at least `dsp.c`, `dsp.h`, the interpreter backend `dsp_c.c` if the pinned commit has one, `dsp_dma.c`, `dsp_dma.h`, `dsp_dma_regs.h`, `dsp_internal.h`, `interp/dsp_cpu.c`, `interp/dsp_cpu.h`, `interp/dsp_cpu_regs.h`, `interp/dsp_emu.c.inc`, and `interp/dsp_dis.c.inc` / `debug.c` only if needed to compile), keeping upstream headers intact; list every local modification in `a4b-xemu-pin.md`. Exclude `dsp_jit.c`.
2. **One layout.** Replace the stale `dsp_core_t` / `DSPDMAState` / `DSPState` definitions in `apu_state.h` with the pinned headers (single source of truth); fix `apu_core.c:503-510` (`pram_opcache` reset) to the pinned API. Add missing shims (`memory_region_set_dirty`, `trace_dsp*`, `DPRINTF`, anything else) to `apu_shim.h` as no-ops or pass-throughs, never as value-producing stand-ins.
3. **Route GP MMIO.** Port the GP half of the pinned `gp_ep.c` (LGPL-2.1-or-later): `0x30000..0x3FFFF` → `gp_read`/`gp_write` (XMEM, MIXBUF, YMEM, PMEM, GPRST via `proc_rst_write`) from `mcpx_apu_dispatch_mmio` and `mcpx_apu_mmio_read_quiet`. EP stays unrouted. The once-per-block read note keeps firing for EP and for any GP offset the ported code does not model.
4. **Scratch/FIFO DMA** through the step-3 translation function (Device semantics 3).
5. **Frame execution** per Device semantics 2; delete the synthetic ack (Device semantics 4).
6. **Observation-only instrumentation**, all inside `if (getenv("RECOMP_APU_TRACE"))` (read once and cached), changing no value, register, store or control flow:
   - `[GPBOOT] n=%u sge0=%08X gprst=%08X` at each bootstrap, followed by the first `0x200` PRAM words as `[GPBOOT] pram %03X: w0 w1 … w7` (24-bit hex, 8 per line, 64 lines).
   - `[GPRUN] frame=%u cycles=%u insns=%llu pc=%06X halt=%d tone=%d` for the first 8 frames after each bootstrap and every 256th frame, plus `[GPRUN] se_frame_after_boot=%u` in the same cadence.
   - `[GPDMA]` summary counters (reads/writes, bytes) every 256th frame; and, for **every** GP DMA write whose destination range covers `W_va = MEM32(0x001BA858)+0x810` (read at write time): `[GPDMA] watch va=%08X before=%08X payload=%08X after=%08X cas=<ok|fail|n/a> observed=%08X dsp_addr=%06X frame=%u insns=%llu` (first 16 occurrences). `cas` and `observed` report Device semantics 5: `n/a` when the payload is not `0`, and `observed` is the value the exchange found.
   - `[GPIN] kind=<PERIPH|DMA_READ|FIFO_READ|MIXBUF> addr=%08X first=%08X frame=%u` once per distinct (kind, addr) until the first `[GPDMA] watch` line with `payload=00000000`; `MIXBUF` lines also carry `vp_active_voices=%u` for that frame.
   - Game side, generated-chunk observation edits (hand-applied; comment `A4b observation — re-apply after regeneration`): before each of the stores at `recomp_0005.c:6746` (the guest's `3`, **positive control**), `recomp_0005.c:6524` (`0x001A1751`), `recomp_0005.c:8088` (`0x001A1FA7`), and every other generated store textually matching `MEM32\(.* \+ 0x810\) =` (at baseline `recomp_0000.c:135283, 135406, 135871`), call `jsrf_watch_store(site_va, target_va, value)`, which, when `RECOMP_APU_TRACE` is set and `target_va == MEM32(0x001BA858)+0x810`, prints `[A4BSTORE] site=%08X va=%08X value=%08X` (first 8 per site). The Session records each site's VA from its generated label. **These counters are corroboration only**; the primary witness is Device semantics 5. Declaration: each edited chunk gets a single file-scope `extern void jsrf_watch_store(uint32_t, uint32_t, uint32_t);` beside the edit. No generated header is changed. Definition: in the existing `src/diagnostics.c`, which is already linked into `jsrf_recomp`.
7. **Licence bookkeeping** (toolkit): add verbatim `LICENSES/GPL-2.0.txt` (from `https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt`); `NOTICE` gains a "GPL-2.0-or-later — extracted from xemu" section naming each ported GPL file with its copyright lines and the pinned SHA, adds the ported LGPL code to the LGPL list, and states that a binary linking `xbox_apu` is a combined work distributed under GPL-2.0-or-later; the MIT paragraph is amended to say so; `LICENSES/README.md` names both texts. **Per the pin record, list each ported file under its own header's licence — the DMA layer (`dsp_dma.c`, `dsp_dma.h`, `dsp_dma_regs.h`) and `gp_ep.c`/`gp_ep.h` are LGPL-2.1-or-later, while `dsp.c`/`dsp_c.c`/`dsp_internal.h`/`debug.h`/`interp/*` are GPL-2.0-or-later.**
8. **Fixture test** (toolkit or game `tests/`, registered in `ctest`): see AC-FIX.
9. Build, `ctest`, runs, measurements; record everything in `docs/reviews/a4b-execution-evidence.md`.

**Runs** (from the game root, 30 s each, archived under `logs/runs/`):

```powershell
Remove-Item Env:\RECOMP_APU_DSP_ACK,Env:\RECOMP_AC97_READY,Env:\JSRF_ALLOW_UNRESOLVED,Env:\JSRF_ABI_CONTINUE,Env:\RECOMP_APU_TRAP,Env:\RECOMP_APU_TRACE -ErrorAction SilentlyContinue
$env:RECOMP_GPU_ACK='0'; $env:RECOMP_KERNEL_LOG_BUDGET='100000'
$env:RECOMP_APU_TRAP='1'; $env:RECOMP_APU_TRACE='1'
python -X utf8 scripts\run-jsrf.py --seconds 30 --profile strict --label a4b-gp-trap-trace   # R1 (claim run)
Remove-Item Env:\RECOMP_APU_TRAP,Env:\RECOMP_APU_TRACE
python -X utf8 scripts\run-jsrf.py --seconds 30 --profile strict --label a4b-default         # R0 (default path)
```

If R1 selects R-UNKNOWN, rerun R1 once with identical settings; the second result stands.

- **Write scope:** toolkit `src/apu/**`, `NOTICE`, `LICENSES/**`, toolkit `tests/` or `CMakeLists.txt` only for the fixture; game `src/recomp/gen/recomp_0000.c` and `recomp_0005.c` (only the step-6 observation calls and their `extern` declarations), game `src/diagnostics.c` (only the `jsrf_watch_store` definition), game `CMakeLists.txt` and `tests/` (only the fixture), `docs/reviews/a4b-*.md`. Nothing else.
- **Build/run owner:** the Session.

### Gates (checked before any AC below is read)

G1: `check-run-profile.py` prints `STRICT` for R1 and R0. G2: `check-dump-mapping.py` gives `matches ≥ 1`, `content-mismatch: 0` for both. G3: `result.json` readable for both. G4: the recomp_0005.c 6748/6751 pre-check passes. Any gate failing → R-UNKNOWN.

### AC-PORT — the pinned core is ported with recorded provenance and the tree builds and tests green

- Mandatory: yes
- Guards against: an unpinned or silently modified core; a port that breaks the build or existing behaviour tests.
- Evidence profile: structural.
- Procedure: `a4b-xemu-pin.md` complete (SHA, per-file upstream SHA-256, licence header, local-modification list); `python -X utf8 scripts\build-jsrf.py`; `ctest --test-dir build -C Release --output-on-failure > <R1>\ctest.txt`; `Select-String -Path <toolkit>\src\apu -Pattern 'RECOMP_APU_DSP_ACK|dsp_ack_' -Recurse` and a strings search of `build\Release\jsrf_recomp.exe` for `RECOMP_APU_DSP_ACK`.
- Artifact: pin record; build output; `ctest.txt`; new exe SHA-256; toolkit commit.
- Oracle: upstream file hashes at the pinned SHA.
- PASS: pin record complete; build succeeds; every ctest passes (baseline 12 + the fixture); both searches return zero hits. FAIL: any of these not met. UNKNOWN: pin record missing a field.
- Controls: the string search's known-good is the baseline exe `9597ff7c…` (string present).
- Claim limits: structural only; says nothing about DSP semantics.

### AC-LIC — the combined work's licensing is recorded (Advisor constraint 7)

- Mandatory: yes, for **every** row in which the GPL core remains in the toolkit tree.
- Guards against: shipping a GPL-2.0-or-later combined work described as MIT-only.
- Evidence profile: manual/structural.
- Procedure: `Get-FileHash LICENSES\GPL-2.0.txt`; compare with the gnu.org text; read `NOTICE` and `LICENSES/README.md`.
- PASS: GPL-2.0 text present and verbatim; NOTICE lists every ported GPL file with its copyright and the pinned SHA, lists the ported LGPL code, and states the combined-work licence; README names both texts. FAIL otherwise.
- Controls: waived — a document check against a canonical text.
- Claim limits: bookkeeping only; not legal advice.

### AC-FIX — fixture: bootstrap through a VA SGE, and fail-closed translation

- Mandatory: yes
- Guards against: a bootstrap that masks addresses, reads the wrong page, fires on the wrong GPRST transition, or reads unmapped memory silently.
- Evidence profile: **fixture — positive control only, not acceptance evidence.**
- Procedure: a ctest that maps a guest window (or uses the runtime layout) with a scratch page at a `0x80xxxxxx` VA filled with a known pattern, writes an SGE entry pointing at it at another `0x80xxxxxx` VA, programs GPSADDR/GPSMAXSGE, and drives GPRST through the pinned code's non-bootstrapping and bootstrapping transitions.
- Oracle: the pattern itself (independent of the core): `PRAM[i] == LE32(page + 4i) & 0xFFFFFF` for `i < 0x800` as far as the pattern covers.
- PASS: (a) after the bootstrapping transition PRAM matches the pattern; (b) **known-bad:** after only the non-bootstrapping transition PRAM is unchanged; (c) **known-bad:** an SGE entry pointing at an unmapped VA produces the `unmapped` log and leaves PRAM unchanged with no crash; (d) **known-bad:** the same pattern placed at `addr & 0x03FFFFFF` only (the masked address) is **not** loaded; (e) a GP DMA write with a zero payload covering a watched dword holding `3` reports `cas=ok`, and the same write over a dword holding `0` reports `cas=fail observed=00000000`; the final memory is identical in both cases. FAIL: any of (a)-(e) not met.
- Claim limits: fixture; says nothing about the title's image or run.

### AC-DEFAULT — the default (untrapped) strict path is unchanged

- Mandatory: yes
- Guards against: the port leaking into the default run.
- Evidence profile: strict (R0).
- Procedure: R0; `W = MEM32(MEM32(0x001BA858)+0x810)` via `inspect-jsrf.py memory`; `F` as in A4a (`sub_001A1769\+0x[0-9A-Fa-f]+ .*recomp_0005\.c:67(4[89]|5[01])\b` count in `stacks.txt`); counts in `jsrf_run.log` of `\[APUMMIO\]`, `\[GP(BOOT|RUN|DMA|IN)\]`, `\[A4BSTORE\]`, `DSP GP/EP initialized`.
- Oracle: accepted A4a R0 (`20260924-191906-091`): `diagnostic_deadline`, `W = 3`, `F ≥ 1`, zero `[APUMMIO]`.
- PASS: `outcome = diagnostic_deadline`, `W = 3`, `F ≥ 1`, all four counts 0. FAIL: any differs.
- Controls: the A4a R0 artifact is the known-good; R1 is the known-bad for the counts (non-zero).
- Claim limits: one run; not a proof of determinism.

### AC-BOOT — the guest's GPRST write bootstraps the GP from the title's image

- Mandatory: yes
- Guards against: a GP that "runs" code other than the title's image (zeros, a wrong page, the masked address).
- Evidence profile: strict (R1).
- Procedure: from R1 `jsrf_run.log`: the `[GPBOOT]` header lines; the `[APUMMIO] write 0x3FFFC` lines; parse the `pram` lines and compare word `i` for `i < 0x173` with `LE32(default.xbe[0x1A7D60 + 4i]) & 0xFFFFFF` (Session writes this one-off comparison into the evidence record with its output).
- Oracle: the original XBE bytes (A4a image `I`), not the implementation.
- PASS: ≥ 1 `[GPBOOT]` line, the first at a GPRST value the pinned code defines as bootstrapping and after `write 0x02040 = 803CC000`; `sge0 = B`; all `0x173` words equal. FAIL: no `[GPBOOT]`, `sge0 ≠ B`, or any word differs. UNKNOWN: fewer than 64 `pram` lines for that bootstrap.
- Controls: known-bad — the same comparison against file offset `0x1A7D64` must differ at word 0.
- Claim limits: shows what was loaded, not that it executes correctly.

### AC-RUN — GP instructions retire in APU frames after the bootstrap

- Mandatory: yes
- Guards against: a loaded but never-clocked core (U5: frame gate closed, test tone path, halted core).
- Evidence profile: strict (R1).
- Procedure: `[GPRUN]` lines after the first `[GPBOOT]`.
- PASS: some `[GPRUN]` line has `se_frame_after_boot ≥ 1`, `insns > 0`, `tone=0`. FAIL (→ R-NOFRAMES): `se_frame_after_boot = 0` on every line, or no `[GPRUN]` line although `[GPBOOT]` exists; FAIL (→ R-NOEXEC): frames run (`se_frame_after_boot ≥ 1` on some line) but `insns = 0` on every line.
- Controls: waived — retirement count comes from the ported core's own counter; its meaning is inherited from the pinned commit (limit).
- Claim limits: retirement, not correctness.

### AC-CLEAR — the GP's own DMA write made the 3→0 transition atomically (Q1 condition 1; Advisor constraint 3(i) and amended 3(ii))

- Mandatory: yes
- Guards against: attributing the `0` to anything other than the GP write path, including a guest-CPU store that races the DMA write.
- Evidence profile: strict (R1).
- Procedure: `[GPDMA] watch` lines; `W` at freeze via `inspect-jsrf.py memory <R1> <MEM32(0x001BA858)+0x810> 4`; `F` as in AC-DEFAULT.
- **Deciding line:** the **first** `[GPDMA] watch` line in log order with `va = B+0x810` and `payload=00000000`. AC-CLEAR is decided on that line alone. Later lines (for example, a later frame rewriting the status word, which reports `cas=fail observed=00000000`) are recorded only.
- PASS: the deciding line has **`cas=ok`** and `observed=00000003`, and records `dsp_addr` and `insns`. A successful compare-exchange (Device semantics 5) is the primary witness: the GP's own write performed the 3→0 transition, with no store in between. The `before`/`after` samples are recorded, not relied on. Corroboration only (recorded, not sufficient alone): `W = 0` at freeze. `F = 0` is recorded as an observation that the guest left the spin, and **anything after it is exploratory-grade, because it passes through PIO_FREE stub gates**.
- FAIL (→ R-CPU):
  - the deciding line has `cas=fail` and `observed=00000000`, i.e. the word was already 0 when the GP first wrote 0; or
  - there is no deciding line and `W = 0` at freeze.
- FAIL (→ R-NOCLEAR): there is no deciding line and `W = 3` at freeze.
- UNKNOWN: the deciding line has `cas=fail` and `observed` neither `0` nor `3`; or there is no deciding line and `W` is neither `0` nor `3`.
- Controls: known-good for the address computation — AC-NOCPU's control site. Known-bad — the baseline R1 artifact has no `[GPDMA]` line. The `cas` path is exercised by AC-FIX (e).
- Claim limits: one store by one core. No claim about the DSP program's other effects.

### AC-NOCPU — no synthetic ack exists, and the stop-path counters corroborate (Q1 condition 1; Advisor constraint 3(iii) and amended 3(ii))

- Mandatory: yes
- Guards against: a synthetic acknowledgement, and an instrumented guest-CPU stop-path store that contradicts AC-CLEAR.
- Evidence profile: strict (R1) + structural.
- Procedure: `[A4BSTORE]` lines in R1, grouped by `site`; R1 launch environment in the archive; AC-PORT's source and exe searches.
- PASS: `RECOMP_APU_DSP_ACK` is absent from the environment and AC-PORT's searches return zero (3(iii)). Corroboration: the control site `recomp_0005.c:6746` has `[A4BSTORE]` lines (≥ 1, `value=00000003`), and no other instrumented site has a line with `value=00000000` before the first `cas=ok` watch line (compare by log order).
- FAIL (→ R-CPU): the ack is present or a search hits; or an instrumented non-control site stored `0` to the watched VA before the first `cas=ok`.
- UNKNOWN: the control site has no line, so the counters are unwitnessed. This alone does not override a `cas=ok` from AC-CLEAR: the Session records it, and the Advisor decides whether it is material.
- Controls: the 6746 site is the known-good. The known-bad is the pre-change exe (no lines at all).
- Claim limits: the counters cover only the instrumented generated stores, and they are corroboration only. The stores are register-indirect, so no enumeration is complete. The attribution rests on AC-CLEAR's compare-exchange.

### AC-INPUTS — every input the GP read before the clearing write is guest-written or modelled (Q1 condition 2, Advisor constraint 4)

- Mandatory: yes
- Guards against: a stub value (GP/EP register zeros, `PIO_FREE`, stub-VP output, the trap-disabled counter) shaping the GP's result.
- Evidence profile: strict (R1) + source reading.
- Procedure: list every `[GPIN]` line before the first `[GPDMA] watch … payload=00000000` line; for each, the Session classifies provenance from the ported source and guest memory: **guest-written** (guest RAM the guest or XBE load wrote; DSP-internal memory filled by the bootstrap from the image counts as guest-written), **modelled** (a register the ported core computes from state it tracks; `MIXBUF` when `vp_active_voices = 0` is modelled zero), or **stub/unknown** (a shim constant, VP state from the stub resampler — `MIXBUF` with `vp_active_voices > 0` — the trap-disabled counter, anything unclassifiable). Table in the evidence record: kind, addr, first value, class, source line.
- PASS: every row guest-written or modelled. FAIL (→ R-EXPL-INPUT): any row stub/unknown. UNKNOWN: `[GPIN]` absent although AC-CLEAR passed (coverage not witnessed: at minimum the bootstrap's scratch read must appear as `DMA_READ`).
- Controls: `DMA_READ` of `G`/`B` during bootstrap is the known-good presence witness.
- Claim limits: classification is per value, per the profile doc's "one wait at a time" rule; not a data-flow proof.

### AC-PIO — `PIO_FREE` is only a gate at every direct read site of `0xFE820010` (Q1 condition 3)

- Mandatory: yes
- Guards against: the stub constant flowing into data the guest writes (Q1 reversal (a)); a PASS over a subset of the sites.
- Evidence profile: static (original-XBE disassembly, reconciled against the generated tree by value).
- **Frozen population: 28 instructions**, all in `DSOUND`, derived from `game\default.xbe` (SHA-256 `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C`) by `python -X utf8 scripts\inspect-jsrf.py find 0xFE820010` (28 hits, `0x001A2297` … `0x001A611D`):
  - `A1 moffs` (`mov eax,[0xFE820010]`, hit − 1), 10: `001A2296 001A22D0 001A3EB3 001A3FDB 001A4181 001A4E4C 001A5671 001A57CD 001A60B8 001A611C`.
  - `8B /r disp32` (`mov r32,[0xFE820010]`, hit − 2), 18: `001A2A7F(edx) 001A303D(edx) 001A30E4(edx) 001A3414(esi) 001A34EA(edx) 001A35AA(ecx) 001A3710(ecx) 001A3847(edx) 001A38F4(edx) 001A39AE(edx) 001A3A48(edx) 001A3B69(edx) 001A3C21(edx) 001A3F24(edx) 001A4242(edx) 001A4325(ebx) 001A4A34(edx) 001A579E(ebx)`.
- Procedure:
  1. **Population.** Run the `find` command and record its output verbatim with the XBE SHA-256. For each hit, decode the instruction starting at hit − 1 (`A1`) or hit − 2 (`8B /r` with `mod=00, r/m=101`). The instruction start is the one that has a `loc_<VA>: ;` label in the generated tree (step 2). At hit `001A3FDC` only the `A1` form decodes: the byte at hit − 2 is `0x65`, the displacement of the preceding `mov [esi+0x65],al`, so it is not an instruction start. The label confirms `001A3FDB`. A hit with neither decoding, or with no label, is recorded as `UNKNOWN`; it is never dropped.
  2. **Reconciliation by value, not text.** Parse every integer literal that is the operand of `MEM8(`/`MEM16(`/`MEM32(` in `src/recomp/gen/recomp_*.c`, decimal or hex, signed or unsigned, and normalise it mod 2^32. Keep those equal to `0xFE820010`. Map each one to the nearest preceding `loc_<VA>` label. The result must be exactly the 28 frozen VAs, one-to-one, with no leftovers in either direction. At baseline these are 10 spelled `MEM32(0xFE820010u)` and 18 spelled `MEM32(-25034736)`, all in `recomp_0005.c`.
  3. **Per site.** Disassemble the loop and every exit path with `inspect-jsrf.py disasm`. Record the threshold form and N, then trace the polled register `r` on every exit path until `r` is overwritten.
  4. **Exit trace.** From each loop exit, apply the exit-trace rule below to every path, and record the trace per site as a list of instruction VAs with the tracked set at each call, `ret` and `jmp` out of the function.
  5. **Indirect-access supporting evidence.** Record `inspect-jsrf.py find` for `0xFE800000` (at planning: 7 hits, `001A2E4A 001A2E5F 001A2E65 001A2EDE 001A2F80 001A2FA4` in `DSOUND` and **`0022DB72` in `.data`**), `0xFE820000` (0 hits) and `0x00020010` (1 hit, `.rdata 001E4888`). For each `DSOUND` hit, show from disassembly that the base/index registers come from the table at `0x001B9ECC`, and record that table's entries as data (`0x2054`…`0x2074`, voice-list registers). For `0022DB72` and `001E4888`, show that nothing references them in a way that can form `0xFE820010`, or that they are not operands at all. Any of these that can reach `0xFE820010` makes the criterion `UNKNOWN`.
- **Exit-trace rule ("not used after the loop").** The trace keeps a tracked set `T`: the registers and stack slots that may hold the polled value, or a value derived from it. It starts as `T = {r}` at the loop exit. Each instruction on the path is applied in execution order:
  - **Derivation.** An instruction that reads a member of `T` and writes a register adds that register to `T`.
  - **Overwrite.** An instruction that writes a member register with a value not derived from `T` removes it (for example `xor r,r`, `mov r,imm`, `mov r,[mem]` with no tracked operand, or `pop r` of an untracked slot).
  - **Uses (→ FAIL):** a member of `T` flows into:
    - the value or address of a guest store or MMIO store;
    - a branch condition (flags set by an instruction that reads `T`);
    - an address computation;
    - the argument of a resolved import that reads it (below).
  - **Stack.** `push t` with `t ∈ T` adds that stack slot to `T`, and the matching `pop x` makes `x` tracked. A callee-save `push`/`pop` pair is not a use. Any other read of a tracked slot is a use, unless shown to be the matching `pop`. Slots of a frame that has returned are dropped.
  - **Direct call** (`call rel32`) with `T` non-empty: the trace continues into the callee with the same `T`, under the same rule. At the callee's `ret` it resumes at the return address with the callee's final `T`.
  - **Import call** (`call [slot]`, where `slot` is a kernel thunk slot):
    - *Input.* The import cannot read `r` when `r ∉ {ecx, edx}` and no tracked stack slot lies in its argument area. Otherwise the executor resolves the import from the XBE: ordinal = `MEM32(slot) & 0x7FFFFFFF` at the **exact slot VA the instruction references** (neighbouring slots may hold the same value). It then maps the ordinal via toolkit `src/kernel/kernel_thunks.c`, takes the prototype from `src/kernel/kernel.h`, and applies its convention: `__fastcall` reads `ecx` (arity ≥ 1), then `edx` (arity ≥ 2), then the stack; `__stdcall` reads the stack only. An argument read of a `T` member is a use. The site is `UNKNOWN` only if resolution fails.
    - *Clobber convention after the import.* The import neither removes nor adds members of `T`. The volatile registers `eax`/`ecx`/`edx` are **not** assumed overwritten, so a member stays in `T`. The non-volatile `ebx`/`esi`/`edi`/`ebp` are preserved and keep their state. Exception: for a prototype with a non-`VOID` return that read no `T` member, `eax` is removed from `T`.
    - *Observed at planning* (`docs/reviews/a4b-r2-session-checks.md`): `sub_001A1BAF` calls `[0x1C4004]`, which holds `0x800000A1` = ordinal 161 → `xbox_KfLowerIrql`, `VOID __fastcall (KIRQL NewIrql)`, with its argument in `cl` (`001A1BB8 mov cl,[esi]`). So `eax`, `edx`, `ebx`, `esi` and `edi` are not read, and every `T` member survives the call. `0x1C3FF8` holds the same value and is not the referenced slot.
  - **Indirect call or jump** (`call reg`, `call [mem]` other than a thunk slot, `jmp reg`, `jmp [mem]`) with `T` non-empty: `UNKNOWN`, unless every target is resolved from the XBE and traced.
  - **Leaving the function with `T` live.** A `ret`/`ret n` reached with any register in `T` is a **flow into the caller**; a callee-save `pop` just before it overwrites that register and so removes it. So is a `jmp` out of the function with `T` non-empty. The trace continues at the return address in **every** caller: every `call rel32` to the function's entry in the XBE, cross-checked against the generated tree's direct calls to `sub_<entry>`. It is `UNKNOWN` when:
    - the entry VA appears in the XBE as an immediate or data value (the function may be called indirectly);
    - the callers cannot be enumerated;
    - or the trace would continue beyond **three** caller levels above the site's function.
  - **Termination.** A path ends when `T` is empty. It ends in `UNKNOWN` at any instruction the executor cannot decode or resolve.
  - **Outcome.** The site passes when every path ends with `T` empty and no use. It FAILs on any use, and is `UNKNOWN` otherwise.
- PASS: **all** of the following hold:
  - the `find` count = **28**;
  - step 2 yields exactly the 28 frozen VAs, one-to-one;
  - **every** one of the 28 is evaluated;
  - each is a threshold re-poll (`(v & ~3) < N` or `(v >> 2) < reg`, jumping back to the read on below) whose value is not stored inside the loop and is not used after the loop;
  - step 5 shows no other path to `0xFE820010`.

  FAIL (→ R-PIO-DATA): any site uses the value as data. UNKNOWN: a count other than 28, a reconciliation mismatch, any site not evaluated or not resolved (including a callee not shown), or any step-5 item unresolved. UNKNOWN is never PASS.
- Controls: known-good **for the loop-shape classification (step 3)** — the Session's three sampled sites (`001A2296`, `001A4181`, `001A611C`) in `a4b-q1-advisor-ruling.md`. Their exit traces are evaluated under the exit-trace rule like every other site, not presumed. Known-bad for the reconciliation — a text grep of `MEM32\(0xFE820010u\)` alone finds 10, not 28, and must be reported as a mismatch.
- Claim limits: static. **It covers only the 28 direct reads of `0xFE820010` and cannot exclude register-indirect or computed access to that address, beyond the supporting evidence in step 5.** It says nothing about timing or about whether the stub value is true.

### Decision rows (evaluate in order; first match wins)

- **R-PORT-FAIL:** AC-PORT, AC-FIX or AC-LIC cannot be made to pass within two attempts on the same root cause → toolkit and game reverted to baseline (no GPL code left in tree) → Planner re-plans (candidate: a port-only packet `A4b-port`).
- **R-DEFAULT-REGRESS:** AC-DEFAULT FAIL → revert → Planner.
- **R-UNKNOWN:** after the one R1 rerun, any gate fails, or any AC is UNKNOWN, or AC-CLEAR PASS while AC-BOOT or AC-RUN is not PASS (the evidence is inconsistent) → evidence insufficient → Planner re-plans with the failing gate/AC as brief.
- **R-CPU:** AC-NOCPU FAIL, or AC-CLEAR FAIL routed to R-CPU (`cas=fail` with `observed=0`, or no `payload=0` watch line with `W = 0` at freeze) → the 0 did not come from the GP (Q1 reversal (c)) → **FAIL**, not a narrowed claim → Advisor.
- **R-PIO-DATA:** AC-PIO FAIL → the claim needs a modelled `PIO_FREE` first → Advisor; landed code handled as R-LAND.
- **R-NOBOOT:** AC-BOOT FAIL → **R-LAND** applies → next: `A4c` discovery of the bootstrap/SGE path.
- **R-NOFRAMES:** AC-BOOT PASS, AC-RUN FAIL on frames → **R-LAND** → next: `A4c` discovery of the FE/SE frame gate after GPRST.
- **R-NOEXEC:** AC-BOOT PASS, AC-RUN FAIL on `insns = 0` (frames run, the core retires nothing) → **R-LAND** → next: `A4c` discovery of the GP run/halt state after bootstrap (reset/halt bits, the pinned `mcpx_apu_dsp_frame` start condition; `[GPRUN] pc`/`halt` is the brief).
- **R-NOCLEAR:** AC-BOOT and AC-RUN PASS, AC-CLEAR FAIL → R-NOCLEAR (no deciding line, `W = 3`) → **R-LAND** → next: `A4c` discovery of what the GP program waits on (its `[GPIN]` inventory is the brief).
- **Coverage (AC-RUN × AC-CLEAR, with AC-BOOT PASS and every earlier row not matched):** each combination matches exactly one row.

  | AC-RUN \ AC-CLEAR | PASS | FAIL → R-CPU | FAIL → R-NOCLEAR | UNKNOWN |
  |---|---|---|---|---|
  | PASS | R-EXPL-INPUT or R-PASS (decided by AC-INPUTS; NOCPU FAIL was already R-CPU) | R-CPU | R-NOCLEAR | R-UNKNOWN |
  | FAIL, no frames | R-UNKNOWN (inconsistent) | R-CPU | R-NOFRAMES | R-UNKNOWN |
  | FAIL, `insns = 0` | R-UNKNOWN (inconsistent) | R-CPU | R-NOEXEC | R-UNKNOWN |
  | UNKNOWN | R-UNKNOWN | R-UNKNOWN | R-UNKNOWN | R-UNKNOWN |

  With AC-BOOT FAIL: AC-CLEAR PASS or any UNKNOWN → R-UNKNOWN; AC-CLEAR → R-CPU → R-CPU; otherwise R-NOBOOT.
- **R-EXPL-INPUT:** AC-BOOT, AC-RUN, AC-CLEAR, AC-NOCPU PASS; AC-INPUTS FAIL → the clearing is observed but **exploratory for the named input**; A4b's claim is **not** satisfied → **R-LAND** → Advisor decides whether that input needs its own model packet first.
- **R-PASS:** every AC PASS → A4b accepted with exactly the "Establishes" claim → next: the `PIO_FREE` model packet before any strict liveness/boot criterion past the spin; the next stop after `loc_001A18D0` is recorded as an exploratory-grade observation only.
- **R-LAND (pre-frozen landing-only outcome; never chosen during execution):** applies only when referenced above, and requires AC-PORT, AC-LIC, AC-FIX and AC-DEFAULT all PASS; the core stays in tree (reachable only under `RECOMP_APU_TRAP`). **It does not satisfy A4b's claim.** If any of those four fails, R-PORT-FAIL applies instead.

### Closure

- Evidence index (in `docs/reviews/a4b-execution-evidence.md`): each AC → artifact path + SHA-256 (`result.json`, `stacks.txt`, `jsrf_run.log`, `process.dmp` for R1/R0; `ctest.txt`; pin record; exe SHA-256; toolkit and game commits) → result → selected row.
- Instrumentation stays committed only behind `RECOMP_APU_TRACE` (R0's zero counts are its witness); removing the generated-chunk observation calls later needs no packet, keeping them after a regeneration requires re-applying them.
- Follow-ups to record, not to do: the classifier's treatment of the now-inert `RECOMP_APU_DSP_ACK` name (retired-override registry); the game repository has no licence file of its own; EP routing; GP→CPU interrupt delivery.
- Post-review edits reopen affected criteria. An unrelated next stop is recorded as a follow-up; scope is not expanded.

### Author's adequacy block (Planner; independent review still required, §5.1.5)

- **Verdict (author):** executable as one packet, but large. Scale risk is carried by R-PORT-FAIL and R-LAND, both frozen in advance.
- **Blocking (author-known):** none.
- **Advisory:**
  1. The pinned `gp_ep.c` GPRST transition and the name of the GP-enable preference are inferred from the brief/Advisor memory, not read. The Session records them from the pinned source and does not add them to criteria.
  2. After routing, the guest's GP bulk read at `0x001A1B41` (src `0xFE830200`, `apu_core.c:637-641`) becomes a modelled read of GP XMEM. It is after the spin and outside the claim.
  3. AC-NOCPU does not instrument block copies (`rep movs`) into `B+0x800..`. The primary witness is AC-CLEAR's compare-exchange, per the amended Advisor constraint 3(ii).
  4. Hand edits to the generated chunks are lost on regeneration. They are labelled for re-application.
- **Observed by the author:** `apu_dsp.c` (full); `apu_core.c:454-468, 498-530, 598-671`; `apu_state.h:43-122`; `apu_regs.h` GP and FE/SE masks; `apu_shim.h:96-105`; `NOTICE`; `LICENSES/README.md`; `apu/CMakeLists.txt`; game `main.c:249-254`; `xbox_memory_layout.c:1173, 1529-1563`; `kernel_bridge.c:1700-1707`; `recomp_0005.c:6515-6530, 6735-6751, 8085-8098`; XBE disasm `0x001A1747-0x001A1768`, `0x001A1F8C-0x001A1FAA`; R1 log lines L3126-L3287 (GP/SE writes); generated `+0x810 =` stores.
- **Inferred:** nothing load-bearing. The draft's two inferences — the pinned `gp_ep.c` GPRST transition and the GP-enable preference name — were resolved from the pinned source by the Session and are recorded in `docs/reviews/a4b-xemu-pin.md`; they are recorded, not added to criteria.
