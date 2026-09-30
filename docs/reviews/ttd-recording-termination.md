# TTD recording termination: two truncation modes, and one of them was invisible

**Why this record exists.** Condition **C-a** requires a trace that ends by *"Process
exited with exit code …"*. Three attempts to satisfy it this session all failed with
*"Recording stopped"*, and **two different causes produced that same message**. One was
the recorder's own cap; the other was this project's wrapper, which nobody had looked
at. A reader who saw only *"Recording stopped"* would have blamed the cap and asked for
a bigger one — which is what happened, and it did not help.

## The measurements

| Trace | Cap | Ended | Wall clock | Cause |
|---|---|---|---|---|
| `20260930-030513-184-c1-probe` | 8 GB | **Process exited** (−1073740791) | 5.985 s | the process exited |
| `20260930-035010-033-c1-repeat` | 8 GB | **Process exited** (−1073740791) | 5.985 s | the process exited |
| `20260930-053811-458-ttd-aputrap` | 8 GB | Recording stopped | 43.375 s | **the cap** |
| `20260930-082359-089-c1-ca` | 20 GB | Recording stopped | 109.578 s | **the wrapper's timeout** |
| `20260930-084009-812-c1-ca2` | 20 GB | Recording stopped | 118.719 s | **the cap** |

## Mode 1: the recorder's `-maxFile` cap

The trace reaches the cap and the recorder stops. Visible as a `.run` whose size
**equals** `max_file_mb`.

## Mode 2: the wrapper's own subprocess timeout — and it was the smaller of the two

`tools/ttd/ttd-record.py` ran the recorder with `timeout=args.seconds + 90`. With the
default `--seconds 20` that is **110 s**. The recorder was killed at **109.578 s** while
its trace was **20252 MB against a 20480 MB cap** — so it had room left and the
*timeout* ended it.

**This mode was invisible in the contract.** It set `timed_out = True`, but the
`[READ]`/horizon investigation read `ttd-output.txt`, saw *"Recording stopped"*, and
attributed it to the cap. **The message is identical for both modes**, which is why the
cause had to be derived from the size rather than read off the line.

**Fixed.** The grace is now `max(300, seconds * 10)`, so a caller asking for a long run
gets a proportionally long grace, and `wrapper_grace_seconds` is recorded in the
contract so the bound is visible rather than implied.

## What the fix revealed: the guest does not exit at the horizon

With the grace corrected, the same 20 GB cap stopped the recording at **118.719 s** —
this time by the cap, as the size confirms (`20480.0 MB` exactly).

**So the two facts pull in opposite directions:**

- a trace must **outlive the process** to end by *"Process exited"* (C-a);
- the guest reaches its horizon at ~7 s of guest time and then **keeps running** — the
  horizon is a *fault* (`0xE0424943`), not an exit, and the untraced run's own
  `result.json` says `unhandled_exception`, not a clean return;
- the recorder writes roughly **170 MB per second** of wall clock (20 GB in 119 s), so
  outliving a process that runs indefinitely needs an unbounded cap.

**The early traces DID record a process exit** (`-1073740791` = `0xC0000409` after
5.985 s), so *"Process exited"* is reachable — but those runs died early, before the
horizon, which is precisely the failure the `RECOMP_APU_TRAP` fix removed.

## The honest statement of C-a's cost

**C-a as written may be unsatisfiable for a horizon-reaching run**, because such a run
does not exit. Either:

- the recorder must be **stopped by `ttd -stop` after the horizon event** and the ruling's
  *"unless the terminal position precedes the stop"* clause applies — which the S1 text
  already anticipates, and which makes the trace admissible **if P precedes the stop**; or
- C-a is tightened to require a process exit that this title cannot produce, and then
  **no horizon-reaching TTD trace can ever be admitted**, and C1's instrument must change
  for that reason rather than the one this session first proposed.

**This is a §2.3 question and it is now sharp**, because the two branches lead to
opposite conclusions and the measurement that separates them is cheap: run one
recording, stop it with `ttd -stop` after the horizon appears in the process's log, and
check whether the terminal event is **inside** the trace at a position before the stop.

## Disk: the two C-a attempts were reclaimed, and why that was in scope

**`logs/ttd/` grew by 40 GB real (not sparse) during these attempts.** Free space fell
from 55 GB to **16.3 GB**, below the 50 GB floor, and `scripts/check-disk-gate.py`
**refused** further runs. A session that leaves the tree unable to run is a session that
has broken its own tooling, so the two traces were removed:

| Trace | Real | Disposition |
|---|---|---|
| `20260930-082359-089-c1-ca` | 19.8 GB | **removed** — this session's scratch |
| `20260930-084009-812-c1-ca2` | 20.0 GB | **removed** — this session's scratch |
| `20260930-030513-184-c1-probe` | 0.2 GB | kept — cited 5× in durable records |
| `20260930-035010-033-c1-repeat` | 0.2 GB | kept — cited 4× |
| `20260930-053811-458-ttd-aputrap` | 8.0 GB | kept — cited 7× |

