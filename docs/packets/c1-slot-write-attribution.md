## C1 — discover which code writes the terminal slot value

**Class:** discovery   **Contract revision:** r1   **Status:** draft
**Starts from:** the strict terminal event, freshly reproduced on the current binary:
the kernel thunk table `0x001C3F60..0x001C413F` is overwritten and the next thunk
call faults. `docs/reviews/strict-horizon-ledger.md` row
`20260930-053722-314-v3-repro-check` (`exit_code=0xE0424943`, first site
`0x0014982E`, slot 65, 7.3 s).
**Baseline:** game `3e2f642`, toolkit `4ec3eca`
**Senior-call budget:** 1 Planner (self-review), 1 acceptance review (W13)

### Load-bearing premises (W10)

Each premise this packet rests on, with the byte-level command that establishes it.
The reviewer re-runs these first.

- **The terminal read is a call through slot 65 of the thunk table.** Command:
  `python -X utf8 scripts/inspect-jsrf.py disasm 0x00149820 0x00149834`.
  Establishes: `00149828 call dword ptr [0x1c4064]`, and the return address
  `0014982E` the failing call reports.
- **The horizon fires on the current binary.** Command:
  `python -X utf8 scripts/v3-evidence.py logs/runs/20260930-053722-314-v3-repro-check`.
  Establishes: `exit_code=0xE0424943` and `[ICALL] invalid target 0x00000000 …
  return=0014982E` at slot 65.
- **A trace exists that contains the terminal event.** Command:
  `python -X utf8 tools/ttd/ttd-terminal.py <trace>`.
  Establishes: the terminal sequence and the exception code, and it is the command
  that produces W11's position P.
- **The launch environment is the horizon-reachable one.** Command:
  `git log -1 --format=%H` plus the recipe; the environment is
  `RECOMP_GPU_ACK=0 RECOMP_APU_TRAP=1 RECOMP_KERNEL_LOG_BUDGET=100000`, and
  `RECOMP_APU_TRAP=1` is what makes the horizon reachable within 8 s
  (`docs/reviews/strict-horizon-ledger.md`).
- **`RECOMP_APU_TRAP` is feature enablement, so the run stays STRICT.** Command:
  `python -X utf8 scripts/check-run-profile.py logs/runs/20260930-053722-314-v3-repro-check`.
  Establishes: `STRICT`.

### Dry-run transcript (W3)

Every command in `### Experiments` that can run before the packet is frozen, executed
on this host and tree, with its output hashed. Commands that need the new trace
(`just ttd-record c1-r2`) are marked as not yet run, because that recording is the
packet's first step rather than a prerequisite.

| Command | Exit | Output sha256 (sha256 of stdout+stderr, UTF-8) |
|---|---|---|
| `python -X utf8 scripts/inspect-jsrf.py disasm 0x00149820 0x00149834` | 0 | `2fb9466e1a38462a626b0226c0c9ac3010cf77e2240f4999d5216aedec6fdf70` |
| `python -X utf8 scripts/check-run-profile.py logs/runs/20260930-053722-314-v3-repro-check` | 0 | `92b3ac7f0f79904d2ff67acc33beb6d36719dd80edafe36536e655c49584aa08` |
| `python -X utf8 tools/ttd/ttd-terminal.py logs/ttd/20260930-053811-458-ttd-aputrap/jsrf_recomp01.run --json` | 0 | `18eec12f54445a064c66097b1fa0d6e15a82984c8f1733c37e937bb8c760466f` |
| `python -X utf8 tools/ttd/ttd-query.py <trace> 0x001C4064 --terminal-sequence <P>` | 1 | NOT ADMITTED — S1, S5, S6 fail (output below) |
| `just ttd-record c1-r2` | — | **not yet run**; it is step 2 of `### Experiments` |

The three hashes are of the commands as run on this host and tree; a reader who
re-runs them and gets a different hash has a different tree or a different host, and
that difference is itself the finding.

**Verified-by:** every hex literal in this packet's tables was re-read against the
command output it came from, in this session, by the process that produced the dry run
above -- the `disasm` output for `0x00149820` / `0x00149828` / `0x0014982E`, the
`ttd-terminal` record for `0x001C4060` / `0x001C4064`, and the plan's V4 row for the
`0x001D5078` reference. `scripts/check-transcribed-values.py` reports the values it
cannot see a source for, and this line is the W5 remedy it asks for.

Measured output of the three that ran:

```
00149828 call     dword ptr [0x1c4064]
0014982E mov      byte ptr [ebp - 0x1d], 1
```

```
20260930-053722-314-v3-repro-check   STRICT
```

```
invalid ICALLs : 1 — target=0x00000000 return=0014982E tid=27080
exception      : tid=27080 code=0xE0424943
last kernel    : #5175 ordinal 294 (slot 64 = 0x001C4060) ret=00149F5D
trace end      : sequence 38090827
value at P     : UNAVAILABLE — the slot reads ????????, TTD recorded no memory there
```

```
W11 ADMISSION: NOT ADMITTED
  PASS  S2_terminal_in_trace  terminal event at sequence 38090827
  PASS  S3_coverage           29/29 aliases, none truncated, 28/28 views
  PASS  S4_last_before_P      29 alias(es) reported a last-write-before-P row
  FAIL  S1_full_mode          trace is 8192.0 MB against an 8192 MB cap
  FAIL  S5_controls           no mirror positive (W-c)
  UNKNOWN S6_value_consistency  P is known but the value at P was not read
```

