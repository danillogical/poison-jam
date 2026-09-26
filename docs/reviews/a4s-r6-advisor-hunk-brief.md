# A4s-r6 Advisor brief: the five unresolved hunks

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** evidence package for the persistent Advisor. **Not** a packet and not a ruling.
**Source of truth for the raw text:** `logs/a4s/conflict-hunk-inventory.txt` and the saved
`logs/a4s/conflicted-*` files, produced by the real merge attempt (`A4s-r5` step 3, selected
`R-CONFLICT`). Everything below is **Session-measured** unless marked otherwise.

## Repository pins

- toolkit: `0d7929c86771dd0b971941592fd4f15436116e82` (local / ours), clean
- upstream: `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` (theirs, = `v0.11.0` = `upstream/main`)
- merge base: `051a128df5ec27ef14f1ceaaead11c5457321eef`
- conflict-free merge tree (read-only preview): `75083476d3277f5c6d91eb7040f7a3ce5dc25335`
- game: `c1cdb9157bcb26c071c2ad83b69c85925f1c8d92`

## Newly measured facts that bear on hunks 1–2 (not in the earlier inventory)

1. **The merge tree contains a duplicate function definition — a guaranteed compile error.**
   `bridge_KeResetEvent` is defined at **line 1378** (local's copy, inside the conflicted hunk 1) and
   again at **line 6734** (upstream's copy, from a clean hunk). The base `051a128` defines it
   **nowhere**: both sides *added* it, in different places. Verified by classifying every `static …(`
   match in the merge tree as definition (next line `{`) or declaration (ends `;`):
   `bridge_KeResetEvent` = 2 **definitions**; `ke_shadow_insert`, `ke_shadow_lookup`,
   `bridge_resolve_handle`, `bridge_take_handle`, `bridge_write_handle` are each one declaration plus
   one definition. Script: `logs/a4s/dup-defs.py`.
2. **Local's in-place machinery survives the merge independently of these hunks.**
   `src/kernel/kernel_sync.c` is a **local-only** change (changed locally, untouched upstream), so the
   merge keeps local's bytes verbatim; the merge tree's `kernel_sync.c` is byte-identical to
   `0d7929c`'s. Upstream contains **no** reference to `InplaceEvent` anywhere in `src`. The merge tree
   still has `xbox_KeSetInplaceEvent` declared (`kernel.h:759`) and `guest_va_is_inplace_kevent`
   defined (`kernel_bridge.c:1340`), and nine call sites of it.
3. **Upstream's mechanism is a handle-shadow table.** `bridge_KeInitializeEvent`
   (`766ecef:6418`) creates a Win32 event and calls `ke_shadow_insert(guest_va, h)`; `KeSetEvent`,
   `KeResetEvent`, `KeWaitForSingleObject` resolve the guest VA through `ke_shadow_lookup()` and fall
   back to `bridge_resolve_handle()` then `XBOX_TO_NATIVE()`. Local has **no** `ke_shadow_insert` and
   **no** `bridge_KeInitializeEvent` at all.
4. **Both sides wrap the same ordinal set.** Local: `case 138: bridge_KeResetEvent`, `145: KeSetEvent`,
   `159: KeWaitForSingleObject`. Upstream: `145`, `159` in one dispatch table and `138` in another.

## Hunk 1 — `kernel_bridge.c` lines 1365–1404 (UNDECIDED)

Base (051a128), one line:
```c
    g_eax = (uint32_t)xbox_KeSetEvent(XBOX_TO_NATIVE(event_ptr), increment, (BOOLEAN)wait);
```
LOCAL (`0d7929c`) replaces it with the in-place-aware body **plus a whole new
`bridge_KeResetEvent` definition**:
```c
    recomp_diag_record(10, event_ptr, g_xbox_kernel_caller, 0);
    if (guest_va_is_inplace_kevent(event_ptr)) {
        g_eax = (uint32_t)xbox_KeSetInplaceEvent(
            event_ptr, XBOX_TO_NATIVE(event_ptr), BRIDGE_MEM8(event_ptr),
            increment, (BOOLEAN)wait);
    } else {
        g_eax = (uint32_t)xbox_KeSetEvent(
            XBOX_TO_NATIVE(event_ptr), increment, (BOOLEAN)wait);
    }
    recomp_diag_record(10, event_ptr, g_xbox_kernel_caller, g_eax);
}

static void bridge_KeResetEvent(void)
{
    uint32_t event_ptr = STACK_ARG(0);
    recomp_diag_record(11, event_ptr, g_xbox_kernel_caller, 0);
    if (guest_va_is_inplace_kevent(event_ptr)) {
        g_eax = (uint32_t)xbox_KeResetInplaceEvent(
            event_ptr, XBOX_TO_NATIVE(event_ptr), BRIDGE_MEM8(event_ptr));
    } else {
        g_eax = (uint32_t)xbox_KeResetEvent(XBOX_TO_NATIVE(event_ptr));
    }
    recomp_diag_record(11, event_ptr, g_xbox_kernel_caller, g_eax);
```
UPSTREAM (`766ecef`) replaces the same base line with the shadow-lookup form:
```c
    (void)increment;
    (void)wait;

    h = ke_shadow_lookup(guest_va);
    if (!h)
        h = bridge_resolve_handle(guest_va);
    if (!h)
        h = XBOX_TO_NATIVE(guest_va);
    if (h)
        g_eax = (uint32_t)SetEvent(h);
    else
        g_eax = 0;
```
Note: upstream's text uses `guest_va` and `h`, which do not exist on local's side (upstream declares
`uint32_t guest_va = STACK_ARG(0); HANDLE h;` in its own version of the function header).

## Hunk 2 — `kernel_bridge.c` lines 1425–1445 (UNDECIDED)

Base, one line:
```c
    g_eax = (uint32_t)xbox_KeWaitForSingleObject(
        XBOX_TO_NATIVE(object), wait_reason, wait_mode,
        (BOOLEAN)alertable, XBOX_TO_NATIVE(timeout_ptr));
```
LOCAL:
```c
    if (guest_va_is_inplace_kevent(object)) {
        g_eax = (uint32_t)xbox_KeWaitInplaceEvent(
            object, XBOX_TO_NATIVE(object), BRIDGE_MEM8(object),
            (BOOLEAN)alertable, XBOX_TO_NATIVE(timeout_ptr));
    } else {
        g_eax = (uint32_t)xbox_KeWaitForSingleObject(
            XBOX_TO_NATIVE(object), wait_reason, wait_mode,
            (BOOLEAN)alertable, XBOX_TO_NATIVE(timeout_ptr));
    }

    recomp_diag_record(9, diag_object, g_xbox_kernel_caller, g_eax);
```
UPSTREAM:
```c
    g_eax = (uint32_t)xbox_KeWaitForSingleObject(
        h, wait_reason, wait_mode,
        (BOOLEAN)alertable, XBOX_TO_NATIVE(timeout_ptr));
```
Local's function header declares `uint32_t diag_object = STACK_ARG(0);` and
`recomp_diag_record(8, diag_object, …)` before `uint32_t object = STACK_ARG(0);`; upstream declares
`HANDLE h;` and resolves it before the call.

## Hunk 5 — `lifter.py` lines 400–412 (UNDECIDED) + the H1 rule defect

`_FLAGS_UNDEFINED`, base has one line `"lock xadd",  # Lock prefix - complex flag behavior`.
LOCAL **deletes** it (local's `_FLAGS_UNDEFINED` is just `mul/div/idiv/rdtsc/cpuid`).
UPSTREAM **keeps** it and appends a comment block plus `"popfd",`.

Local handles `lock xadd` instead in `_EFLAGS_SETTERS` (line 265: `"xadd", "lock xadd", "lock
cmpxchg"`) and in `translator.py:832` as a result-snapshot setter. Local provenance for the deletion:
commit `92d7458` ("kernel+lifter: the guest's own thread safety, which was doing nothing").
Precedence in both revisions is the same: `_FLAGS_UNDEFINED` is tested **before** `_EFLAGS_SETTERS`
(`lifter.py:3300` vs `:3304` local; `:3612` vs `:3616` upstream), so the two classifications are
mutually exclusive for one mnemonic.

**The H1 defect.** Packet step 3's H1 reads: *"Every non-blank line one side added in the hunk is also
present in the other side's version of the hunk → take the containing side."* When one side **added no
lines** (it only deleted), the antecedent is **vacuously true**, so H1 "succeeds" and returns the
*other* side — silently discarding the deletion. That is what happened here: local added nothing, so
literal H1 would take upstream and resurrect `"lock xadd"` into `_FLAGS_UNDEFINED` while local's
`_EFLAGS_SETTERS` entry still lists it, changing which branch the lifter takes.

## Hunk 8 — `test_icall_feedback.py` lines 116–124 (UNDECIDED)

`test_seeds_drops_unaligned_targets`, inside `with tempfile.TemporaryDirectory() as tmp:`.
Preceding unconflicted lines (identical in both sides) create a real empty file and explain why:
```python
        fns = os.path.join(tmp, "functions.json")
        with open(fns, "w") as f:
            json.dump([], f)
```
LOCAL passes that file: `assert main(["--db", db, "--functions", fns,`
UPSTREAM passes a path that **does not exist**:
`assert main(["--db", db, "--functions", os.path.join(tmp, "none.json"),`
then the shared tail `"seeds", "--out", out, "--align", "16"]) == 0`.

Session-measured semantics: `load_function_bodies(path)` returns `None` when
`not os.path.exists(path)` (`icall_feedback.py:135-136`) and an empty list when the file exists but
holds `[]` (it appends only entries with `size > 0`). The consumer is
`if bodies:` (`icall_feedback.py:276`), and **both `None` and `[]` are falsy**, so the interior check is
skipped either way and the test's asserted outcome is identical. `icall_feedback.py` is byte-identical
between `0d7929c` and `766ecef` (empty diff). The next test in the same file, unconflicted, uses
`none.json` (`75083476:131`).

## Hunk 9 — `translator.py` lines 987–994 (UNDECIDED)

Base line: `                                 "lock cmpxchg")`
LOCAL: `                                 "lock cmpxchg", "xadd", "lock xadd")`
UPSTREAM: `                                 "lock cmpxchg", "inc", "dec")` **plus** a new clause line
`               or insn.mnemonic in _RESULT_SNAPSHOT_SETTERS`.
Shared tail (both sides): `               for insn in instructions):`.

Session-measured: `_RESULT_SNAPSHOT_SETTERS` is **introduced by upstream** and is **absent from local**
entirely; it is defined at `766ecef:lifter.py:355` as `{and, or, xor, adc, sbb, neg, shl, sal, shr,
sar, shld, shrd, add, sub}` — it does **not** contain `xadd`, `lock xadd`, `inc` or `dec`. The merge
tree already imports it cleanly (`75083476:translator.py:26`) and uses it at `:991`, and the merge tree
already defines it (`75083476:lifter.py:355`). The enclosing expression is the flag-snapshot
declaration guard (`lines 977-996`).

## Binding rulings that constrain the outcome (Session-verified present on disk)

- `docs/reviews/a4s-ac97-hunk-ruling.md` — hunk 4 resolves to LOCAL; upstream's NABM trap enters
  **unarmed with zero call sites**; "Interpretation ruling 2: scope" fixes `SCOPE` = toolkit `src/` +
  `include/`, deleted names match only as quoted literals, comments/docs/tests/unbuilt scaffolds are
  inventoried and never failed.
- `docs/jsrf-run-profiles.md` §"Upstream merges never silently change admitted evidence semantics"
  (rules 1–5) — the general policy the inventory implements.
- `docs/agent-workflow.md` §2.4 (evidence invariants), §3.1 (blocking test), §5.4, §6.1.

## Explicitly out of scope for this brief

Hunks 3 (`kernel_bridge.c:8981-8988`, H1), 6 and 7 (`lifter.py`, H2) already have mechanical
classifications and are **not** reopened. Hunk 4 is ruled (`HA`).
