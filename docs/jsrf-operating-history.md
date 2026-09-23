# JSRF operating history

Dated checkpoints and handoff narratives moved out of `AGENTS.md` on
2026-09-22, because that file is loaded with a 65,536-byte budget and had
grown past it — its tail was being truncated away unread. Nothing here was
edited in the move. `AGENTS.md` owns current operating knowledge; this file
owns what happened and why.

Chronological, and where a later section corrects an earlier one the later
one wins. `report-deepseek.md`'s CURRENT STATE block is the only place that
states where things stand *now*.

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