The S1 and S6 failures are what step 2 and step 3 of `### Experiments` address; S5
(W-c) is W11's unresolved control and is carried as a limit rather than waived.

### Question

Which code writes the value the terminal read sees at slot 65 of the kernel thunk
table, and through which of the 29 linear aliases of that address?

The unknowns behind it:

- **U1.** Is the writer a guest store, a host store, or a kernel-mode write that TTD
  does not report? W11's exclusion (d) is unresolved and only a control settles it.
- **U2.** Does any write reach a **mirror** alias? `O-ALIAS` is selectable only if
  one does, and the mirror positive (W-c) has never fired on a real trace.
- **U3.** What is the value at P? TTD records only touched pages, and the slot's page
  was not recorded at the end position, so W-b cannot yet be evaluated.

### Experiments

1. **No new instrumentation.** The trace is
   `logs/ttd/20260930-053811-458-ttd-aputrap/jsrf_recomp01.run` (8192 MB, horizon
   reached). This packet reads it.
2. **Record one more trace with a raised cap**, so W11's S1 holds:

   ```
   just ttd-record c1-r2          # sets RECOMP_APU_TRAP=1, --max-file-mb 20480
   ```

   Profile: strict (`RECOMP_GPU_ACK=0 RECOMP_APU_TRAP=1
   RECOMP_KERNEL_LOG_BUDGET=100000`; `RECOMP_APU_TRAP` is feature enablement, so the
   run stays STRICT — `docs/jsrf-run-profiles.md`). Artifact: the `.run` file and its
   `record-contract.json`.

3. **Find P, then query all 29 aliases bounded by P:**

   ```
   just ttd-terminal "<trace>"
   just ttd-writes "<trace>" 0x001C4064 --terminal-sequence <P> --value-at-p <value>
   ```

   The query evaluates W11's S1–S8 and exits nonzero when any fails.

4. **Resolve each reported native IP** to a guest function with
   `build/Release/jsrf_recomp.map` and `config/xdk-symbols.json` (T2), so the row
   names a function rather than an address.

5. **The W-d control (U1).** Record a trace of a run that performs a known
   `NtReadFile` into a known guest buffer, and ask the query whether that write
   appears. It either does — retiring W11's exclusion (d) — or it does not, and the
   exclusion stays a stated limit that W-b catches.

### Outcomes

| ID | Observed | Means | Next packet |
|---|---|---|---|
| O-GUEST-FILL | a guest `rep stos`/`memset` lowered to a host call writes the slot, named by IP | the destination overlaps the table; a heap/allocator packet asks why | allocator packet |
| O-ALIAS | a write through a mirror alias (index > 0) | the allocation handed out an address above 64 MB; a heap packet | allocator packet |
| O-HOST | the writer is a toolkit function | a toolkit fix packet | toolkit fix |
| O-DATA-CALL | the slot holds a `0x001D5078`-style data pointer | the object overlap with `g_Device` from V4 | object-layout packet |
| O-UNATTRIBUTED | the value at P differs from every recorded write | an unrecorded writer: kernel, external, or a wide store (W11's W-b) | the W-d control above |
| O-READ-PATH | no write at any alias, and the value at P is zero | nothing clobbered the slot; the fault is in the read path | re-plan |
| O-UNKNOWN | none of the above, or any W11 condition fails | not decided | re-plan |

### Limits and stops

- **Does not establish:** any strict claim. A TTD trace is not an archived strict run
  (W11's S8); it may decide this attribution and nothing else. It cannot satisfy a
  boot, liveness or horizon criterion and cannot add a ledger line.
- **Does not establish** that the recompiled guest is correct. `O-READ-PATH` is a
  finding about this slot, not about the title.
- **Known limits carried from W11:** kernel-mode writes may not be reported
  (exclusion d, unresolved pending the W-d control); `--max-hits` bounds only the
  per-event display list, and the count and last-write rows are uncapped.
- **Stop if:** the fresh trace fails any W11 condition after the raised cap; or the
  W-d control shows a kernel write to the slot, which changes the question to the
  kernel path; or the query returns `O-UNATTRIBUTED` for every alias.
- **Closure:** no instrumentation is added, so nothing needs removing. Artifacts:
  the trace, its `record-contract.json`, the `ttd-terminal` record, the
  `ttd-query` record, and the symbol resolution.

### Why this is a discovery packet, not a change packet

It changes no behaviour and claims nothing works. Its output is "which code wrote
this value", which a later change packet cites as a pinned precondition. That is
§5.8's discovery class exactly, and the plan's C1 row already names it as discovery.

### Premise freshness (W2, run before this draft)

`scripts/qualify-premise.py` on the cited run:

```
--run 20260930-053722-314-v3-repro-check --failing-instruction 0x00149828
  PASS  disassembly        0x00149828 is `call dword ptr [0x1c4064]`, decoded from
                           0x00149428
  PASS  strict_and_fresh   the run classifies STRICT
```

The premise is a **fresh measurement on the current binary**, not an inherited one:
the horizon was re-reproduced this session at game `5fc6348` / toolkit `4ec3eca`, and
the earlier "the horizon may have moved" reading was refuted as a missing
`RECOMP_APU_TRAP=1` in the launch environment
(`docs/reviews/strict-horizon-ledger.md`).
