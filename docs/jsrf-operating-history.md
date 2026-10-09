# JSRF operating history

This file is **historical narrative only**. It records what happened and why.
It is never an execution/status authority. Current work, status, blocker and next
action come only from `plan-jsrf-bare-minimum.md`; staffing comes from
`docs/agent-workflow.md`. Historical `next packet`, `current stop`, model names,
acceptance language and imperative wording below are provenance, not instructions.

Where later historical entries correct earlier ones, the later entry wins only for
historical interpretation; neither can override the current plan.

---

## Immediate handoff after the GPU harness pass

The user asked to prioritize this guide because weekly usage is nearly exhausted.
Finish this checkpoint without broadening into renderer implementation.
**11b1 through 11b3 are complete (2026-09-19):** the NV2A register aperture has one serialized
owner, conflicting acknowledgement mutations are quiesced, actual concurrent
VEH accesses and decoder flags are tested, frozen model publication is
generation-validated, and teardown is verified. PBUS and the real HAL PCI path
now share validated NV2A configuration backing while identity fields remain
immutable. PTIMER now has pinned split-TIME semantics, deterministic contracts
and a wakeable service owned by the same serialized lifecycle; the 16-probe
harness proves expiry without another guest PTIMER read and clean shutdown.
**11b4a is also complete:** the real trapped USER path consumes physical-offset
pushbuffers only from the contiguous window, atomically records bounded packet
streams, and rolls GET/sink state back on unsupported work. **11b4b1 and
fixture-scoped 11b4b2 are complete:** fixture lookup plus transactional
SET_OBJECT proves per-subchannel binding, and only NV097 NOP and packed clip H/V
are accepted. Format, pitch and offsets remain unsupported and roll the entire
stream back. The full harness has 19 probes. 11b4b3 now has validated PRAMIN
backing: `0x00194913`/`0x001949E2` write through `device+0x700000` into the
claimed instance range shared with `nv_dma_load`. Production SET_OBJECT now
walks that RAMHT; the fixture seam remains for isolated 11b4b2 tests. The
packet is **11c1 leaf `0x00196800`**, the self-contained PLL coefficient decoder
inside `0x00194676`; leaves `0x0019460A` and `0x00194635` are recovered and
directly tested while the parent remains fatal. Follow it with `0x00196967`,
which claims and clears the first real instance-memory range needed by 11b4b3.
Keep recording facts versus
hypotheses, and do not stub helpers merely to reach another missing call.

11c1 has now accepted nine direct leaves: `0x0019460A`, `0x00194635`, `0x00196800`,
`0x00196967`, `0x00196A65`, identity ramp `0x00194533`, and register table
`0x00196B34`, instance clear `0x00196B6A`, and defaults `0x00196B9A`. The allocation leaf establishes the original 0x5000-byte instance
allocation and address derivation, but production intentionally still traps its
unrecovered `0x00194520` dependency and the parent `0x00194ADD`. Its focused
fixture passes 81 checks; combined Release CTest passes 7/7. The next narrow leaf
The real `0x00194520..0x00194533` dependency is now accepted and no longer fatal;
it returns the prior context+0x19C value in EAX while storing the incremented
value. Integrated helper `0x00194676..0x00194780` is also accepted with 1,752
focused checks and the standard routine frame-metadata caveat. The next packet is
`0x00194780..0x001948B9` is decomposed and tiny leaf `0x00196E80` is accepted.
`0x00196C83` is accepted with 1,889 focused checks and an evidence-backed
`restore_frame_metadata` manifest opt-in limited to that balanced prologued
helper. `0x00197BCF` is accepted after a fresh focused rebuild with 1,935 checks.
Timestamp leaf `0x00193C40` and the shared QPC publication contract are accepted:
the toolkit uses `INIT_ONCE`, the focused fixture proves exact EDX:EAX halves,
Release CTest passes 8/8, toolkit tests 27/27, and final run
`logs/runs/20260920-014707-773-timestamp-publication-final/` retains the expected
`0x00194ADD` boundary. The `0x00197C50` chain contains a polling/callback cycle among
`0x00196C0B`, `0x00194210`, `0x00193D90` and `0x00197AAC`; use Sol Medium for
that concurrency contract. The bounded test-only `0x00196C0B` harness is accepted:
1,984 checks over 64 repetitions and Release CTest 9/9. It does not prove the
generic event seam is the real `0x00193D90` signal contract. Luna may now recover
only `0x00196C0B` with test callback seams while production callees remain fatal.
That helper is now accepted with 1,956 focused checks and clean Terra review.
Queue helper `0x00193D10..0x00193D85` is now accepted with 1,988 focused checks;
both `0x0018E500` and `0x0018E584` remain fatal. The real `0x00193D90`
callback/signal and `0x00194210`/`0x00197AAC` reentry contract is accepted
with 5,440 bounded checks; those production bodies and `0x00196C4A` are now
recovered. `0x00194ADD`, `0x00194780`, `0x00197C50`, RAMIN object helpers,
and `0x001912A0` are recovered. Ordinary startup stops at unresolved
`0x001918E0`. WBINVD ran. Keep `0x00193F70`, `0x0018E120`, and `0x001918E0`
fatal. Production SET_OBJECT walks claimed PRAMIN RAMHT. The kick chain
stays fatal until its wait/wrap/kick contract is written.

## Fresh-context handoff — 2026-09-21

**The repository history was rebuilt on 2026-09-21.** `.git/objects` had gone
missing, so no git command worked and the plan/reports had been uncommitted. The
old directory is kept as `.git.broken-backup/` (gitignored) and history now
starts at `e336a1c`; SHAs before that do not resolve. When committing, put the
**toolkit revision in the commit message** — that is the only record of which
`xboxrecomp` revision a build came from now that history is truncated.

Preserve the unrelated untracked `C:\Users\logic\Repos\xboxrecomp\recomp_run.log`.
The current game binary stops at expected fatal `0x001918E0`. Latest bounded
evidence: `logs/runs/20260921-100901-149-pramin-backing/`. `0x00192090` has
not returned; it reached the first pushbuffer kick after WBINVD.

Current acceptance baseline:

```powershell
cmake --build build --config Release --target jsrf_recovery_11c1_test jsrf_nv2a_test
.\build\Release\jsrf_recovery_11c1_test.exe  # PASS: 2197
.\build\Release\jsrf_nv2a_test.exe  # PASS: 304
ctest --test-dir build -C Release --output-on-failure  # 11/11
Push-Location ..\xboxrecomp
python -m unittest tools.recomp.test_lifter_atomics tools.recomp.test_lifter_string_compare tools.recomp.test_lifter_carry tools.recomp.test_seh_frame_owner  # toolkit: 27/27
Pop-Location
```

Hardware setup through framebuffer publish is recovered. Toolkit `KeSetEvent`
uses per-waiter host events. PGRAPH_INTR writes are W1C. `OUT` lifts to
`xbox_outb`. PFB WBC reads idle after a flush write. Production `0x00193F70`,
`0x0014B794`, `0x0018E120`, and `0x001918E0` stay fatal.

Ordinary startup now stops at unresolved `0x001918E0`
(`logs/runs/20260921-103827-656-ramht-lookup/`). Production SET_OBJECT uses
the claimed RAMHT. The next packet, after independent acceptance of that
lookup, is
**kick/GET contract, on the Grok 4.7 xhigh parent, for `0x001918E0` → `0x001917F0` → `0x001916B0` /
`0x00191530` plus `0x00190240`**. Do not stub it.

Focused 11c1: PASS 2197. NV2A: PASS 304. CTest 11/11.

For a small future Sol/Terra/Luna task, select one helper or one register contract,
name its files and original address range, provide the baseline capture and exact
acceptance commands, and require updates to report/plan/this guide. A port/MMIO
implementation must have behavior tests plus a bounded game run; a documentation
contract audit alone needs no rebuild. Keep Nsight completeness as a separate
optional tool task: it does not block original-XBE analysis or RenderDoc inspection.

## Guest startup stop — the alias fold deleted a static initializer (2026-09-21)

**This supersedes the "next packet is the SEH chain" note above. There is no
exception, no unwinder and no EH-table defect on the path.**

The first real guest stop after `guest_entry` was
`[ICALL] invalid target 0x00700010 return=0017E627`. The previous session read
`esi=0x001EB764` / `edi=0x001EB76C` as EH-table pointers and `0x00700010` as an
EH handler slot. They are not. `sub_0014B50D` is **`_initterm`** — it walks the
function-pointer array `0x001EB760`..`0x001EB76C`, so `esi`/`edi` are its own
loop pointers — and slot `0x001EB764` holds **`0x0017E58F`**, a C++ static
initializer. `0x00700010` is stale stack read as `[ebp+8]` because the
three-argument `sub_0017E600` was called with zero arguments.

`0x0017E58F` was classified `tail_jump_alias` and folded by the `ff4d442`
"abutting alias" rule into the next entry `0x0017E600`: the dispatch tuple became
`{ 0x0017E58F, sub_0017E600 }` and the body was deleted. Pre-fix evidence that
this is a regression: `logs/runs/20260921-111607-936-kick-ack/source.zip` has
`{ 0x0017E58F, sub_0017E58F }` and a clean body in `recomp_0007.c`.

Fixed by recovering `0x0017E58F` in `config/recovered-functions.json`
(`kind: "routine"`, entry 71). An entry there becomes
`detection_method: reviewed_runtime_target`, gets its real translated body, and
overrides the wrong generated dispatch because `RECOMP_ICALL_SAFE` consults
`recomp_lookup_manual` **before** `recomp_lookup` — **no chunk regeneration was
needed.** Use `kind: "routine"` for anything that calls other framed functions:
a generated epilogue emits `POP32(esp, ebp)` without restoring `g_ebp`, so
`g_ebp` is a last-published-frame hint and the initializer default's
`g_ebp != before_bp` check fails spuriously.

Before/after: kernel calls **157 -> 177**, initializer now logs
`[RECOVERED] 0x0017E58F returned; ABI verified`, and the stop moves later to
`[ICALL] Failed to resolve VA 0x00148005`. Runs:
`logs/runs/20260921-154535-674-resume-check/` (before) and
`logs/runs/20260921-155635-882-alias-fix-verified/` (after). CTest 11/11.

**CLOSED for the CRT-initializer class.** The same defect was then enumerated
rather than chased. The CRT calls these function pointers with **no arguments**:

| table | range | walked by |
|---|---|---|
| A | `0x001EB760`..`0x001EB76C` | `_initterm` `0x0014B50D` |
| B | `0x001EB770`..`0x001EB83C` | `_initterm` `0x0014B4B5` |
| C | `0x001EB840`..`0x001EB854` | `_initterm` `0x0014B4B5` |
| D | `0x001EB854`..`0x001EB86C` | `_initterm` `0x0014B4B5` |
| hook | `[0x0022ED2C] = 0x0017BF79` | `sub_0014B4B5` |

**Table B is exactly the 14 initializers milestones 02/03/04 recovered** — the
rest of the same tables were never recovered because the fold had redirected
them to something that resolved. 60 distinct targets, **44 folded**; all 44 are
entries by definition (only the tables call them, with no arguments) and all 44
are now in `config/recovered-functions.json` (`kind: "routine"`, 114 entries).

**Range trap:** an entry's `end` must be **the alias's own DB end**, not the
first terminator from linear disassembly. `0x00181212` ends in `ret` at
`0x00181224` but branches to `0x00181225`, so a linear bound truncated the body
and made the branch an unresolved call. 40 of 43 bounds were widened.

Result: kernel calls 177 -> **200**, **56** `ABI verified` lines, no unresolved
ICALL, **CRT initialization completes** and the guest reaches heap allocation
(`[HEAP] #4..#7`, 4.2 MB of 50 MB). CTest 11/11. Run
`logs/runs/20260921-160209-538-crt-initializers-bounds/`.

**RESOLVED, and the previous "next packet" was wrong.** `0xC00000FD` was **not**
a host-stack sizing problem. The disassembler reads a **data table of `.text`
addresses as a switch table**; a COM vtable is exactly that shape, so every
vtable method became `tail_jump_alias` and was folded into the next function. The
interface table at `0x001E0F00` is `[0x0014CF20 QueryInterface, 0x0014CF00
AddRef, 0x0014CF80 Release, ...]`, stored by constructor `0x0014CDB0` with
`mov dword ptr [esi], 0x1E0F00`. `sub_0014CFB0` calls `vtable[0]` at
`0x0014CFDE`, and the fold had written `{ 0x0014CF20, sub_0014CFB0 }` — so
**`sub_0014CFB0` called itself**. Events 78418+ repeat `target=0014CF20
site=0014CFE0` with `esp` descending **0x1C per cycle**, ~13,000 cycles × 28 =
364 KB = the guest stack in use at the fault: an unbounded guest recursion.

**1120 vtable methods had a redirected dispatch; 1111 are recovered** (9 are
genuine fragments the translator refuses to lift standalone). With the CRT
initializer class that is `config/recovered-functions.json` = **1228 entries**,
exe 9.4 → 13.2 MB.

**The guest no longer crashes.** Outcome is `diagnostic_deadline` (exit 3), not an
unhandled exception: CRT init and GPU setup complete, and **the pushbuffer kick
chain this guide called the fatal stop now runs and returns** (`0x00190FB0`,
`0x00190240`, `0x001917F0`, `0x001918E0`). 98 verified, 0 ABI failures, guest
events 78,441 runaway → 563, CTest 11/11. Run
`logs/runs/20260921-161956-152-reviewed-restored/`.

**Two manifest rules learned the hard way:**
- An entry's `end` is **the next function entry inside the range**, not the
  alias's DB end. `0x00150C00` is 112 bytes but its bound was `0x00150EA0`, so its
  body swallowed `sub_00150C70` (560 bytes) and had two epilogue deltas. 636 of
  1228 bounds were tightened.
- **`stack_args` only affects the wrapper text in `recovered.c` — regenerate after
  editing it**, or the change is invisible. And **reviewed values win over any
  derivation**; restore them from `96ca4e0` if a derivation overwrites them.

**The hang is a sweep of instance memory, not a register poll.** Toolkit
revision **`dc79321`** adds `RECOMP_MMIO_TRACE` to `nv2a_mmio_hook.c`: distinct
MMIO offsets with counts and the faulting RIP, each new offset printed once and
the hottest reprinted every 200,000 accesses, so the answer survives a run killed
at a deadline. It exists because a handled MMIO fault is otherwise invisible from
every artifact a run produces — the hook returns `CONTINUE_EXECUTION`, the
collector's `DEBUG_EXCEPTION` lines carry no address, and a deadline run writes no
`ExceptionStream` to the minidump.

Trace result (`logs/runs/20260921-162652-137-mmio-trace/`): **400,000+ MMIO
accesses in 12.6s over 100+ distinct offsets, none above 8 occurrences, all from
one RIP**, advancing `0x700000, 0x700004, 0x700008, …` — a **linear dword sweep of
the PRAMIN / instance window** inside `body_00192090` → `sub_00191BA0` →
`sub_0018E160` → `sub_00194C3F` → `sub_00194A72` → `sub_00194300`. So the guest is
**scanning instance memory for something the model does not contain**: a GPU-model
content gap, not a register contract and not a translation defect. `0x002652B0`
was a red herring — it is `.data` BSS written at runtime.

**Next packet:** identify the object the sweep searches for. Raise
`MMIO_TRACE_SLOTS` past 96 (it filled, which is why the hottest count is only 8) to
see whether the sweep terminates or wraps, then read the scanning site in
`body_00192090` and compare its key against what the model writes into the claimed
RAMIN window. **Do not chase the interrupt angle** — `PMC_INTR_0`, `PFIFO_INTR_0`,
`PGRAPH_INTR` and `PCRTC_INTR` being zero is consistent with the sweep, not its
cause.

**Finding a vtable (reusable):** a data table whose entries are a run of
**distinct** `.text` addresses, stored by a constructor as an immediate. Take
candidates from the **generated C** (`= 0xADDRu;`) — a linear capstone sweep over
`.text` desyncs and missed `0x1E0F00` entirely. **The read must stop at the first
repeated target**; reading past the table into adjacent `.rdata` let one
unrelated duplicate reject the whole table (384 → 1120 on the second pass). A
switch table repeats almost immediately, so a distinct run of ≥ 3 separates them.

**Immediate next step:** see the "Next packet" above — the `stack_args` gap is
closed (`0x0014CF20` corrected to 12; the `0x00150C00` bound tightened to
`0x00150C70`; the reviewed `0x00190FB0` restored to 12), and the run now shows 0
ABI failures.

**This is a workaround, not the fix.** The real repair is
`tools/recomp/translator.py`: do not fold an alias whose body needs none of its
parent's labels. Needs its own regeneration and baseline; re-run the initializer
and vtable audits afterwards to confirm the counts reach zero. Do not merge the
config workaround and the toolkit repair in one change.

**Two traps:** never put backticks in a string passed to `bash -c` through a
double-quoted `python -c` (bash command-substitutes them and the text silently
vanishes — write the script to a file). And `recover-functions.py` correctly
raises `Incomplete translation at XXXXXXXX`; drive it in a loop that drops the
offending entry rather than weakening the check.

**Alias-fold defect, still open for the general rule.** 3123 dispatch tuples
redirect a VA to a different symbol; only **87** have a `loc_<VA>` label in the
parent, so **3036** redirect to a body that does not contain the address; of the
971 aliases adopted by the abutting rule, **759 end in `ret`**. Three
discriminators were measured and **all three discarded because they saturate** —
"last instruction ends exactly at the alias end" (`int3` padding decodes as
instructions), "ends in `ret`" (fires 759/971), and "entered from data" (fires
**2651/3123**, because a switch table is also a run of `.text` addresses).
**Do not re-propose these without new evidence.** The remaining sound signal is
structural: fold an alias only when its body actually needs the parent's labels.
Until the rule is repaired, **an address entered from data must be recovered
explicitly.**

## GPU direction and bounded next packets

Read `docs/jsrf-gpu-setup-contract.md` before this work. Milestone 11a is
complete as a corrected documentation audit, and 11b1–11b3 implement the single
register-aperture owner, PCI paths and PTIMER. The open items (GPIO[0] electrical effect, vendor-specific PCI
0x4C..0x4F, constant 0xFE502A,
KeInitializeInterrupt 7th argument) are recorded
as hypotheses. The existing MMIO hook does not install a complete interception
path; PFIFO/USER are stubs and its VRAM is detached from guest allocations. See
the audit before using its initialization API.

Preserve the guest device/resource layout and command stream; connect the
existing NV2A/D3D11 components behind it. This is a direction, not a working
renderer. The memory-layout acknowledgement worker advances DMA_GET and busy
states without executing commands. That does not establish GPU completion.
Do not enable competing GPU/MMIO owners without reconciling their state.

1. **11a: GPU setup contract.** Inspect original `0x00194ADD..0x00194C3F` and
   callees `0x0019460A`, `0x00194635`, `0x00194676`, `0x00194780`. Record register
   reset values, OUT port `0x80C0`, device/queue fields and interrupt/DPC contracts.
   Source scope: original XBE, toolkit `src/nv2a`, memory mapping and kernel
   bridge. Use primary hardware references when local evidence is insufficient.
   Acceptance: exact behavior/address evidence and a proposed single state owner;
   no guessed successful port write or invented GPU status.
2. **11b: Port/MMIO and completion.** 11b1 connected one register model to the
   existing aperture and retired conflicting register acknowledgements. 11b2
   implements PCI configuration paths; 11b3 completes PTIMER semantics; next,
   11b4a adds bounded PFIFO/USER packet intake; 11b4b1 establishes the atomic
   unsupported-method boundary; 11b4b2 proves fixture-scoped SET_OBJECT and clip
   state; 11b4b3 production SET_OBJECT walks claimed PRAMIN RAMHT; 11b5
   reconciles command completion.
   A deliberately stalled command must remain blocked and diagnosable.
3. **11c: Complete GPU setup recovery.** Add reviewed entries and internal tails
   to the appropriate manifests; retain fatal dependency traps. Require verified
   outputs/return from `0x00192090`, then archive the next stop. WBINVD at
   `0x0019243A` ran in `logs/runs/20260921-020149-996-gpu-setup-912a0/`. The
   initializer now dies at pushbuffer kick `0x001918E0`.
4. **11d: Guest ISR/DPC execution.** Prove worker stack/register/IRQL isolation
   before dispatching callbacks. Recover ISR `0x00193C50` and DPC `0x00194480`
   when needed. Test missed signals and reentrancy; close relevant 01c/01d gaps.
5. **12: First presentation.** Connect the game's framebuffer to a responsive
   Windows window. Require a game-generated clear/frame, not a canned toolkit
   screen or a synthetic triangle (those are fixtures only).

WBINVD invokes `recomp_guest_cache_flush`, a full host memory barrier for coherent
emulated memory; it is not native cache invalidation or GPU completion. Existing
SFENCE/LFENCE/MFENCE handling and optional runtime concurrency paths need review
before relying on them for GPU workers. The video probe verifies behavior and
return contracts, not weak-memory ordering or actual GPU execution.

Each future packet must name its milestone, expected outcome, source scope,
baseline artifact, confirmed facts versus hypotheses, reproduction/acceptance
commands and required documentation. If evidence expands scope, record the new
boundary rather than adding guessed fixes. Update this guide's checkpoint and
next task, the plan's status and the live report after substantive progress.
No commit, publication or scheduling is implied by maintaining these
documents. Delegated JSRF packets use the user's standing authorization and
follow the ownership rules above.


---

## Archived execution-plan snapshot moved out of the active plan — 2026-09-23

**NON-EXECUTABLE ARCHIVE.** The following is the pre-hardening plan snapshot. All statuses, model names, `next` statements and imperatives in this section are historical. Use the current `plan-jsrf-bare-minimum.md` for execution.

# Jet Set Radio Future: Windows port milestone plan

Agent onboarding, operating rules and handoff guidance live in `AGENTS.md`.
Maintain that guide as capabilities and commands change; this plan is the
**execution authority** for current packet selection, milestone status, acceptance
criteria, evidence requirements, blockers, and next action. Reports are optional
session/audit logs and are never required to determine what to do next.

## P0 — Repair the execution and evidence loop before continuing A2h

**Contract:** `docs/packets/p0-acceptance-contract.md`, revision `P0-AC-r1`.
**Status:** **P0.1, P0.S and P0.2 accepted.** P0.2's interface amendment is **ADEQUATE** at `r14`, and its **implementation review returned all four criteria AGREED** at round 5 — after four rounds of refutation that found five AC2 defects, four of them in the fix for the previous one. P0.3–P0.7: **all 18 criteria AGREED** on one evidence revision. Evidence: `docs/reviews/p0-1-vblank-adjudication.md`, `p0-2-acceptance.md`, `p0-3-to-p0-7-acceptance.md`.
**Next action:** work the bounded A2h writer investigation at `docs/packets/a2h-writer-investigation.md` (ADEQUATE at `A2h-r4`). It authorizes exactly one observation-only trace and one strict run — not a recovery pass and not a whole-image census. Later A1–A5 material remains deferred.
**Scope:** bounded execution/evidence tooling, not a renderer or full translation regeneration.
**Authority:** criteria, statuses, blockers and next action live here and in the linked packet/contract; roster, session loop, reviewer and advisor policy live only in `docs/agent-workflow.md`. Reports may record session chronology but cannot override this plan.

**Completed prerequisite history:** P0.S (`docs/packets/p0-save-isolation.md`) moved
disposable save-root selection and real-path verification ahead of P0.1's live
baseline so no baseline could write the existing save root. P0.S is accepted after
native replay and 12/12 Release CTest (`docs/reviews/p0-1-execution.md`). P0.1's
VBLANK coverage disagreement was adjudicated, corrected inside frozen `P0-AC-r1`
without changing criterion text, re-reviewed, and accepted; see
`docs/reviews/p0-1-vblank-adjudication.md`.

`MEASURED` identifies inspected source/artifacts or an attributed historical
measurement. `INFERRED` identifies a proposal and expected benefit. A packet record
must bind stable criterion IDs to evidence revision, exact procedure and reviewer
disposition. `PASS`, `FAIL` and `UNKNOWN` are per criterion; missing or
`CANNOT VERIFY` evidence remains pending. An advisor may resolve an interpretation
or scope dispute but cannot turn a failed measurement into a pass.

### P0.1 — Enforce strict/exploratory classification at launch and in archive checks

**Pre-packet measured basis:** A2g's
`logs/runs/20260922-224429-003-a2g-304f0-span/metadata.json:25-31`
enables `RECOMP_AC97_READY=1` and
`RECOMP_APU_DSP_ACK=0x803C0810`. Both are synthetic completion under
`docs/jsrf-run-profiles.md:36-43`, despite the plan's strict label.
`scripts/run-jsrf.py:157-172` has no profile argument.
The owner's wider audit reports the same overrides in A2e, A2f and both
A2g runs, and 247 exploratory runs among 649 inspected. That count does
not itself establish that all 247 were mislabeled.

**INFERRED change:**
- Add explicit strict/exploratory/fixture selection to `scripts/run-jsrf.py`.
- Share a tested classifier between the runner and
  `scripts/check-run-profile.py`; maintain effective-setting semantics in
  one source used by `docs/jsrf-run-profiles.md`.
- Strict must reject enabled synthetic completion and bypasses, including
  inherited settings and relevant runtime defaults. Never silently downgrade
  a requested strict run.
- Archive requested profile, effective settings, classification, reasons and
  classifier version. Where possible, verify runtime-reported effective state.
- Missing metadata, unknown schema, malformed settings and conflicting duplicate
  keys produce UNKNOWN/INVALID, not CLEAN. Support only explicitly tested
  historical metadata shapes.
- Annotate historical profile findings separately; preserve original artifacts.
  Reopen only acceptance criteria unsupported by the corrected classification.

**Files:** runner, profile checker/shared module, profile documentation,
runner/checker tests, affected plan/review acceptance records.

**INFERRED benefit:** the fresh DSH session can trust a machine-checked
classification rather than a copied human label.

**Acceptance:** `P0.1-AC1`–`P0.1-AC5` in the linked contract. Runtime flag
semantics must be checked per variable at the source call sites. In particular,
`RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED` and `JSRF_ABI_CONTINUE` are
presence-enabled; `RECOMP_GPU_ACK` is active by default and only the caller's
explicit exact value `0` disables it. Strict launch rejects an absent
`RECOMP_GPU_ACK`; the runner must not supply it silently.

**Post-delivery amendment (2026-09-23), inside this same frozen contract.**
Review found a coverage gap: a retired override (`RECOMP_VBLANK`) was neither
classified nor rejected, so `RECOMP_GPU_ACK=0` plus `RECOMP_VBLANK=1` classified
strict. The advisor ruled the correction is an **implementation change within
`P0-AC-r1`, not a criterion change**, because AC1 commands classification from
actual runtime semantics and a name the current binary never reads has none.
Added: a `RETIRED_OVERRIDES` registry, a revision-aware resolver using the
removal boundary commit, fail-closed strict rejection of retired names at launch
and in records/archives, and a new "Retired overrides" section naming
`docs/jsrf-run-profiles.md` the single authority for override classification.
`AGENTS.md` carries a pointer, not a duplicate enumeration. Full ruling,
measured basis, applied change and hashes: `docs/reviews/p0-1-vblank-adjudication.md`.

### P0.2 — Make review ingestion a durable acceptance transaction

**Final status: ACCEPTED.** The executable interface amendment is
`docs/packets/p0-review-records.md`; its final interface revision is `r14`, and the
implementation review returned all four criteria AGREED at round 5. Earlier hashes
and adequacy rounds are historical evidence for how the contract was hardened, not
current blockers. The amendment supplies the concrete schema, validator/export
commands, and source-backed Codex/DSH review procedures.

**MEASURED basis:** an A2f acceptance verdict existed but was omitted from durable
project state, causing work to be re-diagnosed instead of resumed from its settled
review. The old `scripts/check-recorded-reviews.py` was tied to one DSH session, did
not establish actual parentage, examined only the first turn, and matched generic
verdict text rather than a packet/criterion identity. The workflow's former CANNOT
VERIFY gap is historical: current closure behavior is defined in
`docs/agent-workflow.md` §2.7.

**INFERRED change:**
- Add a tracked review index under `docs/reviews/`, keyed by packet and evidence
  revision. Record exact parent/child/turn IDs, route/effort, criteria, artifact
  paths, full verdict and follow-up findings.
- Persist the review before updating accepted status or starting dependent work.
- Parameterize and repair `scripts/check-recorded-reviews.py` and
  `scripts/check-subagent-routes.py`: filter actual ancestry, inspect relevant
  completed turns, and match exact review IDs rather than words such as ACCEPT.
- Treat missing/unreadable projection data as incomplete. Projection caches are
  recovery inputs, not the acceptance authority.
- Define delivered, review-pending, changes-requested, verification-blocked and
  accepted transitions in the workflow. Post-review changes invalidate affected
  criterion dispositions.
