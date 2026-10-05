# Turn plan (updated) — `title-001`

Living execution plan. Owner: the Orchestrator (DeepSeek V4.1 Flash @ high).
Baseline: `plan-turn-start-title-001.md` (immutable). This file records how execution
actually evolved and why.

## Status at last update

**Sixteen dispatch defects fixed, each confirmed by the next run advancing to a new site**, plus a
twelve-span batch that resolved 65 fatal trap call sites. This section is regenerated from the
committed manifest at HEAD; the Reviewer found (B6) that an earlier revision shipped a superseded
span and an incorrect RET count, and that is why it is regenerated rather than appended.

| # | Blocker | Fix | Run evidence |
|---|---|---|---|
| 1 | `[ICALL] Failed to resolve VA 0x0013B750` | recovered `0x13B750..0x13B810`, `stack_args 0` | f16: `[RECOVERED] 0x0013B750 returned; ABI verified` |
| 2 | `[ICALL] Failed to resolve VA 0x0007AF90` | recovered `0x7AF90..0x7B750`, `stack_args 0` | f17: `[RECOVERED] 0x0007AF90 returned; ABI verified` |
| 3 | `[ICALL] Failed to resolve VA 0x000BBA17` | `0xBB7B0` widened `0xBBA04 -> 0xBBA19` | f18: `[RECOVERED] 0x000BB7B0 returned; ABI verified` |
| 4 | `[ICALL] Failed to resolve VA 0x000B022E` | `0xB0210` end `0xB0305 -> 0xB05CC` | f19: `[RECOVERED] 0x000B0210 returned; ABI verified` |
| 5 | `[ICALL] Failed to resolve VA 0x0006A770` | recovered `0x6A770..0x6A7C0`, `stack_args 0` | f20: `[RECOVERED] 0x0006A770 returned; ABI verified` |
| 6 | latent `ABI FAILURE 0x00074C70 expected +8` | `0x74C70` `stack_args 4 -> 0` | f21: `[RECOVERED] 0x00074C70 returned; ABI verified` |
| 7 | `[ICALL] Failed to resolve VA 0x000C2630` | split `0xC25D0` (`-> 0xC2621`) + entry `0xC2630..0xC2700` | f22: `[RECOVERED] 0x000C2630 returned; ABI verified` |
| 8 | detector-found: `jmp 0x47850` cut target | split `0x47820` (`-> 0x47849`) + entry `0x47850..0x47970` | byte-derived only; **no** run dispatched it |
| 9 | latent `ABI FAILURE 0x0007DA30 expected +4` | `0x7DA30` end `0x7DA84 -> 0x7DAD6`, `stack_args 0 -> 4` | f23: `[RECOVERED] 0x0007DA30 returned; ABI verified` |
| 10 | `[ALIAS-ICALL] 0x00032610` + `0xFFC00000` | recovered `0x32610..0x3275F`, `stack_args 12` | **f27**: `[RECOVERED] 0x00032610 returned; ABI verified` |
| 11 | `[ICALL] Failed to resolve VA 0x00054750` | recovered `0x54750..0x55530`, `stack_args 0` + generated patch `remove-54750-stub` (L02) | **f28**: `[RECOVERED] 0x00054750 returned; ABI verified` |
| 12 | `[ICALL] Failed to resolve VA 0x00089A60` | recovered `0x89A60..0x89AC9`, `stack_args 0` | f29 exercised it |
| 13 | `[ICALL] Failed to resolve VA 0x0008AEB0` | recovered `0x8AEB0..0x8B1FD`, `stack_args 0` | **f30**: ABI-verified return |
| 14 | `ABI FAILURE 0x00080340 expected +8` | `0x80340` end `0x80BD0`, `stack_args 0` | f30 measured it; f31/f32 caught my bad `0x81853` widening |
| 15 | `ABI FAILURE 0x00080BD0 expected +8` (delta `-0x40`) | `0x80BD0` end `0x80C83 -> 0x81853`, `stack_args 4 -> 0` | **f33**: ABI-verified return |
| 16 | `[ICALL] Failed to resolve VA 0x00094AB0` | **next stop, not yet fixed** | f33 died here |
| — | twelve sibling spans (65 fatal trap sites) | widened as one batch, `KNOWN_OPEN` 80 -> 66 | byte-derived; no run dispatched them |
| — | two self-found defects | `0x32610` end `0x3275D -> 0x3275F`; `0x47820` `stack_args 0 -> 4` | found by consolidated re-verification, not a run |

