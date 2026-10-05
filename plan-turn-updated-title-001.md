# Turn plan (updated) — `title-001`

Living execution plan. Owner: the Orchestrator (DeepSeek V4.1 Flash @ high).
Baseline: `plan-turn-start-title-001.md` (immutable). This file records how execution
actually evolved and why.

## Status at last update

**Nine dispatch defects fixed, each confirmed by the next run advancing to a new site.**

| # | Blocker | Fix | Run evidence |
|---|---|---|---|
| 1 | `[ICALL] Failed to resolve VA 0x0013B750` | recovered `0x13B750..0x13B810`, `stack_args 0` | f16: `[RECOVERED] 0x0013B750 returned; ABI verified` |
| 2 | `[ICALL] Failed to resolve VA 0x0007AF90` | recovered `0x7AF90..0x7B750`, `stack_args 0` | f17: `[RECOVERED] 0x0007AF90 returned; ABI verified` |
| 3 | `[ICALL] Failed to resolve VA 0x000BBA17` | widened `0xBB7B0` `0xBBA04 -> 0xBBA19` | f18: `[RECOVERED] 0x000BB7B0 returned; ABI verified` |
| 4 | `[ICALL] Failed to resolve VA 0x000B022E` | `0xB0210` end `0xB0305 -> 0xB05CC` | f19: `[RECOVERED] 0x000B0210 returned; ABI verified` |
| 5 | `[ICALL] Failed to resolve VA 0x0006A770` | recovered `0x6A770..0x6A7C0`, `stack_args 0` | f20: `[RECOVERED] 0x0006A770 returned; ABI verified` |
| 6 | latent: `ABI FAILURE 0x00074C70 expected +8` | `0x74C70` `stack_args 4 -> 0` | f21: `[RECOVERED] 0x00074C70 returned; ABI verified` |
| 7 | `[ICALL] Failed to resolve VA 0x000C2630` | split `0xC25D0` (end `-> 0xC2621`) + new entry `0xC2630..0xC2700` | f22: `[RECOVERED] 0x000C2630 returned; ABI verified` |
| 8 | detector-found: `jmp 0x47850` cut target | split `0x47820` (end `-> 0x47849`) + new entry `0x47850..0x47970` | not yet run in isolation; f23 loaded it |
| 9 | latent: `ABI FAILURE 0x0007DA30 expected +4` | `0x7DA30` end `0x7DA84 -> 0x7DAD6`, `stack_args 0 -> 4` | f23: `[RECOVERED] 0x0007DA30 returned; ABI verified` |

`recovered.c` 3095 -> 3100. `recovery-unresolved.json` lost `0x000BBA17`, `0x000B022E`,
`0x0006A770`. `scripts/check-span-exits.py` findings 431 -> 428, and **none** of the eleven
entries touched this turn is a finding. CTest 35/35. All checkers pass.

Every run is **exploratory** (`RECOMP_GPU_ACK` defaulted on, `RECOMP_APU_TRAP=1`,
`RECOMP_PB_EXEC=1`, `RECOMP_FB_WINDOW=1`, `RECOMP_FB_PRESENT_DUMP_EVERY=10`).
**The title screen has NOT been reached.** The last visible frame is still the graffiti
disclaimer, hash `5bdaea576b8509f5`, and presents still stop at exactly 1000.

## Blocker 10 — `0xFFC00000`, a new class, NOT yet characterised

f23 cleared `0x7DA30` and then reached two `[ICALL] Failed to resolve VA 0xFFC00000` failures
(tid 35064 and 33952). `0xFFC00000` is not a plausible code address: it is the bit pattern of
a negative quiet NaN, and the toolkit already names it (`nv2a_pb_exec.c:3000`, `NAN_NEG`).
The ICALL history immediately before it is `... 0x00141A00, 0xFE000104, 0xFE000100,
0xFE00011C, 0xFFC00000`, i.e. a kernel-thunk path. **This is a different defect class from
the nine above** — not a missing dispatch entry, and it must not be treated as one. Whether
it is a float consumed as a pointer, a corrupted vtable slot, or a symptom of the presents
freeze is **UNVERIFIED**. It needs its own investigation and is the immediate next task.

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

## Critical path — remaining

1. **Blocker 10** (`0xFFC00000`): characterise it before treating it as a dispatch defect. It is
   a NaN bit pattern, not a code address.
2. **Batch the 12 sibling spans** the Advisor identified, verifying each proposed end by decode
   before landing it, so the remaining instances of the shared-epilogue class do not each cost a
   run.
3. **The presents/flips freeze at exactly 1000** — see PLAN_CHANGE 5: probably a symptom of the
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