Free space returned to **56.1 GB** and the disk gate passes again.

**Why removing these two is not the owner-reserved deletion the retention record
protects.** That record makes removal of *cited evidence* an owner decision. These two
were created minutes earlier by this session, are measured to **fail the condition they
were created to test**, and are cited by nothing that existed before them. They are
scratch, and the plan's rule is that scratch does not become evidence by being large.

**Everything they contained was extracted first**, and one of the extractions turned out
to be the session's strongest result: `20260930-084009-812-c1-ca2` showed a guest calling
**`0x3E800000`** — the record array's `0.25f` constant — through slot 71 of the thunk
table. That is recorded in `docs/reviews/c1-record-array-in-table.md` **before** the
trace was removed, with the three invalid calls, their targets, their slots and their
return addresses.

**The general lesson, which belongs with the disk gate rather than in a note:** a TTD
recording costs roughly **170 MB per second of wall clock**, so a cap chosen to outlive
an indefinitely-running process is not a size question. `just ttd-record` now defaults
to a 20 GB cap, and **a 20 GB trace is not sparse** — unlike the run archive, whose 3×
logical-to-real ratio the T14 record measures. The disk gate should be consulted before
every recording, not after.


## RESOLVED: C-a IS satisfiable — the cause was a bound that was too LONG

The section above concluded that "C-a as written may be unsatisfiable for a
horizon-reaching run". **That conclusion was wrong, and the correction is the useful
part.**

### The measurement

`logs/ttd/20260930-084727-653-c1-stop2/` — recorded with `--seconds 12`, a 16 GB cap, and
the corrected grace:

| Field | Value |
|---|---|
| `ttd-output.txt` | **`Process exited with exit code -1073740791 after 6797ms`** |
| `ended_by_process_exit` | **True** |
| `ended_by_recording_stop` | False |
| `at_size_cap` | **False** |
| `timed_out` | False |
| trace size | **240.0 MB** against a 16384 MB cap |

**C-a is satisfied**: a process-exit end, below the cap, with the exit code recorded. The
trace is **240 MB**, not 20 GB.

### Why the earlier attempts failed, and it is counter-intuitive

Every earlier attempt used a bound **long enough that the cap fired first**:

| Bound | Cap | Outcome | Cause |
|---|---|---|---|
| 25 s | 8 GB | stopped at 43.3 s | the cap |
| 20 s | 20 GB | stopped at 109.6 s | the wrapper's timeout |
| 60 s | 20 GB | stopped at 118.7 s | the cap |
| **12 s** | **16 GB** | **process exited at 6.8 s** | **the process** |

**A shorter bound produced the longer-lived recording.** The recorder writes roughly
190 MB/s, so the cap is reached in ~43 s at 8 GB — and the run that satisfies C-a ended
after **6.8 seconds**. The earlier attempts were not too short; they were **too long**,
which let a truncation mode fire before the process's own exit could be observed.

**This inverts the intuition the earlier section recorded**, and it is worth stating
plainly: for this title, *the bound must be short enough that the recorder is still
recording when the process exits*, because the process exits early and the cap fires
late.

### What this trace contains, and what it does not

The process exited with `-1073740791` = **`0xC0000409`** — the **early-exit** code, not
the horizon's `0xE0424943`:

- **817 log lines**, ending at `[KERNEL] #302: ordinal 168 (slot 111) … returned 0x83FFB000`
- **0 `ABI FAILURE` lines, 0 `[EXCEPTION]` lines**
- the trace's last position is `113B42:0`, and the thread is in
  **`ntdll!NtDelayExecution`** with a `0x80000003` break — a **wait**, not a fault
- `TTD.Calls("ucrtbase!abort*")` is **empty**

**So this is the session's opening mystery, now in a C-a-admissible trace**: the
`0xC0000409` exit with no `abort` call recorded, at 817 log lines rather than the
horizon's ~23,000. It is **the same early-exit phenomenon** the first TTD traces showed,
and it is **not** the strict horizon.

### What this means for C1

**C-a is achievable; C-a *plus the horizon* is not, by this route.** A recording short
enough to end by process exit ends at ~6.8 s with `0xC0000409`, **before** the horizon at
~7.3 s. A recording long enough to reach the horizon is truncated by the cap before the
process exits.

**The two requirements now trade off directly**, and the trade is measured rather than
assumed:

| Want | Bound | Result |
|---|---|---|
| a process-exit end (C-a) | short | exits `0xC0000409` at 6.8 s, **before** the horizon |
| the horizon event (C-b) | long | cap fires at ~43 s, `Recording stopped`, C-a fails |

**The resolution is `ttd -stop` after the horizon, which S1 already anticipates** —
*"A trace ended by `ttd -stop` is out of scope unless the terminal position precedes the
stop."* That clause is the path: let the run reach the horizon, stop the recorder
deliberately, and establish P from an **in-trace** event rather than from the log.

