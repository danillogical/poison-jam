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
- **Stop (UTC) convention, prospective from 2026-10-01 (Advisor fault-diagnosis ruling §D):** the
  Stop column is `started_utc + duration`, i.e. **`metadata.json`'s `started_utc` plus
  `result.json`'s `duration_seconds`** — the two fields live in different files (Session
  clarification, 2026-10-01; the Advisor's §D wording says "from metadata.json", which is shorthand
  and is left as quoted). Rows written before this convention are **not rewritten**; the last MOVED
  row's `05:57:40.3` stands as the recorded baseline, about 92 s conservative against
  `start + duration` (immaterial).

| Date | Game | Toolkit | Run ID | Horizon event | First stop site(s) | Stop (UTC) |
|---|---|---|---|---|---|---|
| 2026-09-29 | `44becd4` | `2a349c8` | `20260929-231110-868-rebaseline-strict` | Kernel thunk table `0x001C3F60..0x001C413F` overwritten by a 40-byte-stride record array; first thunk call after it faults | `0x00147D36` (tid 63012) | 06:11:16.686 |
| 2026-09-29 | `44becd4` | `2a349c8` | `20260929-231211-023-rebaseline-gmeter` | same | `0x0014982E` (tid 46508) | 06:12:16.688 |
| 2026-09-30 | `5776aab` | `2a349c8` | `20260930-001405-390-v1-verified-strict` | same (kernel thunk table overwritten); re-verified on the binary built this session from the same revision pair | `0x0014982E` (tid 64532) | 07:14:15.588 |
| 2026-09-30 | `5fc6348` | `4ec3eca` | `20260930-034938-427-ttd-exit-control` | **NOT REACHED** — `diagnostic_deadline` at 11.98 s with **0 invalid ICALLs**; the guest was still live (1002 kernel calls, 6 threads, 0 `[UNIMPL]`, 0 exceptions, the same 10 data exports) | none | 10:49:50.4 |
| 2026-09-30 | `5fc6348` | `4ec3eca` | `20260930-053228-652-nonttd-196a29` | **NOT REACHED** — `diagnostic_deadline` at 25.04 s, 0 invalid ICALLs, 0 `[UNIMPL]`, 3226 log lines. Reaches and passes `0x00196967`/`0x00194520`/`0x00196A65`, the region where the TTD-recorded run dies at 577 lines | none | 12:32:53.7 |
| 2026-09-30 | `5fc6348` | `4ec3eca` | `20260930-053502-372-nonttd-90s` | **NOT REACHED** — `diagnostic_deadline` at 93.94 s, 0 invalid ICALLs, 0 `[UNIMPL]`, 4031 log lines. Reaches and passes `0x00196967`/`0x00194520`/`0x00196A65`, the region where the TTD-recorded run dies at 577 lines | none | 12:35:03.3 |
| 2026-09-30 | `5fc6348` | `4ec3eca` | `20260930-053722-314-v3-repro-check` | **REPRODUCED** — the kernel thunk table overwritten and the next thunk call faults: `[ICALL] invalid target 0x00000000` `return=0014982E` (slot 65), `exit_code=0xE0424943`, 16442 log lines, 0 ABI failures, 1 `[EXCEPTION]` at that code | `0x0014982E` (tid 21432) | 12:37:32.1 |
| 2026-09-30 | `5fc6348` | `1572256` | `20260930-081601-393-read-dst-verify` | **REPRODUCED** — the same terminal site on a build carrying the toolkits new `[READ] dst=` field: `[ICALL] invalid target 0x00000000 return=0014982E`, `exit_code=0xE0424943`, 0 ABI failures. Recorded because W14 requires every archived STRICT run to have a row, and because it shows the observation-only toolkit change did not move the horizon | `0x0014982E` (tid 4612) | 08:16:10.6 |
| 2026-09-30 | `ef2c855` | `86113c7` | `20260930-221054-913-f0b-first-run-new-toolkit` | **REPRODUCED** — the first run on the pulled toolkit `b857665..86113c7` (D1, D2, D3, file I/O, DPC, timer period, mixdown, IRQL tracking). Same terminal site: `[ICALL] invalid target 0x00000000 return=0014982E`, `exit_code=0xE0424943`, 0 ABI failures, 0 `[UNIMPL]`, 17906 log lines. The new toolkit did **not** move the horizon. Strict, `RECOMP_APU_TRAP=1`, log budget 100000 | `0x0014982E` (tid 47532) | 2026-10-01 05:11:00.2 |
| 2026-09-30 | `ef2c855` | `86113c7` | `20260930-221404-630-f1b-rdata-guard` | **REPRODUCED, and the writer named** — `RECOMP_RDATA_GUARD=1` (ledger L32, observation only, still STRICT). The guard fired 256 times, its total cap, and every report is the same writer: `rip=exe+0x5361B8` = `sub_00038530+0x398`, a `rep movsd` image copy running upward from `0x00011000` (the reports stop at `0x0005000C`, where the cap was reached). Shifted byte provenance maps the copy's effect across the whole range: the dump at `V` equals the XBE at `V + 0x37608` for 431 of 436 sampled 4 KB pages, 0 original, including the table's own page. The terminal ICALL differs (`invalid target 0x00000001 return=00147CF8`) because the copy is still in progress when the first thunk call faults — same event, different first site. See the F1 finding below | `0x00147CF8` (tid 26240) | 2026-10-01 05:14:15.7 |
| 2026-09-30 | `ef2c855` | `86113c7` | `20260930-223608-115-f4-frames-pb-exec-fb-window` | **NOT REACHED, 62 s** — EXPLORATORY (`RECOMP_GPU_ACK` default-on plus `RECOMP_PB_EXEC=1`, `RECOMP_FB_WINDOW=1`; ledger L16, L18), so this row is **not** a strict horizon claim and is recorded as a contrast, not as progress. `diagnostic_deadline` at 62.3 s, **0 invalid ICALLs, 0 `[EXCEPTION]`, 0 ABI failures, 0 `[UNIMPL]`**, 138,597 log lines, 45,506 kernel calls. **No `[ALIAS-ICALL] … 00037550` line appears**, where both strict runs of that build have one. **This is not a single-variable comparison** (`docs/jsrf-run-profiles.md`): this run differs from the strict pair in three settings at once (GPU_ACK, PB_EXEC, FB_WINDOW), so it does not establish that any one of them prevents the alias. It is recorded because it is the only run of that build that reaches 62 s without the clobber, and because it bounds where the F3 fix had to be tested — on the strict path. No frames were produced (`FLIP`/`present`/`FB_DUMP` all 0), so F4 is **not** satisfied | none | 2026-10-01 05:36:08 |
| 2026-09-30 | `ef2c855` | `86113c7` | `20260930-225440-580-f3-alias-fix-strict` | **MOVED** — first run after the `0x00037550` recovery entry. The kernel thunk table is **no longer overwritten**: `invalid target` 0, `0xE0424943` 0. The run ends at 14.6 s on `exit_code=0xC0000409` with `[RECOVERED] ABI FAILURE 0x00026780` — the **next entry in the same function-pointer table** (`.data 0x001EC10C`, where `0x00037550` sits at `0x001EC108`), same defect shape. Strict, `RECOMP_APU_TRAP=1`, budget 100000, 77,608 log lines | `0x00026780` (ABI) | 2026-10-01 05:54:41.1 |
| 2026-09-30 | `ef2c855` | `86113c7` | `20260930-225739-446-f3-alias-fix-2-strict` | **MOVED, and the clobber is gone** — after also retargeting `0x00026780` to its own code (ending at its jump table `0x0002730C`). `diagnostic_deadline` at **93.0 s** (against ~6 s before), **0** invalid ICALLs, **0** `0xE0424943`, **0** `[EXCEPTION]`, **0** ABI failures, **0** `[UNIMPL]`, 487,394 log lines. `check-dump-mapping.py` reports **`matches: 1, content-mismatch: 0`** (it was `CONTENT_MISMATCH` in every prior run): `.text` at `0x00011000` is byte-identical to the XBE, and the thunk table at `0x001C3F60` holds the runtime's installed `0xFE000000+` thunks instead of the record array. **The A2h/C1 terminal event is closed.** Strict, `RECOMP_APU_TRAP=1`, budget 100000 | none (deadline) | 2026-10-01 05:57:40.3 |
| 2026-09-30 | `de02df3` | `1f9309a` | `20261001-004608-186-f4-smoke-1720-admitted` | **NOT REACHED — EXPLORATORY, recorded as a contrast, not as a strict claim** (`RECOMP_GPU_ACK` default-on plus `RECOMP_PB_EXEC=1`, `RECOMP_FB_WINDOW=1`). The F4 `0x1720` smoke: 47.3 s, `diagnostic_deadline`, **0** invalid ICALLs, **0** exceptions, **0** ABI failures, **0** `[UNIMPL]`. **The walk's diagnostic changed and its address did not:** `unsupported_method` is gone (0, was 54) and every submission from #12 reports `diag=sink_capacity get=00008EF0` while `put` advances to `0x47A84`. Cause: that one submission (`0x8EF0..0xA440`, 259 packets) stages **1109 methods** against a per-submission sink of **1024**, overflowing within the submission at packet #239. No frames (`FLIP`/`present`/`FB_DUMP` 0) | none (deadline) | 2026-10-01 05:46:09.2 |
| 2026-10-01 | `3be0adb` (archived record-only citation diff) | `e8a6e03` | `20261001-020407-358-f4-capacity-fix-smoke` | **EXPLORATORY contrast, no strict horizon move**. Default GPU_ACK, PB_EXEC=1, FB_WINDOW=1, APU_TRAP=1, log budget 100000; active ledger L14–L18, L20–L25, L39, L40 (L19 dormant). **[Added 2026-10-01: on this build those IDs were *set but inert* — the owner clears `g_nv2a_ack_enabled`; see the `20261001-023335-656` row below. Classification only; this row's Stop predates the prospective convention and is not rewritten.]** `diagnostic_deadline`, 48.336403 s; dump/profile/checkpoints good. Logged submissions #0–63 all `diag=ok`, GET=PUT through `0x47A84`, beyond old `0x8EF0`; final frozen GET=PUT `0x16648`. No sink/budget diagnostic. **Draw/clear/flip/present UNKNOWN, not zero** (corrected 2026-10-01 by the Advisor fault-diagnosis ruling: the only `[GPU]` executor lines are printed during `xbox_MemoryLayoutInit`, before the guest ran, and `RECOMP_NV2A_TRACE` was unset, so this capture has no end-of-run counters). F4 frames remain unsatisfied. Advisor W14 CONTINUE until 11:00 UTC or two more smokes, whichever first; next is ONE observation-only run with `RECOMP_NV2A_TRACE=1`/`RECOMP_PB_SCAN=1` | deadline, no observed rejected submission | 2026-10-01 09:04:55 |
| 2026-10-01 | `c01e292` | `e8a6e03` | `20261001-023335-656-f4-observation-60s` | **NOMOVE — EXPLORATORY observation run; no strict claim and no horizon move.** Requested 60 s, `diagnostic_deadline`, 62.580312 s, exit 3; dump/checkpoints/`gpu_report_ok` all good, 21 threads, 165 named frames, 1 snapshot. Run with `RECOMP_NV2A_TRACE=1` and `RECOMP_PB_SCAN=1` on top of the smoke's exploratory set; `exe_sha256 1ecf8363…`, byte-identical to the capacity smoke's binary. **The instrumentation was inert:** `main.c:529` installs the MMIO state owner, `nv2a_mmio_hook.c:681` calls `xbox_Nv2aClaimRegisterOwner()`, `xbox_memory_layout.c:173-179` clears `g_nv2a_ack_enabled`, and the legacy GPU body `:1048-1177` (acks, DMA_GET mirroring, pushbuffer scan, executor call, periodic report) is inside that check — so L16/L18 are **set but inert** on this build. MEASURED: 64 `[PFIFO] submit` lines all `diag=ok`, GET=PUT through `0x47A84`; six `[GPU]` lines all at log 32-38, i.e. **before** `guest_entry` (line 75); **zero** `[PB]` lines and nothing after `guest_entry` matching draw/flip/pixel/surface. **Runtime counters are UNKNOWN for this capture** (no end-of-run report path), not measured zero; that the executor never runs under the owner is **INFERRED from the code path**, not observed. Stop derived from `metadata.json` `started_utc` 09:33:36.447445 + `result.json` `duration_seconds` 62.580312. Advisor ruling A: fix must reach NV097→`PB_EXEC` under the owner with no second walk/GET, keep rejection atomic, pin clear/flip counts in fixtures, and prove a post-guest periodic `[GPU]` report; W14 extended until A's first smoke plus one diagnostic; **no more inert reruns** | deadline; no render evidence either way | 2026-10-01 09:34:39.027757 |

| 2026-10-01 | `f676b1c` | `a71f937` | `20261001-033805-242-f5-sequence-180s` | **NOMOVE — EXPLORATORY observation run; no strict claim and no horizon move.** Same pair and profile as the 60 s F4 run, plus the observation `RECOMP_FB_DUMP` (prefix `logs/workers/f4a-sequence/f`). `diagnostic_deadline`, 182.688193 s, exit 3; dump and GPU report good, 21 threads, 179 named frames; **exploratory** (requested exploratory, `GPU_ACK` default). Mapping gate **1 match / 0 mismatch**. **25 BMP dumps, 2 distinct hashes in ordered blocks: `f000–f007` black (byte-identical), `f008–f024` SEGA (byte-identical)** — within those files there is no alternation (a statement about the dumped files, not a global no-animation claim). The 8-then-17 split is **attributed (INFERRED, not directly logged)** to the two writers in source (executor draw path `nv2a_pb_exec.c:2206-2209`, cap `FB_DUMP_AFTER_DRAW`=8 at `:2000`, mtimes 10:38:09–10:38:10 OBSERVED; periodic report `:3124`, 10:38:20→10:41:01 at 10 s, OBSERVED). One-to-one BMP↔report pairing is **INFERRED from code+timestamps, not measured** (no per-dump line; 19 reports for 25 files). Pixels are a **guest DRAW surface** via `dma_resolve(drawn_offset ? drawn_offset : color_offset)` + clip/pitch (`:937-940`), **not** the window/`AVSetPCRTC`; the once-only notice prints `color_offset`, unreliable as a file's source. A **third artifact** — extensionless `…\f` (sha256 `9E7541A8ABC68B31FAA3F4553867DCD70A7364EB5FC35B60B7055344E5EE7401`, mtime 10:38:19) — comes from `xbox_FramebufferDumpBmp` (`src/video/fb_present.c:212`, guard `:219-220`, one-shot at `frames == 600` `:341-342`, prefix raw); the F5 ruling accepts it as **one-shot SEGA at that instant only**, with nothing claimed about the window after. Present path remains **INFERRED**. Counts are **LOWER BOUNDS, per run and NOT to be mixed**: this 180 s run's last report is `L205921–205927` (clears 2943, draws 2943, flips 987, stalls 987, 14 skips) with its own `[GPU] clear #3000` at **L207880** of 209257; the **60 s** run's last report is `L115261–115267` (clears 840, draws 840, flips 286, stalls 286) with `clear #900` at **L117696** of 119913. Ledger IDs L14, L15, L16 (legacy, inert under the owner), L17, L18 (owner consumer, active), L20–L25, L39, L40; L19 dormant. No F5 causal ruling and no W14 change from this run | deadline; no strict horizon event | 2026-10-01 10:41:08.636886 |

| 2026-10-01 | `c4bcd2b` | `a71f937` | `20261001-043629-961-f5-observe-600s-c4bcd2b-retry1` | **NOMOVE — EXPLORATORY observation run; no strict claim and no horizon move.** Second attempt after the invalid truncated one; parent-owned managed job `pwsh-2573`. Same **pushed pair authorized for the replacement** (prior runs used different game commits), fresh empty root, no seed, `exe_sha256 7027fafa…b303` (unchanged, no build change). `diagnostic_deadline`, exit 3, `dump_ok` true, 21 threads, **166 named frames**, 1 snapshot, 0 dropped, `save_root_verified` true, `missing_checkpoints []`, `checkpoints_passed` true, `gpu_report_ok` true; **exploratory**. **Stop (UTC) = `metadata.started_utc` 11:36:30.743170 + `result.duration_seconds` 603.170746 = 11:46:33.913916** (metadata-derived, not the script's `11:36:29Z` launch line). Helper hashes: census `55E10B51…`, window watcher `0801459D…`, launch script `73B18640…`. **Exact typed counters (final row, `gpu-reports-v2.csv`):** `clears` 10500, `draws` 10500, `with_coordinates` 10500, `indices` 52482, `flips` 3506, `flip_stalls` 3506, `unhandled_methods` 1178547, `distinct_unhandled` 242, `non_NV097_skipped` 14. **Counts rise across the 61 blocks while the images stay static SEGA by eye — rising counts are not new images; still not the title.** All **19 tables `distinct_mtimes = 1`** over 10 s samples ("not observed to change"); only the **nine zero-byte `.CMP` markers** change. **Exact padding fact** (`logs/workers/f5-retry1/table-prefix-test-utf8.txt`): **19/19 save-root tables = DVD bytes + zero padding rounded to a 512-byte sector**. `Cache09`/`CMP09` absent on both sides — an observed consistency, not an expected shape. **No new run and no seed until the CMP/table check loop is explained** (no-rerun ruling). Ledger IDs L14, L15, L16 (legacy ack body retired; feed replaced by the consumer), L17, L18 (owner consumer, active), L20–L25, L39, L40; L19 dormant. Outer shell job `1` vs WRAP/result `3` — **statuses only, no cause claimed** | deadline; no strict horizon event | 2026-10-01 11:46:33.913916 |

| 2026-10-02 | `fd435ac` | `348a0e3` | `20261002-014152-190-owner-d2-600` | **NOT REACHED — EXPLORATORY owner observation**, requested 600 s, `diagnostic_deadline` / code 3 at 603.032515 s; fresh root verified. Unnumbered CMP marker mtime 08:45:14.621172Z, 401.241961 s before stop. Window captures 008–066 (59) over 583.185910 s all SHA `905e9204…15cd9f`, final image still Presented by SEGA. No observed SEGA exit, no strict-horizon move or liveness claim. Ledger IDs L14–L18, L20–L25, L39, L40 (L16 configured/inert, L19 dormant); always-active support IDs and qualifications in `docs/reviews/owner-sega-600-observations.md`. W14 bounds explicitly owner-waived | none (deadline; SEGA image) | 2026-10-02 08:51:55.863133 |

**F4 — MET, exploratory (2026-10-01).** Frames exist: the 60 s run `20261001-033129-276-f4-a-smoke-60s`
produced a coherent guest image (the "Presented by SEGA" card), so **F4 is met under the exploratory
profile**. This is **not** a milestone acceptance — it is carried with the **F6 milestone Review**.
**W14 was reset at 10:32:32 UTC** on the first critical-path frame finding; the earlier extension is
consumed and closed. Counters are lower bounds, not totals, and no strict claim is made.

**Global qualification on every zero/absence counter claim in this ledger (added 2026-10-01).**
Rows written before 2026-10-01 record statements such as "no frames (`FLIP`/`present`/`FB_DUMP` 0)"
and "0 draw/flip/present". Those figures were **not read from a runtime end-of-run report** on the
runs in question: with the MMIO state owner installed, the legacy GPU body that prints the periodic
and end-of-run `[GPU]` report is inert (`xbox_memory_layout.c:1048-1177` inside the
`g_nv2a_ack_enabled` check cleared at `:173-179`). MEASURED for two of the affected exploratory rows
(`20260930-223608-115-f4-frames-pb-exec-fb-window` and `20261001-004608-186-f4-smoke-1720-admitted`):
every `[GPU]` line in their logs lies before `guest_entry`, and neither log contains a `FLIP` or
`FB_DUMP` line at all. So those zeros are **initialisation-time values, not runtime measurements**,
and are **UNKNOWN** as runtime counters. Rows are left as written (historical record); this
qualification applies to all of them. **Scope limit:** this qualification is stated as a code-path
inference for the owner era plus the two runs measured above — it is **not** a claim that the gate
was observed closed in every earlier run, and the strict rows' *horizon* evidence (`[ICALL]`,
`exit_code`, thunk-table state) is unaffected, being a different mechanism. The capacity result
itself stands on the MEASURED committed-method evidence (64 `[PFIFO] submit` lines, `diag=ok`,
GET=PUT through `0x47A84`), which does not depend on any counter.

**`20260930-221054-913-f0b-first-run-new-toolkit` and `20260930-221404-630-f1b-rdata-guard` are
the same event, not two horizons.** In both, the kernel thunk table `0x001C3F60..0x001C4140` is
overwritten by a copy that carries the XBE's own `.data` content over it. What the second run adds
is the writer's identity and the mechanism, both measured — see `docs/jsrf-technical-record.md` §5
and the compatibility ledger. The **horizon event is unchanged** between them; the *first site* that
faults differs (`0x0014982E` vs `0x00147CF8`) because the fault races the copy's progress.

**`20260930-034938-427-ttd-exit-control` and `20260930-053228-652-nonttd-196a29` are strict runs that
did not reach the horizon, and they are recorded as such.** The first is the discriminating control
for the TTD question: the same environment (`RECOMP_GPU_ACK=0` only) that exits `0xC0000409` under
TTD recording runs to its deadline without it. Reading either as a horizon *move* would be exactly
the error this ledger exists to prevent — the horizon is an event, and no thunk call faulted in
them. What they establish is the comparison, not progress.

**`20260930-225440-580-f3-alias-fix-strict` and `20260930-225739-446-f3-alias-fix-2-strict` are the
two rows that actually moved the horizon, and they moved it by removing the event.** The first fixed
the `0x00037550` fold and the table stopped being clobbered, so its terminal fault is a *different*
event (an ABI failure at the next slot, `0x00026780`). The second fixed that fold too, and the run
reached its 93-second deadline with the table intact and `.text` undamaged. The horizon is therefore
**closed**, not relocated: there is no longer a terminal event to record, and the next stop is
whatever the deadline-bounded run reaches next.

**Which rows count as progress.** A row is a horizon **move** only if it says `MOVED`; `REPRODUCED`
means the same event was observed again, and `NOT REACHED` means no event occurred. The checker reads
this distinction (`scripts/check-horizon-ledger.py`), and the superseded table below is excluded from
its calculation.

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
