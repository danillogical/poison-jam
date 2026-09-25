## A4p — discover whether `PIO_FREE` is gate-only at the 28 direct reads of `0xFE820010`

**Class:** discovery   **Contract revision:** `A4p-r1`   **Status:** draft
**Starts from:** `A4b-r3` criterion `AC-PIO` drew two consecutive `INADEQUATE` verdicts on its exit/callee rule (`docs/reviews/a4b-r2-adequacy-review.md` B1/B2, `docs/reviews/a4b-r3-adequacy-review.md` B1). The Advisor's §5.5 ruling (`docs/reviews/a4b-pio-methodology-ruling.md`) replaced that rule with the calling-convention rule C1–C4 and moved the analysis here. `A4b` cannot be promoted until this packet is ACCEPTED with `O-GATE`. Q1 condition 3 (`docs/reviews/a4b-q1-advisor-ruling.md`) is discharged here.
**Baseline:** game `fa3d875` plus the commit that adds this packet; toolkit `0d7929c86771dd0b971941592fd4f15436116e82`. `game\default.xbe` SHA-256 `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C`. The generated tree `src/recomp/gen/` is as committed at baseline.
**Revision log:** `docs/reviews/a4p-revision-history.md` (non-authoritative).

### Question

Is the `PIO_FREE` stub value only a gate? That is, at each of the 28 direct reads of `0xFE820010` in `DSOUND`, is the loop a threshold re-poll whose polled value reaches no use under C1–C4? The answer decides whether `A4b` can proceed as written, or whether a modelled `PIO_FREE` must come first.

### Experiments

Static only. No build, guest run, or instrumentation. Working directory: game root. Tool: `python -X utf8 scripts\inspect-jsrf.py disasm|data|find` (original XBE). All output goes to **`docs/reviews/a4p-execution-evidence.md`**, the only file the executor writes.

**E0. Identity.** Record `(Get-FileHash game\default.xbe).Hash` and `git rev-parse HEAD`. If the hash differs from the baseline, stop (→ O-UNKNOWN).

**E1. Frozen population (`A4b-r3` step 1, unchanged).** Run `python -X utf8 scripts\inspect-jsrf.py find 0xFE820010` and record the output verbatim. It must report `28 occurrence(s)`, `0x001A2297` … `0x001A611D`, all `DSOUND`. For each hit, decode the instruction that starts at hit − 1 (`A1`) or hit − 2 (`8B /r`, `mod=00`, `r/m=101`). The start is the VA that has a `loc_<VA>:` label in the generated tree (E2). At hit `001A3FDC` only the `A1` form decodes (the byte at hit − 2 is `0x65`, a displacement), and the label confirms `001A3FDB`. A hit with neither decoding, or with no label, is recorded as `UNKNOWN` and never dropped. The expected instructions:
- `A1` (`mov eax,[0xFE820010]`), 10: `001A2296 001A22D0 001A3EB3 001A3FDB 001A4181 001A4E4C 001A5671 001A57CD 001A60B8 001A611C`.
- `8B /r` (`mov r32,[0xFE820010]`), 18: `001A2A7F(edx) 001A303D(edx) 001A30E4(edx) 001A3414(esi) 001A34EA(edx) 001A35AA(ecx) 001A3710(ecx) 001A3847(edx) 001A38F4(edx) 001A39AE(edx) 001A3A48(edx) 001A3B69(edx) 001A3C21(edx) 001A3F24(edx) 001A4242(edx) 001A4325(ebx) 001A4A34(edx) 001A579E(ebx)`.

