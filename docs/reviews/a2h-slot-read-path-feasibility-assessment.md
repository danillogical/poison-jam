# `A2h-slot-read-path-displacement-r1` — Session implementation-feasibility assessment

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-slot-read-path-displacement.md`, revision
`A2h-slot-read-path-displacement-r1`, frozen
**`75E6AE7C9A26A2CF63F9AD19502A170B2960FE2C90523A566F3F382942BB9B6A`** (56 lines, 30856 bytes).
**Purpose:** discharge the packet's own STOP condition — *"Any expansion outside declared scope, **no feasible
native arming**, or contradiction in handler order → STOP/re-refer."*

**Verdict: FEASIBLE. The stop condition is NOT triggered.** And the implementation is **simpler than the
packet's seam text implies**, in a way that removes a whole class of failure modes.

---

## The architectural tension the packet must resolve

| Half | Has | Lacks |
|---|---|---|
| **Toolkit** (in-process) | the mapping addresses (`g_mirror_views`, `xbox_GetMemoryBase/MappedSize`) | ability to `SetThreadContext` on other threads |
| **Collector** (`jsrf_collect.exe`, the debugger) | debug handles; already does `GetThreadContext`; routes first-chance exceptions | **does not link the toolkit** — `add_executable(jsrf_collect tools/harness/collect.c src/jsrf_save_root.c)` + `target_link_libraries(... dbghelp shell32)` |

**DR0 must be programmed per-thread, which only the debugger can do** — so the two halves must
communicate. The packet's answer is a gated first-chance handshake, which is the right mechanism: the
toolkit raises, the collector sees it via `WaitForDebugEvent` **before** the game's own VEH, arms, and
continues.

## The simplification — the collector already has what the derivation needs

**The packet's seam text implies the alias host addresses may need to travel through the handshake. They do
not.** The collector already resolves both inputs:

```
tools/harness/collect.c:134   DWORD64 addr = symbol_address("g_xbox_mem_offset");
tools/harness/collect.c:136   guest_offset = offset;
tools/harness/collect.c:139   ram_base = offset + 0x10000;
tools/harness/collect.c:102   out->MemorySize = ram_size;
```

**And that is sufficient**, because each mirror view maps the same file region from its own base, so guest
VA `S` sits at `view_base + S` in every view:

```
canonical host = guest_offset + SLOT
alias m host   = guest_offset + (m+1) * ram_size + SLOT
```

with `SLOT = 0x001C4064` a compile-time constant. **With Run 2's measured geometry** (`guest_offset =
0x10000`, `ram_size = 64 MB`): canonical `0x00000000001D4064`, alias 1 `0x00000000041D4064`, …, alias 28
`0x00000000701D4064` — **29 addresses, all computable in the collector with no cross-process data flow.**

**Why this matters:** it removes truncation, ordering and staleness failure modes from a 29-entry handshake
payload, and it means the handshake's job shrinks to **signal → arm → acknowledge.**

## What each side must actually add — capabilities, not knowledge

**Collector (two capability additions, both small and local):**

1. **`SetThreadContext` with `CONTEXT_DEBUG_REGISTERS`.** It currently opens threads with
   `THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION` (`collect.c:152`) and only **gets** context
   (`collect.c:157`). Arming needs `THREAD_SET_CONTEXT` in the `OpenThread` mask plus a `SetThreadContext`
   call — a flag and a call.
2. **A `CREATE_THREAD_DEBUG_EVENT` branch.** It handles `CREATE_PROCESS_DEBUG_EVENT` (`collect.c:338`) but
   **not** `CREATE_THREAD`, which is exactly where the packet requires a new thread to be armed **while
   stopped**, before `ContinueDebugEvent` permits its first instruction.

**Toolkit:**

1. The gated **first-chance handshake** at the install callback, so the collector can arm before the install
   store executes.
2. The **alias-census VEH** with **record-before-open** on the 28 alias pages.
3. **Nothing about addresses needs to cross the boundary.**

## Handler-order check — no contradiction found

The packet's third stop condition is *"contradiction in handler order."* **None was found:**

- the toolkit raises **first-chance**, so the collector sees it **before** the game's own VEH;
- the collector continues only **after** arming, so no guest instruction runs unarmed;
- for a **new** thread, `CREATE_THREAD` fires while the thread is stopped, so arming there precedes its first
  instruction by construction.

**That is the same first-chance channel the collector already routes** (`collect.c:319-347`), so no new
dispatch mechanism is needed.

## Two requirements the implementation must carry, derived here

1. **`THREAD_SET_CONTEXT` must be added to the existing `OpenThread` mask** — without it `SetThreadContext`
   fails on every thread and the instrument silently arms nothing. **That failure must be checked and must
   fail closed**, not be allowed to produce an "unarmed but silent" run.
2. **The `CREATE_THREAD` branch must arm before `ContinueDebugEvent`.** Arming after the continue would
   leave a window in which the new thread could execute, and the packet's own rule is that **any unarmed
   observed tid is `UNKNOWN`** — so a late arm must be *detected*, which means the census needs the
   arm-time/terminal-time tid sets the packet already requires.

## Scope note

**Nothing in this assessment expands the packet's declared write scope.** Both collector additions are inside
`tools/harness/collect.c`, which the packet names; both toolkit additions are inside
`kernel_bridge.c`/`xbox_memory_layout.c`, which it names. **No generated-code edit, no synthetic completion,
no guest-semantics change.**