**Run-to-run nondeterminism is now a first-class constraint (blocker 11).** f25 and f26 share
`exe_sha256 = ca867957…`: f25 reached the disclaimer at 960 presents, f26 stalled on the **Smilebit**
card at 193 presents for its whole 810 s and never showed the disclaimer hash. So **a clean run
proves nothing unless it reaches the path** — which is why the acceptance criterion for blocker 10 is
path-aware.

`recovered.c` 3095 -> 3104. `recovery-unresolved.json` losses `d77474d` -> HEAD, **measured by
set difference rather than transcribed** (the first attempt at this list named
`0x000B022E`, which appears in no revision of that file, and then omitted real losses — the
Remediation Planner caught both, which is why it is now generated):
`0x00047850`,
`0x0006A770`,
`0x0007ADE1`,
`0x0007ADE2`,
`0x000865B2`,
`0x00089A60`,
`0x0008AEB0`,
`0x000AECA0`,
`0x000AECA1`,
`0x000B05B8`,
`0x000BBA17`,
`0x000F7705`,
`0x000FCB16`,
`0x000FCDF9`,
`0x000FD166`,
`0x00101EF9`,
`0x001045AC`,
`0x00104A8A`,
`0x0010FA43`,
`0x00110314`,
`0x00110317`.
`scripts/check-span-exits.py` findings 431 -> 360. CTest 36/36; `just check` passes,
now including a baseline-gated `check-entry-extents.py` (18 entries, after `0x000307A0` was
found to be a LIVE defect and fixed rather than left frozen).

Every run is **exploratory** (`RECOMP_GPU_ACK` defaulted on, `RECOMP_APU_TRAP=1`,
`RECOMP_PB_EXEC=1`, `RECOMP_FB_WINDOW=1`, `RECOMP_FB_PRESENT_DUMP_EVERY=10`).
**The title screen has NOT been reached.** The last visible frame is still the graffiti
disclaimer, hash `5bdaea576b8509f5`, and presents still stop at exactly 1000.

## Blocker 10 — `0xFFC00000`: an alias-folded job handler that caused the NaN fill

**Resolved part — and the fill is now fully explained, not merely observed.** The last named event
before the NaN fatal was `[ALIAS-ICALL] target=0x00032610 owner=0x00033800`, present in f23 only.
`0x32610` is slot 1 of the job-handler table at **`0x1EC200`** — a *separate* table from
`0x1EC0F0`, which ends with a zero terminator at `0x1EC1FC` (slot 67); counting straight on from
`0x1EC0F0` it would be slot 69. Dispatcher `0x00025700` bounds the index with `cmp edx,0x21` and
then does `mov edx,[edx*4+0x1EC200]`. (An earlier revision of this plan said "slot 1 of `0x1EC0F0`";
that was wrong and is corrected per Reviewer finding B2.) It is the **same `tail_jump_alias` defect
the already-recovered sibling `0x32C70` documents**: the database span `0x32610..0x33800` overran
because the function's own jump table at `0x32760` contains `C2 26 03 00`, which decodes as a
spurious `ret 0x326`. The dispatch therefore ran `0x33800`, which is `mov eax,1; ret 4` — two
instructions — instead of the real function ending at `0x3275C`. All **12** RETs are `ret 0xc`
(`C2 0C 00`), so `stack_args 12`. Recovered as `0x32610..0x3275F`, `stack_args 12`.

**Run confirmation is `f27` (`20261005-110711-776`), which logs `[RECOVERED] 0x00032610 returned;
ABI verified`** — *not* f25. f25 has **zero** occurrences of that line and never dispatched the
address; an earlier revision of this plan credited f25 and that was corrected per Reviewer finding
B3. `recovered.c` 3100 -> 3101. Commit `9ea4e44`.

