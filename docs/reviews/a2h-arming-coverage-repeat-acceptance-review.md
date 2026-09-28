# Stage-1 acceptance review — `A2h-arming-coverage-repeat` (packet frozen `4AC455E7…B1174A1`, 15 lines)

**Reviewer:** independent stage-1 acceptance reviewer (did not author the packet, did not execute the runs).
**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet class:** discovery. **Evidence under review:**
`docs/reviews/a2h-arming-coverage-repeat-execution-evidence.md`.
**Row ruling under review:** `docs/reviews/a2h-repeat-row-interpretation.md`.
**Game `HEAD` at review:** `fdf872e1`. **Toolkit `HEAD` at review:** `571982d9`.

**Disposition: ACCEPT-WITH-CORRECTIONS.** All eight acceptance criteria pass on my own independent
verification. The corrections are not defects in the *execution* — the runs are clean, the identity is
sound, K = 3 is mechanically real — but the evidence's **row section is now superseded in scope** by a
finding made after it was written (`docs/reviews/a2h-dr0-positive-control-failure.md`, committed
`61f813c`). I independently re-verified that finding and **could not falsify it**; I in fact
strengthened it. The row outcome is therefore recorded as **`PREMISE_CHANGED`**, not as a closed
`O-READ-PATH` selection.

---

## Criterion 1 — Artifacts exist: AGREED

All five new ON dirs exist and each carries the identical 19-file artifact set
(`build-source.json`, `gpu-analysis.log`, `gpu-report.json`, `gpu-report.md`, `gpu-snapshots.jsonl`,
`jsrf_collect.exe`, `jsrf_collect.pdb`, `jsrf_recomp.exe`, `jsrf_recomp.map`, `jsrf_recomp.pdb`,
`jsrf_run.log`, `metadata.json`, `process.dmp`, `project.patch`, `project-status.txt`, `result.json`,
`source.zip`, `stacks.txt`, `toolkit.patch`, `toolkit-status.txt`). The reused OFF and the pinned
comparator carry the same set.

I recomputed `jsrf_run.log` SHA-256 and compared with `metadata.json:run_log_sha256` in every run:

| Run | Declared | Recomputed | Match |
|---|---|---|---|
| `…035337-386-a2h-repeat-on-1` | `FFA21DFC5E97684E…` | same | **YES** |
| `…035402-315-a2h-repeat-on-2` | `ECB6074F1937F5A7…` | same | **YES** |
| `…035409-942-a2h-repeat-on-3` | `4DE4D73208CCE8F4…` | same | **YES** |
| `…035417-757-a2h-repeat-on-4` | `54087E58AEC119F8…` | same | **YES** |
| `…035425-633-a2h-repeat-on-5` | `EF3EFD50B7172F1C…` | same | **YES** |
| `…030751-407-…-inert-off` (reused) | `2920E1D88BB1078C…` | same | **YES** |
| `…030924-722-…-on-2` (pinned) | `1AC9ACBCA3674B2B…` | same | **YES** |

The pinned comparator's `jsrf_run.log` hash matches the packet's raw pin
`1AC9ACBC…A3676299`. **No criterion-1 defect.**

## Criterion 2 — Artifacts match the commands: AGREED

`python -X utf8 scripts/check-run-profile.py <dir>` returns **STRICT** for all five
(`checked 1; strict 1, exploratory 0, fixture 0, unknown 0, missing 0`).

Gates read from each `metadata.json` (`settings` and `run_profile.inherited_settings`):

- `JSRF_TRACE_A2H_DR = 1` and `JSRF_TRACE_A2H_SLOT = 1` — present in **all five**.
- `RECOMP_GPU_ACK = 0` (`resolved_defaults…effective = disabled`), `RECOMP_APU_TRAP = 1`,
  `RECOMP_KERNEL_LOG_BUDGET = 100000` — present in all five.
- Forbidden synthetic-completion vars `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`,
  `JSRF_ALLOW_UNRESOLVED`, `JSRF_ABI_CONTINUE` — **absent from every run's effective
  environment**, and **zero occurrences** when grepping `stacks.txt` and `jsrf_run.log` across all five dirs.
