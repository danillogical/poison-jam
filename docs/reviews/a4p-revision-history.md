# A4p revision history (non-authoritative)

Per `docs/agent-workflow.md` §5.7. Not reviewed for accuracy; loses to the frozen contract
on any conflict. It exists so the packet carries no revision narrative.

## A4p-r1 — first revision

Written by Planner child `7e5d78ef-fc04-407e-897c-9eafe739ab5f` (route
`claude` / `claude-opus-5-5` @ `high`) against the Advisor's §5.5 methodology ruling.
Class: **discovery**. Original SHA-256
`B8BDBFAEC31213AA43D7A2BC8771326A73BE6CC124B6E2993BD2D33C57687A4B`.

### Why this packet exists rather than a fourth `AC-PIO` patch

`A4b`'s `AC-PIO` criterion took **two consecutive `INADEQUATE` adequacy verdicts** on the
same mechanism, meeting §5.5's redesign trigger:

- **`A4b-r2` (B1).** Its callee rule excused an import only when the polled register was
  `ebx`/`esi`/`edi`/`ebp`. At `eax`-live sites — including the criterion's own known-good
  control — a literal executor had to mark the site `UNKNOWN`, so every execution would
  have selected `R-UNKNOWN` after the whole GPL port and both runs.
- **`A4b-r3` (B1).** The replacement exit-trace rule was **sound** — the reviewer found no
  false-PASS path — but it treated `eax`/`ecx`/`edx` as possibly live across every call and
  `ret`, so dead `edx` scratch climbed through callers until a 3-level bound returned
  `UNKNOWN`. Again a deterministic false `UNKNOWN`.

The Session took the methodology to the Advisor instead of briefing a fourth revision
(§5.5: "A third patch of the same shape is not allowed").

### What the Advisor ruled, and how A4p reflects it

Ruling recorded verbatim in `docs/reviews/a4b-pio-methodology-ruling.md`:

- **Shape.** Replace the whole-program dataflow proof with a **locally-checked
  calling-convention rule** (C1–C4): assume the standard x86 convention and *verify it at
  every boundary the analysis relies on*, so each trace is bounded by the site's own
  function plus one-level callee checks plus a caller trace only for `eax` (or `edx` in the
  `edx:eax` pass-through case).
- **Home.** The analysis is itself execution (§5.1.3) and its result decides whether
  `A4b`'s claim is even the right one. It therefore moves into a **discovery packet** that
  runs first, and `A4b` cites its accepted outcome as a precondition.
- **Worked cases.** The Advisor resolved the two sites that defeated r3 by hand
  (`001A38F4` and `001A34EA`, both `edx`); the Session reproduced both disassemblies. A4p
  evaluates them like every other site rather than presuming them.

### Session mechanical fill-in (before adequacy review)

Per §5.1.2 the Session verified that every command in the packet runs, at game `fa3d875` /
toolkit `0d7929c`:

| Command / value | Result |
|---|---|
| `(Get-FileHash game\default.xbe).Hash` | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` — matches the packet's baseline |
| E2's PowerShell reconciliation command, run verbatim | prints **28**, `0xFE820010 10`, `-25034736 18` |
| E2's 28 VAs vs E1's 28 VAs | **identical sets**, one-to-one, no extras either side |
| `inspect-jsrf.py find 0x001A1BAF` | 0 hits (so `sub_001A1BAF` is not address-taken) |
| `inspect-jsrf.py data 0x1C4004 4` | `800000A1` (ordinal 161) |
| `inspect-jsrf.py find 0xFE820000` | 0 hits |
| `inspect-jsrf.py find 0x00020010` | 1 hit |
| `inspect-jsrf.py find 0xFE800000` | 7 hits |

The packet's E2 command was checked specifically because it is new: it normalises every
`MEM8/16/32` operand mod 2^32 and compares against `4269932560`, since PowerShell reads the
literal `0xFE820010` as a negative `int32`. It reproduces the count and the exact VA set.

### Deferred advisories carried from the adequacy block

- **D1:** when some sites FAIL and others are `UNKNOWN`, first-match selects `O-DATA` and
  the `UNKNOWN` list is recorded only in the table. That matches the ruling — a use
  requires a model regardless of other unresolved sites.
- **D2:** the "premise looks false widely" stop has no numeric threshold. It fails closed
  to `O-UNKNOWN`, so it cannot produce a false `O-GATE`.
- **D3:** this file — created by the Session, as the packet's pointer required.
