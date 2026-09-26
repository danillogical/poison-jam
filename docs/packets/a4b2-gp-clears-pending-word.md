## A4b2 — the ported GP engine clears the title's pending word in a strict run

### Sketch (A4b2-r4, for the Advisor shape preflight — body below is still r3 until PROCEED)

1. **Claim:** unchanged from r3 (one STRICT trap+trace run: boot, run, GP clear, no CPU/ack, inputs). Re-bind only; not a redesign.
2. **Class:** change. Grounds: §5.4(2) `PREMISE_CHANGED` (A4b1-r4 replaced the `[GPIN]` mechanism, added `sge0_va`, and re-baselined the toolkit to `M`→`3a3c7c1`). r3 was never ADEQUATE, so this is a draft revision.
3. **Edit 1: P2.** Repin to `A4b1-r4` at toolkit `3a3c7c1`. The field map uses r4 AC-FIX cases: `at_clear` (xi); PERIPH/FIFO/MIXBUF/DMA (viii); `BOOT_SCRATCH_READ` (vi); out-of-universe (x); `sge0_va` (vii), both placements.
4. **Edit 2: AC-BOOT.** Compare `sge0_va = B`, never raw `sge0`. **Also demote** the r3 PASS conjunct "last `0x02040` write = `803CC000`" to a recorded value. Under P-F, at `M` GPSADDR may hold the physical form `003CC000`, which would give a deterministic false UNKNOWN. **The boundary note missed this item.**
5. **Edit 3: AC-INPUTS.** Rewritten on the first `[GPIN] at_clear` block, which A4b1 emits at the freeze.
   - It is **evaluated only when AC-CLEAR is PASS** (§6.1 6b).
   - **UNKNOWN** if any of these holds: the block is missing; its header `seq ≠ GP_CLEAR.seq`; `out_of_universe ≠ 0`; or `boot_scratch_read ≠ 1`.
   - **FAIL** on a stub read: any PERIPH offset with reads > 0 outside the six offsets A4b1 classified as modelled (`0x33, 0x45, 0x54–0x57`); FIFO slots 0–1 with reads > 0; MIXBUF `reads_while_stub > 0` or the `mixbuf_stub_read` flag; or DMA reads in `DEVICE`/`OTHER_MAPPED`.
   - **UNKNOWN** if FIFO slots 2–5 are non-zero.
   - **PASS** otherwise. `LOW_RAM`/`CONTIG` count as guest-written.
6. **Edit 4: rows/exhaustiveness.** When AC-CLEAR is not PASS, AC-INPUTS is `NOT EVALUATED`, which is not UNKNOWN. The R2-NOCLEAR brief becomes the running `[GPIN] summary` block.
7. **Edit 5: EP (the carried-forward input) → a structural precondition P3, not a lead.**
   - The EP is unreachable at `3a3c7c1`. EP MMIO is unrouted (`apu_core.c:648`), `ep_ops` has no caller, and `ep.regs[EPRST]` is written only in `ep_write` (`gp_ep.c:557-559`). The state is calloc'd (`apu_core.c:543`), so the EP never runs (`gp_ep.c:650`).
   - If the EP were reachable, the ledger could not tell an EP `GP_CLEAR` from a GP one, which is a false PASS.
   - P3 is a toolkit text check that EP routing is absent. It fails closed.
8. **Unchanged:** AC-DEFAULT2 (oracle repointed to A4b1 R0 `20260926-010303-411`), AC-RUN, AC-CLEAR, AC-NOCPU, the step-1 sites, gates, and runs.
9. **Unknowns (subject of execution, not planning):** whether R1 boots, runs, and clears, and which inputs it reads.
10. **Experiment:** unchanged. R1 (trap+trace) and R0 (default), 30 s each, with one R1 rerun on UNKNOWN.
11. **Questions for the Advisor:**
    - **(a)** The printed `at_clear` block is fixture-tested only on the snapshot (xi) plus a line count. Its field values are covered through the shared `emit_gpin_block` emitter, which the `summary` line-vs-snapshot check exercises, not by a direct at_clear line-vs-snapshot check. Is that admissible as a decision input?
    - **(b)** PERIPH `0xFFFFB3`: the pin returns a constant `0` (`dsp.c:57`, `// core->num_inst ??`), and A4b1 classified it as modelled. Does A4b2 inherit that classification, or treat it as a shim constant?

