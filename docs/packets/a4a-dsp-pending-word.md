## A4a — discover the GP start handshake and scratch provenance behind the DSP pending-word spin

**Class:** discovery   **Contract revision:** `A4a-r2`   **Status:** draft
**Starts from:** the guest spins at `loc_001A18D0` (XBE `0x001A18C1`–`0x001A18D3`, `sub_001A1769`, `src/recomp/gen/recomp_0005.c:6746-6751`) after writing `3` to the DSP pending word at `MEM32(0x001BA858)+0x810` = `0x803C0810`; nothing clears it and the run ends `diagnostic_deadline`. Shown STRICT by `logs/runs/20260924-143745-909-session-startup-baseline` (no trap) and `logs/runs/20260924-143932-031-apu-trap-register-trace` (trap + trace), and by the command checks `20260924-155151-292-a4a-commandcheck-r1` / `20260924-155223-143-a4a-commandcheck-r0`.
**Baseline:** game `98edf113d14246f09144ac4c6bf491bcf35ce5a4` plus the committed `docs/jsrf-run-profiles.md` § "Enabling a feature does not turn stub answers into modelled ones" (Session records the promotion commit); toolkit `c97ce2c3c3f268d849ee330e346ff91721f415e5`; pre-change exe SHA-256 `879716046b99cecfb61a8373237df24fcdefa4d00317f50c726aeae7a35187ef`.
**Governing ruling:** Advisor case ruling on `RECOMP_APU_TRAP` admissibility, verbatim in `docs/reviews/startup-20260924-session-4a79ce06.md` (commit `5e739f6`). **Revision log:** `docs/reviews/a4a-revision-history.md` (non-authoritative).

### Question

Is the next packet the GP DSP56300 engine (A4b), and what must its scope include? That turns on four unknowns:
- **U1:** what value the title finally leaves in GPRST (`0x3FFFC`). The current trace prints a read-back that is `0` for every GP/EP offset, so the actual write (XBE immediate `3` at `0x001A585E`) is not visible.
- **U2:** whether the title reads any GP/EP state (served today by a stub `0`).
- **U3:** whether GP scratch page 0 (SGE[0] of the table at GPSADDR `0x02040`) is the published block `B = MEM32(0x001BA858)`, holding the title's DSP image (XBE VA `0x001BA0A0`, `0x5CC` bytes) with the pending word `3` at `B+0x810`, at freeze.
- **U4:** whether the stop is unchanged on the instrumented build, with and without the trap.

### Experiments

1. **Instrumentation** (toolkit `src/apu/apu_mmio_hook.c` only; both changes live inside the existing `if (getenv("RECOMP_APU_TRACE"))` block, so they are **off by default**):
   - **T1** — each `[APUMMIO]` line prints the value the decoded instruction **wrote** (register, immediate, or computed OR/AND result) or, for a read, the value **delivered** to the guest, carried out of `apu_decode_and_handle` by an out-parameter. Line format unchanged: `  [APUMMIO] %s 0x%05X = %08X`.
   - **T2** — on the first access past the unchanged 400-line cap, print once: `  [APUMMIO] trace cap 400 reached; later accesses not logged`.
   - Must not change any value returned to the guest, any argument to `mcpx_apu_mmio_write`/`_read`, any APU state store, or any `ctx` register/`Rip`/`EFlags` update. GP/EP writes stay dropped; GP/EP reads stay `0`.
   - Commit the toolkit change; `python -X utf8 scripts\build-jsrf.py`; `ctest --test-dir build -C Release --output-on-failure`. Record the toolkit commit and new exe SHA-256.
2. **Runs**, from the game root, profile `--profile strict`, 30 s bound, archived under `logs/runs/`:
   ```powershell
   Remove-Item Env:\RECOMP_APU_DSP_ACK,Env:\RECOMP_AC97_READY,Env:\JSRF_ALLOW_UNRESOLVED,Env:\JSRF_ABI_CONTINUE,Env:\RECOMP_APU_TRAP,Env:\RECOMP_APU_TRACE -ErrorAction SilentlyContinue
   $env:RECOMP_GPU_ACK='0'; $env:RECOMP_KERNEL_LOG_BUDGET='100000'
   $env:RECOMP_APU_TRAP='1'; $env:RECOMP_APU_TRACE='1'
   python -X utf8 scripts\run-jsrf.py --seconds 30 --profile strict --label a4a-r2-trap-trace   # R1
   Remove-Item Env:\RECOMP_APU_TRAP,Env:\RECOMP_APU_TRACE
   python -X utf8 scripts\run-jsrf.py --seconds 30 --profile strict --label a4a-r2-default      # R0
   ```
