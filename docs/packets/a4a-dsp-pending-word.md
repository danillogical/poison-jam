# A4a — GP start handshake: value-faithful APU trace and DSP scratch provenance

- **Contract revision:** `A4a-r1`
- **Status:** `draft`
- **Author:** Planner. Mechanical fields marked **[SESSION]** are for the Session to fill
  in and check before adequacy review (workflow §5.1).
- **Governing requirement:** `plan-jsrf-bare-minimum.md` § "CURRENT PACKET — none". The
  measured blocker there is the DSP pending-word spin at `loc_001A18D0`.
- **Depends on:** `A3a-r25` (accepted). The policy edit that adds "Enabling a feature does
  not turn stub answers into modelled ones" to `docs/jsrf-run-profiles.md` § "Feature
  enablement — real capability, not a bypass". The Advisor case ruling on
  `RECOMP_APU_TRAP` admissibility (Advisor child `407c54a3-6ca4-4a65-835b-faf5355195cd`,
  route `claude/claude-opus-5-5` @ `high`), recorded verbatim in
  `docs/reviews/startup-20260924-session-4a79ce06.md` and to be copied into this packet's
  review record.
- **Baseline:** game `98edf113d14246f09144ac4c6bf491bcf35ce5a4`, toolkit
  `c97ce2c3c3f268d849ee330e346ff91721f415e5`, `jsrf_recomp.exe` SHA-256
  `879716046b99cecfb61a8373237df24fcdefa4d00317f50c726aeae7a35187ef`, `game/default.xbe`
  SHA-256 `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C`.
  **[SESSION]** Before promotion, commit the dirty `docs/jsrf-run-profiles.md` edit and
  record the resulting game commit here as the baseline.

## Motivating evidence

Labels follow workflow §2.4.1: **O** = observed by the Planner (read or ran), **O-S** =
observed by the Session and not re-checked by the Planner, **I** = inferred,
**U** = uncertain.

Runs (all on the baseline revisions above unless stated otherwise):

| Run | Profile (reclassified) | Settings | Role |
|---|---|---|---|
| `logs/runs/20260924-143745-909-session-startup-baseline` | STRICT (O) | `RECOMP_GPU_ACK=0` | strict blocker, no trap |
| `logs/runs/20260924-143856-017-apu-trap-classification-probe` | STRICT (O-S) | `+RECOMP_APU_TRAP=1` | trap classifies strict |
| `logs/runs/20260924-143932-031-apu-trap-register-trace` | STRICT (O) | `+RECOMP_APU_TRAP=1, RECOMP_APU_TRACE=1` | current-revision register traffic |
| `logs/runs/20260924-100502-623-a3a-codec-model` | strict (from A3a record) | A3a | older strict blocker |
| `logs/runs/20260922-104131-686-apu-trace` | exploratory (`RECOMP_AC97_READY` set), toolkit `a02780d` | trap+trace | older revision; **not** used for values |

1. **The spin (O).** Original XBE `0x001A18C1`–`0x001A18D3`: `push 3; pop eax; and ecx,eax;
   add ebx,0x810; rep movsb; mov [ebx],eax; cmp dword ptr [ebx],0; jne 0x1a18d0`, inside
   `sub_001A1769` (`0x001A1769`–`0x001A18DE`). Generated as `recomp_0005.c:6746-6751`.
   The live frame `sub_001A1769+0xB20 … recomp_0005.c:6749` appears in `stacks.txt` of
   both `143745-909` and `143932-031`. In both dumps, the dword at `0x001BA858` is
   `803C0000` and in `143932-031` the dword at `0x803C0810` is `00000003`.
   `check-dump-mapping.py 143932-031` reports `matches: 1 content-mismatch: 0`.
   The only other writer of `+0x810` seen so far is `0x001A1751 and dword ptr [edi+0x810],0`
   in the host-side stop path `0x001A1747` (O). That path is not a completion path (I).