**Class:** change   **Contract revision:** `A4b2-r3`   **Status:** draft
**r3 delta (not a redesign; `docs/reviews/a4b1-a4b2-r2-adequacy-review.md`):** P2 repinned to `A4b1-r3` with a per-decision-field map to `AC-FIX` cases; `AC-INPUTS` decides from `A4b1-r3`'s lossless `[GPIN]` accounting and is UNKNOWN on overflow or missing accounting; step 1 cites sites by content/VA. `AC-CLEAR`'s logic is unchanged.
**Split decision:** `A4b` is split into `A4b1` (port, ledger, licence, fixtures, unchanged default path; no guest-run claim) and `A4b2` (this packet: the strict trap+trace run — boot, run, clear, no-CPU, inputs), because build, ctest and one default run settle the port's scale risk, so it should not wait for the run criteria or be reviewed with them.
**Governing requirement:**
- `A4a-r2` row `O-6` (`docs/packets/a4a-dsp-pending-word.md`).
- Q1 ruling `docs/reviews/a4b-q1-advisor-ruling.md`. Conditions 1, 2 and 4 are discharged here; condition 3 was discharged by `A4p`.
- Checkpoint-40 constraints `docs/reviews/a4b-planning-rulings.md`.
- **Watch-ledger ruling `docs/reviews/a4b-watch-ledger-ruling.md` §(b).** This is the authority for AC-CLEAR, AC-NOCPU and R2-UNATTRIBUTED.

**Depends on:** `A4b1-r3` (ACCEPTED, `R1-PASS`); `A4p-r1` (ACCEPTED, `O-GATE`); `A4a-r2` (ACCEPTED); `A3a-r25` (ACCEPTED).
**Baseline:** game and toolkit at `A4b1`'s accepted commits, plus the game commit that adds this packet. Both trees are clean. The Session records the SHAs and the exe SHA-256.
**Revision log:** `docs/reviews/a4b-revision-history.md` (non-authoritative).

### Preconditions (checked before any step; if any fails, stop and select no row)

- **P1.** `A4p-r1` was ACCEPTED with row **`O-GATE`** on XBE SHA-256 `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` (`docs/reviews/a4p-r1-acceptance-review.md`). `Get-FileHash game\default.xbe` must equal that hash.
  - What `A4p` established: all 28 direct reads of `0xFE820010` are threshold re-polls, and the polled value reaches no use. So `PIO_FREE` gates reachability and does not shape guest-written data.
  - Claim limits carried here:
    - `A4p` rests on an **inferred** x86 register-convention premise, checked only at the boundaries it used.
    - It is **blind to register-indirect or computed access** to `0xFE820010`, beyond its E3 evidence.
    - It says nothing about **timing**, or about whether **`0x80` is the true device value**.
- **P2.** `A4b1-r3` was ACCEPTED with `R1-PASS`, and the toolkit HEAD equals `A4b1`'s accepted toolkit commit. `A4b1-r3` defines (Device semantics 6) and its AC-FIX asserts, **as printed line equal to snapshot** (AC-FIX line-vs-snapshot rule), every field this packet decides on:
  - `GP_CLEAR.va` (= `W_va`, not the DMA destination start): case (i); `GP_CLEAR.observed`: (e), (i); `GP_CLEAR.insns` (= `gp_insns` at the event): (i); `GP_CLEAR.dsp_addr`: (i); `GP_CLEAR.seq`: (i);
  - `CPU_ANCHOR.seq` and its ordering against `GP_CLEAR.seq`: (i);
  - `CPU_ZERO.site`/`.seq`: (iv), (v); `CPU_ZERO_OVERFLOW` latch and counter: (v);
  - `GP_ZERO_OVER_OTHER`: (ii); `GP_PARTIAL`: (iii);
  - counts-line `boots`/`gp_frames`/`gp_insns` equal to the ledger: (vi);
  - `[GPIN]` accounting, per kind classified by AC-INPUTS: `DMA_READ` — (vi), (viii); `PERIPH` — (viii); `FIFO_READ` — (viii); `MIXBUF` with `vp_active_voices` `0` and `> 0` — (viii); the cut-off at `GP_CLEAR` and its reset — (viii); counts `gpin`/`GPIN_OVERFLOW` and the `GPIN_OVERFLOW` latch — (ix);
  - `[GPBOOT]` `gprst`/`prev`: (vii).

  This packet changes none of them.

