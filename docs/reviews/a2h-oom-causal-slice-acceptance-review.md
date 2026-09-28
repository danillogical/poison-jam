# `A2h-oom-causal-slice-r1` — Acceptance review (stage 1)

**Reviewer:** Acceptance reviewer, stage 1 (`workbuddy-ai/hy4-preview-f` — contract role, §2.2).
**Date:** 2026-09-27. **Harness:** DSH.
**Contract:** `docs/packets/a2h-oom-causal-slice.md`, revision **`A2h-oom-causal-slice-r1`**, **class: discovery
(§5.8)**.
**Frozen SHA-256 — verified by me:** `E9CDB1B39066CE5F5626FF74246E5EAD34C1612BC31BE622341B50FCC541108C`
(16 198 bytes, 47 lines). **Matches the revision and hash the Session recorded, and matches
`plan-jsrf-bare-minimum.md:139`.**

**Discovery acceptance bar (§5.8), applied literally:**

> *reviewer confirms the artifacts exist, match the commands, and select the recorded outcome row*

A discovery packet **never satisfies a strict criterion and never claims anything works.** Nothing below
treats any finding as evidence that anything works.

## Method

Everything below is a measurement I performed in this session: `Get-FileHash`, `git`, `git grep`,
`git show`, `git log -S`, `python -X utf8 -m unittest`, `scripts/inspect-jsrf.py disasm`,
`scripts/check-run-profile.py`, `scripts/check-dump-mapping.py`, `scripts/a2h-oom-slice.py`, and two
scratch CFG scripts written to **`%TEMP%`** (outside both repositories). I fetched nothing external, ran no
guest, no build, and modified no file except this review. Both working trees were clean before and after.

---

## 1. Artifacts exist and match the commands

| Artifact | Exists | Role per packet/record |
|---|---|---|
| `scripts/a2h-oom-slice.py` | yes (9 044 B) | the required checked-in parser/binder |
| `scripts/test_a2h_oom_slice.py` | yes (11 245 B) | its tests |
| `docs/reviews/a2h-oom-causal-slice-binding.json` | yes (32 326 B) | the parser's deterministic output |
| `docs/reviews/a2h-oom-causal-slice-evidence.md` | yes (16 947 B, 273 lines) | the main evidence record |
| `docs/reviews/a2h-mechanism.md` | yes (10 475 B) | the Session's earlier characterisation |
| `docs/reviews/a2h-oom-causal-slice-r1-session-verification.md` | yes (6 435 B) | the promotion record |

**The recorded command reproduces byte-for-byte.** I ran, from the game root:

```powershell
python -X utf8 -m unittest scripts.test_a2h_oom_slice
python -X utf8 scripts\a2h-oom-slice.py --log logs/runs/20260927-160330-655-a4b2-gp-trap-trace/jsrf_run.log --log logs/runs/20260922-224429-003-a2g-304f0-span/jsrf_run.log --out docs/reviews/a2h-oom-causal-slice-binding.json
```

The second command's output, written to `%TEMP%` so as not to touch the record under review, hashes to
`A11C55FEEA70079C8F4258F40A6007333EC5B55E5A28BF731DD9ADD96F8B600C` — **identical to the checked-in
`a2h-oom-causal-slice-binding.json`.** The binding is deterministic and matches the command.

**Identity:** game HEAD `977fee3d590f502a9b0ceb7aacb44fd84d9f7110` (clean); toolkit `c151d4e32a782e4e…`
(clean); XBE `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C`. All three match the
record. The evidence record cites game `b3bf2c1cf06e…` as clean at execution; `git merge-base --is-ancestor`
confirms it is an ancestor of HEAD, and the two intervening commits (`1201774`, `977fee3`) touch only the
packet's own write scope. Consistent.

**Verdict: AGREED** — artifacts exist and match the commands.

---

## 2. Trap necessity — the semantic identity reproduces, but the "no-trap" characterisation is wrong

I verified both logs myself rather than trusting the table.