2. **The command block is GP scratch memory and starts with the title's DSP image.**
   (O) In the `143932-031` trace, GPSADDR (`0x02040`) is written `803CC000` and GPSMAXSGE
   (`0x020D4`) is written `8`. In that dump, `0x803CC000` holds 8-byte entries
   `803C0000, 803C1000, …, 803C7000, 803E0000, …`. So scratch page 0 is the block the
   guest spins on.
   (O) In both `143745-909` and `143932-031`, dump bytes `[0x803C0000, +0x804)` equal
   `game/default.xbe` file offset `0x1A7D60`. That is VA `0x001BA0A0` in section `DSOUND`
   (VA `0x19E340`, raw `0x18C000`). The first differing byte is at `+0x804`, where live
   mailbox state begins (`+0x804 = 0DB8`, `+0x808 = 3EF8`, `+0x80C = 0D1E`,
   `+0x810 = 3`).
   (O) `0x001A16FC`/`0x001A1702` store `{0x001BA0A0, 0x5CC}` into a descriptor, and
   `0x001A173F` calls `0x001A53FC` with it.
   (U) The units of `0x5CC` are unknown (bytes or words).
3. **The title starts the GP (O static, I dynamic).** `sub_001A5742` writes
   `[0xFE83FFFC] := edi` at `0x001A581A`, `[0xFE83FF14] := 0xFF` at `0x001A582A`, and
   `[0xFE83FFFC] := 3` at `0x001A585E` (immediate). GP offset `0xFFFC` is `NV_PAPU_GPRST`,
   and `3` = `GPRST|GPDSPRST` (toolkit `src/apu/apu_regs.h`, `NV_PAPU_GPRST*`).
   `sub_001A586B` writes `1` to `0xFE85FFFC` (EP reset) at `0x001A587C`.
   (I) The `143932-031` trace group at log lines 906–920 matches this sequence in order:
   `0x02040`, `0x020D4`, `0x3FFFC`, `0x3FF10`, `0x3FF14`, FIFO CUR copies, `0x3FFFC`.