### Motivating evidence

- **A4a R1** (`logs/runs/20260924-191833-331-a4a-r2-trap-trace`): STRICT, trap+trace.
  - The guest wrote GPSADDR `0x02040 = 803CC000` (L3188), GPSMAXSGE `0x020D4 = 8` (L3189), and GPRST `0x3FFFC = 1` (L3190), then `= 3` (L3201).
  - The FE/SE frame gate opened at L3286-3287, after GPRST=3.
  - `MEM32(803CC000) = 803C0000 = B`, and `[B, B+0x5CC)` equals XBE file offset `0x1A7D60`. That range is image `I`.
  - `MEM32(B+0x810) = 3`.
  - The run ended `diagnostic_deadline`, spinning at `loc_001A18D0`.
- **A4a R0** (`logs/runs/20260924-191906-091-a4a-r2-default`): STRICT, default. Same stop.
- **Guest stores to the word** (XBE disassembly, read):
  - the control store `0x001A18CE mov [ebx],eax` (`recomp_0005.c:6746`, immediately before `loc_001A18D0` at `0x001A18D0`);
  - `0x001A1751 and dword [edi+0x810],0` (`recomp_0005.c:6524`);
  - `0x001A1FA7 mov [edi+0x10],ebp` with `edi = MEM32(0x1BA858)+0x800` (`recomp_0005.c:8088`).
- **Hook order** (`apu_mmio_hook.c:283` handles the access, `:300` prints it): `[APUMMIO] write 0x3FFFC = 00000003` is printed **after** the synchronous bootstrap. So the `[GPBOOT]` block precedes it (`docs/reviews/a4b1-a4b2-adequacy-review.md`, A4b2 B1).

### Claim and boundaries

- **Establishes (on R2-PASS only):** in one STRICT run with `RECOMP_GPU_ACK=0` and `RECOMP_APU_TRAP=1`, modelled GP execution of the guest's own command satisfied the wait at `loc_001A18D0`. Specifically:
  - the guest's GPRST write bootstrapped the GP from the scratch pages named by the guest's SGE table, and the loaded words equal image `I`;
  - the GP retired instructions in APU frames;
  - the GP's own DMA write path exchanged the `3` at `B+0x810` for `0`. The ledger's `GP_CLEAR` latch witnesses this, after the guest's anchor store of `3`;
  - no synthetic ack exists;
  - every input the GP read before that write was guest-written or modelled.
- **Does not establish:**
  - Everything Q1 condition 4 lists. The guest's arrival at the spin, and everything after it, pass through `PIO_FREE` stub gates and are exploratory-grade.
  - Boot progress, liveness, DirectSound init success, audio output, or behaviour under a modelled VP FIFO.
  - Real-hardware state or timing at the spin. "The guest leaves `loc_001A18D0`" is an observation only.
  - Anything beyond the `A4p` limits carried in P1.
  - DSP56300 instruction-level correctness.
  - Physical/VA aliasing, EP execution, or GP→CPU interrupts.
  - A complete enumeration of CPU stores to the word. The CPU instrumentation is a text-matched lead; attribution rests on the compare-exchange alone.
- **Non-goals:** any toolkit change; a `PIO_FREE` model; EP; audio; regeneration; changing `docs/jsrf-run-profiles.md`.

### Readiness

- **Tools:** those `A4b1` verified: `build-jsrf.py`, ctest, `run-jsrf.py --profile strict`, `check-run-profile.py`, `check-dump-mapping.py`, and `inspect-jsrf.py memory|disasm`.
- **Stop if:**
  - P1 or P2 fails;
  - any of `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED` or `JSRF_ABI_CONTINUE` is in a launch environment;
  - the runner refuses `--profile strict`;
  - any **toolkit** change would be needed (→ Planner; that is an `A4b1` defect);
  - any step would set or clear `+0x810` other than through GP execution.

  A session that ends mid-execution selects no row.

### Execution

**Steps**

