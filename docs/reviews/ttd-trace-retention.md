# TTD trace retention: exact candidates, nothing deleted

Plan T14 makes deletion an owner decision. This records the **measured** state so the
owner is choosing between numbers rather than approving a guess.

## Why this matters now

The disk gate's floor is 50 GB and the host is at **55.2 GB free**. Two TTD traces
have been **superseded by measurement** this session, and together they are 10.2 GB —
more than the margin above the floor.

## The traces

| Trace | Size | Status |
|---|---|---|
| `logs/ttd/20260930-053811-458-ttd-aputrap` | 12.88 GB | **KEEP** — the horizon-reachable trace; every C1 measurement this session cites it |
| `logs/ttd/20260930-030513-184-c1-probe` | 5.09 GB | superseded |
| `logs/ttd/20260930-035010-033-c1-repeat` | 5.09 GB | superseded |

**Total: 23.06 GB.** `logs/ttd/` is gitignored (`/logs/` in `.gitignore`), so none of
it is committed, and none of it can be pushed.

## Why the two are superseded, stated precisely

Both were recorded with `RECOMP_GPU_ACK=0` **only**. The strict-horizon ledger's
2026-09-30 rows establish that this configuration does **not reach the horizon** even
untraced — 20 s and 90 s runs both hit the collector's deadline with 0 invalid ICALLs
— because `RECOMP_APU_TRAP=1` is required. So both traces stop at 577 log lines
**before** the terminal event, which is the one thing a C1 trace must contain.

Their measured content is already preserved in durable records:

- the 29-alias query result, the install positive, the mirror negative and the
  trace-live control: `docs/reviews/rulings/ttd-query-decision-input.md`;
- the `0xC0000409` exit, its corrected attribution, and the reason it was
  mis-attributed: `docs/reviews/ttd-recording-exit-finding.md`;
- the write census, the mirror exclusion, the W-d control and the non-TTD
  confirmation: `docs/reviews/c1-terminal-slot-finding.md`.

**Every conclusion drawn from them is in a durable record**, which is what makes them
candidates rather than evidence. That is the property T14's retention policy is built
on: protect what a record cites, and a trace whose findings are recorded is not
itself cited.

## What is NOT proposed

- **The horizon-reachable trace is not a candidate.** It is the artifact every C1
  measurement cites, and re-recording it costs 12.9 GB and ~25 s of recording plus
  the disk headroom to hold both.
- **Nothing is deleted by this record.** The `just disk` recipe reports the same
  numbers; removal is the owner's call.

## The tool's own verdict, and why it says "protect all three"

`python -X utf8 scripts/logs-reclaim-plan.py --runs-root logs/ttd`:

```
runs total:       3
cited run names:  56 found in durable records
protected:        3 runs, 8.42 GB (cited or newest 20)
CANDIDATES:       0 runs, 0.0 GB
```

**The tool protects all three, and it is right to.** Each is named by a durable
record — that is precisely the property the retention policy is built on, and the
tool cannot tell "cited because the record needed this trace" from "cited because the
record explains why this trace is superseded". A human reading the records can.

**The allocation figure is also worth noting: the tool reports 8.42 GB where
`Get-ChildItem` sums to 23.06 GB.** The tool is sparse-aware
(`GetCompressedFileSizeW`) and the `Get-ChildItem` sum is logical size, so the TTD
`.run` files are heavily sparse — the same ~3x overstatement the plan's T14 row
records for the run archive (2,735 GB logical against 172 GB allocated). **The
sparse-aware number is the one that matters for free space.**

## The reclaim options, with their exact effect

| Option | Reclaims (sparse-aware) | Resulting free space |
|---|---|---|
| remove both superseded traces | ~3.7 GB | ~59 GB |
| remove neither | 0 | 55.2 GB (above the floor) |

**The sparse-aware figure is the honest one.** The logical 10.18 GB those two traces
appear to occupy overstates what removing them would return, which is exactly why the
plan's T14 row insists on measuring allocation rather than size.

## Why the plan's general reclaim policy does not decide this

`scripts/logs-reclaim-plan.py` protects runs cited by durable records and the newest
20, and reports candidates by kind. TTD traces are **not** under `logs/runs/`, so that
policy does not cover them — which is why this record exists rather than an automatic
sweep. The same principle applies: a trace whose findings are recorded is not itself
cited, and the horizon-reachable one is.
