# A4s-r6: ordinal reachability — 108 and 138 are not imported by JSRF

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** measured evidence for the Advisor's hunks 1–2 ruling. Not a ruling.
**Scripts:** `logs/a4s/reachability.py`, `logs/a4s/verify-ordinals.py`, `logs/a4s/ordinal108c.py`.

## Finding

JSRF's XBE declares **120** kernel imports. Checked against
`game/mygame_analysis.json` (the same source `scripts/gen-kernel-import-contracts.py` uses):

| Ordinal | Name | Declared in XBE? | Import thunk |
|---|---|---|---|
| 145 | `KeSetEvent` | **YES** | `0x001C4014` |
| 159 | `KeWaitForSingleObject` | **YES** | `0x001C4018` |
| **138** | **`KeResetEvent`** | **NO** | none |
| **108** | **`KeInitializeEvent`** | **NO** | none |
| 146 | `KeSetEventBoostPriority` | **NO** | none |
| 110 | `KeInitializeMutant` | **NO** | none |

Every declared `Ke*` ordinal: 95, 97, 98, 99, 100, 107, 109, 113, 119, 124, 125, 128, 129, 137, 139,
142, 143, 144, **145**, 149, 151, 153, 156, **159**. The gaps at **108, 110, 138, 146** are real
absences, not a parsing artefact — 107/109 and 137/139 bracket them.

## Coverage witness (absence needs one — `docs/agent-workflow.md` §2.4.5)

The declared table is a usable population, shown by a **positive control**: all **27** ordinals that
`docs/jsrf-kernel-import-contracts.md` records as *measured called on a real run* are present in it —
**zero missing**. So the table covers what is actually callable, and it is not an empty or truncated
list.

## What this establishes, and what it does not

**Establishes (observed):** a guest call reaches the kernel through the import thunk table, and an
ordinal that is not declared has **no thunk to call**. So JSRF cannot reach ordinal 138
(`KeResetEvent`) or ordinal 108 (`KeInitializeEvent`) through the normal import mechanism.

**Does NOT establish:** that the guest never computes an ordinal dynamically and calls a thunk slot
directly. That would need a guest-code search for indirect calls into the thunk region, which I have
**not** done. Recorded as a limit rather than glossed.

**Provenance limit:** the table comes from `game/mygame_analysis.json`, produced by earlier tooling; I
did not re-derive it from the XBE in this session. Its own positive control (27/27) is what makes it
usable here.

## Consequences for the hunks 1–2 ruling

1. **Ordinal 138 is not reachable by JSRF.** Both sides' `bridge_KeResetEvent` bodies exist to serve
   that ordinal. Whatever the merge decides, the function is **dead code for this title** unless the
   dynamic-call limit above is contradicted. That materially lowers the stakes of choosing its body —
   but it does **not** make the *duplicate definition* harmless: two definitions in one translation
   unit is a compile error regardless of reachability, so exactly one must survive.

2. **The Size-convention hazard is likewise unreachable in practice.**
   `docs/reviews/a4s-r6-size-convention-hazard.md` described a type confusion where an event created
   by upstream's `bridge_KeInitializeEvent` (which writes `Size = 16`) would fail local's sniffer
   (`Size == 4`) and reach `SetEvent` with a guest pointer. **That scenario requires a guest call to
   ordinal 108, which is not imported.** So the hazard is **latent, not live** for JSRF. It is still
   worth recording — the merged code is internally inconsistent, and a future title or a
   dynamically-computed call could reach it — but it should **not** be the basis for a hunk-1/2
   side choice, and it does not justify a redesign on its own.

3. **Ordinals 145 and 159 ARE imported**, so `bridge_KeSetEvent` and
   `bridge_KeWaitForSingleObject` are live for this title. Those are the two functions whose
   semantics genuinely matter, and they are exactly the ones the accepted contract test exercises
   (`docs/reviews/a4s-r6-contract-discrimination.md`). So the ruling should turn on **145 and 159**,
   not on 138.

## Consequence for the packet

The `A4s-r6` design can treat the `bridge_KeResetEvent` duplicate as a **mechanical** obligation
(keep exactly one definition, and say which) rather than a semantic one, because the ordinal it serves
is not imported. The semantic decision concentrates on `KeSetEvent` (145) and
`KeWaitForSingleObject` (159), where the accepted contract test already discriminates the two models.
