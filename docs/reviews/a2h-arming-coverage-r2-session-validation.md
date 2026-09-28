# `A2h-arming-coverage-attribution-r2` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-arming-coverage-attribution.md`, revision `A2h-arming-coverage-attribution-r2`,
authored by Planner `0f53ea61-7f1c-4574-8313-bcff5c0c67bb` (`codex/gpt-6-sol` @ `high`).
**Authority:** `docs/reviews/a2h-arming-coverage-advisor-reruling.md` — the re-ruling that **shrank** the fix to
reporting-only after catching an error in the Session's own correction.
**Adequacy:** the authoring Planner's §5.3 review — **`ADEQUATE`**, `BLOCKING: NONE`, conditional on the
Session's literal-command and record-semantics verification.

**The Planner named its own limitation honestly:** *"collector-source details were taken from the parent's
corrected evidence, not independently inspected."* **That was reasonable — and it is precisely why the
Session's correction had to reach it before freeze, which it did.**

---

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-arming-coverage-attribution-r2`** |
| Lines | **23** |
| Bytes | **10965** |
| **SHA-256 (frozen)** | **`80E9425977547BC1EE44DD95645F72ACF4F7941857A39B6DB67B427E7DD81D84`** |

## Validation — every checkable claim verified

### The withdrawn inference is correctly repudiated

The packet states `cleared=17` *"counts live threads successfully cleared at teardown **regardless of whether
previously armed**, not seventeen arms"*, and that the defect is *"insufficient **reporting** to certify
continuous coverage, not proven missed arming."* **All present and correct.** **The unsupported "seven silent
arms" inference is gone.**

### The queue/drain is correctly DROPPED, per the re-ruling

The packet retains the existing sweep and adds **"no new arming mechanism, queue/drain or second sweep."**
**The only remaining occurrence of "queue" is the sentence explaining why a hypothetical queue cannot
retroactively cover a pre-mapping exit** — which is the Advisor's required bound, not a reintroduced
mechanism. **Correct.**

### The regression census is REAL — all six tids verified to dispatch guest code

The Advisor named six tids; the packet carries them as *"prior run 1's `50616`, `58620`, `60872`, `66492`,
`67988`, `57376` as a regression census."* **Session-verified in run 1's own artifacts:**

| tid | guest dispatches | verdict |
|---|---|---|
| 50616 | **5555** | dispatches |
| 58620 | **1212** | dispatches |
| 60872 | **227** | dispatches |
| 66492 | **2** | dispatches |
| 67988 | **179** | dispatches |
| **57376** | **286** | **dispatches** |

**All six dispatch guest code, so the census is a valid regression target and not a guess.** **`57376` is the
one the Session had originally missed entirely** — it appears **1149 times** in `stacks.txt` with **1148
first-chance `0xC0000005`**, and the Advisor's list is what put it into the packet.

### The required pre-mapping-exit bound is present

*"prove by complete ICALL-tid/bridge-dispatch evidence that no tid born before mapping availability, then
exited before the handshake, dispatched guest code; otherwise `UNKNOWN`, because neither sweep nor a
hypothetical queue can retroactively cover it."* **This is the one genuinely NEW requirement in the packet
and it is stated with its fail-closed consequence.**

### Anchors, gates and env

| Item | Status |
|---|---|
| `JSRF_TRACE_A2H_DR`, `JSRF_TRACE_A2H_SLOT` | **both present in the built instrument** |
| OOM `598869040` / `12715008` / `50855936`, status `0xC0000017`, prefix `5555` | **all pinned in the packet** |
| `RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, `RECOMP_KERNEL_LOG_BUDGET=100000` | **all prescribed** |
| **`never infer a no-write or named writer from no DR hit`** | **present** — the rule the whole line turns on |

**One advisory, not a blocker:** the packet says *"no synthetic acknowledgments or unresolved-call bypass"*
but **does not name the four variables explicitly** (`RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`,
`JSRF_ALLOW_UNRESOLVED`, `JSRF_ABI_CONTINUE`). **The prose covers them**, and the predecessor named them by
reference. **The Session will spell them in the run command**, which is where an executor needs exact
spellings anyway — recorded so the omission is deliberate rather than unnoticed.

## Freeze decision

**All of the Planner's stated conditions that the Session can verify are satisfied, and the packet's central
premise is the CORRECTED one.** **The packet is `ADEQUATE`, validated, and frozen.**

**Nothing in this validation changes its class (`discovery`), its row set, its `N = 5` bound with early stop,
its validity anchors, or its prohibitions.** **No synthetic completion.** The producer line stays
**PARKED**; `PIO_FREE` stays **DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3`
stays **`UNRESOLVED`**.

## What the successor must do, in order

1. **C1** — reporting-only collector fix: print every successful arm **at the event** with `tid`, `why`, DR7
   readback and seq/tick; record post-handshake successes (now silent); publish a **terminal full-list
   summary**. **No new arming mechanism.**
2. **C2** — lossless per-birth reconciliation over **every** `CREATE_THREAD` event, plus the operational
   dispatch criterion, the six-tid regression census, and the **pre-mapping-exit bound**.
3. **Fixture tests**, then **guards** and collector harness probes.
4. **One fresh OFF** (the code change breaks the carry rule), record-level inertness.
5. **Up to `N = 5` ON runs**, early stop **only** on two coverage-complete TARGETs that **agree**.