1. **Game-side forwarding edits** (hand-applied; each is commented `A4b observation — re-apply after regeneration`; no value changes).
   - In `src/diagnostics.c`, define `void jsrf_watch_store(uint32_t site_va, uint32_t target_va, uint32_t value)`. It is a **thin forwarder**:
     - On its first call, it calls the toolkit's `apu_watch_set_anchor_site(0x001A18CEu)` once.
     - It then calls `apu_watch_cpu_store(site_va, target_va, value)`.
     - It has no filtering, no cap, no output and no decision logic.
     - The toolkit prototypes are declared `extern` in `diagnostics.c`.
   - Immediately before each of the following stores, insert `jsrf_watch_store(<site VA>, <target VA>, <value>)`. Sites are located **by content and guest site VA** (the generated label/instruction for that VA), not by line number; the baseline line numbers are hints only:
     - control site `0x001A18CE` `mov [ebx],eax` — the `MEM32(ebx) = eax;` immediately before `loc_001A18D0: ;` in `recomp_0005.c` (baseline ≈ `:6746`);
     - `0x001A1751` `and dword [edi+0x810],0` in `recomp_0005.c` (baseline ≈ `:6524`);
     - `0x001A1FA7` `mov [edi+0x10],ebp` in `recomp_0005.c` (baseline ≈ `:8088`);
     - every other generated store that textually matches `MEM32\(.* \+ 0x810\) =`; at baseline these are in `recomp_0000.c` (≈ `:135283, 135406, 135871`). The Session records each site's VA from its generated label.
   - Each edited chunk gets one file-scope `extern void jsrf_watch_store(uint32_t, uint32_t, uint32_t);`. No generated header changes.
2. Build, then run ctest.
3. Make runs R1 and R0 (below).
4. Record everything in `docs/reviews/a4b2-execution-evidence.md`.

**Runs** (from the game root, 30 s each, archived under `logs/runs/`):

```powershell
Remove-Item Env:\RECOMP_APU_DSP_ACK,Env:\RECOMP_AC97_READY,Env:\JSRF_ALLOW_UNRESOLVED,Env:\JSRF_ABI_CONTINUE,Env:\RECOMP_APU_TRAP,Env:\RECOMP_APU_TRACE -ErrorAction SilentlyContinue
$env:RECOMP_GPU_ACK='0'; $env:RECOMP_KERNEL_LOG_BUDGET='100000'
$env:RECOMP_APU_TRAP='1'; $env:RECOMP_APU_TRACE='1'
python -X utf8 scripts\run-jsrf.py --seconds 30 --profile strict --label a4b2-gp-trap-trace   # R1 (claim run)
Remove-Item Env:\RECOMP_APU_TRAP,Env:\RECOMP_APU_TRACE
python -X utf8 scripts\run-jsrf.py --seconds 30 --profile strict --label a4b2-default         # R0
```

If R1 selects R2-UNKNOWN, rerun R1 once with identical settings; the second result stands.

- **Write scope (game only):**
  - `src/recomp/gen/recomp_0000.c` and `recomp_0005.c`: the step-1 calls and their `extern` declarations only;
  - `src/diagnostics.c`: the forwarder only;
  - `docs/reviews/a4b2-*.md`.

  Nothing else.
- **Build/run owner:** the Session.

### Gates (checked before any AC; any failure → R2-UNKNOWN)

- **G1:** `check-run-profile.py` prints `STRICT` for R1 and R0.
- **G2:** `check-dump-mapping.py` gives `matches ≥ 1` and `content-mismatch: 0` for both.
- **G3:** `result.json` is readable for both.
- **G4:** in `src/recomp/gen/recomp_0005.c` **at the build commit (after the step-1 edits)**:
  - `loc_001A18D0: ;` occurs exactly once, at line `L`;
  - a line in `L+1..L+3` contains `goto loc_001A18D0;`;
  - the non-blank line before `L` is `MEM32(ebx) = eax;`, optionally preceded on the same line or the line above by the step-1 call.

### Definitions (all read from R1 unless stated)

- **`B`:** `MEM32(0x001BA858)` in the run's dump.
- **`Wf`:** `MEM32(B+0x810)` in the dump, read with `inspect-jsrf.py memory`.
- **`F`:** the number of matches in `stacks.txt` of `sub_001A1769\+0x[0-9A-Fa-f]+ .*recomp_0005\.c:(L|L+1|L+2|L+3)\b`, with `L` taken from G4 of **this** build. The A4a R0 oracle's `F` used that build's own line numbers, and the two are not compared by line.
- **Latch:** a `[GPWATCH] latch class=<C> …` line. `<C>.seq` and the other fields are read from it. Each CPU site has at most one `CPU_ZERO` latch. "Non-control" means `site ≠ 001A18CE`.
- **Counts line:** a `[GPWATCH] counts …` line. "Ledger alive" means at least one counts line exists.
- **Cleared:** `Wf = 0` or `F = 0`.