**E2. Reconciliation by value, not text (`A4b-r3` step 2, unchanged).** Take every integer literal that is the operand of `MEM8(`/`MEM16(`/`MEM32(` in `src/recomp/gen/recomp_*.c`, decimal or hex, signed or unsigned. Normalise it mod 2^32, keep the ones equal to `0xFE820010`, and map each to the nearest preceding `loc_<VA>` label. Run this command and record its output verbatim. It was verified to run at planning, printing `28`, `0xFE820010 10`, `-25034736 18` and the 28 VAs above:
```powershell
$hits=foreach($f in Get-ChildItem src\recomp\gen\recomp_*.c){$lab=$null;$n=0;foreach($line in [IO.File]::ReadLines($f.FullName)){$n++;if($line -match 'loc_([0-9A-Fa-f]{8}):'){$lab=$Matches[1].ToUpper()};foreach($m in [regex]::Matches($line,'MEM(?:8|16|32)\(\s*(-?(?:0x[0-9A-Fa-f]+|\d+))u?\s*\)')){$s=$m.Groups[1].Value;$neg=$s.StartsWith('-');$a=$s.TrimStart('-');$v=if($a -match '^0x'){[Convert]::ToInt64($a.Substring(2),16)}else{[int64]$a};if($neg){$v=-$v};$u=(($v % 4294967296)+4294967296)%4294967296;if($u -eq 4269932560){[pscustomobject]@{file=$f.Name;line=$n;label=$lab;text=$s}}}}};$hits.Count;($hits|Group-Object text|%{"$($_.Name) $($_.Count)"});($hits.label|Sort-Object) -join ' '
```
(`4269932560` = `0xFE820010`. The PowerShell literal `0xFE820010` is a negative `int32`, so do not use it.) The result must be exactly the 28 E1 VAs, one-to-one, with no extras on either side. Control (known-bad): a text search for `MEM32\(0xFE820010u\)` alone finds 10 sites. Record that it does, and that 10 ≠ 28.

**E3. Indirect-access evidence (`A4b-r3` step 5, unchanged).** Record `find` for `0xFE800000` (at planning, 7 hits: `001A2E4A 001A2E5F 001A2E65 001A2EDE 001A2F80 001A2FA4` in `DSOUND`, plus **`0022DB72` in `.data`**), for `0xFE820000` (0 hits) and for `0x00020010` (1 hit, `.rdata 001E4888`). For each `DSOUND` hit, show from disassembly that the base/index registers come from the table at `0x001B9ECC`, and record that table's entries as data (`0x2054`…`0x2074`, voice-list registers). For `0022DB72` and `001E4888`, show either that nothing references them in a way that can form `0xFE820010`, or that they are not operands at all. An item that is unresolved, or that can reach `0xFE820010`, is UNKNOWN.

**E4. Per-site analysis (all 28).** For each site, name the containing function (the nearest preceding `sub_<VA>` definition in the generated tree) and the polled register `r`. Then:

*(a) Loop shape.* Disassemble the loop. It must be a threshold re-poll: `(v & ~3) < N` or `(v >> 2) < reg`, jumping back to the read when below. Record the form and N (or the register). The loop's own test is not a use. A store of the value inside the loop is a use. Any other shape makes the site UNKNOWN.

