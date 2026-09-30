# Strict-horizon ledger

One line per session that produced a strict run (plan §3, W14). The **strict horizon** is where a
strict-profile run's guest execution ends; it is the project's progress metric, because a strict run is
the only run that can establish boot, device or liveness progress (`docs/jsrf-run-profiles.md`).

Rules this ledger follows:

- Values come from the run's own archived artifacts, not from transcription. `scripts/v3-evidence.py`
  extracts them; the run directory is named so every line can be re-derived.
- A session that runs a strict run and leaves no line here is a ledger failure (W14).
- **The horizon is an event, not an address.** Since 2026-09-29 the terminal event is the kernel thunk
  table being overwritten; which call faults first is a race, so the ledger records the event and the
  observed first sites separately (`docs/jsrf-technical-record.md` §5).

| Date | Game | Toolkit | Run ID | Horizon event | First stop site(s) | Stop (UTC) |
|---|---|---|---|---|---|---|
| 2026-09-29 | `44becd4` | `2a349c8` | `20260929-231110-868-rebaseline-strict` | Kernel thunk table `0x001C3F60..0x001C413F` overwritten by a 40-byte-stride record array; first thunk call after it faults | `0x00147D36` (tid 63012) | 06:11:16.686 |
| 2026-09-29 | `44becd4` | `2a349c8` | `20260929-231211-023-rebaseline-gmeter` | same | `0x0014982E` (tid 46508) | 06:12:16.688 |

## Prior horizon (superseded 2026-09-29)

Recorded for continuity; the framing, not the address, is what changed.

| Date | Game | Toolkit | Run ID | Horizon | Stop (UTC) |
|---|---|---|---|---|---|
| 2026-09-28 | `e73e495` | `2925f0b` | `20260928-185612-449-regen-v012-strict` | `call dword ptr [0x1C4064]` at `0x00149828` read `0` → `[ICALL] invalid target 0xE0424943` | (archived run; see `metadata.json`) |

The 2026-09-29 dumps hold the **same** record array at the **same** addresses as the 2026-09-28 dump, so
the lifter and kernel-memory changes did not move this horizon: the horizon was **reframed, not moved**.
The old address `0x00149828` still fires in 3 of the 6 strict runs taken that session.

## Observed first-site distribution (2026-09-29, six strict runs, one build)

| First stop site | Count | Runs |
|---|---|---|
| `0x0014982E` (slot 65) | 3 | V3(c), r2, r4 |
| `0x00147D36` (slot 68) | 1 | V3(a) |
| `0x00147DE2` (slot 71) | 1 | repeat |
| `0x00147DBC` (slot 70) | 1 | r3 |

This spread is the race, not four horizons: every site is a call through a slot of the one overwritten
table.
