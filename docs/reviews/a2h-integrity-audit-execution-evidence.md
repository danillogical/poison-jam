# `A2h-integrity-audit-r1` — execution evidence: **`A-TAINTED`** (one accepted row consumed a specifically-gated failing-run dump read)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-integrity-audit.md`, revision **`A2h-integrity-audit-r1`**, frozen
**`03F53E0AF09283EBD2B14886DC3FA625C25402875107D0759537A7C9D1B242F9`** (17 lines, 6116 bytes; per
`docs/reviews/a2h-integrity-audit-r1-session-validation.md:17`). The packet was **not** edited by this executor.
**Tree state at execution:** game `29e6e8c1f2457c21c319a482199d62c9d319a66c`, clean working tree.
**Authority:** `docs/reviews/a2h-callback-slot-identity-advisor-ruling.md:18` (point 2) — *"Bounded audit PACKET
first … dependency, not hygiene."*
**Method:** read-only. **No game run, no build, no tests, no instrumentation, no source edit.** All 24 archived
A2h run directories were gated with the primary command; no dump was repaired, shifted, or inferred.

---

## Audit row

> **`A-TAINTED`.**

**One accepted A2h row's affirmative decision claim consumed a guest-memory read from a run whose dump fails the
integrity gate, and I checked that exact run myself.** The row is `A2h-named-producer-frame-r1` (`O-OPEN` label;
its accepted record's frame-identity claim was independently verified by the stage-1 reviewer against a
**`CONTENT_MISMATCH`** dump of `20260927-160330-655-a4b2-gp-trap-trace`).

**Identity and control gaps did not block closure:** the inventory reconciles to **7 accepted packets** against an
independently authored cross-check (`docs/reviews/a2h-integrity-audit-session-crosscheck.md`), every guest-memory
input resolved to an exact archived run directory, and **every extraction control named in packet lines 8–10
passed**. No `A-IDENTITY` or `A-OPEN` condition was found. Per packet line 14, the taint therefore governs, and a
**separate remediation decision is required before the queued caller-trace** (packet line 16).

---

## 1. Inventory (packet line 4)

Enumerated from three primary sources: `plan-jsrf-bare-minimum.md`, **all** `docs/reviews/a2h-*-acceptance-*.md`
(including stage-two and records) and **all** `docs/packets/a2h-*.md` — 72 review files and 14 packet files —
then reconciled against **final disposition lines read from each acceptance artifact itself**, not from the plan's
summary. Repeated plan history for the same revision was deduplicated to one row per revision.

### 1.1 Accepted universe — 7 packets

| # | Packet / revision | Final disposition | Accepted row | Acceptance artifact |
|---|---|---|---|---|
| 1 | `A2h-oom-causal-slice-r1` | **`ACCEPT`** (stage 2, final) | `O-OPEN` | `a2h-oom-causal-slice-acceptance-record.md:17` |
| 2 | `A2h-named-producer-frame-r1` | **`ACCEPT`** + 3 corrections | `O-OPEN` | `a2h-named-producer-frame-acceptance-review.md:12` |
| 3 | `A2h-null-slot-triage-r1` | **`ACCEPT`** (stage 1) | `O-NO-BOUNDARY-TRANSITION` | `a2h-null-slot-triage-acceptance-review.md:11` |
| 4 | `A2h-slot-within-run-attribution-r1` | **`ACCEPT-WITH-CORRECTIONS`** | `O-COVERAGE` | `a2h-slot-within-run-acceptance-review.md:15` |
| 5 | `A2h-arming-coverage-attribution-r2` | **`ACCEPT`** | `O-COVERAGE` (K ≥ 2) | `a2h-arming-coverage-acceptance-record.md:7` |
| 6 | `A2h-arming-coverage-repeat-r1` | **`ACCEPT-WITH-CORRECTIONS`** | `O-COVERAGE` (**`PREMISE_CHANGED`**) | `a2h-arming-coverage-repeat-acceptance-review.md:10` |
| 7 | `A2h-rdata-call-target-r1` | **`ACCEPT-WITH-CORRECTIONS`** | `O-OPEN` | `a2h-rdata-call-target-acceptance-record.md:7` |

**Reconciliation against the independent cross-check:** the Session's cross-check
(`a2h-integrity-audit-session-crosscheck.md:15-24`) lists the same seven packets with the same final dispositions.
**The two inventories agree on the row universe; no disagreement required an `A-IDENTITY` referral.**

