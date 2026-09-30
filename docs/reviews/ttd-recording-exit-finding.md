# TTD recording: the traced process exits `0xC0000409` before the horizon

**Question this answers.** Plan §7's C1 attributes the terminal slot write with a
TTD recording. The W11 ruling (`ttd-query-decision-input.md`) admitted TTD traces as
a lossless class but **not** the delivered artifact, because the traced run never
reaches the horizon. This record establishes *why*, so the next session starts from
the finding rather than re-deriving it.

**Status:** discovery finding, not a packet. It blocks C1 and hands the next Planner
a bounded subject.

## The measurement

| | |
|---|---|
| Traces | `logs/ttd/20260930-030513-184-c1-probe/`, `logs/ttd/20260930-035010-033-c1-repeat/` |
| Binary | `build/Release/jsrf_recomp.exe`, sha256 `134bda3a…` (identical in both, and to the ledger strict run) |
| Exit | `0xC0000409` (`STATUS_STACK_BUFFER_OVERRUN`) after 5765 ms / 5985 ms |
| Control | `logs/runs/20260930-034938-427-ttd-exit-control/` — **same environment, no TTD**, `outcome=diagnostic_deadline`, exit 3, `checkpoints_passed=true` |

The control is the discriminating one: the environment that dies under recording runs
to its deadline without it. **The failure is recording-induced or
recording-correlated, not a property of the build.**

## The death site (measured in both traces, identical)

Thread 0's final PC is `jsrf_recomp!body_00196967+0x40d`, guest PC **`0x00196A29`**,
with the same guest stack in both traces:

```
body_00196967+0x40d      <- the faulting PC
sub_00196967+0x6f
body_00194676+0x4cf  ->  sub_00194676+0x6f
body_00194ADD+0x8c7  ->  sub_00194ADD+0x78
body_00192090+0x3d1
```

Trace 1 stopped at `99FBB:0`, trace 2 at `99CF5:0`. Registers at death:
`edi = 0xFD000000` (the NV2A MMIO base), `esi = 0x00F7FC5C`.

The guest instruction at `0x00196A29` is, from the original XBE:

```
00196A1D mov dword ptr [edi + 0x2214], edx
00196A23 mov dword ptr [esi + 0x19c], eax
00196A29 lea ecx, [edi + 0x100214]     <- the final PC
00196A2F mov eax, dword ptr [ecx]      <- never reached
```

So the guest dies **entering an NV2A register access** (`edi + 0x100214` =
`0xFD100214`), immediately after two ordinary stores.

## Why the log stops at 577 lines, exactly

The TTD `jsrf_run.log` is substantively identical to the control's through line 577
(the only differences are paths, timestamps and thread ids). The control's **line
578** is `[RECOVERED] 0x00194520 returned; ABI verified (ESP/EBX/ESI/EDI)` — absent
from the TTD log because the trace ends first.

That is explained by the death site: `sub_00194520` is called ~190 bytes after
`0x00196A29`, so it was never entered. **Its ABI-failure assertion is therefore not
the cause**, and neither is any other abort path downstream of the death PC.

## Abort paths ruled out by measurement

TTD call queries returned **zero calls** for each of: `ucrtbase!abort`,
`kernelbase!RaiseFailFastException`, `ucrtbase!_invoke_watson`,
`kernelbase!RaiseException`, `ntdll!RtlRaiseException`,
`kernelbase!UnhandledExceptionFilter`, `ntdll!NtTerminateProcess`.

The stack cookie is intact: `__security_check_cookie` ran exactly three times and
returned normally each time; `__report_gsfailure` was never reached. The absence of
`RaiseException` also clears `recomp_icall_fail_log` and
`recomp_icall_not_code_log` (`src/recomp_manual.c`).

⇒ `0xC0000409` came from an in-process `int 29h` (`__fastfail`) that is **not a
symbolicated call**, which is why `TTD.Calls` cannot see it.

## Correction to the first reading

The first examination of the trace reported the stop "inside `nv2a_ack_thread`"
(`ntdll!NtDelayExecution` ← `jsrf_recomp!nv2a_ack_thread+0x693`). **That is a
reporting artifact.** `+0x693` is the `Sleep(0)` at `xbox_memory_layout.c:1256`, and
with `RECOMP_GPU_ACK=0` the whole acknowledgement block is skipped, so that thread
only spins. cdb showed it because **TTD reports the last-scheduled thread at the
trace tail, not the faulting one.** Recorded because the wrong reading was written
down first and would otherwise be inherited.

## Ranked candidates

1. **INFERRED** (locus **MEASURED**): TTD's `TTDRecordCPU` emulation perturbs the
   guest thread inside the recovered GPU/PLL setup path
   (`body_00196967`, guest `0x00196967–0x00196A65`), driving it into a `__fastfail`.
   Both traces stop at the same instruction, so it is deterministic.
2. **INFERRED**: TTD raises the fastfail on the guest's behalf (an unsupported
   instruction or a recorder-internal inconsistency) and attributes the exit code to
   the guest. Not yet separated from 1.
3. **MEASURED-EXCLUDED**: the recovered-code ABI `abort()` assertions in
   `src/recomp/recovered/recovered.c`.

## The one cheapest discriminating experiment

No new recording is needed — walk thread 0 inside the existing trace:

```
cdb -z logs\ttd\20260930-035010-033-c1-repeat\jsrf_recomp01.run -cf <script>
  !tt 100
  dx -g @$cursession.TTD.Calls("jsrf_recomp!body_00196967")   # note TimeStart
  !tt <TimeStart>
  ~0s
  p                                                          # step to the fault
```

It separates candidate 1 (the guest executes into a failfast) from candidate 2 (TTD
raises it). Script files must live outside the repository.

## What this does not establish

- It does **not** show the recompiled guest is correct: the control run reached its
  deadline with **0 invalid ICALLs**, so it did not reach the horizon either. Neither
  run exhibits the terminal event.
- It does **not** name the cause of the failfast. It locates it to one instruction
  and excludes every symbolicated abort path.
- A TTD trace is not an archived strict run (W11's S8), and nothing here changes
  that.
