# `A2h-slot-read-path-displacement-r1` — Run 1 (OFF) evidence: **`O-COVERAGE`**, Run 2 **NOT LAUNCHED**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-slot-read-path-displacement-r1`, frozen
**`75E6AE7C9A26A2CF63F9AD19502A170B2960FE2C90523A566F3F382942BB9B6A`**.
**Run 1 (OFF):** `logs/runs/20260928-014526-583-a2h-slot-write-inert-off` — `--profile strict --seconds 8`,
`RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, `RECOMP_KERNEL_LOG_BUDGET=100000`, **no A2h or DR gates present**
(verified in the archived `settings`).
**Run 2 (ON): NOT LAUNCHED.** The packet's own precondition was not met.

---

## The instrument IS inert — that part passed

| Check | Result |
|---|---|
| Effective env | **no `JSRF_TRACE_A2H_DR`, no `JSRF_TRACE_A2H_SLOT`, no `RECOMP_KERNEL_WATCH*`** |
| `[A2HSLOT]` lines | **0** |
| `GUEST_DR_ARM` / `GUEST_DR_HIT` / `GUEST_DR_ARM_FAIL` | **0 / 0 / 0** |
| `GUEST_SLOT_TRANSITION` | **0** |
| `alias census` / `handshake` | **0 / 0** |
| **Collector latch summary** | **`install_seen=0 install_raw=00000000 install_value=00000000 install_ok=0 claimed=0 overflow=0 partial=0`** |
| Frozen-registry extraction | the collector now prints the latch, and it reads **all-zero**, i.e. **the install callback never ran** |

**So the gates are genuinely off and the run is inert at the RECORD level, not merely the log level** — which
is the stronger control the packet demanded. **The new instrumentation did not perturb anything it gates.**

## But the run is NOT COMPARABLE to the accepted baseline — so the packet says stop

The packet: *"If OFF fails same OOM (`598869040`), same checked site/fault, and comparable numbered prefix,
**do not launch ON**; choose `O-COVERAGE`."* **All three comparability checks failed:**

### Check 1 — OOM value/status

**The OOM itself reproduced identically:** `xbox_HeapAlloc: out of memory (requested 598869040, used
12715008/50855936)` and `[KERNEL] → returned 0xC0000017` — **same request, same status, same used/total.**

### Check 2 — same checked site / fault — **FAILED**

| Run | Invalid-target events | Target | Site |
|---|---|---|---|
| **OLD (accepted OFF)** | **1** | `0x00000000` | **`0x0014982E`** |
| **NEW (OFF)** | **2** | `0x00000000` **and `0x00000001`** | **`0x00147DBC`** and **`0x0018CE73`** |

**The new run fails EARLIER and DIFFERENTLY.** The terminal event is no longer the ordinal-277 thunk call at
`0x00149828` that the whole A2h line has been about. **It is a NULL call through a *different* thunk slot:**

```
00147DB0  inc   ebp
00147DB1  or    byte ptr [eax - 1], dl
00147DB4  jne   0x147dbe
00147DB6  call  dword ptr [0x1c4078]     <-- a DIFFERENT slot (0x1C4078, not 0x1C4064)
00147DBC  test  eax, eax
```

**And there is a second failure with target `0x00000001`** — not even NULL — at `0x0018CE73`, in the **D3D**
section.

### Check 3 — comparable numbered prefix, per thread IDENTITY — **FAILED**

*(Compared by thread **identity**, not tid, because tid values differ between runs — the same discipline an
earlier acceptance review required.)*

| identity | OLD | NEW | match |
|---|---|---|---|
| 1 | **5555** | **5555** | **yes** |
| 2 | 2 | 2 | yes |
| 3 | 872 | **765** | **NO** |
| 4 | 143 | **124** | **NO** |
| 5 | 131 | **116** | **NO** |

**The main thread (identity 1) is EXACTLY identical — 5555 dispatches in both runs.** The auxiliary worker
threads (3, 4, 5) each did **less** work, and one of them reached a NULL call first.

---

## Outcome: `O-COVERAGE` → `A2h-slot-write-coverage-provenance`

**Run 2 was not launched, and no attribution row is selected.** Per the packet, a divergent OFF run means the
ON-vs-OFF comparison that would license interpretation **does not exist**, so **no zero-writer, guest-writer,
host-writer, transient, or read-path conclusion follows.**

## What this run nevertheless establishes — recorded because it is genuinely new

**1. The gates are inert.** Zero trace lines, and the collector's own latch summary reads all-zero with
`install_seen=0`. The instrument does not fire when switched off, **confirmed at the record level.**

**2. The terminal failure is NOT deterministic across runs.** This is the significant finding. The accepted
baseline and this run share **the same OOM request and status** and **an exactly identical main-thread prefix
(5555 dispatches)**, yet they **diverge in which thread faults, on which instruction, and with how many
auxiliary dispatches**. **The auxiliary threads' progress is not reproducible between runs.**

**3. That divergence is NOT attributable to the instrumentation**, on the evidence available: the gates were
off, every gated print count is zero, and the latch shows the install callback never ran. **A run-to-run
scheduling difference is the natural reading**, and it is **the same class of nondeterminism that made the
original bridgeless gap possible** — the gap exists because the terminal read is not bracketed by a bridge
call, and the auxiliary threads' progress is what moves the boundary.

**Recorded as an observation, not a conclusion.** The Session did **not** set out to measure run-to-run
variance and this is **one** comparison; **a claim that A2h is nondeterministic would need its own packet with
a pre-specified repeated-run design.** What is established is narrower and sufficient: **the accepted baseline
is not reproducible, so it cannot serve as the OFF control this packet requires.**

## Consequence for the successor

**The packet's premise needs re-establishment before it can be executed.** Its OFF control is "the accepted
baseline, with gates off" — but the baseline **does not reproduce**, so the two-run OFF/ON design cannot
produce an interpretable ON-vs-OFF comparison. **That is a planning matter for the Planner and, if the
mechanism choice is affected, for the Advisor — not something the Session should paper over by running ON
anyway.**

**No third run** was taken, per the packet's own bound. **No synthetic completion.** The producer line stays
**PARKED**; `PIO_FREE` stays **DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3`
stays **`UNRESOLVED`**.