**Stage-history deduplication:** `A2h-oom-causal-slice-r1` accumulated **stage-1 `NOT ACCEPTED`** plus
**stage-2 rounds 1–4 `NOT ACCEPTED`** before round 5 `ACCEPT`
(`a2h-oom-causal-slice-stage2-acceptance-review.md:1001`). Only the **final** disposition is inventoried; the
intermediate rejections are history, not rows. The same treatment applies to the repeated
`EXECUTED → O-OPEN` / `PROMOTED` blocks for `a2h-named-producer-frame-r1` and `a2h-oom-causal-slice-r1` in the
plan, which are the same revision recorded at different lifecycle stages.

### 1.2 Excluded rows, with reasons

| Packet / revision | Row | Reason for exclusion |
|---|---|---|
| `A2h-callback-slot-writer-r1` | `O-OPEN` (named edge: `edi` identity) | **Row selected and Advisor-confirmed, but acceptance is PENDING — no acceptance artifact exists.** Excluded from the accepted universe. Reported in §3.4 because it is the motivating case. |
| `A2h-dr0-delivery-gate-r1` | none | **No acceptance artifact.** Terminated `P1-UNKNOWN`; the DR leg was dropped. |
| `A2h-dr0-terminal-snapshot-r1` | none | **No acceptance artifact.** Terminal ceiling reached; DR leg + coverage-provenance dropped. |
| `A2h-slot-read-path-displacement-r1` | `O-COVERAGE` (Run 1 OFF) | **Row withdrawn / superseded** by the within-run redesign; the packet is `DRAFT, NOT promoted` (`docs/packets/a2h-slot-read-path-displacement.md:3`). Its acceptance never occurred. |
| `A2h-live-oom-arg2-bridge-r1` | none | **Never executed, never accepted.** |
| `A2h-writer-investigation` | none | **Not a promoted revision**; planning/analysis only. |
| `A2h-arming-coverage-repeat` earlier `O-READ-PATH` ruling | `O-READ-PATH` | **Superseded** by `docs/reviews/a2h-repeat-row-interpretation.md:3` after the DR0 positive-control failure. Only `O-COVERAGE` is accepted. |
| `A2h-integrity-audit-r1` | — | **The current packet.** Not a decision row. |
| `A4b2-r8`, `A4b1`, `A4b1-r4`, `A4b2-r7`, `PIO_FREE`, `0xFFFFB3`, retired NULL line | — | **Out of scope by hard constraint 2/3 and by the Advisor's clearances** (`a2h-callback-slot-identity-advisor-ruling.md:22,67-69`). Not audited. No DR record is cited anywhere in this audit. |

**No row was silently dropped, and no contradictory identity was found.**

---

## 2. Per-run integrity gates (packet lines 6–7)

**Primary command, run separately for every implicated run directory:**

```text
python -X utf8 scripts/check-dump-mapping.py logs/runs/<run-dir>
```

### 2.1 The full A2h population — 24 runs, reproducing the census exactly

I re-ran the gate on **all 24** archived A2h run directories. **Result: 23 `CONTENT_MISMATCH`, 1 pass.**

| Gate status | Count | Runs |
|---|---|---|
| **PASS** (`matches: 1`, `content-mismatch: 0`) | **1** | `20260928-035337-386-a2h-repeat-on-1` |
| **`CONTENT_MISMATCH`** | **23** | every other `*a2h*` run directory |

**Every failing run reports the identical displaced prefix** `83c41456ff5010eb5d8b44240c85c06a` against the
documented `.text` prefix `8b512c85d28b4130c70190431c00741c`.

> **This 23/24 figure is used here ONLY as a population statement, exactly as packet line 15 requires.** No
> verdict in §3 rests on it. Each verdict cites the primary gate result for the row's **own** run(s), checked
> individually.

### 2.2 Per-row gate results — the runs actually consumed