*(b) Exit trace.* From each loop exit, follow every path in execution order with a tracked set `T`, starting at `T = {r}`. `T` holds the registers and stack slots that may carry the polled value or a value derived from it. A read or write of a sub-register (`dl`, `dh`, `dx`, `cl`, …) counts as a read or write of the full register. For "written", only a full 32-bit write counts (for example `mov`, `movzx`, `xor r,r`, `pop`, `cdq` for `edx`); a partial write does not. Implicit reads count too: `div`/`idiv` read `edx:eax`; `rep`, `loop`, `jecxz` and shifts by `cl` read `ecx`.
- **Derivation.** An instruction that reads a member of `T` and writes a register adds that register to `T`.
- **Overwrite.** A write of a member register with a value not derived from `T` removes it.
- **Stack.** `push t` with `t ∈ T` adds that slot to `T`, and the matching `pop x` makes `x` tracked. A callee-save `push`/`pop` pair is not a use. Any other read of a tracked slot is a use. A tracked slot inside a call's argument area is UNKNOWN unless C2 resolves it (import prototype or traced callee). Slots of a frame that has returned are dropped.
- **Uses (→ site FAIL).** A member of `T` flows into: the value or address of a guest store or MMIO store; an address computation; a branch other than the loop's own test (that is, flags set by an instruction that reads `T`); or a resolved import argument.
- **C1 — volatile registers after a call.** On return from any call (direct, import or indirect), remove `ecx` and `edx` from `T`. Remove `eax` as well, unless the call was traced and the callee's final `T` contains `eax`. **Check** (required whenever C1 removed `ecx` or `edx`): in the caller, on every path from the return address to the next boundary (call, `ret`, or `jmp` out of the function), `ecx` and `edx` are not read before they are written. A read before a write breaks the convention at that site: UNKNOWN. `ebx`/`esi`/`edi`/`ebp` in `T` survive the call.
- **C2 — register arguments at a call** (applies when `ecx` or `edx` is in `T`; the direct-call check below applies to every member of `T`):
  - *Import* (`call [slot]`, where `slot` is a kernel thunk): take the ordinal as `MEM32(slot) & 0x7FFFFFFF` at the **exact slot VA the instruction references** (`inspect-jsrf.py data <slot> 4`). Map it through toolkit `src/kernel/kernel_thunks.c` and take the prototype from `src/kernel/kernel.h`. `__fastcall` reads `ecx` (arity ≥ 1), then `edx` (arity ≥ 2), then the stack. `__stdcall` reads the stack only. An argument read of a `T` member is a use. If resolution fails, the site is UNKNOWN. Observed: `[0x1C4004]` = `0x800000A1`, ordinal 161, `KfLowerIrql(KIRQL)` fastcall in `cl`.
  - *Direct call* (`call rel32`): disassemble the callee from its entry to its first boundary. A `push reg` of a callee-save register does not count as a read. If a tracked register is read before it is written, trace into the callee under these same rules (one level; a trace needing a deeper level is UNKNOWN). Otherwise the register is not an argument, and C1 applies at the return.
  - *Indirect call* (`call reg`, `call [mem]` other than a thunk slot) with `ecx` or `edx` in `T`, inside the site's own function: UNKNOWN.
  - *`jmp` out of the function* (tail jump): a `jmp rel32` is handled as a direct call whose `ret` is this function's `ret`. An indirect `jmp` with `T` non-empty is UNKNOWN.
- **C3 — leaving a function (`ret`/`ret n`) with `T` live.** A callee-save `pop` just before the `ret` removes that register from `T`.
  - `ecx`: dead. No x86 convention returns a value in `ecx`.
  - `edx`: dead unless it carries the high half of a 64-bit return. Check every direct caller's return address as in C1. If no caller reads `edx` before writing it or reaching its next boundary, `edx` is dead. If one does, trace into that caller. If that caller then reaches its own `ret` with `edx` **and** `eax` both unwritten since the call (a possible 64-bit pass-through), go up one more level under the same rule.
  - `eax`: it is the return value, so trace into every caller. The site is UNKNOWN beyond **three** caller levels above the site's function.
  - `ebx`, `esi`, `edi` or `ebp` in `T` at `ret` and not restored by a `pop`: an ABI violation, UNKNOWN.
  - *Callers.* Every direct call to the function's entry: generated-tree calls to `sub_<ENTRY>(` (outside `recomp_dispatch.c`), each confirmed as `call <entry>` by XBE disassembly. *Address-taken:* if `inspect-jsrf.py find <entry>` returns any hit (the entry appears as an immediate or data) and `edx` or `eax` is in `T` at the `ret`, the site is UNKNOWN. So is a site whose callers cannot be enumerated.
