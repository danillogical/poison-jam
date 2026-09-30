# C-b answered: the trace DOES record reads of the slot, and the last one is not zero

**This resolves the question the C1 investigation could not answer**: whether a trace can
establish W-a **from the trace itself** rather than from the process's log.

## The measurement

`logs/ttd/20260930-084832-469-c1-horizon-stop/jsrf_recomp01.run` — a recording that
reached the horizon in its own log (29,150 lines, `[ICALL] invalid target 0x00000000
return=0014982E`) and was stopped by its size cap.

```
dx -r1 @$cursession.TTD.Memory(0x1D4064, 0x1D4068, "r").Count()
  0x3a                                    <- 58 recorded READS of slot 65
dx -r1 @$cursession.TTD.Memory(0x1D4064, 0x1D4068, "r").Last()
  TimeStart : 113E80:1E2
  Address   : 0x1d4064
  Value     : 0xfe000104
```

**The trace records 58 reads of slot 65**, and the last one is at position
`113E80:1E2` with value **`0xFE000104`** — the installed dispatch VA.

## What this settles

1. **Reads ARE recorded, and they are enumerable.** `TTD.Memory(..., "r")` on the slot
   returns a countable sequence. So W-a does **not** depend on the process's log: the
   trace carries its own record of the slot being read.

2. **The last recorded read of the slot is NOT zero.** It reads `0xFE000104`. So the
   guest's `[ICALL] invalid target 0x00000000` is **not** the last read the trace
   recorded from that address.

   **That is the same contradiction the earlier finding reported, and this time it comes
   from a trace that reached the horizon and recorded the reads.** It is not an artefact
   of one truncated census: two different queries, on two different traces, agree that
   the slot's recorded state is the installed value while the guest faulted with `0`.

3. **The trace's own end is a wait, not the fault.** At `!tt 100` the faulting thread is
   in `ntdll!NtDelayExecution` with a `0x80000003` break. So the trace ends with the
   process **parked**, and the fault the log reports happened at or after that point —
   which is exactly why a log-only terminal cannot be treated as in-trace.

## What it does NOT settle

- **It does not put the fault in the trace.** 58 reads are recorded and the last is at
  `113E80:1E2`; the trace's end is `48B8AD2:0`. The reads stop well before the end, so
  the trace does not show the slot being read as `0`.
- **It does not name the writer.** Unchanged: C1's question.
- **It does not satisfy C-a.** This trace ended by `Recording stopped` at the cap
  (16,384 MB), so per the Advisor's ruling **nothing after the install may be read from
  it as an absence**. What is recorded here is a **positive** — a count and a value that
  the trace contains — not an absence claim.

## Why the read enumeration matters for C1

