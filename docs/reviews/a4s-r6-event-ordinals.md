# A4s-r6: which event ordinals JSRF actually imports

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** measured evidence completing `docs/reviews/a4s-r6-ordinal-reachability.md`.
**Script:** `logs/a4s/event-ordinals.py`.

## Measured

| Ordinal | Name | Declared? | Thunk | Measured called? |
|---|---|---|---|---|
| **145** | `KeSetEvent` | **YES** | `0x001C4014` | no (not on the observed path yet) |
| **159** | `KeWaitForSingleObject` | **YES** | `0x001C4018` | no |
| **189** | `NtCreateEvent` | **YES** | `0x001C3F98` | no |
| **186** | `NtClearEvent` | **YES** | `0x001C3FA4` | no |
| **225** | `NtSetEvent` | **YES** | `0x001C3FA0` | no |
| 16 | `ExEventObjectType` | YES | `0x001C3F9C` | no |
| 108 | `KeInitializeEvent` | **NO** | — | no |
| 110 | `KeInitializeMutant` | **NO** | — | no |
| 138 | `KeResetEvent` | **NO** | — | no |
| 146 | `KeSetEventBoostPriority` | **NO** | — | no |

Positive control: ordinal 190 (`NtCreateFile`), known measured-called, is declared — so the table
distinguishes correctly.

## What this completes

The merged tree has **two event models**, and this table says which of their entry points JSRF can
reach:

**Reachable (declared):**
- `KeSetEvent` (145), `KeWaitForSingleObject` (159) — the two functions hunks 1 and 2 decide.
- `NtCreateEvent` (189), `NtClearEvent` (186), `NtSetEvent` (225) — the handle-based Nt family.

**Unreachable (not declared):**
- `KeInitializeEvent` (108) — so upstream's shadow-table **populator** cannot be reached.
- `KeResetEvent` (138) — so **both** sides' `bridge_KeResetEvent` bodies are dead code for this title.
- `KeSetEventBoostPriority` (146), `KeInitializeMutant` (110).

## Consequences

1. **Upstream's shadow table has no reachable producer.** `ke_shadow_insert` is called from
   `bridge_KeInitializeEvent` (ordinal 108, not imported), `bridge_KeInitializeMutant` (110, not
   imported), `bridge_KeInitializeTimerEx`-style paths and `bridge_NtCreateEvent`. Of those, only
   `NtCreateEvent` (189) is declared. So a *combined* form would add a lookup whose main populator
   cannot run — which is a reason to weigh the combined form on its merits, not to assume it is
   needed for coverage.

2. **The `bridge_KeResetEvent` duplicate is mechanical.** Its ordinal is unreachable, so the choice of
   *which* body survives does not change JSRF behaviour. What is non-negotiable is that **exactly one
   definition** remains: two definitions in one translation unit is a compile error independent of
   reachability.

3. **The Size-convention hazard is latent, not live** (already recorded): it required ordinal 108.

4. **The live semantic decision is `KeSetEvent` (145) and `KeWaitForSingleObject` (159)** — both
   declared, both exercised by the accepted contract test, which requires guest `SignalState` writes
   that upstream's bodies do not perform (`docs/reviews/a4s-r6-contract-discrimination.md`).

## Limits (unchanged, restated)

- "Not declared" does not prove the guest never computes an ordinal dynamically and calls a thunk slot
  directly; that needs a guest-code search for indirect calls into the thunk region, which I have
  **not** done.
- `declared=True` does not mean *reached on the observed path* — none of 145/159/186/189/225 appears in
  the measured-called set yet, because the run stops at the DSP spin long before these are used. So
  "reachable" here means *the guest has a thunk for it*, not *the guest has been observed to call it*.
- The table comes from `game/mygame_analysis.json`, produced by earlier tooling; its positive control
  (27/27 measured-called ordinals present) is what makes it usable here.
