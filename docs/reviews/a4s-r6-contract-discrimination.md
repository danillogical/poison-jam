# A4s-r6: the accepted contract test discriminates the hunk-1/hunk-2 semantics

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** measured evidence for the Advisor's hunks 1–2 ruling. Not a ruling.
**Scripts:** `logs/a4s/contract-discriminates.py`, `logs/a4s/contract-chain.py`.

## The chain, every link measured on the merge tree

| # | Link | Measured result |
|---|---|---|
| 1 | `tests/kernel_inplace_event_test.c` is a **local-only** addition | changed locally **True**, changed upstream **False** → the merge keeps local's bytes |
| 2 | The **game** repo registers it as a ctest | `CMakeLists.txt:118-125`: `add_executable(jsrf_inplace_event_test ${XBOXRECCOMP_DIR}/tests/kernel_inplace_event_test.c)` … `add_test(NAME jsrf_inplace_event_bridge …)`. It is one of the 12 tests that **passed 12/12** in `A4s-r5` step 2 |
| 3 | Its helpers survive the merge | merge tree `kernel_bridge.c`: `xbox_test_bridge_KeSetEvent` at 9375, `xbox_test_bridge_KeResetEvent` at 9383, `xbox_test_bridge_KeWaitForSingleObject` at 9391; declarations in `kernel.h` at 779 |
| 4 | Those helpers call the bridge functions the hunks decide | `xbox_test_bridge_KeSetEvent → bridge_KeSetEvent();`, `xbox_test_bridge_KeResetEvent → bridge_KeResetEvent();` |
| 5 | Upstream has **none** of these helpers | `xbox_test_bridge_KeSetEvent`, `…KeResetEvent`, `…KeWaitForSingleObject`, `xbox_inplace_event_test_reset` are all **ABSENT** from `766ecef`'s `src` |

## Why this discriminates

The contract test asserts **guest-memory** `SignalState` transitions, which only an in-place
implementation performs. The discriminating assertions (local test file line numbers):

```
 78: check(*signal_state(EVENT_VA) == 1, "notification wait leaves SignalState");
 81: check(*signal_state(EVENT_VA) == 0, "NtClearEvent stores SignalState 0");
 86: check(*signal_state(EVENT_VA) == 1, "KeSetEvent stores SignalState 1");
 91: check(xbox_test_bridge_KeResetEvent(EVENT_VA) == 1, "KeResetEvent previous 1");
 92: check(*signal_state(EVENT_VA) == 0, "KeResetEvent stores 0");
 95: check(*signal_state(EVENT_VA) == 1, "NtSetEvent stores SignalState 1");
236: check(*signal_state(EVENT_VA) == 0, "pulse leaves SignalState 0");
```

`signal_state(va)` is `(volatile LONG *)(native(va) + 4)` — the guest `KEVENT.SignalState` field.

**The two sides differ on exactly this:**

| Implementation | writes guest memory | `KeResetEvent` return |
|---|---|---|
| LOCAL `bridge_KeSetEvent` / `bridge_KeResetEvent` | **True** — routes to `xbox_KeSetInplaceEvent` / `xbox_KeResetInplaceEvent`, which store `SignalState` | previous state (`KeResetEvent previous 1`) |
| UPSTREAM `bridge_KeSetEvent` / `bridge_KeResetEvent` | **False** — `SetEvent(h)` / `ResetEvent(h)` on a host handle, no `BRIDGE_MEM32` write | `g_eax = 0` |

So resolving hunks 1–2 to upstream would leave the accepted contract test asserting a guest
`SignalState` that nothing writes: line 86 expects `1` after `KeSetEvent` and line 92 expects `0` after
`KeResetEvent`, while upstream's bodies write no guest memory at all. **That test is part of the
game's ctest suite that `A4s-r5` treats as a pass/fail gate (`AC-TEST` (i), 12/12 at step 2).**

## What this does and does not establish

**Established (observed):** the semantics are **not** interchangeable textually. Local's in-place
model is the one the accepted, currently-passing contract test exercises; upstream's shadow model is a
different object model that writes no guest `SignalState`.

### The mapping chain, now closed from source

The earlier version of this record withheld the claim that an upstream resolution would *fail* the
test, because it was unclear whether `signal_state(EVENT_VA)` observes the memory
`xbox_KeSetInplaceEvent` writes. **That link is now closed by reading source, with no execution
needed:**