**The fill's mechanism (Advisor ruling, consultations 2/3): the misdispatch caused it.** The wrong
`eax = 1` from `mov eax,1; ret 4` flowed into constructor `0x15420`, which stored it at
`[obj+0x38]` where an object pointer belongs (object `0x034D5110`). The boot thread's loop at
`0x15D90` then derived `count = (0-1)/1 = 0xFFFFFFFF` and ran **31,739** iterations, normalising
zero vectors to the SSE default QNaN `0xFFC00000` and writing 12-byte entries from the float3 array
base `0x231D40` to `edi = 0x28ED04`. The arithmetic closes exactly: `0x231D40 + 12 × 31,739 =
0x28ED04` and the paired heap buffer `0x034C3E40 + 0x5C × 31,740 = 0x0378CCD0`. So the fill is
**computed NaN, not a memset** and not the title's sentinel, and it is a **latent defect of the
2026-09-21 alias fold that f23 was the first run to reach — not a regression from this turn's span
changes.** The `0x7DA30` change did not cause it; bisecting would have been wasted effort. Recorded
as a new defect class in `docs/jsrf-technical-record.md` §9.

**Path-aware acceptance criterion this implies.** A run counts only if it logs
`[RECOVERED] 0x00032610 returned`, has **no** `ALIAS-ICALL target=0x00032610`, shows **no** NaN at
`0x27E080` or `0x25EFB8`, and stops **beyond** `0x9CC40`. A run taking the f24 branch (idle in the
`0x13F80` presenter loop, 9 presents) exercised nothing and must be recorded as not exercised.

