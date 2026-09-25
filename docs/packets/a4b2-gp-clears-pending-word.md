## A4b2 — the ported GP engine clears the title's pending word in a strict run

**Class:** change   **Contract revision:** `A4b2-r1`   **Status:** draft
**Split decision:** `A4b` is split into `A4b1` and `A4b2`. `A4b1` covers the port, the licence, the fixtures and the unchanged default path, and makes no guest-run claim. `A4b2` (this packet) covers the strict trap+trace run: boot, run, clear, no-CPU and inputs. The reason: the port's scale risk is settled by build, ctest and one default run, and should not wait for the run criteria or be reviewed with them.
**Governing requirement:** `A4a-r2` row `O-6` (`docs/packets/a4a-dsp-pending-word.md`); Q1 ruling `docs/reviews/a4b-q1-advisor-ruling.md` (conditions 1, 2 and 4 are discharged here; condition 3 was discharged by `A4p`); checkpoint-40 constraints `docs/reviews/a4b-planning-rulings.md`; `docs/reviews/a4b-pio-methodology-ruling.md` §(d).
**Depends on:** `A4b1-r1` (ACCEPTED, row `R1-PASS`); `A4p-r1` (ACCEPTED, row `O-GATE`); `A4a-r2` (ACCEPTED); `A3a-r25` (ACCEPTED).
**Baseline:** game and toolkit at `A4b1`'s accepted commits, plus the commit that adds this packet. Both trees are clean at promotion; the Session records the SHAs and the exe SHA-256.
**Revision log:** `docs/reviews/a4b-revision-history.md` (non-authoritative).

### Preconditions (checked before any step; if any fails, stop and select no row)

- **P1.** `A4p-r1` was ACCEPTED with row **`O-GATE`** on XBE SHA-256 `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` (`docs/reviews/a4p-r1-acceptance-review.md`). `Get-FileHash game\default.xbe` must equal that hash.
  - What `A4p` established: all 28 direct reads of `0xFE820010` are threshold re-polls, and the polled value reaches no use under its rules C1–C4. So `PIO_FREE` gates reachability and does not shape data the guest writes.
  - What `A4p` did **not** establish, carried here as claim limits:
    - It rests on an **inferred premise**: DSOUND follows the standard x86 register convention. That premise is checked only at the boundaries the analysis used.
    - It is **blind to register-indirect or computed access** to `0xFE820010`, beyond its E3 supporting evidence.
    - It says nothing about **timing**, or about whether **`0x80` is the true device value**.
- **P2.** `A4b1-r1` was ACCEPTED with row `R1-PASS`. The toolkit HEAD equals `A4b1`'s accepted toolkit commit.

### Motivating evidence

- `logs/runs/20260924-191833-331-a4a-r2-trap-trace` (A4a R1): STRICT, trap and trace on.
  - The guest writes GPSADDR `0x02040 = 803CC000` (L3188), GPSMAXSGE `0x020D4 = 8` (L3189), and GPRST `0x3FFFC = 1` (L3190) and then `= 3` (L3201).
  - FECTL/SECTL open the APU frame gate at L3286-3287, **after** GPRST=3.
  - `MEM32(803CC000) = 803C0000 = B`. The bytes `[B, B+0x5CC)` equal XBE file offset `0x1A7D60` (image `I`), and `MEM32(B+0x810) = 3`.
  - The run ends `diagnostic_deadline`, spinning at `loc_001A18D0` (`recomp_0005.c:6748-6751`).
- `logs/runs/20260924-191906-091-a4a-r2-default` (A4a R0): STRICT, default path. Same stop; zero `[APUMMIO]`.
- Guest stores to the word (XBE disassembly, read):
  - `0x001A1751 and dword [edi+0x810],0` (`recomp_0005.c:6524`);
  - `0x001A1FA7 mov [edi+0x10],ebp` with `edi = MEM32(0x1BA858)+0x800` (`recomp_0005.c:8088`);
  - the guest's own write of `3` at `recomp_0005.c:6746`.
- `A4b1` supplies the core, routing, DMA translation, atomic store and trace formats (its Device semantics 1–6). `A4b2` changes none of them.

### Claim and boundaries

