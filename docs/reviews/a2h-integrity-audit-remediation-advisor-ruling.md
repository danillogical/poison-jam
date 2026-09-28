# Advisor ruling — `A2h` integrity audit remediation: demote R-1, correction record, summary stands

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e83a-6c5b-7000-9dd1-37f9a312b627`.
**Raised because:** the integrity audit returned `A-TAINTED` with one remediation item R-1, and packet line 16
requires a separate remediation decision before any dependent trace.
**Authority:** §2.3, §4.3.

---

## RULING

**RULING: demote (re-derivation from archives foreclosed, not merely declined); correction record, not a packet; summary as framed, no stronger statement.**

**1. DEMOTE — with wider scope than briefed.** The dump-window table goes non-admissible (same record refused the same dump for the slot question — indefensible asymmetry), **and so does the caller return-address binding** (`[0x00F7FEA0]` sits inside the tainted window — the brief omits this and it must be included: caller drops to candidate-set). What stands mapping-immune: frame arithmetic (2× log-`esp` + log-ordinal), call-site arg counts (static), no-writer proof (static search). Row `O-OPEN` stands — *strengthened* (missing-value ground intact; its "recorded value at `0x00F7FEAC`" specificity survives verbatim via arithmetic). Re-derivation is **foreclosed, not declined**: returned frames leave no trace and the crash activation overwrote (proven in-record) — no archive can contain it; live capture is the row's named successor, not re-derivation. The crash-register match is preserved UNEXAMINED as the lead back, if anyone ever needs the window (provenance of those registers first).

**2. CORRECTION RECORD.** The remedy is documentation — nothing obtainable to re-derive, no criterion to decide, row unchanged. Append-only dated erratum on the accepted record (original bytes preserved — same form as the OOM correction); no re-review (§3.2); Session writes, links from audit + plan.

**3. Summary as framed is honest; no stronger statement.** `CLEAN`-zero is the correct output of the rule (no row stakes positives on passing-run dump memory), not a hygiene deficit. `NO-GUEST-MEMORY` stands on host-address provenance — acceptance verifies that classification (the one spot-check that matters). **Handoff the brief doesn't state but requires:** callback-slot-writer-r1 is PENDING, hence outside audit scope — its acceptance must verify its register inputs' provenance; audit silence there is scope, not clearance.

**BASIS:** observed — corroboration table + caller binding both inside the tainted window; arithmetic/static routes outside it; packet row texts as cited. Inferred — nothing load-bearing.

**REVERSED BY:** a mapping-clean archive shown to capture the OOM activation live (contradicts the activation-reuse proof — implausible, stated for completeness).

**RECORD IN:** verbatim in the audit review record; Session writes the demotion erratum + plan pointers; caller-trace waits on its acceptance's provenance check.

---

## The correction the Session MISSED, and it matters

**The Session's remediation brief named ONE affected claim** — the `E = 0x00F7FEA0` corroboration table.
**The Advisor found a SECOND:**

> **the caller return-address binding** — `[0x00F7FEA0] = 0017C926`, recorded as *"return address =
> `call@0x0017C921 + 5` — exact"*.

**The Session verified the geometry:** **`0x00F7FEA0` lies INSIDE the tainted window `0x00F7FE8C..0x00F7FEAC`.**
**So the caller binding read from the dump is equally inadmissible, and the caller attribution drops to a
candidate set.**

**Why the Session missed it:** it treated the corroboration *table* as the affected artifact **without checking
whether the row's other uses of the same window fell inside it.** **The Advisor read the window's geometry
against every claim that cited it; the Session read only the claim it had been pointed at.**

> **Recorded as the fourth reading failure this session, and the pattern is consistent: the Session reads the
> artifact it is directed to and does not check the surrounding geometry.** **The Advisor's correction is now
> the erratum's second item.**

## The re-derivation point — a distinction the Session got wrong

**The Session offered re-derivation as an option and leaned toward declining it as redundant.** **The Advisor
refused the framing:**

> **"Re-derivation is foreclosed, not merely declined: returned frames leave no trace and the crash activation
> overwrote (proven in-record) — no archive can contain it."**

**So it is not that re-derivation is unnecessary — it is IMPOSSIBLE.** **That is a materially different
statement, and it means no future packet may propose it.** **A live capture is the row's NAMED SUCCESSOR.**

## The handoff the Session's brief did not state

> **"callback-slot-writer-r1 is PENDING, hence outside audit scope — its acceptance must verify its register
> inputs' provenance; audit silence there is scope, not clearance."**

**The Session had noted the pending row used only the passing run, and treated that as sufficient.** **The
Advisor's point is sharper: the audit's silence about a pending row is a SCOPE boundary, not a finding, and
the pending row's acceptance must therefore verify provenance itself.** **Recorded as a requirement on that
acceptance, not an assumption.**

## What the zero-`CLEAN` result means — confirmed, with no stronger statement needed

**`CLEAN`-zero is the CORRECT OUTPUT of the rule**, not a hygiene deficit: **no accepted row stakes positives on
passing-run dump memory.** **And `NO-GUEST-MEMORY` stands on HOST-ADDRESS provenance** — **acceptance verifies
that classification, which the Advisor calls *"the one spot-check that matters."***
