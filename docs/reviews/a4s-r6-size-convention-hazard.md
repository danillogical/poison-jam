# A4s-r6: cross-model hazard — the two event models disagree on the DISPATCHER_HEADER `Size` field

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** measured evidence for the Advisor's hunks 1–2 ruling. Not a ruling.
**Scripts:** `logs/a4s/size-convention.py`, `logs/a4s/mutual-recognition.py`, `logs/a4s/coverage.py`.

## The two models

| | LOCAL (in-place) | UPSTREAM (shadow table) |
|---|---|---|
| Object identity | **sniffed from the guest struct** — no registry | **registered** in `g_ke_shadow[]` keyed by guest VA |
| Creation path present | `xbox_KeSetInplaceEvent` etc. lazily create the host event on first use | `bridge_KeInitializeEvent` / `NtCreateEvent` call `CreateEventW` then `ke_shadow_insert(guest_va, h)` |
| `bridge_KeInitializeEvent` | **ABSENT locally** | present, dispatched at **ordinal 108** |
| Ordinal 108 routed? | **NO** | **YES** (`case 108: return bridge_KeInitializeEvent;`) |

## The measured disagreement

Local's dispatcher guard `guest_va_is_inplace_kevent` (`kernel_bridge.c:1323-1339`) **reads** the
guest `DISPATCHER_HEADER.Size` byte and requires it to be exactly `4`:

```c
type = BRIDGE_MEM8(va);
size = BRIDGE_MEM8(va + 2u);
if (type > 1u || size != 4u)
    return 0;
```

Measured field traffic:

| Writer | `guest + 2` value |
|---|---|
| **LOCAL** | **never writes it** (no writes at all — it only sniffs what the guest wrote) |
| **UPSTREAM** `bridge_KeInitializeEvent` (`766ecef:6442`) | **`16`** |
| UPSTREAM `bridge_KeInitializeTimer` (`:2190`) | `40` |
| UPSTREAM APC / queue objects (`:6642`, `:6661`, `:6679`) | `0` |
| **Accepted contract test** `init_header` (`kernel_inplace_event_test.c:51`) | **`4`** |

**The merge tree contains both sides of this disagreement**: local's sniffer at `75083476:1347`
(`size = BRIDGE_MEM8(va + 2u);`) and upstream's `BRIDGE_MEM8(guest_va + 2) = 16;` at `75083476:6603`,
`:6633`, `:6661`.

## Correction: which creation paths are reachable, and is the timer a hazard?

An earlier version of this record said the shadow table's populators were all unreachable. **That was
wrong**, and the corrected reachability (`docs/reviews/a4s-r6-event-ordinals.md`,
`logs/a4s/shadow-populators.py`) is:

| `ke_shadow_insert` caller | Ordinal | Declared by JSRF? |
|---|---|---|
| `bridge_KeInitializeTimerEx` | **113** | **YES — reachable** |
| `bridge_KeInitializeEvent` | 108 | no |
| `bridge_KeInitializeMutant` | 110 | no |
| `bridge_KeInitializeSemaphore` | 112 | no |

So upstream's shadow table **does** have a reachable populator. Does that make the hazard live?
**No — the timer is correctly rejected, not mis-handled.** Measured type bytes written at guest `+0`
(`logs/a4s/timer-hazard-assess.py`):

| Path | type byte at `+0` | Size at `+2` | Local sniffer verdict |
|---|---|---|---|
| `bridge_KeInitializeTimerEx` (113, reachable) | `0x08`/`0x09` | `40` | **rejected on type** — and that is **correct**: a timer is not a `KEVENT` |
| `bridge_KeInitializeEvent` (108, unreachable) | `0`/`1` | `16` | rejected on size |
| `bridge_KeInitializeMutant` (110, unreachable) | `19` | `16` | rejected on type — correct |
| `bridge_KeInitializeSemaphore` (112, unreachable) | `18` | `16` | rejected on type — correct |

The only path that writes a **genuine `KEVENT` type** (`0`/`1`) is `bridge_KeInitializeEvent`, whose
ordinal **108 is not imported**. So the type-confusion scenario below requires an unreachable path.

**The hazard is therefore LATENT, not live.** It is still worth recording — the merged code is
internally inconsistent about the `Size` convention, and a dynamically-computed call or a future title
could reach it — but it must **not** be the basis for a hunks 1–2 side choice, and it does not by itself
justify a combined form.