- **C4 — back-edges.** Keep a visited set of `(VA, T)` pairs. Reaching a pair already visited, with the same `T`, ends that path.
- **Termination.** A path ends when `T` is empty or at a C4 revisit. It ends UNKNOWN at any instruction the executor cannot decode or resolve. A **site PASSes** when (a) holds and every path ends with no use and no UNKNOWN. It **FAILs** on any use. Otherwise it is **UNKNOWN**, with the VA and the clause that failed.
- **Cross-checks.** The ruling worked two sites by hand (the Session reproduced the disassembly): `001A38F4` (`edx`), where `sub_001A1BAF` never reads `edx` so C2 and C1 empty `T`, and `001A34EA` (`edx`), where C3 finds `edx` dead at the return address `001A363E`. The executor evaluates these like every other site. A disagreement is recorded, and the site is UNKNOWN.

**E5. Deliverable.** In `docs/reviews/a4p-execution-evidence.md`, record:
- E0–E3 outputs verbatim.
- One table row per site, all 28: site VA; `r`; function entry; loop form and N; each exit path as a list of instruction VAs; `T` at every call, `ret` and `jmp` out; each C1 check (return address and the VAs read up to the boundary); each C2 resolution (slot/ordinal/prototype, or callee entry-to-boundary VAs); each C3 check (caller return addresses examined, and the result of `find <entry>`); C4 revisits; and the result (PASS, FAIL with the use VA, or UNKNOWN with the VA and clause).
- The selected outcome row.

### Outcomes (evaluate in order; first match wins)

| ID | Observed | Means | Next packet |
|---|---|---|---|
| O-UNKNOWN (identity) | XBE hash mismatch; E1 count ≠ 28; E2 not exactly the 28 VAs one-to-one; an artifact missing or unreadable; the executor stopped | population or identity not established | Re-plan (Planner). `A4b` stays unpromoted. |
| O-DATA | E0–E2 clean, and at least one site FAILs (a use is recorded) | the stub value may shape guest data; Q1 condition 3 not discharged | A `PIO_FREE`-model packet comes **before** `A4b` → Advisor. |
| O-UNKNOWN (sites) | E0–E2 clean and no FAIL, but some site is UNKNOWN or some E3 item is unresolved | not decided for the listed sites | Planner + Advisor decide, **per listed site**, whether to close it by targeted reads (a follow-up discovery revision) or to treat it as `O-DATA`. `A4b` stays unpromoted. |
| O-GATE | E0–E2 clean; all 28 sites PASS; E3 clean | gate-only under C1–C4 | `A4b` revision deletes `AC-PIO` and `R-PIO-DATA`, and adds the precondition "`A4p` ACCEPTED with `O-GATE` on XBE SHA-256 `FD190557…EF9C`" plus the claim limits below. Then the `A4b` adequacy review and promotion. |

### Limits and stops

- **Does not establish** (even at `O-GATE`):
  - It rests on the **inferred** premise that DSOUND follows the standard x86 register convention: `eax`/`ecx`/`edx` volatile, values returned only in `eax` or `edx:eax`, and `ebx`/`esi`/`edi`/`ebp` never used as hidden arguments. C1 and C3 check this at every boundary the analysis relies on, not everywhere.
  - It cannot see a whole-program custom convention that returns a value in `edx` and passes it straight on to a later callee with no instruction touching `edx` in between.
  - It covers only the 28 direct reads. It does not cover register-indirect, computed or table-driven access beyond E3, timing, or whether `0x80` is the true device value.
  - It is static. It never satisfies a strict criterion and never claims that audio, the GP, or anything else works (§5.8). An optional exploratory run that varies the stub value would be corroboration only. It is not part of this packet and can never stand in for `O-GATE`.
- **Stop if:** the XBE hash differs; `inspect-jsrf.py` fails to run; the analysis seems to need a code change, build, guest run or instrumentation; or a C1/C3 check fails at so many boundaries that the premise looks false for DSOUND as a whole. Record the stop and select O-UNKNOWN (identity), naming the reason. Do not improvise a rule.
- **Closure:** no instrumentation exists. Artifacts: `docs/reviews/a4p-execution-evidence.md` (SHA-256 recorded at acceptance). The acceptance reviewer confirms that the commands reproduce and re-checks the row selection.