4. **The current trace cannot show GP/EP values, and the toolkit drops GP/EP writes (O).**
   Toolkit `apu_mmio_hook.c` `apu_hook_handle_mmio` prints
   `mcpx_apu_mmio_read_quiet(...)` for writes as well as reads, which is a read-back
   value. `apu_core.c` `mcpx_apu_mmio_read_quiet` returns 0 for offsets `>= 0x30000`.
   `mcpx_apu_dispatch_mmio` ignores writes `>= 0x30000` ("GP (0x30000) and EP (0x50000)
   regions ignored for now"). So `143932-031` shows `write 0x3FF14 = 00000000` and
   `write 0x3FFFC = 00000000` although the code writes `0xFF` and `3`. The trace cap is
   a hard-coded 400 (`n++ < 400`) with no truncation marker; `143932-031` logged 344
   `[APUMMIO]` lines.
5. **No GP DSP engine exists (O).** `src/apu/apu_dsp.c` is a 166-line passthrough stub
   ("STUBBED - passthrough mode"). The Session found no DSP56300 executor anywhere in
   either repository (O-S).
6. **Stub answers are in the trapped path (O).** `0xFE820010` under trap is served by
   `mcpx_apu_vp_read` → `NV1BA0_PIO_FREE` → constant `0x80` (`apu_vp.c:556-557`).
   `143932-031` has 58 such reads. The first one comes before the VP address writes.
   `143932-031` has zero `[APU] read of unimplemented … DSP block` lines and zero
   `MMIO decode fail` lines. The read note is emitted on the first read of each block
   `>= 0x30000` (`apu_core.c` `apu_note_unimplemented_block_read`), so this is a coverage
   witness that the guest made **no GP/EP reads** in that run.
7. **Kernel ordinal caveat (O).** `[KERNEL] #200` is the default log budget
   (`kernel_bridge.c` `kernel_log_budget`, default 200), not a progress measure. Runs in
   this packet set `RECOMP_KERNEL_LOG_BUDGET=100000`, as `docs/jsrf-run-profiles.md`
   § "Observation only" requires.

## Claim and boundaries

**Bounded claim.** On the execution revision, a strict `RECOMP_APU_TRAP` run records
every guest APU-window access with the value actually written or returned. From that
record and the frozen dump the packet establishes three things:

- which GP/EP reset and control writes the title performs before the capture, with their
  values;
- whether the title reads GP/EP state at all;
- whether the GP scratch memory named by GPSADDR holds the title's DSP image at offset 0
  and the pending word `3` at `+0x810`.

The stop location stays unchanged. These facts decide whether a GP DSP engine packet is
the correct next step, and what its positive control is.

**Establishes (on PASS):**

- The APU trace is value-faithful for GP/EP writes, validated against XBE immediates.
- The complete in-run inventory of GP/EP accesses with values, with a coverage witness.
- Scratch-page provenance at freeze: GPSADDR → SGE[0] → the published base →
  XBE `0x001BA0A0` image bytes, plus the mailbox word `3`.
- The strict stop is unchanged under the instrumented trap build, and unchanged
  without the trap.

**Does not establish:**

- That the GP would clear `+0x810` if it ran. Only a real GP DSP56300 execution can
  show that (Advisor ruling D).
- Any device fidelity, audio, EP behaviour, liveness, or boot progress.
- That the guest's arrival at the spin under trap is strict progress. It depends on
  `PIO_FREE = 0x80` stub answers, so per `docs/jsrf-run-profiles.md` § "Feature
  enablement — real capability, not a bypass" and the Advisor case ruling (B), it is
  at most exploratory-grade. This packet claims only the **ordered traffic and the stop
  location** (ruling A).
- Any attribution of a trap vs non-trap difference to any single trap effect (ruling C).
- The order of the GPRST write relative to the `+0x810` write. The trace cannot attribute
  lines to threads. Bounded uncertainty is accepted: an engine would bootstrap on the
  write whenever it occurs.
- Scratch state at the moment of the GPRST write. Only freeze state is measured.

**Non-goals:**

- No DSP56300 core, interpreter, or disassembler, and no bootstrap.
- No storage or semantics for GP/EP writes: they stay dropped. GP/EP reads keep
  returning 0.
- No write to `+0x810` by anything. No `RECOMP_APU_DSP_ACK` or any equivalent.
- No change to `0xFE820010` (neither the tick counter nor `PIO_FREE`). No adjudication
  of the grandfathered `0x020010` row.
- No EP/audio output, GPU work, regeneration/recovery, classifier or profile-doc edits,
  or new environment variables.
- No static analysis of the DSP program. That needs a DSP56300 disassembler, which
  raises the same licensing question as the engine (open question Q2).

## Readiness

- **Tools the Session must confirm run before submission [SESSION]:**
  `scripts/build-jsrf.py`, `ctest`, `scripts/run-jsrf.py`,
  `scripts/check-run-profile.py`, `scripts/check-dump-mapping.py`,
  `scripts/inspect-jsrf.py memory … --out`, and PowerShell `Select-String`.
- **Prerequisites:**
  - Both trees are clean at the baseline, with the profile-doc edit committed.
  - The Advisor case ruling is recorded verbatim.
  - Neither the build tree nor `%LOCALAPPDATA%\xboxrecomp` is blocked by the sandbox.
- **Stop if:**
  1. Any of `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED`, or
     `JSRF_ABI_CONTINUE` is present in the launch environment, or the runner refuses
     `--profile strict`.
  2. Implementing T1–T3 would require changing:
     - any value returned to the guest;
     - any argument passed to `mcpx_apu_mmio_write`/`mcpx_apu_mmio_read`;
     - any `d->regs`, `d->gp`, or `d->ep` store;
     - any `ctx` register, `Rip`, or `EFlags` update.

     If so, stop and return to the Planner.
  3. A change outside the write scope appears necessary.
  4. The build or `ctest` fails for a reason not caused by T1–T3.
  5. Any temptation arises to make GP/EP writes "stick" or reads echo. That is a model,
     and it is out of scope.

## Execution

**Steps.**

1. **T1** (toolkit `src/apu/apu_mmio_hook.c`): the `[APUMMIO]` trace prints the value
   the decoded instruction **wrote** for writes, including computed read-modify-write
   results for OR/AND. For reads it prints the value **delivered** to the guest. The line
   format stays `  [APUMMIO] %s 0x%05X = %08X`. Suggested mechanism: an out-parameter
   from `apu_decode_and_handle`.
2. **T2** (same file): when the 400-line cap is first exceeded, print exactly one line,
   `  [APUMMIO] trace cap 400 reached; later accesses not logged`. The cap value is
   unchanged, and there is no new environment variable.
3. **T3** (toolkit `src/apu/apu_core.c`, `mcpx_apu_mmio_write`): for `addr >= 0x30000`,
   emit once per 64 KiB block
   `[APU] write to unimplemented <GP|EP|unknown> DSP block at offset 0x%05X value 0x%08X -> dropped; no model`,
   mirroring `apu_note_unimplemented_block_read`. The write is still dropped.
4. Build and run `ctest`. Commit the toolkit change. **[SESSION]** Record the commit.
5. Run **R1** and **R0** (procedures below). Evaluate AC1–AC6. Write the evidence record.

- **Write scope:**
  - Toolkit: `src/apu/apu_mmio_hook.c`, `src/apu/apu_core.c`.
  - Game: `docs/reviews/a4a-*.md` and the plan's status block at closure.

  Nothing else.
- **Worker scopes:** none (single owner).
- **Build/run owner:** Session.

**Run R1 (trap + trace).** Game root. **[SESSION: verify it runs]**

```powershell
Remove-Item Env:\RECOMP_APU_DSP_ACK,Env:\RECOMP_AC97_READY,Env:\JSRF_ALLOW_UNRESOLVED,Env:\JSRF_ABI_CONTINUE -ErrorAction SilentlyContinue
$env:RECOMP_GPU_ACK='0'; $env:RECOMP_APU_TRAP='1'; $env:RECOMP_APU_TRACE='1'; $env:RECOMP_KERNEL_LOG_BUDGET='100000'
python -X utf8 scripts\run-jsrf.py --seconds 30 --profile strict --label a4a-r1-trap-trace
```

**Run R0 (default path).** Same, but remove `RECOMP_APU_TRAP` and `RECOMP_APU_TRACE`,
and use label `a4a-r0-default`.

Common checks for a run directory `<run>`:

- `python -X utf8 scripts\check-run-profile.py <run>` → must print `STRICT`.
- `python -X utf8 scripts\check-dump-mapping.py <run>` → `matches` ≥ 1 and
  `content-mismatch: 0`.
- `<run>\result.json`.

## Criteria

### A4a-AC1 — The toolkit change is observation-only

- **Mandatory:** yes.
- **Guards against:** the instrumentation changing guest-visible behaviour, which would
  invalidate AC5 and AC6 and every later comparison.
- **Evidence profile:** manual (diff review) plus fixture (`ctest`).
- **Procedure:**
  `git -C C:\Users\logic\Repos\xboxrecomp diff c97ce2c <A4a toolkit commit> -- .`,
  then `python -X utf8 scripts\build-jsrf.py` and
  `ctest --test-dir build -C Release --output-on-failure`.
- **Artifact:** diff (hash **[SESSION]**), build and ctest output, in the evidence record.
- **Oracle:** the Stop-if-2 list above.
- **PASS:** all of the following hold.
  - The diff touches only the two write-scope files.
  - Every changed statement is logging, a static counter or once-flag, or the passing of
    an already-computed access value to the trace.
  - None of the Stop-if-2 items changes.
  - The build succeeds and `ctest` passes.
- **FAIL:** any Stop-if-2 item changes, a file outside scope changes, or build/ctest
  fails because of the diff.
- **UNKNOWN:** the diff cannot be bound to a commit.
- **Controls:** waived for the diff review. It is a manual judgment; AC5/AC6 are its
  behavioural control.
- **Claim limits:** PASS does not show that the new trace is correct. That is AC2.

### A4a-AC2 — The trace reports the values the guest actually wrote to GP registers

- **Mandatory:** yes.
- **Guards against:** a value-blind trace being read as device evidence. The current tool
  prints `0` for `3`.
- **Evidence profile:** strict (R1).
- **Procedure:**
  `Select-String -Path <R1>\jsrf_run.log -Pattern '^\s*\[APUMMIO\] write 0x3FF(14|FC) = '`.
- **Artifact:** the matched lines with line numbers, in the evidence record.
- **Oracle:** original-XBE immediates, `0x001A582A mov dword ptr [0xfe83ff14], 0xff` and
  `0x001A585E mov dword ptr [0xfe83fffc], 3`
  (`python -X utf8 scripts\inspect-jsrf.py disasm 0x001A5817 0x001A586B`).
- **PASS:** R1 is STRICT, and the log contains ≥1 line `write 0x3FF14 = 000000FF` and ≥1
  line `write 0x3FFFC = 00000003`.
- **FAIL:** R1 is STRICT, and lines for both offsets are present, but no line carries the
  expected value for at least one of them.
- **UNKNOWN:** no line for one of the offsets (path not reached, or cap hit before it),
  or R1 is not STRICT.
- **Controls:**
  - *Known-bad:* apply the same predicate to archived `143932-031`. It must evaluate to
    FAIL (O: that log shows `write 0x3FF14 = 00000000` and `write 0x3FFFC = 00000000`).
  - *Known-good, main-register value preserved:* R1 contains `write 0x02000 = 0000000F`,
    as `143932-031` does.
- **Claim limits:** value fidelity for traced accesses only. It says nothing about GP
  semantics.

### A4a-AC3 — The GP/EP access inventory for R1 is complete and self-consistent

- **Mandatory:** yes.
- **Guards against:** deciding the next packet from a truncated or partial trace. This is
  the failure mode of the withdrawn `#200` claim.
- **Evidence profile:** strict (R1), per ruling (A): ordered traffic.
- **Procedure:** from `<R1>\jsrf_run.log` extract, in log order:
  - every `[APUMMIO]` line with offset `>= 0x30000`;
  - every `[APU] read of unimplemented` line and every `[APU] write to unimplemented`
    line;
  - the count of `trace cap 400 reached` lines and of `MMIO decode fail` lines;
  - the count of `read  0x20010` lines.

  **[SESSION]** Write the exact `Select-String` commands.
- **Artifact:** an inventory table in `docs/reviews/a4a-execution-evidence.md`. It
  records:
  - offset, direction, value, and log line for each GP/EP access;
  - `last_GPRST` (value of the last `write 0x3FFFC`) and `last_EPRST` (value of the last
    `write 0x5FFFC`);
  - `gp_ep_reads` (count);
  - `pio_free_reads` (count of `read  0x20010`).
- **Oracle:** internal consistency between two independent emitters. The per-access trace
  (T1) is bounded by the cap. The once-per-block notes (T3 and the existing read note)
  are unbounded.
- **PASS:** all of the following hold.
  - R1 is STRICT.
  - There are zero cap lines and zero decode-fail lines.
  - Every once-per-block note has ≥1 `[APUMMIO]` line in the same 64 KiB block and
    direction.
  - Every block/direction with ≥1 `[APUMMIO]` line has its note.
  - The inventory table is written.
- **FAIL:** a note/trace mismatch in either direction. This means the trace missed
  accesses.
- **UNKNOWN:** a cap line or decode-fail line is present, or R1 is not STRICT. Row
  R-UNKNOWN then applies.
- **Controls:**
  - *Known-good:* `143932-031` had 344 lines (under the cap), zero decode fails, and
    zero read notes. Consistent.
  - *Known-bad:* none archived. The cap line itself is the designed negative witness.
- **Claim limits:**
  - Covers the whole run up to freeze, all threads. It is not proof that each access
    preceded the spin.
  - `pio_free_reads` values come from a stub. They are recorded for Q1, not interpreted.

### A4a-AC4 — GP scratch page 0 holds the title's DSP image and the pending word at freeze

- **Mandatory:** yes.
- **Guards against:**
  - designing a bootstrap for the wrong memory;
  - relying on an image that is not in place.
- **Evidence profile:** strict (R1 dump).
- **Procedure:**
  1. Read `B` = the dword at guest `0x001BA858`.
  2. Read `G` = the last `write 0x02040` value from AC3.
  3. Read `S0` = the dword at `G`.
  4. Export `[B, B+0x800)` with `inspect-jsrf.py memory <R1> <B> 0x800 --out <tmp>`.
  5. Compare the export byte-for-byte with `game\default.xbe` file offset `0x1A7D60`,
     length `0x800`.
  6. Read the dword at `B+0x810`.

  **[SESSION]** Mechanize this as one PowerShell block; derive `0x1A7D60` from the
  `DSOUND` section header (VA `0x19E340`, raw `0x18C000`) and record the derivation.
- **Artifact:** `B`, `G`, `S0`, the compare result, the first differing offset, and the
  mailbox dword, in the evidence record.
- **Oracle:** original XBE bytes at VA `0x001BA0A0`, the pointer the title itself stores
  at `0x001A16FC`.
- **PASS:** all of the following hold.
  - The dump-mapping check passes.
  - `B ≠ 0`.
  - `S0 == B`.
  - All `0x800` bytes are equal.
  - `MEM32(B+0x810) == 3`.
- **FAIL:** one of those is false while the dump-mapping check passes.
- **UNKNOWN:** the dump-mapping check fails, the range is absent from the dump, or AC3
  did not yield `G`.
- **Controls:**
  - *Known-bad 1:* the same compare against XBE offset `0x1A7D64` (shifted by 4) must
    differ.
  - *Known-bad 2:* comparing `[B+0x804, B+0x818)` with the XBE must differ, because that
    is live mailbox state.
  - *Known-good:* the same procedure on `143932-031` gives PASS (O: equal through
    `+0x803`, `0x803CC000 → 803C0000`, `+0x810 = 3`).
- **Claim limits:**
  - Freeze state only.
  - Does not establish the image length or units (`0x5CC`, U).
  - Does not show that a bootstrap would load it, or what the program does.

### A4a-AC5 — The instrumented trap build still stops at the DSP pending word

- **Mandatory:** yes.
- **Guards against:** the instrumentation moving the stop, and a moved stop being misread
  as progress.
- **Evidence profile:** strict (R1).
- **Procedure:**
  - Read `result.json`.
  - `Select-String -Path <R1>\stacks.txt -Pattern 'sub_001A1769\+0xB20 .*recomp_0005\.c:6749'`.
  - Read the dump dword at `B+0x810` (from AC4).
  - Run the profile and dump-mapping checks.
- **Artifact:** the outcome, the matched frame line, the word, and the profile, in the
  evidence record.
- **Oracle:** the XBE spin instructions `0x001A18D0`/`0x001A18D3` and their generated
  line 6749.
- **PASS:** all of the following hold.
  - `outcome = diagnostic_deadline`.
  - ≥1 matching frame.
  - The word is `3`.
  - R1 is STRICT.
  - The dump-mapping check passes.
- **FAIL:** a different outcome or fault, no matching frame, or the word is not `3`.
- **UNKNOWN:** the profile or dump-mapping check fails.
- **Controls:**
  - *Known-good:* `143932-031` satisfies the predicate (O).
  - *Known-bad:* waived. A false PASS would require the same frame and word by accident.
- **Claim limits:** location only. Not liveness.

### A4a-AC6 — The default (untrapped) path is unchanged

- **Mandatory:** yes.
- **Guards against:** a toolkit edit leaking into the default strict path.
- **Evidence profile:** strict (R0).
- **Procedure:**
  - Apply the AC5 predicate to R0.
  - Count `[APUMMIO]`, `[APU] started by the title`, and `trapped for MMIO` in
    `<R0>\jsrf_run.log`.
- **Artifact:** in the evidence record.
- **Oracle:** the baseline `143745-909`, which has the same frame and zero lines for
  each of those patterns (O).
- **PASS:** the AC5 predicate holds on R0, and all three counts are 0.
- **FAIL:** the predicate fails, or any count is > 0.
- **UNKNOWN:** the profile or dump-mapping check fails.
- **Controls:**
  - *Known-good:* `143745-909`.
  - *Known-bad:* R1 must have counts > 0 for `[APUMMIO]` and `trapped for MMIO`. This
    shows the counters can detect the trap.
- **Claim limits:** the default path's stop only.

## Decision rows

Evaluate the rows in order; the first match wins. "All PASS" means AC1–AC6 are all PASS.

- **R-TOOL-FAIL:** AC1 or AC2 or AC3 is FAIL → the tooling is defective. → The Session
  fixes T1–T3 within the write scope and reruns R1 **once**. If it fails again, return
  to the Planner. Nothing is interpreted.
- **R-UNKNOWN:** any mandatory criterion is UNKNOWN → no conclusion. → Rerun once with
  the same settings. If it persists, return to the Planner. A persistent cap line means
  the next revision makes the cap configurable.
- **R-MOVED:** AC5 or AC6 is FAIL → the instrumentation is not observation-only, or the
  stop is nondeterministic. → Stop and escalate to the Advisor. **Do not** read this as
  progress.
- **R-IMAGE:** AC4 is FAIL → the scratch page does not hold the image, or SGE[0] is not
  the block. → The next packet investigates the upload path (`0x001A16D2` →
  `0x001A53FC`) before any engine work.
- **R-NO-START:** all PASS, but `last_GPRST & 3 != 3` → the title never leaves the GP
  started. → The engine premise fails. The next packet investigates why `sub_001A5742`'s
  start write is absent or overridden.
- **R-GP-READS:** all PASS, `last_GPRST & 3 == 3`, and `gp_ep_reads > 0` → the title
  consumes GP/EP state that is currently a stub (ruling B). → The next packet is the GP
  engine packet, and its scope must also cover those read offsets (named in the AC3
  table). Evidence of reaching the spin stays exploratory-grade for progress.
- **R-READY:** all PASS, `last_GPRST & 3 == 3`, and `gp_ep_reads == 0` → the start
  handshake is the only GP interaction, and the image is in place. → Design **A4b, the
  GP DSP engine packet**. Its positive controls:
  - the GPRST `0→3` transition triggers a bootstrap that loads scratch from `G`;
  - the loaded PRAM words equal the AC4 image;
  - GP instructions retire (count > 0).

  Its acceptance: `+0x810` is cleared by GP execution in a STRICT run. It needs the
  answers to Q1 and Q2 first.

## Open questions

- **Q1 (Advisor):** under ruling B, the guest reaches the spin only after 58 reads of
  `PIO_FREE` = constant `0x80` (stub). Would a future A4b strict claim "the GP cleared
  `+0x810`" be contaminated by that upstream stub dependence? If so, does A4b also need a
  sourced `PIO_FREE` model (reversal condition ii)? This packet is not blocked by Q1;
  A4b is.
- **Q2 (owner, through the Session/Advisor — licensing, workflow §3.4):** the only
  known DSP56300 core, xemu `hw/xbox/mcpx/apu/dsp/` (`dsp.c`, `interp/dsp_cpu.c` at xemu
  commit `67cc79e663038d1f55448c0f566b37dde016adf6`, O), carries **GPL-2.0-or-later**
  headers. The toolkit is MIT with LGPL-2.1-or-later APU files (`NOTICE`, O). Importing
  that core is a licensing choice for the owner. An independent implementation would be
  a large scope change. This packet is not blocked by Q2; A4b is.
- **Noted, not ruled (owner-reserved):** `0xFE820010` is labelled "GP sample counter" by
  the grandfathered row and `NV1BA0_PIO_FREE` by the VP map.

## Unknowns carried forward (not resolved here)

- Whether executing the image clears `+0x810` (A4b).
- The units of `0x5CC`.
- The semantics of GP offsets `0xFF00/0xFF04/0xFF10/0xFF14` and their EP equivalents.
- GP frame timing and realtime mode.
- SGE entries hold **VAs** (`0x803C0000`), because `MmGetPhysicalAddress` returns the VA
  (`kernel_bridge.c` `bridge_MmGetPhysicalAddress`, O). A4b's scatter-gather must map
  them.
- The order of the GPRST write relative to the `+0x810` write (accepted bounded
  uncertainty).

## Closure

- **Evidence index [SESSION]:**

  | Criterion | Artifact / hash | Result |
  |---|---|---|
  | AC1 | toolkit diff `c97ce2c..<commit>` hash; build/ctest log | |
  | AC2 | R1 `jsrf_run.log` SHA-256; matched lines | |
  | AC3 | inventory table in `docs/reviews/a4a-execution-evidence.md` | |
  | AC4 | R1 `process.dmp` SHA-256; compare output | |
  | AC5 | R1 `result.json`, `stacks.txt` SHA-256 | |
  | AC6 | R0 run directory; counts | |

  Record the selected decision row.
- Post-review edits reopen the affected criteria.
- An unrelated next stop is recorded as a follow-up; do not expand scope.
- The plan's `CURRENT PACKET` is updated only by the promotion and closure steps of
  workflow §5.