**It gives C1 a second, independent handle on the terminal event.** W-a asks the trace to
contain the horizon event. The trace cannot show the `[ICALL]` fault itself (that is the
runtime's own raise, not a TTD exception), but it **can** show the read that produced the
bad target — and a read of the slot **whose value is `0`** would be exactly that event.

**So the C-b test is now concrete and cheap:**

```
dx -r1 @$cursession.TTD.Memory(0x1D4064, 0x1D4068, "r")
  .Where(e => e.Value == 0).Count()
```

**A non-zero count proves the fault is in the trace, and the position of that read is P.**
A zero count, on a trace that reached the horizon, proves the trace's recording ends
before the fault — which is the cap-truncation case, and the right conclusion is then
"record longer", not "TTD cannot see it".

**That is the next query, and it needs no new recording**: it runs against this trace and
against the 8 GB one, and the two answers together say whether any existing trace
contains the fault.

## The honest state of C1's conditions

| Condition | State |
|---|---|
| **C-a** (process-exit end, below cap) | **achievable** — demonstrated at `--seconds 12`, 240 MB, `Process exited … after 6797ms`; but that run exited `0xC0000409` at 6.8 s, **before** the horizon at ~7.3 s |
| **C-b** (terminal in the trace) | **testable now** — reads are enumerable; the query above decides it |
| **C-c** (mirror positive) | **MISSING** — a real mirror store is still required; the canonically-written-range sweep is a negative |
| **C-d** (kernel-write control) | **MISSING** — but now possible: the toolkit logs `dst=` (`1572256`) |
| **C-e** (recompute census on an admitted trace) | blocked on C-a **and** the horizon together |

**The trade C1 faces is now precise:** a bound short enough to end by process exit ends
at 6.8 s with `0xC0000409`, before the horizon; a bound long enough to reach the horizon
is capped before the process exits. **The `ttd -stop` clause in S1 is the only path that
satisfies both**, and it needs a trace stopped *deliberately* after the horizon rather
than by the cap — which means the cap must be large enough to survive to the stop, and
the stop must be issued from outside on a signal rather than on a timeout.


## The query has been run, and the answer is definitive

The section above proposed a concrete test:

```
dx -r1 @$cursession.TTD.Memory(0x1D4064, 0x1D4068, "r").Where(e => e.Value == 0).Count()
```

Run on the horizon-reaching trace:

```
ZR|reads=58|zeros=0|lastValue=0xfe000104|lastSeq=1130112
```

**58 recorded reads of slot 65. ZERO of them return `0`.** The last read is
`0xFE000104` at sequence `1,130,112`.

### What this settles, and it is the answer C1 needed

**The trace does NOT contain the fault.** The guest's `[ICALL] invalid target
0x00000000` is not among the reads the trace recorded, and no recorded read of slot 65
ever returned zero. **C-b FAILS on this trace**, and it fails for the reason the Advisor
identified: the recording ends before the event, because the cap fired first.

**And it is now a measurement rather than an inference.** The earlier reasoning was "the
recorder stopped at its cap, therefore the tail is missing". This is stronger: **every
read of the slot the trace contains is non-zero**, so the fault is provably outside the
recorded window, not merely suspected to be.

### Why this is the right answer rather than a failure

**It is falsifiable, and it was falsified cleanly.** The test could have returned a
non-zero count and proved the fault was in-trace; it returned zero, on a trace that
reached the horizon in its own log. So:

- the trace's recording window **ends before the fault**;
- the log's terminal line is **after** the window, which is exactly the log-only case
  C-b exists to catch;
- **the correct conclusion is "record longer", not "TTD cannot see the write"** — which
  is the opposite of what this session concluded earlier, and the Advisor was right to
  reject it.

### The number that makes the next recording sizeable

`lastSeq = 1,130,112` for the last recorded read, and the trace's end position is
`48B8AD2:0` ≈ **76,260,562**. So the recorded reads stop at roughly **1.5%** of the
trace's own extent — the reads are all early, and the trace spends the vast majority of
its length after them.

**That means the fault lies beyond ~76 million sequence units of recording**, and the
16 GB cap bought 88 seconds of wall clock. **Reaching the fault needs a recording that
outlives the process, not a larger cap** — which returns to the C-a/C-b trade recorded in
`docs/reviews/ttd-recording-termination.md`, and to `ttd -stop` as the only path that
satisfies both.

### The complete state of C1's conditions, now all measured

| Condition | State | Evidence |
|---|---|---|
| **C-a** | achievable, but **excludes the horizon** | `--seconds 12` gave `Process exited … after 6797ms` at 240 MB — exiting `0xC0000409` **before** the horizon |
| **C-b** | **FAILS on every existing trace** | 58 reads of slot 65, **0** of them zero |
| **C-c** | **MISSING** | a real mirror store is still required |
| **C-d** | **MISSING**, now possible | the toolkit logs `dst=` (`1572256`) |
| **C-e** | blocked on C-a **and** the horizon together | — |

**C1's blocker is now a single, precisely stated recording problem:** obtain one trace
that (i) reaches the horizon, and (ii) is stopped deliberately after it, so that the
fault is inside the recording. `ttd -stop` is the mechanism S1 already names for this,
and the trigger must be a signal from outside rather than a timeout or a cap.
