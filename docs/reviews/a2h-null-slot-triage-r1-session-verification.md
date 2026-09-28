# `A2h-null-slot-triage-r1` — Session validation and promotion

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a2h-null-slot-triage.md`.
**Class:** **discovery** (§5.8) — the **writing Planner** reviews its own packet; **no second Planner**. A
**Muse Advisor shape preflight WAS obtained** because the packet changes the pinned toolkit, which the owner's
toolkit constraint makes preflight-mandatory for toolkit-scope work. Ruling:
`docs/reviews/a2h-critical-path-advisor-ruling.md` — **`SHAPE: REDIRECT`**, four binding corrections, **all
relayed and adopted.**
**Authoring Planner:** child `67dbadd0-99e3-47f6-96ee-d39c0bdbdc4a`, `codex/gpt-6-sol` @ `high`.
**Adequacy:** the writing Planner's own review — **`ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS:
BOUNDED`, **conditional on Session validation of literal commands and byte assertions (§5.1.5)** — performed
below.

| Item | Value |
|---|---|
| Revision | **`A2h-null-slot-triage-r1`** |
| SHA-256 (frozen) | **`F9A6522E8579AD756701C150A0AF60275DCFF4158705CE5331BE3BF2EA7A9F20`** |
| Lines | **80** |

**Hash read 3× over ~12 s and identical, after the Planner reported completion and the file settled.** An
earlier read caught the file **still changing** (`9760C637…` → `F9A6522E…`) — **the stale-pin hazard this
project has now hit twice**, and the reason the freeze was taken only after three consecutive identical
reads.

## §5.1.5 literal-command validation — the Planner's stated condition

**Every command and path the packet names was executed before freezing.** No command is invented and none
failed.

| Named item | Validation |
|---|---|
| `python -X utf8 -m unittest scripts.test_a2h_null_slot_triage scripts.test_a2h_frame_audit scripts.test_a2h_oom_slice scripts.test_pio_free_demand` | **`Ran 102 tests … OK`** |
| `python -X utf8 scripts/a2h-null-slot-triage.py --self-test` | **`SELF-TEST OK`** |
| `python -X utf8 scripts/check-run-profile.py logs/runs/20260927-160330-655-a4b2-gp-trap-trace` | works → **`STRICT`** |
| `python -X utf8 scripts/check-run-profile.py logs/runs/20260927-130036-879-a4b2-nr-baseline` | works → **`EXPLORATORY`** — matching the packet's stated profile difference |
| `python -X utf8 scripts/check-dump-mapping.py` (both runs) | works → **`content-mismatch: 1`** on **both**, confirming the packet's prohibition on using either dump for image-content claims |
| `python -X utf8 scripts/a2h-frame-audit.py verify` | **`ALL VERIFIED`** — the packet's byte assertions hold |
| `scripts/run-jsrf.py --profile/--seconds/--label` | all three flags exist |
| `RECOMP_KERNEL_WATCH`, `RECOMP_KERNEL_WATCH_ALL`, `RECOMP_KERNEL_LOG_BUDGET`, `RECOMP_GPU_ACK`, `RECOMP_APU_TRAP` | **all present in toolkit `c151d4e`** — the packet's "existing gate" claims are real |
| **`JSRF_TRACE_A2H_SLOT`** | **absent from game AND toolkit — it is the packet's own deliverable**, correctly gated |

**One correction the Session made to the packet's inputs:** the Advisor's ruling spelled the contrast archive
`…-a2b-nr-baseline`; **the actual directory is `…-a4b2-nr-baseline`.** The Planner caught this independently
and the packet uses the **exact** archive name. **The Session confirms the `a4b2` spelling is correct.**

## Validation of the design decisions that matter

| Decision | Session assessment |
|---|---|
| **Two pre-specified same-build runs (OFF/ON)** | **Correct.** The "one attempt" constraint was **the Session's own drafting**, mis-attributed to the owner; it has been amended, and two pre-specified runs with different gate states is a **control**, not fishing |
| **Per-thread fixed CAS latch as the authoritative record** | **Correct, and the Session was wrong to challenge it.** `docs/agent-workflow.md:794-798` requires decision inputs to be **lossless by construction**; a capped log is **observation only**. The Session's "the log is reliably archived" argument confused *archived* with *lossless* and is **withdrawn** |
| **`KWATCH` explicitly observation-only** | **Correct** — sampled and change-gated, so **no row may rest on the absence of a KWATCH sample** |
| **`tid` augmentation as a precondition, not a nicety** | **Correct, and Exp1 proves it necessary:** the archived `[KERNEL]` lines carry **no `tid`**, so per-thread windows are **not reconstructable from the archive** |
| **Terminal hook: two gated fields, `RaiseException` untouched** | **Correct.** The existing `[ICALL]` print **already** carries raw target/`tid`/`esp`/return (`recomp_manual.c:37-38`); the hook adds **live slot value and latest per-thread call #** as **corroboration and positive control**, not as the authoritative record |
| **No extraction from a `content-mismatch` dump** | **Correct and confirmed on both runs** |
| **Install positive control (original `0x80000115` → installed `0xFE000104`, slot 65)** | **Correct** — an image value, a logged event and a source transformation that must agree |
| **Producer line stays PARKED** | **Correct**, with the Advisor's two reactivation conditions |

## The Planner's freeze condition: **artifact extraction is PROVABLE — no scope revision needed**

The Planner made promotion conditional and offered a stop:

> *"Session must mechanically validate literal commands and archive/artifact extraction capability and
> freeze/hash/promote only on those checks; **if artifact can't be guaranteed without out-of-scope
> runner/collector change, stop and request scope revision** rather than promote ad-hoc log substitute."*

**It can be guaranteed, and the mechanism already exists — no runner or collector change is required.**

| Check | Result |
|---|---|
| `tools/harness/collect.c:207` resolves the latch's container **by symbol name** | `symbol_address("g_jsrf_debug")` |
| …and reads **`sizeof(JsrfRegistry)`** from the live process | `read_remote(address, registry, sizeof(*registry))` |
| …and places it in the archived dump's extra-memory set | `extra_memory[extra_count].base=address; …size=sizeof(*registry)` |
| …and reports its header | `GUEST_REGISTRY address=… version=… claimed=… overflow=…` |
| **Does an actual archive contain it?** | **YES — both runs:** `version=1 claimed=5 overflow=0`, with **5 `GUEST_THREAD` lines each** |

**So the latch's storage is already a fixed-layout, symbol-addressed, collector-archived structure, and the
collector reads it as a whole struct rather than field-by-field.** That is exactly what §6.1.6 requires of a
decision input — **lossless by construction** — and it means the packet's `src/diagnostics.c` /
`src/diagnostics.h` write scope **suffices**: the latch extends a struct the collector **already** archives,
so no new extraction machinery is needed.

**One design consequence the Session verified and the packet already respects:** `JsrfRegistry` is
**`version`-tagged** (`diagnostics.h:22`), and the collector prints that version — so a field added to the
latch **must** be accompanied by a version increment, or the collector and the latch will silently disagree
about the struct's layout. **The packet's `version`-tagged fixed-layout requirement is therefore not
decoration; it is what keeps the archived bytes interpretable.** The Session flags this as a **hard
implementation precondition**: *bump `version`, and have the readout refuse a version it does not know.*

**Also confirmed:** the collector already handles **exited** slots without reading their TLS
(`collect.c:228-229`), which matches the packet's *"never recycle or overwrite even after thread exit"*
requirement for the latch.

**Therefore: no scope revision is required, and the packet is promotable on the Planner's own condition.**

**No synthetic completion** — the packet does not suppress the APU trap or `0x80`, fake the allocation, widen
the arena, bypass the NULL call, or alter guest error handling. **No generated-code edits** (`src/recomp/gen/*.c`
untouched). **`PIO_FREE` stays DEFERRED**; `A4b2-r7`, accepted/closed `A4b2-r8`, `A4b1-r4` **not reopened**;
`0xFFFFB3` stays **`UNRESOLVED`**. **P4's discovery-transfer bridge must be re-established before any P4
inheritance** (recorded as a standing obligation).

## Promotion

**`A2h-null-slot-triage-r1`** frozen and promoted into `CURRENT PACKET` in the same step, byte-identical with
no revision (§5.3). **The Session did not edit the packet.** Hash read **3× over ~12 s**, identical, and
matching the Planner's reported value.

**Next:** the packet's Exp1 (offline — **already executed by the Session**, results in
`docs/reviews/a2h-null-slot-triage-exp1-evidence.md`), then the toolkit diagnostic change under the Advisor's
four binding corrections, then the two pre-specified runs.