| Run directory | Gate status | Command | Consumed by |
|---|---|---|---|
| `20260928-035337-386-a2h-repeat-on-1` | **PASS** | `check-dump-mapping.py` | §3.4 identity gate (the **one** passing run) |
| `20260927-160330-655-a4b2-gp-trap-trace` | **`CONTENT_MISMATCH`** | `check-dump-mapping.py` | **§3.1 `A2h-named-producer-frame-r1`** |
| `20260927-130036-879-a4b2-nr-baseline` | **`CONTENT_MISMATCH`** | `check-dump-mapping.py` | §3.2 `A2h-null-slot-triage-r1` (named read only) |
| `20260922-224429-003-a2g-304f0-span` | **`CONTENT_MISMATCH`** | `check-dump-mapping.py` | §3.5 `A2h-oom-causal-slice-r1` (named log source) |
| `20260927-111948-580-a4b2-r7-gp-trap-trace` | **`CONTENT_MISMATCH`** | `check-dump-mapping.py` | §3.6 (checked for completeness) |
| `20260928-001502-101-a2h-null-slot-inert-off` | **`CONTENT_MISMATCH`** | `check-dump-mapping.py` | §3.2 (OFF control) |
| `20260928-001520-474-a2h-null-slot-authoritative-on` | **`CONTENT_MISMATCH`** | `check-dump-mapping.py` | §3.2 (authoritative ON) |
| all five `20260928-0204*-a2h-within-run-on-*` | **`CONTENT_MISMATCH`** | `check-dump-mapping.py` | §3.7 |
| all six `20260928-030*-a2h-arming-coverage-*` | **`CONTENT_MISMATCH`** | `check-dump-mapping.py` | §3.7 |
| `20260928-035402-315…035425-633-a2h-repeat-on-2..5` | **`CONTENT_MISMATCH`** | `check-dump-mapping.py` | §3.7 |
| `20260928-043810-117` / `20260928-043836-929-a2h-dr0-gate-*` | **`CONTENT_MISMATCH`** | `check-dump-mapping.py` | §3.7 |
| `20260928-052203-982` / `20260928-052216-631-a2h-snapshot-*` | **`CONTENT_MISMATCH`** | `check-dump-mapping.py` | §3.7 |
| `20260928-014526-583-a2h-slot-write-inert-off` | **`CONTENT_MISMATCH`** | `check-dump-mapping.py` | §3.7 |
| `20260928-011149-570-a2h-latch-print-verify` | **`CONTENT_MISMATCH`** | `check-dump-mapping.py` | §3.7 |

**Both rows that cite a run by name and then decline to interpret its XBE-backed content did so correctly.**
`a2h-named-producer-frame-evidence.md:312` states the run is `content-mismatch` and that *"the apparent `0` at
`0x001C4060` in the dump is **not** admissible evidence"*; the `a2h-named-producer-frame-acceptance-review.md:23`
re-ran the gate and reproduced it. That is the fail-closed behaviour the packet demands — **except that the same
run's dump was nonetheless consumed for a different claim (§3.1).**

---

## 3. Per-row ledger and verdicts (packet lines 5, 11, 12)

Full table as required by packet line 13:

| Packet / revision | Accepted row / claim | Exact decision inputs (artifact location) | Source per input | Exact run(s) / timestamp(s) | Per-run mapping status + control | Verdict |
|---|---|---|---|---|---|---|
| `A2h-oom-causal-slice-r1` | `O-OPEN` — chain bound to producer; **no positive attribution** | Acceptance record `a2h-oom-causal-slice-acceptance-record.md:17`; log lines `logs/runs/20260922-224429-003-a2g-304f0-span/jsrf_run.log:16990-17000` | `log/trace/code-static` (log lines, XBE bytes via `a2h-oom-slice.py`) | `20260922-224429-003-a2g-304f0-span` | **`CONTENT_MISMATCH`**; control passed (line 26 checkpoint reproduced) | **`ASSERTS-NOTHING`** |
| `A2h-named-producer-frame-r1` | `O-OPEN` **+ affirmative frame-identity claim** (`E=0x00F7FEA0`, `ebp=0x00F7FE9C`, `arg2=0x00F7FEAC`) | `a2h-named-producer-frame-acceptance-review.md:27,31,112-120`; evidence `a2h-named-producer-frame-evidence.md:83-102` | **mixed**: `code-static` (XBE/frame arithmetic) **+ `guest-memory-from-dump`** (`inspect-jsrf.py memory … 0x00F7FE60 0xA0`) | `20260927-160330-655-a4b2-gp-trap-trace` | **`CONTENT_MISMATCH`** (primary gate re-run); control passed | **`TAINTED`** (label `O-OPEN` ⇒ row-level `ASSERTS-NOTHING`) |
| `A2h-null-slot-triage-r1` | `O-NO-BOUNDARY-TRANSITION` — positive negative-result row | `a2h-null-slot-triage-execution-evidence.md`; latch `observed=0`; `[A2HSLOT]` terminal line | `log/trace/code-static` **+ `live hook reads`** (latch at bridge boundaries) | `20260928-001502-101…inert-off`, `20260928-001520-474…authoritative-on` | **`CONTENT_MISMATCH`** both; control passed (`jsrf_run.log:26`, `[A2HSLOT] terminal` line) | **`NO-GUEST-MEMORY`** |
| `A2h-slot-within-run-attribution-r1` | `O-COVERAGE` | `a2h-slot-within-run-acceptance-review.md:86-89`; `stacks.txt` `GUEST_DR_ARM` / census lines | `log/trace/code-static` **+ `live hook reads`** (`stacks.txt`, 5 runs) | five `20260928-0204*-a2h-within-run-on-*` | **`CONTENT_MISMATCH`** all five; control passed (`stacks.txt` count 1149) | **`NO-GUEST-MEMORY`** |
| `A2h-arming-coverage-attribution-r2` | `O-COVERAGE` (K ≥ 2) | `a2h-arming-coverage-acceptance-record.md:31-47`; 17 `GUEST_DR_ARM_OK`, registry via `a2h-read-registry.py` | `log/trace/code-static` **+ `live hook reads`**; registry = **HOST** dump address | six `20260928-030*-a2h-arming-coverage-*` | **`CONTENT_MISMATCH`** all six; control passed | **`NO-GUEST-MEMORY`** |
| `A2h-arming-coverage-repeat-r1` | `O-COVERAGE` (`PREMISE_CHANGED`) | `a2h-repeat-row-interpretation.md:3-7` | `inference` from explicit premises (DR0 positive-control failure) | five `20260928-0353*/0354*-a2h-repeat-on-*` | **`CONTENT_MISMATCH`** 4 of 5; **`PASS`** 1 of 5; control passed | **`NO-GUEST-MEMORY`** |
| `A2h-rdata-call-target-r1` | `O-OPEN` — named edge | `a2h-rdata-call-target-acceptance-record.md:7,15`; static sweeps + `recovered-functions.json` | `code-static` only (XBE disassembly, section tables, manifests) | none (offline static) | **No run consumed** — no mapping gate required | **`ASSERTS-NOTHING`** |

