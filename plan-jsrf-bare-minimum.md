# Jet Set Radio Future: Windows port milestone plan

Agent onboarding, operating rules and handoff guidance live in `AGENTS.md`.
Maintain that guide as capabilities and commands change; this plan remains the
source of truth for milestone status and acceptance evidence.

## P0 — Repair the execution and evidence loop before continuing A2h

**Contract:** `docs/packets/p0-acceptance-contract.md`, revision `P0-AC-r1`.
**Status:** **P0.1, P0.S and P0.2 accepted.** P0.2's interface amendment is **ADEQUATE** at `r14`, and its **implementation review returned all four criteria AGREED** at round 5 — after four rounds of refutation that found five AC2 defects, four of them in the fix for the previous one. P0.3–P0.7: **all 18 criteria AGREED** on one evidence revision. Evidence: `docs/reviews/p0-1-vblank-adjudication.md`, `p0-2-acceptance.md`, `p0-3-to-p0-7-acceptance.md`.
**Next action:** record the P0.2 implementation review when it returns, then work P0.7-AC4's bounded deliverable at `docs/packets/a2h-writer-investigation.md` (ADEQUATE at `A2h-r4`). That packet authorizes one observation-only trace and one strict run — not a recovery pass, and not a whole-image census. Later A1–A5 material remains deferred.
**Scope:** bounded execution/evidence tooling, not a renderer or full translation regeneration.
**Authority:** criteria and statuses live here and in the linked contract; roster, session loop, reviewer and advisor policy live only in `docs/agent-workflow.md`.

**Advisor-approved prerequisite adjustment:** before P0.1's live baseline, complete
P0.S (`docs/packets/p0-save-isolation.md`) for disposable save-root selection and
real-path verification. Its AC5-r2 amendment preserves strict/profile criteria and
adds observed save-root identity. This moves only save isolation ahead of P0.6;
the rest of P0.6 remains pending. No baseline may write the existing save root.
P0.S is accepted: all four criterion dispositions are AGREED after native replay
and 12/12 Release CTest; record `docs/reviews/p0-1-execution.md`. P0.1's isolated
baseline is reviewed. Its VBLANK coverage disagreement was adjudicated by the
persistent advisor and a bounded correction applied inside frozen `P0-AC-r1`
(no criterion text rewritten); AC1–AC3 are reopened for re-review on the
corrected revision. See `docs/reviews/p0-1-vblank-adjudication.md`.

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
runner/checker tests, affected plan/report acceptance records.

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

Executable interface amendment: `docs/packets/p0-review-records.md`, reviewed
adequate at SHA256 `BAD23C071647EE4B7674820EEF501928E3B764AE6C82348E44EBB2246226432A`.
The subsequent structured per-criterion source-verdict addition awaits adequacy
re-review; that earlier hash is not approval of the changed amendment.
Implementation remains pending P0.1 acceptance. This amendment supplies concrete
schema, validator/export commands and source-backed Codex/DSH review procedures.

**MEASURED basis:** `report-deepseek.md` CURRENT STATE records the lost A2f
verdict and duplicated diagnosis. `scripts/check-recorded-reviews.py` is tied to a
specific DSH session, does not establish actual parentage, examines only the first
turn and matches generic verdict text anywhere in the report. The workflow's former
CANNOT VERIFY gap is historical: current closure behavior is defined in
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
plan/report status links.

**INFERRED benefit:** settled children and new findings survive context changes,
and the session cannot mistake another packet's ACCEPT for its own.

**Acceptance:** `P0.2-AC1`–`P0.2-AC4` in the linked contract. Durable recording and
per-criterion transaction behavior are the gap; do not restate the workflow's
current closure policy as missing.

### P0.3 — Finish the single-authority split and bound onboarding

**MEASURED basis:** the inspected plan repeats reviewer/route policy at
lines 28 and 64-140; the skill repeats route/spawn rules at lines 23-39.
The inspected CURRENT STATE retains retired/current-looking instructions at
`report-deepseek.md:488-524`; AGENTS retains dated next-packet instructions.

**INFERRED change:**
- Keep roster/loop/escalation authority only in `docs/agent-workflow.md`.
  Replace plan, skill and report replicas with pointers.
- Generate or mechanically check any AGENTS summary.
- Scope continuation mechanics to the relevant harness/version; verify persistence
  with an actual follow-up rather than route availability alone.