- Define how reviewer measurements are serialized with the one build/run owner.
  Owner-supplied new evidence must be checked by the reviewer, or receive an
  explicit criterion-specific advisor disposition.

**Files:** workflow, review index, both inspection scripts, their tests,
plan/review status links.

**INFERRED benefit:** settled children and new findings survive context changes,
and the session cannot mistake another packet's ACCEPT for its own.

**Acceptance:** `P0.2-AC1`–`P0.2-AC4` in the linked contract. Durable recording and
per-criterion transaction behavior are the gap; do not restate the workflow's
current closure policy as missing.

### P0.3 — Finish the single-authority split and bound onboarding

**MEASURED basis:** the pre-P0 documentation duplicated reviewer/route policy in
multiple files, while onboarding and session notes also carried dated next-packet
instructions. Those copies could disagree with the actual workflow authority and
made a fresh session reconcile stale policy before doing work.

**INFERRED change:**
- Keep roster/loop/escalation authority only in `docs/agent-workflow.md`.
  Replace plan, skill and report replicas with pointers.
- Generate or mechanically check any AGENTS summary.
- Scope continuation mechanics to the relevant harness/version; verify persistence
  with an actual follow-up rather than route availability alone.
- Remove retired suggested-model assignments from active A1–A5.
- Keep current packet, revision, criterion status, evidence, blockers and next action
  in this plan. Reports may link to those records but must not duplicate authority.
  Move narrative history into `docs/jsrf-operating-history.md`.
- Remove "all packets pending" and move old APU text out of A2h.
- Add a documentation check for active-policy contradictions, missing local
  command paths, stale authority links and UTF-8 instruction size.

**Files:** workflow, AGENTS, plan, report pointers, procedure skill, operating
history, new `scripts/check-agent-docs.py` and fixtures.

**INFERRED benefit:** the fresh DSH session receives one actionable instruction
set instead of resolving several generations of policy.

**Acceptance:** `P0.3-AC1`–`P0.3-AC3` in the linked contract. The checker must
include positive and injected-negative controls, report UTF-8 size against a
conservative budget below 65,536 bytes, and preserve historical records.

### P0.4 — Separate capture validity from guest-memory integrity