### AC-DEFAULT2 — the default strict path is still unchanged with the forwarding edits

- **Mandatory:** yes.
- **Guards against:** the step-1 edits leaking into the default run.
- **Evidence profile:** strict (R0).
- **Procedure:**
  - take R0's `result.json`, `Wf` and `F`;
  - count the lines in R0's `jsrf_run.log` that match `\[APUMMIO\]` or `\[GP(BOOT|RUN|DMA|IN|WATCH)\]`.
- **Oracle:** A4a R0 (`20260924-191906-091`), and `A4b1`'s accepted R0: `diagnostic_deadline`, `W = 3`, `F ≥ 1`.
- **PASS:** `outcome = diagnostic_deadline`, `Wf = 3`, `F ≥ 1`, and both counts are 0.
- **FAIL:** any of these differs.
- **Controls:** known-good: `A4b1`'s R0. Known-bad for the counts: R1.
- **Claim limits:** one run.

### AC-BOOT — the guest's GPRST write bootstraps the GP from the title's image

- **Mandatory:** yes.
- **Guards against:** a GP running code other than the title's image.
- **Evidence profile:** strict (R1).
- **Procedure:**
  - Take the first `[GPBOOT]` header and its 64 `pram` lines.
  - Take the first `[APUMMIO] write 0x3FFFC` line after that block, and the last `[APUMMIO] write 0x02040` line before it.
  - Compare PRAM word `i`, for `i < 0x173`, with `LE32(default.xbe[0x1A7D60 + 4i]) & 0xFFFFFF`. The Session records this one-off comparison and its output.
- **Oracle:** the original XBE bytes.
- **PASS:** all of the following hold:
  - a `[GPBOOT]` header exists;
  - it has `gprst=00000003`, and a `prev` that has `GPRST` or `GPDSPRST` clear (the pinned bootstrap transition);
  - the first `0x3FFFC` write after the block has value `00000003`;
  - the last `0x02040` write before it is `803CC000`;
  - `sge0 = B`;
  - all `0x173` words are equal.
- **FAIL (→ R2-NOBOOT):** either
  - there is no `[GPBOOT]` line, although a `write 0x3FFFC = 00000003` line exists; or
  - a complete `[GPBOOT]` block exists with `sge0 ≠ B`, or with any word different.
- **UNKNOWN:** any other case. For example: fewer than 64 `pram` lines; `gprst`/`prev` do not match the transition; no `0x3FFFC` line after the block; or no `write 0x3FFFC = 00000003` line at all.
- **Controls:** known-bad: the same comparison at file offset `0x1A7D64` must differ at word 0.
- **Claim limits:** shows what was loaded, not that it executes correctly.

### AC-RUN — GP instructions retire in APU frames after the bootstrap

- **Mandatory:** yes.
- **Guards against:** a core that is loaded but never clocked (frame gate closed, test-tone path, or halted).
- **Evidence profile:** strict (R1).
- **Procedure:** the counts lines. `gp_frames` and `gp_insns` are uncapped counters, and a counts line is emitted at the first GP frame after each bootstrap (`A4b1` Device semantics 6). `[GPRUN]` lines are observation only: `pc`, `halt` and `tone` are recorded for the next brief.
- **PASS:** some counts line has `boots ≥ 1`, `gp_frames ≥ 1` and `gp_insns > 0`.
- **FAIL (→ R2-NOFRAMES):** some counts line has `boots ≥ 1`, and every counts line has `gp_frames = 0`.
- **FAIL (→ R2-NOEXEC):** some counts line has `gp_frames ≥ 1`, and every counts line has `gp_insns = 0`.
- **UNKNOWN:** any other case, including no counts line with `boots ≥ 1`.
- **Controls:** waived. The retirement count is the ported core's own counter, whose meaning is inherited from the pin; its path is exercised by `A4b1` AC-FIX (vi).
- **Claim limits:** retirement, not correctness.

