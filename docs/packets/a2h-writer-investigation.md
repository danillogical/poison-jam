# A2h — bounded writer investigation for the unresolved ICALL at `0x0017DBBD`

**Packet:** `A2h-r5`. **Status:** Proposed (investigation packet; no fix authorized).
**Depends on:** accepted P0.7 (provenance guard and candidate generation).
**Governing requirement:** `plan-jsrf-bare-minimum.md` A2's open defect, carried
through P0.7-AC4 as *"publish only a bounded A2h writer-investigation packet with
identity, strict/effective profile, existing evidence, attempt count, instrumented
address and next discriminating experiment. A protective trap is containment, not
a behavioral fix."*

This packet is **investigation only**. It authorizes one discriminating
experiment, not a recovery pass, and explicitly does not authorize a whole-image
census or a full translation.

**Revision history:** `r1` (first draft), `r2` (five blocking defects closed: the
objective/experiment mismatch, a non-exhaustive decision table, PASS not pinning
the failing invocation, an overstated profile claim, and a missing positive
control), `r3` (PASS and row 1 no longer pinned to the *literal* `0x00700010`, row
3 marked unselectable under PASS, the heap/window test given bounds, the trace cap
required to retain the most recent lines, and strict reachability recorded as
**unproven**), `r4` (the decision table is a **complete partition** —
the heap-base-with-bogus-slot cell was uncovered — the call-site record at
`0x0017DC23` is required because the return address is shared with a second
dispatcher caller, and the measured/inferred wording is harmonized), `r5`
(this revision: PASS requires the call-site record's slot value to equal the
logged invalid target, which closes the last misattribution path; `0x0017DC23`
added to the instrumented-address list; and two MEASURED numbers corrected —
the exit code is `0xE0424943`, not `0xE0464643`, and the kernel-call counts are
200 pre-fix and 157 post-fix, not "both 157").

## Claim and boundaries

- **Establishes:** the descriptor, count, base, index and slot value present at the
  **failing** invocation, and — from those — which of four mechanisms is consistent
  with the observation. It does **not** name the writer of the slot: a read-site
  trace records values, not writers.
- **Does not establish:** the identity of the code that wrote the slot; that any
  particular fix is correct; that the defect is a translation error rather than a
  guest-data error; that the ICALL is reachable under strict conditions.
- **Non-goals:** recovering new functions, regenerating the translation, a broad
  image census, or changing the trap. Performing a census or a recovery pass under
  this packet is a **FAIL**.
- **Known facts (MEASURED, from the original XBE and archived runs):**

The defect site, disassembled from `game/default.xbe` (the full sequence, so the
slot address is unambiguous — an earlier draft omitted the `ecx` reload and as
printed the slot computed to `base + index*8 + index*8 + 4`):

```
0017DBE0  mov  edi, [ebp+0x10]        ; the table descriptor
0017DBE3  cmp  esi, [ebp+0x14]
0017DBED  cmp  esi, [edi+4]           ; bounds check against descriptor+4
0017DBF7  mov  eax, esi
0017DBF9  shl  eax, 3                 ; index * 8
0017DBFC  mov  ecx, [edi+8]           ; the table base
0017DBFF  add  ecx, eax
0017DC0D  cmp  [ecx+4], 0             ; is the callback slot set?
0017DC11  je   0x17DC28
0017DC13  mov  [ebx+8], esi
0017DC16  push 0x103
0017DC1B  push ebx
0017DC1C  mov  ecx, [edi+8]           ; RELOADED -- not the same ecx as above
0017DC1F  push [ecx+eax+4]            ; the callback value
0017DC23  call 0x17E600               ; dispatches it
```

So the descriptor is `{count at +4, base at +8}` and the slot is
`base + index*8 + 4`, computed from the **reloaded** `ecx` at `0x0017DC1C`.