- Command shape identical in all five: `jsrf_collect.exe 8 <run-dir> <run-dir>\jsrf_recomp.exe
  --save-root=<run-dir>\save-root` — fresh label and fresh save root per run, no yield steering.

**No criterion-2 defect.**

## OFF-reuse claim: CONFIRMED — my own hash comparison

Recomputed at review time:

| Binary | Pinned | Measured | |
|---|---|---|---|
| `jsrf_recomp.exe` | `A7E324642A41DF3F9070A0366E4E199CCA6DED34304320BC12CEF24E7ACDEAD9` | `A7E32464…7ACDEAD9` | **MATCH** |
| `jsrf_collect.exe` | `95440EC43011A01D25CE0AB9D0077C892312BA39FD9238C1C7AC8001676B3EF7` | `95440EC4…676B3EF7` | **MATCH** |

Binary-affecting-change gate:

- `git diff --stat d369880..HEAD -- src/ tools/ CMakeLists.txt` — **empty**.
- Same in `C:\Users\logic\Repos\xboxrecomp` from `571982d` — **empty** (toolkit `HEAD` *is* `571982d9`,
  so the range is a no-op and the result is trivially empty; the substantive gate is the binary hash,
  which matches).

**Both checks pass, so the reused OFF control `logs/runs/20260928-030751-407-a2h-arming-coverage-inert-off`
was legitimately borrowed — not an `O-IDENTITY` condition.** I additionally confirmed the OFF run is
record-level inert: `GUEST_DR_ARM_OK` = **0**, `GUEST_DR_ARM_TERMINAL` = 0,
`GUEST_DR_ARM_RECONCILE` = 0, `GUEST_DR_HIT` = 0. **No fresh OFF was needed.**

## Criterion 4 — Classification: AGREED (including run 1)

Re-derived independently from `jsrf_run.log` / `stacks.txt` / `result.json`:

| Run | `outcome` | Terminal line | Site | Class | Evidence says |
|---|---|---|---|---|---|
| on-1 | `diagnostic_deadline`, exit 3 | **none** | — | not a realization | matches |
| on-2 | `unhandled_exception` | `target=00000000 slot=001C4064 live=00000000 call=#5555` | `0014982E` | TARGET | matches |
| on-3 | `unhandled_exception` | `target=41200000 … call=#1354` | `00147D36` | NON-TARGET | matches |
| on-4 | `unhandled_exception` | two terminals: `00000000` @`00147DBC`, `00000001` | `00147DBC` | NON-TARGET | matches |
| on-5 | `unhandled_exception` | `target=00000000 slot=001C4064 live=00000000 call=#5555` | `0014982E` | TARGET | matches |

**Run 1 verified specifically, as required:**

- `result.json`: `{"outcome": "diagnostic_deadline", "exit_code": 3, …}` — confirmed.
- `[A2HSLOT] terminal …` lines: **0**. `[ICALL] invalid …` lines: **0**. So **no fatal terminal**.
- OOM anchors: `598869040` occurs **0** times; `0xC0000017` occurs **0** times. The `50855936`
  matches in run 1 (32) are all `[HEAP] … used N/50855936` denominators, **not** the OOM anchor.
- Identity-1 prefix: `call=#5555` occurs **0** times (`5555` count = 0).
- Its `GUEST_SLOT_WATCH` reads `terminal_seen=0`.

So run 1 is genuinely a bounded capture before any fatal terminal, with no OOM and no identity-1
prefix, and its exclusion from the realization denominator is correct — **not** scored as NON-TARGET.

Two precision notes, neither a defect in the classification:

1. Run 1's `GUEST_DR_ARM_TERMINAL` reads `arms_recorded=18` (and `arm_tid_list=18`), not 17 like the
   four anchored runs. The evidence's blanket "17/17" coverage sentence is scoped to *anchored* runs
   and run 1 is excluded, so this is consistent — but the sentence should be read as anchored-only.
2. Runs 3 and 4 are non-unique terminal runs: run 4 emits **two** terminal lines
   (`target=00000000` and `target=00000001`). The evidence's table shows only the first. The
   TARGET predicate requires *exactly one* fatal terminal, so run 4 remains NON-TARGET either way —
   but the table under-reports it.