### AC-CLEAR — the GP's own DMA write made the 3→0 transition (ledger decision)

- **Mandatory:** yes.
- **Guards against:**
  - attributing the `0` to anything other than the GP write path;
  - a pre-command GP write-back read as the clear;
  - blaming the CPU without a positive witness.
- **Evidence profile:** strict (R1).
- **Procedure:** the latch lines, the counts lines, `B`, `Wf` and `F`. `GP_ZERO_OVER_ZERO` and `GP_NONZERO_OVER` are recorded only, and never decide. Evaluate in this order; the first match wins:
  1. **UNKNOWN** if any of the following holds:
     - the ledger is not alive;
     - `CPU_ZERO_OVERFLOW` is latched;
     - there is no `CPU_ANCHOR` latch (instrumentation broken);
     - `GP_CLEAR` is latched with `GP_CLEAR.seq < CPU_ANCHOR.seq` (the exchanged `3` came from a store that was not instrumented).
  2. **PASS** if `GP_CLEAR` is latched with all of:
     - `va = B+0x810`;
     - `observed=00000003`;
     - `insns > 0`;
     - `dsp_addr` recorded;
     - `CPU_ANCHOR.seq < GP_CLEAR.seq`.

     `Wf` and `F` are recorded as corroboration only.
  3. **UNKNOWN** if `GP_CLEAR` is latched but step 2 fails, either because `va ≠ B+0x810` or because `insns = 0`.
  4. **R2-UNATTRIBUTED** (not PASS, and not blamed on the CPU): there is no `GP_CLEAR`, the word is cleared, and `GP_PARTIAL` or `GP_ZERO_OVER_OTHER` is latched. A GP writer the CAS does not protect competes for the clear.
  5. **FAIL → R2-CPU** (positive witness only): there is no `GP_CLEAR`, the word is cleared, and some non-control `CPU_ZERO` latch has `seq > CPU_ANCHOR.seq`.
  6. **R2-UNATTRIBUTED:** there is no `GP_CLEAR`, the word is cleared, and no non-control `CPU_ZERO` latch has `seq > CPU_ANCHOR.seq`. Absence of a witness is never a positive attribution.
  7. **FAIL → R2-NOCLEAR:** there is no `GP_CLEAR`, `Wf = 3`, and `F ≥ 1`.
  8. **UNKNOWN** otherwise, for example when `Wf ∉ {0,3}` and `F ≥ 1`.
- **Controls:**
  - `A4b1` AC-FIX exercises every class used here: `GP_CLEAR` in (e) and (i), `GP_ZERO_OVER_OTHER` in (ii), `GP_PARTIAL` in (iii), `CPU_ZERO` in (iv), `CPU_ZERO_OVERFLOW` in (v), and `CPU_ANCHOR` ordering in (i).
  - Known-bad for the old design: AC-FIX (i).
- **Claim limits:** one store by one core; nothing about the DSP program's other effects.

### AC-NOCPU — no synthetic ack exists, and no instrumented CPU zero contests the clear

- **Mandatory:** yes.
- **Guards against:** a synthetic acknowledgement, and an instrumented CPU zero racing the GP clear.
- **Evidence profile:** strict (R1) + structural.
- **Procedure:**
  - Take R1's launch environment from the archive.
  - Run `Select-String -Path <toolkit>\src\apu -Pattern 'RECOMP_APU_DSP_ACK|dsp_ack_' -Recurse`.
  - Search the strings of the exe for `RECOMP_APU_DSP_ACK`.
  - Read the `CPU_ANCHOR`, `CPU_ZERO` and `GP_CLEAR` latches.
- **A contested zero** is a non-control `CPU_ZERO` latch with `CPU_ANCHOR.seq < seq < GP_CLEAR.seq`. Two other cases are recorded only:
  - a `CPU_ZERO` latched before the anchor, which the anchor's `3` superseded (a successful CAS proves `3` was the last value landed);
  - a `CPU_ZERO` latched after `GP_CLEAR`, for example the stop path clearing an already-zero word.