| Step | Source | Value |
|---|---|---|
| test's own buffer mapping | `kernel_inplace_event_test.c:37` | `native(va) = ram + (va - CANONICAL_LO)` |
| test's offset setup | `:503` | `g_xbox_mem_offset = (ptrdiff_t)ram - (ptrdiff_t)CANONICAL_LO` |
| what the test reads | `:40` | `signal_state(va) = native(va) + 4` |
| runtime's guest→host map | `kernel_bridge.c:66` | `XBOX_TO_NATIVE(va) = va + g_xbox_mem_offset` |
| where the model stores state | `kernel_sync.c:122,251` | `jsrf_inplace_state(Header) = Header + 4` (`JSRF_INPLACE_HEADER_SIGNAL`) |
| what local passes as `Header` | `kernel_bridge.c:1348-1350` | `XBOX_TO_NATIVE(event_ptr)` |

Substituting the test's own offset:

```
XBOX_TO_NATIVE(EVENT_VA) + 4 = EVENT_VA + (ram - CANONICAL_LO) + 4
                             = ram + (EVENT_VA - CANONICAL_LO) + 4
                             = native(EVENT_VA) + 4
                             = signal_state(EVENT_VA)
```

**They are the same address.** Local's model writes exactly the location the test reads.

Verified numerically for four different plausible host bases (`logs/a4s/address-identity.py`) — the
identity holds for every one, and algebraically it is forced:
`EVENT_VA + (ram − CANONICAL_LO) + 4 == ram + (EVENT_VA − CANONICAL_LO) + 4 == native(EVENT_VA) + 4`.

**All 15 helpers the test calls survive the merge** (`logs/a4s/helper-survival.py`): the eight
`xbox_inplace_event_test_*` (defined in `kernel_sync.c`) and the seven `xbox_test_bridge_*` (defined in
`kernel_bridge.c`) each have **both a definition and a `kernel.h` declaration** in the merge tree — no
missing definitions, no missing declarations. So the argument does not rest on a helper that the merge
might have dropped.

### Therefore the first discriminating assertion fails under an upstream resolution

The test's opening sequence (lines 74–92) is:

```
74: init_header(EVENT_VA, XboxNotificationEvent, 1);   // sets SignalState = 1
76: check(KeWaitForSingleObject(...) == STATUS_SUCCESS, "pre-signaled notification wait completes");
78: check(*signal_state(EVENT_VA) == 1, "notification wait leaves SignalState");
79: check(NtClearEvent(EVENT_VA) == STATUS_SUCCESS, "NtClearEvent in-place succeeds");
81: check(*signal_state(EVENT_VA) == 0, "NtClearEvent stores SignalState 0");
84: check(KeSetEvent(EVENT_VA, 1, FALSE) == 0, "KeSetEvent previous was 0");
86: check(*signal_state(EVENT_VA) == 1, "KeSetEvent stores SignalState 1");
91: check(KeResetEvent(EVENT_VA) == 1, "KeResetEvent previous 1");
92: check(*signal_state(EVENT_VA) == 0, "KeResetEvent stores 0");
```

Upstream's bodies for these three functions contain **no `BRIDGE_MEM` write at all** (measured:
`bridge_KeSetEvent` → `NONE`; and `bridge_NtClearEvent` is
`HANDLE handle = bridge_resolve_handle(STACK_ARG(0)); g_eax = xbox_NtClearEvent(handle);` — no guest
write). `SetEvent`/`ResetEvent` signal a *host event object*; they do not store into a caller buffer.
So under an upstream resolution the guest `SignalState` retains the value `init_header` wrote, and:

- **line 81 expects `0`, observes `1` → FAIL** (the earliest discriminating assertion)
- line 86 expects `1` after a set that writes nothing → depends on the same missing write
- line 91 expects `1` from `KeResetEvent`, while upstream's returns `g_eax = 0` → FAIL

**So the discrimination is now derivable from source**, not merely predicted. What remains a
prediction is only the *mechanical* outcome — that a build of such a tree links and the test then
reports these failures — because I have not compiled a merged tree (`A4s-r5` selected `R-CONFLICT`
before any build). The **semantic** conclusion — that upstream's form cannot satisfy this accepted
test — follows from the address identity above and needs no build.

**Cheapest confirmation, for the Planner to place in a packet:** resolve hunks 1–2 to upstream, keep
exactly one `bridge_KeResetEvent`, build, and run `ctest -R jsrf_inplace_event_bridge`. Expected: fail
at line 81 (`NtClearEvent stores SignalState 0`). That converts the prediction into a measurement.

## Bearing on the Advisor's ruling

This is evidence that the hunk-1/hunk-2 decision is **semantic, not textual**, and that "which side
wins" must be answered by asking *which event model the accepted evidence requires* — not by
preferring local or upstream. It also means a **combined form** is a real candidate: local's in-place
dispatch with upstream's `ke_shadow_lookup` fallback would need an explicit statement of precedence,
and the duplicate `bridge_KeResetEvent` must be reduced to exactly one definition either way
(see `docs/reviews/a4s-r6-structural-findings.md`, Finding 2).