## K = 3 claim: CONFIRRED on the mechanical facts I counted myself

**My own `GUEST_DR_ARM_OK` counts** (from `stacks.txt`, the file that carries them — they are absent
from `jsrf_run.log`, which is why a log-only count returns 0):

| Run | `GUEST_DR_ARM_OK` | `arms_recorded` | `arm_tid_overflow` |
|---|---|---|---|
| pinned `…030924-722-on-2` | **17** | 17 | 0 |
| new on-2 | **17** | 17 | 0 |
| new on-3 | **17** | 17 | 0 |
| new on-4 | **17** | 17 | 0 |
| new on-5 | **17** | 17 | 0 |
| on-1 (excluded) | 18 | 18 | 0 |

Each `GUEST_DR_ARM_OK` line carries `dr0=00000000001D4064 dr7_readback=00000000000D0001
canonical=00000000001D4064` — registers genuinely programmed (10 at handshake + 7 at `create_thread`).

**Coverage-complete in both new TARGETs**, independently confirmed:

- `GUEST_DR_ARM_TERMINAL arms_recorded=17 armed_at_or_after_handshake=17 arm_attempt_failures=9
  collision=0 create_thread_events=16` — exact, in both.
- `GUEST_DR_ARM_RECONCILE arm_tid_list=17 arm_tid_overflow=0 … distinct_armed_tids=17` — exact, in both.
- Census, from `GUEST_SLOT_WATCH`: `armed=1 alias_count=28 mapped_mask=0FFFFFFF
  protect_mask=0FFFFFFF touched=0 publish_failed=0` — identical in on-2, on-5 and the pinned run.
- Install control, from `GUEST_SLOT_LATCH`: `install_seen=1 install_raw=80000115
  install_value=FE000104 install_ok=1` — identical in all three.

**The mechanical agreement — the terminal triple — verified by me in all three realizations:**

| Component | pinned on-2 | new on-2 | new on-5 |
|---|---|---|---|
| slot | `001C4064` | `001C4064` | `001C4064` |
| target | `00000000` | `00000000` | `00000000` |
| live | `00000000` | `00000000` | `00000000` |
| site | `0014982E` | `0014982E` | `0014982E` |
| faulting ICALL | `invalid target 0x00000000 … return=0014982E` | same | same |
| install control | `80000115 → FE000104 ok=1` | same | same |
| census | `armed=1, 28, 0FFFFFFF/0FFFFFFF, touched=0` | same | same |

So **K = 3 holds as a claim about mechanical agreement of the terminal triple, install control and
census**, and the K-scope ruling is applied correctly (pinned run supplies K = 1; two new
coverage-complete TARGETs; Set-A and "4/10" not imported). **`GUEST_DR_HIT` is 0 in all three** — see
the next section for what that number may and may not be read as.

## Row `O-READ-PATH` — faithful to the ruling as written, but the ruling's own premise has since changed

**As an application of the ruling that existed when it was written, the evidence is faithful and does
not over-claim.** I verified it does **not** assert that the slot was never written, does not name a
writer, and does not claim a proven read-path fault or miscompile. It states writer `UNKNOWN`,
mechanism `UNKNOWN`, generality limited to the agreed row, and "K=3 establishes agreement on that
limited row, not a no-write conclusion" — matching the ruling's
*"K=3 permits agreement on this **limited row** … it does not convert unknown provenance into a
negative-write or causal finding."* The ruling's narrow audit-referral reading
("coverage within which no zero-write was observed", leg 3 conditional on a hit) is reproduced
accurately. **No over-claim by the evidence against the ruling as written.**

**But the ruling contains a premise that is now measured false, and the evidence inherits it.**
`docs/reviews/a2h-repeat-row-interpretation.md:7` states: *"The positive install trap tests that the
DR watch actually fires…"*. **It does not.** The install trap is a software value comparison
(`GUEST_SLOT_LATCH install_raw/install_value/install_ok`); it never exercised a debug register. We now
know from direct measurement that **the DR watch did not fire on that store** — so the ruling's stated
positive control for DR firing does not exist. This is a defect in the **ruling** (and in the
predicate wording the ruling itself calls "genuinely ambiguous"), not in the evidence.

---

