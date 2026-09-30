# Ruling ledger: TTD query output as a decision input

**Question.** Can the reproducible TTD query artifact produced by
`tools/ttd/ttd-query.py` + `tools/ttd/writes.js` be admitted, under
`docs/jsrf-run-profiles.md`'s evidence rule, as a **lossless decision input**, so
that a criterion in C1 may select a row from its output?

**Ruling (W11).** TTD traces are **admitted as a lossless record class**, but only
under conditions S1–S8 below. **The current artifact is NOT admitted, and C1 may
not select a row from it.** Where any condition fails the result is `UNKNOWN`: it
never names a writer, and it never selects `O-UNKNOWN` ("read path").

## Attribution

| | |
|---|---|
| Role | Persistent advisor (`docs/agent-workflow.md` §2.3) |
| Child ID | `251fe018-dfdc-4608-89b8-ca7a1e8d27ec` |
| Provider / model | `claude` / `claude-opus-5-5` |
| Effort | `high` |
| Route shape | `CONTINUABLE_PINNED` — probed in this session: turn 1 read a named file and reported a fact omitted from the brief; turn 2 to the **same child** returned the turn-1 marker `ADVISOR-PROBE-7c3e9a` with no file access. The child is listed in the continuable-agent listing. |
| Ruling text | Recorded verbatim in the session record for 2026-09-30; this file records the ruling and its conditions. |

## Ruling text

> **RULING:** CONDITIONALLY ADMISSIBLE AS A CLASS; THE CURRENT ARTIFACT IS NOT
> ADMITTED, AND C1 MAY NOT SELECT A ROW FROM IT.
>
> A full-mode TTD trace is admitted as a lossless record class, but only for the
> scope below. `run-profiles.md` has to be amended to say so, because the rule's
> text names only "write-once latch / uncapped counter written by the code that
> performs it". The query output is a decision input only when every condition
> S1–S8 holds and is evaluated mechanically. Any failure selects UNKNOWN. It never
> selects a named-writer row and never selects O-UNKNOWN ("read path").

## Findings the brief did not cover

These were raised by the Advisor against the delivered artifact and are the
reason the artifact is not admitted. **F1, F2 and F6 were independently
re-measured by the Session and confirmed**; F4 was confirmed by reading the
source.

- **F1 — the trace stops long before the horizon.** The recorded process exited
  with `0xC0000409` after 5765 ms (`ttd-output.txt`); `timed_out` was false. Its
  log is 577 lines against the ledger run's 16,636, and the ledger run's first
  `[ICALL] invalid target` is at line 16,633. **Confirmed by re-measurement**, and
  **reproduced** on a second recording (`20260930-035010-033-c1-repeat`): same
  exit code, same 577 lines. The trace therefore cannot answer the question C1
  asks, and the "install write only" result is expected rather than informative.
- **F2 — settings differ from the ledger run.** The exe sha256 matches
  (`134bda3a…`), but the ledger run also set `RECOMP_APU_TRAP=1` and
  `RECOMP_KERNEL_LOG_BUDGET=100000`. **Confirmed.** `RECOMP_APU_TRAP` is a real
  capability, not observation (`docs/jsrf-run-profiles.md`), so the two runs are
  not a single-variable comparison.
- **F3 — the per-alias limit was 16, not the default 64.**
- **F4 — `ttd-query.py` never evaluates the known positive.** `THUNK_TABLE_*` is
  defined but unused, and the script returns 0 even when controls fail, aliases
  are missing, an alias is truncated, coverage is INCOMPLETE, or cdb exits
  nonzero. **Confirmed by reading the source.**
- **F5 — nothing shows the mirror queries work.** The only positive control is at
  alias 0, so zero writes at mirror 1 cannot distinguish "mirror queries work and
  saw nothing" from "mirror queries are broken". `O-ALIAS` depends on exactly
  this. The Advisor states the control is **missing and not waived**.
- **F6 — the mirror count is hardcoded.** `aliases.py` fixes
  `XBOX_NUM_MIRRORS = 28` and never checks it against the run's own `expected`
  count. Toolkit `xbox_memory_layout.h:455` agrees today; a future change would be
  reported as COMPLETE while missing views.

## Losslessness — where the class can drop the deciding event

Full-mode TTD records every user-mode store of every thread from launch. The
ruling identifies these ways it can still lose the deciding event:

| | Condition | Status |
|---|---|---|
| a | Ring mode (`-ring`) drops the head | Not used; no `-ring` in the command |
| b | Reaching `-maxFile` ends recording, dropping the tail | 216 MB against an 8192 MB cap — not reached, but nothing checks it mechanically |
| c | A `ttd -stop` at the timeout, or an early exit, truncates the tail | **This happened** (F1) |
| d | Kernel-mode writes are not instruction writes, so `TTD.Memory "w"` may not report them | Uncertain; needs a control (W-d) |
| e | Another process writing the section | Not expected: the RAM section is unnamed and pagefile-backed, and the game never uses `xbox_GetMappingHandle` (inferred) |
| f | Overlapping or wide stores starting before the queried 4 bytes | Undemonstrated; values are also truncated to 32 bits |
| g | First-N enumeration (`--max-hits`) | **Exactly a first-N log**, which the evidence rule classes as observation only. It drops the deciding write first. |

**Verdict:** lossless for user-mode instruction writes under (a)–(c). The current
query projection is **not** lossless because of (g) and the unevaluated flags, and
the class is not lossless for (d).

## Boundedness

The alias-summary table's key universe is finite and source-derived (29 alias
indices, keyed by the property `O-ALIAS` classifies) and qualifies **once F6 is
fixed**. The per-event list is keyed by event identity and grows with run length;
`--max-hits` makes it **capped, not bounded**, so it is observation only.

An admissible bounded projection is, per alias: an **uncapped count**; the
**single last write before the terminal read position P** (address, size, full
value, IP, tid, position); and the **value at P**. That is 29 fixed-shape rows.

## Absence witnesses

Known-negative, trace-live and the install-positive are necessary but **not
sufficient** — F1 shows all three passing on a trace that cannot answer.

- **W-a — terminal-in-trace.** The trace contains the strict horizon event (the
  fatal `[ICALL] invalid target` / `0xE0424943` raise, or the terminal read of the
  slot) at position P, and the trace ends by process exit after P with no
  `-maxFile` or `-stop` truncation.
- **W-b — value consistency.** TTD's value at P equals the full value of the last
  found write at any alias before P. Equal → the positive attribution is sound.
  Different, or the slot changed with no write found → an unrecorded writer
  (kernel/external/overlap) selects an explicit **"unattributed writer"** row,
  never `O-UNKNOWN`. `O-UNKNOWN` ("read path") is selectable **only** when the
  value at P equals the last recorded write.
- **W-c — a mirror-alias known positive**: a seeded fixture store through a mirror
  VA, or a naturally occurring mirror write in the same trace, found at the right
  alias index.
- **W-d — a kernel-write control**: a known `NtReadFile` into a known guest
  buffer. Either the query reports it, retiring (d), or (d) stays a stated
  exclusion that W-b catches.

## Scope of admission (conditions, not advice)

- **S1.** Full mode only; no `-ring`; trace size below `-maxFile`; the `.out` shows
  a process-exit end with the exit code recorded. A trace ended by `ttd -stop` is
  out of scope unless P precedes the stop.
- **S2.** W-a holds; the traced run's exe sha256 and profile settings match a
  ledger strict run, or each difference is recorded (F2's `RECOMP_APU_TRAP`
  difference must be resolved or recorded); the thunk table is overwritten
  **inside** the trace, before P.
- **S3.** All 29 alias summaries present, none truncated, counts uncapped,
  `mapped == expected == XBOX_NUM_MIRRORS`, and cdb exits 0. Otherwise `UNKNOWN`.
- **S4.** The deciding write is "the last write before P", not enumeration order.
- **S5.** Every pass runs and checks the install positive (alias 0, `0xFE000104`
  from `xbox_kernel_bridge_init`), the mirror positive (W-c), the known negative
  and trace-live, all evaluated mechanically.
- **S6.** W-b decides between the attributed, unattributed and `O-UNKNOWN` rows.
- **S7.** The artifact binds the trace sha256 (matching `record-contract.json`),
  the sha256 of `writes.js` and `ttd-query.py`, and the cdb version; the query is
  **rerun** from those, not transcribed (W5).
- **S8.** A TTD trace is **not** an archived strict run. Under this ruling it may
  decide C1's attribution rows only. It cannot satisfy strict boot/liveness/horizon
  criteria, and it cannot add a line to
  `docs/reviews/strict-horizon-ledger.md` by itself.

## Basis

- **Observed:** F1–F6; the 29-alias derivation and the fail-closed exit 3; no
  `-ring`; a 216/8192 MB trace; a process-exit end; `ReadFile` writing into the
  guest buffer.
- **Inferred:** full-mode TTD records every in-process user-mode store; the
  clobber postdates this trace's end (cross-run).