- Remove retired suggested-model assignments from active A1–A5.
- Shorten CURRENT STATE to current packet, revisions, criterion status, evidence,
  blockers and next experiment. Move narratives into history.
- Remove "all packets pending" and move old APU text out of A2h.
- Add a documentation check for active-policy contradictions, missing local
  command paths, stale authority links and UTF-8 instruction size.

**Files:** workflow, AGENTS, plan, CURRENT STATE, procedure skill,
operating history, new `scripts/check-agent-docs.py` and fixtures.

**INFERRED benefit:** the fresh DSH session receives one actionable instruction
set instead of resolving several generations of policy.

**Acceptance:** `P0.3-AC1`–`P0.3-AC3` in the linked contract. The checker must
include positive and injected-negative controls, report UTF-8 size against a
conservative budget below 65,536 bytes, and preserve historical records.

### P0.4 — Separate capture validity from guest-memory integrity

**MEASURED basis:** `report-deepseek.md:303-348` describes real displaced image
content with an intact stack. The inspected `scripts/check-dump-mapping.py:16-22`
then rejects that dump as evidence; lines 69-94 also permit a named missing
dump to return success.

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
`scripts/inspect-jsrf.py`, relevant tests, AGENTS, plan and report.

**INFERRED benefit:** the next session can use the corruption evidence without
discarding it or accidentally shifting the intact stack.

**Acceptance:** `P0.4-AC1`–`P0.4-AC3` in the linked contract. Structural
readability, stack/register location and image-content integrity remain separate;
all actual-VA reads and malformed/missing controls are required.

### P0.5 — Make the Python wrapper the single guarded build entry point

**MEASURED basis:** `scripts/build-jsrf.ps1` was absent at inspection while
onboarding and `scripts/build-identity.py:26-32` recommended it.
`scripts/build-jsrf.py:22-28` generalized a confined-policy failure to DSH.
The report and AGENTS correctly scope the failures to policy.
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
checkpoints. `report-deepseek.md:355-371` records policy-dependent temporary-file,
parallel-build and emulated-disk access failures.

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

**MEASURED basis:** `report-deepseek.md:532-553` records non-reproducible full
generation and overwritten ABI instrumentation. `scripts/build-identity.py:12-24`
fingerprints emitted sources but not the generation recipe.

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
onboarding, plan and CURRENT STATE.

**INFERRED benefit:** the next A2h instrumentation build cannot unknowingly
replace its translation baseline or erase diagnostics.

**Acceptance:** `P0.7-AC1`–`P0.7-AC4` in the linked contract. No-op/check-only
generation must leave production chunks byte-identical; mutations to inputs,
recipe or protected instrumentation must fail closed. After P0 acceptance, resume
only the bounded A2h writer investigation. A protective trap is containment, not
proof of a completed behavioral fix.

## Deferred roadmap and historical audit records — not executable

**Current next packet: P0.1** as defined in the P0 contract above. This A1–A5
sequence is a superseded planning snapshot retained for evidence and accepted
history; its old ordering, statuses, “next packet” statements and open work do not
authorize execution. No A1–A5 item may be resumed from this text. Before any is
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
- The three documents agree. `AGENTS.md` named only `report-jsrf-bare-minimum.md`
  as the current status, while the sessions and the hourly automation write
  `report-deepseek.md`; both are now named with their roles, and each carries a
  pointer to the CURRENT STATE block. No superseded NULL-ICALL/SEH/PRAMIN claim is
  an active instruction; the historical text is preserved with the corrections
  marked.
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
  requires. Its ranked mechanisms for the A2 chain are recorded in the report;
  the cheapest first move is a write-watch on `[0x0019D630..+0x20]` logging the
  caller PC, plus a probe at `0x00193D90`.

Update the current summaries in this plan, `report-deepseek.md` and `AGENTS.md`;
separate historical investigations from current instructions. Record the existing
D3D device/context/event relationship and distinguish strict runs from exploratory
runs using synthetic completion. Reuse the environment captured by `run-jsrf.py`.

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
removing `RECOMP_VBLANK` rather than keeping it. Full adopt/reject list in
`report-deepseek.md`.

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
rather than silently zeroed. Full detail in `report-deepseek.md`.

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
and its real additions are the fix, the strict run and the detector finding. The
verdict went unrecorded for a session; see the correction in `report-deepseek.md`.
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

### A2h — The displaced-RAM defect, root-caused