## The DR0 positive-control finding — independently re-verified, NOT falsified, and strengthened

I reviewed `docs/reviews/a2h-dr0-positive-control-failure.md` against source and archive. **Every link
holds under my own scrutiny.** (Note: the real source path is
`C:\Users\logic\Repos\xboxrecomp\src\kernel\kernel_bridge.c` — the record cites it as
`kernel_bridge.c`, which is a path shorthand, not an error.)

| # | Link | My verification |
|---|---|---|
| 1 | handshake precedes store | **confirmed** — `kernel_bridge.c:9395 a2h_install_handshake();` inside `if (a2h_watch_on())`, before the loop at `:9410` and the store at `:9468` |
| 2 | loop reads the slot | **confirmed** — `:9412 uint32_t current = BRIDGE_MEM32(va);` |
| 3 | loop writes unconditionally | **confirmed** — `:9468 BRIDGE_MEM32(va) = synthetic;` sits **outside** the `if (va == A2H_SLOT_VA && a2h_slot_trace_on())` gate at `:9461-9467`, so it always executes |
| 4 | write hits the watched host address | **confirmed** — `collect.c:51 A2H_SLOT_VA 0x001C4064u`; log shows `install … index=65 slot=001C4064`; `dr0` readback `0x1D4064`, consistent with the reported `mapped 65536 KB at 0x10000` offset |
| 5 | DR0 is a 4-byte write watch on that address | **confirmed** — `collect.c:56 #define A2H_DR7 0x000D0001u` (`L0=1`, `R/W0=01` writes, `LEN0=11` 4 bytes); readback `00000000000D0001` in every `GUEST_DR_ARM_OK` line |
| 6 | store runs on the armed thread | **confirmed** — `install tid=43016` == `GUEST_DR_ARM_OK seq=20 tid=43016` |
| 7 | store is after the arm | **confirmed** — the `0xE0424452` handshake event appears once in `stacks.txt`, and `GUEST_SLOT_WATCH handshake_seen=1`; arming happens in that handler (`collect.c:1167-1177`) before the toolkit continues to the store |
| 8 | no hit | **confirmed and strengthened** — see below |

**I strengthened link 8 beyond what the record claims.** The record shows no `GUEST_DR_HIT` line. I
additionally counted the raw debug-event stream and found that **no `EXCEPTION_SINGLE_STEP`
(`0x80000004`) was ever delivered to the debugger at all**, in any run of this line:

| Run | `DEBUG_EXCEPTION` events | `80000004` | `GUEST_DR_HIT` |
|---|---|---|---|
| on-1 | 15,036 | **0** | 0 |
| on-2 | 14,358 | **0** | 0 |
| on-3 | 14,396 | **0** | 0 |
| on-4 | 14,429 | **0** | 0 |
| on-5 | 14,446 | **0** | 0 |
| pinned on-2 | 14,340 | **0** | 0 |

(The overwhelming majority are `0xC0000005`; the stream is otherwise healthy and the collector's
dispatch is reached — `collect.c:1163` routes `EXCEPTION_SINGLE_STEP && dr_on()`.) So this is **not**
merely "a hit was not recorded": **no data breakpoint reached the debugger even once** across ~14k
debug events, including for a write that demonstrably happened. The collector code is correct in
isolation (`dr_handle_single_step` is wired, `A2H_DR6_MEANINGFUL 0x0000E00F` correctly avoids the
always-true `dr6 & ~1` bug documented at `collect.c:61-67`). The failure is downstream of correct
arming — consistent with the record's leading hypothesis that a data breakpoint under
`DEBUG_ONLY_THIS_PROCESS` is not surfacing as a first-chance debug event.

**What survives, which I verified rather than assumed:**

- **The census is unaffected and remains certified.** `armed=1 mapped=28 protected=28 touched=0` is
  page-protection based and process-wide; it does not touch DR registers. Confirmed in all five runs.
- **The arming records are unaffected.** `DR7` genuinely read back `0x000D0001`, so **the registers
  were programmed — the watch was armed; it simply never fired.** That distinction is real and I
  reproduce it deliberately.