**MEASURED basis:** the A2g dump is structurally readable at the failing call site
(the logged ESP contains the ICALL's pushed return VA), while its XBE-backed image
content is displaced by `0x37608`: 460 of 545 XBE-backed pages match
`original[VA + 0x37608]`, 0 match `original[VA]`, and the canonical `.text` control
fails. This proves structural capture validity and image-content integrity are
separate properties. The pre-P0 dump checker conflated them and also allowed a
named missing dump to avoid the failure list.

**INFERRED change:**
- Separate structural capture checks, address mapping checks and image-content
  comparisons in the dump checker/reader documentation and diagnostics.
- Preserve actual guest-VA reads for structurally readable corrupted captures.
  Never apply a global displacement correction.
- Label shifted comparisons as byte-provenance analysis, not repaired memory.
- Report MATCH, CONTENT_MISMATCH, UNREADABLE and MISSING separately.
  Named missing inputs must not pass.
- Describe A2h as "displacement characterized; writer unknown," not a completed
  root-cause/fix packet.

**Files:** `scripts/check-dump-mapping.py`, `scripts/jsrf_dump.py`,
`scripts/inspect-jsrf.py`, relevant tests, AGENTS, plan and review records.

**INFERRED benefit:** the next session can use the corruption evidence without
discarding it or accidentally shifting the intact stack.

**Acceptance:** `P0.4-AC1`–`P0.4-AC3` in the linked contract. Structural
readability, stack/register location and image-content integrity remain separate;
all actual-VA reads and malformed/missing controls are required.

### P0.5 — Make the Python wrapper the single guarded build entry point

**MEASURED basis:** `scripts/build-jsrf.ps1` was absent at inspection while
onboarding and `scripts/build-identity.py:26-32` recommended it.
`scripts/build-jsrf.py:22-28` generalized a confined-policy failure to DSH.
AGENTS and this plan scope the failures to policy rather than treating them as universal machine defects.
The wrapper owns a manual target list; `CMakeLists.txt:112` invokes unqualified
`python`.

**INFERRED change:**
- Make `scripts/build-jsrf.py` canonical and correct command references.
- Add diagnostic/preflight output for resolved interpreter, CMake/CTest,
  dependencies, tool versions and known execution policy.
- Preserve environment normalization and explicit parallelism.
- Retain logs for every attempt. Diagnose the measured silent confined-build
  signature without claiming every exit 1 is a sandbox failure.
- A permitted serial retry must preserve the original failure; an explicit
  sandbox denial must not be bypassed.
- Define an aggregate acceptance-build target in CMake so the Python wrapper
  does not maintain a separate executable inventory.
- Bind CTest Python to the configured interpreter and include configuration/tool
  provenance in build identity.

**Files:** build wrapper, identity script, CMakeLists, onboarding,
wrapper tests.

**INFERRED benefit:** DeepSeek can build all required artifacts without
reconstructing missing-script/Bash workarounds or forgetting a test executable.

**Acceptance:** `P0.5-AC1`–`P0.5-AC4` in the linked contract. Build every
discovered CTest target; report the full test inventory and each
passed/failed/unknown/not-run result. Do not fabricate a confined-policy
remeasurement.

### P0.6 — Make harness permissions and probe expectations explicit

**MEASURED basis:** `tests/test_gpu_inspection.py:68,77` uses temporary
directories; `scripts/run-jsrf.py:170-171` defaults every launch to guest-entry
checkpoints, while `scripts/test-harness.py:105-108` already knows probe-specific
checkpoints. Three measured environment failures must remain distinguishable from
guest regressions: under confined workspace policy, MSBuild parallelism (`N > 1`)
can fail silently while serial succeeds; `tempfile.mkdtemp` output can be unwritable
while the same test passes with permitted storage/full access; and denial of write
access to `%LOCALAPPDATA%\xboxrecomp\Partition0.img` produces `ACCESS_DENIED` and
an early `HalReturnToFirmware(2)` exit. These are policy-dependent, not universal
machine properties.

**INFERRED change:**
- Support an explicit policy-permitted scratch root for offline tests, with
  preflight and cleanup. Do not switch creation APIs to evade a denial.
- Check configured emulated-disk directory/image access without changing original
  image or save contents. Classify failure as ENVIRONMENT_BLOCKED.
- Centralize probe checkpoint defaults shared by runner and harness tests.
- Keep launch, capture, environment and semantic outcomes distinct.

**Files:** runner, harness tests, offline GPU tests, shared probe definitions,
environment diagnostics and onboarding.

**INFERRED benefit:** a permission failure or probe checkpoint mismatch is
identified before it becomes another guest-regression investigation.

**Acceptance:** `P0.6-AC1`–`P0.6-AC4` in the linked contract. Probe coverage is
the enumerated probe set, and blocked tests remain in the denominator. Original
assets and existing saves must remain byte-identical.

### P0.7 — Guard regeneration provenance and publish the bounded next packet

**MEASURED basis:** ordinary regeneration rewrites generated output and can erase
ABI instrumentation that acceptance evidence depends on, while the pre-P0 identity
record did not bind the inputs/recipe that produced the committed tree. Therefore a
regeneration-induced semantic change could not be distinguished reliably from an
implementation regression. `scripts/build-identity.py:12-24` fingerprints emitted
sources but, by itself, not the complete generation recipe.

**INFERRED change:**
- Add generation provenance/check-only support covering toolkit revision,
  original XBE/analysis hashes, commands, generation mode and protected ABI
  instrumentation.
- Integrate checks with build identity without automatically invoking full
  translation.
- Generate comparison candidates only in isolated output; reject unexplained
  drift or missing protected instrumentation. Keep full semantic regeneration
  in A4c rather than expanding this cleanup.
- Freeze packet criteria. Naming an unrelated later defect does not silently add
  a new criterion; any actual scope change goes through the review/advisor gate.
- Publish a short final handoff with exact two-repository/build identities,
  effective profile, review records and the A2h writer experiment.

**Files:** build/identity/recovery scripts, provenance manifest/checker,
onboarding, plan and report pointers.

**INFERRED benefit:** the next A2h instrumentation build cannot unknowingly
replace its translation baseline or erase diagnostics.

**Acceptance:** `P0.7-AC1`–`P0.7-AC4` in the linked contract. No-op/check-only
generation must leave production chunks byte-identical; mutations to inputs,
recipe or protected instrumentation must fail closed. After P0 acceptance, resume
only the bounded A2h writer investigation. A protective trap is containment, not
proof of a completed behavioral fix.

## Deferred roadmap and historical audit records — not executable

**Historical snapshot:** this A1–A5 sequence once named P0.1 as its next packet.
It is retained for evidence and accepted history only; its old ordering, statuses,
‘next packet’ statements and open work do not authorize execution. The current next
action is the one declared at the top of this plan. No A1–A5 item may be resumed from this text. Before any is
reactivated, write a bounded packet with stable criterion IDs, admissible evidence,
verified commands or explicit tooling prerequisites, positive/negative/missing
controls, scope and a decision rule; obtain plan adequacy review under
`docs/agent-workflow.md`.

Snapshot baseline: game `6cb350e`, toolkit `18a0837`; exploratory run
`logs/runs/20260922-110738-253-nv2a-1bcc/` records toolkit `cf03f46`. It is not a
fresh validation of the reviewed source and establishes neither rendering nor
liveness. Preserve its original classification and evidence limits.

**Historical run-label correction:** older A2e/A2f/A2g text calls four runs
“strict.” That label is contradicted by their original metadata and is not valid
strict-profile evidence. `docs/reviews/p0-1-historical-profiles.json` fixes the
four run paths, metadata hashes and expected `exploratory` classification for
P0.1-AC4. Existing reviewer identities/verdicts remain historical provenance;
they do not validate the profile claim. The per-artifact adjudication is pending.

### Review authority

The historical packets below were written under earlier review and advisor wording. Do not use that copied policy. Current reviewer selection, per-criterion review, CANNOT VERIFY handling, escalation and packet closure come from `docs/agent-workflow.md`. Existing reviewer identities and evidence provenance remain as originally recorded; they are not current route instructions.
### A1 — Reconcile status, evidence profiles and known device mapping



Done:
- The authority split is explicit: this plan owns current status/blocker/next action;
  `AGENTS.md` owns operating knowledge; `docs/agent-workflow.md` owns staffing and
  workflow; reports are non-authoritative logs. No superseded NULL-ICALL/SEH/PRAMIN
  claim is an active instruction.
- The mapping is recorded: `device=0x0019B200`, `context=device+0x2268`,
  `event=context+0x1C8=device+0x2430=0x0019D630`, producer `0x00193D90`, citing
  `docs/jsrf-callback-reentry-contract.md`.
- **The `0x0018CE80` stride is corrected to 24 bytes**, verified from the
  instructions rather than taken on either party's word: `0x0018CE90 lea
  eax,[eax+eax*2]` multiplies the index by 3 before `0x0018CE93 lea
  esi,[ecx+eax*8+0x211c]`, so the address is `index*24`, and `rep movsd` with
  `ecx=6` copies 24 bytes. The report had read the `*8` as the stride. The
  work-queue inference drawn from the entry size is retracted: 24 bytes constrains
  layout, never semantics.
- **A further artifact was found while doing this**, and it invalidates a negative
  result the report was treating as evidence: `0x0019D630` appearing in no data
  table is *expected* under this mapping, because the event is only reached as a
  device-relative displacement. The `+0x2430`/`+0x2434` "exactly one site"
  reading has the same weakness. Only `KeSetEvent = 0 calls` survives, and its
  ordinal mapping still needs re-checking.
- Strict and exploratory profiles defined, with every override classified;
  `RECOMP_APU_DSP_ACK` is named as synthetic completion. `diagnostic_deadline`
  is described as bounded capture with liveness unresolved.
- Advisor consulted on the A1 result before starting A2, as the acceptance rule
  requires. Its cheapest discriminating move is recorded here: a write-watch on
  `[0x0019D630..+0x20]` logging the caller PC, plus a probe at `0x00193D90`.

Keep current execution state in this plan and operating rules in `AGENTS.md`;
separate historical investigations from current instructions. Reports are optional
logs only. Record the existing D3D device/context/event relationship and distinguish
strict runs from exploratory runs using synthetic completion. Reuse the environment
captured by `run-jsrf.py`.

Acceptance:

- All three documents agree on the current blocker, evidence revision, next packet
  and remaining limitations; superseded NULL-ICALL/SEH/PRAMIN investigations no
  longer appear as active instructions. Preserve useful historical evidence.
- Record `device=0x0019B200`, `context=device+0x2268`,
  `event=context+0x1C8=0x0019D630`, and the producer `0x00193D90`, citing
  `docs/jsrf-callback-reentry-contract.md` and original instructions. Correct the
  `0x0018CE80` array stride to 24 bytes; do not infer a work queue from its size.
- Define reproducible strict and exploratory configurations. Explicitly classify
  `RECOMP_APU_DSP_ACK` as synthetic completion and record other overrides and
  their effects. Bypass-enabled results cannot satisfy ordinary boot/audio/GPU
  acceptance. A strict run may stop earlier; document that honestly.
- Describe `diagnostic_deadline` as bounded capture with liveness unresolved unless
  a separate semantic progress criterion is met. Each current claim links to an
  artifact and its own revisions rather than borrowing newer source identity.

### A2 — Trace and repair delivery to the known event producer



Delivered and measured:
- The exact missing transition was that the card had **no interrupt source**.
  `kernel_vblank_tick` asserted vblank by OR-ing into `NV_PCRTC_INTR_0` and
  `NV_PMC_INTR_0`, both of which are write-1-to-clear, so it cleared pending
  bits instead of setting them. Measured with that path enabled
  (`logs/runs/20260922-155540-785-a2-vblank-probe/`): the ISR queued 968 DPCs
  while the published model snapshot read both pending registers as 0, and the
  producer `0x00193D90` never ran.
- The model now has a display clock (`nv2a_vblank_pulse`) and a real interrupt
  line (`nv2a_set_irq_sink`, replacing two no-op `pci_irq_*` stubs). The
  guest's own write-1-to-clear is the only acknowledgment; `NV_PCRTC_INTR_EN_0`
  and `NV_PMC_INTR_EN_0` are the guest's and both gate delivery. `RECOMP_VBLANK`
  is removed, so this is a strict-profile result rather than an exploratory one.
- Connected handler and context: `KeConnectInterrupt(vector 3, routine
  `0x00193C50`, context `0x0019D468`)`, read from the run log. The DPC routine is
  `0x00194480` (`KeInitializeDpc(context+0x84, 0x00194480, context)`, confirmed
  from the minidump at `0x0019D4EC`), and the producer is `0x00193D90` ->
  `KeSetEvent(context+0x1C8)`.
- **`[RECOVERED] 0x00193D90 returned; ABI verified`** — the producer ran and its
  spin exited, which requires the guest's own W1C to have cleared the pending
  bit. **Waiters on `0x0019D630` went 3 -> 2**: an intended waiter made
  observable progress.
- Deterministic fixture added to `jsrf_nv2a_registers`: source assertion with
  every mask off, block-mask versus master-mask delivery, guest W1C
  acknowledgment, W1C of an unrelated bit not clearing, and no spurious repeat
  while unacknowledged. No sleeps; no direct host signalling.

Why not accepted: the packet also requires the archived run's *next* stop to be
this packet's own, and this run stops on `0x00048190` — newly reachable code
that the baseline never entered. That is A2b.

Advisor: resumed `agent-d3294b58` before implementing. Its cheapest falsifier
(read the pending bits out of an existing artifact) confirmed the root cause
without a new run; its synthetic/legitimate discriminator (delete the guest's
acknowledge path and see whether anything still advances) is what justified
removing `RECOMP_VBLANK` rather than keeping it. That adopted discriminator is the
load-bearing result; detailed consult chronology belongs in operating history.

Trace the modeled interrupt source through pending bits, masks, ISR registration,
guest ISR `0x00193C50`, helper `0x00193D90`, acknowledgment and event waiters.
Investigate the `RECOMP_VBLANK` gate without assuming that enabling it establishes
correct source assertion or callback execution. Preserve guest thread context.

Acceptance:

- Evidence identifies the exact missing transition in the source-to-signal chain;
  include the actual connected handler/context and relevant register state.
- Deterministic tests prove source assertion, masked versus enabled delivery,
  acknowledgment/clearing and absence of spurious repeated delivery. Guest callback
  ABI and register/thread-context preservation are checked where delivery crosses
  into translated code; tests do not use sleeps as proof of waiter registration.
- The signal reaches `context+0x1C8` through the real guest producer, and the
  intended waiter makes observable progress. Do not substitute direct host event
  signaling, unconditional completion or forced register values for that path.
- Archive an identity-verified bounded guest run with profile, event/interrupt
  evidence and next stop. If an earlier strict-profile blocker prevents live
  reachability, retain the focused evidence but consult the advisor before calling
  this packet accepted on exploratory evidence alone.

### A2b — Root-cause the newly reachable ABI failure at `0x00048190`



Delivered and measured:
- The declared end `0x00048305` was a disassembler `tail_jump_alias` **phantom**
  (`_build_alias_entries`), `has_prologue` false and no xref, sitting inside the
  entry's own argument setup. The real body ends `pop edi/esi/ebp/ebx; add esp,8;
  ret 0xc` at `0x00048385..0x0004838C`. Corrected to end `0x00048390`.
- The over-long alias span had swallowed the body's own shared epilogue at
  `0x00048371`, which is why it was trapped as an aborting stub; it is now an
  internal label. Regeneration dropped five traps.
- `stack_args` is the `ret N` operand, not an argument count — verified against
  `0x0004A6C0` (declared 4, epilogue `ret 4`). So `ret 0xc` means 12. The first
  corrected run measured `esp` exactly four bytes high with the callee-saved
  registers already preserved, which is how the rule was found.
- Two neighbours corrected with their own evidence: `0x00048390` (→ end
  `0x00048570`, `ret 0xc`, stack_args 12) and `0x00048570` (→ end `0x00048690`,
  `ret 8`, stack_args 8).
- **`[RECOVERED] 0x00048190 returned; ABI verified`** and no ABI failure anywhere
  in the run. `0x00025310`, the second diagnostic failure, did not recur: its
  span was already correct, so it was collateral from the corrupted frame. That
  prediction is now measured.
- Strict 30 s run archived and compared against the baseline; the run's next stop
  is named (`[ICALL] Failed to resolve VA 0x00173DB0`, which is A2c). Named
  frames 109 → 180.

**Method note worth keeping:** the detector for this class is the *emitted code* —
a generated `body_XXXXXXXX` with no `return` statement at all — not the
disassembler. `scripts/check-entry-extents.py` sees only an informational
`NO-TERMINATOR`, because `end` is a clean instruction boundary. And
`scripts/inspect-jsrf.py disasm` is the authority for evidence: capstone in a
throwaway script desynced on bytes this tool decodes cleanly.

A2 delivered the interrupt line, so guest code the baseline never reached now
runs and stops 3.5 s in on:

```
[RECOVERED] ABI FAILURE 0x00048190 esp 00F7FDA0->00F7FD80 expected +8
```

The diagnostic run collects three failures — `0x00048190` twice and
`0x00025310` once — then `[ICALL] invalid target 0x00000000 ...
return=00011D6A` and exception `0xE0424943`.

`0x00048190` is recovered with `stack_args: 4` and a declared body spanning
`0x00048190..0x0004A6F0` (0x2570 bytes). Its own evidence records that the entry
exists because the `ff4d442` abutting-alias fold had pointed the dispatch tuple
at `sub_0004A6F0`. First hypothesis to test: the declared body end swallows one
or more sub-functions whose epilogues clean a different number of bytes, so a
path that runs past the real end returns with a desynchronised stack. The
signature — `esp` 0x20 below entry instead of 8 above, all four callee-saved
registers clobbered — is what a fall-through into a different `ret N` looks
like.

Acceptance: the failing entry either returns with its declared contract or is
shown by evidence to be an ownership/entry defect that A4a must fix; a strict
30 s run is archived and compared against
`logs/runs/20260922-160535-643-a2-irq-line/`; and the run's next stop is named.
Do not silence the check and do not widen the body end without evidence.

### A2c — Recover the functions only a pointer table reaches



The class, and why it is not A2b's. A function reached **only** through a data
table occurs in no call and no jump, so `tools/disasm` never registers it and no
alias fold can create it either. The preceding reviewed entry's end, tightened
to "the next function", then runs straight over it. The detector already exists:
`scripts/check-table-targets.py` reports **134 unresolvable pointer-table
candidates — 64 swallowed by a span, 70 with no span at all.**

Delivered:
- `0x00173D70` end `0x00173ED0` → `0x00173DB0` (real body ends `pop esi;
  add esp,0x14; ret 8` at `0x00173DA2`, padding to `0x00173DAF`).
- `0x00173DB0` **new entry**, `0x00173DB0..0x00173EC9`, `sub esp,0x44` prologue,
  `add esp,0x44; ret 0x14` at `0x00173EC3`, `stack_args` 20. Measured:
  `0x00173DB0 returned; ABI verified`, and the stop moved to `0x00175300`.
- `0x00175250` end `0x001753D0` → `0x00175300` (real body ends `pop esi; ret 8`
  at `0x001752F0`).
- `0x00175300` **new entry**, `0x00175300..0x001753CB`, early `add esp,8; ret 8`
  at `0x001753AF/BA/C5`, `stack_args` 8. Measured: `0x00175300 returned; ABI
  verified`. Run duration 3.30 s → 4.13 s.

Remaining work, and the decision taken on it: **do not fix these one run at a
time.** Write a generator that proposes, per candidate,
`{start, end, stack_args, evidence}` from the original XBE — start at the
pointer-table entry, walk to the first `ret N`, take the padding up to the next
known function start as the end, take `N` as `stack_args` — and review its output
against `scripts/inspect-jsrf.py disasm` before anything is written into
`config/recovered-functions.json`. `scripts/inspect-jsrf.py disasm` is the
authority; capstone in a throwaway script disagreed with it on the same bytes.

Acceptance: every candidate is either recovered with its own `ret N` evidence or
explicitly recorded as a deliberate fragment with the reason; a strict 30 s run
is archived; the run's next stop is named.

**Known open risk from this packet:** `0x00048690` is a pointer-table target with
no span and `0x00048570`'s corrected end now stops just short of it. Nothing has
called it yet; if something does it traps **by name** rather than executing
`0x00048570`'s body twice, which is the intended behaviour, but it needs its own
entry. It is the first item for the generator.

### A2d — The APU MMIO decode failure



**A2d.1 delivered.** The mechanism is neither of the two the packet posed: the
guest never handed a pointer to a CRT routine, and no host routine was chosen to
touch MMIO. **The recompiler** lowered a guest `rep movsd` over an APU-sourced VA
into a host `memcpy`, which faulted inside `VCRUNTIME140` in VEX encoding the
guest-instruction VEH decoder cannot read, so the access violation escaped. Fixed
by guarding the `movs`/`stosb` block forms with `recomp_range_is_mmio` (toolkit
`484887b`; forward-ported into the generated chunks). The GP read is now named
rather than silently zeroed. The planning-relevant mechanism and bounded result are recorded here; consult chronology belongs in `docs/jsrf-operating-history.md`.

**A2e delivered in the same session.** The next stop, `[ICALL] Failed to resolve
VA 0x000252B5`, was not an unresolved indirect call either: it was the shared
epilogue of the switch at `0x00025040`, emitted as a tail call because that
entry's declared span stopped 0x82 bytes early at a `gap_prologue` **false
entry**. Widening the span to `0x00025040..0x000252B9` made it an internal label:
`0x00025040 returned; ABI verified`, three traps dropped, the emitted switch went
from 6 to **8 of 8** targets, and the stop moved to `0x0007E255`.

This is the **A4a class** arriving early: a span boundary that mis-states
ownership. It was fixed with evidence rather than a heuristic — exclusivity of the
false entry was measured three ways from the image (one control-transfer
reference, and that from inside the body; zero dword occurrences anywhere, so
never address-taken; and stack arithmetic that balances only under the parent's
pushes, because the callee is `stdcall`). The advisor was consulted first and
raised the bar to *necessary CFG inclusion*, explicitly **not** exclusive
ownership; the extra two tests were then added to meet it. Do not generalise this
into a broad "widen the span" rule — A4a still owns that, and this packet only
established one entry.

**Next stop, named: `0x0007E255`.** The next packet (A2f) should first check
whether it lies inside a declared span — that single check separated the A2e class
from a genuinely absent function and cost nothing.

### A2f — The false entry at `0x0007E242`

**Status:** **ACCEPTED 2026-09-22.** Independently reviewed by `workbuddy-ai` /
`hy4-preview-f` at `high` (child `2aa4b25a`): **"Verdict: ACCEPT. All 7 criteria
AGREED on measurements I reproduced myself"**, CTest 11/11. That review also
independently identified the `0x000304F0` defect as its closing "LOUD NEW FINDING",
which is A2g below — so A2g's *diagnosis* was a re-derivation of work already done,
and its real additions are the fix, the strict run and the detector finding. The verdict was initially omitted from durable project state; that process failure
is the concrete motivation for P0.2's durable review transaction.
**Depends on:** A2e accepted. **Evidence:**
`logs/runs/20260922-190336-778-a2f-7e255-span/` (strict, 30 s).

`0x0007E255` **was** inside a declared span, and that was not enough to clear it:
the containing span belonged to a **false** `gap_prologue` entry `0x0007E242`,
which had truncated the true parent `0x0007E180` at exactly that address. So this
is the A2e class again, not a third mechanism — **span membership is not
ownership**. Four forward branches out of `0x0007E180` fell beyond its declared
end and were emitted as tail calls: one into the wrong function (`sub_0007E242`,
which happens to be defined) and three into the deliberate trap at `0x0007E255`.
An independent confirmation the A2e case lacked: `0x7E180`'s body contains a
21-byte **inline copy** of the same tail at `0x7E22D..0x7E241`, byte-identical to
`0x7E242..0x7E256` except for two call displacements — a compiler tail-merge, not
a function. Fixed by widening `0x0007E180`'s end `0x0007E242 -> 0x0007E257`.
Measured: `0x0007E180 returned; ABI verified`, `0x0007E255` absent, stop moved.

### A2g — The span that ended at its own switch dispatch

**Status:** **ACCEPTED 2026-09-22.** Independently reviewed by `workbuddy-ai` /
`hy4-preview-f` at `high` (child `1415063e`): **AGREED on all eight criteria**, with
one correction applied (`[ICALL]` is 1, not 0 — only the trap addresses are absent)
and one nuance recorded (the widened span now duplicates `0x30570`'s bytes; its own
entry, body and dispatch are preserved). **Depends on:**
A2f accepted. **Evidence:** `logs/runs/20260922-224429-003-a2g-304f0-span/`
(strict, 30 s); baseline `logs/runs/20260922-190336-778-a2f-7e255-span/`.

The A2f fix exposed an ABI failure at `0x000304F0`, and it was **not** an ABI
defect: the declared end `0x00030508` was the **switch dispatch instruction
itself** (`FF 24 85 18 06 03 00` = `jmp [eax*4+0x30618]`), so the emitted body was
prologue + `cmp` + `ja` with no case and no epilogue, and the `0x104` it
subtracted was never restored. The `ja 0x3060E` fell outside the span and became a
tail call into the trap at `recomp_stubs_recovery.c`. Widening the end
`0x00030508 -> 0x00030618` made it an internal label. Measured: ABI failures
1 -> **0**, `ABI verified` 335 -> **345**, kernel calls 6,308 -> **6,709**,
duration 4.39 s -> **4.95 s**, switch targets **4 of 4**, traps
`3060E`/`252B5`/`7E255` all **absent**, `recovery-unresolved.json` 255 -> **254**,
build identity verified, **CTest 11/11**. Negative control honoured: `0x30570` is
a genuine adjacent entry and keeps its own body and dispatch entry.

**The packet's real deliverable is the detector, not the entry.**
`scripts/check-span-exits.py` exists for exactly this class and had reported
**none of A2e, A2f or A2g** — all three were found by a run. Measured cause: its
entry set came from `resolution_starts.runtime_starts()`, which answers *"can the
runtime resolve this"*, and a folded `tail_jump_alias` resolves **fine** because
the dispatch tuple names the **parent's** symbol. A branch into a reassigned body
therefore looked like a tail call to a real entry. New
`genuine_starts()` counts an address only when its own symbol answers it;
`runtime_starts()` is unchanged for existing consumers. Findings **283 -> 446**;
the three fixed entries are now clean. `--selfcheck` replays each case at its
**pre-fix** span, and the negative control measures the old rule at **0 of 3**
against the new rule's **3 of 3** — a self-check that passed under both rules
would prove nothing.

Acceptance: each of the three entries is recovered with its own branch-census,
dword-occurrence and stack-arithmetic evidence; a genuine adjacent function is
shown not to be swallowed; the detector detects all three at their pre-fix spans
with a negative control proving it previously did not; a strict 30 s run is
archived and compared against the A2f baseline; and the run's next stop is named.

**Next stop, named: `[ICALL] invalid target 0x00000000 return=0014982E` (A2h).**
The site is `call dword ptr [0x1C4064]` — thunk slot 65 = ordinal 277 — reached
immediately after `xbox_HeapAlloc: out of memory (requested 598869040, used
12715008/50855936)`. The first question is whether the NULL is a real
uninitialised thunk or a consequence of that failed allocation. **Do not** treat
it as a missing-function problem before that is settled.

### A2h — Displaced RAM characterized; writer still unknown

**Status:** **DISPLACEMENT CHARACTERIZED; WRITER UNKNOWN.** The executable next
step is the bounded writer investigation in
`docs/packets/a2h-writer-investigation.md`; this historical section supplies
the measured basis only. **Depends on:** A2g. **Evidence:**
`logs/runs/20260922-224429-003-a2g-304f0-span/`; controls and probes in
`scripts/check-dump-mapping.py`, `check-displaced-ram.py`,
`check-thunk-relocation.py`, `check-thunk-survival.py`.

The stop was **not** a missing thunk. The run's own log says the loader resolved
every slot (`120/120 resolved`, `Synthetic VA range: 0xFE000000-0xFE0001DC`), so
slot 65 should hold `0xFE000104`. My first reading — "the slot is zero" — came
from a dump whose guest RAM is **displaced by exactly `0x37608` bytes**. The
control: a dump must reproduce the XBE's own `.text` at guest VA `0x00011000`,
which the loader never patches. **545 of 546 archived dumps pass; only this one
fails**, and in it 460 of 545 XBE-backed pages read as `original[VA + 0x37608]`
with **0** reading as `original[VA]`, uniformly across every section; `XBEH`
occurs 0 times in the file.

**This is a guest defect, not a capture artifact**, and the live log settles it:
`original[0x1C4064] = 0xFE000104` and `original[0x1C4064 + 0x37608] = 0x00000000`
(both from a mapping-verified dump), and the ICALL macro logged `0x00000000` — so
the **guest** read the displaced value. A capture mis-mapping would have made it
read `0xFE000104`. The stack did not move (`dump[esp=0x00F7FD00] = 0x0014982E`,
the return VA the ICALL pushed, occurring once in the 64 KiB window).

**The displacement mechanism is strongly constrained, but its writer is not yet identified.**
A consult on the contradiction ranked an overlapping bulk transfer with
`source = destination + 0x37608` first and predicted the patched thunk table would
survive *relocated* at `0x001C3F60 - 0x37608 = 0x0018C958`. Measured: **110 of 120
slots survive at exactly that address**, slot 65 reads `0xFE000104`, and the
8-dword synthetic-VA sequence occurs there and **nowhere else** in the 64 MiB
window. It also warned that `0x37608` is a *separation*, not a copy length.

Acceptance for the writer-investigation/fix sequence: identify the writer of the transfer with evidence
(the advisor's cheapest experiment is a hardware write-watch on host
`0x001D4064`, armed after loader patching, inspecting the **simulated** guest
registers `ESI`/`EDI`/`ECX`/`DF`, not native ones); state its contract; fix it or
trap it; and archive a strict run whose thunk table is intact at its own address.
A `.text` control must pass for any dump used as evidence.

**Standing rule added:** `jsrf_dump.py` validates that a capture *has* a
`guest_ram=` identity but cannot tell whether that identity matches the payload.
Run the one-read `.text` control before any dump-based claim.

### A2h-r6 retired — the premise was refuted, not the writer found (2026-09-23)

**Status:** **RETIRED.** The bounded descriptor/slot discriminator
`docs/packets/a2h-writer-investigation.md` (`A2h-r6`) was reviewed for plan
adequacy and returned **INADEQUATE — retire, do not revise**. It was never
implemented: no build and no guest run was performed under it. Records:
`docs/reviews/a2h-r6-adequacy-review.md`,
`docs/reviews/startup-20260923-dsh-a2h-r6.md`.

**What was wrong.** The packet's "measured" structural claim — that the logged
`invalid target 0x00700010 ... return=0017E627` was the *slot value* read from
`[base + index*8 + 4]` and dispatched by `sub_0017DBBD` at `0x0017DC23` — was
false. Three independent records put the failure on a different path:

- `logs/runs/20260921-143340-028-run/stacks.txt:250-251`:
  `EVENT 170 kind=1 target=0017E58F site=0014B52C` immediately followed by
  `EVENT 171 kind=1 target=00700010 site=0017E627`. Same pair in
  `20260921-154535-674-resume-check`.
- The frozen dump (`.text` control passed first) holds `F7FF44=0014B52C` — the
  dispatcher's return into the walker — and `F7FF48=00700010` as its `[ebp+8]`,
  with **no** `0x0017DC28` or `0x0017C238` return word anywhere in the frame.
- The native unwind: `recomp_icall_not_code_log` ← `sub_0017E600+0x20C`
  (`recomp_0004.c:6044`) ← `sub_0014B50D+0x17F` (`recomp_0003.c:24660`).

The mechanism: the C++ static-initializer walker `0x0014B50D` walked the table at
`0x001EB760`, whose slot `0x001EB764` held `0x0017E58F`. The translation's
`tail_jump_alias` fold had mapped that address onto the **three-argument**
dispatcher `sub_0017E600` (`src/recomp/gen/recomp_dispatch.c:6865`), so a
zero-argument initializer call ran the wrong body, read `[ebp+8]=0x00700010` and
executed it.

**Already fixed.** Commit `cb7cae2` (2026-09-21 15:58:12, "Recover the static
initializer the alias fold deleted") gave `0x0017E58F` its own body, consulted
before the generated dispatch table (`recomp_types.h:869-870`). `cb7cae2` is an
ancestor of both HEAD and the packet's recorded evidence revision `40ae5bd`, and
all four runs that ever logged this failure (09-21 14:33, 14:34, 14:46, 15:45)
predate it. The archived strict run's log line 443 reads
`[RECOVERED] 0x0017E58F returned; ABI verified (ESP/EBX/ESI/EDI)`.

**Why it could not be repaired in place.** Beyond the refuted premise, the
instrument is unplaceable within the packet's own rules: the instrumented sites
are lifted statements inside the provenance-pinned generated chunk
`src/recomp/gen/recomp_0004.c`, which step 0 forbids editing while step 2 requires
the P0.7 guard to pass. No interior/basic-block trace seam exists in the toolkit
(only entry/exit/pop/after-call/tail-jump), `relift-selected.py trace` rewrites
whole bodies inside the guarded file, and the packet's required "last N" trace cap
is a "first N" countdown today (`src/kernel/recomp_trace.c:29-38`). Measured
negative control: a temporary byte change to that chunk made the guard report
`generated output src/recomp/gen/recomp_0004.c changed since the manifest`, exit 1;
the file was restored and its hash verified.

**Also recorded:** the packet misattributed the literal `0x00700010` to the A2f/A2g
runs, which contain no such failure line; it omitted the dispatch-alias entry route
that actually failed; and its decision table had uncovered cells (index ≥ count;
unreadable descriptor; row 4's "valid in-image" is not the runtime's
`RECOMP_ICALL_IS_CODE` test).

**Method note that generalizes.** The refutation needed **no code change and no new
guest run** — the collector's ICALL history plus the frozen dump were sufficient.
Prefer that route first. Two standing cautions came out of this:

1. **Absence of a log line is not a coverage witness** for an uninstrumented
   generated body. An early draft of the retirement claimed `sub_0017DBBD` "never
   executed" because zero of 651 archived logs mention it; that was withdrawn,
   because the function carries no trace hook and so would log nothing either way.
   Its execution status remains **UNKNOWN**.
2. **"Measured" in a packet is only as good as its evidence profile.** The refuted
   premise was assembled from exploratory, pre-fix runs and presented as
   structural fact; the profile was disclosed but its staleness was not checked.

**Next.** No packet is promoted. Reactivating this area requires a new packet with
stable criterion IDs and its own adequacy review, based on the **current** strict
stop (`normal_exit` / `exit_code 0` after 1.92 s via `HalReturnToFirmware(2)`,
`logs/runs/20260923-013448-357-p0-strict-baseline`) — not on any 09-21 exploratory
run. Any future observation *inside* a generated function needs a separate tooling
prerequisite packet first (a seam outside the guarded files, or a declared
reversible diagnostic delta the guard recognises; a last-N trace mode;
known-good/known-bad fixture controls).


The run stops 4.1 s in on:

```
[APU] MMIO decode fail at RIP=00007FFA628FCCA7 offset=0x30200: C5 FE 6F 02 C4 A1
[EXCEPTION first-chance] tid=21528 code=0xC0000005 RIP=0x7FFA628FCCA7 fault=0xFE840200 (read)
```

Facts: the faulting RIP is in a system DLL, not in the recompiled image; the
bytes there are a VEX-encoded AVX instruction (`C5 FE 6F 02` =
`vmovdqu xmm0,[edx]`); the fault address is `0xFE840200`, inside the APU
aperture. Guest regs at the fault: `eax 0x118 ecx 0x46 edx 0x13 ebx 0x118
esi 0xFE830200 edi 0x010DF724 esp 0x00F7FE74`. The VEH instruction decoder in
the toolkit's APU hook recognises legacy MOV/CMP/OR/AND/LEA forms only, so it
could not emulate the access and the fault escaped as an access violation.

First question to answer with evidence: is the guest legitimately handing a host
CRT routine a pointer into the APU aperture, or is a host routine being used to
touch MMIO at all? Both answers have different fixes.

Acceptance: the mechanism is stated with the instruction that faults and the
call path that put a pointer there; the fix either decodes the encoding or stops
the access reaching a host routine; a strict 30 s run is archived and compared
against `logs/runs/20260922-162546-668-a2c-175300/`; and the run's next stop is
named.

**Do not** make the aperture readable as the fix. That converts a named failure
into a silent wrong read, which is worse than the trap.

### A3 — Make GPU acceptance describe implemented behavior



Split this work into sequential leaves so Luna can own one class/action at a time:

- **A3a — Classification :** separate observed methods from
  reviewed state setters and action executors. Acceptance: each accepted method has
  a class-specific contract and implementation location; observed-only actions
  remain explicitly unsupported. GET=PUT is labeled consumption, not proof of
  completed work. Unsupported-stream tests preserve the documented rollback rules.
- **A3b — First required action :** select the earliest
  unsupported action from an archived real stream and establish its inputs,
  ordering, memory effects and completion contract before implementing it.
  Acceptance: an independently specified fixture verifies the observable effect
  (for example destination bytes or notification/semaphore writeback), including
  negative cases. Sink entries and register storage alone do not count. Verify
  class binding at the time of each command, including a subchannel rebind.
- **A3c — Repeat and integrate :** repeat A3b as separate
  bounded packets for subsequent required actions. Acceptance: the reviewed real
  stream reaches a documented semantic checkpoint, unsupported actions still stop
  explicitly, and completion cannot outrun modeled execution. Preserve an explicit
  renderer-pending status unless a guest rendering result is actually verified.

For all leaves, run affected NV2A contracts and submission probes; archive a bounded
guest run when claiming guest progress. Do not bulk-whitelist an observed inventory
to make a ring drain. If the execution/queue design cannot represent the required
ordering, consult the advisor for an architecture decision before implementation.

### A4 — Repair translation entry and branch semantics



- **A4a — Adjacent entries :** replace the
  assumption that abutting spans imply equivalent function entries with a proven
  ownership/entry rule. Acceptance: a semantic fixture with distinct adjacent
  `ret 8` and `ret 16` bodies dispatches each original VA correctly and preserves
  stack/register behavior. Proven interior fragments remain supported; existing
  recovered/manual overrides are not lost during regeneration.
  **A2g supplies the detector this packet should be built on, and one hard
  constraint.** `resolution_starts.genuine_starts()` already separates *resolvable*
  from *an entry in its own right* — the distinction whose absence let A2e, A2f and
  A2g all reach a run undetected — and `check-span-exits.py --selfcheck` is a
  positive control that fails if that sensitivity is lost. Reuse both rather than
  writing a fourth census. The constraint is the advisor's, from consult #3:
  *"preserving genuine independent entries and absorbing internal false entries are
  opposite operations"*, so a rule that fixes one can break the other. A2g honoured
  it with an explicit **negative control** (`0x30570`, a genuine adjacent entry,
  keeps its own body and dispatch after the widening) and that control must be part
  of A4a's acceptance, not only of its investigation.
- **A4b — Missing branches :** inventory
  the current 40 generated missing-label rewrites and preserve taken-edge semantics.
  Acceptance: each remaining edge is represented correctly, proven unreachable with
  evidence, or produces a deterministic diagnostic. No potentially executable edge
  silently becomes `(void)0`. A taken conditional cross-boundary fixture checks an
  observable result rather than merely compilation or emitted text.
- **A4c — Regeneration validation :** regenerate through the
  documented full translation/recovery pipeline with one build owner. Acceptance:
  generated declarations, definitions and dispatch agree; relevant toolkit semantic
  regressions and all current game Release CTests pass; build identity is verified;
  a same-profile bounded run is compared with the pre-change artifact. Report any
  newly exposed diagnostic as an unresolved defect, not an acceptance success.

Do not implement a broad alias heuristic merely to reduce a deletion count. If
entry preservation needs a new translation interface, ask the advisor to review
the architecture and return the resulting small implementation packets to Luna.

### A5 — Independent acceptance reconciliation and return to the main milestones



Acceptance:

- Review artifacts establish the behavioral criteria above, not just passing
  suite totals; resolve high-confidence correctness findings and explicitly record
  any advisor-approved changes to the criteria.
- Run the current Release CTest suite, affected toolkit regressions and harness
  checks appropriate to the changed interfaces, with builds and runs serialized.
  Archive strict and, when useful, separately labeled exploratory results.
- Update 11b/11c and dependent milestone statuses from this evidence. Record what
  consumes commands, what executes actions, what completes work and what renders.
  Choose the next bounded milestone from the actual strict-profile stop.
- Update this plan, review records, and handoff guidance with both revisions, exact
  validation and remaining risks. Long-form narrative, if useful, belongs in
  `docs/jsrf-operating-history.md` rather than a live report.

## Historical checkpoints and original milestone detail

The following investigation narrative and milestone snapshots predate the audit
above. Their "current"/"next" wording is historical and does not override A1–A5.
A1 must reconcile the relevant rows before they are used for new acceptance.

Historical checkpoint: `logs/runs/20260921-174239-356-reverted-clean/` (clean,
diagnostics reverted; 154 recovered bodies verify, CTest 11/11). Toolkit
`f5fbdea`, game `d40e936`.

**Where the guest is now.** The PFIFO low-mark spin is fixed (toolkit
`f5fbdea`), so the hang is gone and the guest runs a long way into startup: CRT
initialization completes, GPU setup completes, the pushbuffer kick chain returns,
and **154 recovered bodies verify with zero ABI failures**. The stop is a **NULL
indirect call**: `[ICALL] invalid target 0x00000000 return=0006FA51`.

**Root cause traced four levels down.** `sub_0006F9E0` builds an object and stores
it in `0x22FCE0` (the game's central object, 2341 references, exactly one store in
the whole tree), then calls `vtable[0](this, 1)`. A targeted probe shows the
constructor `sub_00012210` returns `0x38` instead of the object because its `esi`
was destroyed: the saved-ESI slot changes across `sub_0005F350`'s call to
`sub_001680D0`, which therefore writes above its own frame. ESP inside
`sub_0005F350` is constant, so it is not over-popping.

**Next packet — the chain is now pinned to a call, one level short of an
instruction.** An inner call returns with ESP **4 bytes too high**, so
`sub_0005F350`'s epilogue pops read the wrong slots and restore `g_esi = 0x38`
instead of the object. Measured: `sub_0005F350` returns +8 (frame base
`0x00F7FEC0` → `0x00F7FEC8`) across `sub_001680D0`; inside it, the **indirect call
at `0x168111` targets `0x001690A0`** (caught from inside the ICALL macro by matching
the pushed return address `0x00168114`) and comes back +4. `0x001690A0`'s own
`ret 4` delta is correct, so the over-pop is inside it or its callee `0x168F60`
(whose epilogue also looks balanced). **Next:** probe `esp` at `0x001690A0`'s entry
and return to settle which one, then read the offending body's `esp` adjustments.

**The general fix is worth doing first — it is the same amount of work and finds
all of them.** `RECOMP_ABI_CALL`'s `g_esp < _ap + 4` is a lower bound (deliberately
— the header explains that no single value is correct in general), so an
over-popping callee passes silently. Where the delta **is** known it can be
compared exactly, and both sources exist already: `stack_args` for recovered
entries, the body's own `esp += N` for generated ones. Generate a
`va -> expected delta` table and check equality when the entry is present.

**Three instrumentation errors are recorded so they are not repeated:** scope probe
insertion to one function body (a whole-file pass interleaved probes from several
functions); anchor one watch address once instead of reading `MEM32(esp+4)` at
sites with different `esp`; and never treat a static push/pop count as a per-path
count (`sub_0005F350` is 4/1 and balanced because three pushes are arguments;
`sub_0006E360` is 1/2 and balanced because the pops are on different exits), nor
`returns=0` as "no exit" (a tail jump is an exit). Do **not** use a window-based
frame canary: `__SEH_prolog` builds the exception frame inside its caller's frame
by design. And note **`RECOMP_ABI_CHECK` is off by default**, so every
`RECOMP_ABI_CALL` compiles to a plain call unless the build passes
`-DRECOMP_ABI_CHECK` — a diagnostic that reports "no hits" without it is reporting
nothing.

**The guest no longer crashes.** Outcome is `diagnostic_deadline` (exit 3) rather
than an unhandled exception: CRT initialization completes, GPU setup completes,
and the pushbuffer kick chain that this plan listed as the fatal stop now runs and
returns — `0x00190FB0`, `0x00190240`, `0x001917F0` and `0x001918E0` all log
`[RECOVERED] ... ABI verified`. So the previous checkpoint is **passed**, not
merely retained. 98 recovered bodies verify, 0 ABI failures, guest events 78,441
runaway → 563, CTest 11/11.

The new state is a **hang, not a fault**: 797,491 first-chance `C0000005` access
violations in a tight fault-and-retry loop (~43k/second), with the most frequent
indirect target `0x002652B0` — an address in `.data`'s **BSS tail** (`.data` is
`raw_size` 0x44574 but `virtual_size` 0x92914). The guest stack is healthy at the
deadline (`esp=0x00F7FB98` of `0x00780000`..`0x00F80000`).

**Where the guest is: inside `sub_00192090`, the GPU device-setup function this
plan records as "has not returned".** The deadline dump shows
`sub_00191BA0` → `sub_0018E160` → `sub_00194C3F` → `sub_00194A72` →
`sub_00194300` beneath it, two recovered vtable methods live in the chain, and the
last kernel calls coming from GPU-range sites. So the guest is **initialising its
renderer**, not stalled in startup.

**The ~800k first-chance `C0000005`s are not errors — they are the NV2A MMIO hook
doing its job.** `src/main.c:73` offers every fault in guest range
`0xFD000000`..`0xFE000000` to `nv2a_hook_handle_mmio` and continues execution when
the hook handles it; `0xFD000000` is the NV2A register aperture, and the guest's
`esi` at the deadline is exactly `0xFD000000`. No `[EXCEPTION first-chance]` line
reaches the game log because the hook returns TRUE first.

**So the hang is a GPU register poll that never terminates** — the guest reads an
NV2A register ~43,000 times a second inside device setup and the modelled value
never satisfies the loop's exit condition. That is a **register-contract gap in
the GPU model (11b territory), not a translation defect**, and it is the first
time this port has reached live NV2A polling in device setup.

**Next packet — and this corrects the guess above.** The ~800k faults are not a
wait on one register. With toolkit `dc79321` (`RECOMP_MMIO_TRACE`) the trace shows
**400,000+ MMIO accesses in 12.6s across 100+ distinct offsets with no offset above
8 occurrences, all from a single RIP**, advancing `0x700000, 0x700004, 0x700008,
…`. That is a **linear dword-by-dword sweep of the PRAMIN / instance-memory
window** (`device+0x700000`) inside the GPU device-setup chain `body_00192090` →
`sub_00191BA0` → `sub_0018E160` → `sub_00194C3F` → `sub_00194A72` →
`sub_00194300`. So the guest is **scanning instance memory for something the model
does not contain** — a GPU-model content gap, not a register contract and not a
translation defect. **Next packet:** identify the object being searched for; raise
`MMIO_TRACE_SLOTS` past 96 (it filled, which is why the hottest count is only 8) to
see whether the sweep terminates or wraps, then read the scanning site in
`body_00192090` and compare its key against what the model writes into the claimed
RAMIN window. Do **not** chase the interrupt angle. **BSS mapping is already ruled
out**: `xbox_memory_layout.c:1225` does `memset(XBOX_VA(sec_va), 0, sec_vsize)`, so
the loader is correct and `0x002652B0` must have been written at runtime.

Two manifest gaps were closed to get here, both mine rather than the game's:
1. **The bound must be the next function entry inside the range, not the alias's
   DB end.** `0x00150C00` is 112 bytes but its bound was `0x00150EA0`, so its body
   swallowed `sub_00150C70` (560 bytes) and reported two epilogue deltas instead
   of one. 636 of 1228 bounds were tightened. This answers the earlier range trap
   from the other side too (there the bound was too small).
2. **`stack_args` must be regenerated, not just edited.** It only affects the
   wrapper text in `recovered.c`; editing it after `recover-functions.py` leaves
   the old value in place and the fix looks inert. And **reviewed values win over
   any derivation** — the re-derivation wrongly reset `0x00190FB0` from 12 to 0.

**The link blocker is cleared as of 2026-09-21.** `build/Release/jsrf_recomp.exe`
builds with a verified source/executable identity stamp.

**The alias fold's real mechanism is identified, and both blocking classes are
recovered.** The disassembler reads a data table of `.text` addresses as a switch
table. A COM vtable looks identical by that test, so every vtable method was
classified `tail_jump_alias` and folded into the next function — body deleted,
dispatch tuple pointing at the wrong function. The two classes this broke, both
now fixed by explicit recovery in `config/recovered-functions.json`
(**1228 entries**, exe 9.4 MB → 13.2 MB):

1. **The CRT initializer tables** (60 targets, 44 folded). Table B is exactly the
   14 initializers milestones 02/03/04 recovered. The guest now completes CRT
   initialization and reaches heap allocation.
2. **COM vtable methods** (1120 with a redirected dispatch; 1111 recovered, 9 are
   genuine fragments the translator refuses to lift standalone). Demonstrated
   defect: the interface table at `0x001E0F00` is `[0x0014CF20 QueryInterface,
   0x0014CF00 AddRef, 0x0014CF80 Release, ...]`, stored by its constructor
   `0x0014CDB0`. `sub_0014CFB0` calls `vtable[0]` at `0x0014CFDE`, which the fold
   redirected back to `sub_0014CFB0`, so the guest self-recursed 28 bytes of guest
   stack per level (~13,000 levels) until the host stack overflowed.

**How a vtable is identified** (reusable, and the discriminator that finally
worked): a data table whose entries are a run of **distinct** `.text` addresses,
stored by a constructor as an immediate in the generated code. **The read must
stop at the first repeated target** — reading past the table into adjacent
`.rdata` let an unrelated duplicate reject the whole table, which is why the count
went 384 → 1120 on the second pass. A switch table repeats almost immediately, so
a distinct run of ≥ 3 separates the two.

**Verified now:** the self-recursion is gone (no `0xC00000FD`), 62
`[RECOVERED] ... ABI verified` lines (up from 56), `AddRef` `0x0014CF00` passes,
CTest 11/11.

**Open and immediate:** `stack_args` for the 1113 mechanically-derived entries is
computed from the translator's own epilogue (`esp += N; return;` →
`stack_args = N - 4`) and validates 54/55 against the reviewed entries that already
carried a value — **but it is not correct for every entry.** The wrapper caught
`0x0014CF20` (both exits `RET 0xC`, corrected to 12) and now stops on `0x00150C00`,
where the derived 16 is 8 on the taken path. **Functions with several distinct
epilogues cannot be resolved statically from one number.** Until that is
refined, treat the run as a work-in-progress manifest rather than a regression:
the previous stop was an unbounded recursion and it is gone.

The first real guest stop after `guest_entry` was **not** in the GPU path and
**not** in the C++ EH machinery. It was a translation defect in the alias fold,
and it is now fixed for the whole CRT-initializer class:

- The CRT calls a set of function pointers with **no arguments**:
  table `0x001EB760`..`0x001EB76C` walked by `_initterm` `0x0014B50D`; tables
  `0x001EB770`..`0x001EB83C`, `0x001EB840`..`0x001EB854` and
  `0x001EB854`..`0x001EB86C` walked by `_initterm` `0x0014B4B5`; and the hook
  global `[0x0022ED2C] = 0x0017BF79`.
- 60 distinct targets. **44** had been folded into a later entry by the
  `ff4d442` abutting-alias rule, which deleted the body and pointed the dispatch
  tuple at the wrong function.
- Proof case `0x0017E58F` (table `0x001EB764`): the fold wrote
  `{ 0x0017E58F, sub_0017E600 }`, and `sub_0017E600` is the three-argument
  catch-frame invoker, so the zero-argument call read `[ebp+8] = 0x00700010` off
  the stack and executed `call eax` on it. Every register matches that reading:
  `esi=0x001EB764` / `edi=0x001EB76C` are `_initterm`'s own loop pointers, and
  `ebp=0xC` is `arg1+0xC` with `arg1=0`.
- All 44 are function entries **by definition** — they are only ever reached
  through those tables — so all 44 are recovered in
  `config/recovered-functions.json` (`kind: "routine"`).
- Pre-fix evidence that this is a regression: in
  `logs/runs/20260921-111607-936-kick-ack/source.zip` the dispatch read
  `{ 0x0017E58F, sub_0017E58F }` and `recomp_0007.c` defined a clean standalone
  body. The fold is the regression, and it is what moved the stop from the GPU
  kick (`0x001918E0`) to startup.

**Range trap, worth not repeating:** an entry's `end` must be the alias's own DB
end, not the first terminator found by linear disassembly. `0x00181212` ends in
`ret` at `0x00181224` but branches to `0x00181225`, so a linear-scan bound
truncated the body and turned the branch into an unresolved call to
`0x00181225`. 40 of the 43 bounds had to be widened after that failure.

What actually blocked the link is worth keeping in the record, because none of
the three causes was the one previously recorded here. The dominant cause was
not a detection gap at all: `scripts/recover-functions.py` was overwriting
`src/recomp/gen/recomp_stubs_unresolved.c`, a file it does not own, deleting the
541 stub bodies the full translation pass had written and refilling it from its
own 70-entry view. The header declared 541 unresolved stubs while the file
defined 3. Two smaller causes sat underneath: mid-body `tail_jump_alias` targets
were emitted as tail calls (`sub_X(); return;`) instead of `goto loc_X`, and the
full pass was being run without the project's manual-function list. Those three
causes are the planning-relevant record; detailed chronology belongs in
`docs/jsrf-operating-history.md`.

Two defects are known and are **not** link blockers. Both are correctness
defects that a successful boot would not reveal:

1. **CLOSED.** Live jumps silently rewritten to `(void)0`: same-function
   deletions went 1032 → 0. Cross-function deletions were 345 → 40. Do not
   weaken the label validator; cross-function `goto` is not C.
2. **CLOSED, but the fix was too broad — see item 5.** `tail_jump_alias`
   entries are no longer emitted as standalone bodies; they are folded into a
   parent and dispatched under the parent's symbol. The fold is correct when
   the alias lies *inside* its parent (the same-end rule), and wrong when the
   alias merely *abuts* it.

Two further items remain:

3. **RESOLVED BY RE-INIT, and this is still the user's call.** The game
   repository's `.git/objects` was missing entirely — no loose refs, no
   `packed-refs`, no remote — so no commit could be made and the working tree
   was unversioned. It was rebuilt with `git init -b master` and a single
   commit; the old directory is preserved as `.git.broken-backup/` (gitignored)
   and history before 2026-09-21 no longer resolves. The reflog's tip was
   `7e2c4f43`. **Note this contradicts the earlier instruction in this item that
   `git init` must not be run as a "fix".** No source was lost — the working
   tree, `config/` and `game/` are intact — but if a backup or a preferred
   remote exists, restoring it is still preferable to the rebuilt history.
4. **The toolkit fix is committed.** `tools/recomp/lifter.py`,
   `tools/recomp/translator.py` and the new
   `tools/recomp/test_tail_jump_into_batch.py` +
   `tools/recomp/test_alias_body_suppression.py` are toolkit revision
   `58a9cf9`, on top of `488286f`. Later toolkit revisions this session:
   `a301962`, `5d68f27`, `ff4d442`, `db54746`, `9568f29`. The toolkit repo is
   healthy and the game repo's `AGENTS.md` records the revision.
5. **PARTLY FIXED — the alias fold deletes real function bodies.** The
   "abutting alias" rule added by `ff4d442` assumes an alias whose `end` equals
   the next real entry's `start` is a fragment of that entry. Measured against
   the current tree that assumption is false in the general case:
   - 3123 dispatch tuples redirect a VA to a different symbol;
   - only **87** of those have a `loc_<VA>` label in the parent, so 3036
     redirect to a body that does not contain the address at all;
   - of the 971 aliases adopted by this rule, **759 end in `ret`** and 132 in
     `jmp`, i.e. the large majority are self-terminating, not fragments.

   **Fixed for the class that actually blocks boot:** all 44 CRT initializer
   targets whose bodies were deleted are now recovered explicitly (see the
   checkpoint above). That is the complete set for the initializer tables, so
   no further boot stop can come from this class.

   **The general rule is still open.** Three discriminators have now been tried
   and **all three discarded because they saturate** — do not re-propose them
   without new evidence:
   - "the last decoded instruction ends exactly at the alias end": `int3`
     padding decodes as instructions, so the defect case passes it too;
   - "the alias body ends in `ret`": fires on 759 of 971;
   - "the alias address is entered from data": fires on **2651 of 3123**,
     because a switch table is also a run of `.text` addresses, so this cannot
     tell a function-pointer table from jump-table data.

   What remains is a **structural** test rather than a byte heuristic: fold an
   alias only when its body actually needs the parent's labels (it contains a
   `goto loc_X` for a label the parent emits); otherwise emit it as its own
   body. That is what `ff4d442` was really fixing for 56 aliases, and it is the
   only signal so far that separates the two shapes by construction. Unfolding
   the rest is a large change (restores ~1.4 MB of bodies, re-introduces the
   deleted-goto problem the fold was hiding) and needs its own packet, its own
   regeneration and a fresh baseline. **Until then, an address that is entered
   from data must be recovered explicitly** rather than trusted to the fold.
4. **The toolkit fix is committed.** `tools/recomp/lifter.py`,
   `tools/recomp/translator.py` and the new
   `tools/recomp/test_tail_jump_into_batch.py` +
   `tools/recomp/test_alias_body_suppression.py` are toolkit revision
   `58a9cf9`, on top of `488286f`. The toolkit repo is healthy and the game
   repo's `AGENTS.md` records the revision.

## Goal and working rules

First deliver a small playable slice: boot from the extracted retail files,
reach the title/menu, start a new game, load the opening playable area, skate,
jump and spray graffiti, then save and resume. Expand that slice until the
whole campaign and unlockable content work. Audio, progression, transitions
and saves are part of a complete port. Resolution upgrades and mods come later.

This is an evidence-driven backlog, not a promise that the existing toolkit
already supports every required subsystem. Split each milestone further when
it reveals more than one independent defect. Finish and verify one fix before
moving to the next; update this document after each session.

- Keep original XBE/data unchanged. Store host saves separately and back them up
  before testing writes or migrations.
- Record the first failing guest address, caller, arguments, stack convention,
  expected behavior, change, and before/after evidence.
- Recover real code for missed game functions. Replace identified library or
  hardware boundaries with implementations that satisfy the caller's contract.
  Record every temporary stub and its removal criterion; returning success
  without producing required state is not completion.
- A disappearing error is insufficient: verify that the operation happened
  and later execution still reaches the previous checkpoint.
- Use the toolkit docs as examples, not proof that JSRF has Burnout's engine,
  addresses, memory needs, or behavior. Confirm against this XBE and, when
  available, the same scene on original hardware or xemu.
- Version the project and toolkit changes separately. The user requested a
  local committed checkpoint; the game source and companion toolkit changes
  are committed independently, with the toolkit revision recorded in AGENTS.md.
  Preserve later uncommitted work while reviewing changes. Do not commit game
  assets, generated build products, or saves.

## Baseline (2026-09-12)

Project: `C:\Users\logic\Repos\my_xbox_game`.
Toolkit: `C:\Users\logic\Repos\xboxrecomp`.

- Executable: `build\Release\jsrf_recomp.exe`; launch with the **project root**
  as working directory. `src/main.c` resolves both `game\default.xbe` and
  `jsrf_run.log` relative to the working directory.
- XBE title ID `0x5345000A`, XDK 4134, entry `0x00148023`.
- XBE SHA-256: `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C`.
- Existing analysis: 8,437 functions, 120 kernel imports, 11 loaded sections.
- Two old instances were stopped after preserving `logs/jsrf-before-session.log`.
- A fresh 15-second run reproduced the first missing target `0x0017D15C`.
  Baseline saved as `logs/jsrf-baseline.log`; process required termination.
- Four startup warnings precede it: unbridged ordinals 204, 144, 91 and 8.
  These remain unresolved work; they are not the failed runtime call fixed here.
- Later logged failures include `0x0018AFB0`, `0x0018B1A0`, `0x0018B390`,
  `0x0018B580`, `0x0018B770`, `0x0018B960`, `0x0018BB50`, `0x0018BD40`,
  `0x0018C1D0`, `0x0018C3C0`, `0x001BDAA9`, `0x0018E410`, `0x001A5299`,
  and `0x001A52A4`. This is observed coverage, not a complete missing-function list.

## First fix: CRT memmove

The target `0x0017D15C` is an internal basic block of `memmove` at
`0x0017CEC0`, not a standalone function. The function-identification output
names it memmove, and the original instructions confirm a three-argument copy
with an overlap check and forward/backward paths. The generated body ends at
`0x0017D15A`, before the backward-copy remainder blocks and epilogues.
Its negative-index jump table at `0x0017D150` is emitted as `RECOMP_ITAIL`.
Failure skips real copy/return work and leaves the guest frame incorrectly handled.

Implemented the routine in `src/jsrf_crt.c` using host `memmove` after
translating Xbox addresses. Arguments are `[esp+4]=destination`,
`[esp+8]=source`, `[esp+12]=size`; EAX returns the guest destination and ESP
advances by four bytes for RET. The caller removes the three cdecl arguments.
The implementation leaves the guest nonvolatile registers untouched.

Removed its broken generated definition from `recomp_0007.c`, kept the existing
symbol for direct callers and dispatch, and registered it in manual lookup.
`config/manual-functions.json` preserves this decision during regeneration.
Do not add `0x0017D15C` as a no-op or an independent function seed.

Validation: Release build and the `jsrf_memmove` CTest pass. The test checks
36,900 cases with sizes 0–1024, all four alignments, both overlap directions,
identical pointers, disjoint ranges, full-buffer bounds, return value, and
stack cleanup. The first corrected game run removed this failure and retained
all later missing-target checkpoints, starting at `0x0018AFB0`.

Enabling the toolkit watchdog exposed a separate host diagnostic defect:
`xboxrecomp/src/kernel/xbox_memory_layout.c` used `getenv` without including
`stdlib.h`. Added that include to supply the proper pointer-return declaration
on x64. An intermediate diagnostic run crashed before guest entry; its archived
log is retained and must not be counted as game progress.

Final verification: `logs/jsrf-verified-20260912-210815-058.log` records the
replacement copying 48 bytes on its first invocation, no `0x0017D15C` failure
and no access violation. The next failed target is `0x0018AFB0`. The watchdog
captures the remaining hang at 15 seconds with 388 indirect calls and guest
ESP `0x00F7FC24`, then exits with code 3. No game process was left running.
This establishes a repaired operation, not a booted menu or a resolved hang.
Build warnings remain in generated code and the toolkit; the new CRT source
builds without warnings. `logs/build-first-fix.log` holds the final build output.

## Repeatable workflow

From the project root in PowerShell:

```powershell
python -X utf8 scripts/build-jsrf.py
ctest --test-dir build -C Release --output-on-failure
python -X utf8 scripts/run-jsrf.py --seconds 15 --label checkpoint
Get-Content .\jsrf_run.log -Tail 50
Select-String .\jsrf_run.log -Pattern '\[JSRF\]|Failed to resolve|\[CRASH\]|\[WATCHDOG\]'
```

The build wrapper regenerates reviewed recovery functions and compiled test
fixtures, stops on failure, and verifies source identity across compilation.
The runner refuses stale builds or concurrent game copies. It uses the project
working directory, an external debugger with a deadline, and archived symbols,
source, log, thread report and dump. It restores the environment and stops only
its launched process tree. Inspect `result.json`: a diagnostic stop is not game
completion. Direct interactive launch remains possible from the project root,
but the bounded runner is the acceptance and investigation workflow.

When regeneration becomes necessary, run from the toolkit root and pass this
project's analysis directories explicitly:

```powershell
python -m tools.recomp ..\my_xbox_game\game\default.xbe --all --split 1000 `
  --game-name "Jet Set Radio Future" `
  --disasm-dir ..\my_xbox_game\tools\disasm\output `
  --func-id-dir ..\my_xbox_game\tools\func_id\output `
  --manual-functions ..\my_xbox_game\config\manual-functions.json `
  --gen-dir ..\my_xbox_game\src\recomp\gen
```

This is the original full-regeneration command, not the current incremental
workflow. It has NOT been validated with all subsequent recovery changes.
Before full regeneration, stage an analysis directory whose `functions.json`
is the merged `functions.recovered.json`, and point `--disasm-dir` there.
Do not overwrite the original database. Snapshot generated files first and review
the diff. Re-running disassembly can lose recovered names. Check generated
headers, dispatch and direct callers as well as the changed body. Keep manual
lookup and the manual-function manifest synchronized.

## Harness audit and improvement gate (2026-09-12)

This audit describes the harness BEFORE improvements. Its original output was
sufficient for sequential triage but lacked reliable multithreaded capture.
Stages 01a/01b and core 01c/01d are now implemented and verified below. Broader
thread-bootstrap, wait and scheduling coverage remains explicitly open.

### What the original output captured

- `src/main.c` logs the native fault instruction/address and the faulting
  thread's guest registers for access violations. It initializes DbgHelp but
  does not walk/symbolize native frames or write a crash dump. The vectored
  handler sees first-chance exceptions too: a `[CRASH]` line alone does not
  establish that an exception went unhandled.
- The watchdog's `GS address value` lines are raw guest stack words. Some are
  return addresses; others are arguments, data or stale values. The 16 ICALL
  entries are recent indirect targets, not a call chain, and omit direct calls.
- `xbox_WatchdogStart` stores pointers to only the calling thread's registers.
  Its stack scan uses the main stack's bounds and reads while execution runs;
  it is neither an all-thread dump nor a consistent frozen snapshot.
- The ICALL ring, index and count are shared `volatile` globals updated with
  ordinary writes/increments. Concurrent writers can mix or lose history and
  introduce data races in diagnostics. `volatile` supplies no synchronization.
- Guest workers already receive thread-local registers and separate simulated
  stacks/TIB allocations. That is a useful foundation, not proof of correct
  scheduling or synchronization semantics. The first guest system-thread call
  runs inline by default; subsequent calls can spawn real Windows threads.
- The toolkit already reports some critical-section owners/waiters and provides
  `RECOMP_CS_TRACE_CRT=all` plus `RECOMP_CS_WATCH=<guest VA>` for focused traces
  (enable the former with the latter). Its guest backtraces are heuristic stack
  scans. Reuse these hooks, then validate ordering and concurrent access.
- The current runner archives a log and terminates a timed run. It does not
  preserve matching symbols/build metadata, capture worker contexts, or assert
  milestones. The present Release output has no game PDB or linker map.
- stdout and stderr independently open the same filename with different modes.
  Their file positions can overwrite output; a run needs one coherent logging
  path before concurrent multi-line reports are trustworthy.

### 01a — Reproducible run artifacts and native symbols

Keep optimized code but generate matching PDBs and a linker map. Give each run
an artifact directory containing its log, executable/symbol identity, XBE hash,
build configuration, toolkit revision/local-change identity, diagnostic settings,
duration and outcome (normal exit, unhandled exception, diagnostic deadline,
or collector failure). Add explicit boot checkpoints and expected-checkpoint
checks. Consolidate log output; tag diagnostic events with monotonic time and
thread identity, and keep multi-line records together or use structured events.

Acceptance: two runs remain independently interpretable after a rebuild;
a controlled fault resolves to the correct source/function; timeout is never
reported as game success; concurrent test messages do not overwrite each other.

### 01b — Crash and hang collection across all threads

Use an external collector/debugger for unhandled-exception and timeout dumps,
so collection need not acquire locks in a broken game process. Distinguish
handled first-chance exceptions from fatal ones. Capture all native thread
contexts/stacks with matching symbols and sufficient guest RAM to inspect
guest stacks, globals and objects. Register guest threads with stable guest/host
IDs, start routine, actual stack bounds, TIB/TLS address and lifecycle state;
preserve the association in dumps. Take the dump before watchdog termination.
Retain a bounded fallback if collection itself fails.

Acceptance: an intentional worker-thread fault identifies that worker and
its guest state. A two-thread deadlock yields both native stacks and both guest
stacks, while a pure CPU spin still produces a useful dump. Thread exit during
collection must not leave dangling register pointers or invalid stack reads.

### 01c — Thread-safe recent history and synchronization evidence

Replace shared unsynchronized call history with bounded per-thread buffers and
safe publication/snapshot rules; merely making writers thread-local does not
make live concurrent reads safe. Record guest call site/target and enough
context to distinguish a call from a tail jump; identify any dropped events.
Add selectable function/ABI tracing where needed rather than logging every
instruction. Trace thread create/start/exit and critical-section/event/semaphore
operations, including object identity, owner/waiter, signal/reset, timeout and
return status. Reuse the existing lock diagnostics after auditing their shared
state. Avoid introducing a global diagnostic lock into critical guest paths.

Acceptance: concurrent known call sequences stay attributed to their own
threads without corrupting buffers; a deliberate lock-order deadlock names
both owners/waiters and acquisition sites; a missed signal can be distinguished
from a signaled object whose waiter never resumes.

### 01d — Repeatable concurrency regression probes

Add small fixtures for worker register/TLS/stack isolation, create/exit/join,
event wakeups, critical-section exclusion, worker crash, deadlock and CPU spin.
Run bounded repetitions and archive failure artifacts automatically. On
suspected races, add narrowly targeted scheduling delays/yields with recorded
seeds/settings and rerun with lighter instrumentation to account for timing
changes. Repetition improves coverage; it does not prove absence of races or
give deterministic replay of the Windows scheduler.

The toolkit's `RECOMP_WORKERS=inline` can help narrow a hypothesis, but changes
execution semantics and can itself deadlock when a worker waits for its caller.
A passing inline run is neither a fix nor proof of a race. A static crash dump
also cannot reveal every earlier racing write; use targeted event/memory-write
tracing once the suspect object and code paths are known.

Acceptance: each deliberately broken fixture produces its expected failure
classification and actionable artifacts, and healthy fixtures pass repeated
runs with no state leakage. Run the existing memmove test and game checkpoint
smoke run after harness changes to ensure diagnostics preserve boot behavior.

## Historical harness implementation checkpoint (2026-09-12)

- Added optimized PDB/map builds and an external debugger/collector. Each run
  archives binaries/symbols, metadata/hashes/settings, patches, source snapshots,
  game log, native/guest thread report and a dump when capture is requested.
- Fixed shared stdout/stderr file positioning and null-log checkpoint handling.
  Checkpoint and ICALL diagnostics carry thread/time identity. All-thread event
  histories are bounded per-thread records inspected only at debugger stops.
- Canonical guest RAM, registry and live guest registers are verified present
  in dump memory streams; exited threads retain history without TLS dereferences.
- Legacy ICALL rings and kernel dispatch selection are now thread-local.
  A forced-interleaving regression proved the old shared kernel slot called the
  wrong ordinal; `dispatch-before` failed and `dispatch-after` passed.
- Eight collector probes passed: three healthy concurrency repetitions with
  8,000 protected increments each, event signal/reset/timeout through actual
  imported kernel bridges, forced dispatch interleaving, handled exception,
  worker crash, two-thread deadlock and CPU spin. Summary:
  `logs/harness-test-results.json`. Existing memmove CTest also passes.
- Ordinary game verification still reaches the prior guest-entry/memmove path,
  with first missing target `0x0018AFB0` and a timed dump. Latest:
  `logs/runs/20260912-214337-093-harness-ready/`.
- The named native stack locates the current hang in `sub_001497DC`, reached
  through guest `nh_malloc` and game initialization. Cause is not yet proven.

Run `python scripts/test-harness.py` for the collector suite. The run script now
uses the external collector, disables the in-process watchdog for collected
runs, and emits `result.json`. Script outcomes: 0 normal zero exit with expected
checkpoints; 1 nonzero game exit; 2 collector failure; 3 diagnostic capture or
unhandled exception; 4 missing expected checkpoint. Inspect JSON to distinguish
crash from deadline. Shell wrappers may report nonzero codes differently.

Remaining harness coverage: the fixture workers currently use native thread
creation plus runtime stacks/TIBs, rather than the complete guest
PsCreateSystemThreadEx bootstrap/termination path. Semaphore and multiple-wait
edge-case matrices, scheduled perturbation sweeps, and diagnostics outside the
instrumented synchronization boundaries remain follow-up work. Registry capacity
is 128 thread lifetimes per run; overflow is explicit and never reuses stale TLS.
Old optional tracing/profiling paths have not all been audited for concurrency.
Source ZIPs describe files at launch, not proof of a clean build of every source.

The proven crash/hang/history capabilities are sufficient to resume sequential
startup recovery now. Keep 01c/01d's remaining coverage open and return to it
before declaring guest worker scheduling or general concurrency correct.

## Milestones and acceptance checks

Statuses apply only to observed, verified behavior. Each row should produce a
small reviewable change and a saved log, test result, or scene capture.

Suggested Agent is a routing recommendation, not a record of who performed the
work. Labels follow the escalation gates in `AGENTS.md`: Luna is the default
for bounded implementation with a clear contract, Sol Medium is for concrete
ambiguity or a repeated failed Luna attempt, Astra Medium is for architecture
or task-graph revision, and Terra review is an independent correctness review.
Sol Light orchestrates a parent milestone when several Luna packets must be
sequenced; it does not replace the suggested implementation agent.

| ID | Status | Milestone | Acceptance check | Suggested Agent |
|---|---|---|---|---|
| 00 | Done | Reproduce launch and first failure | Correct working directory, fresh archived baseline, no lingering process. | Luna + Terra review |
| 01 | Done | Repair the first memmove failure | Copy/ABI tests pass, first failure gone, later startup path retained. | Luna + Terra review |
| 01a | Done | Preserve runs and native symbols | Matching artifacts, coherent thread-tagged logs, meaningful outcomes and checkpoint checks. | Luna + Terra review |
| 01b | Done | Capture crashes/hangs across all threads | Controlled worker crash, deadlock and spin produce inspectable dumps before termination. | Luna + Terra review |
| 01c | Core verified; coverage open | Make tracing safe and useful for concurrency | Per-thread call history and verified lock/wait/signal ownership evidence. | Luna + Terra review |
| 01d | Core verified; coverage open | Validate concurrency diagnostics | Positive/negative multithreaded fixtures produce expected outcomes across bounded repetitions. | Luna + Terra review |
| 01e | Done for current backing | Preserve actual GPU memory in captures | Capture separate contiguous/instance memory and readable GPU aperture; verify marker bytes in dumps across nine harness probes. Future trapped model state remains separate work. | Luna + Terra review |
| 01f | Done for current backing and native fixture | GPU snapshots, offline decoder and failure probes | Thirteen harness probes, twenty offline tests, five CTests, NVIDIA/WARP readback and verified RenderDoc capture. Snapshot storage does not prove GPU execution. | Luna + Terra review |
| 01g | Partial; optional tool follow-up | Nsight Systems trace completeness | Report and annotations captured; resolve the DX11 startup diagnostic and verify complete intended events before timing claims. Does not block 11a. | Luna + Terra review |
| 02 | Done | Recover startup target `0x0018AFB0` | Inspect its caller and original boundaries, lift real initialization, verify initialized globals and return stack. | Luna + Terra review |
| 03 | Done | Recover the adjacent startup routines | Investigate the `0x0018B1A0`–`0x0018C3C0` group one function at a time; compare initialized tables with XBE intent. | Luna + Terra review |
| 04 | Done | Recover remaining observed library targets | Classify and handle `0x001BDAA9`, `0x0018E410`, `0x001A5299`, `0x001A52A4`; verify individual contracts. | Luna + Terra review |
| 05 | Done | Locate the boot hang | Capture native call stack plus guest registers/stack; identify exact polling instruction or recursive path. | Luna + Terra review |
| 06 | 06a done; 06b blocked on reachability | Audit imported kernel gaps | Complete 06a and 06b; name ordinals 204/144/91/8, measure actual calls, implement required semantics and stack cleanup. | Luna + Terra review |
| 06a | Done | Establish imported kernel contracts | Contract table in `docs/jsrf-kernel-import-contracts.md`: the four unbridged ordinals (204 NtProtectVirtualMemory, 144 KeSetDisableBoostThread, 91 IoDismountVolumeByName, 8 DbgPrint) are declared but never called — 0 calls each — and all 23 actually-called ordinals are bridged. Evidence: `logs/runs/20260922-001023-466-bind-class/`. | Luna |
| 06b | Blocked on reachability | Implement reached imported kernel semantics | No reached import lacks an implementation. Re-open when a measured call to an unbridged ordinal appears; the criterion is a call, not presence in the import table. | Luna + Terra review |
| 07 | In progress | Establish reliable CRT/game initialization | Complete 07a and 07b; trace constructor completion and first game entry, then resolve reachable uninitialized-EBP warnings and ABI mismatches. | Luna + Terra review |
| 07a | Pending; 07b worked ahead of it | Prove constructor and CRT completion | Capture constructor order, return registers/stack and first game-entry checkpoint across a bounded run. | Luna |
| 07b | In progress | Repair evidenced initialization ABI defects | Fix only reproduced EBP/register/stack contract defects and verify constructor outputs plus the next failure address. **Measured (2026-09-22):** two contract classes fixed with the run as the oracle — the `0x00168FF0` tail-thunk `stack_args` class (11 entries) and `0x00178F40`, whose `end` cut the last byte of its three-byte `ret 0x14` so the body had no epilogue at all (`ABI FAILURE ... expected +24`, observed delta 0). Correcting `end` to `0x001791BB` makes the run pass the abort and reach a 31.7 s deadline instead of dying at 2.1 s, with native threads 11 -> 12. Detector: `scripts/check-entry-extents.py`. **Still open, measured:** 40 of the 3068 recovered bodies contain no `return` statement because a head entry's span stops at an internal label (`0x00014870-0x00014880` ends at a `cmp` while `0x00014881` is the fragment entry and `0x00014880 push edi` belongs to neither); each is a latent abort of the `0x00178F40` kind. Evidence: `logs/runs/20260922-101516-467-p1-178f40/` and
`logs/runs/20260922-101710-964-p1b-178f40-end/`; the planning-relevant findings are
fully stated in this row. | Luna + Terra review |
| 08 | Pending | Validate paths and first real asset read | Complete 08a and 08b; resolve a requested file under `game/Media`, verify length/content and missing-file behavior, and inventory asset formats. | Luna + Terra review |
| 08a | Pending | Establish path and file error contracts | Exercise relative/rooted paths, existence, length and missing-file returns with guest/host pointer checks and saved evidence. | Luna |
| 08b | Pending | Read and classify one real asset | Read one requested `game/Media` asset byte-for-byte, verify its length/content against the retail file, and record the format needed by later rendering. | Luna + Terra review |
| 09 | Pending | Validate allocation and object ownership | Complete 09a and 09b; check guest pointers, alignment, virtual reserve/commit behavior, frees and repeated-load memory growth. | Luna + Terra review |
| 09a | Pending | Verify guest allocation invariants | Probe reserve/commit, alignment, pointer translation and boundary failures with canaries and captured guest addresses. | Luna |
| 09b | Pending | Verify ownership across repeated loads | Exercise one load/free cycle repeatedly, detect double-free/leak/aliasing, and archive bounded memory-growth evidence. | Luna + Terra review |
| 10 | Pending | Validate timers, threads and synchronization | Complete 10a and 10b; startup workers complete, waits unblock for a real reason, timing is monotonic and reproducible. | Luna + Terra review |
| 10a | Pending | Verify timer and wait contracts | Test monotonic time, timeout boundaries, signal/reset and return statuses with bounded repeated probes. | Luna |
| 10b | Pending | Verify startup worker lifecycle | Prove worker creation, wake, join/exit and synchronization isolation on the real startup path, with a captured next checkpoint. | Luna + Terra review |
| 11 | **Blocker cleared**; renderer still pending | Select and wire graphics interception | Trace JSRF's D3D initialization and complete 11a–11d; decide which XDK entry points use host D3D8/D3D11 versus NV2A handling. **Measured scope (2026-09-22):** the title binds four classes — `NV_MEMORY_TO_MEMORY_FORMAT` 0x39 (subch1), `NV_CONTEXT_SURFACES_2D` 0x62 (subch3), `NV_IMAGE_BLIT` 0x9F (subch2), `NV_KELVIN_PRIMITIVE` 0x97 (subch0) — and the walk rejects its fifth packet, `subch1 method 0x180 (NV097_SET_CONTEXT_DMA_NOTIFIES) param 7`. That packet registers a DMA notify buffer the title then waits on, so 11 requires: name the four classes, implement `NV_MEMORY_TO_MEMORY_FORMAT` far enough to accept its methods, and implement the notify writeback. `jsrf_nv2a_registers` must be extended rather than weakened. Evidence: `scripts/decode-pushbuffer.py` on `logs/runs/20260922-001023-466-bind-class/`. **Update (2026-09-22, later):** the notify packet is accepted and the walk commits — the ring-space wait at `0x00191440` returns and both submissions report `diag=ok` once the fence mirror publishes the device's counter rather than a push buffer position. Evidence: `logs/runs/20260922-054824-982-p2-fence-counter/`. What stops the run now is not graphics: it is DirectSound init, see the audio note below. **Update (2026-09-22, third):** the audio stop is root-caused and the missing mechanism is named. DirectSound hands the GP DSP a program in a 48K contiguous allocation (guest `0x803C0000`, base published at `0x1BA858`), sets the pending word at `buffer+0x810` to 3, and spins at `0x001A18D0`; only three sites in the whole recompiled title touch `+0x810`, and none of them can clear it on the commit path, so the acknowledgement must come from the APU. Measured APU traffic (`logs/runs/20260922-104131-686-apu-trace`, 344 decoded accesses): the title programs `VPSGEADDR`/`VPSLADDR`, `GPFADDR`, `GPSADDR = 0x803CC000` (its own global at `0x1BA868`), `EPSADDR`, `EPFADDR`, and as its last APU access before the spin `GPSMAXSGE = 206`, after a 12-page `MmGetPhysicalAddress` walk fills entries 194..205 of that 206-entry SGE table. The APU is now instantiated and routed (`RECOMP_APU_TRAP`), so the remaining work is a **GP SGE engine in the APU model**, not a graphics packet. **Update (2026-09-22, fourth): the icall frontier is cleared and the ring drains; the run now blocks on a dispatcher object.** Four span repairs were confirmed by execution rather than by reading the disassembly -- the log shows `[RECOVERED] 0x0007BE30 returned; ABI verified`, `0x0007BDD0`, `0x00024700` and the new `0x001185B0` all returning -- and the run contains **no `[ICALL] Failed to resolve VA` anywhere in 6063 lines**. `[PFIFO] submit #1 diag=ok get=00002764 put=00002764` means the model now walks and commits the **whole** second pushbuffer, GET reaching PUT at 0x2764, past the 0x1B24 this row recorded earlier. Reaching it required decoding the ring the title submits *now*, which exposed two defects in `scripts/gen-nv2a-method-inventory.py`: the ring ends were hardcoded to the first pushbuffer ever submitted, and the packet classifier used `h >> 30` where `nv2a_submit_pending` uses `(h & 3) == 1` for a call and `(h & 3) == 2` for a return -- the two agree over the first 0x1B24 words and diverge after, so the generated table was not the list the model walks. The generator is now a word-for-word port of the walk, and the regenerated table (`NV097_CLASS` 250 -> 362 methods, toolkit `18a0837`) carries the whole programmable-vertex path: `0x1B00`/`0x1B04`/`0x1B08`, `0x1BC8` and `0x1BCC`. `jsrf_nv2a_registers` gained an acceptance case for `0x1BC8`/`0x1BCC` **and** a rejection case for `0x1BD0`, which the title never submits, so the table can only grow by measurement. **What stops the run now is neither graphics nor audio:** the guest blocks in `KeWaitForSingleObject` (ordinal 159, `kernel_thunks.c:199`) called from `0x0018CE6D` inside `sub_0018CE50`, waiting on the object at `[[0x19DCE0]+0x2430]` after clearing the flag at `+0x2434`; `sub_0018CE50` is called from `0x0013B1D0` and `0x0013B240`, and `+0x2430`/`+0x2434` occur at exactly one site in the whole recompiled title. Outcome `diagnostic_deadline`, 41.8 s. Evidence: `logs/runs/20260922-110738-253-nv2a-1bcc/`. | Sol Light orchestration; Luna + Terra review |
| 12 | Pending | Create a window and present a clear | Host device and responsive window initialize, and an actual guest command repeatedly changes the presented framebuffer. | Luna + Terra review |
| 13 | Pending | Draw one game-owned UI primitive | Correct vertex/index format, viewport, blend and texture sampling; compare with reference. | Luna + Terra review |
| 14 | Pending | Load one actual menu texture | Decode the format JSRF requests, including swizzle/mips as needed; display with correct alpha/color. | Luna + Terra review |
| 15 | Pending | Reach the title screen | Render an identifiable stable screen; identify intro/FMV sequencing and any temporary skip explicitly. | Luna + Terra review |
| 16 | Pending | Connect controller input | Map movement and menu buttons; verify held versus pressed states and controller disconnect/reconnect. | Luna + Terra review |
| 17 | Pending | Navigate the main menu | Start/back/options work without missing dispatch or stack drift; labels and selection are visible. | Luna + Terra review |
| 18 | Pending | Start a new game and load the opening area | Actual game loader finishes, objects and collision data exist, transition completes. | Luna + Terra review |
| 19 | Pending | Render the opening scene and character | Geometry, transforms, camera, skinning and depth behave correctly for this scene. | Luna + Terra review |
| 20 | Pending | Match JSRF's distinctive rendering | Implement observed cel shading/outlines, materials, transparency and effects; compare the same reference scene. | Luna + Terra review |
| 21 | Pending | Make skating and camera playable | Movement, turning, jumping, collision and camera respond at a stable simulation rate. | Luna + Terra review |
| 22 | Pending | Complete one graffiti interaction | Prompt/input, animation, paint consumption and progression state agree. | Luna + Terra review |
| 23 | Pending | Add audible game feedback | Implement the actual sound path; movement/UI effects play once, at correct pitch and volume. | Luna + Terra review |
| 24 | Pending | Add music and streaming audio | Music starts, loops/transitions and streams without starvation or runaway memory. | Luna + Terra review |
| 25 | Pending | Save and resume the playable slice | New save in isolated host location, exit, restart and restore expected location/progression; handle missing save. | Luna + Terra review |
| 26 | Pending | Stabilize the minimum playable slice | Repeat new-game/play/save/load; sustain at least 15 minutes without crashes, stack drift or unbounded memory growth. | Luna + Terra review |
| 27 | Pending | Cross one area transition | Assets unload/reload correctly, state persists, return transition works. | Luna + Terra review |
| 28 | Pending | Support an additional character and challenge | Character-specific animation/movement and challenge completion/progression function. | Luna + Terra review |
| 29 | Pending | Validate cutscenes and FMV | Decode/play required sequences with synchronized sound and working skip/return behavior. | Luna + Terra review |
| 30 | Pending | Expand area coverage | Maintain an area checklist; load, traverse, interact, leave and return to every area individually. | Luna + Terra review |
| 31 | Pending | Expand mission and encounter coverage | Checklist each mission, enemy/encounter type, failure/retry and progression unlock. | Luna + Terra review |
| 32 | Pending | Complete a full campaign playthrough | New save through ending without debug skips; save checkpoints and capture remaining defects. | Luna + Terra review |
| 33 | Pending | Cover optional/unlockable content | Verify characters, collectibles, challenges and options against a content checklist. | Luna + Terra review |
| 34 | Pending | Harden Windows behavior | Focus loss, resize/fullscreen, controller reconnect, audio device changes and clean shutdown work. | Luna + Terra review |
| 35 | Pending | Profile and resolve bottlenecks | Measure representative scenes, fix demonstrated CPU/GPU/I/O stalls, preserve simulation speed. | Luna + Terra review |
| 36 | Pending | Package a reproducible port | Clean build and launch on another Windows setup with user-supplied assets, documented controls/configuration and known issues. | Luna + Terra review |

Graphics, input and audio may be reordered when the real boot path requires
them sooner. Do not call the minimum slice finished merely because a window
opens, and do not call the whole game finished after one playable scene.

### Audio is on the critical path now, not at milestone 23

Measured 2026-09-22. The run does not reach rendering without DirectSound,
because the title treats a failed audio initialisation as a reason to restart
itself: it polls the AC'97 codec-ready bit at `0xFEC00130` 1000 times, times out,
and calls `HalReturnToFirmware(2)` with a launch data page naming its own title id.
Setting `RECOMP_AC97_READY=1` answers that poll and the run gets 200 kernel calls
further, then faults at `0x001A2BFC div esi` with `esi = 0`.

That zero is fully traced: the game passes a `WAVEFORMATEX` with
`wFormatTag = 0`; `0x001A4BC4` has branches only for 1, `0x69` and `0x10001`,
writes nothing and returns 0 for anything else; `0x0019F320` reads the 0 return as
"nothing to do"; the device format stays zeroed; `0x001A29EB` computes
`ceil(nChannels/2) = 0`. So milestones 23 and 24 are not "add sound later" — the
audio device path has to answer correctly before the title will proceed, and the
next packet is the `DSBUFFERDESC` and `lpwfxFormat` the game passes at
`0x001A09DE`. Evidence: `logs/runs/20260922-055208-364-p5-ac97-only/`,
`logs/runs/20260922-055304-689-p6-ac97-ecx/`. The causal chain needed for
planning is stated above; any session narrative belongs in operating history.

## Historical initializer checkpoint (superseded below)

Artifact: `logs/runs/20260912-215131-391-all-initializers/`.
All 14 observed missing initializer entries now run real translated instructions.
`python scripts/verify-initializers.py` verified 616 initialized words against
an independent interpreter of original XBE instructions; wrappers checked ESP
and nonvolatile preservation at every return. Recovery boundaries/evidence are
in `config/recovered-functions.json`; regenerate only them with
`scripts/recover-functions.py`. The resulting merged function database is
`tools/disasm/output/functions.recovered.json`, and the manual manifest excludes
these external definitions during any complete regeneration.

Use `scripts/build-jsrf.py` before running. It rejects compiler failure and
source edits during a build; the runner verifies source/executable identity.
A failed older build/run artifact `20260912-214847-905-initializer-group` is not
evidence of a working initializer group. Subsequent verified builds supersede it.

The then-open milestone 05 captured loop `0x00149B28` in heap allocation `sub_001497DC`.
The free-list head `0x00F81180` points to `0x0105D968`, whose links point to
itself while its chunk is marked allocated. The scan seeks size `0x368`,
but the linked chunk is size 2. The investigation was to find the first incorrect list mutation and
compare its original instructions, operands, and callee ABI before fixing it.
Do not replace the heap or bypass the scan based on this snapshot alone.

## Translation and startup checkpoint (later on 2026-09-12)

Heap hang cause: LOCK XADD's result flags were lost, so COM Release methods
always took their destructor branches and eventually freed the same object
again. Fixed locked atomic flag tracking and snapshotting of the operation's
result (rather than re-reading shared memory). Relifted 82 affected functions.
Compiled regression: 288 cases pass, including intervening writes; original
translator fails (`logs/lifter-before/`). Milestone 05 is complete.

Repeated word/dword comparisons likewise used stale flags after CMPS, making
unequal GUIDs compare equal. Corrected tracking and zero-count ZF preservation;
relifted 140 functions. Additional 300 byte/word/dword comparison cases pass,
including mismatches, reverse direction and zero count. Both CTests pass.

All 541 generated direct stubs now report failure, and this project stops by
default at unresolved/invalid calls (exception E0424943). Explicit historical
fallback requires JSRF_ALLOW_UNRESOLVED=1; do not use it as acceptance evidence.
The first strict-stub stop was an omitted internal branch in 0x00154D70, now
restored. That investigation continued through D3D mode enumeration and missing tails;
see `config/boundary-fixes.json`; current execution artifacts must be named by the
active packet/review records rather than a report line reference.

Kernel gap names are now identified from the toolkit's ordinal contracts:
204 NtProtectVirtualMemory (16 stack bytes), 144 KeSetDisableBoostThread (8),
91 IoDismountVolumeByName (4), 8 DbgPrint (cdecl, 0 callee cleanup).
None has yet produced the runtime 'no bridge' warning in the verified path.
Keep their implementations pending until required semantics are established.

## GPU boundary checkpoint and decomposed work

Video queries now pass an actual-guest-code probe: 24 mode entries, 212-byte
caps copy with boundary canaries, expected error returns and tested ESP/shared
nonvolatile registers. The game requests 640x480, format 0x11, two back buffers.
Six reviewed boundary fixes preserve real internal branches; 20 recovered
functions include 14 startup initializers and six D3D routines. Device setup
at 0x00192090 runs as far as its hardware helper 0x00194ADD, after allocating
512 KiB for the pushbuffer. There is still no game-owned rendered image.

| ID | Status | Next bounded outcome | Acceptance | Suggested Agent |
|---|---|---|---|---|
| 11a | Complete as corrected documentation audit | GPU setup and port 0x80C0 contract | Every setup address and named register decoded; PCI 0x0C/0x30 meanings and masks independently rechecked; single state owner proposed. Open hypotheses (GPIO effect, vendor 0x4C bytes, PTIMER TIME-write encoding/effect, 0xFE502A, 7th KeInitializeInterrupt arg) are explicit 11b/11d inputs. See docs/jsrf-gpu-setup-contract.md. | Luna + Terra review |
| 11b | In progress (through 11b4b2 complete) | Single coherent NV2A register/command model | Complete 11b1–11b5; real progress drives acknowledgements and a blocked command remains blocked and diagnosable. | Sol Light orchestration; Luna + Terra review |
| 11b1 | Complete | Install one NV2A MMIO state owner | Permanent serialized MMIO trapping, quiesced legacy mutations, actual concurrent VEH probes, generation-validated frozen snapshots, explicit unreadable behavior and clean teardown verified. | Luna → Sol Medium; Terra review |
| 11b2 | Complete | Implement NV2A PCI configuration paths | Immutable identity/class, standard setup fields, exact PBUS range and deliberately shared vendor backing round-trip through the real HAL bridge; other buses/invalid accesses retain zero/ignored behavior. | Luna → Sol Medium; Terra review |
| 11b3 | Complete | Complete PTIMER register contracts | Pinned xemu-compatible split TIME writes, divisors, alarm scheduling, late enable, W1C/PMC aggregation and clean asynchronous service lifecycle are covered by deterministic core tests and the real trapped-MMIO runtime probe. | Luna → Sol Medium; Terra review |
| 11b4 | In progress (through 11b4b2 complete) | Add bounded PFIFO/USER submission | Complete 11b4a–11b4b; execute only observed packet/control and reviewed method forms, preserve guest pushbuffer addressing, and stop explicitly on unsupported work without advancing GET or completion. | Luna + Terra review |
| 11b4a | Complete | Validate packet intake and rollback | Physical-offset USER submission through the real trapped aperture reads only the contiguous window; bounded packet/control decoding atomically commits GET and a method-record stream, while unsupported work preserves all accepted state and reports an exact diagnostic. | Luna → Sol Medium; Terra review |
| 11b4b | In progress (11b4b1–11b4b2 complete) | Execute a reviewed method subset | Complete 11b4b1–11b4b3; apply only method effects whose class binding and host contract are proven; an unsupported method blocks before GET/completion and records its subchannel, method and parameter. | Luna + Terra review |
| 11b4b1 | Complete | Establish atomic unsupported-method boundary | With no class binding model, only NOP 0x0100 on subchannel 0 is accepted; every other method records exact fields and rolls back the entire stream without renderer or completion effects. | Luna + Terra review |
| 11b4b2 | Complete for fixture contract | Prove class binding and first state effects | Fixture-gated handle lookup plus transactional SET_OBJECT proves per-subchannel NV097 binding; only packed clip H/V are accepted, while format/pitch/offset methods roll back as unsupported. | Luna → Sol Medium; Terra review |
| 11b4b3 | Complete | Connect real RAMIN object lookup | Production SET_OBJECT walks claimed PRAMIN RAMHT with the original 0x001945D6 11-bit hash, valid bit, handle match, and class from RAMIN word0 low byte (including `0xB03D` → `0x3D`). Fixture binding stays opt-in for 11b4b2 tests. Focused NV2A PASS 304. Independent review accepted. Solo `logs/runs/20260921-103827-656-ramht-lookup/` still stops at `0x001918E0`. | Next: kick/GET contract |
| 11b4b4 | Complete | Write the kick/GET contract and own the kick latch | `docs/jsrf-kick-get-contract.md` maps the ring control block, `0x00191530` (RET 8, fast/slow exits), `0x001916B0`, `0x001916C0`, `0x00191390`, `0x00191440`, `0x001918E0`, `0x001917F0`, `0x00190240` and its two real callers. The kick is a write to `NV_PFIFO_CACHE1_DMA_PUT` bit 16 plus a poll until the engine clears it, so bit 16 is now a model-owned latch (toolkit `488286f`); the offset mask is `0x1FFEFFFF` because the generic `0x1FFFFFFF` contains bit 16. Kick requests/acks are counted and a blocked stream still acknowledges. 13 new contract cases, NV2A 319. Ordinary boot unchanged at `0x001918E0`, GET=PUT=`0x1000`. | Next: recover the kick chain |
| 11b4b5 | Complete | Recover the pushbuffer kick chain | All 16 chain entries added to `config/recovered-functions.json` (70 total) with `stack_args` read off real epilogues: `RET 8` on `0x00191530`/`0x00191440`/`0x001916C0`, `RET 4` on `0x00191390`/`0x00191730`/`0x001917B0`/`0x00190240`, `RET 0xC` on `0x00190FB0`, plain `ret` elsewhere. `0x001910C0` and `0x001910E0` are separate functions (fall-through); `0x001918E0` tail-jumps to `0x001917F0` and shares its frame. Confirmed the method blocks do not call `0x00191270`; the kick is reached from `0x00191440`. Three latent generator gaps fixed; the planning-relevant causes are recorded in the A4/11c historical notes in this plan. **The link blocker is cleared**; `jsrf_recomp.exe` now builds (19,245,568 bytes, identity-verified). | Clear the run blocker, then run the chain and expect the accepted-submission contract |
| 11b5 | Pending | Reconcile completion and legacy acknowledgements | Retire conflicting synthetic acknowledgements for modeled registers; implemented work advances completion, while the forced-stall probe remains blocked and yields a useful dump. | Luna + Terra review |
| 11c | Partial | Complete recovered GPU initialization | Complete 11c1 and 11c2; `0x00192090` returns with correct outputs/ABI, including reviewed helpers and port handling; no success stubs. | Sol Light orchestration; Luna + Terra review |
| 11c1 | In progress (chain recovered, the game links, **real-guest-run ICALL open** — see below) | Establish remaining GPU-helper contracts | Hardware setup through framebuffer publish is recovered. Focused 11c1 2197 checks. Ordinary stop `logs/runs/20260921-020149-996-gpu-setup-912a0/` is `0x001918E0`. WBINVD ran. Keep `0x00193F70` and `0x0018E120` fatal. The kick chain `0x001918E0`/`0x001917F0`/`0x00191530`/`0x00191440`/`0x00190240` is recovered per 11b4b5, and `jsrf_recomp.exe` now links with verified build identity. Both known correctness defects are **closed**: `tail_jump_alias` entries are declared and folded into their parent rather than emitted as second bodies, and silent deletion of live jumps went **1309 -> 309 -> 115 -> 40**, with same-function deletions 1032 -> 0. Six toolkit revisions: `58a9cf9` (alias fold), `a301962` (a real tail call is not an intra-body goto — 864 tail calls were being rewritten to `(void)0`), `5d68f27` (create an alias for a branch target mid-body in another function), `ff4d442` (**Cause A fixed**: fold an alias into the body that starts where it ends — deleted jumps **309 -> 115**, duplicate alias bodies **958 -> 0**, header declarations 6,072 -> 5,714), `db54746` (**Cause B fixed**: do not accept a bare `mov edi,edi` as a prologue — `8b ff` is the MSVC hot-patch pad *and* the 2 bytes before a switch table, so the pass was claiming table data as code; the rule now needs a run of >=4 dwords that are each a decoded instruction start at `+2`; `gap_prologue` 363 -> 303, deleted jumps **115 -> 40**, declarations 5,714 -> 5,652), and `9568f29` (two recomp tests that read another module's output dir). **Cause B was mostly not Cause B**: only a minority of the 115 were genuine cross-body branch targets; 71 were phantoms. **The `db54746` fix repairs 91 truncated functions and adds 0 entries.** Verification is **field-for-field identical on the fixture probe** (`--probe gpu-progress --expect-checkpoint probe_gpu`: `normal_exit`, exit 0, 4 snapshots, 36 named frames) — an earlier "guest run regressed" claim was a fixture probe compared against a real guest run and is retracted. Measured correctly, against real guest runs: pre-fix `c98dbdc6` and post-fix `9568f29` are **both** `unhandled_exception` `0xE0464643`, both reach `guest_entry` (log line 48), both issue 157 kernel calls; the difference is that the pre-fix run logged **48** `[RECOVERED] ABI verified` checks and died at `Failed to resolve VA 0x001918E0`, while the post-fix run logs **0** and dies at `[ICALL] invalid target 0x00700010 ... return=0017E627`. The failure moved **earlier**, into startup: `sub_0017DBBD` builds `ecx = MEM32(edi + 8) + index*8` and pushes `MEM32(ecx + 4)` as a callback into `sub_0017E600`, which dispatches via `call eax` at `0x17E625`; `edi` is `[ebp + 0x10]`, a runtime-built table descriptor, and `0x00700010` is in no image section. **`sub_0017DBBD` is the MSVC C++ exception unwinder, not game code** — `0x0017DBA4` is `cmp dword ptr [eax], 0xe06d7363`, the MSVC `_EH_EXCEPTION_NUMBER` magic; `sub_0017D1F8` is `__SEH_prolog`, `sub_0017E3F8` reads `fs:[0x24]`/`fs:[0x28]` (`_getptd`), `sub_0017DC6E` maintains `[eax+0x7c]` (`_catchlevel`), and `sub_0017E600` dispatches the catch handler via `call eax` at `0x17E625`. Crash-dump memory at `0x001EB744` holds `0x19930520` (`EH_MAGIC_NUMBER1`) and `0x001EB760` a valid table pointer, so the EH tables are laid out correctly and `0x00700010` is a **handler-table slot holding a small integer where a code address belongs**. A `JSRF_ALLOW_UNRESOLVED=1` diagnostic run (permitted as a diagnostic, not as acceptance) shows continuing past it gives `esp=0x6A0CC48F` — not a stack address — and a wild read at `0x10000FFFC`, so `0x00700010` is a **hard stop, not a missing callee**; implementing "one more address" cannot fix it. Also note `g_xbox_code_lo/hi` covers **`.text` only** (`0x00011000`..`0x0018CB30`, built from XBE sections flagged `0x04` EXECUTABLE), so every other section is outside it — **do not widen the range to mask this**, `0x00700010` is in no section anyway. **Not shippable; next packet is the SEH chain** — what the demo throws immediately after `guest_entry`, auditing `fs:[0]` and `g_seh_ebp`, since a garbage `esp` is what a desynchronised SEH unwind produces. Verification otherwise green: build identity, all eight compiled suites, 19/19 harness probes, toolkit 256 passed, and the fixture probe field-identical (callers of `sub_0017DBBD`: `0x0017DCE6`, `0x0017DD6B`, `0x0017DDAE`, `0x0017E004`, `0x0017E2EE`). Two regeneration traps, both of which produced wrong numbers I briefly published: **`recover-functions.py` is not the translation pass** (the `recomp_NNNN.c` chunks come only from the manual invocation in AGENTS.md), and **a clean regeneration must clear `tools/disasm/output/*.json`, not just `.disasm_cache.json`**, because the disassembly JSON is an input to the translation. Real figures are 5,652 declarations / 40 gotos, reproduced from a cold start. **`--split 1000` chunk count can change** (6 -> 5 -> 6 across regens), so re-run `cmake -S . -B build` after regenerating or the build keeps a reference to a chunk that no longer exists | Reach the `probe_gpu` checkpoint on `gpu-submit-supported` and satisfy its accepted-submission contract: `diag=ok sink=1 successes=1 atomic=accepted` with `USER_DMA_GET == USER_DMA_PUT`. **`unsupported_method` belongs to `gpu-submit-blocked` only and would be a defect here** — an earlier version of this row said the opposite and was corrected. All three submission modes pass through the harness. A probe run expects **its probe checkpoint only**: `--probe=` returns before `checkpoint("guest_entry")`, so the default `['memory_ready','guest_entry']` does not apply and passing it reports a false failure. Treat a fixture submission as necessary but not sufficient: it does not prove the guest ran a real command stream |
| 11c2 | Pending | Recover and integrate reviewed GPU helpers | Add only approved helper entries/internal tails, run behavior and ABI checks, then archive the next real stop or prove `0x00192090` returned. | Luna + Terra review |
| 11d | Pending | Guest ISR/DPC execution | Complete 11d1 and 11d2; correct worker stack/register/IRQL isolation, notification and reentrancy tests finish relevant 01c/01d gaps. | Sol Light orchestration; Luna + Terra review; Sol Medium if concurrency evidence conflicts |
| 11d1 | Pending | Prove guest callback execution context | Verify ISR/DPC entry ABI, guest stack/register ownership, IRQL and per-thread isolation with direct callback probes. | Luna + Terra review |
| 11d2 | Pending | Deliver GPU interrupts through ISR/DPC | Exercise signal, masked, missed-signal and reentrant cases; callbacks run for modeled causes and no unrelated worker state is corrupted. | Luna + Terra review; Sol Medium if ambiguity remains |
| 12 | Pending | Window and first game-owned clear/present | Responsive Windows window and an actual guest command changes its framebuffer. | Luna + Terra review |

Keep the guest device/resource layout and connect the NV2A/D3D11 path behind
it. The existing toolkit acknowledgement worker and optional software executor
are not a complete GPU; their state ownership and completion semantics must be
reconciled before claiming rendering. Detailed task packets are in AGENTS.md.

## GPU register and capture checkpoint

`docs/jsrf-gpu-setup-contract.md` records original setup instructions, pinned
xemu implementation references, and local integration gaps. The NV2A reset
clock disagreed with its PLL coefficient: writing the same value changed the
clock from 466666648 to 233333324 Hz. Reset now reuses the register-write path.
The new compiled register-only fixture failed before and passes all 11 checks
afterward; logs/gpu-registers-before.txt and gpu-registers-after.txt preserve
evidence. All three CTests pass. No renderer is exercised by this fixture.

GPU audit exposed separately allocated physical memory missing from the old
dumps. The collector now adds the entire committed/readable 64 MiB contiguous
window and 16 MiB GPU backing, including reserved instance memory. All nine
harness probes pass with known contiguous-allocation marker bytes verified in
the dumps. The latest ordinary run retains all 616 initializer words, captures
the same missing 0x00194ADD, and permits reading pushbuffer address 0x80001000.
When registers become trapped, a frozen model-state snapshot must replace the
readable-aperture capture. Neither these tests nor existing acknowledgements
prove real GPU command completion.


## GPU harness completion and next-session handoff

Milestone 01f is verified in `logs/harness-test-results.json`; all thirteen
collector probes and five CTests pass. The frozen GPU report distinguishes
unreadable storage, synthetic progress, acknowledgement mode and pre-submission
state. The ordinary checkpoint above retains all 616 initializer words and stops
at the same missing helper. Native NVIDIA/WARP clear/copy/readback passes with
4,096 checked pixels and no D3D11 debug errors. RenderDoc capture is verified.

Nsight Systems is installed and produced a report with annotations, but its
exported diagnostic warns DX11 profiling may not have started correctly. Treat
01g as partial; the wrapper only validates report creation. Detailed commands and
limitations are in AGENTS.md; durable evidence is the named artifact set above.

The user requested a handoff focus due to remaining weekly usage. The corrected
11a contract audit is complete; the next bounded packet is 11b, not broad
renderer implementation.
Current GPU harness changes remain uncommitted in both repositories. Inspect and
preserve them before proceeding; earlier commit hashes represent the baseline.

---

## 2026-09-27 — UPDATE: the A2h OOM is HANDLED; the critical path moves to the NULL thunk slot

**Appended, not rewritten.** The A2h text earlier in this file framed an open question
("whether ... or ...") plus temporal claims, and asserted nothing false; this entry records the
answer. Nothing above has been edited.

**What was believed:** that the ~571 MB allocation failure was the terminal event, because the
guest "does not check the result and calls through NULL", raising `0xE0424943`.

**What measurement showed.** Verified bytes after the ordinal-184 call:

    00149E4A  call  dword ptr [0x1c3f88]   ; NtAllocateVirtualMemory
    00149E50  mov   dword ptr [ebp-0x12c], eax
    00149E56  test  eax, eax
    00149E58  jl    0x149eec               ; a SIGNED check on the result

`0xC0000017` is negative as signed 32-bit, so `jl` IS taken; the error path materialises
`STATUS_NO_MEMORY` and returns cleanly through `__SEH_epilog`. **The OOM is handled, not fatal.**
The earlier reasoning was circular: the bare sequence was cited as evidence for "no check", and
"no check" then explained the sequence. The originating packet had warned this inference "must
itself be tested, not assumed".

**The actual terminal event.** `call dword ptr [0x1c4064]` at `0x00149828` — near the TOP of the
same function, whereas the ordinal-184 call is near the END, so they are DIFFERENT PASSES. The
slot read `0`; `RECOMP_ICALL_IS_CODE` rejected it as non-code and the guard raised
`0xE0424943` NONCONTINUABLE. **The original XBE holds `0x80000115` at `0x001C4064` — a valid
ordinal-277 kernel thunk** — and the toolkit patches that table at runtime.

**Consequence.** The Advisor ruled that the **critical path has moved** to *what zeroed / what
read as zero at `0x001C4064`*, and that the producer line is **PARKED, not retired**, with two
named reactivation conditions. Ruling recorded verbatim in
`docs/jsrf-technical-record.md §5`; errata appended to
`a2h-oom-causal-slice-evidence.md` and `a2h-mechanism.md`.

**Also withdrawn:** two hand-counts (1177 and 1909) for the ordinal-277 dispatch multiplicity.
They were the same claim with different numbers and neither named its run and method, so neither
is citable. Binding lesson recorded: **no hand counts in decision inputs** — tool-computed
quantities with positive controls and loss accounting.

---

## 2026-09-28 — A frozen packet's stated uncertainty is a SCHEDULED VERIFICATION TASK, not a carried caveat

**Advisor practice note** (`docs/reviews/a2h-fix-packet-advisor-shape-preflight.md`, turn `01a0e7b8`):

> *"A frozen packet's stated uncertainty is a scheduled verification task, not a carried caveat — test it at
> scoping time, before writing criteria around it, with a named owner."*

**Planner adequacy decisions should map each uncertainty to its verification point.**

**Why this is recorded.** `docs/packets/a2h-slot-within-run-attribution.md:19` said, in the frozen contract
itself: *"A missing canonical install trap is `O-COVERAGE`, never no-write: **live `#DB` chance semantics and
all-thread arming have not yet been proven in the game.**"* **That is a named uncertainty with a testable
consequence — and it sat as a caveat through three packets.**

**It was tested only when the Session asked what the NEXT packet would target and noticed that the toolkit's
install store is a KNOWN canonical-slot write under an armed DR0 watch.** The result: **the DR0 write watch has
never been observed to fire, not once, including for a write that demonstrably happened.** The acceptance
reviewer strengthened it — **no `EXCEPTION_SINGLE_STEP` (`0x80000004`) was ever delivered to the debugger at
all, across ~14k debug events per run, in every run of the line.**

**The caveat was a scheduled test that nobody had scheduled.** Had it been mapped to a verification point when
the packet was written, the failure would have surfaced **before** three packets and ten runs were built on
the assumption that the channel worked.

**The two-phase consequence, now binding on the fix packet:** Phase 1 is the install-trap gate — does a `#DB`
hit arrive at a known canonical write under an armed watch? Phase 2 attribution runs **only if Phase 1
passes**. **DR records are inadmissible for any row until a live install trap passes.** If the leg cannot be
repaired in bounded effort, **it is dropped with an explicitly stated ceiling** rather than carried as
decoration.

**Related standing lessons this line has accumulated, kept together because they are the same shape:**
- **no hand counts in decision inputs** — tool-computed quantities with positive controls and loss accounting;
- **a decision input must be lossless by construction** (§6.1.6), and **absence of a witness is never positive
  attribution**;
- **a script outside the guard suite can rot silently across many commits** (`scripts/test-harness.py` was
  broken from `009f624` until a packet happened to require it);
- **"nothing observed on certified channels" is narrower than "nothing happened."**

---

## 2026-09-28 — A2h NULL line RETIRED at its ceiling; pivot to `0x001D5078`

**Advisor ruling** (`docs/reviews/a2h-null-line-closure.md`, turn `01a0e7f9`): *"drop CONFIRMED — no
re-referral for the contradiction (both branches terminate the leg); pivot to `0x001D5078` now DUE; NULL line
exhausted."*

**The line's honest final statement:** the slot read zero at **two independent terminal reads**; **zero alias
touches** across a **complete 28/28 page-protection census**; **the canonical-write channel is
UNCERTIFIABLE — which is not the same as "no write occurred," and that distinction is the whole finding.**

**Why the ceiling was right, in the Advisor's own words:** *"Explanatory power is not decidability — both
branches end at drop."* The install-thread contradiction was specific and interesting — a named thread whose
DR state disagreed with its own successful arm record — and the Session offered it as possibly worth another
look. The Advisor refused: a genuine clear **explains zero `#DB` but opens an unbounded "who cleared it"
question while leaving the leg uncertifiable**, and a read artifact **voids the snapshot for exactly the
thread that matters**. **Either way the leg ends uncertified, so another round would buy explanation, not
decidability.** The diminishing-returns diagnosis — *"five packets plus preflights with each fix revealing the
next caveat"* — is what the ceiling existed to stop.

**The read-path leg is DEAD, not deferred.** It needs **complete canonical coverage**, which is now
unachievable with any available instrument: **DR is dead, and the page-guard was already redirected away for
its single-step races.** So `A2h-terminal-read-path-audit` is not merely unreached — **it is unreachable on
this line.**

**What survives:** the 28-alias census (page-protection based, **no DR dependency**), mapping stability, the
two terminal software zero reads, the install software control, and the triage / `O-NO-BOUNDARY-TRANSITION`
rows (**they never touched DR**). **The producer line stays PARKED** — nothing here produced link evidence
either way. **Unreachable:** canonical-write attribution (guest/host/transient-as-write) and the DR leg of
read-path.

**The durable gains, recorded because they are real and outlive the negative result:**
1. **The `#DB` delivery premise is PROVED on this host** — a genuine debugger-delivered `#DB` claimed from
   `DR6.B0`. A long-standing open question is **closed permanently**, and it means the failure was
   **game-context-specific**, not a host or collector incapability.
2. **The gate defect is fixed and demonstrated** — `complete=1` where it was structurally 0, and a fixture
   yielding `decision=NON_FIRING` that was impossible before.
3. **The contradiction is LOCALISED to a named thread**, sharper than "zero hits, cause unknown."
4. **Six defects were found by measurement** across the line — the handshake-fatal VEH, the always-true
   `dr6 & ~1` test, the wrong terminal-witness gate, the `cleared=17` overcount, route-counter pollution
   (`ss_routed_generic_first=14405` against `raw_single_step=0`), and a toolkit gate coupling that would have
   made `NON_FIRING` unreachable again through a second env var.
5. **The practice note** above: a frozen packet's stated uncertainty is a **scheduled verification task**,
   not a carried caveat. **The frozen packet predicted this failure in writing and it sat as a caveat through
   three packets.**

## 2026-10-02 — Plan §13 history moved out of the active plan

Moved verbatim from `plan-jsrf-bare-minimum.md` §13 when that section was cut to its current state
and fast-path status (it had grown to about 52 KB of a 111 KB plan that every session reads at
startup). Nothing below was edited; it is the record of F0–F4b, the owner checkpoints and the
follow-up leads as the plan held it on 2026-10-02.

### Current state (owner, 2026-10-01; read this block first)

1. **Observer parked, no retry** — the fail-fast observer is STOP UNKNOWN by owner decision; no retry, correction, consultation, list extension or further fixture work.
2. **D2 accepted** — the directory-context cleanup closed as a bounded unit and stays accepted.
3. **SEGA gate located by read-only T3:** logo update `0x7E360` phase 2 waits on `0x24650()!=0`, i.e. subsystem-6 fade+0xC0. Normal hold is >120 update frames, then fade to alpha1 at step1/120. xemu visually reaches Smilebit; port logo phase2/counter121, fade alpha0/target1/done0 after3766 frames. Per-frame producer `0x24700` translation agrees with original/archive; missing armed-object update is likely, exact runtime cause unproven. Audio-leading hypothesis superseded; no title claim. [T3 evidence](docs/reviews/owner-sega-600-observations.md).
4. **Checkpoint 2 observed:** `20261002-014152-190-owner-d2-600`, 603.03 s deadline; 59 timed window captures identical SEGA, about 401 s after CMP marker; fresh root, no strict-horizon move. [Run record](docs/reviews/owner-sega-600-observations.md).
5. **No-APU classified from archive:** `20261002-015434-370-owner-d2-noapu-600` spins at `001A18D0` DSP pending-word initialization, before logo; APU_TRAP/L21 required on this build/path. L21–L24 removed, L25 remains; no framebuffer/CMP, visual UNKNOWN.
6. **Owner L02 recovery chore COMPLETE / STOP (2026-10-02; bounded explicit authorization):** game pulled `d385e37` first (dump executable globals + symbol reader). Applied **L02 `0x14FEF0..0x150104`, stack_args12**, generated10inspan switch targets+unsigneddefault, ESP+16checked, recovered lookup before foldedalias; L02count3076. Initial gates failed only stale provenance records; narrowly amended preserving full-generation history and documenting the unreachable neighboring14FE60 tail plus stale marker recount. Fresh ordered **just build0→test0(32/32)→check0**, post-prosecheck0, standalone behavior fixturePASS; fresh Reviewer independently accepts code, finalclosurepending. **Sole600s D2 exploratory run APU_TRAP1 `20261002-174731-263-owner-l02-d2-600` finished deadline3/602.768215s**, exeSHAe29986matchesgreenbuild,mappingPASS/rootverified. RecoveredABIreturnlog2644; requiredreaderfailedconst.rdata, equivalentcaptured.data+verifiedarchivedexemapcensus all134hits0/index11114FEF0retainedhit0. Logo2/hold121,fadealpha0/target1/done0;59identicalSEGAframes,noobservedexit. **No strict movement/fadefix**; ledgerL14–L18,L20–L25,L39,L40(L16inert,L19dormant); no extra portexperiment, observerretry/APUtrace/bypass. Xemu six verified **phase0** SEGA fadehits: manualreturnchain `1108A→11096→11096→124C3→13B24→13F9E→6FA41`; EBP1 notframepointer. Histories match target24700/site1108A(return, notcallinstruction); boundedabsence UNKNOWN, noECX/objectidentity. Concrete render-state defect restoration, **not proven fade-cause repair**. Priorcountercensus unavailable in olddump; newcapture cumulativezero independentlyverified despite symbolreader.rdata defect. [Evidence and limits](docs/reviews/owner-sega-600-observations.md#L244). Xemu exited; broadergoalpaused. FreshReviewer code+recordsACCEPT;results/ledgerIDsrecorded. **PUSHED_TO: origin / BRANCH: main / COMMIT:929856fcfc145036252510509abaa0782d910a22 / REMOTE_URL:https://github.com/danillogical/xboxrecomp.git / RESULT:Everything up-to-date**, toolkitfirst. **PUSHED_TO: origin / BRANCH: master / COMMIT:5c1d7d6664c5aab16c6e19425285cb0543428a3b / REMOTE_URL:https://github.com/danillogical/poison-jam.git / RESULT:fast-forward success**;clean committedcheckPASS,outgoing8blobs0secrets/max2.81MB,noassets/generatedpaths. Receipt-only closure, then **STOP**, broadergoalpaused.
7. **Owner kernel-name chore complete:** toolkit `929856f`; sole 600 s D2 run `20261002-132843-938-owner-d2-kernel-noop-600` ended at 604.83 s deadline, final SEGA. Two startup no-ops only (shutdown registration, boost control), none first appearing after CMP; no new gate identified. Archived per-thread rankings, times/limits and one Advisor consult in [review](docs/reviews/owner-sega-600-observations.md); no implementation/observer retry. Records commit/push toolkit first, then STOP; broader goal paused.

### Owner checkpoint 1 — records and gates (2026-10-02)

Toolkit fast-forwarded to `348a0e38fdf3fd940d6e5f79ad24cf25fbcf2c41`: only Mac runner tooling, no runtime change. Eight ruling-lint findings closed using the rulings' own content and one quick metadata Advisor consult (`4ec43a66-99c2-4d6b-b981-5a558f8e6d54`, Claude Opus 5.5 high). Missing technical reversal conditions and the historical fail-fast Advisor attribution remain explicitly absent rather than reconstructed. Observer stays parked; historical process debt is not waived.

The existing ruling checker now runs in `just check` and CTest, without a new fixture/build target. `just ruling-check`, `just check`, dedicated ruling CTest and full CTest **32/32** passed. CMake registration changed the source fingerprint: initial identity verification refused it, so `just build` refreshed identity through the guarded path (no regeneration); production EXE remains SHA-256 `74377ac6130e812b7a830492d4995bdc58efb8899ff5c1d6c23b7ee6c0c0424e`. Plan worker's stale run identity/conditional-step errors were found and corrected by Session before closure.

Run preparation decision: use the existing runner's **fresh empty disposable root**; it has no seed option and refuses nonempty roots. No runner modification or copy-race. The allowed copy was optional; prior D2 root and original assets remain untouched. The 600 seconds therefore includes cache filling; report the measured post-marker interval, not a claimed 600 seconds after marker. This adds no seed shortcut/ledger entry. Run profile and its ledger IDs remain the D2 profile until conditional APU removal.

Staffing correction: Session mistakenly dispatched content review on `codex/gpt-6.1-sol` medium (`49b63b11-7402-48ab-a628-b6852da7fadb`); it is not roster-compliant Reviewer acceptance. Fresh authoritative `claude/claude-opus-5-5` medium Reviewer (`3084f0a2-73d3-4dbf-b449-761555bacc4a`) accepted checkpoint 1 with no blocking defects; independently verified ruling lint, dedicated CTest registration/pass and unchanged executable identity. Parent-held spawn receipt pins route/effort; roster unchanged.

Push receipt is reported at this checkpoint and in the next durable run record; final receipt names the resulting commit rather than a self-referential hash.

### Title-screen fast path (owner direction, 2026-09-30) — supersedes the entries below

**Superseded as current status by the block above; kept as history.** The naming note below applies to
every F-number in this section and in the historical material that follows.

*Naming note: the F-numbers are the names these steps were recorded under. **F5 names two different
things in the history** — the intro movies (its current meaning, below) and, in the superseded
2026-10-01 material, the fade/logo/cache observation work that later took the names **F4b** (the
logo/cache observation) and **F5 fail-fast observer** (the parked attempt). Old text keeps its old F5
wording; read it through this note rather than rewriting it. **F5 is reserved for the intro movies.***

Chores, run by the Session in order; each step adds or updates ledger entries in the same
commit and one strict-horizon-ledger line per session. Stop and report only when a step's fallback
list is exhausted.

- **F0 — Unblock (decided, see below).** Ordinary runs gate at **15 GB** free (TTD recordings still
  need about 50 GB; check with `just disk` first). No reclaim is needed for F1–F6.
- **F0a — Before pulling on Windows, copy `src/recomp/` aside.** Game `52dc97b` untracks the lifted
  code, so the pull deletes it; copy it back afterwards (it is ignored from then on). If it is lost,
  rebuild it as `AGENTS.md` "Rebuilding the lifted code" says, and record what that took.
- **F0b — Rebuild on the new toolkit first.** `just build` then `just test` (new or changed:
  `jsrf_dpc_queue_bridge`, the NV2A contract's PUT cases, the guest-meter serial cases). Toolkit
  `b857665..179439b` changed D1, D2, file I/O, DPCs, the timer period and the mixdown, so the
  first run after this may stop somewhere new; record it in the horizon ledger before chasing it.
- **F1 — Find the table writer the cheap way.** Three single runs, cheapest first; stop at the first
  that names the writer. All may be exploratory (`RECOMP_KERNEL_LOG_BUDGET=100000`).
  - **(a) Reads.** List every `[READ] … dst=0x… got=N` line whose `[dst, dst+got)` intersects the thunk
    table `0x001C3F60..0x001C4140`; if none does, widen to `0x001C2B20`, where index 0 would sit if the
    40-byte-stride record array (index `0x79` at `0x1C3E08`, TR §5) starts at index 0. Reads now go
    through a host bounce buffer (L29), and the line warns when a read lands in a read-only section.
    A hit names the file, offset and caller: fix why that buffer address is wrong.
  - **(b) Tripwire.** `RECOMP_RDATA_GUARD=1` (L32, observation). The first `[RDATA-GUARD] write`
    inside `0x001C3F60..0x001C4140` gives the value, the module-relative RIP (symbolise it against
    the PDB) and the guest return chain. Do not combine with `JSRF_TRACE_A2H_DR`/`_SLOTW`.
  - **(c) Where the array belongs.** In xemu (T3), stop at the same horizon and search guest RAM for
    the record pattern (`0x3E800000`, `0x41200000`, `0xFFFFFFFF`, `1`, `0x001FA1D8` at a 40-byte
    stride). Its address on real hardware says which base pointer the port computes wrongly.
  No answer → `RECOMP_GUEST_SERIAL=1` (L34, D4) and re-run (b); then the C1 TTD recording with
  `ttd -stop`. (The old fallback "(a) VP DMA" is gone: D2 is fixed.)
  - **DONE 2026-09-30, at step (b), and the writer is named.** (a) was answered from the already
    archived strict run `20260930-081601-393-read-dst-verify`: 14 `[READ] dst=` lines, **0** intersecting
    the table or the widened range; no `[READ] WARNING` in either run. (b) then named the writer:
    `rip=exe+0x5361B8` = **`sub_00038530+0x398`**, the slot writer A2h already named, at a different
    site — a `rep movsd` image copy running upward from `0x00011000`. The array is not built at the
    table: it is **copied there from the XBE's own `.data`** (the dump at `V` equals the XBE at
    `V + 0x37608` for 431 of 436 sampled 4 KB pages, 0 original; the dump's bytes at the table VA are
    byte-identical to the XBE's at `0x001FB568`). Recorded as **D5** in the compatibility ledger and in
    `docs/jsrf-technical-record.md` §5. (c) was therefore not needed and was not run.
  - **The fix, and it is F3's first job:** the copy's **destination and length** are wrong, not the
    table. `sub_00038530` computes both. Not yet established: why the copy runs at all, and which of
    its callers is responsible. Cheapest honest class first (ledger Rules); do not patch the table.
  - **SUPERSEDED — the three paragraphs below record F3's intermediate states (cause found, then
    blocked, then fixed). They are kept as history; the authoritative outcome is the F3 DONE block
    below, which follows these historical notes and precedes F4.** The reasoning that follows is what
    the fix rested on, and the "BLOCKED" wording in it describes a state that no longer holds.
  - *(historical)* **F3 CAUSE FOUND 2026-09-30 — it is a wrong tail-jump alias.** The guest called
    `0x00037550`, but the translator classified that address `detection_method: tail_jump_alias` and
    folded it into `sub_00038530`, deleting its body. `0x00037550` is a real function (its own SEH
    prologue, its own bare `ret` at `0x00037603`, its own jump table at `0x00037FB4`) and occurs
    exactly once as a pointer in a function-pointer table at `.data VA 0x001EC108` with **0** direct
    call sites — so the table is its only route, and the fold silently enters the wrong function. The
    generated dispatch's own `[ALIAS-ICALL] target=0x00037550 owner=0x00038530` appears in **both**
    runs, and in the guard run it is 19 log lines before the first `[RDATA-GUARD] write`, on the same
    thread. This is the same defect class `config/recovered-functions.json` already documents for
    `0x000BCF40`, which was fixed by a recovery entry plus a boundary fix.
    **ATTEMPTED 2026-09-30 and BLOCKED on a toolkit limitation — this state is SUPERSEDED; the fix
    landed later the same session (see the F3 DONE block).** The config edit was written and tested,
    then reverted at that point; the tree was left clean and `recovered.c` unchanged. Three measured
    results, all still valid:
    1. **`recover-functions.py` aborted on exactly 2 of 3075 entries**, enumerated by translating every
       entry: `0x001063A0` (`/* TODO: arpl word ptr [eax], dx */` at `0x00106565`, past its `ret` at
       `0x00106560`) and the then-new `0x00037550`. Both were spans that ran past a terminator into
       non-code bytes. Tightening `0x000BCF40`'s end to its real `ret` at `0x000BD8B0` **worked** —
       the abort moved on to `0x001063A0` — so that part of the fix was sound and reusable.
    2. **`0x00037550`'s four TODO markers all sit inside its own 36-entry jump table at
       `0x00037FB4`** (`outsb`, `sti`, `popfd`, `aad`), which the translator decoded as instructions.
       The table's 36 targets are *all* inside the span, so the in-function-goto path applies.
    3. **The toolkit detects the table but the recovery path never uses it.** `_recover_cfg` for this
       address returns **3 jump tables** (41/41/5 targets) and decodes **0** instructions inside the
       table region — the CFG recovery is correct. But `translate_function` reaches its instructions
       through `decode_function`, which consults `self._recovered_cfg`, and that dict is populated
       **only by the coalescence path** (`translator.py:740`). `recover-functions.py` translates a
       reviewed entry directly, so `recovered is None`, the CFG knowledge is discarded, and
       `decode_function` falls back to a linear `disassemble_function` over the whole span — which
       walks into the table.
    **The way through was to end the span at the jump table**, so the linear decode never reaches the
    table's bytes. That needs no toolkit change and is what landed.
  - **Follow-up worth a census:** whether any *other* `tail_jump_alias` fold deletes a
    table-referenced function entry. `0x000BCF40`, `0x00037550` and `0x00026780` are three instances of
    one rule, and the rule's population has never been enumerated.
    **The census was attempted on 2026-09-30 and the instrument was NOT valid.** Scanning the XBE for
    each folded address as a raw 4-byte pattern reported 3501 of 3508 folded addresses as
    "referenced", which cannot be true — a 4-byte pattern matches by chance constantly. Recorded as a
    **failed instrument**, not as a population count. A usable census needs the function-pointer tables
    identified first (the way `0x000BCF40`'s and `0x00037550`'s entries were), then membership tested
    against those tables, not a raw dword search.
- **F2 — DONE 2026-09-30:** the DMA_PUT bit-16 mask (D1) is removed in toolkit `b857665`, and
  `docs/jsrf-kick-get-contract.md:60` records that `0x100410` is `NV_PFB_WBC`.
- **F2b — DONE 2026-09-30:** ML3 (toolkit `e43e9bf`).
- **F3 — DONE 2026-09-30. THE STRICT HORIZON IS CLOSED.** The kernel thunk table is no longer
  overwritten and `.text` is no longer displaced. Two wrong `tail_jump_alias` folds were fixed by
  recovery entries whose spans end at each function's own jump table:
  `0x00037550` (slot `.data 0x001EC108`, folded into `sub_00038530`, span now `..0x00037FB4`) and
  `0x00026780` (slot `.data 0x001EC10C`, folded into `sub_000278F0`, span now `..0x0002730C`). Two
  further config spans that ran past their terminators were tightened so the recovery pass could run
  at all: `0x000BCF40` (`..0x000BD8B0`) and `0x001063A0` (`..0x00106560`).
  **Measured, strict profile, `RECOMP_APU_TRAP=1`, budget 100000:**

  | Run | Outcome |
  |---|---|
  | `20260930-221054-913-f0b-first-run-new-toolkit` (before) | `0xE0424943` at 5.8 s, 17,906 lines |
  | `20260930-225440-580-f3-alias-fix-strict` (alias 1) | `0xC0000409` at 14.6 s, ABI failure at `0x00026780` |
  | `20260930-225739-446-f3-alias-fix-2-strict` (both) | **`diagnostic_deadline` at 93.0 s**, 487,394 lines, 0 invalid ICALLs, 0 `0xE0424943`, 0 exceptions, 0 ABI failures, 0 `[UNIMPL]` |

  `check-dump-mapping.py` went from `CONTENT_MISMATCH` in every prior run to **`matches: 1,
  content-mismatch: 0`**: `.text` at `0x00011000` is byte-identical to the XBE and `0x001C3F60` holds
  the runtime's installed `FE000000+` thunks. Full finding in `docs/jsrf-technical-record.md` §5.
  **Two caveats recorded with it.** (a) The recovery regeneration rewrote **3042 of 3075** bodies — a
  generator-version effect (`uint32_t ebp = 0`, `RECOMP_FCMP_CC`), not only the two alias repairs; the
  provenance record states this and the two intermediate runs isolate the aliases as what moved the
  horizon. (b) `0x00037550`'s span and `stack_args: 0` were re-verified after review and are correct:
  its own code ends at a bare `ret` at `0x00037603`, the `ret 4` at `0x00038525` belongs to
  `0x00038460`/`0x0003848E`, and the runtime logged
  `[RECOVERED] 0x00037550 returned; ABI verified (ESP/EBX/ESI/EDI)`.
- **F3's continuing subject.** The 93-second run ended at its own deadline with no fault, so the next
  stop is unknown. Fallbacks unchanged: the guest heap keeps failing → ML2 (replacement XAPI heap,
  *reimplemented*); a vblank wait hangs → ML5; missing APU interrupts or slow audio clocks → ML4.
- **F4 — Frames.** *(Historical — this was the active next step when recorded; §13's current-state
  block above supersedes it. The logo/cache observation work carried in this entry is **F4b** under the
  naming note above.)* **The `0x1720` walk blocker is IMPLEMENTED (toolkit `1f9309a`);
  the next measurement is whether the walk now advances past GET `0x8EF0`.**
  **Diagnosed 2026-09-30: the submission walk stops on the first method the model does not know.**
  On the fixed build (`20260930-230206-594-f4-frames-after-horizon-fix`, exploratory, 123.3 s, 629,781
  lines, 0 invalid ICALLs/exceptions/ABI failures/`[UNIMPL]`), the run logs **64** `[PFIFO] submit`
  lines: the first ten advance `get` (`0x1000` … `0x8B5C`), and **all 54 remaining report the same
  `get=00008EF0`** while `put` keeps advancing. Submits #12 onward carry
  `diag=unsupported_method … method=1720 … at=00008EF0`. `0x1720` is
  **`NV097_SET_VERTEX_DATA_ARRAY_OFFSET`**. `FLIP`, `present` and `FB_DUMP` are all 0 because nothing
  past that command is ever interpreted.
  **IMPLEMENTED 2026-09-30 in toolkit `1f9309a`, by the measured-inventory route, not a bypass.** The
  walk still rejects anything absent from the generated table; `0x1720` is now *in* it, because a real
  submission contained it. Seven NV097 methods were admitted, every one measured:
  `0x1720 0x172C 0x1730 0x1744` (the vertex-data-array-offset slots the title uses) and
  `0x1800 0x1804 0x1808` (PGRAPH antialiasing / blend / blend-colour). It is deliberately **not** the
  whole `0x1720..0x175C` array: the array is indexed, so a blanket range would admit slots the title
  never submits. Only the four measured slots are admitted and the first unmeasured slot (`0x1724`)
  still rejects — a test pins that asymmetry. Admitted methods flow through the existing state path
  (`pgraph_method` stores them in `PGRAPHState.methods`); no execution semantics were invented.
  **Two generator defects had to be fixed first**, both in `scripts/gen-nv2a-method-inventory.py`, and
  both are why the method was invisible: its decode budget was **4096 words** while `0x1720` first
  appears at word **8124** of the F4 ring's 72,353, and it derived the table from **one** ring, so the
  F4 ring alone would have **dropped 148 methods** the older ring contributes. The table is now the
  union of the rings named on the command line.
  **Tests:** five new functions in the toolkit's `tests/nv2a_actions_test.c` — accepted and staged,
  GET advances past it, the indexed-range control, unrelated methods still rejected, wrong-class and
  unbound still rejected, and a stream through the block commits. Verified both ways: all pass with
  the fix, and **15 failures without it** (GET pinned at `0x1000` with `unsupported_method`).
  **What this does NOT establish: that frames exist.** The next runtime question is the one below.
  **SMOKE-MEASURED 2026-09-30 (`20261001-004608-186-f4-smoke-1720-admitted`, 47.3 s): the blocker
  MOVED, and GET did NOT advance.** `unsupported_method` is gone (0 occurrences, was 54), so the
  admission works. The walk now stops at the **same** `get=00008EF0` with
  `diag=sink_capacity`, on all 52 submissions after #12, while `put` advances to `0x47A84`.
  **Cause, decoded from the ring:** the failing submission (`0x8EF0..0xA440`, 1364 words, 259 packets,
  0 jump words) stages **1109 methods**, and the sink is a per-submission array of **1024**
  (`nv2a_core.c`, reset per submission at `:1468`), so it overflows **within one submission** at packet
  #239 — not by accumulating across submissions. Integrity stays clean (0 invalid ICALLs, 0 exceptions,
  0 ABI failures, 0 `[UNIMPL]`); `FLIP`/`present`/`FB_DUMP` are still 0.
  **SUPERSEDED 2026-10-01 — owner-directed F4 sink ruling recorded.** Advisor chose A′: size both
  staging and sink to the existing 4096-word walk budget, move staging into PFIFO state, retain
  whole-submission atomicity and all other rejection rules. The local atomicity approximation is
  recorded as L40; hardware/prior art dispatch per method. Verbatim decision, basis, tests and reversal
  conditions: `docs/reviews/rulings/f4-submission-capacity.md` (TR §5). **Implemented; focused tests
  measured:** both sink and PFIFO-owned staging hold 4096 entries, both statically asserted against the
  word budget; 1024-packet budget and action/rejection atomicity unchanged. New tests accept 1109
  methods over 555 packets (1664 words, same method count but different shape from the real stream)
  and one count-1025 packet (1026 words). Reversing only the toolkit fix produces **9 assertion
  failures**; restoring it produces **344 passing register/clock contracts**. Toolkit action checks
  pass. The fixture holds only 2048 words, so no >4096-word budget rejection fixture was added;
  the static assertions pin method-capacity ≥ word-budget. Toolkit commit `e8a6e03`.
  **Full validation passed:** toolkit Release build, 5/5 CTest, 30 lifter unittests; game `just check`
  and `just test` (29/29 CTest). `xbox_guest_meter` passed both runs.
  **Bounded smoke measured:** `20261001-020407-358-f4-capacity-fix-smoke`, game `3be0adb`
  (archived record-only citation diff), toolkit `e8a6e03`; exploratory default-on GPU_ACK plus
  APU_TRAP/PB_EXEC/FB_WINDOW/log budget 100000. Requested 45 s, actual 48.336403 s,
  diagnostic deadline. Logged submissions #0–63 all OK, GET=PUT through `0x47A84`, beyond
  old `0x8EF0`; final GET=PUT `0x16648`. No sink/budget rejection. **Draw/clear/flip/present are
  UNKNOWN for this capture, not zero** — corrected 2026-10-01 by the Advisor fault-diagnosis
  ruling: the only `[GPU]` executor lines are printed during `xbox_MemoryLayoutInit`, before the
  guest ran, and `RECOMP_NV2A_TRACE` was unset, so no end-of-run counters exist. F4 frames remain
  unsatisfied; no strict horizon claim.
  Active ledger L14–L18, L20–L25, L39, L40; L19 dormant. Advisor **W14 CONTINUE until
  11:00 UTC or two more smoke iterations, whichever first**. Next: read-only deadline
  main/render wait-site diagnosis on this archive first; GPU-specific survey only if waits
  point to GPU. **Archive diagnosis corrected (Advisor fault-diagnosis ruling, 2026-10-01):** the
  guest looks like a running game loop — main tid 6984 polls input via `XGetDevices` in game code
  (`sub_00161C20`), with `DirectSoundDoWork` (`0x0019F260`) and the DSOUND critical section
  (`0x0019E438`, CS `0x001BA050`) repeating. Candidate flag `0x001BA04C` is DSOUND library state
  (**not** a render-arming flag; drop it as the F4 lead): 48 references, 43 `cmp …,0`, and one
  write `mov [0x001BA04C],1` at `0x001A2317` inside a DSOUND method calling `0x1A1C8A`. Thread
  59696 is in D3D `BlockUntilVerticalBlank` (ret `0x0018CE73`) and that return site appears 1,622
  times, so vblank delivery is INFERRED to work and the interrupt-delivery hypothesis drops to
  low. INFERRED from raw stack words (not unwound): D3D state-call addresses (`SetStateUP`,
  `UpdateProjectionViewportTransform`, `SetScissors`) sit in main's live stack region, suggesting a
  render path. **Open question: whether it renders and presents — this capture cannot answer it.**
  **ANSWERED 2026-10-01 — the instrumentation was inert, and the answer needs a fix first.**
  Observation run `20261001-023335-656-f4-observation-60s` (game `c01e292`, toolkit `e8a6e03`,
  `exe_sha256 1ecf8363…` identical to the capacity smoke) ran with `RECOMP_NV2A_TRACE=1` and
  `RECOMP_PB_SCAN=1`: 62.580312 s `diagnostic_deadline`, dump/checkpoints/GPU report good, but
  **zero `[PB]` lines and no post-`guest_entry` render output**. Root cause (Advisor, MEASURED in
  source): `src/main.c:529` installs the MMIO state owner → `nv2a_mmio_hook.c:681`
  `xbox_Nv2aClaimRegisterOwner()` → `xbox_memory_layout.c:173-179` clears `g_nv2a_ack_enabled`, and
  the legacy GPU body `:1048-1177` (acks, DMA_GET mirroring, pushbuffer scan, executor call,
  periodic report) sits inside that check. So **L16/L18 are set but inert on this build** and the
  switch is itself a **new instrument defect**. MEASURED: 64 `[PFIFO] submit` lines all `diag=ok`,
  GET=PUT through `0x47A84` (committed methods are real); six `[GPU]` lines all at log 32-38, before
  `guest_entry` (line 75). **Runtime counters are UNKNOWN for this capture, not measured zero**;
  that the executor never runs under the owner is **INFERRED from the code path**.
  **Ruling A (Advisor), FINAL GO design:** the owner's NV097→`PB_EXEC` path must be reached with
  **no second walk and no second GET**; rejection stays **atomic** (reject without executing);
  fixtures must pin **clear/flip counts**; the run must show a **post-guest periodic `[GPU]` report**
  as positive proof. Order: **capture per entry class → bindings → `action_commit` → consumer
  (ordered all committed classes; the kernel executes the NV097 subset) → last method → GET**. **Interface refinement (Advisor APPROVED):** the seam
  takes **four args `(subch, class_id, method, param)`** and **all committed entries reach the core
  callback**; the **kernel wrapper** filters to NV097 and keeps the skip count — replacing the earlier
  3-arg NV097-only-core-consumer shape (a policy-free core is the better factoring). Order and
  atomicity unchanged (callback after `action_commit`, inside the ok path); tests must show a
  **non-NV097 entry leaving `EXEC` counters unchanged** while the **wrapper skip count increments**;
  core local-callback tests check the **correct class including across a rebind**; the **skip count is
  read for the report under the same PFIFO lock**. Not a new shortcut, **no new ledger class entry**.
  Runtime seam: **core static fn + setter**, no env var, **no weak symbol**; "no `extern`" means the
  **core holds no `extern` executor reference** while the **kernel does register the seam**;
  registered before guest start, `PB_EXEC` presence the only trigger. Integration: **existing HAL
  target links across the whole toolkit** (dedicated real-exec minimal stub target only as fallback);
  earlier "A2 new targets" **superseded**. Standalone core tests: order, atomicity, rebind with a
  local callback. Owner lock: **free getter on the existing active flag**; legacy guard **logs once
  and skips `EXEC`**. Report only under the **consumer lock, 10 s cadence**. Flip accounting adds a
  **NEW `flip_stalls`** counter: `0x0130` is a completed swap that calls `FrameCounterFlip` but
  **does not increment the existing `s_gpu.flips`**; **`0x012C` is the `s_gpu.flips++`**.
  **Game-side registrant audit mandatory — no blocking callbacks under the lock.** W14 is extended
  **until A's first smoke plus one diagnostic**; **no more inert reruns**; **L18's edit lands at the
  coordinated cross-repository checkpoint** with the accepted toolkit code (the toolkit SHA recorded
  in the game's ledger record — separate repositories, so not literally one git commit), and until
  then L18 keeps its old entry. **Architecture A is IMPLEMENTED AND ACCEPTED** — toolkit
  `a71f9374ddb2a6685b790493855c835842228212` (9 files, +329/−10); the Advisor read the diff itself and
  ruled ACCEPT/GO with final validation green (core 5/5 in 2.34 s, 30 lifter unittests, game 29/29
  CTest in 28.81 s, all checks pass, conformant to the approved design). **F4 IS MET, exploratory:**
  the 60 s run `20261001-033129-276-f4-a-smoke-60s` produced a coherent guest image (the
  "Presented by SEGA" card), so F4's frame criterion is satisfied under the exploratory profile —
  **not** a milestone acceptance, which is carried with the **F6 milestone Review**. **W14 reset at
  10:32:32 UTC** on that first critical-path frame finding; the earlier extension is closed. The
  180 s observation run `20261001-033805-242-f5-sequence-180s` (same pair + `RECOMP_FB_DUMP`) is
  recorded as **observed** in the strict-horizon ledger and the ruling: 25 BMPs, 2 distinct hashes in
  ordered blocks (8 black, then 17 SEGA), pixels from the **guest draw surface** (not the window), one
  separate window-dump artifact at 10:38:19, counts as **lower bounds**. That run produced **no horizon
  move and no W14 change**; the F5 consult ruling that followed is recorded next. Commit and push the
  clean pair **before** the next run.
  **F5 consult ruling (2026-10-01, Advisor; verbatim in the F5 appendix of
  `docs/reviews/rulings/f4-submission-capacity.md`): the logo phase is NOT a stall** — the guest is
  doing its **first-boot HDD cache fill**, slowly. OBSERVED **360 `[PATH]` opens of
  `\Device\Harddisk0\Partition5\Media\…~`, 184 distinct**, 20–33 per 10 s report to the end of the log
  (last line 209119); the cache holds `Cache00-02.tbl`/`DmCache00-02.tbl` with
  **`JSRF_CACHE_COMPLETE00.CMP` written 10:39:41** and **Cache02 started 10:39:42**; payload **182
  files, 67.9 MB**; rate **~1.0–1.3 files/s in every recent run, with or without the executor** (48 s:
  64; 60 s: 82 and 80; 180 s: 184), so **the executor does not limit it**. **INFERRED** the SEGA screen
  covers the fill in stages (00 then 01–03); **UNCERTAIN** whether the logo is really **fill-gated**.
  The word **"stop" is REJECTED** — the frozen `VirtualQuery` sample in `submit_read_word` is a
  **capture-time location, not a stop** (guest live; the per-word `VirtualQuery` is a perf lead, not a
  defect). Window accepted only as one-shot at 10:38:19.247 (`frames==600`, `fb_present.c:341`), SEGA.
  **NEXT (authorized): ONE 600 s observation run**, same clean pair and profile plus `RECOMP_FB_DUMP`
  and `RECOMP_FB_WINDOW_DUMP_EVERY=600` (both observation-only), with the **≥15 GB disk gate** checked
  first (~5.3 GB save-root per run). **Seeded cache PREPARED, NOT AUTHORISED** — no owner decision now
  and no implementation. **W14 clock unchanged at 10:32:32.** Reversals in the appendix.
  **Duration cap raised to 600 — IMPLEMENTED (Advisor duration-cap ruling, replacement child; verbatim
  in the same ruling file):** one **shared constant** `MAX_RUN_SECONDS = 600` in
  `scripts/jsrf_run_profile.py` (bound **:676**), imported by `scripts/run-jsrf.py` (bound/message
  **:219-220**) — the ruling's own `:218-219`/`:668` citations are its verbatim text and stand as
  written. Measured boundaries: `1 -> 1`, `300 -> 300`, `600 -> 600`, `601`/`0`/`-5` -> `SystemExit 2`.
  **RED** (unmodified 300 caps): **4 failed, 2 passed, 6 subtests passed**, exit 1. **GREEN:** focused
  **4 passed, 10 subtests passed**; full `test_run_profiles.py` **38 passed, 100 subtests passed** in
  **1.29 s**; `just check` **exit 0**; `build-identity.py verify` **rc 0** (exe unchanged). Guards
  preserved (identity verify, empty save-root, disk gate, archive, classification). Commit title
  `harness: permit bounded 600-second cache observations`. **300 s
  is not a substitute** — two 300 s fresh roots each restart the first-boot fill and cannot show the
  >300 s decision rows. **The first 600 s attempt FAILED to launch**: `2026-10-01T11:01:26Z`, **rc 2**
  on the **dual 300 cap** — a **harness launch fact, not a runtime or guest fact**: no game run
  occurred, no run artifact exists, only launch logs; recorded as a launch failure and **not** a
  horizon entry.
  **600 s run TAKEN — `20261001-043629-961-f5-observe-600s-c4bcd2b-retry1` (exploratory; NOT a title,
  NOT a horizon move).** Parent-owned job `pwsh-2573`; same pair game `c4bcd2b` / toolkit `a71f937`,
  fresh empty root, no seed, `exe_sha256 7027fafa…b303` unchanged. `diagnostic_deadline`, exit 3,
  166 named frames, 1 snapshot 0 dropped, `gpu_report_ok` true. **Stop 11:46:33.913916** =
  `metadata.started_utc` 11:36:30.743170 + `result.duration_seconds` 603.170746. Exact typed counters
  (`gpu-reports-v2.csv`): clears 10500, draws 10500, with_coordinates 10500, indices 52482, flips 3506,
  flip_stalls 3506, unhandled_methods 1178547, distinct_unhandled 242, non_NV097_skipped 14.
  **Counts rise while the images stay static SEGA by eye — not new images, still not the title.**
  19 tables `distinct_mtimes = 1` ("not observed to change"); only the nine zero-byte `.CMP` markers
  change; **19/19 tables = DVD bytes + zero padding to a 512-byte sector** (exact padding fact).
  **Observer/helper full hashes: see the TR's 600 s paragraph (single authority — not duplicated here).**
  **No new run and no seed** until the CMP/table check loop is explained. **No wrapper-argument or
  loop-cause interpretation recorded** — *as recorded at the 600 s checkpoint; **superseded by D1/D2
  below*** (the source candidate is now explained and its test is pending; the no-seed decision is
  unchanged). Ledger IDs L14, L15, L16 (legacy ack
  retired; feed replaced), L17, L18 (owner consumer), L20–L25, L39, L40; L19 dormant. **W14 reset
  unchanged at 10:32:32; no strict-horizon move** — as of **12:10 UTC the clock has elapsed 1 h 38 m**.
  **W14 RESET (2026-10-01, superseding the above):** the D2 300 s smoke produced the **actual eligible
  event — the unnumbered `JSRF_CACHE_COMPLETE.CMP`** (0 B, `CreationTimeUtc == LastWriteTimeUtc ==`
  **`2026-10-01T13:27:49.9575858Z`**) — so the **horizon moves to `13:27:49.9575858Z`** and the
  **ceiling to `17:27:49.9575858Z`**, **reset count 0**. The old `14:32:32` is **historical and
  superseded**. The eligible event is the **actual marker, not a frame**. **F4b CMP loop resolved; the
  new stop is after the marker phase.** **No title claim** (the frame is still SEGA).
  **Owner-resume W14 ceiling call (2026-10-01 18:19 UTC): CONTINUE with a fixed bound.** Same-route replacement Persistent Advisor `356aed7e-07f5-4327-aa32-88bdfe21ac8b` passed independent-read/second-message continuity under workflow §4.4 after prior child repeatedly failed delivery. Conservatively count wall time; owner pause does not reset the clock. No accepted source-only finding/reset, and no strict-horizon claim from the exploratory CMP event. Bound: **U1 plus conditional U2 reported, 20:15:00Z, or one packet, whichever first**. At that bound without an accepted critical-path finding/new eligible event: **STOP for owner decision, no second ceiling call**. U1: one source-only worker ≤45 min checking reset writers/update gates against original XBE. U2 only on a complete negative U1: existing binary/profile, canonical object captures ≥60 s apart and advancing frame-count control, no code/instrumentation/seeding; unchanged harness or explicitly authorized weaker 240 s/300 s cross-run captures. Full oracle, predicates and constraints: `docs/reviews/rulings/f4-submission-capacity.md` §W14 ceiling ruling on owner resume. **A/B still UNCLASSIFIED; no new run/patch/build yet.**
  **FINAL W14 STOP — owner decision required (2026-10-01 ~18:29–18:31 UTC).** U1 returned **UNKNOWN**: unresolved per-frame receiver/alias/inline writers, not proven immaterial. Bound (a) reached, **U2 not authorized, no second ceiling call or clock reset**. Replacement Advisor delivered STOP and accepted corrected branch/reachability wording. No new build/run/patch; D2 accepted, A/B UNCLASSIFIED, no F6/title. Parent original checks preserve only frozen eligibility: all four mode words zero permits `11070`; any nonzero branch skips it via JMP `124C3`; stage8 with ID1DDA present skips `666F0`. Worker raw-byte scan/address-reference overclaims rejected; no new cause or exhaustive writer proof. Owner options: recommended small observation-only packet for same-object within-run reads with RED/GREEN/Reviewer gates; weaker one-run cross-capture candidate; explicitly ledgered pragmatic shortcut; or pause/end. **None is authorized until owner chooses.** Full ruling/qualifications in the ruling record's §U1 UNKNOWN and final W14 STOP.
  **New stop — F6 candidate, UNCLASSIFIED after the cache.** ***Superseded reading:*** the earlier
  "`1A03` loop / poll" interpretation was the **sampled callee region** and the Advisor's **initial
  hypothesis**, **not the actual poll head** — **kept as history**. **Current classification
  (Advisor, own reads; read-only):** the **277/294 traffic is PER-FRAME RENDER WORK, not a poll** —
  **277@19E452 = 12 × 515**, **294 sites = 3 × 515 and 1 × 515**, IRQL pairs ≈ 515 each, **E_FAIL paths
  0 times**, all lock-stepping at **≈515 = one set per frame**; so **≈515 frames after the marker over
  ≈90 s ≈ 5.7 fps** (a **rate estimate only** — nothing timestamps individual frames). **Capture
  state:** root stack `… → 13A80 (game main loop) → 14D090 → 198F10 → 198ED0 → 191390 → 1912A0`
  (push-buffer kickoff), with **PFB_WBC = 0** and **GET == PUT == 0x5B50C**, so the flush and queue were
  **not stuck at capture** — a **snapshot fact, not proof of no stall at any other time**. **F6 candidate
  remains UNCLASSIFIED:** **(A)** a slow frame/time-counted sequence vs **(B)** a step gated on an event
  that has not happened (audio/movie/thread, given `RECOMP_APU_TRAP=1`). **Not called a stall or a
  hang.** **Next action — READ-ONLY, one stop (parent-assigned to the worker, in progress):**
  disassemble **`13A80`**'s per-frame dispatch, the vtable calls (`call [edi+0x14]`, `[edi+0x1c8]`,
  `[ecx+0x20]`) from **`13CB2–13EE4`**, with **`14D090`**; identify the active logo/scene object and its
  **step/timer field**, then read those fields from the new dump (the **mapping gate has already
  passed** for `062415`). **No run, no fix, no seeding** until (A)/(B) is classified. **The D2 closure
  is independent and stays accepted.**
  **Log evidence (parent-read, own):** lines **234619–622** show the **unnumbered marker `CREATE`** with
  return **`1456CB`**, the **`PATH`**, and **status 0** (the accompanying token in that record is not
  interpreted here).
  **Strict-path audit (qualified):** the strict-path counts came out **12 vs 474 matches**, and that
  set **includes the demo**; the **marker creates fall in an adjacent 6-line window**, which is
  **qualified — not a tid proof**.
  ***Historical — F4b D1 and the D2 design before GREEN.*** *As recorded then:* the F4b D1 probe was
  **INCONCLUSIVE** (`s_dir_contexts` live VA with **0 containing ranges**, **0/4928** readable slots,
  coverage **0.96%** over 113 ranges; **cause NOT proven**, occupancy UNKNOWN); the D2 design was
  **approved** with **no new run until its RED/GREEN and test gate passed**; the Advisor
  **`63c4869f…`** (ACK 1 + ACK 2) had delivered the **A–E ruling, reply 3, the D2 design ruling and the
  harness-plan ACK** (all verbatim in the ruling appendix); and the **bounded unit**
  (`docs/packets/f5-directory-context-close.md`) was **RED-only, parent-authorised**, with no ledger
  entry because the fix was **not yet introduced**.
  ***Current — ACCEPTED, tested, toolkit pushed.*** The bounded unit is **ACCEPTED**: RED verified →
  GREEN verified (focused 2/2 0.19 s; toolkit CTest 7/7 2.71 s; lifter 134 OK skipped=1 14.460 s; game
  31/31 31.32 s; `just check` passed) → **300 s smoke PASS-F5 (a)** with the **unnumbered
  `JSRF_CACHE_COMPLETE.CMP`** created; **W14 reset to horizon `13:27:49.9575858Z` / ceiling
  `17:27:49.9575858Z`**; **toolkit pushed** as **`a8262014eec9cd8d720184f7f2fb7dce4652105d`**
  (fast-forward `a71f937 → a826201`, 5 files, 571+/1−, test included, 0 blobs/secrets). **CURRENT
  PACKET remains none** — this is a bounded F5 unit, **not** a workflow promotion.
  Corrected evidence in TR and `logs/workers/f4-drained-no-frames-brief.md`.
  **Historical 2026-09-30 question (answer recorded then; superseded by the accepted capacity work
  and now by Architecture A):** Whether the answer is a larger sink,
  a sink that drains as it fills, or incremental commit during the walk is a design question about what
  the sink is *for*, not a constant to raise. Do not start the next long run until that is decided.
  *(Also noted: the comment at `nv2a_core.c:1461-1467` says "its 256 cap" while the array and its test
  are 1024 — a stale comment, not behaviour.)*
  **The next runtime question (historical, ANSWERED 2026-09-30):** *does the submission walk advance
  beyond GET `0x8EF0`, and if so, what is the next measured stop or the first draw/flip event?*
  **ANSWERED: no, it did not advance; the next stop was `sink_capacity` at the same GET (see the smoke
  measurement above).** That capacity blocker is **closed** (A′ accepted, toolkit `e8a6e03`).
  *(Superseded history: the Architecture A question — whether the owner path yields a post-guest
  `[GPU]` report and whether GET passes `0x8EF0` — was **ANSWERED** by the 600 s run above, which did
  produce post-guest reports. The **active** question is now F5's: the CMP/table check loop, pending
  the Advisor.)* Fallbacks once
  frames exist: missing draw forms/formats are fixed in the executor; a GPU stall on this path → ML6;
  if the title needs register combiners the CPU executor cannot show, start ML7's feasibility study.
- **F5 — Intro movies.** If the Sofdec intros block, skip them (ledger: *patched* or *intentionally
  ignored*); decoding them is post-slice (M29).
- **F6 — Title screen (M15).** Acceptance: a frame dump of the title screen plus the run record with
  its ledger IDs; compare by eye with an xemu screenshot of the same screen (T3). One Acceptance
  review for the milestone.

**Decisions taken with this direction (2026-09-30), each reversible by the owner:**
- The disk floor for ordinary runs is 15 GB (`scripts/check-disk-gate.py`); a run measured a few GB.
  TTD recordings keep the 50 GB expectation. Deleting old runs stays an owner decision and is not
  needed now.
- The strict-horizon ledger's scope is **from 2026-09-29 onward**, now the lint's default
  (`--since all` covers the archive); the 36 earlier runs are not backfilled.
- C3 is parked and C5 is decided (executor) for the bare minimum, as recorded in §7.

### Earlier entries

1. **T14's free-space gate**, then **V1 → V2 → V3 → V4** on the Windows host (DeepSeek chores;
   0 senior calls); start the strict-horizon ledger (W14) with V3's result.
2. Alongside, on any host: **T6, T7, T4**, then **T1, T2, T8–T13, T5**; T3 when the owner supplies
   the images. Owner approves the W-row document edits as they land (W7, W8, W15 first).
3. Then **C1** as the first packet (after W11), with C5's Advisor preflight in parallel.

**Updated 2026-09-30 (second entry, end of session).** Supersedes the note above.

**Phase 1 is complete** (T14, T6, T7, T4, T1, T2, T3, T5, T8–T13), and **Phase 2's
checks are implemented** (W1, W2, W3/W10, W4/W12, W5, W6, W7, W9, W11, W13, W14, W15,
W16; **W8's fallback half is owner-reserved**). `just check` runs ten checkers.

**The critical path moved twice more, and both moves are measurements:**

1. The `0xC0000409`-under-TTD reading was **wrong**: it was a missing
   `RECOMP_APU_TRAP=1` in the launch environment. With it, a traced run reaches the
   horizon (23,484 log lines, same site, same exit code).
2. **TTD cannot see the write class C1 is looking for.** 400,000 writes scanned, zero
   outside `jsrf_recomp.exe` and `VCRUNTIME140` — kernel-mode writes are invisible —
   and the clobber is confirmed from a **non-TTD** minidump. So C1's instrument choice
   is re-opened with the Advisor under §2.3.

**The §2.3 ruling has landed** (`docs/reviews/rulings/ttd-query-decision-input.md`,
"C1 instrument (2026-09-30)"): **W11 stands, TTD remains C1's instrument**, and the
Session's contrary conclusion was withdrawn because it rested on a trace truncated at
its size cap.

**C1's conditions are now all measured, and they reduce to one recording problem.**

| Condition | State | Evidence |
|---|---|---|
| C-a (process-exit end, below cap) | **achievable, but it excludes the horizon** | `--seconds 12` gave `Process exited … after 6797ms` at 240 MB — exiting `0xC0000409` **before** the horizon at ~7.3 s |
| C-b (terminal in the trace) | **FAILS on every existing trace** | 58 recorded reads of slot 65, **0** of them zero (`docs/reviews/c1-slot-reads-in-trace.md`) |
| C-c (mirror positive) | **MISSING** | a real store through a mirror VA is still required |
| C-d (kernel-write control) | **MISSING**, now possible | the toolkit logs `dst=` (`xboxrecomp` `1572256`) |
| C-e (census on an admitted trace) | blocked on C-a **and** the horizon | — |

**The trade is measured, not assumed:** a bound short enough to end by process exit ends
at 6.8 s with `0xC0000409`, before the horizon; a bound long enough to reach the horizon
is capped before the process exits. **`ttd -stop` is the only path that satisfies both**,
which S1 already anticipates — *"A trace ended by `ttd -stop` is out of scope unless the
terminal position precedes the stop."* The trigger must be a signal from outside, not a
timeout or a cap.

**The Session is BLOCKED on the disk floor before it can take that step.**
`scripts/check-disk-gate.py` reports **33.9 GB free against a 50 GB floor**, and T14 makes
clearing it an **owner decision** (*"archive or delete the rest after owner approval of
the policy"*). The reclaim plan reports 1,131 candidate runs holding 163 GB, of which the
808 `test` runs alone are **111 GB** — more than twice the shortfall. **None was created
by this session**; every run this session created is cited by a durable record and kept.

**So the next action is an owner decision, not a Session step:** approve a reclaim of the
archived `test` runs (or another subset), after which the Session can record the
`ttd -stop` trace, evaluate C-b on it, and promote
`docs/packets/c1-slot-write-attribution.md`.

**A second owner decision is recorded** in `docs/reviews/ttd-recording-termination.md`:
`check-horizon-ledger.py` without `--since` reports **36 pre-2026-09-29 runs** with no
ledger row, while `just check` passes because it passes `--since 2026-09-29`. Either the
ledger's scope is "from inception onward" and the bare invocation should say so, or the
rule is retroactive and 36 rows need backfilling — a packet-sized job.

Until a packet is promoted, packet implementation remains BLOCKED and the chores run
as owner-directed work.

### Follow-up leads from the 2026-09-30 F0/F1 session (recorded, not acted on)

- **`check-horizon-ledger.py`'s "last horizon move" reads a stale row, and always has.**
  It computes `moved = [row for row in rows if not row['not_reached']]` and takes the
  last one (`check-horizon-ledger.py:222-223`), but the ledger's **"Prior horizon
  (superseded 2026-09-29)"** table sits *below* the session rows, so its 2026-09-28
  entry is the last non-`NOT REACHED` row. Measured: at `HEAD~5` (before this session)
  and in the current tree alike, the reported value is **2026-09-28
  `20260928-185612-449-regen-v012-strict`**, even though rows for 2026-09-30 exist and
  this session moved the horizon twice. **This is pre-existing, not introduced here**,
  and it is a checker defect, not a ledger defect: the ledger itself is right. It
  matters because W14's ceiling rule ("3 packets or 4 h without moving the horizon")
  is judged from that number, so the rule has been measuring the wrong date. A fix
  would scan only the session rows, or exclude the superseded table explicitly.
- **`tests/test_generation_provenance.py` is not registered in CTest.** `CMakeLists.txt`
  names no such target and `ctest -N` lists 28 tests without it, so `just test` cannot
  catch a provenance failure — only a bare `just check` can. That is why the gate was red
  for 73 commits without a session noticing. Registering it is a small chore; it is a
  lead here because it changes what `just test` means.
- **`scripts/recover-functions.py` cannot complete a from-nothing rebuild.** It aborts at
  `0x000BCF40` (`RuntimeError: Incomplete translation`): the configured span
  `0x000BCF40..0x000BD8D0` runs 0x20 bytes past that function's real `ret` at
  `0x000BD8B0` into padding that decodes as `aaa`, which the lifter reports as
  `/* TODO: aaa */`. The live `recovered.c` predates the check and is unaffected. This is
  the first concrete blocker for the rebuild AGENTS.md records as never done end to end.
  **Diagnosed and fixed-in-principle 2026-09-30.** The abort is **not** a regression from the
  `86113c7` pull: a worktree at the pre-pull `1572256` emits the same `/* TODO: aaa */`
  for the same entry, measured directly. The committed `body_000BCF40` stops at
  `loc_000BD8AB` (its last label; no label at or after `0x000BD8B0`), so the generation
  that produced it never walked the padding — the config's `end` and the check have been
  inconsistent since `ddb0697` tightened bounds.
  **The fix is one field and it is verified in memory:** ending the entry at
  `0x000BD8B0` (the `ret`) yields a body with **0** TODO markers and all real labels
  preserved (`000BD889`, `000BD8A9`, `000BD8AB`); `0x000BD8B1` and the other candidates
  also clear it, but `0x000BD8B0` is the instruction boundary. Applying it is part of the
  F3 recovery work below, because it must be followed by a regeneration and a re-run.
  The boundary is confirmed against the XBE's own bytes: `.text` at file offset `0xAD8AB`
  holds `5e 5d 83 c4 10 c3 8d 49 00 90` — `pop esi` / `pop ebp` / `add esp,0x10` / `ret`
  at `0x000BD8B0` / `lea ecx,[ecx]` / `nop`, then padding.
  *(A note here first claimed `inspect-jsrf.py data` and `disasm` disagreed about the byte
  at `0x000BD8B0`. They do not: `data` prints little-endian dword **values**, so its
  `C4835D5E` is the bytes `5e 5d 83 c4`. The claim was a misreading of the output format
  and is withdrawn; it is recorded because this is exactly the transcription-error class
  plan W5/T10 exists for.)*

## 2026-10-05 — Game checkout renamed to `poison-jam`

The game checkout moved from `C:\Users\logic\Repos\my_xbox_game` to
`C:\Users\logic\Repos\poison-jam`, matching its remote. Records, review transcripts,
archived runs and DSH session paths written before this date name the old folder; they
were left as written.

## 2026-10-06 — plan condensed; turns title-001…004 summarised

`plan-jsrf-bare-minimum.md` had grown to 1,952 lines (170 KB), about 1,000 of them per-run narrative
that duplicated TR §9–§21, plus finished Phase 0/1/2 tables and the superseded TTD line (C1). It was
cut to its current state, fast path, next actions, backlog and milestones. **The full text before the
cut is `git show dcc93ab:plan-jsrf-bare-minimum.md`**; nothing was copied here, because the facts it
held are in the TR, `config/stop-chain.json` and the ledger.

The four DSH turns of 2026-10-05/06, each named after its `plan-turn-*` files (deleted in the same
change; read them with `git show dcc93ab:<file>`):

| Turn | First commit | What moved | Landmarks |
|---|---|---|---|
| title-001 | `7e683a3` | Stops 1–16, each confirmed by the next run (f16–f33): unowned gaps, swallowed later functions, alias-cut switch arms, the alias-folded job handler `0x32610`, `0x80340`/`0x80BD0`. `check-entry-extents.py` became a gate; `check-run-exercised.py` added after a run was credited with an address it never dispatched. Nondeterministic boot recorded (f25/f26). | `7e683a3`, `e96c586`, `9ea4e44`, `4cac35c`, `4c61042` |
| title-002 | `80d3f1e` | The stack-depth CFG validator (TR §10) and the 24 `stack_args` defects it proved without a run; stops 17–19 confirmed (g03–g05); the this-adjusting thunk `0x154540` found (stop 20). | `80d3f1e`, `c9173cb`, `f603e1e` |
| title-003 | `6f2e4e2` | The hidden-entry detector (TR §14) and its 50 repairs; stop 20 confirmed by g07; the under-wide `0xB5EB0` (stop 21); a host crash from a dispatch-table count left stale by a patch, found by re-running an archived binary. | `6f2e4e2`, `64945a3`, `0f07195`, `464df3b` |
| title-004 | `8e5b93a` | Dispatch count derived and gated (toolkit `2cee914`); the rename's loss of the strict archives fixed; the under-wide batch (stop 22, `0x48690`, `0xB06E0`); the stop-chain record gate (L43); F7b–F7d (152 + 7 spans); stops 25–28; the alias-shim census. Presents moved from 1000 to 888 after `0445a80`, recorded as a correlation (TR §19). | `2a1dbc9`, `892dd1e`, `b1aca89`, `0445a80`, `59f3ebf`, `8cd1e08`, `0cc6d5d`, `49bfd4e` |

**What the four turns did not do:** reach the title screen, or explain the present ceiling. A code
review of the toolkit the same day (recorded in the plan's current state and backlog) found that a
rejected NV2A submission walk would produce exactly the observed ceiling and that its diagnostic was
printed only for the first 64 submissions; the toolkit now logs and exports it.

**Removed in the same change (recover with `git show dcc93ab:<path>`):** the 13 closed-turn
`plan-turn-*` files; `.muse-workers.md` (it held only an Advisor handle retired 2026-09-29); and 16
retired scripts that no recipe, test, hook or active document used — the A2h instruments
(`scripts/a2h-map-rip.py`, `a2h-read-registry.py`, `a2h-slot-triage-classify.py`,
`a2h-frame-audit.py`, `a2h-null-slot-triage.py`, `a2h-oom-slice.py` and their three
`scripts/test_a2h_*.py`), the 2026-09-22 thunk-table probes (`check-thunk-relocation.py`,
`check-thunk-survival.py`, `check-displaced-ram.py`), `check-subagent-routes.py`,
`jsrf_dsh_source.py` (the P0.2 DSH review adapter), `v3-evidence.py` (Phase 0 V3), and
`decode-pushbuffer.py` (hard-coded to one run; superseded by `jsrf_gpu.py`). Historical records that
cite them were left as written.

**Pushed 2026-10-06 (Mac session), toolkit first; written from the push output.** Toolkit
`PUSHED_TO: origin / BRANCH: main / COMMIT: d5b0b830630c6a89adeca4e00dd5c86c9036464f / REMOTE_URL: https://github.com/danillogical/xboxrecomp / RESULT: fast-forward 2cee914..d5b0b83 main -> main`
(`28c289a` submit diagnostics and `RECOMP_NV2A_ADMIT_UNKNOWN`, `d5b0b83` four review fixes).
Game `PUSHED_TO: origin / BRANCH: master / COMMIT: d80f49e7841256ae7de67f9369788bb90cb762c9 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward dcc93ab..d80f49e master -> master`
(`f1c8ff7` cleanup, `14fbeba` tooling, `d80f49e` this plan and record). Outgoing game commits add no
`game/` path; largest new blob 213 KB; `scripts/secret-audit.py` over the 31 outgoing blobs: 0 hits.
Nothing here has run on Windows yet: the next Windows turn builds both, runs CTest (including
`nv2a_submit_diag`, `jsrf_logq` and `jsrf_nv2a_registers`) and takes the plan's next actions.

**The four open review findings, fixed the same day (owner request).** The fence mirror now publishes
the fence of the last consumed kick and a rejected walk is re-tried every 100 ms (L17, L40); the
present choice falls back to the buffer a frame cleared or targeted; alertable waits probe a signalled
object first and otherwise return `STATUS_USER_APC` after delivering (L42); heap frees merge without
leaving placeholders. The advisor rejected the first fence design (pausing the mirror deadlocks a
rejection that would clear, because D3D's ring-space wait at `0x1914F0` never kicks) before any code
was written. Toolkit
`PUSHED_TO: origin / BRANCH: main / COMMIT: dc04dc0344369ea95cbff2cfc62213a26ad5324d / REMOTE_URL: https://github.com/danillogical/xboxrecomp / RESULT: fast-forward d5b0b83..dc04dc0 main -> main`.
Game
`PUSHED_TO: origin / BRANCH: master / COMMIT: f92db3fb16d337739eeaa2f58b276246fcc68853 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 1dffbd6..f92db3f master -> master`
(5 text files; no `game/` path; secret audit over the outgoing objects: 0 hits). Verified on the Mac
by `posix_check` native/cross/python; `kernel_file_apc_test` and every runtime effect are first
exercised on Windows.

**Turn `title-005` (Windows): the present ceiling was one stale method-table entry, and it is
cleared.** The submission walk rejected the whole stream with `unsupported_method` on `0x1810`
(`NV097_DRAW_ARRAYS`), which was missing from the generated admission table although the executor
already implemented it; the table was regenerated (+`0x1810`, nothing removed) and a run with neither
the live-mirror nor the admit-unknown switch reaches presents **2410** against the old 888 with zero
rejections and `last walk ok` (TR §22). **M15 is not reached** — the run still ends on the graffiti
disclaimer. The four `dc04dc0` behaviours were judged on Windows: the fence mirror holds back-pressure
as designed, and the `RECOMP_FENCE_MIRROR_LIVE=1` A/B demonstrates the overwrite-rescue mechanism.
That A/B reaches **620** presents, not 888 or 1000, so it does **not** settle the exact historical
counts (TR §22 states the claim limit). Pushes for this turn:

Toolkit
`PUSHED_TO: origin / BRANCH: main / COMMIT: 505cda5b96e5b2a70492825df34151594805a1e5 / REMOTE_URL: https://github.com/danillogical/xboxrecomp / RESULT: fast-forward dc04dc0..505cda5 main -> main`.
Game (six commits, pushed in five batches)
`PUSHED_TO: origin / BRANCH: master / COMMIT: a8691e1, 2d99b30, 7e5db3c, a3e0b2f, 06b1e3d, 3cc084b / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 5e7a1a3..a8691e1, then a8691e1..2d99b30, then 2d99b30..7e5db3c, then 7e5db3c..a3e0b2f, then a3e0b2f..06b1e3d, then 06b1e3d..3cc084b master -> master`
— `a8691e1` (records), `2d99b30` (admitted-method list and wait witness), `7e5db3c` (review-1
remediation), `a3e0b2f` (the evidenced wrapped-ring caution), `06b1e3d` (the corrected receipts) and
`3cc084b` (remediation R1–R5 and R7). This receipt paragraph is itself carried by a later game commit;
that commit's own hash cannot be named here without making the receipt false, so it is identified by
position (the commit containing this text) rather than by a hash, and it is pushed with the same remote
and branch.

Toolkit-first order has **two independent witnesses**, neither of them a push transcript (none was
kept):

1. The **local reflogs**: `refs/remotes/origin/main` updated by push at 2026-10-06 22:19:39 local
   (−07:00) and `refs/remotes/origin/master` at 22:19:55.
2. The **GitHub server-side activity API** (`https://api.github.com/repos/danillogical/<repo>/activity`,
   re-fetched for this record): toolkit `dc04dc0…→505cda5…` on `refs/heads/main` at
   **2026-10-07T05:19:39Z**, then game `5e7a1a3…→a8691e1…` on `refs/heads/master` at
   **2026-10-07T05:19:54Z** — 15 s later, the same instants as the reflogs. Later pushes from this turn
   are also recorded there: game `a8691e1→2d99b30` at 05:21:31Z, `2d99b30→7e5db3c` at 05:42:30Z,
   `7e5db3c→a3e0b2f` at 05:43:51Z, `a3e0b2f→06b1e3d` at 05:45:37Z; all are fast-forwards
   (`before` is the ancestor, `after` the pushed tip). The reflog times for the same events are
   22:19:39, 22:19:55, 22:21:32, 22:42:31, 22:43:52 and 22:45:38 local (−07:00).

The toolkit has **no** commit after `505cda5`, so no later toolkit push exists to order against. The
outgoing game commits are records-only: **0 secret-audit hits scoped to this turn's objects** (14 + 8 +
4 + 4 + 6 = 36 objects over `5e7a1a3..2d99b30`, `2d99b30..7e5db3c`, `7e5db3c..a3e0b2f`,
`a3e0b2f..06b1e3d`, `3cc084b..594c1e8`, each blob audited individually), no `game/` path, largest blob
212 KB. (A whole-history `just secret-audit` reports 2 hits; both are the audit script's own pattern
literals and predate this turn.)

**The closing push of this turn** (`594c1e8`, which deleted the four `plan-turn-*-title-005.md`
artifacts) is recorded here as a follow-up commit, because a commit cannot carry a receipt for its own
push. Toolkit was pushed first and was already up to date at `505cda5` (`Everything up-to-date`); the
game closing push is confirmed server-side by the activity API as `3cc084b…→594c1e8…` on
`refs/heads/master` at **2026-10-07T05:51:51Z**, matching the local reflog at 22:51:51 (−07:00). So
the full order for the turn is: toolkit `505cda5` at 05:19:39Z, then the game's six pushes at
05:19:54Z, 05:21:31Z, 05:42:30Z, 05:43:51Z, 05:45:37Z and 05:51:51Z.

Next Windows turn: admit the six
runtime-confirmed state methods (`0BB0`/`0BB4`/`0BB8`/`0BBC`/`1724`/`1728`) and then fix
`budget_exhausted`.

## 2026-10-06 (later) — staffing substitution, and the generator's fail-closed guards

**Owner-authorised staffing change.** The `claude` route ran out of tokens, so the owner directed that
`codex/gpt-6.1-sol` stand in for it at matching effort: the Turn Planner role at `high` and the
Persistent Advisor role at `xhigh` (workflow §1 otherwise pins both to
`claude/claude-opus-5-5`). The Turn Reviewer already used `codex/gpt-6.1-sol` at `high`, so that role
is unchanged. This is an owner decision, recorded here as required; §1 remains the authority and a
future session should use the claude route again unless the owner says otherwise.

**`scripts/gen-nv2a-method-inventory.py` now fails closed (game `52657f4`).** The generator writes two
tracked files — this repo's `docs/jsrf-nv2a-method-inventory.md` and the toolkit's
`nv2a_method_table.c` — and parses argv by hand. Two destructive paths were measured, not theorised:
`--help` fell through both filters and regenerated from the default single old run, dropping ~86
methods while printing `wrote ...`; and `--budget=0` stopped the walk on its first word and wrote a
three-method table over the real toolkit file. Both were caught and restored from git, and both are
now refused, along with an incomplete decode (a run that did not reach PUT) and any generation that
would remove methods. `--table-out`/`--doc-out` redirect both writes so the tool can be exercised
without touching either repository, and the documented three-run invocation still reproduces both
committed artifacts with zero differing lines.

**A durable provenance limit found while testing the guards (TR §22).** "Reached PUT" is necessary but
**not sufficient**: the generator is not the model's walk. It increments every parameter's method
(`m = method + 4 * i`) while the model honours the non-incrementing bit and rejects an incrementing
span past `0x1FFC`. Two verified fixtures diverge — header `0x400C1810` (non-incrementing, three
params) yields `1810/1814/1818` where the model writes `1810` three times, and header `0x00081FFC`
yields `1FFC/2000` where the model rejects with `method_range`. So the next packet must admit methods
from a **runtime committed witness** (`[PFIFO] admit-unknown`, queued only after a successful commit),
not from a decode whose only credential is reaching its own chosen PUT. The `0x1810` fix is unaffected:
it is a single-parameter incrementing packet, independently confirmed at runtime.

**A safety lesson about the test suite itself.** Running the guard tests against a *reverted* generator
destroys the real artifacts, because the reverted script ignores the redirect flags — that happened
twice during this work, both times caught and restored from git. The suite now preflights whether the
script under test can redirect both writes and **skips** the whole suite if it cannot, so producing a
RED baseline can no longer damage the tree.

## 2026-10-07 — the six witnessed methods admitted; the blocker beyond them is eight more

**Staffing, continued.** Claude capacity was still exhausted, so the owner re-authorised the same
temporary substitution for this continuation: Turn Planner `codex/gpt-6.1-sol` @ `high`, Persistent
Advisor `codex/gpt-6.1-sol` @ `xhigh`, Turn Reviewer `codex/gpt-6.1-sol` @ `high`, Orchestrator and
Workers `workbuddy-ai/deepseek-v4.1-flash` @ `high`. The roster in workflow §1 was **not** edited; this
remains an owner-authorised substitution, and a future session returns to the claude route unless the
owner says otherwise.

**F8b done (toolkit `46b3265`, game `779c6a0`).** The six methods `0x0BB0`/`0BB4`/`0BB8`/`0BBC` and
`0x1724`/`0x1728` are admitted from the runtime `[PFIFO] admit-unknown` witness — not from a decode, per
the limit recorded above. Admission is durable and auditable: `config/nv2a-runtime-witnessed-methods.json`
carries each witness with its run, log line and log SHA-256, and the generator unions it via `--witness=`,
failing closed on an unknown class, an unaligned method, a missing provenance field, or an unreadable
manifest. Measured delta: exactly `+6` on class `0x97`, zero removals, no other class changed, `0x1810`
retained (381 → 387). Ordered delivery is pinned by a contract in `tests/test_nv2a_hal.c` that reads the
real executor through a new narrow accessor (toolkit `ec98ffe`); it was validated by **mutation** — a
last-value-only constant handler fails 15 of its assertions. Full suite 45/45, `just check` GREEN.

**The next blocker is not the one the plan predicted, and the count was corrected by review.** Decoding the
region the walk was consuming when it reported the budget reports **39** NV097 methods missing from the table
(`0420`–`042C`, the full `0480`–`04BC` and `0680`–`06BC` runs, `1748`, `1B40`/`1B44`), found through the
repository's own `jsrf_gpu.missing_methods`, which expands an incrementing packet's parameter slots. An
earlier "eight" was **wrong** — it counted only packet START methods. They were never witnessed because
`RECOMP_NV2A_ADMIT_UNKNOWN=1` *bypasses* the method reject, and the witness queue only fills on a successful
commit, which a budget-rejected walk never reaches. So the six-line witness is a **lower bound**.

**Temporal provenance of that region is NOT established, and the review caught a circular proof.** The first
check decoded the reject-time and capture-time pointer ranges from the *same* final dump and compared their
common prefix — which compares the same bytes with themselves. The replacement bounded the rewritten arc from
the `PUT` trajectory, but the archived log holds only **three** pointer samples for that phase (the reject at
`PUT=0x4E680`, one `still rejecting` at `0x6D060`, and the final `0x3E984`), so the distance is
`114881 + k·131072` words for unknown `k` and a full lap cannot be excluded. (A draft claim of "170 re-reject
lines" was also wrong: `170` is the `rejections=` counter, not a count of pointer records.) The 39-method
list is therefore **conditional**, and must not be admitted without a runtime witness.

**Budget mechanism: strongly indicated, not proven.** The header-path dump is unconditional and never
printed, while the parameter path emits none, so exhaustion was **inside a packet** — conditional on complete
stderr. Raising the budget remains explicitly **not** the fix.

**Honest coverage note, revised.** The post-admission title run drained the walk with zero rejections and
`last walk ok`, which shows the admission **broke nothing**. Whether it **exercised** the six is **NOT
demonstrated**: final-ring absence is not proof of absence (the ring is reused, and the bytes at `0x2FF4C`
demonstrably differ from m15), "zero rejections" is not proof (the pre-admission `fixed` run also had zero),
and the `[PFIFO] submit` log is capped at 64 lines so it cannot establish that `PUT` never wrapped. Two of my
earlier measurements were wrong and are corrected: `[PFIFO] submit` logging is capped at 64 lines (so its max
`put=` is not the final PUT), and `[FBPRESENT]` lines are sampled (so a line count is not the `presents=`
counter — the apparent "163 vs 2410" regression was a units error; the counters are 1490 / 2410 / 1680 with
unequal durations, which shows only that the additive diff is not a proven regression, not that none exists).

**Review also hardened the witness path.** The pinned generator accepted a manifest entry whose declared
class/method contradicted its quoted witness, with an invalid hash and no log line — i.e. it could admit a
method no witness justified. Validation now cross-checks the witness text against the declared class and
method, requires a 64-hex `log_sha256`, and — when the archive is present — verifies the log's actual hash and
that the quoted record appears in it. Four forgery vectors (contradictory entry, stale hash, fabricated
witness text, malformed hash) were each verified to fail closed with both artifacts untouched.

Push receipts: toolkit `505cda5..46b3265` and `46b3265..ec98ffe`, then game `610b12d..779c6a0`.

**Review remediation and the first-stop instrument.** The Turn Review returned **FIX** on five blockers; all were
closed (game `726402f`, toolkit `b95a24e`): the witness gate was hardened so a manifest entry whose declared
class/method contradicts its quoted witness, or whose hash is malformed or stale, or whose quoted record is
absent from the archive, is refused (nine regression tests, six of which fail against the pre-fix generator);
the incomplete "eight methods" count was corrected to **39**; the circular temporal proof was retracted and the
region's provenance recorded as **not established**; changed-path coverage and no-regression were downgraded to
**not demonstrated**; and the toolkit header that contradicted its own handler was fixed.

Then the measurement the Reviewer said was missing. The walk's budget stop was the least observable event in
the model: it can fire at a packet HEADER or inside a packet's PARAMETERS, and **only the header path printed**,
so the case that actually happened stopped silently — which is why the previous turn could only *infer* "inside
a packet" from the absence of a dump. Two further limits compounded it: the `[PFIFO] submit` lines stop after 64
walks, and the "last 32 visit addresses" array was written only at headers but indexed by TOTAL words, so its
slots were sparse, stale and out of order — it could not distinguish a long valid stream from a cyclic walk,
the one thing it existed to do.

Toolkit `4b6cc2f`/`76c76fd` and game `1d71631` replace that with one emitter both exits call, latching the
FIRST stop into the exported submit state so it survives the log cap, stderr filtering and the retries that
follow a rejection: which limit fired and where, the straddling packet, and a dense chronological trajectory of
the last 64 consumed words (headers *and* parameters). `budget_local_pc` is deliberately separate from
`submit_diag_get`, which is the **rollback origin** the stream is retried from — reporting that as the failure
point is a known trap. The game report decodes the new fields, keeping the base fields decodable so archives
built before the change still read correctly rather than misreading adjacent memory as a transcript.

Pinned by a deterministic test whose shape is forced by the walk's own guards: the sink guard rejects a header
when `staged + count > 4096`, and one packet carries at most 2047 parameters, so a single packet can **never**
reach 4096 words — the stop would always land on a header. The parameter path needs counts `2047, 2043, 5`, so
the third header passes the sink check (`staged + 5 = 4095`) and the word cap fires with one parameter unread.
Validated by mutation: deleting the parameter-path call makes the test fail.

Push receipts: toolkit `ec98ffe..b95a24e`, `b95a24e..4b6cc2f`, `4b6cc2f..76c76fd`; game `94b3e32..726402f`,
`726402f..1d71631`.

**A second review round found two real defects in that new instrument, both mine.** Toolkit `07e7dac`, game
`02c5abc`.

1. **The trajectory kept rolling after the first stop.** The scalars latched but `submit_trace_word` ran on
   every later walk, so a dump paired the FIRST stop's scalars with the MOST RECENT walk's words — and a retry
   consumes different bytes, since the guest keeps writing the ring. Recording now stops once a stop is latched.
2. **My test could not detect it, and neither could I when I first checked.** It retried the *same* stream and
   compared counts, which are identical either way; only contents differ. Worse, the corrupted word lands
   mid-window, so removing the freeze guard still passed. The test now corrupts a word inside the window and
   compares all 64 entries, and removing the guard fails with exactly the expected evidence
   (`trajectory[32] ... word 111107DD->DEADBEEF`). **Lesson worth keeping: a retry test must change the bytes
   and compare the whole window, or it tests the counter rather than the recording.**
3. **A regression I introduced in the reader.** `jsrf_gpu.py` asked for the current struct size
   unconditionally, so a pre-change archive had adjacent globals read as budget fields — admit3 reported
   `budget_count = 44040355` from unrelated memory. The size now comes from the archive's **own linker map**
   (64 bytes before the transcript, 96 after). A source-hash lookup was tried first and is unusable:
   `build-source.json` records the SHA-256 of file *content*, and a run built from a dirty tree matches no
   commit. The decision is split into a testable function and covered by a test that goes through
   `read_submit_state` on both real archives, because a decoder test cannot catch a size-choice bug.

The review also confirmed the earlier fixes hold (B1/B2/B3/B5 closed) and that **B4 was still open at the pinned
revision**: the record said "so there is no regression" from counters at unequal durations, and that the
admission "broke nothing" while exercise was undemonstrated. Both are now retracted — the counters invalidate
the old comparison without establishing the absence of a regression, and an unexercised run is close to silent
about the changed path either way.

Push receipts: toolkit `76c76fd..07e7dac`; game `10654f0..02c5abc`.

## 2026-10-09 — review of title-007..010, and the upstream v0.13.1 merge (Mac session)

A review of toolkit `dc04dc0..5d6ebbd` and game `5e7a1a3..068eaec` found two walk defects introduced
by the unit commits (`1f86fbb`) and fixed them before the merge (toolkit `fafe0f6`, game `51bc1ff`; see
the plan's Current work). Upstream v0.13.1 was then merged as one commit, `409c635`, with upstream's
pushbuffer executor adopted and the fork's integration ported into it (owner decisions: land on main;
adopt the executor). The inventory and the resolution as applied are
`docs/reviews/upstream-v0.13.1-merge.md`; the summary is TR §1 "v0.13.1 sync". Nothing has run on
Windows yet; the plan's next action −1 is the merge's gate.

Toolkit
`PUSHED_TO: origin / BRANCH: main / COMMIT: 409c63541aa526819828b26089bdffae08ff69f9 / REMOTE_URL: https://github.com/danillogical/xboxrecomp / RESULT: fast-forward fafe0f6..409c635 main -> main`
(earlier the same session: `5d6ebbd..fafe0f6`). Game
`PUSHED_TO: origin / BRANCH: master / COMMIT: 49379d0683bcafcedec54f70c86181b1ff2a3e9e / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 51bc1ff..49379d0 master -> master`
(earlier: `068eaec..51bc1ff`). Outgoing game commits: no `game/` path, 6 blobs (largest 333 KB),
`scripts/secret-audit.py` 0 hits.

## 2026-10-09 — plan condensed again; turns title-005…010 summarised

`plan-jsrf-bare-minimum.md` had regrown to 1,012 lines: 474 of them were the closed title-009 turn's
narrative (itself carrying title-008's), and "Next actions" was three numbered lists concatenated across
turns. The superseded section is now a pointer, "Next actions, in order" is one list (−1 and 0-6) plus a
"Settled — do not re-run" list, and the still-open items from the cut text moved into the Backlog. **The
full text before the cut is `git show 2a580f7:plan-jsrf-bare-minimum.md`**; the facts it held are in TR
§22-§25 and ledger L39-L55. Turn boundaries below are reconstructed from `git log` and the run labels
(no `plan-turn-*` file exists for title-006 or title-009); the four toolkit hashes the table cites
(`505cda5`, `46b3265`, `1f86fbb`, `5d6ebbd`) were confirmed against the Mac's toolkit clone.

| Turn | First commit | What moved | Landmarks |
|---|---|---|---|
| title-005 | `a8691e1` | The present ceiling was one stale method-table entry, `0x1810`; a no-switch run reaches presents 2410 with zero rejections (toolkit `505cda5`, TR §22). The four `dc04dc0` behaviours judged on Windows; the 1000 → 888 count recorded as a hypothesis. | `a8691e1`, `2d99b30`, `7e5db3c`, `594c1e8` |
| title-006 | `52657f4` | The method-inventory generator fails closed; the six witnessed methods admitted from a runtime manifest with mutation-validated ordered delivery (toolkit `46b3265`); the first-stop budget instrument and archive-size-aware decoder; two Turn Review rounds. | `52657f4`, `779c6a0`, `726402f`, `1d71631`, `02c5abc`, `12cfb44` |
| title-007 | `14f56ad` | The M15 criterion rewritten from a hash blacklist to content; the `0x80084000` measurement and the disclaimer as a timed hold; input shown undeliverable (dead pad seam); the 39-method admission and the capacity bound cleared in whole-packet units (toolkit `1f86fbb`). Turn Review 1 returned FIX. | `14f56ad`, `74566ed`, `e7dd47b`, `078e176`, `b349768`, `56f8974`, `773083a` |
| title-008 | `1c594a9` | The same-flip trace (L47) and the 29-method admission; the black interval explained by the missing viewport constants and fixed (L48); the 38-method admission; three indirect-call recoveries (stops 29-31) and the 25-method admission; Turn Review 3 corrections. | `1c594a9`, `73ff567`, `827b537`, `0cba32a`, `11458dd`, `d63e792` |
| title-009 | `f007ebc` | The host clock overflow found and fixed (L53); `budget_exhausted` classified Case A with its packet-cap livelock recorded (L54, L55); the city drawn but not presented; the wrap-causation claim retracted, then corrected, in Turn Review 2. | `f007ebc`, `f426331`, `def4860`, `e9f187e`, `98b8a94`, `73d5d2c` |
| title-010 | `9526ea2` | The ADX "worker deaths" shown to be parked waiters and retired; the walk's per-word `VirtualQuery` fixed (toolkit `5d6ebbd`); the present-rate wall re-attributed to flip frequency; vblank and owner-lock telemetry decoded. | `9526ea2`, `1a3ae2e`, `068eaec` |

**What the six turns did not do:** reach the title screen. M15 is still open; the Mac review and the
v0.13.1 merge that followed title-010 are in the entry above.

**Checking the cut.** A second worker diffed the old plan against the new one (W5). It found five open
items the cut had dropped (the flip-2425 selection alternative, sampling/UV/blend/ordering reopened, the
binary-identity and run-comparability rules, the `batches_untransformed` reading, and F8b's owed A/B),
one wrong pointer (the `+4.2 s` epoch offset is TR §24.2, not §25.8), and two results recorded as more
settled than the TR allows: `budget_exhausted` rests on the 17-stop drain, not on the pre-repair 63/63/0
audit, which carries no weight (TR §24.1); and the stop chain is cleared through stop 27, with stop 28
repaired but not exercised. All were restored or corrected before commit.

**Other housekeeping in the same pass.** The closed title-008 turn files (`plan-turn-start-title-008.md`,
`plan-turn-updated-title-008.md`) and `scripts/test-native-gpu.ps1` (replaced by its Python port) are
deleted. Toolkit `a5e2762` tidies after the merge: two `nv2a_backend.h` comments name the merged vertex
interpreter, the translator cites `Lifter._lift_fpu` instead of a line range, README states the fork's own
test count, and three unused statics are removed; `tools/posix_check.py native cross python` passed on
the Mac with only the known failures.

Toolkit
`PUSHED_TO: origin / BRANCH: main / COMMIT: a5e2762d7f075865617b8ee90c340d79cd87d699 / REMOTE_URL: https://github.com/danillogical/xboxrecomp / RESULT: fast-forward 409c635..a5e2762 main -> main`

Game
`PUSHED_TO: origin / BRANCH: master / COMMIT: c10d1a21fac72f25f2d27ae8d2a057588e95cf56 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 2a580f7..c10d1a2 master -> master`.
Outgoing game commit: no `game/` path, 2 blobs (largest 239 KB), `scripts/secret-audit.py` 0 hits.

## 2026-10-09 — the v0.13.1 merge's Windows gate, the telemetry epoch, and two executor instruments

First session run from the Mac against the Windows box over ssh. The owner's rule: code on the Mac, copy
the files over with `scp`, and build, test, commit and push from Windows. Nothing is pushed from the Mac.

- **Next action −1, done.** Toolkit `a5e2762` and game `a2f185e` build, and every suite passes: game
  CTest 49/49, toolkit CTest 15/15, the four standalone toolkit test projects, and `just check`. The
  300 s A/B (`…141651-461-title011-ab-a5e2762` against `…142247-172-title011-ab-fafe0f6`) shows no crash
  in either binary, but the merged executor is slower: presents at t = 120 s 960 against 1750, and late
  re-arms 27.6 % against 0.4 %. The owner kept the merge; a performance regression on a non-working
  prototype is not a revert criterion (TR §1).
- **R3 control, inconclusive.** `…142848-669-title011-R3-control-5fd62cb` ran 900 s clean but reached
  only 1320 presents, against R3's 2885 at its crash (TR §25.5). Closing `0x9188C` is next action 0.
- **Telemetry epoch fixed.** The `nv2a_mono_clock.h` anchor was one per translation unit. It is now one
  `g_nv2a_mono_anchor_count` in `nv2a_core.c`, pinned by a two-file test that failed before the fix
  (TR §26.1).
- **Two instruments, always on.**
  - `[GPU] executor time:` attributes the slowdown: pixel fill is 207 s of 218 s busy, and vertex
    programs take 46 ms. With `RECOMP_NO_VSH=1` the same pixels go through the screen-space path, which
    is still slow (TR §26.2).
  - The stage-0 CONTROL0 latch shows the guest itself disables stage 0: 7581 of 16230 writes clear
    ENABLE (TR §26.3).
  - Both are pinned by a new arm of `jsrf_nv2a_hal`.