### 3.1 `A2h-named-producer-frame-r1` — **`TAINTED`** (the one consumed failing-run dump read)

**The exact affirmative claim.** The stage-1 acceptance review records, as **Criterion 3 `AGREED`**
(`a2h-named-producer-frame-acceptance-review.md:60`):

> *"**Criterion 3 — correct row selected: `AGREED`**"*, with the frame identity **independently recomputed** at
> `:93` — *"I also get `E = 0x00F7FEA0` — **AGREED, and more strongly corroborated than the evidence claims**"*.

**The exact consumed input.** The reviewer's own replay table (`:31`) names:

```text
python -X utf8 scripts/inspect-jsrf.py memory … 0x00F7FE60 0xA0     ->  "the live stack window below"
```

and at `:112-120` that window is tabulated **as the decisive corroboration of `E`**:

| Address | Dump value | Reviewer's judgement |
|---|---|---|
| `E−16` = `0x00F7FE90` | `001804A0` | helper's `push 0x1804A0` — **exact** |
| `E−12` = `0x00F7FE94` | `001E0BE8` | callee's relocation — **exact** |
| `E` = `0x00F7FEA0` | **`0017C926`** | return address = `call@0x0017C921 + 5` — **exact** |

**This is `guest-memory-from-dump`, not a live read and not a log line.** It is a read of guest VA
`0x00F7FE60–0x00F7FF00` out of `process.dmp` by `inspect-jsrf.py memory`.

**The exact run and its per-run gate.** The run is `logs/runs/20260927-160330-655-a4b2-gp-trap-trace`, corroborated
in primary metadata by the acceptance review's own command table (`:22-23`) and by
`docs/packets/a2h-live-oom-arg2-bridge.md:20`. I re-ran the gate on that directory:

```text
python -X utf8 scripts/check-dump-mapping.py logs/runs/20260927-160330-655-a4b2-gp-trap-trace
20260927-160330-655-a4b2-gp-trap-trace    CONTENT_MISMATCH   83c41456ff5010eb5d8b44240c85c06a
matches: 0   content-mismatch: 1   unreadable: 0   missing: 0
Not usable for an IMAGE-CONTENT claim
```

**⇒ A specifically checked failing run's guest memory was consumed by an accepted affirmative claim.**