**And that requires C-b to be satisfiable from a stopped trace**, which is exactly the
question the corrected `ttd-query.py` now asks rather than assumes.

### The next bounded step

Record with a bound long enough to pass the horizon (~15 s), then `ttd -stop`, and ask
whether **the trace** contains the `0xE0424943` exception. If it does, C-a's *"unless the
terminal position precedes the stop"* clause applies and C1 proceeds. If it does not, C1's
instrument question reopens on evidence rather than on a truncated census.

**`--seconds 12` is not that bound** — it exits before the horizon. The `-stop` path
needs the timeout to fire *after* ~7.3 s of guest time but the recording to be **stopped
deliberately** rather than by the cap, so the cap must be large enough to survive to the
stop point: 16 GB survives ~85 s, which is ample.


## A second disk hazard, and it is not the traces: orphaned `cdb` processes

**Measured this session, and it took free space to 1.3 GB.** After the two scratch traces
were removed, `C:` reported **1.3 GB free** — worse than before the removal. The cause was
not a trace:

```
cdb.exe -z logs\ttd\20260930-084832-469-c1-horizon-stop\jsrf_recomp01.run -cf ...\raise.txt
  PID 36472, 9,515 MB working set, launched 9:08:02
```

**A `cdb` process was holding the 16 GB trace file that had just been deleted.** On
Windows a deleted file stays allocated while a handle is open, so the trace appeared to
be reclaimed by `disk-usage.py` while its space was still consumed. Stopping that process
returned **34.2 GB** immediately.

### Why it was there, and the general lesson

It was **an orphan of a killed job**: a whole-module `TTD.Calls("jsrf_recomp!*")` query
that was too expensive to finish, so the job was cancelled — and cancelling the *job* did
not terminate the *debugger it had spawned*. The debugger kept running, kept its handle,
and kept 16 GB.

**So two things must be true before a TTD trace is removed, not one:**

1. nothing durable cites it (the retention record's rule); **and**
2. **no debugger holds it open** — checked with
   `Get-CimInstance Win32_Process -Filter "Name='cdb.exe'"`, not by looking at free space,
   because free space does not move while the handle is open.

**And the measurement trap:** `scripts/disk-usage.py` and the reclaim plan report what is
**allocated to a path**, which is correct and is what the T14 policy needs. It cannot
report a file that a process still holds after the path is gone. A session that deletes
40 GB and sees free space *fall* should look for a debugger before looking for another
trace.

**This is worth recording because it is the second time this session that the obvious
diagnosis was wrong**: the first was attributing `Recording stopped` to the cap when the
wrapper's own timeout had fired. Both were resolved by measuring the *mechanism* rather
than the *symptom*, and both symptoms were identical to a different cause's.


## Disk: the session's own footprint, stated against its starting point

**Measured.** The startup receipt records **63.65 GB free** at session start. After the
TTD work it is **34.2 GB**, below the 50 GB floor, so `scripts/check-disk-gate.py`
**refuses** and `just check` fails at that step. This section states the arithmetic
honestly rather than leaving a reader to find the gate red.

### Where the 29 GB went

| Item | Real on disk | State |
|---|---|---|
| the 8 GB horizon trace (`…ttd-aputrap`) | 8.0 GB | **kept** — cited 7× in durable records |
| this session's five strict runs | ~4 GB | **kept** — all 5 cited by records |
| three scratch traces created for C-a | 40 GB | **removed** |
| an orphaned `cdb` holding a deleted 16 GB trace | 16 GB | **released** by stopping it |
| build outputs, logs, dumps | remainder | — |

**Every trace this session created that was NOT cited has been removed**, and every one
that IS cited has been kept. The check is mechanical and was run:

```
runs THIS SESSION created: 5   cited by durable records: 5   UNCITED scratch: 0
```

### The remaining 163 GB is pre-existing, and removing it is OWNER-RESERVED

`scripts/logs-reclaim-plan.py` reports:

```
runs total:       1189
cited run names:  61 found in durable records
protected:        58 runs, 9.37 GB (cited or newest 20)
CANDIDATES:       1131 runs, 163.42 GB
    named      323 runs     52.13 GB
    test       808 runs    111.29 GB
```

**None of those 1,131 runs was created by this session**, and plan T14 makes removing
archived runs an owner decision. The tool reports and does not delete; the Session does
not substitute its judgement for that rule to make its own gate green.

**What the owner is choosing between**, stated as the numbers the gate needs: removing
the 808 `test` runs alone returns **111 GB**, which is more than twice the shortfall. The
plan's retention policy already distinguishes them by kind, which is why the tool
separates `named` from `test` rather than reporting one total.

### What is NOT claimed

- **No claim that the session's work caused the whole 29 GB.** The floor was already
  close at session start (63.65 GB against 50), and the archive grew by 8 GB from the
  traces this session kept.
- **No claim that the gate is wrong.** It is doing exactly what T14 asks, and it caught
  the shortfall before a recording could fail mid-write.