- **Uncertain:** whether `TTD.Memory` reports kernel-mode writes (d) or
  overlapping wide stores (f); the cause of `0xC0000409` under TTD.

## Reversed by

- A seeded user-mode store (canonical or mirror) that full-mode TTD fails to
  report → **NOT ADMITTED** outright.
- A W-d control showing kernel writes **are** reported → drop exclusion (d).
- A control showing overlapping stores are reported with their full value → drop
  (f).
- Evidence that `0xC0000409` is recording-induced and prevents any traced run from
  reaching the horizon → the class stays admitted, but **C1 via TTD is BLOCKED**
  and must be re-planned.

## Recorded in

- `docs/jsrf-run-profiles.md` → "Unconditional modeled hardware causes" →
  "Evidence rule": the paragraph "TTD trace query (W11)" stating the lossless
  class, S1–S8 and the W-a/W-b witnesses.
- This ledger.
- `plan-jsrf-bare-minimum.md`: §6 row W11's check, and §7 C1's acceptance amended
  to require W-a and W-b, with `O-UNKNOWN` selectable only under W-b equality.
- Tool changes needed before C1 (a DeepSeek chore; T1 reopened): the mechanical
  verdict with a nonzero exit on any failed condition; a last-before-P projection
  with an uncapped count; full-width values; the mirror positive; the hash
  binding; and the `expected == XBOX_NUM_MIRRORS` check.

## Session measurement added after the ruling

The Session ran the discriminating control the ruling's "Reversed by" list names.
Same environment as the traced run, **no TTD**:

```
RECOMP_GPU_ACK=0 python -X utf8 scripts/run-jsrf.py --seconds 8 --label ttd-exit-control
-> outcome=diagnostic_deadline exit_code=3 checkpoints_passed=true
```

Two TTD recordings of the same binary both exited `0xC0000409` at 577 log lines;
the same environment without TTD ran to its deadline. **The failure is
recording-induced or recording-correlated, not a property of the build.** The
trace ends inside `nv2a_ack_thread` (`ntdll!NtDelayExecution` →
`jsrf_recomp!nv2a_ack_thread+0x693`), which is the GPU acknowledgement thread —
the thread whose body sits behind `g_nv2a_ack_enabled`.

This is the ruling's fourth reversal condition in its **"class stays admitted,
C1 via TTD is BLOCKED"** branch: a traced run does not reach the horizon, so W-a
cannot hold and no C1 attribution can be made from a TTD trace until the cause is
found and removed. The Session does **not** treat this as a new ruling; it is
recorded as the measurement the ruling asked for, and the branch it selects.


---

# C1 instrument (2026-09-30, second entry)

**Question put to the Advisor.** Given that the write C1 is looking for is not visible
in a TTD trace if it is not an instruction store, and that two non-TTD sources agree
the `0` is real while TTD's record disagrees, should C1 continue with TTD or does the
measurement require a different instrument?

## RULING

**W11 STANDS UNCHANGED, AND TTD REMAINS C1's INSTRUMENT.** The measurement offered as
the new W-d control is **not** a valid W-d control, so it triggers neither reversal
branch. The C1 finding's conclusion "TTD cannot see the write" is **not established**,
and the artifact it rests on is **NOT ADMITTED**.

## The observation the brief and the finding missed

`logs/ttd/20260930-053811-458-ttd-aputrap/` **fails S1: it was truncated at its size
cap.** Verified independently in this session:

| Claim | Measured |
|---|---|
| the `.run` is at the cap | **YES** — `8,589,934,592` bytes = exactly `8192 MB` |
| the recorder ended by cap, not process exit | **YES** — `ttd-output.txt` says *"Recording stopped after 43375ms"*; the two earlier traces say *"Process exited with exit code …"*; `timed_out=false` |
| the terminal is in the LOG, not the TRACE | **YES** — `[ICALL]` at log line 23,481 of 23,484, written by the process after recording stopped |

**The consequence.** `ttd-terminal.py` set P from `!tt 100` — the trace's END — and
`ttd-query.py` passed S2 whenever the **log** contained the terminal. S2 therefore
checked "the log has the event", not "the trace has the event", which is **not what W-a
requires**. So the census, the `TTD.Memory` value, and the apparent disagreement with
the minidump are all what a tail-truncated trace produces when the clobber happens
after recording stopped. **The simplest reading needs no new write class.**

## The four points, addressed

