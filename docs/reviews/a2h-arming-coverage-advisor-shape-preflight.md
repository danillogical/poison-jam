# Advisor shape preflight — arming-coverage successor to `A2h-slot-within-run-attribution-r1`

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e75d-4aa9-7000-b8e1-ac6cbc1a2720`.
**Raised because:** the successor requires a **bounded toolkit change** (the collector's arming path), which
the owner's toolkit constraint makes **preflight-mandatory**. The Planner cannot reach the Muse handle, so the
Session relayed the measured defect and returned the ruling.
**Authority:** `docs/agent-workflow.md` §2.3 (Advisor), §4 (escalation).

**Route note:** two earlier attempts at this preflight failed with `MODEL_UNAVAILABLE` (*"model stream idle
timeout after 180000ms"*). A one-sentence probe confirmed the route itself was healthy, so the cause was the
**prompt length at high reasoning effort** exceeding the idle timeout, not an outage. **Re-sent concisely at
`medium` effort and served.** Recorded because a future reader should not diagnose this as a route failure.

---

## RULING

```text
SHAPE: PROCEED — with binding corrections below.
BASIS: session-measured leads in your brief; inferred only the seam choice.
```

**1. Seam: (b) + (a), reject (c).** At handshake, sweep all live threads **and** drain the queued `CREATE_THREAD` tids; arm every unarmed live thread once the offset exists. (a)-alone misses threads that exit before the handshake; (b)-alone misses births the debug path never records. Do both; keep it in the collector. No game-side arming.

**2. Yes — per-birth record required.** Record every birth/attempt/outcome: tid, timestamp, offset-available?, queued/armed/failed/exited. Terminal reconciliation must cover **every tid that dispatched guest code**; any dispatching tid never attempted → `UNKNOWN`, never absence.

**3. Yes — fresh OFF required.** Code changed, so the carry rule fails. Same build, gates OFF, then ON.

**4. Keep N = 5, with pre-specified early stop.** Run until **two coverage-complete TARGETs** with independent attribution; stop early only if they agree, otherwise complete N = 5 and report per-realization + `UNKNOWN` generality. Zero targets in N → report rate, stop, re-refer. No extension.

**5. Scope confirmed minimal:** fix arming → fresh OFF → re-run → attribute. Do not re-litigate the census; no second-slot attribution; read-path legs untouched.

**6. Bracket changes nothing structural.** Between-boundaries zero corroborates the bridgeless gap and the canonical-writer hypothesis, but it does not name the writer. No new row, no instrument change; still need RIP/class from the trap.

---

## Why each correction is load-bearing

**Correction 1 rejects my own proposal as insufficient — and it is right.** I proposed the handshake sweep
alone (option b). The Advisor's objection is precise: **a sweep of *live* threads misses any thread that
exited before the handshake**, and **the queue alone misses births the debug path never recorded.** The
measured defect shows **both failure modes are real in this run** — four guest threads were never attempted
*at all*, which is the second mode. **Doing only what I proposed would have left a residual hole.**

**Correction 2 is the absent-record-as-negative rule applied to the defect itself.** I asked whether "never
attempted" needs a positive control; the Advisor ruled **yes**, and specified the reconciliation bar: **every
tid that dispatched guest code must be accounted for**, and **any dispatching tid never attempted is
`UNKNOWN`, never absence.** That is exactly the shape this line keeps meeting — including in this very
defect, where `armed=10 failed=9` *looked* like a 90% failure and was actually a 100% miss over the threads
that mattered.

**Correction 4 adds a pre-specified early stop the packet did not have**, which is the anti-optional-stopping
rule made concrete: **stop early only on agreement**, otherwise complete `N = 5`. **It does not license
stopping early to chase a favourable result.**

**Correction 6 refuses to let a strengthening observation change the contract.** The between-boundaries
bracket is genuinely stronger evidence for a canonical writer, and the Advisor's answer is that it **still
does not name the writer** — so **no new row, no instrument change.** Recorded because the temptation to
promote a good finding into a row is exactly what the row-priority discipline exists to prevent.