| Field | R1 `20260927-160330-655-a4b2-gp-trap-trace` | A2g `20260922-224429-003-a2g-304f0-span` | Same |
|---|---|---|---|
| ordinal-184 invocations | **94** (my count: 94) | **94** (my count: 94) | ✓ |
| failing index | **93** (my count: 93) | **93** (my count: 93) | ✓ |
| failing `ret` | `0x00149E50` | `0x00149E50` | ✓ |
| failing `esp` | `0x00F7FCF0` | `0x00F7FCF0` | ✓ |
| `base` / `size` / `type` | `0x0` / **598869040** / `0x801000` | identical | ✓ |
| OOM tuple | `(598869040, 12715008, 50855936)` | identical | ✓ |
| ICALL | `(0x0, 00F7FD00, 0014982E)` | identical | ✓ |
| `check-run-profile.py` | **STRICT** (exit 0) | **EXPLORATORY** (exit 1) | as claimed |
| `check-dump-mapping.py` | `CONTENT_MISMATCH` | `CONTENT_MISMATCH` | ✓ |
| log SHA-256 | `c52d71655c48…6c874` | `b9631ee5af44…76a04` | matches record |

**Stronger than claimed, and it favours the Session:** I dumped all 94 sizes by index for both logs. They
are **identical index-for-index across all 94 invocations**, not merely at the failing index. Index 89 =
`2097200` and index 93 = `598869040` in both. This is a better witness for semantic identity than the
record's single-row table.

**The trap is not a necessary cause: CONFIRMED.** The chain reproduces on a different exe
(`bc8e288dd54d…` vs `2cd0472a256e…`) with a different toolkit revision (`484887b88ff3…` vs `c151d4e…`),
five days earlier. A2g is correctly classified **EXPLORATORY** and the record uses it only to contradict
trap necessity, never as strict validation. That is exactly what the packet permits.

### BUT — the row label "**no-trap**" is factually wrong, and it is load-bearing

The evidence record's table heads the A2g column **"A2g (no-trap, historical)"**, the narrative says
*"A run with **no trap at all**"*, and `docs/reviews/a2h-mechanism.md:28` tabulates A2g's trap as
**"ABSENT"**. All three are false:

1. `logs/runs/20260922-224429-003-a2g-304f0-span/metadata.json` → `settings` contains
   **`{"name": "RECOMP_APU_TRAP", "value": "1"}`**.
2. The A2g log, **line 26**, reads **`APU: 0xFE800000..0xFE880000 trapped for MMIO`** — the same line as R1.
3. A2g's toolkit revision `484887b88ff3…` (an ancestor of `c151d4e`; `git merge-base --is-ancestor`
   exit 0) contains the trap arm at `xbox_memory_layout.c:1636`
   (`if (getenv("RECOMP_APU_TRAP"))` → `VirtualProtect(..., PAGE_NOACCESS)`), and commit `bd904c2`
   ("Route APU register access to the emulated APU", 2026-09-22, before A2g ran at 2026-09-23T05:44Z)
   wires `apu_hook_handle_mmio` into the game's `main.c`. The APU-core message
   *"read of unimplemented GP DSP block at offset 0x30200"* at A2g log line 12989 is emitted from
   `apu_core.c`, reached only through the MMIO-hook path.