- **PASS:** `RECOMP_APU_DSP_ACK` is absent from the environment, both searches return zero hits, and there is no contested zero.
- **FAIL (→ R2-CPU):** the ack is present, or a search hits.
- **UNKNOWN:** a contested zero exists. Record-before-store makes that order indeterminate, so neither PASS nor a CPU FAIL can be claimed; the one-rerun rule applies.
- **Controls:** `A4b1` AC-FIX (iv) (`CPU_ZERO` before a GP write gives no `GP_CLEAR`).
- **Claim limits:**
  - This covers only the instrumented, text-matched stores; it is not a complete enumeration.
  - A site's `CPU_ZERO` latch records only that site's first zero, so a later zero from the same site is only counted.
  - Attribution rests on AC-CLEAR's compare-exchange.

### AC-INPUTS — every input the GP read before the clearing write is guest-written or modelled

- **Mandatory:** yes.
- **Guards against:** a stub value shaping the GP's result. The candidates are GP/EP register zeros, `PIO_FREE`, stub-VP output, and the trap-disabled counter.
- **Evidence profile:** strict (R1) + source reading.
- **Procedure:** the `[GPIN]` lines together with the counts lines' `gpin` and `GPIN_OVERFLOW`, and any `[GPWATCH] latch class=GPIN_OVERFLOW` line. `A4b1-r3` Device semantics 6 records one entry per distinct (kind, addr) until `GP_CLEAR` latches, and emits one `[GPIN]` line per entry; every key it does not record is counted in the uncapped `GPIN_OVERFLOW` counter and fires its write-once latch. The accounting is lossless only when checked against those counters, so the decision never rests on the bare absence of a line:
  1. **Accounting present:** take the last counts line, `K` (after `GP_CLEAR` the table is frozen). It must carry `gpin` and `GPIN_OVERFLOW`, and the number of distinct `[GPIN]` lines (by `seq`) with `seq ≤ K.seq` must equal `K.gpin`. The inventory classified in step 3 is every `[GPIN]` line (any after `K`, possible only when `GP_CLEAR` never latched, are classified too).
  2. **No overflow:** `GPIN_OVERFLOW = 0` in every counts line, and no `GPIN_OVERFLOW` latch line exists.
  3. **Classify** each `[GPIN]` line from the ported source and guest memory:
  - **Guest-written:** guest RAM written by the guest or by the XBE load, including DSP memory filled by the bootstrap from the image.
  - **Modelled:** a register the core computes from state it tracks, including `MIXBUF` with `vp_active_voices = 0`.
  - **Stub/unknown:** a shim constant, `MIXBUF` with `vp_active_voices > 0`, the trap-disabled counter, or anything that cannot be classified.

  The evidence record gives, for each line: kind, addr, first value, class, source line.
- **UNKNOWN** (checked first), if any of:
  - step 1 fails: no counts line carries `gpin`/`GPIN_OVERFLOW`, or the `[GPIN]` line count differs from `gpin` (no complete `[GPIN]` accounting is present);
  - step 2 fails: `GPIN_OVERFLOW` counter non-zero in any counts line, or a `GPIN_OVERFLOW` latch line exists;
  - AC-CLEAR passed, but there is no `[GPIN] kind=DMA_READ` line for the bootstrap's scratch read.
- **FAIL (→ R2-EXPL-INPUT):** steps 1–2 hold and any row is stub/unknown.
- **PASS:** steps 1–2 hold and every row is guest-written or modelled.
- **Controls:** the `DMA_READ` of the scratch page is the presence witness. `A4b1-r3` AC-FIX exercises each kind this AC classifies — `DMA_READ` in (vi) and (viii), `PERIPH`, `FIFO_READ` and `MIXBUF` (with `vp_active_voices` `0` and `> 0`) in (viii) — and the overflow counter/latch in (ix).
- **Claim limits:** a per-value classification, not a data-flow proof. Inputs are recorded per distinct `(kind, addr)` with the first value only; a later different value at the same key is not classified (r2 review D5).

### Decision rows (evaluate in order; first match wins)

- **R2-DEFAULT-REGRESS:** AC-DEFAULT2 FAIL. → Revert `A4b2`'s game edits. → Planner.
- **R2-UNKNOWN:** after the one R1 rerun, any of:
  - a gate fails;
  - any AC is UNKNOWN;
  - AC-CLEAR PASS while AC-BOOT or AC-RUN is not PASS.

  → Planner, with the failing gate or AC as the brief.
