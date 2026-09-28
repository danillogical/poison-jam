# `A2h-integrity-audit-r1` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-integrity-audit.md`, authored by Planner
`4537025e-678f-43fe-8741-bf93081df246` (`codex/gpt-6-sol` @ `high`), committed `ee86e7a`.
**Authority:** `docs/reviews/a2h-callback-slot-identity-advisor-ruling.md` point 2 — *"Bounded audit PACKET
first … dependency, not hygiene."*
**Adequacy:** the Planner's §5.8 review — **`ADEQUATE`**, `PREMISE_FRESHNESS: BOUNDED`.

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-integrity-audit-r1`** |
| Lines | **17** (18 with the trailing newline) |
| Bytes | **6116** |
| **SHA-256 (frozen)** | **`03F53E0AF09283EBD2B14886DC3FA625C25402875107D0759537A7C9D1B242F9`** |

## Validation — 14 of 14 checks pass

**Every element of the Advisor's mandate is present, and the Session verified the worked control against its
own measurement:**

| Mandate | Packet |
|---|---|
| **accepted A2h rows ONLY** | *"inspect only accepted A2h rows"* |
| **read-only, no runs/build/tests/instrumentation** | stated in the authority line |
| **input source classification** | all four classes named: `log/trace/code-static`, `guest-memory-from-dump`, `live hook reads`, `inference` |
| **per-run gate, NOT census** | *"separately for EACH run actually used"* |
| **census caveat** | *"MUST NOT be cited for any specific accepted row"* |
| **label-only for open/unknown** | `ASSERTS-NOTHING` |
| **verdicts** | `TAINTED`, `CLEAN`, `NO-GUEST-MEMORY`, `ASSERTS-NOTHING` |
| **known-answer control for EVERY extraction** | stated in caps |
| **the worked `djv000_0.adx` control** | **and the Session verified it matches its own measurement exactly** |
| **the `.text` control prefix** | `8b512c85d28b4130c70190431c00741c` |
| **remediation list** | required |
| **other lines excluded** | *"or other-line audit"* |
| **caller-trace gated behind it** | *"only after this audited inventory"* |
| **no concurrent trace** | stated |

## The design decisions worth recording, because they are the right ones

**1. The packet forbids backfilling.** Line 11: *"An accepted `O-OPEN`/`UNKNOWN` asserts nothing positive:
verify only final row label, revision and acceptance identity, report `ASSERTS-NOTHING`; **do not backfill
positive claims from narrative as though the row accepted them.**"* **That is the discipline this line has
needed repeatedly — an `O-OPEN` row with a strong narrative must not be read as having accepted the
narrative.**

**2. Identity gaps take PRECEDENCE over closure.** Line 14: *"`A-IDENTITY` … and `A-OPEN` … **take priority over
closure even if a taint is already flagged.**"* **So a tainted row cannot be reported until its identity is
established — which prevents a mislabelled taint.**

**3. The run binding is explicitly primary.** Line 6: *"corroborate the row-to-run link in primary
packet/review/log metadata … absent/ambiguous run or unavailable gate yields `A-IDENTITY`/`A-OPEN`, **never
CLEAN**."* **Fail-closed, which is the correct default for an audit.**

**4. It forbids repairing a dump.** Line 7: *"do not repair a shifted dump, infer a passing status from
another run, or replace a missing primary result with a quotation."* **That is the AGENTS.md rule
(*"it must never be used as a correction that fabricates repaired guest state"*) applied to an audit.**

**5. `A-CLEAN` is explicitly NOT a claim that things work.** Line 14: *"`A-CLEAN` = complete reconciled
inventory with none → **permit a separately scoped caller-trace packet, not a claim it already works.**"*

**6. The `A-CLEAN` condition is correctly narrow.** Line 12: **`CLEAN` requires positive inputs to *include*
dump guest memory *and* all implicated runs to pass** — **so a row that simply does not use guest memory is
`NO-GUEST-MEMORY`, not `CLEAN`.** **That distinction prevents `CLEAN` from being over-counted.**

## One honest note on the pinned baseline

**The packet pins nothing explicitly**, which is appropriate for an audit that must **resolve run identities
from primary metadata**. **The tree is at `ee86e7a`; the toolkit at `37226b2`.** **The audit is read-only and
resolves its own identities, so no re-pin is required** — **but the audit record should state the tree state
it ran against**, which the Session will ensure.

## Freeze decision

**The packet is read-only, correctly scoped, fail-closed, and every validation check passes.** **It is
`ADEQUATE`, validated, and frozen.**

**Nothing changes its class (`discovery`/audit), its rows, or its prohibitions.** **No synthetic completion.**
**No toolkit change is required or authorized.** The producer line stays **PARKED**; `PIO_FREE` stays
**DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**; **the
retired NULL line stays retired and no DR record may be cited.**
