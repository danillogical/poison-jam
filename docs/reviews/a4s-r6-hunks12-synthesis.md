# A4s-r6: hunks 1–2 — Session synthesis of the measured evidence

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status: Session analysis for the Advisor, NOT a ruling.** The Advisor owns the hunk semantics
(`docs/agent-workflow.md` §2.3). This record assembles the measurements so the ruling can be checked
against them, and so the Planner has the reasoning trail. It is **not** authority and must not be cited
as one. The Advisor's ruling, when received, goes in `docs/reviews/a4s-r6-advisor-ruling.md` and wins.

## What hunks 1–2 decide

| Function | Ordinal | Declared by JSRF? | Local form | Upstream form |
|---|---|---|---|---|
| `bridge_KeSetEvent` | **145** | **YES — live** | in-place sniffer → `xbox_KeSetInplaceEvent`, else `xbox_KeSetEvent` | `ke_shadow_lookup` → `bridge_resolve_handle` → `XBOX_TO_NATIVE`, then `SetEvent(h)` |
| `bridge_KeWaitForSingleObject` | **159** | **YES — live** | in-place sniffer → `xbox_KeWaitInplaceEvent`, else `xbox_KeWaitForSingleObject` | resolve handle, then `xbox_KeWaitForSingleObject(h, …)` |
| `bridge_KeResetEvent` | 138 | **NO — unreachable** | in-place sniffer → `xbox_KeResetInplaceEvent`, else `xbox_KeResetEvent` | resolve handle, then `ResetEvent(h)` |

## The evidence, in order of strength

1. **The accepted contract test requires local's semantics** (`a4s-r6-contract-discrimination.md`).
   `jsrf_inplace_event_bridge` — a local-only test registered in the **game's** `CMakeLists.txt:118-125`,
   one of the 12 that passed 12/12 in `A4s-r5` step 2 — asserts guest `KEVENT.SignalState`
   transitions. The address identity `XBOX_TO_NATIVE(EVENT_VA)+4 == signal_state(EVENT_VA)` is
   **proved from source** and checked numerically for four host bases. Upstream's
   `KeSetEvent`/`KeResetEvent`/`NtClearEvent` bodies contain **no `BRIDGE_MEM` write at all**, so they
   cannot satisfy assertions that expect `SignalState` to become `0`/`1`. **All 15 helpers the test
   calls survive the merge** with both definitions and declarations.
2. **Ordinal 138 is unreachable** (`a4s-r6-ordinal-reachability.md`). Both sides' `bridge_KeResetEvent`
   bodies are dead code for this title, so the **duplicate definition is a mechanical obligation**
   (exactly one must survive), not a semantic choice.
3. **The timer path does not route through `bridge_KeSetEvent`**
   (`a4s-r6-clean-hunk-semantics.md`). `bridge_KeSetTimer` is byte-identical on both sides and calls
   `kernel_set_timer`; the fire path uses `ke_shadow_lookup(fired_va)` against the shadow table that
   `bridge_KeInitializeTimerEx` (ordinal 113, reachable) populates. So the timer machinery is
   **independent of hunks 1–2** and is unaffected by whichever form they take.
4. **Upstream's shadow table has only one reachable populator** — `bridge_KeInitializeTimerEx`
   (113). `KeInitializeEvent` (108), `KeInitializeMutant` (110) and `KeInitializeSemaphore` (112) are
   **not imported**. And `bridge_NtCreateEvent` (189, reachable) is **byte-identical** on both sides
   and does **not** insert into the shadow table — it uses the handle table. So a combined form would
   add a `ke_shadow_lookup` step that, for reachable paths, finds entries only for timer VAs — which
   the timer machinery already consults directly.
5. **The `Size`-convention hazard is latent, not live** (`a4s-r6-size-convention-hazard.md`). The only
   path writing a genuine `KEVENT` type (`0`/`1`) with `Size = 16` is `bridge_KeInitializeEvent`,
   ordinal **108 — not imported**.

## What this points to (Session's reading; the Advisor decides)

- **`bridge_KeSetEvent` (145) and `bridge_KeWaitForSingleObject` (159): take LOCAL's form.** The
  accepted, currently-passing contract test requires the in-place guest `SignalState` writes that only
  local performs, and the address identity is proved. Upstream's form would leave assertions at
  lines 81/86/91/92 reading a `SignalState` nothing writes.
- **`bridge_KeResetEvent`: exactly one definition survives.** Ordinal 138 is unreachable, so the body
  does not change JSRF behaviour; keeping local's keeps the file internally consistent with the
  in-place model chosen above, and its `else` branch still calls `xbox_KeResetEvent` for handle-typed
  callers. The **non-negotiable** part is that the count is one — two definitions in one translation
  unit is a compile error regardless of reachability.
- **A combined form is not indicated by the evidence I have measured.** Its `ke_shadow_lookup` step
  would have no reachable populator that the in-place model does not already handle, and the one
  reachable populator (timers) is consulted directly by the timer machinery.

## The one caveat I cannot close by reading

If the guest ever calls `KeSetEvent`/`KeWaitForSingleObject` on a **timer** VA, local's sniffer rejects
it (type `8`/`9`) and falls to `xbox_KeSetEvent(XBOX_TO_NATIVE(va), …)`, which treats a guest pointer
as a host `HANDLE`. I have **not** established whether JSRF does that, and I am not claiming either
way. Under an upstream form the same call would find the timer's shadow handle instead. Settling it
needs either a guest-code search for `KeSetEvent` call sites whose argument is a timer, or a run with a
witness — **packet work**, not a source read. Recorded as an open question rather than resolved.

## What the Planner needs regardless of the ruling

- The **duplicate `bridge_KeResetEvent` definition** must be resolved to exactly one.
- The **duplicate `case 138` labels** in both dispatch switches must be resolved to one per switch
  (both sides added the same logical line; measured byte-identical).
- A **post-resolution structural check** is needed, because both defects live in **clean** hunks and
  the conflict-driven rules cannot see them (`a4s-r6-structural-findings.md`).