**The dispatcher chain is confirmed, which is what makes the trace decisive.**
`sub_0017E600` loads `eax` from its own `[ebp+8]` — the *last* value pushed before
the call, i.e. `[ecx+eax+4]`, the slot — then does `call eax` at `0x0017E625`:

```
0017E611  mov  eax, [ebp+8]     ; = the pushed slot value
0017E61B  mov  ebp, [ebp-4]     ; context; eax already loaded
0017E625  call eax              ; the slot value is what gets called
0017E627  pop  edi              ; <- the logged return address
```

The archived failure logged `invalid target 0x00700010 ... return=0017E627`, and
`0x17E627` is exactly the instruction after that `call`. **The structural claim is
measured**: the invalid target is the *slot value* read from
`[base + index*8 + 4]` — not a corrupted return address and not the descriptor.
**The value `0x00700010` itself is INFERRED**, not established: it comes from
exploratory runs, and the return address is shared with a second dispatcher caller,
so *which* site dispatched is not settled by the log alone. That distinction is why
the experiment below records the call site and keys row 1 on a predicate rather
than on this number.

`0x00700010` is confirmed **not** in any file-backed XBE section, using the
project's own reader rather than a hand-rolled section walk:

```
$ python -X utf8 scripts/inspect-jsrf.py disasm 0x00700010 0x00700020
Inspection failed: range is not contained in one file-backed XBE section
```

That matters for the decision table: a slot value outside every image section is
either a runtime-computed value that reached a callback slot, or a genuinely
corrupt table. The trace below distinguishes them, and the address alone does not.

**Call sites (MEASURED).** There are **three** direct calls to `sub_0017DBBD`:

| call site | containing entry | entry kind |
|---|---|---|
| `0x0017DDFD` | `0x0017DCE6` | `call_target` (a real body) |
| `0x0017E03A` | `0x0017E004` | `prologue` (a separate function) |
| `0x0017E329` | `0x0017E2EE` | `prologue` (a separate function) |

`0x0017DD6B` and `0x0017DDAE` are **`tail_jump_alias` fragments** inside the
`0x0017DCE6` body, not separate callers. An earlier draft said "one shared call
reached from three entries" and listed the aliases as callers; both were wrong.

**Cleanup (MEASURED).** `sub_0017DBBD` ends at `0x0017DC67` with a **plain `ret`**;
the **callers** clean (`add esp, 0x10` at `0x0017DE02`, `add esp, 0x1c` at
`0x0017E2E4`). An earlier draft said the callee cleans — wrong, and the kind of
error that matters when reasoning about the stack.

- **Attempts so far: 2.**
  1. The pre-alias-fix run logged **48** `[RECOVERED] ... ABI verified` checks and
     died at `[ICALL] Failed to resolve VA 0x001918E0` after 379 thread calls.
  2. The post-alias-fix run logged **0** and died at
     `[ICALL] invalid target 0x00700010 ... return=0017E627`. The failure moved
     **earlier**, into the demo's startup path. Both runs are
     `unhandled_exception` / `exit_code 0xE0424943`, both reach `guest_entry` at
     log line 48; the pre-fix run issued **200** kernel calls and the
     post-fix run **157**.
- **Assumptions/open questions (INFERRED):** that `0x00700010` is a *value* the
  guest computed rather than a corrupted pointer. Tested by the experiment below.

## Identity and profile

- **Instrumented addresses:** `0x0017DDFD`, `0x0017E03A` and `0x0017E329` (the
  three calls into `sub_0017DBBD`), **`0x0017DC23`** (the call into the dispatcher,
  which step 1 makes mandatory), and `0x0017DC1F` (the slot read).
- **Evidence profile required: `strict`.** `run-jsrf.py` gained
  `--profile strict|exploratory|fixture` in P0.1, and a strict run is already
  archived (`logs/runs/20260923-013448-357-p0-strict-baseline`, reclassified
  **STRICT** by `check-run-profile.py`), so a strict **launch** is executable today.
  The caller supplies `RECOMP_GPU_ACK=0` exactly; `validate_launch_profile` refuses
  a strict request without it, and the runner injects `RECOMP_GPU_ACK=0` only for
  `gpu-*` probes, so it never supplies it for this run. A fresh disposable save
  root is created automatically. **Strict *reachability* of this site is a separate
  question and is unproven — see "Reachability is not established" below.**
