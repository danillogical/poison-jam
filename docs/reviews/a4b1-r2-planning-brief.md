# A4b1-r2 planning brief — FROZEN

**Dispatched to:** `workbuddy-ai/kimi-k3`, `reasoning_effort` **omitted** (per the owner's instruction:
*"kimi k3 doesn't take an effort"*).
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Authority:** the owner's continuation directive (2026-09-25) and `docs/agent-workflow.md`.

## THE ONE TASK

> Produce the **smallest `A4b1-r2` packet necessary to re-bind the GP/DSP investigation to toolkit
> baseline `3f8bf67c…`**, incorporating **only** premise changes that can affect the
> implementation/evidence decision.

**Do not redesign `A4b1` merely because A4s changed many files.** A4s touched 4814 lines under
`src/kernel/`, but that is the event/AC'97/NABM work; `A4b1`'s write scope is the **APU**.

## THE NEW BASELINE (verified, not assumed)

| Fact | Value |
|---|---|
| Toolkit commit | **`3f8bf67c450861aefcbc376698750bc1446bc9dd`** |
| Branch / remote | `main` on `origin` = `https://github.com/danillogical/xboxrecomp.git` (**the owner's fork**) |
| `upstream/main` | `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` (unchanged) |
| Working tree | **clean** |
| A4s disposition | stage-1 **`ACCEPT`**, all 10 mandatory criteria `AGREED`, row **`R-SAME`** |
| Strict stop | **unchanged** — the `loc_001A18D0` DSP pending-word spin; `B = 0x803C0000`, `W = 3`, `F = 2` |
| Accepted exe | `E45026C3DF5AACAF3D66FCC1E17D9C6C1A12247864D58D0911CDBA8435A2A3C7` |

**`A4s` is complete and must not be reopened.** Its acceptance, its merge, and its rulings are closed.

## READ THESE, IN THIS ORDER

1. **`docs/reviews/a4b1-r2-premise-recheck.md`** — **the Session's premise re-check at the new
   baseline. This is your starting point and the core input.** It classifies every `A4b1-r3` premise as
   HOLDS / UNCHANGED / CHANGED / NEW, with the measurement behind each.
2. `docs/packets/a4b1-gp-core-port.md` — the **current `A4b1-r3`** packet (373 lines,
   SHA-256 `321ABCF7C9319B5AC4661B384A21CA2D2CE39C792D9820C9C322B7979EC7AFDA`). You are revising it.
3. `docs/agent-workflow.md` — §2.3 (your authority), §5.1/§5.1.4 (planning test + sketch/checkpoints),
   §5.4 (revising), §5.6 (premise freshness), §5.8, §6.1, §6.2. **§5.1.4 is mandatory for you.**
4. `docs/reviews/a4b1-a4b2-r2-adequacy-review.md` and `a4b1-a4b2-adequacy-review.md` — the in-flight
   adequacy verdicts on the `A4b1`/`A4b2` split. **Useful as design feedback**; the packet has already
   absorbed what it could (see `A4b1-r3`'s r3 delta line).
5. **Preserve these — do not reopen them** (a baseline change alone does **not** reopen a ruling):
   - `docs/reviews/a4b-watch-ledger-ruling.md` — **the authority for Device semantics 6 and 7**;
   - `docs/reviews/a4b-q1-advisor-ruling.md` — the Q1 ruling;
   - `docs/reviews/a4b-q2-owner-decision.md` — the owner's Q2 decision;
   - `docs/reviews/a4b-planning-rulings.md` — the checkpoint-40 constraints;
   - `docs/reviews/a4b-gpin-accounting-ruling.md` — the `[GPIN]` finite-universe/provenance-class
     redesign. **Do not resurrect any retired `A4b` mechanism merely because the baseline changed.**
   - `docs/reviews/a4b-xemu-pin.md` — the xemu pin (commit `67cc79e6…`, per-file SHA-256, licences).
6. `plan-jsrf-bare-minimum.md` — its `A4b1`/`A4b2` section and the `MIXDOWN_ALL` follow-up.
7. `docs/packets/a4b2-gp-clears-pending-word.md` — `A4b2-r3`, for boundary awareness only (**do not
   revise it**; it is your successor's job).

## MEASURED PREMISE DELTA — do not re-derive this; verify only if you doubt it

**A4s changed exactly TWO of the files `A4b1` touches:** `src/apu/apu_dsp.c` (+56/−3) and
`src/apu/CMakeLists.txt` (+7/−1). `apu_core.c`, `apu_state.h`, `apu.h` and `apu_mmio_hook.c` are
**byte-identical**. So the APU surface is almost untouched.

| Premise | Verdict at `M` |
|---|---|
| P-A `apu_dsp.c` has no DSP core, carries synthetic `dsp_ack_frame` | **HOLDS** (file 167→219 lines; `dsp_run`/`dsp_start_frame` still absent) |
| P-B GP/EP writes dropped, reads return 0 | **UNCHANGED** (blob identical) |
| P-C `apu_state.h` stale `DSPState` layouts | **UNCHANGED** (blob identical) |
| P-D APU reachable only under `RECOMP_APU_TRAP` | **HOLDS** (gate text identical; line 1728→2115) |
| P-E `0x80000000` window is separate storage | **HOLDS** |
| P-F `MmGetPhysicalAddress` returns the VA | **FORM CHANGED — premise STRENGTHENED** (see below) |
| P-G GP runs on the APU frame thread | **UNCHANGED** (blob identical) |
| P-STOP strict stop unchanged | **HOLDS** (`R-SAME`) |
| **P-NEW-1** `RECOMP_APU_MIXDOWN_ALL` default-ON in `apu_dsp.c` | **NEW — question RESOLVED by source read** |
| **P-NEW-2** `RECOMP_USB_PORT` (`ohci.c`) | **NEW, OUT OF `A4b1` SCOPE** (inventory only) |

### P-F — the one substantive change, and it went in your favour

The bridge **no longer returns the VA itself**; it now delegates:

```c
g_eax = (uint32_t)xbox_MmGetPhysicalAddress((PVOID)(uintptr_t)addr);
```

and the delegate (`kernel_memory.c:166-185`) is:

```c
uint32_t va = (uint32_t)(uintptr_t)BaseAddress;
return (ULONG_PTR)((va >= XBOX_CONTIG_BASE &&
                    (uint64_t)va < (uint64_t)XBOX_CONTIG_BASE + XBOX_CONTIG_SIZE)
                 ? va - XBOX_CONTIG_BASE : va);
```

**It returns no native pointer** — the old comment's warning ("*Don't call
`xbox_MmGetPhysicalAddress` which would return a native pointer*") is obsolete and was removed with it.
So `A4b1`'s address premise **holds and is better founded**: A4s removed a genuine inconsistency where
the bridge translated and the delegate did not, *"so the answer a title got depended on which dispatch
path it took"*.

**But it has one consequence you must fold in:** the inverse is now **`va - XBOX_CONTIG_BASE` inside the
contiguous arena**, identity outside. `A4b1` Device semantics 3 defines one translation function whose
comment *"names it the inverse of `bridge_MmGetPhysicalAddress`"* — that wording must now be precise
about the contiguous window. **This is a wording/precision correction, not a redesign.**

### P-NEW-1 — resolved, and it does NOT add a criterion

The plan recorded *"uncertain and load-bearing: whether `MIXDOWN_ALL`'s default-on path writes anything
the guest reads back"*. **Measured: it does not.** `monitor.frame_buf` is `int16_t[256][2]` at
`apu_state.h:500`, inside the **host** `MCPXAPUState` struct; instances are host allocations
(`apu_core.c:33` `static MCPXAPUState *g_state`, `apu_mmio_hook.c:16`); there are **zero** hits for
`XBOX_TO_NATIVE(...monitor)`, `MEM8/16/32(...monitor)`, `frame_buf…BRIDGE_MEM`/`MEM32`, or any
guest/monitor cross-reference; its only consumers are host-side `memcpy` into the audio output buffer
and `mixer_render`.

**So it is not new default-on *device* behaviour and needs no new criterion.** It remains a
`jsrf-run-profiles.md` **classification** item (a new unclassified variable) — a documentation
follow-up already in the plan. **`A4b1-r2` is nonetheless the natural place to record the disposition**,
since it lives in the file you are rewriting.

## WHAT TO DO WITH EACH PREMISE (the owner's four questions, answered)

- **Depended on the old baseline:** the four Readiness facts at `a4b1-gp-core-port.md:26` (the packet's
  own text says to re-check them at the baseline), **every line-number anchor into `apu_dsp.c`** (the
  file grew), and the **form** of Device semantics 3's inverse.
- **Unchanged:** P-B, P-C, P-G (blobs identical), P-E, P-D's gate, the strict stop, and every
  `A4a` R0/R1 observation.
- **Must be re-measured:** the baseline **exe SHA-256** and **ctest count** at promotion (the packet
  already requires this); the `apu_dsp.c` anchors re-derived **from text, never line numbers**.
- **Previous evidence that remains admissible:** all `A4a` R0/R1 run observations
  (`logs/runs/20260924-191906-091-a4a-r2-default`, `…-trap-trace`), the watch-ledger ruling, the
  Q1/Q2 rulings, the checkpoint-40 constraints, the `[GPIN]` redesign ruling, and the xemu pin record.
  **None depended on the toolkit baseline's APU internals.**
- **Cheapest discriminating experiment for anything still uncertain:** put it in the packet's criteria.
  **Do not pre-execute the investigation during planning.** If a premise can only be settled by running
  the title, that is an `A4b1` criterion or `A4b2`'s job, not planning work.

## THE PYTEST GATED LEAD — carry it forward verbatim

> - KX does not exercise the **7 pytest modules**.
> - This did **not** block `A4s` because nothing was regenerated.
> - **Before any later packet relies on regenerated output with this toolkit, all 56+ relevant modules
>   must run under real `pytest` in an owner-authorized environment.**
> - **Do not install, borrow, or otherwise introduce `pytest` without owner authorization.**

**This is a gated lead, not permission to change the environment now.** `A4b1-r2` does **not** regenerate
(the port edits source by hand; the packet's own non-goals already forbid regeneration), so **do not
manufacture `pytest` work merely to clear the lead early.** State in the packet whether `A4b1-r2`
regenerates anything; if it does not, the lead stays gated and unexercised.

## MANDATORY WORKFLOW (owner's directive — follow exactly)

**Step 1 — sketch, then the shape preflight.** Write a sketch of **≤15 lines** at the top of your draft
under a `Sketch` heading: bounded claim; packet class; material unknowns; proposed experiment/procedure;
outcome rows. **As soon as it exists and no later than 20 Planner tool calls**, send it to the Session
(agent id `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`) with `send_message`, marked
`SHAPE PREFLIGHT REQUEST`; the Session relays it to the **persistent Opus 5.5 Advisor**. Ask only the
four shape questions and require exactly:

```
SHAPE: PROCEED | REDIRECT | DISCOVERY_FIRST
REASON: <short>
POLICY_ISSUE: NONE | <bounded issue>
REVERSED_BY: <evidence>
```

`PROCEED` means you finish the packet **without** another general Advisor review.
`REDIRECT`/`DISCOVERY_FIRST` is binding. **After `PROCEED`, the Advisor does not redo the full plan.**

**Step 2 — write the packet** to a **NEW** file `docs/packets/a4b1-gp-core-port-r2.md`. **Do not
overwrite** `docs/packets/a4b1-gp-core-port.md`; the Session handles promotion.

**Step 3 — checkpoints.** At 40 tool calls: sketch 2, the yield against your forecast, and a new
forecast → send all three for relay. At 60 calls: **stop investigating**, send sketch 3 plus the yield
and remaining unknowns. Only the Advisor may authorize an extension.

**Step 4 — adequacy.** Your own review is a self-check only. A **fresh** Kimi Planner performs the formal
§5.3 adequacy review **if criteria or decision rows were materially written or re-written** — the Session
will spawn it. Return your §5.3 self-check block.

**Bounded scope.** Do not fold adjacent cleanup, architecture opportunities, or newly noticed defects
into `A4b1-r2` unless omitting them could plausibly cause a false PASS, a false FAIL, a wrong
implementation, a wrong evidence binding, or unsafe execution. Everything else is a **follow-up lead**.

**Constraints:** never push; the Session pushes accepted durable toolkit checkpoints to `origin` (the
owner's fork) only, never `upstream`; no force push; the game repository has no remote; preserve the
owner's uncommitted `docs/agent-workflow.md`.

## REPORTING

Report the **sketch first** — do not wait for the whole packet. Send the Session: what you produced, the
file path, the exact revision string and SHA-256, the line count, and what you need next. If you hit a
genuine Advisor-class question (architecture, evidence admissibility, methodology, or a conflict with an
existing ruling), send it to the Session for relay rather than deciding it yourself.
