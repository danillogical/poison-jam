# Turn plan (updated) — `title-001`

Living execution plan. Owner: the Orchestrator (DeepSeek V4.1 Flash @ high).
Baseline: `plan-turn-start-title-001.md` (immutable). This file records how execution
actually evolved and why.

## Status at last update

**Two blockers fixed, each confirmed by a run. The turn has advanced twice.**

| # | Blocker | Fix | Run evidence |
|---|---|---|---|
| 1 | `[ICALL] Failed to resolve VA 0x0013B750` | recovered `0x0013B750..0x0013B810`, `stack_args 0` | f16 `20261005-000031-102-f16-b750`: `[RECOVERED] 0x0013B750 returned; ABI verified` |
| 2 | `[ICALL] Failed to resolve VA 0x0007AF90` | recovered `0x0007AF90..0x0007B750`, `stack_args 0` | f17 `20261005-000908-412-f17-vtable`: `[RECOVERED] 0x0007AF90 returned; ABI verified` |
| 3 | `[ICALL] Failed to resolve VA 0x000BBA17` | **in progress** — shared-epilogue span defect, see below | f17 died here |

Every run so far is **exploratory** (`RECOMP_GPU_ACK` defaulted on, `RECOMP_APU_TRAP=1`,
`RECOMP_PB_EXEC=1`, `RECOMP_FB_WINDOW=1`, `RECOMP_FB_PRESENT_DUMP_EVERY=10`).
**The title screen has NOT been reached.** The last visible frame is still the graffiti
disclaimer, hash `5bdaea576b8509f5`.

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

## Blocker 3 — `0x000BBA17`: a shared-epilogue span defect (root cause established)

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

**Chosen repair (minimal, keeps ownership honest):** add a recovered entry
`0x000BBA17..0x000BBA19`, `stack_args 0`. The three branch sites then tail-call a real body that
does exactly `pop esi; ret`.

Why this rather than widening `0x000BB7B0` to `0xBBA19`: `sub_000BBA04` is a genuine separate
function (its own `push esi` prologue, referenced by the switch, and classified `game_vtable` by
the generator). Widening `0x000BB7B0` over it would recreate the exact `0x13FAD0` defect class in
reverse — one function's span swallowing another's. The chosen repair leaves `sub_000BBA04`
untouched and adds only the shared epilogue as a resolvable target.

**Recorded caveat (must be measured, not assumed).** Switch slot 4 (`0xBBA04`) is reached as a
tail jump from `0xBB7B0` *after* `0xBB7B0` has already pushed `esi`, so if guest state
`[esi+0x164] == 10` is reachable, that path pushes `esi` twice and pops it once. The run is the
falsifier: if slot 4 is taken and unbalanced, the ABI check reports it. State 10 may be
unreachable in this title, which would explain why the shipped game worked. This is recorded as an
open question, not a claim.

## Critical path — remaining

1. **Blocker 3** (`0xBBA17`): apply the entry, regenerate, rebuild, run. Falsifier: a new fatal or
   an ABI failure.
2. **Blocker 4+**: keep walking the chain while each step is cheap. Each cleared entry is one
   manifest line, one regeneration, one run.
3. **The presents/flips freeze at exactly 1000** — independent, and the more likely owner of the
   title screen. See H4 below. Worker `fe438413` is measuring it.

## Competing hypotheses (updated)

- **H1 (0x13B750 missing entry)** — **RESOLVED and measured.** f16 shows the ABI-verified return.
- **H2 (nop pad makes 0x13B750 a label)** — **refuted**, as the start plan found.
- **H3 (whole class missing)** — **bounded, see PLAN_CHANGE 3.** One of ~5 of its shape.
- **H4 (the 1000-present freeze is an independent second blocker)** — **strongly supported and
  still live.** f9 ran 1203 s to deadline with zero `[ICALL]`, zero ABI failures and never passed
  1000 presents; f10-f13 same. Flips stop with presents, so the guest stopped committing NV097
  methods. No host cap exists. f16 and f17 reproduce the freeze at 1000 while *also* clearing
  blockers, which further separates the two.
- **H5 (freeze is the disc-error/pending-I/O path)** — live. f9's fatal file line comes ~90 s
  after flips stop, so the fatal is at most a consequence.
- **H6 (720-update hold / `+0x24` idle branch)** — weakened by f12/f15 (`+0x24 = 0` at the 1000th
  present with update counters moving), not excluded.
- **H7 (rendering/submit defect unrelated to state-machine timing)** — live and under-weighted.
- **H8 (NEW, from this turn): a switch arm whose target is a shared epilogue just outside the
  span).** Confirmed as the mechanism of blocker 3. This is a distinct sub-class from both
  `0x13FAD0` (table slot inside another span) and `0x13B750` (`.text` immediate, unowned gap).

## Measurements that remain decisive

- **M3 (guest vs host freeze)** — read `[GPU] flips` and `[FBPRESENT]` in the same run. Both stop
  together in f9/f15/f16/f17, so the guest stopped submitting. Keep this as the standing check.
- **M5 (is the freeze a timeout?)** — compare the wall-clock gap between the last present and the
  last committed method against the 240 s pending-I/O threshold and the 720-update hold.
- **M7 (NEW — blocker 3 falsifier)** — after the `0xBBA17` entry, does the run clear it and either
  reach a new site or report an ABI failure at `0x000BB7B0` (which would indicate switch slot 4 is
  reachable and unbalanced)?

## Advisor

Consulted once (blocker 3's repair choice, since several plausible repairs fit and the choice is
ABI-sensitive). The ruling is recorded here when it returns. The start plan's other Advisor points
remain open: whether the freeze mechanism is H5 or H7, and whether the `.text`-immediate class
becomes a systematic pass.

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