**So A2g is trapped with a routed handler — not a no-trap run.** What A2g genuinely lacks is
`RECOMP_APU_TRACE` (0 `[APUMMIO]` lines vs R1's 401), which is *logging*, not the trap.

**Does this break the conclusion?** No — and this is the important part. The conclusion survives, but on
different and weaker ground than the record states:

- R1 is trapped **with** `RECOMP_APU_TRACE`; A2g is trapped **without** it. **Trace is observation-only and
  cannot change guest behaviour**, so the two runs still differ only in a setting that cannot cause the
  allocation. The chain's reproduction across them therefore still contradicts *trace* as a cause.
- What the pair **cannot** contradict is the **trap itself** (`RECOMP_APU_TRAP`, which unmaps the APU and
  does change behaviour), because **both runs have it**. Evidence that the trap is unnecessary requires a
  run with `RECOMP_APU_TRAP` **absent**, and no such run is in this archive pair.

**Consequence for the packet's premise.** The packet's own §"Question and starting evidence" asserts
*"prior no-trap failure rules out a trap-necessary mechanism"*, and the evidence record repeats it as
*"the decisive answer"*. On the artifacts I can read, **that is not established**: the cited run is trapped.
The honest statement is narrower — *the failure is not trace-caused and predates the A4b2 trap work*, which
is still real, still useful, and still supports the packet's decision not to chase the trap. It does not
support "the trap is not a necessary cause" in the strong form the records claim.

**Classification.** This is a **factual error in a load-bearing characterisation**, and it reaches two
records: `a2h-oom-causal-slice-evidence.md` (rows 44 and 55–69) and `a2h-mechanism.md:28`. It is not a
fabricated measurement — the sizes, indices and profiles I reproduced are all correct — but the record
states a property of a run that the run does not have. Per §2.4.1 (*"Never present an inference or a
brief's claim as observed"*) and §2.4.5 (*"Absence needs coverage"*), a "no trap at all" claim is an absence
claim, and the artifact positively refutes it.

Because §5.8's discovery bar includes *"confirm the artifacts ... match the commands"* and the artifact is
characterised wrongly at the point where the packet's premise is answered, I return **DISAGREED** on this
criterion, not because the reproduction failed but because the record's statement about it is false. I flag
it for the Session and, per §2.2, do not myself decide whether it reopens the packet — that is a
judgment-layer call (§3.1–3.2).

**Verdict: DISAGREED** (reproduction of the semantic identity: confirmed; the "no-trap" characterisation of
A2g: **refuted**; the trap-necessity conclusion survives only in the narrower trace/predecessor form).

---

## 3. The producer — reproduces both sizes exactly

I disassembled the region myself with `scripts/inspect-jsrf.py disasm 0x001497DC 0x00149820`. Every
instruction, byte and offset matches the record verbatim:

```
00149800  mov   eax, dword ptr [ebp + 0x10]
00149803  test  eax, eax
00149805  jne   0x149808
00149807  inc   eax
00149808  add   eax, 0x1f
0014980B  and   eax, 0xfffffff0
0014980E  mov   dword ptr [ebp - 0x24], eax
   ...
00149E24  add   dword ptr [ebp - 0x24], 0x20
```

**Arithmetic, recomputed independently** (`align16(x) = (x + 0x1f) & 0xfffffff0`):

| Invocation | Observed size | Pre-add (`size − 0x20`) | Multiple of 16? | Consistent input range for `[ebp+0x10]` | Reproduces? |
|---|---|---|---|---|---|
| index 89 | `2097200` = `0x00200030` | `0x00200010` | **yes** | `0x001FFFF1`–`0x00200010` | **YES** |
| index 93 | `598869040` = `0x23B20430` | `0x23B20410` | **yes** | `0x23B203F1`–`0x23B20410` | **YES** |

Both pre-add values are exact multiples of 16, as `and eax,0xfffffff0` requires. **Both observed sizes are
reproduced by one formula.** This is a positive check, not a fit.

**One arithmetic inaccuracy in the record (advisory, not blocking).** The record's table gives the implied
input ranges as `0x001FFFF2`–`0x00200010` and `0x23B203F2`–`0x23B20410`. The correct lower bounds are
`0x001FFFF1` and `0x23B203F1` (32 consistent values each, `pre − 31 … pre`). The upper bounds and the
"reproduces?" verdicts are right; only the low ends are off by one. The qualitative conclusion —
*"pointer-shaped, far outside the 64 MB RAM window"* — is unaffected.

**Verdict: AGREED.**

---

## 4. Dominance — reproduced independently

This is the claim I most tried to falsify. I did **not** reuse the Session's method: I wrote a fresh
capstone-based CFG script in `%TEMP%`, decoded `0x00149000`–`0x0014A400` from the original XBE
(`game/mygame_analysis.json` + `game/default.xbe`), built successor/predecessor edges with `calls treated
as fall-through` (intraprocedural, the conservative choice for this question), and ran reverse reachability
from the call at `0x00149E4A`.

| Claim | My independent result | Match |
|---|---|---|
| Instructions in region | **1 607** decodable (2 `.byte` at `0x0014A3FE`/`0x0014A3FF`, excluded) | record says 1 609 (raw listing incl. the 2 `.byte`); **same population** |
| Function span `0x001497DC`–`0x00149F48`, 548 insns | **548** — matches, and matches the generated header `recomp_0003.c:20101` | ✓ |
| Instructions that can reach the call | **47** (record: 46) | off by one (below) |
| Writers of `[ebp-0x24]` among them | **2** — `0x0014980E`, `0x00149E24` | ✓ **exactly as claimed** |
| `movzx` writer `0x00149FB6` reaches the call? | **NO** | ✓ **confirmed** |
| Candidate function entry reaching the call | **1** — `0x001497DC` (the only reaching node with no in-region predecessor) | ✓ |
| **Call reachable with all writers removed?** | **FALSE** — forward search from entry reaches only 16 nodes | ✓ **confirmed** |

**Both decisive sub-claims reproduce.** The two-writer set is exactly `0x0014980E` and `0x00149E24` (the
latter being the pre-call `add` itself), `0x00149FB6` provably does not reach the call, and removing the
writers makes the call unreachable from the single candidate entry.

**The off-by-one is explained, not a defect.** My reaching set is 47; the record says 46. The difference is
the conditional at `0x00149805` (`jne 0x149808`) plus its fall-through `0x00149807` (`inc eax`), which is
one node my decode counts inside the set. It does not touch any writer, the entry, or the removal result.
Likewise 1 607 vs 1 609 is the 2 undecodable `.byte` entries at the region tail. Both are counting
conventions, not disagreements about reachability.

**Caveat I record honestly:** my reachability is intraprocedural (calls fall through). `0x001497E6` calls
`0x17d1f8` and `0x00149828` calls `[0x1c4064]`, and `0x00149953` calls `0x14b6c0`; treating these as
fall-through is the conservative choice for "does every path pass a writer" — it can only *add* paths, so
the "unreachable with writers removed" result holds a fortiori. The result is robust to that choice.

**Verdict: AGREED** (reproduced; both decisive sub-claims confirmed).

---

## 5. Stale-slot hypothesis refuted

Follows from §4: with all writers removed the call is unreachable, so every path initialises `[ebp-0x24]`
from `[ebp+0x10]`. The refutation is sound. The record retains the superseded hypothesis visibly rather
than editing it away, which is the right presentation. **Verdict: AGREED.**

## 6. `sub_001497DC` is frameless

`src/recomp/gen/recomp_0003.c:20099-20113` reads, verbatim:

```
 * Frame: fpo_leaf
void sub_001497DC(void) {
    uint32_t ebp;
    ebp = g_ebp;  /* frameless: caller's frame */
    ...
    ebp = g_seh_ebp; /* fpo_leaf: inherit caller's frame */
```

The claimed comments are present verbatim at lines 20108 and 20113, and the packet's own chain instruction
(*"including the caller's passed inputs if reached"*) is honoured by the record's caution that `[ebp+0x10]`
is an offset in the **caller's** frame. **Verdict: AGREED.**

## 7. The arena behaved correctly

`alloc_type = 0x801000`: `MEM_COMMIT` `0x1000` **present**, `MEM_RESERVE` `0x2000` **absent**. Toolkit
`src/kernel/kernel_bridge.c:857` and `:876` both guard on
`(alloc_type & 0x2000) && !(alloc_type & 0x1000)` — **false** for `0x801000`, so neither reserve branch can
run; `:890-892` returns `0xC0000017`. I read all of this directly. Confirmed, and consistent with the
record's note that zero "reserve granted"/"clamped" messages appear. **Verdict: AGREED.**

## 8. No new guest run, no toolkit/runtime change, no instrumentation

- `logs/runs/` newest archives are `20260927-160330-655-a4b2-gp-trap-trace` and
  `20260927-160335-562-a4b2-default` (16:03–16:04). The evidence record is 19:07. **No archive newer than
  the packet's own cited runs exists**, consistent with "no new guest run".
- `git status --porcelain` clean in **both** trees before and after my review.
- `JSRF_TRACE_A2H_SIZE`: **zero** hits across game `src/*.c`, `src/*.h`, `main.c`, `recomp_manual.c`,
  `diagnostics.c` **and** the whole toolkit `src/`. No hook was added; there is nothing to disable at
  closure. (It was correctly listed as a packet *deliverable*, not a prerequisite.)
- Toolkit `c151d4e` clean and unchanged; no toolkit write.

**Verdict: AGREED.**

## 9. Prohibitions and deferrals

No trap suppression, no faked allocation, no arena widening (§7 confirms no widening was warranted or done).
`PIO_FREE` deferred, `A4b2-r7` / `A4b2-r8` / `A4b1-r4` not reopened, `0xFFFFB3` `UNRESOLVED` — all asserted
in the record; I found no artifact contradicting any of them, and no edit touching them (only the packet's
own write scope changed since `b3bf2c1`). **Verdict: AGREED.**

---

## 10. The parser tests — 22 pass, but one required fixture is MISSING

**I ran them:** `python -X utf8 -m unittest scripts.test_a2h_oom_slice` → **`Ran 22 tests ... OK`**, exit 0.
Matches the claim.

Coverage against the packet's named fixtures (step 3):

| Named fixture | Covered? | Test |
|---|---|---|
| good invocation | ✓ | `test_good_invocation_binds_site_size_and_esp` |
| duplicate/conflicting calls | ✓ | `test_duplicate_conflicting_calls_are_visible`, `test_duplicate_calls_at_one_site_are_both_reported` |
| missing / truncated events | ✓ | `test_truncated_invocation_is_fatal_not_skipped`, `test_missing_log_is_fatal`, `test_empty_log_is_fatal` |
| wrong thread | ✓ | `test_wrong_thread_is_preserved_verbatim` |
| wrong size | ✓ | `test_wrong_size_does_not_match_the_failing_set` |
| width / signedness | ✓ | `test_large_unsigned_size_is_not_sign_flipped` |
| **mid-instruction disassembly start REJECTED** | **✗ NONE** | — |

**The last one is absent, and it is not covered by proxy.** I checked exhaustively: no test method name, no
test body, and no code path references a disassembly start or an instruction boundary. The string
"mid-instruction" occurs only in the two docstrings (`test_a2h_oom_slice.py:5,10`), which *describe* the
requirement; nothing implements or exercises it. The nearest test,
`test_undecodable_bytes_are_fatal`, feeds **`b"\xff\xfe\x00bad"` to `parse_log`** — that is **log-file
encoding validation**, i.e. a UTF-8 decode failure, with no disassembly anywhere in it.

The root cause is structural: **`scripts/a2h-oom-slice.py` contains no disassembly function at all.** Its
four regexes (`ORDINAL_184`, `ALLOC`, `OOM`, `ICALL`) all parse log text. There is no capstone import, no
instruction-boundary validation, no `disasm` entry point. So while the requirement's *spirit* (reject
malformed input rather than silently mis-parse) is implemented for **log parsing**, the specific
**disassembly-boundary** guard the packet names does not exist in the tool and cannot be tested.

**The guard matters here, and the tool itself shows why.** `scripts/inspect-jsrf.py disasm` sets
`decoder.skipdata = True` with an in-file comment recording exactly this failure mode:

> *"Capstone stops at the first invalid instruction and yields nothing at all, so an arbitrary range — or a
> start that is not an instruction boundary — printed an empty listing that looked like 'no code here'
> rather than a decode failure."*

My own run confirms the hazard is live: decoding `0x00149000`–`0x0014A400` emitted **two `.byte` entries**
at `0x0014A3FE`/`0x0014A3FF` — silent non-decodes. The packet's boundary guard exists precisely to stop a
mid-instruction start from being read as plausible content, and in this project that mistake has a measured
cost (the record's own docstring cites *"a disassembly started mid-instruction that capstone silently
rendered as plausible garbage"*).

**Does this block acceptance?** The packet's row `O-OPEN` covers *"missing parser coverage"*. The parser
itself is sound for what it does, its 22 tests pass, the binding reproduces byte-for-byte, and the
disassembly in §3–4 was done by `inspect-jsrf.py` — a pre-existing, separately-trusted tool — and I
re-derived it independently. The missing fixture did not corrupt any recorded result.

But §5.8 requires the reviewer to confirm the artifacts *match the commands*, and the packet commands a
test that was not written. The record states "**22 tests, OK**" as if it discharged the requirement; it
discharged 7 of 8 named fixtures. That is an overstatement. I return **DISAGREED** on the parser-coverage
criterion, and note that the honest remedy is one test (or an explicit Planner waiver per §6.1), not a
re-execution.

**Verdict: DISAGREED** — 22 tests pass and 7 of 8 named fixtures are covered; the **mid-instruction
disassembly-start rejection fixture is absent**, and the parser has no disassembly surface for it to test.

---

## 11. Row selection, and whether the closure account is honest

**Was `O-OPEN` correct at selection time?** **Yes.** On the evidence available then, the only in-region
writer found was `0x00149FB6`, a `movzx` of a 16-bit value (max 65535) that cannot produce `0x23B20410`, so
the producer was unbound. `O-OPEN` is defined as *"Any unbound edge/branch … or missing parser coverage"*.
Every other row is correctly excluded at that point:

- `O-IDENTITY` — provenance is clean (XBE matches in both archives and on disk; 94/94 invocations bind).
- `O-APU-INPUT` — no chain binds the local to an APU read; the bridge is a consumer, not a producer.
- `O-OTHER-INPUT` — requires a **positively witnessed** producer; at that moment it was not witnessed.
- `O-SEMANTICS` — `0x801000` is `MEM_COMMIT`, not a reserve (§7), so the reserve defence is unavailable.

**Is the account of the subsequent closure honest?** **Yes — and this is the strongest part of the record.**
The Session did **not** retro-fit the row. It states plainly that `O-OPEN` was selected on the evidence then
available and *"that selection stands as recorded"*, presents the dominance analysis as a later, bounded,
offline, one-function question, marks the superseded section with an explicit
**"⚠ This section SUPERSEDES the `O-OPEN` verdict below"** banner, and keeps the refuted stale-slot
hypothesis visible *"only so the correction is visible"*. It also declines to upgrade to `O-OTHER-INPUT`
even though the chain now has that shape, because the caller's identity — which the packet's own chain
instruction reaches — is not established. That is the correct, conservative reading.

For completeness, on the evidence **now** available the chain does have `O-OTHER-INPUT` shape (positively
witnessed producer, value, PC, arithmetic, branch/ordering). But the record is right not to select it: the
packet's own instruction extends the chain to *"the caller's passed inputs if reached"*, and it is reached
but unidentified. Selecting `O-OPEN` and naming the caller-identification discovery as next is correct and
consistent with the packet's outcome table.

**Verdict: AGREED** — row selection correct at the time; the closure account is honest, not retro-fitted.

---

## Disposition

§5.8's discovery bar has three parts. Artifacts exist; they match the commands; identity is clean; the
producer, dominance, framelessness, arena behaviour, prohibitions and deferrals all reproduce; and the row
was correctly selected with an honest account of the later closure.

Two mandatory items do not clear the bar:

1. **§10 — a required parser fixture is missing.** The packet names seven fixtures plus a
   mid-instruction-disassembly-start rejection; six of seven are covered, the disassembly one exists only
   in a docstring, and the parser has no disassembly surface at all. The record asserts "22 tests, OK" as
   full coverage. `O-OPEN` itself names *"missing parser coverage"* as an `O-OPEN` condition, so this is
   inside the contract, not an outside concern.
2. **§2 — a load-bearing characterisation is false.** A2g is labelled "no-trap" in the evidence record and
   "ABSENT" in `a2h-mechanism.md`, but its `metadata.json` sets `RECOMP_APU_TRAP=1` and its log line 26
   reports the APU trapped. The conclusion survives in the narrower form (not *trace*-caused; predates the
   A4b2 trap work), but the strong form — "the trap is not a necessary cause" — is not established by this
   archive pair, because both runs are trapped.

Per §2.2, `ACCEPT` requires every mandatory criterion `AGREED`; these two are `DISAGREED`, so the
disposition is **`NOT ACCEPTED`**. I do **not** classify either finding as blocking or advisory — that is a
judgment-layer decision (§2.2.6), and I flag both for the Session. Neither is an outside-contract concern;
both sit inside §5.8's "artifacts match the commands / select the recorded row" bar.

**DISPOSITION: NOT ACCEPTED**

### Blocking criteria

| # | Criterion | Disposition | Basis |
|---|---|---|---|
| 1 | Parser tests cover the packet's named fixtures | **DISAGREED** | 22 pass; mid-instruction disassembly-start rejection fixture absent; `a2h-oom-slice.py` has no disassembly surface |
| 2 | Artifacts match the commands — A2g "no-trap" characterisation | **DISAGREED** | `metadata.json` `RECOMP_APU_TRAP=1`; log line 26 "trapped for MMIO"; contradicted by `a2h-mechanism.md:28` and evidence rows 44/55–69 |

### AGREED criteria

Artifacts exist · binding command reproduces byte-for-byte · identity (XBE/game/toolkit) · semantic
identity across the two logs (94/94 sizes identical) · producer formula reproduces **both** sizes ·
dominance/reachability (2 writers; `movzx` does not reach; unreachable with writers removed) · stale-slot
refutation · frameless `sub_001497DC` · arena/`MEM_COMMIT` behaviour · no new run / no toolkit or runtime
change / no instrumentation · prohibitions and deferrals · `O-OPEN` correct at selection time and the
closure account honest.

---

## Advisories (outside the contract — do not change the disposition)

1. **Arithmetic off-by-one (§3).** The record's implied-input lower bounds should be `0x001FFFF1` and
   `0x23B203F1`, not `…F2` (32 consistent values each). Upper bounds and verdicts correct.
2. **Counting conventions (§4).** Reaching-set 47 vs 46, and 1 607 vs 1 609 instructions, are explained by
   one conditional node and two undecodable `.byte` entries respectively. No disagreement about
   reachability; worth a footnote so a future reader does not re-litigate it.
3. **"95 pairs" phrasing.** The record says "94/94 invocations paired"; my independent count is 94 per log
   (paired across logs) — consistent, but the phrasing could read as 95.
4. **The false XBE-mismatch note (`evidence.md:51-53`).** The Session records a case-sensitivity bug in its
   own hash comparison and attributes it to "computing a lowercase digest vs an uppercase expected string".
   I could not reproduce or refute that specific diagnosis (the transient check is not an artifact), but the
   XBE matches, so it is harmless either way. Worth keeping as a process note.
5. **Stronger witness available.** The record argues semantic identity from one row; all 94 sizes are
   identical index-for-index. Using that would make the argument strictly better.
6. **Intraprocedural caveat (§4).** My CFG treats calls as fall-through. Conservative for this question
   (it can only add paths), but a future caller-tracing packet should model them explicitly.
7. **`0x00149E24` is a read-modify-write, not a pure writer.** It both reads and writes `[ebp-0x24]`. It
   does not change the dominance result (removing it still kills reachability, and it is +`0x20` on the
   produced value), but the record's "writers" column slightly under-describes it.

---

## Uncertain

1. **Whether A2g's trap was behaviourally effective.** A2g has `RECOMP_APU_TRAP=1` and the log says
   "trapped for MMIO", but A2g's toolkit revision predates later APU work and A2g's log shows the APU in
   *"STUBBED - passthrough mode"* while R1 shows a pinned DSP core. I confirmed the trap was **armed** and
   the handler was **wired** in the game build of that date (`bd904c2`); I could not, offline, measure how
   much APU behaviour it actually intercepted. **This does not weaken my finding** — armed-and-wired is
   enough to refute "no trap at all" — but the precise behavioural difference between the two runs' traps is
   not measured here.
2. **The correct disposition of the two DISAGREED findings** (blocking vs advisory) is a judgment-layer
   decision I am not permitted to make (§2.2.6).
3. **No run with `RECOMP_APU_TRAP` absent was located.** I checked the two archives named in the contract
   and the most recent ones in `logs/runs/`. I did not census all archives, so I cannot state that no such
   run exists anywhere — only that the pair the packet cites does not include one.

---

## Reproduction commands

```powershell
# packet hash
(Get-FileHash docs\packets\a2h-oom-causal-slice.md -Algorithm SHA256).Hash
(Get-FileHash game\default.xbe -Algorithm SHA256).Hash

# identity
git rev-parse HEAD; git status --porcelain          # game
git -C C:\Users\logic\Repos\xboxrecomp rev-parse HEAD

# parser tests + binding (write --out elsewhere to keep the record untouched)
python -X utf8 -m unittest scripts.test_a2h_oom_slice
python -X utf8 scripts\a2h-oom-slice.py --log logs/runs/20260927-160330-655-a4b2-gp-trap-trace/jsrf_run.log --log logs/runs/20260922-224429-003-a2g-304f0-span/jsrf_run.log --out %TEMP%\review-binding.json

# profiles and dump mapping
python -X utf8 scripts\check-run-profile.py logs/runs/20260927-160330-655-a4b2-gp-trap-trace
python -X utf8 scripts\check-run-profile.py logs/runs/20260922-224429-003-a2g-304f0-span
python -X utf8 scripts\check-dump-mapping.py logs/runs/20260927-160330-655-a4b2-gp-trap-trace

# producer disassembly
python -X utf8 scripts\inspect-jsrf.py disasm 0x001497DC 0x00149820

# A2g trap setting (the refutation)
Select-String -Path logs\runs\20260922-224429-003-a2g-304f0-span\metadata.json -Pattern "RECOMP_APU_TRAP" -Context 0,2
Select-String -Path logs\runs\20260922-224429-003-a2g-304f0-span\jsrf_run.log -Pattern "trapped for MMIO"
```