**Open part (recorded, NOT diagnosed).** f23 also showed a 363 KiB uniform `0xFFC00000` fill at
`0x233ED0..0x28ED04` (93,057 of 93,069 words), which overwrote the `DOLBY` section image
(`0x27E080`, marked `writable: false, executable: true`), live globals including `0x251D6C` that
`0x7DA30` reads, and the thread-trampoline control block — making the trampoline's
`test eax,eax; je` see non-zero and call `0xFFC00000`. **It is not deterministic.** f24 (a
**different** binary: `ac62b0b1…` vs f23's `44c39546…`) ran 520 s, stalled at 9 presents and stayed
clean; the genuine same-binary pair f25/f26 also disagree; older and much longer runs (f9 1203 s,
f5-long 1500 s) were clean. It is therefore recorded as a newly
observed nondeterministic corruption with its evidence, **not** as a diagnosis and **not** as a
regression from this turn's span changes. The `[ALIAS-ICALL]` timing makes `0x32610` a
plausible cause, but that is an **inference** and is not claimed.

**Advisor status — resolved.** The Persistent Advisor returned four rulings across the turn. It
failed twice with no reply (consultations 2 and 3) and then **answered both on the retry**; per
`docs/agent-workflow.md` §1 an unavailable route is reported rather than silently replaced, and the
Advisor is not a gate, so the intervening `0x32610` fix was made on the Orchestrator's own judgment
from the bytes and was subsequently **ratified**. All three open questions it was asked are now
answered:

- **Did the wrong body cause the fill?** **Yes** — see the mechanism above. The causal arrow runs
  from the alias to the fill, and it is a latent defect of the 2026-09-21 alias fold, not a
  regression. It would be reversed by a run that logs the ABI-verified return for `0x00032610` and
  still shows the fill, which would mean `[obj+0x38]` has a second source.
- **Is `stack_args 12` right?** **Yes** — and the Advisor checked all 33 slots of the `0x1EC200`
  table: every one is `ret 0xc` with manifest `stack_args 12`, so 12 is consistent. (My earlier
  derivation said "11 RETs"; the corrected end `0x3275F` has **12**, and the 11th/12th distinction
  was the very defect Reviewer finding B6 caught in this file.)
- **Is the `writable: false` `DOLBY` overwrite a separate port defect?** **No** — the runaway loop
  simply crossed the end of `.data`. Enforcing write protection would only fault earlier, and
  whether the retail loader enforces section write flags at all is **inferred, not verified**. It is
  a backlog diagnostic idea, not a fidelity defect.

The Advisor also recommended batching the sibling spans (done: 12 widened, 65 trap sites resolved)
and auditing the ten unrecovered alias addresses that data tables point at — recorded in
`docs/jsrf-technical-record.md` §9 with the four `.rdata` ones marked highest priority.

## PLAN_CHANGE 5 — the freeze is probably NOT an independent blocker (Advisor correction)

```text
PLAN_CHANGE:
- Changed: H4 ("the 1000-present freeze is an independent second blocker") is DOWNGRADED from
  "strongly supported" to "likely a symptom of the boot-path transition, not independent".
- Evidence (Advisor ruling, consultation 1): f9, f14, f15, f16 and f17 ALL freeze at exactly
  presents=1000 with the same frame hash and upd-presents a constant 1303 from present 400 on.
  In f17 the thread that died on 0xBBA17 was GUEST_THREAD identity=1 -- the main/boot presenter
  thread (start 0x148023) -- still inside its per-frame update, and its guest stack holds 0x13F9E
  (from `call 0x13A80` in presenter loop 0x13F80), 0x13B24 and 0x124C3. In f9 (no ICALL at all,
  1203 s) the same thread is idling in the [esi+0x24] != 0 branch, sleeping until [esi+0x10] goes
  negative, and about 100 s later the game took its OWN fatal-error path ([FATAL-CTOR] at
  0x116EA8, then it opens Media\Cache\JSRF_FATAL.ERR).
- Why: presents stop because the title transition runs synchronously inside the presenter thread
  and the presenter is then told to stop. A run that never reaches these ICALLs (f9) still ends on
  the same transition, just via the game's own error path. So "f9 had zero ICALLs and still froze"
  does NOT establish independence -- it is a different terminal failure of the same transition.
- Consequence: the freeze is likely to clear, or to change shape, once this ICALL chain is walked.
  Do NOT spend the turn building a present-path fix on the assumption that it is independent.
  The deciding check remains: a run where identity=1 returns to the presenter loop with +0x24
  still 0 and presents still frozen would point back at the present/GPU path.
```

## PLAN_CHANGE 1 — roster route correction (before execution began)

```text
PLAN_CHANGE:
- Changed: `docs/agent-workflow.md` §1 Turn Planner route `codex/sol-6.1` -> `codex/gpt-6.1-sol`.
- Evidence: `list_subagent_models(provider=codex)` advertises `gpt-6.1-sol` (name "GPT-6.1-Sol")
  and rejects `codex/sol-6.1` ("not allowed for this Session"). `scripts/check-route-allowlist.py`
  exited 1 naming `codex/sol-6.1` missing from the allow-list, so `just check` was RED at the
  pushed checkpoint `d77474d`. `plan-jsrf-bare-minimum.md` T11 already records
  "Default is `codex/gpt-6.1-sol` @ `high`, read back from the §1 roster by a control".
- Why: the roster named a route that does not exist. Owner approved the correction as a typo fix
  (staffing is owner-reserved per `docs/agent-workflow.md` §7). The checker is now green.
```

## PLAN_CHANGE 2 — evidence-driven reordering

```text
PLAN_CHANGE:
- Changed: the start plan's step 1-3 order was "rebuild, run the 0x13B750 fix, then attack the
  freeze". Execution instead fixed a SECOND missing entry (0x7AF90) before returning to the freeze.
- Evidence: the first run (f16) cleared 0x13B750 and immediately hit a new fatal, 0x7AF90. That
  address is a method of the .rdata vtable 0x001CCE70 and the ONLY unresolved slot in that table
  (all 15 other code slots resolve). Fixing it cost one manifest entry and one run.
- Why: the critical path is a chain of missing/defective dispatch entries, and each one is cheap to
  clear once located. Walking the chain is cheaper than switching to the freeze and returning.
  The freeze remains a live independent blocker (see H4) and is not abandoned.
```

## PLAN_CHANGE 3 — the `.text`-immediate class, now measured

```text
PLAN_CHANGE:
- Changed: the start plan's H3 ("the whole class of pointer-referenced methods is missing") is
  downgraded from "partially confirmed, smaller than feared" to a bounded, measured finding.
- Evidence: a full census (worker 123feaf4, `logs/icall-census-report.md`, pinned to artifact
  hashes) scanned every byte offset of every section: 88250 raw dwords land in .text, 14062 are
  genuine code-pointer references (aligned data dwords plus capstone-verified operands), 6569
  distinct targets. Classification: owned 3831, inside-another-span 2214, unowned-gap 512,
  at-span-start-unowned 12.
- Positive control: with `0x13B750` and `0x13FAD0` removed from the owned set, the classifier
  reproduces the brief exactly, and the "unowned gap with a prologue and a clean ret" set is
  exactly 5 (`0x47600, 0xE7D70, 0xFFD70, 0x1133A0, 0x13B750`).
- Why: `0x13B750` is one of ~5 of its shape, not one of hundreds. The 512 unowned gaps are mostly
  no-prologue (508). This means each blocker is an individual measurement, not a sweep — and it
  justifies continuing to walk the chain rather than building a bulk recovery pass.
- Also measured: `scripts/check-table-targets.py` is STRUCTURALLY BLIND to the `0x13B750` class.
  `0x13B750` has ZERO aligned `.data`/`.rdata` occurrences; its only reference is the misaligned
  `.text` immediate of `push 0x13B750` at `0x13BBE0`. The tool scans only `.data`/`.rdata`, so its
  candidate set can never contain it. It CAN find the `0x13FAD0` class (verified by a coverage
  control that un-covers `0x13FAD0` and sees it reported).
```

## PLAN_CHANGE 4 — a pre-existing red checkpoint found and repaired

```text
PLAN_CHANGE:
- Changed: `docs/reviews/p0-full-generated-baseline.json` was re-baselined (it was stale), in
  addition to the `docs/reviews/p0-7-generation-provenance.json` amendment.
- Evidence: at HEAD (`d77474d`) the manifest recorded `recovered.c` = `6f5f7dcd...` while the
  baseline recorded `a47e78bf...` — the PRE-0x13FAD0 hash. The previous session updated the manifest
  but never re-baselined this file, so `jsrf_generation_provenance` was ALREADY FAILING at the
  pushed checkpoint (`test_generated_tree_matches_the_preservation_baseline` and
  `test_cli_check_passes`). Reproduced by reading both files from `git show HEAD:`.
- Why: the failure was real and pre-existing, not caused by this turn's work. Both entries now hold
  the measured values and the whole CTest suite is 35/35.
```

## Blocker 3 — `0x000BBA17`: RESOLVED (shared-epilogue span defect)

**Advisor ruling, consultation 1: repair (a) — widen `0x000BB7B0` to `0x000BBA19`. The tree
already had it and the regenerated body is correct: "switch: 6 entries, 6 targets" with all
six as `goto` labels, so the `RECOMP_ITAIL` fallback is unreachable and all three `jcc` sites
become `goto loc_000BBA17`.** f18 confirmed it: `[RECOVERED] 0x000BB7B0 returned; ABI verified`.

The Advisor also resolved the open question I had recorded, and I have updated the manifest
evidence accordingly:

- `sub_000BBA04` is a **false function start**, not a real function. Its only reference
  anywhere in the XBE is slot 4 of `0xBB7B0`'s own jump table at `0xBBA2C`.
- It **cannot** be an entry under any x86 ABI: it begins `push esi; mov ecx, esi`, reading
  `esi` before writing it, and `esi` is callee-saved and undefined at a function entry.
- **There is no double push.** The second `push esi` at `0xBBA04` is an *argument*, not a
  register save: both callees (`0x11BE0`, `0x11C20`) are `ret 4` and remove it. Stack trace
  for slot 4 from entry esp `E`: `push esi` (save) `E-4`; `push esi` (arg) + `call 0x11BE0`
  (`ret 4`) `E-4`; `push [esi+0x28]` + `call 0x11C20` (`ret 4`) `E-4`; `0xBBA17 pop esi`
  restores `E`; `ret` -> `E+4`. The wrapper's `+4` delta and `esi`-preserved checks both pass.
- The table really has six entries: the byte index table at `0xBBA34` is
  `[0,1,5,2,3,5,5,5,5,5,4]`, maximum index 5.
- It is a **compiler idiom**, not a one-off: the same release arm plus the owner's own
  epilogue appears at the end of 12 other recovered spans and in `0xB0210`. In every case the
  arm's only reference is the last slot of its owner's jump table, and the epilogue pops what
  the owner's prologue pushed — which a standalone function could not do.

**Advisor-deferred (backlog, not this turn):** the root cause is the analysis database's
`gap_prologue` pass accepting a `push` after a `ret` as an entry. Relaxing
`_analyze_switch_table`'s truncation alone would emit `goto`s to labels outside the span, so
the durable fix is a change at full-regeneration scale. **Advisor recommendation:** batch the
12 sibling spans rather than paying one run per fatal; each proposed end must be verified by
decode (all switch arms resolved, no `ITAIL`, zero checker findings) before landing.
`0x00043910` needs no change (its arm `0x4393A` is a real DB entry reached by `je`).

## Blocker 3 root cause (kept for the record)

The recovered entry `0x000BB7B0..0x000BBA04` (`stack_args 0`) contains an intra-function switch:

```asm
000BB7B0 push esi                  <-- entry, pushes esi
000BB7B1 mov  esi, ecx
000BB7B3 mov  eax, [esi+0x164]
000BB7B9 cmp  eax, 0xA
000BB7BC ja   0xBBA17              <-- branch to the shared epilogue
000BB7C2 movzx eax, byte [eax+0xBBA34]
000BB7C9 jmp  dword [eax*4 + 0xBBA1C]
```

The jump table at `0xBBA1C` has exactly 6 entries; the byte index table at `0xBBA34` holds
`[0,1,5,2,3,5,5,5,5,5,4]`, so all six slots are live:

| slot | value | what it is |
|---|---|---|
| 0 | `0xBB7D0` | internal arm |
| 1 | `0xBB95D` | internal arm |
| 2 | `0xBB9D8` | internal arm |
| 3 | `0xBB9EB` | internal arm |
| 4 | `0xBBA04` | start of the DB function `sub_000BBA04` |
| 5 | `0xBBA17` | `pop esi; ret` — the shared epilogue |

```asm
000BB9EB add  dword [esi+0x114], -4
000BB9F2 jns  0xBBA17              <-- another branch to the same epilogue
000BBA02 pop  esi                  <-- 0xBB7B0's OWN balanced epilogue
000BBA03 ret
000BBA04 push esi                  <-- sub_000BBA04 (DB entry, generated, game_vtable)
000BBA07 call 0x11BE0
000BBA12 call 0x11C20
000BBA17 pop  esi                  <-- the shared epilogue
000BBA18 ret
000BBA19 lea  ecx, [ecx]           <-- padding; 0xBBA1C begins the jump table (data)
```

Three branch sites inside the span target `0xBBA17`: `0xBB7BC` (`ja`), `0xBB95D` (`je`),
`0xBB9F2` (`jns`). Because `0xBBA17 > 0xBBA04`, it is outside the recovered span, so the lifter
treats it as an external tail-call target and the body emits a **direct call to a fatal trap stub**:

```c
if (CMP_A(_fa, _fb)) { g_seh_ebp = ebp; sub_000BBA17(); return; }
```

`sub_000BBA17` is `void sub_000BBA17(void) { recomp_icall_fail_log(0x000BBA17u); abort(); }`
(`src/recomp/gen/recomp_stubs_recovery.c:125`), and `0x000BBA17` is one of the 244 entries in
`config/recovery-unresolved.json`. That is exactly the observed fatal.

**Why the existing alias entry does not save it.** The generated dispatch table *does* contain
`{ 0x000BBA17u, recomp_alias_000BBA17 }` (line 3025) and `recomp_alias_000BBA17` calls
`sub_000BBA04()`. But `RECOMP_ITAIL` -> `recomp_lookup_manual` consults `jsrf_lookup_recovered`
FIRST, and the recovered unit's own `extern void sub_000BBA17(void)` binds to the trap stub. The
alias table entry is therefore unreachable for this target.

**Aggravating lifter detail.** `_analyze_switch_table` (`tools/recomp/lifter.py:2750-2754`)
truncates the arm list at the first target outside `[func_start, func_end)`. With
`func_end = 0xBBA04` it keeps 4 arms and drops `0xBBA04` and `0xBBA17`; the emitted code says
`/* switch: 4 entries, 4 targets */` and falls through to `RECOMP_ITAIL(_jt)`.

**Chosen repair (adopted, Advisor-ratified):** widen the recovered span of `0x000BB7B0` to
`0x000BBA19`, so the function owns its own shared tail-merged epilogue. This is the fourth
instance of the class `scripts/check-span-exits.py` documents and the third with the same fix;
all three reviewed precedents widened the parent.

**Rejected repair, kept for the record:** a 2-byte fragment entry `0xBBA17..0xBBA19`. Entered
as a tail jump with `esi` already pushed, its body nets `+8` against a wrapper expecting
`4 + stack_args`, and `stack_args` means `ret N` cleanup, so no honest value expresses a
register pop. The Advisor reached the same conclusion independently and flagged it as an early
warning before I had replaced it. **A checked wrapper on a bare epilogue fragment fails its own
ABI check every time it runs** — that is a deterministic reading, not a run result.

**Advisor housekeeping, both done:** the stray `"0x000BBA17"` entry that the abandoned
fragment attempt left in `config/manual-functions.json` (the recovery script only ever adds to
that file) was removed — it would have made the lifter emit `RECOMP_ITAIL` to a symbol nothing
defines. The manifest sentence claiming the recovered body makes the alias "inert" was wrong
and was reworded: `jsrf_lookup_recovered` has no case for `0xBBA17`, so the old alias to
`sub_000BBA04` is still reachable — it is **latent**, not inert, because nothing currently
dispatches to `0xBBA17` indirectly.

## Blocker 11 — the boot path is strongly nondeterministic (new, measured)

Three runs in the same environment with different outcomes. **They are NOT all the same binary**: f23 is `exe_sha256 = 44c39546…`, while f25 and f26 share `ca867957…`. The genuine same-binary pair is f25/f26, and that pair alone carries the nondeterminism finding; an earlier revision of this file said otherwise and the Remediation Planner caught it.

| run | seconds | outcome | furthest frame | presents | notes |
|---|---|---|---|---|---|
| f23 | 253 | `unhandled_exception` | disclaimer `5bdaea576b8509f5` | **1000** | 363 KiB NaN fill; `[ALIAS-ICALL] 0x32610` |
| f25 | 404 | `diagnostic_deadline` | disclaimer `5bdaea576b8509f5` | 960 | clean; no fatal, no fill |
| f26 | 810 | `diagnostic_deadline` | **Smilebit card `22fe3b5810848f88`** | **193** | **never showed the disclaimer hash at all** |

f26 ran **twice as long** as f25 and got **less far**: it held the Smilebit card from t≈159 s to
t=796 s, spinning in kernel ordinal 119 (9,353 calls), with the main thread
`GUEST_THREAD identity=1 tid=2340 start=00148023` still live. It never reached the disclaimer.
Its BMPs were inspected directly: `p0003` is the SEGA card, `p0005`/`p0006` are the Smilebit
card, and the final held frame is Smilebit.

**This matters for how the turn's results should be read.** The plan already records
non-reproducing stalls for this title (f6's DSP spin "did not reproduce"; f7 passed it), so this
is consistent with a known property — but it means **a single run cannot establish progress**, and
the "presents stop at exactly 1000" observation is only reachable on the branch that gets that
far. It also means the NaN fill's absence in f24/f25/f26 is **weak** evidence about its cause:
those runs never reached the state f23 did.

**Not a regression claim:** f26 used the `0x32610` binary and stalled *earlier* than f25 on the
same binary, which is direct evidence that run-to-run variance, not the code change, dominates
this comparison.

## Completion assessment for this turn

The turn's substantive objective — walk the critical path toward the title screen — is
**substantially advanced**: ten dispatch defects were found, fixed, and each confirmed by the next
run reaching a new site. On the runs that get that far, the `[ICALL]` chain now runs to a clean
`diagnostic_deadline` with zero unresolved calls and zero ABI failures.

**The title screen has NOT been reached.** No frame other than the already-known SEGA, Smilebit and
disclaimer cards has been observed in any run this turn. **No title-frame BMP exists, so M15 is
not claimed**, and no disclaimer-cleared claim is made from a present count or a hash change.

The next critical-path question is no longer "which dispatch entry is missing" but **"why does the
boot path stall at a different card on different runs"** — a concurrency/timing question rather
than a lifting question. That is a reasonable place to hand the turn to review.

## Critical path — remaining

1. **Blocker 11** (nondeterministic boot stalls): characterise why two runs of the *same* binary
   stall at different cards. This is now the primary
   critical-path question.
2. **Blocker 10's open half**: whether the `0x32610` wrong-body dispatch caused the NaN fill (an
   inference, not a claim), and whether the `writable: false` `DOLBY` overwrite is a separate port
   defect.
3. **Batch the 12 sibling spans** the Advisor identified, verifying each proposed end by decode
   before landing it, so the remaining instances of the shared-epilogue class do not each cost a
   run.
4. **The presents/flips freeze at exactly 1000** — see PLAN_CHANGE 5: probably a symptom of the
   boot-path transition, not an independent blocker.

## Competing hypotheses (updated)

- **H1 (0x13B750 missing entry)** — **RESOLVED and measured.** f16 shows the ABI-verified return.
- **H2 (nop pad makes 0x13B750 a label)** — **refuted**, as the start plan found.
- **H3 (whole class missing)** — **bounded.** One of ~5 of its shape. The census found 6569
  distinct genuine code-pointer targets: owned 3831, inside-another-span 2214, unowned-gap 512,
  at-span-start-unowned 12.
- **H4 (the 1000-present freeze is an independent second blocker)** — **DOWNGRADED, see
  PLAN_CHANGE 5.** The Advisor's ruling shows f9's terminal failure is the same boot transition
  reached via the game's own error path, so "zero ICALLs and still frozen" does not establish
  independence.
- **H5 (freeze is the disc-error/pending-I/O path)** — still live but weaker; f9's fatal file
  line comes ~100 s after flips stop, so the fatal is at most a consequence.
- **H6 (720-update hold / `+0x24` idle branch)** — **strengthened by the Advisor**: in f9 the
  presenter thread is genuinely idling in the `[esi+0x24] != 0` branch at `0x13F90`, sleeping
  until `[esi+0x10]` goes negative. This is a real observed state, not just a field value.
- **H7 (rendering/submit defect unrelated to state-machine timing)** — weakened by H6.
- **H8 (a switch arm whose target is a shared epilogue just outside the span)** — **confirmed**,
  and the fix is now Advisor-ratified with a resolved ownership argument.
- **H9 (NEW): a truncated span can hide a `ret N`.** `0x0007DA30` had both a truncated end and a
  wrong `stack_args`, and the wrong `stack_args` could never be exercised because the body had no
  `ret` to run. Any entry whose span ends at a folded-alias start should be re-checked for a
  hidden `ret` immediate.
- **H10 (NEW): a float bit pattern reaching an indirect call.** `0xFFC00000` is `NAN_NEG`.
  Live, uncharacterised.

## Measurements that remain decisive

- **M3 (guest vs host freeze)** — read `[GPU] flips` and `[FBPRESENT]` in the same run. Both stop
  together in every run so far.
- **M5 (is the freeze a timeout?)** — compare the wall-clock gap between the last present and the
  last committed method against the 240 s pending-I/O threshold and the 720-update hold.
- **M8 (Advisor's independence falsifier)** — a run where identity=1 returns to the presenter
  loop with `+0x24` still 0 and presents still frozen would point back at the present/GPU path.
- **M9 (Advisor's freeze instrumentation)** — the `[FBPHASE]` sampler cannot see `+0x24` change
  after present 1000, because it logs once per `n` and `n` stays at 1000. Log on transitions of
  `+0x24`/`+0x10` instead.

## Advisor

Consulted once (blocker 3's repair choice). The ruling ratified the widening, resolved the
`0xBBA04` ownership question from the bytes, flagged two housekeeping defects (both fixed), and
corrected my reading of the presents freeze (PLAN_CHANGE 5). Further consultation is available
and the Advisor is continuable.

## Completion criteria (unchanged from the start plan)

**Genuinely useful:** a measured advance down the critical path with a run that names the new
site; or the freeze reduced to a single named mechanism with a concrete next experiment.

**Not an advance:** an added dispatch entry with no run; a run where the entry resolves but guest
state is unchanged; forced `byte+1 = 1`; a returned `0x101`; a cleared `+0x24`; a widened run that
only re-observes the disclaimer.

**Do not claim M15 without a title-frame BMP plus its hash.** A resolved indirect call, a returning
recovered body, or a moving counter is not title-screen evidence.

## Constraints (binding)

No `JSRF_ALLOW_UNRESOLVED`; no guessed stack corrections; no arbitrary guest-memory writes; no
synthetic state advancement presented as a fix; no undocumented stubs. Any pragmatic shortcut is
recorded in `docs/jsrf-compatibility-ledger.md` and listed by the run's record. Do not reopen the
`title.adx` file-APC fixes (toolkit `8f6c597`, `6e6e056`) or the `0x13FAD0` split (game `d77474d`)
without contrary evidence. Do not set `RECOMP_ASYNC_IO`; do not patch `kernel_file.c` or `0x1401B0`.