- **R2-CPU:** AC-NOCPU FAIL, or AC-CLEAR → R2-CPU. → **FAIL** (Q1 reversal (c)), not a narrowed claim. → Advisor.
- **R2-UNATTRIBUTED:** AC-CLEAR → R2-UNATTRIBUTED. The word was cleared, but no GP or CPU witness attributes it. Not PASS, and not blamed on the CPU. → Advisor.
- **R2-NOBOOT:** AC-BOOT FAIL. → Next: `A4c`, discovery of the bootstrap/SGE path.
- **R2-NOFRAMES:** AC-RUN FAIL on frames. → Next: `A4c`, discovery of the FE/SE frame gate after GPRST.
- **R2-NOEXEC:** AC-RUN FAIL on `gp_insns = 0`. → Next: `A4c`, discovery of the GP run/halt state; the `[GPRUN] pc`/`halt` values are the brief.
- **R2-NOCLEAR:** AC-BOOT and AC-RUN PASS, and AC-CLEAR → R2-NOCLEAR. → Next: `A4c`, discovery of what the GP program waits on; the `[GPIN]` inventory is the brief.
- **R2-EXPL-INPUT:** AC-BOOT, AC-RUN, AC-CLEAR and AC-NOCPU PASS, and AC-INPUTS FAIL. → The clearing is observed but is **exploratory for the named input**; the claim is not satisfied. → Advisor decides whether that input needs its own model packet first.
- **R2-PASS:** every AC PASS. → `A4b2` accepted with exactly its "Establishes" claim. → Next: the `PIO_FREE` model packet, before any strict liveness/boot criterion past the spin.

**Exhaustiveness.** Once the first four rows fail to match, AC-CLEAR is either PASS or R2-NOCLEAR, and every other AC is PASS or FAIL.
- AC-BOOT FAIL → R2-NOBOOT.
- Otherwise, AC-RUN FAIL → R2-NOFRAMES or R2-NOEXEC. AC-CLEAR here is R2-NOCLEAR, because CLEAR PASS without RUN PASS matched R2-UNKNOWN.
- Otherwise, AC-CLEAR R2-NOCLEAR → R2-NOCLEAR.
- Otherwise, AC-INPUTS decides between R2-EXPL-INPUT and R2-PASS.

In every row except R2-DEFAULT-REGRESS, the step-1 edits stay committed. Without trace they emit nothing, and R0 is the witness for that.

### Closure

- Evidence index (`docs/reviews/a4b2-execution-evidence.md`): each AC → artifact path + SHA-256 → result → selected row. Artifacts:
  - for both R1 and R0: `result.json`, `stacks.txt`, `jsrf_run.log`, `process.dmp`;
  - the exe SHA-256;
  - the game and toolkit commits;
  - `L` from G4;
  - all `[GPWATCH]` latch lines, verbatim;
  - all `[GPIN]` lines and the last counts line, verbatim.
- Post-review edits reopen the affected criteria. An unrelated next stop is recorded as a follow-up; the scope does not expand.
- Removing the forwarding calls later needs no packet. Keeping them after a regeneration requires re-applying them.
- Follow-ups (record, do not do), from reviewer deferred items:
  - there is no R0 rerun when R0's own gate gives R2-UNKNOWN;
  - it is inferred, not guaranteed, that the `0x001A1751` / `0x001A1FA7` stores of 0 do not run between the anchor and the spin;
  - the `[APUMMIO]` 400-line cap. AC-BOOT reads `0x02040`/`0x3FFFC` lines, which were at about line 104 in A4a R1; if they are absent, the result is UNKNOWN, never a FAIL.
  - r2 review (`docs/reviews/a4b1-a4b2-r2-adequacy-review.md`) advisories, recorded not fixed: D1 AC-BOOT could decide from `[GPBOOT]` `gprst`/`prev` plus counts `boots` instead of `[APUMMIO]`; D3 a site's first-zero latch hides a later zero from that site (→ R2-UNATTRIBUTED, safe); D4 AC-CLEAR step 4 precedes R2-CPU, so a pre-command `GP_ZERO_OVER_OTHER` can hide a CPU witness (conservative); D5 `MIXBUF`/`[GPIN]` record first value only; D6 PASS has no claim limit for a second command stopping at the spin again, or a clear by a periodic write-back. D2 (cite sites by content/VA) is fixed in r3.
