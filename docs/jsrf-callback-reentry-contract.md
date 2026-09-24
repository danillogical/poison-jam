# JSRF callback / signal / reentry contract

**Scope:** technical ABI/signal/reentry contract and historical measurement;
not a current packet/status authority. Current acceptance and execution state live in
`plan-jsrf-bare-minimum.md` and review records. Instruction evidence was produced with
`python -X utf8 scripts/inspect-jsrf.py disasm` on the original XBE. The contract records
the recovered production bodies for `0x00193D90`, `0x00194210`, `0x00197AAC`, and
`0x00196C4A` without making a current acceptance claim.

Source ranges (end exclusive): `0x00193D90..0x00193EE2`,
`0x00194210..0x001942F8`, `0x00197AAC..0x00197BCF`,
`0x00196C4A..0x00196C83`. Related already-accepted helpers: timestamp
`0x00193C40`, polling `0x00196C0B`, queue `0x00193D10`. Initializer of the
event object: `0x00194AFA..0x00194B43`.

`0x00194ADD`, `0x00194780`, and `0x00197C50` are now recovered from
original XBE. Keep `0x00193F70`, `0x0018E120`, and `0x001918E0` fatal.

## Facts

### `0x00193D90` ABI

- Thiscall: `ECX` = GPU context. Device pointer is `MEM32(context+0)`.
- No stack arguments. `RET` pops only the return address. EAX is forced to 0
  on both the callback and no-callback exits.
- Callee-saved EBX/ESI/EDI/EBP are pushed and popped. No SEH frame. No IRQL
  import. Locals are `sub esp, 0x14`.
- Calls accepted `0x00193C40` first and uses EAX as the 32-bit timestamp.

### Timestamp and queue

- `context+0x20C` holds the previous timestamp. When it is nonzero,
  `context+0x208 = timestamp - previous`. Then `context+0x20C = timestamp`.
- `context+0x1F4` is incremented. Let `delta = new_1F4 - context+0x1F8`.
  `above_threshold` is `delta >= context+0x1FC` (unsigned `cmp` / `sbb` /
  `inc`).
- Slot is `context+0x1F0 & 1`. When `above_threshold` and
  `context+0x1B0+slot*8` is nonzero, call accepted `0x00193D10` with
  `ECX=context`, `[esp+4]=context+0x1B4+slot*8`, `[esp+8]=timestamp`
  (`RET 8`), then store 0 at that `+0x1B0` slot. The queued flag is 1.
- When the queue path is skipped, the queued flag is 0.

### PCRTC spin (the wait on `device+0x100 & 0x01000000`)

- One byte is saved from `device+0x6013D4` before the spin and written back
  after the callback or no-callback exit. A spin that never leaves does not
  restore it.
- The spin at `0x00193E40` stores `1` to `device+0x600100`
  (`NV_PCRTC_INTR_0`, W1C vblank) and continues while
  `device+0x100 & 0x01000000` is set. That mask is `NV_PMC_INTR_0_PCRTC`
  (`1 << 24`).
- If the PMC bit is already clear, the body still writes `1` once to
  `0x600100` and falls through. If the bit never clears, `KeSetEvent` and the
  guest callback never run. Production has no deadline.

### Real signal: `KeSetEvent`

- After the PMC bit is clear: `push 0; push 1; lea eax, [context+0x1C8];
  push eax; call dword ptr [0x001C4014]`. No `add esp`.
- `0x001C4014` is kernel import ordinal 145 `KeSetEvent`. stdcall, three
  arguments: Event=`context+0x1C8`, Increment=`1`, Wait=`FALSE`.
- `0x00194AFA` initializes that object in place: Type byte `0`
  (NotificationEvent), Size `4`, SignalState `1`, wait-list self-linked at
  `context+0x1D0`. A second dispatcher at `+0x1D8` is initialized the same
  way and is not the `KeSetEvent` target.
- NotificationEvent stays signaled until reset. A wait without reset on an
  already-signaled object completes immediately. Reset-then-wait blocks until
  a later `KeSetEvent`. Reset after `KeSetEvent` and before wait misses that
  signal.

### Indirect callback

- `EAX = MEM32(context+0x1C4)`. If zero, restore `0x6013D4` and return 0.
- Otherwise cdecl `call EAX` with one argument: pointer to a 12-byte stack
  struct `{ context+0x1F4, context+0x1F0, reason }`. Caller `add esp, 4`.
- `reason` is `1` when the queue helper ran, `2` when it did not and
  `above_threshold` and `context+0x1FC` are nonzero, else `0`.
- The struct lives in `0x00193D90`'s frame and is invalid after return.