- **Existing evidence, and its eligibility.** The A2f/A2g runs are
  **exploratory** under the P0.1 classification (`RECOMP_AC97_READY=1`,
  `RECOMP_APU_DSP_ACK=0x803C0810`, `RECOMP_GPU_ACK` absent). They support the
  *local structural* claims above — which instruction supplied the value — and
  **nothing else**. No conclusion in this packet depends on their strictness, and
  a strict run may legitimately stop earlier.
- **Evidence revision:** game `40ae5bd` + P0 edits, toolkit `484887b`.
- **Evidence index (required, not optional).** The run directory; `metadata.json`;
  `check-run-profile.py <run-dir>` output showing STRICT; the descriptor trace; the
  run's map or PDB showing the instrument symbol; the selected decision row. Game
  and toolkit revisions plus dirty state, the XBE hash and the build-identity
  hashes come from the archive's own `metadata.json` and must be quoted with it.

## Execution — one discriminating experiment

The two attempts differ in *which* value arrives. The cheapest experiment that
separates the candidate mechanisms is to log the **descriptor and slot** at the
read, rather than to speculate about the value:

1. Add a bounded, observation-only trace at `0x0017DC0D`/`0x0017DC1F` **and at the
   call `0x0017DC23`** recording `edi` (descriptor), `[edi+4]` (count), `[edi+8]`
   (base), `esi` (index), and `[base + index*8 + 4]` (the slot), with the base's
   XBE section name where one applies. Observation only: it must not alter control
   flow, and it is not a fix. **The call-site record at `0x0017DC23` is required,
   not optional:** `sub_0017E600` has **two** callers — `0x0017DC23` in
   `sub_0017DBBD` and `0x0017C233` in `sub_0017C1F6` (itself reached from
   `0x0017DD57` in the same startup region) — and **both fall through the same body
   to `call eax` at `0x0017E625` and return to `0x0017E627`.** So the return address
   alone does not say which site dispatched, and without the call-site record the
   "last trace line before the ICALL" could belong to an unrelated, healthy
   invocation — selecting a row from good data. **The cap must retain the most
   recent N lines, not the first N** — a ring or last-N buffer. A first-N cap can
   drop the failing invocation, which makes PASS unsatisfiable while looking like a
   healthy trace.
2. Run once, **strict**, with a fresh disposable save root, and read the trace line
   that immediately precedes the failing ICALL, **requiring it to be the call-site
   record from `0x0017DC23` whose slot value equals the logged invalid target**. A
   slot record not followed by its own failing ICALL — or whose slot differs from
   the logged target — is not the failing invocation.
3. Decide by the table above.

- **Allowed implementation choices:** the trace's exact format and its cap, subject
  to the last-N requirement above.
- **Escalate/replan if:** the descriptor is unreadable, the trace never fires, or
  the slot holds a valid in-image address (row 4, which moves the defect into the
  dispatcher).
- **Worker scopes:** none required; this is a single bounded change.
- **Build/run owner:** the session. Serialize: one build and one run.

### Reachability is not established, and the plan must allow for that

`run-jsrf.py` gained `--profile strict|exploratory|fixture` in P0.1 and a strict run
is archived, so a strict launch is **executable today**. What is **not** established
is that a strict run *reaches this site*: measured, the archived strict run has zero
matches for `0017DBBD`, `0017E600`, `0017E627`, `0017DDFD`, `0017DC1F`,
`invalid target` or `ICALL`, and ends `normal_exit` / `exit_code 0` after 1.92 s
via `HalReturnToFirmware(2)`. A strict run that stops **earlier** than the
exploratory A2f/A2g runs is expected and is not a regression — the two profiles are
not comparable. If the authorized run does not reach the site, the outcome is row 5
(`UNKNOWN`) and the next step is a **reachability** packet, not this one.

