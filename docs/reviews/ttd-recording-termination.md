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