3. **Reads** for each run `<run>` (R1 and R0); results into `docs/reviews/a4a-execution-evidence.md`. **[SESSION: verify each runs]**
   - **Validity V:** `check-run-profile.py <run>` prints `STRICT`; `check-dump-mapping.py <run>` gives `matches` ≥ 1, `content-mismatch: 0`; `result.json` readable; `B = MEM32(0x001BA858)` readable and ≠ 0; `W = MEM32(B+0x810)` readable (`inspect-jsrf.py memory <run> <va> 4`).
   - **Stop S:** `outcome` from `result.json`; `W`; frame count `F` =
     `(Select-String -Path <run>\stacks.txt -Pattern 'sub_001A1769\+0x[0-9A-Fa-f]+ .*recomp_0005\.c:67(4[89]|5[01])\b').Count`.
     The offset is deliberately **unpinned**: it varies between runs of one exe (`+0xB20`, `+0xB24`, `+0xB2B` observed, all line 6749). The line range is pinned by a pre-check that lines 6748 and 6751 of `src\recomp\gen\recomp_0005.c` read `loc_001A18D0: ;` and `… goto loc_001A18D0; …`. Control: the pattern gives `F ≥ 1` on all four archived runs above.
   - **R1 trace:** counts of `trace cap 400 reached` and `MMIO decode fail`; lines `write 0x3FF14 = …` and `write 0x3FFFC = …` (oracle: XBE immediates `0xFF` at `0x001A582A`, `3` at `0x001A585E`); `last_GPRST` = value of the last `write 0x3FFFC` (absent if no such line); GP/EP read lines `\[APUMMIO\] read  0x[3-7][0-9A-F]{4} = `; read notes `\[APU\] read of unimplemented`; `gp_ep_reads` = count of those read lines; `G` = value of the last `write 0x02040`; presence of `trapped for MMIO`. Known-bad control: archived `143932-031` shows `write 0x3FFFC = 00000000` (the T1 defect).
   - **Consistency C (R1):** every read-note block (`offset >> 16`) has ≥ 1 GP/EP read line in that block, and every block with a read line has its note.
   - **Leak L (R0):** counts of `[APUMMIO]`, `started by the title`, `trapped for MMIO` in `jsrf_run.log`; all must be 0.
   - **Image I (R1):** `S0 = MEM32(G)`; export `inspect-jsrf.py memory <R1> <B> 0x5CC --out logs\a4a-scratch0.bin` and compare byte-for-byte with `game\default.xbe` file offset `0x1A7D60` (= `0x001BA0A0 − DSOUND VA 0x19E340 + raw 0x18C000`), length `0x5CC` (the section's file-backed bytes end exactly at `0x001BA0A0+0x5CC`). I holds iff `S0 == B` and all bytes equal. Controls on archived `143932-031`: known-good — `MEM32(0x803CC000) = 803C0000 = B` and 2048/2048 bytes equal over `0x800` (Session-verified), so I holds; known-bad — the same compare at file offset `0x1A7D64` differs at offset 0.

### Outcomes

Evaluate in order; first match wins. "Gates" = V holds for both runs; in both runs `outcome = diagnostic_deadline`, `W = 3`, `F ≥ 1`; R1 has zero cap and zero decode-fail lines, ≥ 1 `write 0x3FF14 = 000000FF` (the T1 value oracle), ≥ 1 `write 0x02040` line, and `trapped for MMIO`; C holds; L is all 0. `write 0x3FFFC` is deliberately **not** a gate: whether the title writes `3` there is U1, not a tool check.

| ID | Observed | Means | Next packet |
|---|---|---|---|
| O-1 | both runs STRICT with readable `result.json`, and in R1 or R0 either `outcome ≠ diagnostic_deadline`, or dump-mapping passes and `W ≠ 3` | the stop moved: instrumentation not observation-only, or the stop is nondeterministic. **Not progress.** | Advisor ruling before any packet |
| O-2 | V holds, R1 has zero cap and zero decode-fail lines, and any of: `write 0x3FF14` lines exist but none is `= 000000FF`; C fails; L > 0 | T1/T2 defective, or R0 was not launched on the default path | Session repairs T1/T2 in scope and reruns once under this revision; repeat → Planner `A4a-r3` |
| O-3 | Gates hold; I fails | scratch page 0 is not the image block, or the image is not in place | `A4c` discovery: GP scratch upload path (`0x001A16D2` → `0x001A53FC`, SGE table at `G`) |
| O-4 | Gates and I hold; no `write 0x3FFFC` line, or `last_GPRST & 3 ≠ 3` | the title does not leave the GP started | `A4d` discovery: `sub_001A5742` start path and what overrides it |
| O-5 | Gates and I hold; `last_GPRST & 3 = 3`; `gp_ep_reads > 0` | the title consumes GP/EP state that is a stub today | `A4b` brief: GP DSP56300 engine change packet whose scope also covers the recorded read offsets; needs Q1, Q2 first |
| O-6 | Gates and I hold; `last_GPRST & 3 = 3`; `gp_ep_reads = 0` | the start write is the only GP interaction; the image is in place | `A4b` brief: GP engine; positive controls: GPRST `0→3` bootstraps from `G`, loaded words equal the I image, GP instructions retire; acceptance: `+0x810` cleared by GP execution in a STRICT run; needs Q1, Q2 first |
| O-UNKNOWN | none of the above — including V failing, `F = 0` with `diagnostic_deadline` and `W = 3`, a cap or decode-fail line, no `write 0x3FF14` line, no `write 0x02040` line, no `trapped for MMIO` in R1, or the I range unreadable | not decided (a missing frame is a sampling gap, **never** a moved stop) | rerun once with identical settings; persists → Planner `A4a-r3` with the failing gate as its brief |

### Limits and stops

- **Does not establish:** that the GP would clear `+0x810` if run — only real GP DSP56300 execution can (ruling D); that any APU/DSP path works or any wait was satisfied by a modelled cause (ruling B). R1 reaches the spin only after its `0xFE820010` polls are answered by the `PIO_FREE` stub constant `0x80`, so R1's arrival at the spin is **exploratory-grade however R1 classifies**; this packet claims only the values written, read counts, freeze memory and stop location (ruling A). R0 is a leak check, not a comparison: no R1/R0 difference is attributed to any single trap effect (ruling C). Inventory completeness covers trapped accesses up to freeze only when no cap line is present. Not established: the order of the GPRST write relative to the `+0x810` write (the trace has no thread attribution); scratch state at GPRST time; the units of `0x5CC` (bytes inferred from the section boundary); GP/EP register semantics; whether SGE VAs need mapping in A4b.
- **Open for A4b, not blocking A4a:** **Q1** (Advisor) — does A4b's strict claim inherit contamination from the upstream `PIO_FREE` stub, requiring a sourced `PIO_FREE` model first? **Q2** (owner via Advisor, §3.4) — licensing of a DSP56300 core (the known one, xemu `hw/xbox/mcpx/apu/dsp/`, is reported GPL-2.0-or-later by r1; not re-verified). The `0x020010` / `PIO_FREE` naming conflict stays owner-reserved.
- **Stop if:** any of `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED`, `JSRF_ABI_CONTINUE` is in the launch environment or the runner refuses `--profile strict`; T1/T2 would need any change listed as forbidden in Experiment 1; any change outside toolkit `src/apu/apu_mmio_hook.c` and game `docs/reviews/a4a-*.md` seems needed; build or `ctest` fails for a reason not caused by T1/T2; anything would make GP/EP writes stick, echo reads, or write `+0x810`; the pre-check of `recomp_0005.c:6748/6751` fails (→ O-UNKNOWN, re-plan).
- **Closure:** T1/T2 stay committed only inside the `RECOMP_APU_TRACE` block (off by default; R0's L = 0 is its witness), or are reverted. Artifacts: toolkit commit and diff, build/ctest output, new exe SHA-256, R1 and R0 run directories with `result.json`/`stacks.txt`/`jsrf_run.log`/`process.dmp` SHA-256, `logs\a4a-scratch0.bin`, `docs/reviews/a4a-execution-evidence.md` with every value above and the selected outcome row.