- **The install positive control is unaffected** (software comparison, `install_ok=1`).
- **The two terminal software reads are unaffected** (generated target and hook read both zero).
- **K = 3 is unaffected as a mechanical-agreement fact**, because none of its three components
  (terminal triple, install control, census) depends on a DR hit.

**What does not survive:** any reading of "zero `GUEST_DR_HIT`" as evidence about writes, and
specifically **leg 3 of the row** as the Planner construed it ("DR/mapping leg reconciled": mapping
certified, the DR half never certified anything). The evidence's
*"**Zero `GUEST_DR_HIT`** in all"* is stated as a positive coverage fact; after this finding it is a
**neutral instrument-status fact** and must be re-labelled.

**Narrowed scope I can certify** (matching the parent's table):

| Channel | Status |
|---|---|
| 28-alias census | **certified** (page protection, process-wide) |
| Mapping stability | **certified** |
| Generated read + hook read both zero | **certified** (software reads) |
| Canonical-address writes (DR0) | **NOT certified** — instrumented but never demonstrated to fire |

Honest row statement: *no write was observed through any of the 28 aliases; the canonical-address
channel was instrumented, but the instrument has never been shown to fire.*

**I do not adjudicate the row.** Per the parent's instruction this is **`PREMISE_CHANGED`** and goes
back to the Planner, and to the Advisor if the mechanism choice is affected. The acceptance is open
and nothing is recorded as final.

## Criterion 7 — No synthetic completion: AGREED

- Forbidden env absent (verified above, criterion 2).
- `git diff --stat d369880..HEAD -- src/recomp/gen` — **empty**.
- `RaiseException(0xE0424943u, EXCEPTION_NONCONTINUABLE, 0, NULL);` — present and unchanged
  (two sites in the game tree).
- `PIO_FREE` deferred, `A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened, `0xFFFFB3` `UNRESOLVED` —
  the evidence makes no contrary claim and nothing in the run set touches them.

## Criterion 8 — N = 5 exhausted: AGREED

Exactly five `*-a2h-repeat-on-*` directories (`…on-1` … `…on-5`); directory count = **5**. No sixth.

---

## Corrections required (evidence, not execution)

1. **Row scope.** The `O-READ-PATH` section is superseded. Re-label "Zero `GUEST_DR_HIT` in all" as
   instrument-status, not coverage; state the canonical-write channel as **not certified**; record
   `PREMISE_CHANGED` and the referral. Do not re-decide the row here.
2. **Run 4's table row is incomplete** — it emits two terminal lines (`00000000` and `00000001`);
   the table shows one. Classification unaffected.
3. **"17/17" coverage sentence is anchored-runs-only** — run 1 is 18/18. Clarify the scope.
4. **Ruling defect to carry to the Planner:** the ruling's claim that "the positive install trap tests
   that the DR watch actually fires" is false; the install trap is a software comparison and the watch
   demonstrably did not fire.

## Evidence outrunning the claims

1. The evidence presents `GUEST_DR_HIT = 0` as part of positive coverage; per the finding it certifies
   nothing about writes.
2. The ruling (quoted approvingly by the evidence) asserts the install trap tests DR firing; it does not.
3. The evidence's "mechanical agreement is COMPLETE" is true for the triple/control/census but is
   stated before, and independently of, the discovery that one of the instrument's channels never
   worked — so "complete" must be scoped to the channels that are certified.

## Blocking

**NONE for the execution and the eight acceptance criteria.** The row outcome is **not** settled: it is
`PREMISE_CHANGED`, pending Planner (and Advisor, if the mechanism choice is affected). No additional
run is authorized by this review.

## Uncertain / cannot verify

- **Why the `#DB` never arrived** — I confirmed *that* it never arrived; I did not establish the
  cause. The second-chance / `DEBUG_ONLY_THIS_PROCESS` hypothesis in the record is plausible and
  untested by me.
- I did **not** run the game (per the anti-loop gate) and did not execute any new run.
- I did not independently re-derive the `K`-scope ruling from the Advisor's original text; I checked
  only its application as quoted.
- The `jsrf_collect.exe` hash I compared is the one in `build\Release\`; I did not separately hash the
  per-run archived copies.
- I did not audit the remainder of `dr_handle_single_step` beyond the arming/hit-recording paths
  cited, nor verify `read_remote` correctness.