### Decision rule

Every observation maps to exactly one row, and **any observation not covered is
`UNKNOWN`** (row 5) rather than an implied instruction. "Plausible" is not a
predicate: a base is classified by the **XBE section table**, and the section name
is recorded. A base that is not in an image section is not automatically wrong — a
runtime-built table can legitimately live in the heap or the separately allocated
contiguous window, which is why row 2 requires *both* to be excluded before the
descriptor is blamed. "In the heap or contiguous window" means **within the bounds
reported by the runtime**, not a guess: the contiguous window's base and size are
available from the collector's own capture, and the heap accessor is the runtime
helper that answers allocation membership. The recorded evidence must name which
of those two answered.

| Observation at the failing invocation | Reading | Next |
|---|---|---|
| base is in a named image section **or** in the heap/contiguous window (the evidence records which), and the slot is **not a valid address in any image section** | the **table content** is wrong: a writer put a computed value in the slot | investigate the writer; this packet does not name it |
| base is unreadable, **or** is in neither an image section nor the heap/contiguous window | the **descriptor** is wrong: whoever supplied `[ebp+0x10]` is the defect | trace the descriptor's supplier |
| *(pre-failure diagnostic only — see note)* `[ecx+4]` is 0 and the branch at `0x0017DC11` is taken | the slot is *intentionally* empty and the call should not happen | the defect is upstream of this site |
| the trace fires with a base in the heap/contiguous window and a slot that **is** a valid in-image address | the table is runtime-built and the slot is legitimate | the defect is in the dispatcher's use of it, not the table |
| **any other observation, or the trace never fires** | undetermined | **`UNKNOWN` — re-scope before further work** |

**Rows 1–4 are a complete partition**, which is what makes row 5 a genuine
catch-all rather than a likely outcome: base ∈ {image section, heap/window,
neither-or-unreadable} × slot ∈ {bogus, valid, zero}. An earlier revision put
"base in a named image section" in row 1 and left the **heap base with a bogus
slot** cell uncovered — and that is the *most likely* case for a runtime-built
table, which this packet itself argues for. Measured counterexample: heap base
`0x00A40000` with slot `0x00700010` matched no row and returned `UNKNOWN`, the same
failure mode as keying row 1 to a literal address.

> **Note on row 3.** It is a *pre-failure* diagnostic, not a row PASS can select:
> if the branch is taken there is no call and therefore no failing ICALL line to
> pin against. It is retained because a run that takes it is informative about the
> site being healthy, and must not be mistaken for row 5.