**Mixed provenance — recorded separately, per packet line 5.** The row is **not** blanket-tainted. Its frame
arithmetic is **independently supported without dump guest memory**: `scripts/a2h-frame-audit.py frame
0x00F7FCF0 432` → `E=0x00F7FEA0 ebp=0x00F7FE9C arg2=0x00F7FEAC`, and the reviewer's own three arithmetic facts
(`:103-107`) all resolve `E = 0x00F7FEA0` from **XBE bytes and registers**. The dump window is **corroboration**,
which the reviewer itself ranked as *"more strongly corroborated"* rather than as the sole basis. **The taint
therefore degrades the corroboration and the `:112-120` "exact" match table; it does not by itself falsify the
row.** That is a remediation judgement, not an audit verdict — see §5.

**The label check (packet line 11).** The row **label is `O-OPEN`**, so at row level it **asserts nothing
positive** and is also reported as `ASSERTS-NOTHING` in §3.9. It is reported as `TAINTED` because the **accepted
record's affirmative content** — a claim the reviewer marked `AGREED` and which the corrections process
preserved — consumes the failing-run dump read. Both readings are stated rather than one being chosen silently.

### 3.2 `A2h-null-slot-triage-r1` — **`NO-GUEST-MEMORY`** (named read not consumed)

The packet **names** two runs including `20260927-130036-879-a4b2-nr-baseline` and asks for *"the count of
`(ordinal=277,ret=0x0014982E)`"* from their logs (`docs/packets/a2h-null-slot-triage.md:39`). The acceptance
review states the decision input explicitly (`a2h-null-slot-triage-acceptance-review.md:291-297`):

> *"**The latch is the authoritative record.** … The row's decisive negative (no first-zero transition) comes
> from the latch's `observed=0` field, printed as the terminal `[A2HSLOT] … observed=0`, **not from the absence of
> a log line**."*

I verified that terminal line **in the same log stream** as the run's own checkpoint:

```text
logs/runs/20260928-001520-474-a2h-null-slot-authoritative-on/jsrf_run.log
  line 26:  APU: 0xFE800000..0xFE880000 trapped for MMIO          <- stream control
  terminal: [A2HSLOT] terminal tid=44768 slot=001C4064 live=00000000 call=#5555 observed=0
```

**Every decisive input is a log line or a live hook read. No dump guest memory is consumed.** The gate on both
named runs is `CONTENT_MISMATCH`, but the row does not read XBE-backed memory from either, so the failure is
**immaterial to this row**. `NO-GUEST-MEMORY`, not `CLEAN` — the packet's distinction at line 12 is applied
exactly.

### 3.3 `A2h-arming-coverage-attribution-r2` and the registry extraction — **HOST address, not guest memory**

The `O-COVERAGE` row's registry-derived inputs come from `scripts/a2h-read-registry.py`. **This is a minidump read
but not a guest-memory read**, and the distinction is decisive. The tool's own header
(`scripts/a2h-read-registry.py:11-14`) states it:

> *"the registry sits at a **HOST** address (e.g. `0x00007FF745DF9000`), while `scripts/inspect-jsrf.py` reads
> **guest** addresses and rejects a 64-bit VA outright."*

The `check-dump-mapping.py` gate tests **XBE-backed guest content at guest VA `0x00011000`**. A host-address
extraction of a separately allocated struct is **outside that gate's scope** and is not displaced by the
guest-image displacement. The extraction additionally carries its **own independent known-answer controls**
(`a2h-latch-extraction-record.md:38-44`): a unique registry header, a unique install record, and a measured
signature distance of **538648 = the layout-derived `LATCH_OFF` exactly** — a control that would fail if the
struct layout were wrong, as it did when `EVENT_SIZE` was 24 instead of 32 (`:46-52`). **`NO-GUEST-MEMORY`.**

### 3.4 `A2h-callback-slot-writer-r1` — the motivating case, **excluded** (acceptance pending)

Reported here because it is the case that motivated this audit, but **it is not in the accepted universe** (§1.2).

**Its dump reads used the one passing run, and its row is `O-OPEN`.**

| Read | Run | Gate | Consumed? |
|---|---|---|---|
| `MEM32(0x19DCE0)` = `0x0019B200` | `20260928-035337-386-a2h-repeat-on-1` | **`PASS`** | yes — positive, matches the generated initializer |
| `MEM32(device+0x2268)` = `0xFD` | same | **`PASS`** | yes — **but this is the refutation, not the identification** |
| `MEM32(device+0x242C)` = `0x0015F9D0` | same | **`PASS`** | yes — that run's value only |

**The Advisor has already ruled this content is not load-bearing** (`a2h-callback-slot-identity-advisor-ruling.md:22`):
the `0xFD`/layout values are *"single-snapshot corroboration"* and *"the static no-construction-site carries the
refutation."* I confirm the run it used **passes** the per-run gate, so **no taint arises on this row either
way.** The row label is `O-OPEN` with a named edge (`edi` identity still `UNKNOWN`,
`a2h-callback-slot-writer-r1-row-selection.md`), so it would report `ASSERTS-NOTHING` if accepted.