## The hazard, stated as a concrete scenario

`bridge_KeInitializeEvent` arrives from upstream in a **clean hunk** (local has no such function), so
it is in the merged tree **whatever hunks 1–2 decide**. Consider a guest call to ordinal 108 followed
by a `KeSetEvent` on the same VA, under a **LOCAL-only** resolution of hunks 1–2:

1. `bridge_KeInitializeEvent` creates a host event, inserts it in the shadow table, and writes
   `Size = 16` into the guest struct.
2. `bridge_KeSetEvent` (local's form) calls `guest_va_is_inplace_kevent(va)`.
3. The sniffer reads `Size == 16`, which is `!= 4`, so it returns **0**.
4. Control falls to local's `else` branch:
   `g_eax = (uint32_t)xbox_KeSetEvent(XBOX_TO_NATIVE(event_ptr), increment, (BOOLEAN)wait);`
5. `xbox_KeSetEvent` (`kernel_sync.c:563`) is
   `HANDLE hEvent = (HANDLE)Event; SetEvent(hEvent); return 0;`
   — it treats its argument as a **host HANDLE**, but step 4 passed a **guest pointer**
   (`XBOX_TO_NATIVE(va)`). The shadow-table handle created in step 1 is never consulted.

**That is a type confusion**: a guest VA is handed to `SetEvent` as a handle. Local's own comment in
`xbox_KeSetEvent` anticipates the split — *"HANDLE-typed callers remain on this path. In-place
DISPATCHER_HEADER objects are routed by the kernel bridge before this function"* — but under this
resolution the routing test **fails** for upstream-created events, so they reach the handle path.

## What is observed and what is inferred

**Observed (read directly):** the sniffer's `size != 4u` requirement; local's total absence of `+2`
writes; upstream's `= 16` writes and their merge-tree presence; upstream's ordinal-108 routing and its
absence locally; `xbox_KeSetEvent`'s handle-typed body; the contract test's `p[2] = 4`.

**Inferred, and labelled as such:** that a `Size = 16` event therefore takes local's `else` branch and
reaches `SetEvent` with a non-handle. The branch logic is mechanical, but I have not executed it.

**NOT established:** whether the guest **ever** calls ordinal 108 in JSRF — measured separately, and
it **does not**: ordinal 108 is not in the XBE's declared import table
(`docs/reviews/a4s-r6-ordinal-reachability.md`). So this scenario is unreachable for this title.

**Also NOT established:** which convention is *correct*. Upstream's `Size = 16` reads like a byte
count; local's required `4` reads like a **word** count (a `KEVENT` is 16 bytes = 4 DWORDs), which is
also what the accepted contract test writes. That is an **inference from the numbers**, not a citation
of Windows semantics — I have not verified it against a primary source, and
`docs/jsrf-run-profiles.md`'s evidence rule would require one before it could carry a packet.

**One genuine observation survives the correction.** `bridge_KeInitializeTimerEx` (ordinal 113,
reachable) writes `Size = 40` and `type = 8/9`. Local's sniffer rejects it — correctly, since it is not
a `KEVENT`. But upstream's shadow table *does* insert a handle for that timer VA. So under a **LOCAL**
resolution the timer is handled by local's timer path (if any) and the shadow entry is never consulted;
under an **UPSTREAM** resolution the timer is found in the shadow table. Whether that difference is
observable depends on how `KeSetTimer`/`KeCancelTimer` and the DPC path treat the object — which I have
**not** traced. Recorded as an open question for the Planner rather than resolved here.

## Why this matters for the ruling

It bears directly on the choice among LOCAL / UPSTREAM / combined:

- **LOCAL alone** does not obviously work, because upstream's clean-arriving `KeInitializeEvent`
  writes a `Size` local's sniffer rejects — so local's own dispatch guard does not recognise objects
  the merged tree can still create.
- **UPSTREAM alone** does not obviously work, because the accepted, currently-passing contract test
  requires guest `SignalState` writes that upstream's bodies do not perform
  (`docs/reviews/a4s-r6-contract-discrimination.md`).
- **A combined form** is therefore the live candidate, and it must state **which `Size` convention
  governs** and in what precedence the two lookups run — otherwise the merged tree can create an
  object that neither model's fast path recognises.

This is a **source-decidable** question (the Advisor can read all of it); the only execution-dependent
part is whether ordinal 108 is reachable from JSRF's guest code.