1. **(4) is not under "Reversed by", in either direction.** W-d means querying a known
   `NtReadFile` destination buffer. (4) classified the first 400,000 writes by module —
   a **first-N sample**, observation only — and it cannot show kernel writes are
   unreported, because a kernel-mode write would not carry an `ntdll`/`KERNELBASE` IP
   in the first place. The claim that none of the 14 `[READ]` bytes appears as a write
   **was never measured**: those log lines do not print the destination VA.
   **Exclusion (d) stays exactly as ruled: uncertain, stated, caught by W-b.**
   H-KERNEL is neither excluded nor confirmed.
2. **TTD is not admissible as evidence of absence for a class it cannot see**, and W11
   already says so. An absence excludes writers of the admitted class (user-mode
   instruction stores, inside `[trace start, P]`) in that window only. A value mismatch
   with no write found selects `UNATTRIBUTED`, which never names a writer or a class. A
   trace that fails S1 has no coverage for its tail and yields **no absence at all**.
3. **The required instrument class** is still TTD, on a trace passing S1 and a corrected
   S2. The record array looks like a constructor loop in guest code (inferred), i.e. a
   lifted user-mode store inside the admitted class. **Only if W-b selects
   `UNATTRIBUTED`** does C1 need something else: an **in-process last-write latch keyed
   by destination region** (a finite, source-derived key), set at the event by every
   enumerated host path that writes guest RAM outside lifted guest code — `NtReadFile`
   and IO-completion destinations, APU/GPU DMA into RAM, host `memcpy`/`memset` on guest
   buffers — each fixture-tested.
4. **The A2h alias census is NOT the successor.** It is a **first-touch** census, so its
   key selects the EARLIEST writer (the loader or the install) where C1 needs the LAST
   before P. It inherits the page-guard/VEH instrument class whose defect history is why
   the plan left A2h. It may corroborate; it may not decide.

## Conditions C1 must meet before any row (additions to W11's scope)

- **C-a (S1, strict reading).** The recorder output shows *"Process exited with exit code
  0xE0424943"* and the trace is **below** `-maxFile`. *"Recording stopped"*, or a size at
  or above the cap, is **S1 FAIL**, and then **nothing after the install may be read from
  the trace**. Size the cap from the T14 disk gate.
- **C-b (S2 corrected).** W-a is established **from the trace itself**: the trace
  contains the terminal event — TTD's exception event with code `0xE0424943` on the
  faulting thread, or the faulting thread's read of the failing slot. **P is that
  event's position, not `!tt 100`.** A log-only terminal means **S2 FAIL**.
- **C-c.** **W-c is still MISSING and is not waived.** "All 28 mirrors of a canonically
  written range return 0" is a **negative**: a working mirror query returns zero there,
  and so would a broken one. A mirror positive needs a store actually made **through a
  mirror VA**, found at its alias index.
- **C-d.** **W-d is still MISSING.** It needs the destination VA of one `[READ]`
  (logged by the runtime) and a query at that buffer for the transfer window.
- **C-e.** The census and "last write before P" are **recomputed on the admitted trace**,
  and W-b decides: equal means `ATTRIBUTED`; mismatch means `UNATTRIBUTED`, which
  triggers the in-process latch in point 3.

## Basis

- **Observed:** the trace file equals the cap; "Recording stopped" with no process-exit
  line; `timed_out=false`; the terminal exists only in the process's log; P comes from
  `!tt 100`; S2 passes on the log alone; `[READ]` lines lack destination VAs; (4) is a
  first-N IP sample.
- **Inferred:** the clobber and the terminal read postdate the recording's end, which
  fully explains the "contradiction"; the record array is a guest instruction store.
- **Uncertain:** whether `TTD.Memory "w"` reports syscall-output memory (W-d); the
  writer's identity.

## Reversed by

- A trace meeting **C-a and C-b** whose recomputed census still shows no write of the
  record array before P while the value at P reads `0` → W-b selects `UNATTRIBUTED`, and
  C1 moves to the in-process last-write latch. **TTD is then retired for this question
  only; the W11 class admission stands.**
- A **proper** W-d control showing syscall-output writes are reported → drop exclusion
  (d).
- Evidence that the recording stop was not cap-induced and the trace does contain the
  terminal at P → this ruling's main finding falls and the question reopens on the
  existing trace.

## Withdrawn from `docs/reviews/c1-terminal-slot-finding.md`

The sections *"W-d is ANSWERED"*, *"H-ALIAS is EXCLUDED"*, *"H-KERNEL excluded"* and
*"TTD is the wrong instrument"* are **NOT ADMITTED**, and the record carries a
withdrawal block saying so. The **non-TTD minidump A/B survives** as corroboration.
