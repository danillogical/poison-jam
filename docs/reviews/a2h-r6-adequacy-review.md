# A2h-r6 plan-adequacy review — verdict INADEQUATE (premise refuted)

**Packet:** `A2h-r6`, `docs/packets/a2h-writer-investigation.md`
**Reviewer role:** Planner (`docs/agent-workflow.md` §1)
**Reviewer child:** `955c0f9c-7875-48f3-b567-9edef0e1d024`
**Route (MEASURED from the child's own `request/header`):** `claude` /
`claude-opus-5-5` @ `high`
**Session:** `session-4f68ab6b-30b1-4014-8390-f3be6a924cee`
**Verdict:** **INADEQUATE — retire, do not revise.**
**Second opinion:** Persistent advisor child `5f2d7236-3823-4ba0-ad99-3270a1397399`
(`claude/claude-opus-5-5` @ `high`) independently reproduced the decisive facts
and **concurred**.
**Files changed by the reviewer:** none.

## Outcome

`A2h-r6` is **not implementable and not adequate**. Its central premise is
refuted by archived evidence, and the instrument it specifies cannot be placed
without violating the packet's own step-0/step-2 rules. The recommended
disposition is **retirement**, not a new revision: revising it would keep the
`A2h` identity attached to a question that no known failure supports.

No implementation was performed. No build was run. **No guest run was run.**

## Blocking defects

### B1 — The premise is refuted; the failure was already fixed elsewhere

The packet states (`docs/packets/a2h-writer-investigation.md:84-92`) that "the
structural claim is measured: the invalid target is the *slot value* read from
`[base + index*8 + 4]`" and builds its entire objective on observing the failing
`0x0017DC23` call.

MEASURED, refuting it — the failure came from the C++ static-initializer walker
`0x0014B50D`, via a *different* entry:

- `logs/runs/20260921-143340-028-run/stacks.txt:250-251` records two consecutive
  ICALL events:
  `EVENT 170 kind=1 target=0017E58F site=0014B52C esp=00F7FF44` then
  `EVENT 171 kind=1 target=00700010 site=0017E627 esp=00F7FF14`.
  The same pair appears in `20260921-154535-674-resume-check`.
- Dump integrity gate passed **first** (`scripts/check-dump-mapping.py`: 1 match,
  0 content-mismatch, 0 unreadable). Reading guest memory
  `0x00F7FEE0..0x00F80000` in that run gives
  `F7FF14=0017E627`, `F7FF18=001EB76C`, `F7FF1C=001EB764`, `F7FF44=0014B52C`,
  `F7FF48=00700010`. **Neither `0x0017DC28` nor `0x0017C238`** — the two return
  words the packet's own two-caller analysis predicts — appears in the frame.
  So neither enumerated caller dispatched it.
- `scripts/inspect-jsrf.py disasm 0x0014B50C 0x0014B540` shows the walker:
  `mov eax,0x1eb760 / mov edi,0x1eb76c / cmp / mov eax,[esi] / test eax,eax / je /
  cmp eax,-1 / je / call eax` at `0x0014B52A`, then `add esi,4`.
  Dump `0x001EB760`: `00000000 0017E58F 00181212 00000000` — slot
  `0x001EB764` holds `0x0017E58F`.
- `src/recomp/gen/recomp_dispatch.c:6865` maps
  `{ 0x0017E58Fu, (recomp_func_t)sub_0017E600 }`. The translation's
  `tail_jump_alias` fold redirected `0x0017E58F` onto the **three-argument**
  dispatcher `sub_0017E600`, which read `[ebp+8]=0x00700010` and did
  `call eax` at `0x0017E625` (`recomp_0004.c:6045`).
- The native unwind in the same `stacks.txt` confirms the chain:
  `recomp_icall_not_code_log+0xA0` (`src/recomp_manual.c:44`) ←
  `sub_0017E600+0x20C` (`recomp_0004.c:6044`) ←
  `sub_0014B50D+0x17F` (`recomp_0003.c:24660`) ← `sub_00147FB4` ←
  `sub_00147EBB` ← `bridge_PsCreateSystemThreadEx`.
- **Already fixed:** commit `cb7cae2` (2026-09-21 15:58:12, "Recover the static
  initializer the alias fold deleted"). `git merge-base --is-ancestor cb7cae2 HEAD`
  → true; and → true for `40ae5bd`, the packet's own recorded evidence revision.
- All **four** runs that ever logged the `0x00700010` ICALL line are from
  09-21 14:33, 14:34, 14:46 and 15:45 — **all predate `cb7cae2`**.
- The current tree routes `0x0017E58F` to its own body
  (`recovered.c:357370 case 0x0017E58Fu: return sub_0017E58F;`), consulted
  **before** the generated dispatch table
  (`recomp_types.h:869-870`: `_fn = recomp_lookup_manual(_va); if (!_fn) _fn = recomp_lookup(_va);`).
  The archived strict run's log line 443 reads
  `[RECOVERED] 0x0017E58F returned; ABI verified (ESP/EBX/ESI/EDI)` — the current
  tree passes the exact point that used to fail.

**Consequence:** `A2h-r6`'s PASS requires the trace's last line before a failing
`[ICALL] ... return=0017E627` to be the `0x0017DC23` call-site record. There is no
known failing instance at that site to observe, so **PASS is unsatisfiable by
construction, not merely unlikely.**

### B2 — The instrument cannot be placed within the packet's own rules

- The instrumented sites (`0x0017DC0D`, `0x0017DC1F`, `0x0017DC23`) are ordinary
  lifted statements **inside** the generated body of `sub_0017DBBD`
  (`src/recomp/gen/recomp_0004.c:4021`, statements at lines 4082-4094).
- That chunk's SHA-256 is pinned by **both**
  `docs/reviews/p0-7-generation-provenance.json` and
  `docs/reviews/p0-full-generated-baseline.json` (17 files).
- `check_manifest` (`scripts/check-generation-provenance.py:286-299`) compares the
  recorded manifest against **current** hashes, so any byte change fails it. The
  guard passes today (`ok : True`).
- The packet's step 0 forbids hand-editing or regenerating production generated
  chunks "merely to make the trace easy", and its step 2 requires the guard to
  pass again. **Both cannot hold at once.**
- No interior/basic-block trace hook exists. Enumerated emission points:
  `tools/recomp/translator.py:776-782` (entry) and `tools/recomp/lifter.py`
  lines 947, 1509-1515, 1878-1881, 1925-1932, 2087-2103 (exit / pop / after-call /
  tail-jump). `lift_basic_block` (`lifter.py:3170`) emits none.
  `scripts/relift-selected.py trace` rewrites **whole bodies** inside the
  protected file (lines 84-90).
- **Negative control (run by this session):** a temporary append to
  `recomp_0004.c` made the guard print
  `problem: generated output src/recomp/gen/recomp_0004.c changed since the manifest`
  and exit 1. The file was restored from a backup and its SHA-256 confirmed back
  to `5b8e7c218bb1b9e2da23fac35de78a3e6f56858d1bd0dcd337f8615126983412`.

> **Note on the guard's scope (reviewer's aside, verified):** `--check` compares
> the manifest against **live** hashes plus the baseline's **file list**; the
> baseline JSON's recorded *hash values* are not themselves compared. This does
> not create an opening here — the manifest's own recorded values are compared to
> live bytes — but it is recorded because it is a real property of the checker.

### B3 — The required last-N record cannot be produced

The packet requires a cap that "must retain the most recent N lines, not the
first N" (`a2h:180`). The existing trace is a **first-N countdown**:
`recomp_trace.c:29-38` (`return budget > 0 ? budget-- : 0;`), after which every
trace call returns early. The PASS predicate is decidable as written, but nothing
in the current toolchain can produce the record it depends on.

### B4 — An entry trace is a different instrument and still incomplete

INFERRED from MEASURED code. `sub_0017DBBD` **loops** (next index loaded from
`[entry]`; `recomp_0004.c:4079`, `4108-4110`), so a function-entry trace yields the
descriptor, count and base but only the **initial** index and never the slot. A
`sub_0017E600` entry trace yields the call site (`from=0017DC28` vs `0017C238`)
and arg1 = the slot, but not the descriptor or current index. Combining both
would still require a relift, so B2 applies. A revision using this approach would
have to rename the instrument and redefine PASS around two entry records.

### B5 — The literal `0x00700010` is misattributed

The packet attributes the value to the A2f/A2g runs (`a2h:149-154`, `336-344`).
MEASURED: `logs/runs/20260922-190336-778-a2f-7e255-span/jsrf_run.log` contains
**zero** occurrences of `invalid target 0x00700010` and zero of
`return=0017E627`; likewise `20260922-224429-003-a2g-304f0-span`. A2f's only
`00700010` occurrence is an unrelated `[RECOVERED] ABI FAILURE 0x000304F0` line at
L16008. The real sources are the four 09-21 runs listed in B1. The
**exploratory** classification of A2f/A2g is itself correct (its `metadata.json`
`settings` confirm `RECOMP_AC97_READY=1`, `RECOMP_APU_DSP_ACK=0x803C0810`,
`RECOMP_APU_TRAP=1`, `RECOMP_KERNEL_LOG_BUDGET=100000`, no `RECOMP_GPU_ACK`) — only
the attribution of the number is wrong. Blocking **together with B1**, because the
misattribution concealed that the evidence is stale and pre-fix.

### B6 — The authorized single run would almost certainly return UNKNOWN

MEASURED and confirmed independently: `logs/runs/20260923-013448-357-p0-strict-baseline`
is `normal_exit` / `exit_code 0` / `duration_seconds 1.922415`; its only match for
the site patterns is `L53 [CHECKPOINT] ... guest_entry`; the tail is
`[KERNEL] HalReturnToFirmware: routine=2 - title is exiting`. The packet itself
concedes row 5 (`UNKNOWN`) is the likely outcome. The advisor's independent
position is that planning a one-shot measurement whose predicted row routes
elsewhere is a planning defect; the reviewer agrees it is a defect, but **given
B1 does not recommend inserting a reachability packet for `0x0017DBBD`**, because
no evidence implicates that site.

## Advisory defects

1. The packet enumerates **two** callers into `sub_0017E600` sharing return
   `0x0017E627`. Both are real (`recomp_0004.c:4094` pushes `0x0017DC28`;
   `recomp_0003.c:88127` pushes `0x0017C238`). It **misses the third route** —
   the dispatch alias `recomp_dispatch.c:6865` — which is the one that actually
   failed.
2. The `0x00700010` section-membership check is confirmed:
   `inspect-jsrf.py disasm 0x00700010 0x00700020` → `Inspection failed: range is
   not contained in one file-backed XBE section`.
3. The "Attempts so far: 2" block is numerically correct (48 vs 0 `ABI verified`;
   `guest_entry` at log line 48 in both; 200 vs 157 numbered kernel calls; thread
   calls 379; both `exit_code 3762440515` = `0xE0424943`). Calling the second run
   "post-alias-fix" is **misleading**: it predates the initializer fix.
4. Decision-table gaps: an index ≥ count (`0x17E3CB` path) has no row; an
   unreadable descriptor has no row; row 4's "valid in-image address" is not the
   runtime's test (`RECOMP_ICALL_IS_CODE`, `recomp_types.h:892`), so a slot
   pointing into data would log `invalid target` yet satisfy row 4; row 3 is
   unselectable under PASS and is a diagnostic, not a row.
5. The §5 criterion chain is incomplete: **no stable criterion IDs**; the
   heap-membership helper is never named; the required map/PDB instrument symbol
   cannot exist for an inline trace; "trace never fires" is assigned both to row 5
   and to positive-control failure; the checker has no known-good/known-bad
   controls; the trace artifact path and format are unspecified.
6. The counterexample review's answer to "could stale evidence satisfy it?" is
   **wrong** as written: stale, pre-fix exploratory evidence is precisely what
   produced the packet's premise.

## Minimal repair

**Retire `A2h-r6`.** Do not run it and do not revise it. Mark it
superseded/refuted in the `CURRENT PACKET` block of `plan-jsrf-bare-minimum.md`,
citing `cb7cae2`, ICALL events 170/171, and the stack words at `F7FF44`/`F7FF48`.

Then write a **new** packet starting from the current strict stop — the title
exits through `HalReturnToFirmware(2)` at 1.92 s. Record
"`sub_0017DBBD` execution status: UNKNOWN (uninstrumented)" as an open note, not
a packet; revive it only if a future failure's unwind passes through it.

If a later packet genuinely needs to observe inside a generated function, it
first requires a **separate tooling prerequisite packet** providing:
- a trace seam outside the guarded generated files, or a declared, reversible
  diagnostic change the provenance guard recognises;
- a trace mode that retains the **last** N records;
- known-good and known-bad controls exercised on a fixture.

**Prefer the collector's call history plus the frozen dump first.** They
distinguish call sites with **no code change** — that is exactly how B1 was
established.

## What PASS would still not establish

Even a passing run would show only the values that arrived at one invocation. It
would not say who wrote the slot or why, nor that any fix is correct, nor decide
between a translation error and bad guest data. It proves nothing about strict
boot, liveness, or reachability beyond that one run. And per B1 it would concern a
failure that no longer occurs in the current tree.

## Verification steps requested by the advisor, and their results

The advisor asked for five checks before acting on its ruling. Results:

1. **Retirement record cites run IDs, `stacks.txt` lines, dump-mapping result,
   `cb7cae2` ancestry and strict log line 443.** — Done, above (B1).
2. **Confirm the strict baseline's source identity is at or after `cb7cae2`.**
   — MEASURED by this session: the archive's `metadata.json` records
   `recovered.c` = `3210117dccf0a909cf3f7f881919001d976e14922aea6d1025312df5b1f22fe9`
   and `recomp_0004.c` = `5b8e7c218bb1b9e2da23fac35de78a3e6f56858d1bd0dcd337f8615126983412`,
   **identical** to the current tree's files. The strict baseline was therefore
   built from the current, post-`cb7cae2` source. Confirmed.
3. **Confirm `recomp_lookup_manual` is consulted before the generated dispatch
   table.** — MEASURED: `recomp_types.h:869-870` and `:896-897` and `:915-916` all
   call `recomp_lookup_manual(_va)` first and fall back to `recomp_lookup(_va)`
   only if it returns null. `recomp_manual.c:11-16` forwards to
   `jsrf_lookup_recovered`, whose table contains the `0x0017E58F` case. Confirmed.
4. **The next packet's premise must cite the current strict exit path, not any
   09-21 exploratory evidence.** — Recorded as a requirement in "Minimal repair".
5. **Adequacy review of the new packet must check it does not inherit A2h
   wording.** — Recorded as a requirement; the new packet does not yet exist.

## Self-correction carried into this record

This session initially claimed `sub_0017DBBD` had "never been observed executing"
on the basis that zero of 651 archived logs mention it. That inference was
**withdrawn**: `sub_0017DBBD` carries no trace hook, is not in
`config/trace-functions.json`, and is not a recovered wrapper, so its execution
emits no log line — absence of a log line is not a coverage witness here. The
retirement case above rests instead on the three artifact-backed records in B1
(ICALL events, guest stack words, native unwind), which do not share that
weakness. `sub_0017DBBD`'s execution status remains **UNKNOWN**.