**Status:** **ROOT-CAUSED 2026-09-22; the fix is a separate packet.** **Depends
on:** A2g. **Evidence:** `logs/runs/20260922-224429-003-a2g-304f0-span/`; controls
and probes in `scripts/check-dump-mapping.py`, `check-displaced-ram.py`,
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

**The advisor's prediction was confirmed exactly.** Consulted on the
contradiction, it ranked an overlapping bulk transfer with
`source = destination + 0x37608` first and predicted the patched thunk table would
survive *relocated* at `0x001C3F60 - 0x37608 = 0x0018C958`. Measured: **110 of 120
slots survive at exactly that address**, slot 65 reads `0xFE000104`, and the
8-dword synthetic-VA sequence occurs there and **nowhere else** in the 64 MiB
window. It also warned that `0x37608` is a *separation*, not a copy length.

Acceptance for the fix packet: identify the writer of the transfer with evidence
(the advisor's cheapest experiment is a hardware write-watch on host
`0x001D4064`, armed after loader patching, inspecting the **simulated** guest
registers `ESI`/`EDI`/`ECX`/`DF`, not native ones); state its contract; fix it or
trap it; and archive a strict run whose thunk table is intact at its own address.
A `.text` control must pass for any dump used as evidence.

**Standing rule added:** `jsrf_dump.py` validates that a capture *has* a
`guest_ram=` identity but cannot tell whether that identity matches the payload.
Run the one-read `.text` control before any dump-based claim.

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
- Update the report and handoff guidance with both revisions, exact validation,
  remaining risks and **bold Decision entries** for autonomous recommendations.

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
full pass was being run without the project's manual-function list. See
`report-deepseek.md` for the entries and their evidence.

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
| 07b | In progress | Repair evidenced initialization ABI defects | Fix only reproduced EBP/register/stack contract defects and verify constructor outputs plus the next failure address. **Measured (2026-09-22):** two contract classes fixed with the run as the oracle — the `0x00168FF0` tail-thunk `stack_args` class (11 entries) and `0x00178F40`, whose `end` cut the last byte of its three-byte `ret 0x14` so the body had no epilogue at all (`ABI FAILURE ... expected +24`, observed delta 0). Correcting `end` to `0x001791BB` makes the run pass the abort and reach a 31.7 s deadline instead of dying at 2.1 s, with native threads 11 -> 12. Detector: `scripts/check-entry-extents.py`. **Still open, measured:** 40 of the 3068 recovered bodies contain no `return` statement because a head entry's span stops at an internal label (`0x00014870-0x00014880` ends at a `cmp` while `0x00014881` is the fragment entry and `0x00014880 push edi` belongs to neither); each is a latent abort of the `0x00178F40` kind. Evidence: `logs/runs/20260922-101516-467-p1-178f40/`, `logs/runs/20260922-101710-964-p1b-178f40-end/`, report "Session 2026-09-22 10:13 onward". | Luna + Terra review |
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
`logs/runs/20260922-055304-689-p6-ac97-ecx/`, report "The divide by zero, and
where the zero comes from".

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
see config/boundary-fixes.json and the live report for current artifacts.

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
| 11b4b5 | Complete | Recover the pushbuffer kick chain | All 16 chain entries added to `config/recovered-functions.json` (70 total) with `stack_args` read off real epilogues: `RET 8` on `0x00191530`/`0x00191440`/`0x001916C0`, `RET 4` on `0x00191390`/`0x00191730`/`0x001917B0`/`0x00190240`, `RET 0xC` on `0x00190FB0`, plain `ret` elsewhere. `0x001910C0` and `0x001910E0` are separate functions (fall-through); `0x001918E0` tail-jumps to `0x001917F0` and shares its frame. Confirmed the method blocks do not call `0x00191270`; the kick is reached from `0x00191440`. Three latent generator gaps fixed (see report). **The link blocker is cleared**; `jsrf_recomp.exe` now builds (19,245,568 bytes, identity-verified). | Clear the run blocker, then run the chain and expect the accepted-submission contract |
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
01g as partial; the wrapper only validates report creation. Detailed artifacts,
commands and limitations are in AGENTS.md and the current report.

The user requested a handoff focus due to remaining weekly usage. The corrected
11a contract audit is complete; the next bounded packet is 11b, not broad
renderer implementation.
Current GPU harness changes remain uncommitted in both repositories. Inspect and
preserve them before proceeding; earlier commit hashes represent the baseline.