### 3.5 `A2h-oom-causal-slice-r1` — **`ASSERTS-NOTHING`**

Row `O-OPEN`, selected because *"the known pre-add local still lacks a producing definition"*
(`docs/packets/a2h-oom-causal-slice.md:47`). The accepted record carries **no positive attribution**. Its inputs
are log lines (`a2g-304f0-span/jsrf_run.log:16990-17000`) and XBE bytes via `a2h-oom-slice.py` — **not** dump
guest memory. Per packet line 11 I verify **only** the final row label, revision and acceptance identity, and
**do not backfill** the narrative into an accepted claim. `ASSERTS-NOTHING`.

### 3.6 `A2h-rdata-call-target-r1` — **`ASSERTS-NOTHING`**

Row `O-OPEN`, *"the missing edge is REAL … The gap is a property of the code"*
(`a2h-rdata-call-target-acceptance-record.md:15`). **Fully offline and static:** `inspect-jsrf.py find/disasm/data`
against the **original XBE**, plus `recovered-functions.json`, `recovered.c` and `recomp_dispatch.c`. **No
archived run is consumed at all**, so no mapping gate applies. `ASSERTS-NOTHING`.

### 3.7 The five live-instrument rows — **`NO-GUEST-MEMORY`**

| Revision | Row | Decisive inputs | Source class | Runs | Gate |
|---|---|---|---|---|---|
| `A2h-slot-within-run-attribution-r1` | `O-COVERAGE` | `GUEST_DR_ARM` / alias census in `stacks.txt` | `live hook reads` | 5 ON runs | `CONTENT_MISMATCH` ×5 — not consumed |
| `A2h-arming-coverage-attribution-r2` | `O-COVERAGE` | 17 `GUEST_DR_ARM_OK` + registry | `live hook reads` + HOST-address registry | 6 runs | `CONTENT_MISMATCH` ×6 — not consumed |
| `A2h-arming-coverage-repeat-r1` | `O-COVERAGE` | DR0 positive-control failure | `inference` | 5 runs | 4 mismatch, 1 PASS — not consumed |
| `A2h-oom-causal-slice-r1` | `O-OPEN` | log lines + XBE | `log/trace/code-static` | A2g archive | `CONTENT_MISMATCH` — not consumed |
| `A2h-rdata-call-target-r1` | `O-OPEN` | XBE disassembly | `code-static` | none | n/a |

For all five, **the decisive inputs are records the running process emitted into `jsrf_run.log` or `stacks.txt`,
or live hook observations the collector took during the run.** These are **not** XBE-backed guest memory read back
from a frozen dump, and the `.text` displacement does not reach them. The Session's own preliminary expectation
(`a2h-integrity-audit-session-crosscheck.md:34-45`) is **confirmed** for these rows.

### 3.8 `A2h-arming-coverage-repeat-r1` and the census caveat

This row's comparator set includes the **one passing run** (`…a2h-repeat-on-1`) and four failing ones. **No
verdict here cites the census.** The row is `NO-GUEST-MEMORY` because its decisive input is the DR0
positive-control failure — an `inference` from `install_ok`/DR7 evidence — and the five runs' mapping statuses
are reported above as **individual per-run results**, not as a population statement.

### 3.9 `ASSERTS-NOTHING` rows — label check only

| Revision | Final label | Acceptance identity verified | Positive claim backfilled? |
|---|---|---|---|
| `A2h-oom-causal-slice-r1` | `O-OPEN` | yes — `ACCEPT` stage 2 final | **no** |
| `A2h-named-producer-frame-r1` | `O-OPEN` | yes — `ACCEPT` + 3 corrections | **no** (its affirmative content is separately flagged `TAINTED` in §3.1) |
| `A2h-rdata-call-target-r1` | `O-OPEN` | yes — `ACCEPT-WITH-CORRECTIONS` | **no** |

**Three of the seven accepted rows assert nothing positive.** Their narratives are rich — the frame row's
identity chain in particular — but per packet line 11 the narrative is **not** read as an accepted positive claim.
The one place where the accepted record *did* affirmatively adopt a positive claim (§3.1) is flagged, not
absorbed.

---

## 4. Extraction controls (packet lines 8–10)

**Every extraction was gated by an independent known-answer witness. All controls passed; none was absent or
failed.**

