# A4p-r1 adequacy review — ADEQUATE

**Packet:** `docs/packets/a4p-pio-gate-analysis.md`
**Revision:** `A4p-r1`, frozen SHA-256 `B8BDBFAEC31213AA43D7A2BC8771326A73BE6CC124B6E2993BD2D33C57687A4B`
**Class:** discovery (`docs/agent-workflow.md` §5.8) — self-reviewed by its writing Planner against §5.3's two questions; **no second Planner spawned** (§5.1.5).
**Reviewer:** Planner child `7e5d78ef-fc04-407e-897c-9eafe739ab5f`, route `claude` / `claude-opus-5-5` @ `high`
**Governing ruling:** `docs/reviews/a4b-pio-methodology-ruling.md` (Advisor, §5.5 redesign)
**Session role:** verified that every command runs (§5.1.2); did not author the criteria.

## Planner adequacy block (verbatim)

```text
REVISION: A4p-r1, SHA-256 B8BDBFAEC31213AA43D7A2BC8771326A73BE6CC124B6E2993BD2D33C57687A4B
READ: a4b-pio-methodology-ruling.md (whole); a4b-r3-adequacy-review.md (whole); a4b-r2-adequacy-review.md (blocking lines); agent-workflow.md §5.3–§6.3; a4b-gp-dsp-engine.md lines 1-10 and 181-227; a4a packet header (form); inspect-jsrf.py arg parser; ran find 0xFE820010 (28), data 0x1C4004 4 (800000A1), find 0x001A1BAF (0), and the E2 reconciliation (28/10/18, VAs match); Get-FileHash XBE (matches); HEAD fa3d875; toolkit 0d7929c; kernel_thunks.c/kernel.h exist.
PREMISE_FRESHNESS: PASS. The XBE hash, the 28-site population, the by-value reconciliation and the 0x1C4004 slot were re-observed at fa3d875.
BLOCKING: NONE
DEFERRED:
- D1: when some sites FAIL and others are UNKNOWN, first-match selects O-DATA, and the UNKNOWN list is recorded only in the table. That matches the ruling (a use requires a model).
- D2: the "premise looks false widely" stop has no numeric threshold. It fails closed to O-UNKNOWN.
- D3: the revision-history file is not yet created.
DECISIONS:
- Misread risk. The rows are ordered with identity checks first and O-GATE last. O-GATE requires every site to PASS and E3 to be clean, and any gap is UNKNOWN. So there is no path from missing or partial evidence to O-GATE. Reversed by: a site result recorded without its per-path trace, which the acceptance reviewer catches.
- Safe and reversible. The packet is read-only static analysis with one evidence file, no code, no build, no run and no instrumentation. Reversed by: nothing in scope.
- C1–C4 transcribed with executor precisions. None of the precisions widens PASS: each one either makes a register read more likely to count or sends an unclear case to UNKNOWN. Reversed by: an Advisor reading that a precision contradicts the ruling.
VERDICT: ADEQUATE
```

`VERDICT` is `ADEQUATE` because `BLOCKING` is `NONE` and `PREMISE_FRESHNESS` is `PASS` (§5.3). The two discovery questions are answered: an outcome **cannot** be misread into the wrong row — the rows are ordered identity-first and `O-GATE` last, and `O-GATE` requires all 28 sites PASS **and** E3 clean, with any gap routing to `O-UNKNOWN`; and the packet is **safe and reversible** — read-only static analysis, one evidence file, no code, no build, no guest run, no instrumentation.

## Session mechanical verification (§5.1.2) — every command runs

Run at game `fa3d875`, toolkit `0d7929c`:

| Command / value in the packet | Result |
|---|---|
| `(Get-FileHash game\default.xbe).Hash` | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` — matches the packet's baseline |
| **E2's PowerShell reconciliation, run verbatim** | prints **`28`**, then `0xFE820010 10`, then `-25034736 18` |
| E2's 28 VAs vs E1's 28 VAs | **identical sets**, one-to-one, no extras on either side |
| `inspect-jsrf.py find 0x001A1BAF` | `0 occurrence(s)` — `sub_001A1BAF` is not address-taken |
| `inspect-jsrf.py data 0x1C4004 4` | `800000A1` (ordinal 161) |
| `inspect-jsrf.py find 0xFE820000` | `0 occurrence(s)` |
| `inspect-jsrf.py find 0x00020010` | `1 occurrence(s)` |
| `inspect-jsrf.py find 0xFE800000` | `7 occurrence(s)` |

The E2 command was checked specifically because it is **new** in this packet: it normalises
every `MEM8/16/32` operand mod 2^32 and compares against `4269932560`, because PowerShell
reads the literal `0xFE820010` as a negative `int32`. It reproduces both the count and the
exact VA set, so the packet's "reconciliation by value, not text" step is executable as
written rather than merely plausible.

## Deferred advisories (recorded, not acted on)

1. **D1** — when some sites FAIL and others are `UNKNOWN`, first-match selects `O-DATA` and
   the `UNKNOWN` list is recorded only in the table. This matches the Advisor's ruling: a
   use requires a model regardless of other unresolved sites.
2. **D2** — the "premise looks false widely" stop has no numeric threshold. It fails closed
   to `O-UNKNOWN`, so it cannot produce a false `O-GATE`.
3. **D3** — the revision-history file did not exist. **Resolved by the Session** as a
   mechanical pointer fix: created `docs/reviews/a4p-revision-history.md`. Not a criterion
   change.

## Frozen revision

The packet is frozen at exactly the hash above; **no criterion, outcome row, experiment or
stop condition was altered** by the Session. Its trailing revision narrative lives in
`docs/reviews/a4p-revision-history.md` (§5.7). Promoted into `CURRENT PACKET` in this same
step (§5.3: "ADEQUATE ends plan iteration… freezes and promotes on ADEQUATE as for any
packet").
