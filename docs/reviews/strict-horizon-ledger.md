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
| 2026-09-30 | `5fc6348` | `4ec3eca` | `20260930-053722-314-v3-repro-check` | **REPRODUCED** — the kernel thunk table overwritten and the next thunk call faults: `[ICALL] invalid target 0x00000000` `return=0014982E` (slot 65), `exit_code=0xE0424943`, 16442 log lines, 0 ABI failures, 1 `[EXCEPTION]` at that code | `0x0014982E` (tid 21432) | 12:37:32.1 |
| 2026-09-30 | `5fc6348` | `1572256` | `20260930-081601-393-read-dst-verify` | **REPRODUCED** — the same terminal site on a build carrying the toolkits new `[READ] dst=` field: `[ICALL] invalid target 0x00000000 return=0014982E`, `exit_code=0xE0424943`, 0 ABI failures. Recorded because W14 requires every archived STRICT run to have a row, and because it shows the observation-only toolkit change did not move the horizon | `0x0014982E` (tid 4612) | 08:16:10.6 |
| 2026-09-30 | `ef2c855` | `86113c7` | `20260930-221054-913-f0b-first-run-new-toolkit` | **REPRODUCED** — the first run on the pulled toolkit `b857665..86113c7` (D1, D2, D3, file I/O, DPC, timer period, mixdown, IRQL tracking). Same terminal site: `[ICALL] invalid target 0x00000000 return=0014982E`, `exit_code=0xE0424943`, 0 ABI failures, 0 `[UNIMPL]`, 17906 log lines. The new toolkit did **not** move the horizon. Strict, `RECOMP_APU_TRAP=1`, log budget 100000 | `0x0014982E` (tid 47532) | 2026-10-01 05:11:00.2 |
| 2026-09-30 | `ef2c855` | `86113c7` | `20260930-221404-630-f1b-rdata-guard` | **REPRODUCED, and the writer named** — `RECOMP_RDATA_GUARD=1` (ledger L32, observation only, still STRICT). The guard fired 256 times, its total cap, and every report is the same writer: `rip=exe+0x5361B8` = `sub_00038530+0x398`, a `rep movsd` image copy running upward from `0x00011000` (the reports stop at `0x0005000C`, where the cap was reached). Shifted byte provenance maps the copy's effect across the whole range: the dump at `V` equals the XBE at `V + 0x37608` for 431 of 436 sampled 4 KB pages, 0 original, including the table's own page. The terminal ICALL differs (`invalid target 0x00000001 return=00147CF8`) because the copy is still in progress when the first thunk call faults — same event, different first site. See the F1 finding below | `0x00147CF8` (tid 26240) | 2026-10-01 05:14:15.7 |

**The 2026-09-30 F1 row is the horizon's cause, not a new horizon.** The two rows above
are the same event: the kernel thunk table `0x001C3F60..0x001C4140` is overwritten by a
copy that carries the XBE's own `.data` content over it. What the second row adds is the
writer's identity and the mechanism, both measured — see `docs/jsrf-technical-record.md`
§5 and the compatibility ledger. The **horizon event is unchanged**; the *first site* that
faults differs between the two runs because the fault races the copy's progress.

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


## 2026-09-30: the horizon reproduces exactly, and it depends on `RECOMP_APU_TRAP`

The 90 s and 20 s runs above reached **no** horizon (0 invalid ICALLs, both at the
collector's deadline). Reproducing **V3's exact environment** instead —
`RECOMP_GPU_ACK=0 RECOMP_APU_TRAP=1 RECOMP_KERNEL_LOG_BUDGET=100000`, 8 s — reaches it
in **7.3 s**:

```
[ICALL] invalid target 0x00000000 tid=21432 esp=00F7FD00 return=0014982E
[KERNEL] #5173: ordinal 277 (slot 65) esp=0x00F7FD00 ret=0x0014982E
exit_code = 0xE0424943
```

`0x0014982E` is the **same return address** the 2026-09-29 ledger rows record, and
slot 65 is the same slot. **The horizon did not move; it was not being reached because
the run was missing `RECOMP_APU_TRAP=1`.**

`RECOMP_APU_TRAP` is classified in `docs/jsrf-run-profiles.md` as **feature
enablement — real capability, not a bypass**, so a run with it is still STRICT
(`check-run-profile.py` agrees: `STRICT`). It routes `0xFE800000..0xFE880000` to the
emulated APU. Without it the guest takes a different path and never reaches the thunk
table clobber within 90 s.

**Consequence.** The horizon framing is **unchanged and freshly reproduced on the
current binary** (`5fc6348` / `4ec3eca`, built this session). C1's target is still the
terminal slot write, and the "the horizon may have moved" reading in
`docs/reviews/ttd-recording-exit-finding.md` is **superseded by this row**: the
earlier runs simply lacked the APU trap.

**This is also the control the TTD question needs.** A traced run must be launched
with the same `RECOMP_APU_TRAP=1` if it is to reach the horizon at all — the two TTD
recordings were made with `RECOMP_GPU_ACK=0` only, which is now known to be a
non-horizon-reachable configuration. **The TTD exit may therefore have been
mis-attributed**: the traced run was not on the path that reaches the horizon in the
first place, so "TTD changes the guest's behaviour" and "the traced configuration
never reaches the horizon anyway" are not yet separated.