| # | Control (packet line) | Expected | Observed | Result |
|---|---|---|---|---|
| 1 | **Worked dump control** (line 9) — `inspect-jsrf.py memory` on `djv000_0.adx` at `0x001D5078`, **run gated first** | `30766A64 305F3030 7864612E` | `001D5078: 30766A64 305F3030 7864612E` (run `20260928-035337-386-a2h-repeat-on-1`, gate **PASS**) | **PASS** |
| 2 | **Mapping output** (line 10) — documented `.text` prefix at `0x00011000` | `8b512c85d28b4130c70190431c00741c` | printed as `expected` in all 24 invocations; every failing run compared against it | **PASS** |
| 3 | **Row labels** (line 10) — named accepted-row label + matching final acceptance text | e.g. `O-NO-BOUNDARY-TRANSITION` / `O-COVERAGE` | matched in each acceptance artifact §1.1 | **PASS** |
| 4 | **Code extraction** (line 10) — named fixed source literal | `recomp_0004.c:40150: MEM32(0x19DCE0) = 0x19B200;` | **verified at that exact line** in `src/recomp/gen/recomp_0004.c` | **PASS** |
| 5 | **Log/trace control** (line 10) — known checkpoint line + timestamp **from the same stream** | `jsrf_run.log:26` `APU: 0xFE800000..0xFE880000 trapped for MMIO` | reproduced in the triage runs, in the same file that carries the `[A2HSLOT] terminal … observed=0` line | **PASS** |
| 6 | **Registry layout control** (§3.3) | measured signature distance = `LATCH_OFF` = `538648` | exact match, per `a2h-latch-extraction-record.md:62` | **PASS** |

**Control 1 is the one that mattered.** It independently confirms that `inspect-jsrf.py memory` prints
**little-endian DWORD values**, not memory-order bytes — the byte-order trap that produced a near-miss false
finding in this line (`a2h-callback-slot-identity-gate-executed.md:10-29`). **All dump values quoted in this
record are DWORD values as the tool prints them.** No value was re-decoded, shifted, or repaired.

---

> **R-1 REMEDIATED 2026-09-28** — per the Advisor ruling (`a2h-integrity-audit-remediation-advisor-ruling.md`, turn `01a0e83a`): **demote, do not re-derive** (re-derivation is **FORECLOSED**, not merely declined). **The erratum is written and its scope is WIDER than this record's R-1 brief:** it withdraws the corroboration table **AND the caller return-address binding**, because `0x00F7FEA0` sits **inside** the tainted window. See **`a2h-named-producer-frame-dump-window-erratum.md`**. **Row unchanged: `O-OPEN`.**

## 5. Flagged remediation list (packet line 13)

> **One remediation item. A separate remediation decision is required before the queued caller-trace.**

### R-1 — `A2h-named-producer-frame-r1`: accepted frame-identity claim consumed a failing-run dump read

| Field | Value |
|---|---|
| **Tainted row** | `A2h-named-producer-frame-r1` (label `O-OPEN`; accepted `ACCEPT` + 3 corrections) |
| **Exact consumed input** | `inspect-jsrf.py memory <run> 0x00F7FE60 0xA0` — the guest stack window at `0x00F7FE8C–0x00F7FEAC` |
| **Run** | `logs/runs/20260927-160330-655-a4b2-gp-trap-trace` |
| **Per-run gate status** | **`CONTENT_MISMATCH`** — `matches: 0   content-mismatch: 1   unreadable: 0   missing: 0` |
| **Affected claim** | The accepted `E = 0x00F7FEA0` corroboration table (`a2h-named-producer-frame-acceptance-review.md:112-120`): the `E−16`/`E−12`/`E` *"exact"* matches, and the reviewer's §5 finding that the evidence was *"more strongly corroborated than the evidence claims"* |
| **Not affected** | The frame arithmetic itself — independently supported by XBE bytes, registers and `a2h-frame-audit.py` (three independent routes to `E = 0x00F7FEA0`) |
| **Requested action** | A **separately scoped remediation decision**: either (a) re-derive the `0x00F7FE8C–0x00F7FEAC` window from a **mapping-clean** run, or (b) formally demote the dump-window corroboration to **non-admissible** and rest the accepted claim on the arithmetic routes alone. **Do not** treat the displaced window as repaired memory, and **do not** infer a passing status from another run (packet line 7). |