**Row 1 is not pinned to a literal address.** The value `0x00700010` is itself
INFERRED (see the claim limits) and derives from **exploratory** runs, while the
authorized run is strict. A strict run failing at the same site with a different
bogus slot — `0x00700014`, say — is the *same mechanism* with the *same
discriminating value*, so row 1 keys on the **predicate** ("the slot is not a valid
address in any image section"), not on the number.

**PASS (packet complete):** the trace's **last** line preceding the
`[ICALL] invalid target ... return=0017E627` log line in the **same run** is the
**call-site record from `0x0017DC23`**, and **its recorded slot value equals the
logged invalid target**. Requiring the values to agree is what closes the last
misattribution path: `0x0017E627` is shared with `0x0017C233`, so a *healthy*
`0x0017DC23` record followed with no intervening log line by a failure dispatched
through `0x0017C233` would otherwise satisfy "the next logged event" while
selecting a row from good data. It carries descriptor, count, base, index and
slot; the base's section (or its absence) is recorded; and exactly one row above is
selected with its evidence. The ICALL line's *target value* is deliberately not
pinned — any invalid target at this return address is the same defect — but it must
**match the recorded slot**, which is what ties the observation to this invocation.
**A trace line from a healthy invocation is not evidence**: a cap that captures a
slot-0 call would select row 3 and misattribute the defect upstream.
**FAIL:** the trace alters control flow or the run's outcome; a census or recovery
pass is performed; or PASS is claimed from a trace line that is not the last one
before the failing call.
**BLOCKED/UNKNOWN:** the failing log line is absent, or the site is not reached.

**Positive control required.** The run must show the trace can fire at all: a
recorded hit count of at least one, plus the instrument symbol present in the run's
map or PDB. Without that, "the trace never fired" cannot be distinguished from a
mis-wired instrument, and row 5 would be consumed by a broken trace.

### Claim limits

A recorded descriptor explains *what value arrived*, not *why the guest computed
it*, and **not who wrote it** — the trace reads, it does not watch writes. Naming
the writer requires a second instrument (a bounded write watchpoint or run-to-write
on the slot address), which this revision does **not** authorize; if the decision
table lands on row 1, that is the follow-up packet's subject.

A protective trap on this site is **containment, not a behavioral fix**: it leaves
the defect open and does not advance A2h's status, and must not be reported as a
fix or cited as one. Passing this packet does not make A2h delivered.

**The `0x00700010` value is INFERRED, not established.** It comes from the A2f/A2g
runs, which are **exploratory** under the P0.1 classification, and the only
archived **strict** run does **not reach this site at all** — measured, zero
matches in its log for `0017DBBD`, `0017E600`, `0017E627`, `0017DDFD`, `0017DC1F`,
`invalid target` or `ICALL`; it ends `normal_exit` / `exit_code 0` after 1.92 s via
`HalReturnToFirmware(2)`. So **strict reachability of `0x0017DBBD` is unproven**,
and that is precisely why row 1 keys on a predicate rather than on this number: the
authorized strict run may fail here with a different value, or may not reach here
at all. The latter is row 5 and is a legitimate, informative outcome.

## Closure

- **Required test inventory:** none new; this is an observation-only change.
- **Evidence index:** see "Identity and profile" — the run directory,
  `metadata.json`, the STRICT profile check, the descriptor trace, the instrument
  symbol in the map/PDB, and the selected decision row.
- **Reviewer:** the acceptance reviewer from `docs/agent-workflow.md`.
- **Criterion change procedure:** any change to the decision table is a new
  revision with a reason and re-review.
- **Unrelated next stop:** record as a follow-up; do not expand this packet.
- **Next packet selection:** by the decision table above. If row 1 is selected, the
  follow-up is a **separate** writer-identification packet authorizing a write-side
  instrument; that work is not authorized here.

## Counterexample review

- **Could a broken translation satisfy this?** No: the packet claims only what was
  observed at one site, and explicitly excludes both a fix claim and a
  writer-identity claim.
- **Could an unexercised path satisfy it?** No: the trace must fire on the failing
  invocation, a positive hit-count control is required, and "never fired" is
  `UNKNOWN`.
- **Could an exploratory run carry the claim?** Only the local structural claims;
  the new run must be strict, and strict reachability is not claimed.
- **Could stale evidence satisfy it?** No: the descriptor is read from a fresh run
  under the current revision, bound by the archive's own identity hashes.
- **Could the executor satisfy PASS while learning nothing?** Not by the letter:
  PASS requires the base's section classification and a named decision row, and
  row 5 exists so that "no row applies" fails closed instead of reading as an
  instruction.
- **Could a broad census hide inside this packet?** It is named in FAIL.
- **Could the packet yield nothing even when executed correctly?** Yes, and that is
  recorded rather than hidden: strict reachability of the site is unproven, so the
  authorized run may stop earlier and return row 5. The packet is explicit that
  this is a legitimate outcome whose next step is a reachability packet, not a
  retry of this one.
- **Could a different bogus slot value at the same site be mistaken for "no row
  applies"?** No: row 1 keys on the predicate "not a valid address in any image
  section", not on the literal `0x00700010`.