- **Establishes (on R2-PASS only):** in one STRICT run with `RECOMP_GPU_ACK=0` and `RECOMP_APU_TRAP=1`, the wait at `loc_001A18D0` was satisfied by modelled GP execution of the guest's own command. Specifically:
  - the guest's GPRST write bootstrapped the GP from the scratch pages named by the guest's SGE table, and the loaded words equal image `I`;
  - the GP retired instructions in APU frames;
  - the GP's own DMA write path atomically exchanged the `3` at `MEM32(0x001BA858)+0x810` for `0`, and the compare-exchange succeeded;
  - no synthetic acknowledgement exists;
  - every input the GP read before that write was guest-written or modelled.
- **Does not establish:**
  - Everything Q1 condition 4 lists: the guest's arrival at the spin, and everything after it, passes through `PIO_FREE` stub gates and is exploratory-grade.
  - Boot progress, liveness, DirectSound init success, audio output, or the title's behaviour under a modelled VP FIFO.
  - That the title reaches the spin in the same state or with the same timing as real hardware. "The guest leaves `loc_001A18D0`" is an observation only.
  - The `A4p` limits carried in P1.
  - DSP56300 instruction-level correctness: there is no independent opcode oracle.
  - Physical/VA aliasing, EP execution, or GP→CPU interrupts.
- **Non-goals:** any toolkit change; a `PIO_FREE` model; EP; audio; regeneration; changing `docs/jsrf-run-profiles.md`.

### Readiness

- **Tools:** those `A4b1` verified: `build-jsrf.py`, ctest, `run-jsrf.py --profile strict`, `check-run-profile.py`, `check-dump-mapping.py`, and `inspect-jsrf.py memory|disasm`.
- **Stop if:**
  - P1 or P2 fails;
  - any of `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED`, `JSRF_ABI_CONTINUE` is in a launch environment;
  - the runner refuses `--profile strict`;
  - any **toolkit** change would be needed (→ Planner; that is an `A4b1` defect, not in scope here);
  - any step would set or clear `+0x810` other than through GP execution;
  - the pre-check fails: `recomp_0005.c` lines 6748 and 6751 must read `loc_001A18D0: ;` and `… goto loc_001A18D0; …`.

  A session that ends mid-execution selects no row.

### Execution

**Steps**