**Why this is a taint and not merely a caveat.** The row's own evidence record correctly refused to read the same
run's dump for the **slot** question — *"the apparent `0` at `0x001C4060` in the dump is **not** admissible
evidence"* (`a2h-named-producer-frame-evidence.md:312-314`). The **same run's dump was nonetheless read and
tabulated as `exact` for the frame question.** The asymmetry is the finding: the integrity gate was applied to one
claim and not to its neighbour in the same archive.

**Scope note.** This is the only remediation item. **No other accepted row consumes dump guest memory**, and no
other accepted row's run set contains a consumed failing-run read.

---

## 6. Gaps, and what could not be established

**No `A-IDENTITY` or `A-OPEN` condition was found.** Stated explicitly so the absence is on the record:

| Potential gap | Finding |
|---|---|
| Inventory identity | **Resolved.** 7 accepted packets; agrees with the independent cross-check. No contradictory identity. |
| Accepted-row identity | **Resolved.** Every row label matched its acceptance artifact's disposition line. |
| Input-to-run link | **Resolved.** Every guest-memory input resolved to a named archived run directory, corroborated in primary packet/review metadata. |
| Run identity | **Resolved.** All 24 A2h run directories present and gated; no `unreadable`, no `missing` in any gate output. |
| Control failures | **None.** All six controls in §4 passed. |

**Honest limits of this audit:**

1. **One accepted row's taint is a degradation, not a disproof.** I did **not** attempt to determine whether the
   displaced dump window's `0x00F7FE90 = 001804A0` / `0x00F7FE94 = 001E0BE8` / `0x00F7FEA0 = 0017C926` values
   are *coincidentally correct* under displacement. Packet line 7 forbids repairing a shifted dump, and a
   displaced comparison *"may establish byte provenance; it must never be used as a correction."* **The values
   are uninterpretable; whether they happen to be right is not establishable by this audit.**
2. **The `0x00F7FEAC = 0x4C000010` value is outside this audit's reach.** It is the failing activation's `arg2`,
   and the row is `O-OPEN` precisely because *"the failing activation's `arg2` value is not in any artifact"*
   (`a2h-named-producer-frame-acceptance-review.md:72`). No accepted positive row rests on it.
3. **`A2h-callback-slot-writer-r1` is not covered by an acceptance artifact.** Its row is Advisor-confirmed and
   its dump reads use the single passing run, but **acceptance is pending** (§3.4). If it is later accepted with
   the `O-OPEN` label, it will report `ASSERTS-NOTHING` and **no taint will arise** — I verified its run passes.
4. **This audit did not re-execute any acceptance reviewer's arithmetic.** It verified each row's **label,
   revision, acceptance identity, decisive input location, input class, run binding and per-run gate** — which is
   the packet's scope — not the correctness of the underlying analyses.

---

## 7. Conclusion

**`A-TAINTED`.** Of the seven accepted A2h decision rows:

- **one (`A2h-named-producer-frame-r1`)** consumed guest-memory from a **specifically checked failing run**
  (`20260927-160330-655-a4b2-gp-trap-trace`, `CONTENT_MISMATCH`) in an affirmative, acceptance-`AGREED` claim —
  flagged as **R-1**;
- **three** (`A2h-oom-causal-slice-r1`, `A2h-named-producer-frame-r1`, `A2h-rdata-call-target-r1`) carry
  **`O-OPEN`** labels and assert nothing positive;
- **four** (`A2h-null-slot-triage-r1`, `A2h-slot-within-run-attribution-r1`,
  `A2h-arming-coverage-attribution-r2`, `A2h-arming-coverage-repeat-r1`) are **`NO-GUEST-MEMORY`** — their
  decisive inputs are log lines, `stacks.txt` live-hook records, and host-address minidump struct extraction,
  none of which the guest-image displacement reaches;
- **no row is `CLEAN`** — the packet's narrow definition (positive inputs *include* dump guest memory **and** all
  implicated runs pass) is satisfied by no accepted row, and inventing one is exactly what line 12 forbids.

**Per packet line 16, the queued caller-trace must wait for a separate remediation decision on R-1.** The
caller-trace's own register inputs (`edi` identity) were drawn from the **passing** run and are **not** tainted by
this finding — but the audit's verdict governs the sequence, and the packet makes a taint, not a clearance, the
trigger for referral.

**Prohibitions observed:** read-only throughout; no game run, build, test, instrumentation or source edit; no
synthetic completion; `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` untouched; `PIO_FREE`,
`A4b2-r7`, `A4b2-r8`, `A4b1-r4` and `0xFFFFB3` not touched; the retired NULL line not reopened; **no DR record
cited**; the frozen packet not edited; no scratch file left in `scripts/`.