### `0x00194210` ABI and context-switch path

- Thiscall: `ECX` = context. No stack arguments. Plain `RET`.
- Stores `0` to `device+0x400720` (`NV_PGRAPH_FIFO`) on entry and `1` on the
  normal exit at `0x001942E3`.
- `test ah, 0x10` is `NV_PGRAPH_INTR_CONTEXT_SWITCH` (`1 << 12` = `0x1000`),
  the same bit `0x00196C0B` uses to dispatch this helper.
- When that bit is set: write `0x1000` to `device+0x400100` (W1C),
  `and trapped_addr, 0x1FFC` then `shr 0x14` / `and 0x1F` (channel is 0
  because the method mask already dropped CHID bits), spin while
  `device+0x400700` is nonzero, then `push channel; call 0x00197AAC`
  (`RET 4`).
- If `0x400700` never clears, `0x00197AAC` is not called and FIFO stays 0.
- Remaining NSOURCE / method-`0x100` path calls unrecovered `0x00193F70`.
  The other error combination prints through `0x0014B794` and executes
  `INT3`. Neither is a success path.

### `0x00197AAC` ABI and reentry

- Thiscall plus one stdcall argument: `ECX` = context, `[ebp+8]` = channel,
  `RET 4`. Uses an EBP frame (`push ebp` / `leave`). No SEH.
- If `device+0x400100` is nonzero, calls `0x00194210` again on the same
  context. After a successful W1C of only `0x1000`, that nested call must not
  take the context-switch path again.
- Saves FIFO, writes FIFO `0`, calls accepted `0x00196C0B`. If `0x400700` is
  already 0, that polling loop is idle and does not dispatch `0x00194210` or
  `0x00193D90`.
- If `context+0x130 != channel`, calls recovered `0x00196C4A(old)` (`RET 4`).
- Stores `channel` at `context+0x130`. Channel `2` programs a short FIFO
  restore (`saved | 1`) and returns. Other channels program CTX/RDI/CHANNEL
  registers, call `0x00196C0B` a second time, then return.

## Hypotheses

- Exact electrical meaning of the saved `device+0x6013D4` byte beyond "save
  and restore".
- Whether hardware W1C of `NV_PCRTC_INTR_0` is what clears
  `NV_PMC_INTR_0_PCRTC` on real NV2A; the original software contract is the
  spin, not a model of the interrupt controller.
- Whether live ISR waiters on `context+0x1C8` during ordinary boot match the
  bridge-ABI fixture. That fixture now drives `KeSetEvent`, `KeResetEvent`,
  `NtClearEvent`, `NtSetEvent`, and `KeWaitForSingleObject` through the real
  kernel bridges.

## Kernel in-place event contract

A 16-byte `DISPATCHER_HEADER` in canonical RAM `[0x00010000, 0x04000000)` is
an in-place event when Type is 0 or 1, Size is 4, the whole header is inside
that range, and the wait list is self-linked at `va+8` or both Flink/Blink
are in-range guest pointers. `KeSetEvent`, `KeResetEvent`,
`KeWaitForSingleObject`, `NtWaitForSingleObject`, `NtSetEvent`, `NtClearEvent`,
and `NtPulseEvent` share one critical section for SignalState and the mapped
Win32 event. Acquire resynchronizes the host event to the guest SignalState when no
waiters are present, so a same-VA reinit is visible to the next wait.
Synchronization events keep per-waiter credits (`active`, `in_host_wait`,
`paid`) and a private auto-reset host event. `KeSetEvent` pays one
`in_host_wait` waiter by signaling that waiter's handle, not a shared
auto-reset object. Type-1 pulse pays exactly one such waiter. Type-0 pulse
pays every `in_host_wait` waiter. Timeout refunds a paid credit into
`SignalState`. Reset clears unconsumed state and unpaid waiter handles; it
does not clear `paid`. Test gates can freeze registration, host-wait entry,
host-wait return, and lock reacquire. Registry capacity 128 is
fatal and diagnostic on exhaustion; `abort` drops the registry lock first.
Reuse of a guest VA with a new Type recreates the host event. `KeSetEvent`
Wait=TRUE is unused by `0x00193D90` and is still ignored. HANDLE-typed
`NtCreateEvent` callers stay on the Win32 handle path.

## Acceptance

Bounded fixture `tests/test_callback_reentry.c` proves the original-instruction
cases. `jsrf_inplace_event_bridge` proves the kernel bridges with forced
set/wait/clear interleavings. The generic Win32 event in
`tests/test_service_chain.c` remains outside `0x00193D90` evidence. Keep
`0x00193F70`, `0x0018E120`, and `0x001918E0` fatal.