1. **Game-side observation edits.** These are hand-applied, each commented `A4b observation — re-apply after regeneration`, and change no value.
   - Before each of the following stores, call `jsrf_watch_store(site_va, target_va, value)`:
     - `recomp_0005.c:6746` (the guest's `3`; this is the **control site**);
     - `recomp_0005.c:6524` (`0x001A1751`);
     - `recomp_0005.c:8088` (`0x001A1FA7`);
     - every other generated store that textually matches `MEM32\(.* \+ 0x810\) =`. At baseline these are `recomp_0000.c:135283, 135406, 135871`. The text match is a lead, not a completeness witness.
   - `jsrf_watch_store` prints `[A4BSTORE] site=%08X va=%08X value=%08X` (the first 8 per site) when `RECOMP_APU_TRACE` is set and `target_va == MEM32(0x001BA858)+0x810`.
   - Declarations: each edited chunk gets one file-scope `extern void jsrf_watch_store(uint32_t, uint32_t, uint32_t);`. The definition goes in the existing `src/diagnostics.c`. No generated header changes.
   - The Session records each site's VA from its generated label.
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
  - `src/recomp/gen/recomp_0000.c` and `recomp_0005.c`: only the step-1 calls and their `extern` declarations;
  - `src/diagnostics.c`: only the `jsrf_watch_store` definition;
  - `docs/reviews/a4b2-*.md`.

  Nothing else.
- **Build/run owner:** the Session.

### Gates (checked before any AC)

- **G1:** `check-run-profile.py` prints `STRICT` for R1 and R0.
- **G2:** `check-dump-mapping.py` gives `matches ≥ 1` and `content-mismatch: 0` for both.
- **G3:** `result.json` is readable for both.
- **G4:** the pre-check passes.

Any gate failing → R2-UNKNOWN.

**Log definitions used below:**

- **`W`:** `MEM32(MEM32(0x001BA858)+0x810)`, read from the run's dump with `inspect-jsrf.py memory`.
- **`F`:** the number of matches of `sub_001A1769\+0x[0-9A-Fa-f]+ .*recomp_0005\.c:67(4[89]|5[01])\b` in `stacks.txt`.
- **Watch path alive:** R1's `jsrf_run.log` has at least one `[GPDMA] watch` line, or the `watch cap reached` line.
- **Log not truncated:** both of the following hold:
  - R1's `result.json` has `dump_ok: true`, and `jsrf_run.log` contains at least one `[GPDMA] frame=` summary line;
  - no summary line reports `watch_zero ≥ 1` unless a zero-payload watch line precedes it in log order.

### AC-DEFAULT2 — the default strict path is still unchanged with the observation edits

- Mandatory: yes.
- Guards against: the step-1 edits leaking into the default run.
- Evidence profile: strict (R0).
- Procedure: take R0's `result.json`, `W` and `F`. In `jsrf_run.log`, count lines matching `\[APUMMIO\]`, `\[GP(BOOT|RUN|DMA|IN)\]` and `\[A4BSTORE\]`.
- Oracle: A4a R0 (`20260924-191906-091`): `diagnostic_deadline`, `W = 3`, `F ≥ 1`.
- PASS: `outcome = diagnostic_deadline`, `W = 3`, `F ≥ 1`, and all counts are 0.
- FAIL: any of these differs.
- Controls: known-good is A4a R0; known-bad for the counts is R1 (non-zero).
- Claim limits: one run.

### AC-BOOT — the guest's GPRST write bootstraps the GP from the title's image

- Mandatory: yes.
- Guards against: a GP that runs code other than the title's image (zeros, a wrong page, the masked address).
- Evidence profile: strict (R1).
- Procedure: from R1's `jsrf_run.log`, take the `[GPBOOT]` header, the `[APUMMIO] write 0x02040`/`0x3FFFC` lines, and the `pram` lines. Compare word `i`, for `i < 0x173`, with `LE32(default.xbe[0x1A7D60 + 4i]) & 0xFFFFFF`. The Session records this one-off comparison and its output in the evidence record.
- Oracle: the original XBE bytes, not the implementation.
- PASS: all of the following hold:
  - there is at least one `[GPBOOT]` line;
  - the first one follows `write 0x02040 = 803CC000`, and follows a GPRST write that bootstraps under the pinned rule;
  - `sge0 = B`;
  - all `0x173` words are equal.
- FAIL: there is no `[GPBOOT]` line, `sge0 ≠ B`, or any word differs.
- UNKNOWN: that bootstrap has fewer than 64 `pram` lines.
- Controls: known-bad: the same comparison at file offset `0x1A7D64` must differ at word 0.
- Claim limits: shows what was loaded, not that it executes correctly.

### AC-RUN — GP instructions retire in APU frames after the bootstrap

- Mandatory: yes.
- Guards against: a core that is loaded but never clocked (frame gate closed, test-tone path, halted core).
- Evidence profile: strict (R1).
- Procedure: the `[GPRUN]` lines after the first `[GPBOOT]`. Each line carries `se_frame_after_boot`, `insns` and `tone` together (`A4b1` Device semantics 6).
- PASS: **one and the same** `[GPRUN]` line has `se_frame_after_boot ≥ 1`, `insns > 0` and `tone=0`.
- FAIL (→ R2-NOFRAMES): either
  - `[GPBOOT]` exists but there is no `[GPRUN]` line; or
  - every `[GPRUN]` line has `se_frame_after_boot = 0`.
- FAIL (→ R2-NOEXEC): some line has `se_frame_after_boot ≥ 1`, and every line has `insns = 0`.
- UNKNOWN: any other case. For example, frames ran and `insns > 0`, but `tone=1` on every such line.
- Controls: waived. The retirement count comes from the ported core's own counter, whose meaning is inherited from the pin.
- Claim limits: retirement, not correctness.

### AC-CLEAR — the GP's own DMA write made the 3→0 transition atomically

- Mandatory: yes.
- Guards against: attributing the `0` to anything other than the GP write path, including a guest-CPU store that races it; and a lost log line read as a CPU store.
- Evidence profile: strict (R1).
- Procedure: take the `[GPDMA] watch` lines, the `watch cap reached` line and the `[GPDMA] frame=` summaries; `W`; `F`.
- **Deciding line:** the **first** `[GPDMA] watch` line in log order with `payload=00000000` and `va = MEM32(0x001BA858)+0x810`. `A4b1` guarantees that this line is emitted regardless of the cap. AC-CLEAR is decided on this line alone; later lines are only recorded.
- PASS: the deciding line has `cas=ok` and `observed=00000003`, and records `dsp_addr` and `insns`. The following are recorded as corroboration only, not relied on:
  - `before`/`after`;
  - `W = 0`;
  - `F = 0` (the guest left the spin; anything after that is exploratory-grade, because it passes through `PIO_FREE` stub gates).
- FAIL (→ R2-CPU): either
  - the deciding line has `cas=fail` and `observed=00000000`; or
  - there is no deciding line, `W = 0`, **and** the watch path was alive **and** the log is not truncated.
- FAIL (→ R2-NOCLEAR): there is no deciding line and `W = 3`.
- UNKNOWN: any of the following:
  - the deciding line has `cas=fail` and `observed` is neither `0` nor `3`;
  - there is no deciding line and `W` is neither `0` nor `3`;
  - there is no deciding line and `W = 0`, but the watch path was not alive or the log is truncated.
- Controls: the `cas` path and the cap exemption are exercised by `A4b1` AC-FIX (e) and (f). Known-bad: the A4a R1 artifact has no `[GPDMA]` line.
- Claim limits: one store by one core. Nothing about the DSP program's other effects.

### AC-NOCPU — no synthetic ack exists, and the stop-path counters corroborate

- Mandatory: yes.
- Guards against: a synthetic acknowledgement, and an instrumented guest-CPU store that contradicts AC-CLEAR.
- Evidence profile: strict (R1) + structural.
- Procedure:
  - Take R1's launch environment from the archive.
  - Run `Select-String -Path <toolkit>\src\apu -Pattern 'RECOMP_APU_DSP_ACK|dsp_ack_' -Recurse`.
  - Search the strings of the exe for `RECOMP_APU_DSP_ACK`.
  - Take the `[A4BSTORE]` lines, grouped by `site`.
- PASS: all of the following hold:
  - `RECOMP_APU_DSP_ACK` is absent from the environment, and both searches return zero hits;
  - the control site (`recomp_0005.c:6746`) has at least one line with `value=00000003`;
  - no other instrumented site has a `value=00000000` line before the first `cas=ok` watch line, compared in log order.
- FAIL (→ R2-CPU): either
  - the ack is present or a search hits; or
  - a non-control site stored `0` to the watched VA before the first `cas=ok`.
- UNKNOWN (→ R2-UNKNOWN, like any other UNKNOWN): the control site has no line, so the counters are unwitnessed.
- Controls: known-good: the control site. Known-bad: the `A4b1` exe (no `[A4BSTORE]` lines at all).
- Claim limits: this covers only the instrumented generated stores, as corroboration. Those stores are register-indirect, so no enumeration is complete. Attribution rests on AC-CLEAR's compare-exchange.

### AC-INPUTS — every input the GP read before the clearing write is guest-written or modelled

- Mandatory: yes.
- Guards against: a stub value shaping the GP's result, whether GP/EP register zeros, `PIO_FREE`, stub-VP output, or the trap-disabled counter.
- Evidence profile: strict (R1) + source reading.
- Procedure: list every `[GPIN]` line before the deciding line. The Session classifies each from the ported source and guest memory as one of:
  - **guest-written:** guest RAM that the guest or the XBE load wrote. DSP memory filled by the bootstrap from the image counts here.
  - **modelled:** a register the core computes from state it tracks. `MIXBUF` with `vp_active_voices = 0` counts here.
  - **stub/unknown:** a shim constant, `MIXBUF` with `vp_active_voices > 0`, the trap-disabled counter, or anything unclassifiable.

  The evidence record carries a table: kind, addr, first value, class, source line.
- PASS: every row is guest-written or modelled.
- FAIL (→ R2-EXPL-INPUT): any row is stub/unknown.
- UNKNOWN: there is no `[GPIN]` line although AC-CLEAR passed. At minimum, the bootstrap's scratch read must appear as `DMA_READ`.
- Controls: known-good presence witness: the `DMA_READ` of `G`/`B` during bootstrap.
- Claim limits: this classifies per value; it is not a data-flow proof.

### Decision rows (evaluate in order; first match wins)

- **R2-DEFAULT-REGRESS:** AC-DEFAULT2 FAIL. → Revert `A4b2`'s game edits. → Planner.
- **R2-UNKNOWN:** after the one R1 rerun, any of the following holds:
  - a gate fails;
  - any AC is UNKNOWN;
  - AC-CLEAR PASS while AC-BOOT or AC-RUN is not PASS.

  → Planner re-plans with the failing gate/AC as its brief.
- **R2-CPU:** AC-NOCPU FAIL, or AC-CLEAR FAIL routed to R2-CPU. → **FAIL** (Q1 reversal (c)), not a narrowed claim. → Advisor.
- **R2-NOBOOT:** AC-BOOT FAIL. → Next: `A4c`, discovery of the bootstrap/SGE path.
- **R2-NOFRAMES:** AC-BOOT PASS, and AC-RUN FAIL on frames. → Next: `A4c`, discovery of the FE/SE frame gate after GPRST.
- **R2-NOEXEC:** AC-BOOT PASS, and AC-RUN FAIL on `insns = 0`. → Next: `A4c`, discovery of the GP run/halt state; the `[GPRUN] pc`/`halt` values are the brief.
- **R2-NOCLEAR:** AC-BOOT and AC-RUN PASS, and AC-CLEAR → R2-NOCLEAR. → Next: `A4c`, discovery of what the GP program waits on; the `[GPIN]` inventory is the brief.
- **Coverage.** With AC-BOOT PASS and no earlier row matched, each combination matches exactly one row:

  | AC-RUN \ AC-CLEAR | PASS | FAIL → R2-CPU | FAIL → R2-NOCLEAR | UNKNOWN |
  |---|---|---|---|---|
  | PASS | R2-EXPL-INPUT or R2-PASS (decided by AC-INPUTS) | R2-CPU | R2-NOCLEAR | R2-UNKNOWN |
  | FAIL, no frames | R2-UNKNOWN | R2-CPU | R2-NOFRAMES | R2-UNKNOWN |
  | FAIL, `insns = 0` | R2-UNKNOWN | R2-CPU | R2-NOEXEC | R2-UNKNOWN |
  | UNKNOWN | R2-UNKNOWN | R2-UNKNOWN | R2-UNKNOWN | R2-UNKNOWN |

  With AC-BOOT FAIL:
  - AC-CLEAR PASS, or any UNKNOWN → R2-UNKNOWN;
  - AC-CLEAR → R2-CPU → R2-CPU;
  - otherwise → R2-NOBOOT.
- **R2-EXPL-INPUT:** AC-BOOT, AC-RUN, AC-CLEAR and AC-NOCPU PASS, and AC-INPUTS FAIL. → The clearing is observed but is **exploratory for the named input**; the claim is not satisfied. → Advisor decides whether that input needs its own model packet first.
- **R2-PASS:** every AC PASS. → `A4b2` accepted with exactly its "Establishes" claim. → Next: the `PIO_FREE` model packet, before any strict liveness/boot criterion past the spin.

In every row except R2-DEFAULT-REGRESS, the step-1 observation edits stay committed; they are inert without `RECOMP_APU_TRACE`, and R0 is the witness for that.

### Closure

- Evidence index (`docs/reviews/a4b2-execution-evidence.md`): each AC → artifact path + SHA-256 → result → selected row. The artifacts are, for R1 and R0: `result.json`, `stacks.txt`, `jsrf_run.log` and `process.dmp`; plus exe SHA-256 and the game/toolkit commits.
- Post-review edits reopen the affected criteria. An unrelated next stop is recorded as a follow-up; the scope does not expand.
- Removing the observation calls later needs no packet. Keeping them after a regeneration requires re-applying them.
