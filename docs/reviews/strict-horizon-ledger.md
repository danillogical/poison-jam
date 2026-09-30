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
| 2026-09-30 | `5776aab` | `2a349c8` | `20260930-001405-390-v1-verified-strict` | same (kernel thunk table overwritten); re-verified on the binary built this session from the same revision pair | `0x0014982E` (tid 64532) | 07:14:15.588 |
| 2026-09-30 | `5fc6348` | `4ec3eca` | `20260930-034938-427-ttd-exit-control` | **NOT REACHED** — `diagnostic_deadline` at 11.98 s with **0 invalid ICALLs**; the guest was still live (1002 kernel calls, 6 threads, 0 `[UNIMPL]`, 0 exceptions, the same 10 data exports) | none | 10:49:50.4 |
| 2026-09-30 | `5fc6348` | `4ec3eca` | `20260930-053228-652-nonttd-196a29` | **NOT REACHED** — `diagnostic_deadline` at 25.04 s, 0 invalid ICALLs, 0 `[UNIMPL]`, 3226 log lines. Reaches and passes `0x00196967`/`0x00194520`/`0x00196A65`, the region where the TTD-recorded run dies at 577 lines | none | 12:32:53.7 || 2026-09-30 | `5fc6348` | `4ec3eca` | `20260930-053228-652-nonttd-196a29` | **NOT REACHED** — `diagnostic_deadline` at 25.04 s, 0 invalid ICALLs, 0 `[UNIMPL]`, 3226 log lines. Reaches and passes `0x00196967`/`0x00194520`/`0x00196A65`, the region where the TTD-recorded run dies at 577 lines | none | 12:32:53.7 |
| 2026-09-30 | `5fc6348` | `4ec3eca` | `20260930-053502-372-nonttd-90s` | **NOT REACHED** — `diagnostic_deadline` at 93.94 s, 0 invalid ICALLs, 0 `[UNIMPL]`, 4031 log lines. Reaches and passes `0x00196967`/`0x00194520`/`0x00196A65`, the region where the TTD-recorded run dies at 577 lines | none | 12:35:03.3 |

**The 2026-09-30 row is a strict run that did not reach the horizon, and it is recorded as
such.** It is the discriminating control for the TTD question: the same environment
(`RECOMP_GPU_ACK=0` only) that exits `0xC0000409` under TTD recording runs to its deadline
without it. Reading it as a horizon *move* would be exactly the error this ledger exists to
prevent — the horizon is an event, and no thunk call faulted here. What it establishes is the
comparison, not progress.

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
| `0x0014982E` (slot 65) | 3 | V3(c), `20260929-231451-253-rebaseline-strict-r2`, `20260929-231501-792-rebaseline-strict-r4` |
| `0x00147D36` (slot 68) | 1 | V3(a) |
| `0x00147DE2` (slot 71) | 1 | `20260929-231423-047-rebaseline-strict-repeat` |
| `0x00147DBC` (slot 70) | 1 | `20260929-231456-503-rebaseline-strict-r3` |

This spread is the race, not four horizons: every site is a call through a slot of the one overwritten
table.

**Every run in the table above is named by its full directory name**, so a reader can re-derive its
row from `logs/runs/<name>/` without guessing which file "r3" meant. `scripts/check-horizon-ledger.py`
(W14) enforces that: it fails when an archived STRICT run has no ledger line naming it, which is how
these four were found cited only by suffix.
