# `A2h-live-slot-write-r1` — implementation record

**Packet:** `docs/packets/a2h-live-slot-write.md`, frozen `8F3C6291…F7D4F` (17 lines).
**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Status:** instrumentation implemented, building, fixtures green. **No live ON trial run — that is
the Session's, and no outcome row is claimed here.**

## What was built

| Location | Role |
|---|---|
| toolkit `src/kernel/xbox_memory_layout.c` | page-protection machinery, deferred ARM, all-thread census, the fixture seam |
| toolkit `src/kernel/xbox_memory_layout.h` | the ledger, the loss accounting, the arm/terminal API |
| game `src/main.c` | starts the watch where the device is known live; stops it before teardown |
| game `src/recomp_manual.c` | the fourth-read latch at the read site's own seam |
| game `tools/harness/collect.c` | archives the ledger by symbol; census reconciliation |
| toolkit `tests/a2h_slotw_fixture_test.c` | the required fixtures |

## Mechanism: page protection only, no DR anywhere

The slot's page is made `PAGE_READONLY` in the canonical view and every mapped mirror alias.
An RO page faults on WRITE and never on READ, so the poll loop's read volume is free and the
protection **is** the discrimination. **There is no `DR0`/`DR6`/`DR7` in this facility**, and the
collector's DR-gated single-step counters are not its evidence channel.

## ⚠ Findings that changed the design — each measured, not assumed

### 1. The device global is on the WATCHED PAGE, and at terminal it is not a pointer

`MEM32(0x0019DCE0)` is the ARM input. `0x0019DCE0` and the slot `0x0019D62C` are on the **same
page** `0x0019D000`. On an archived run the terminal read returned `0x30766A64` — ASCII `djv0`,
string data — **not a device pointer**.

So `term_base_ok` distinguishes *the base moved* (a device move, re-scope) from *the base stopped
being a pointer* (the global was overwritten by the same data traffic the watch exists to sort from
the slot). Both set `base_changed`; neither is read as a clean comparison.

### 2. The AC'97 handler would have swallowed the slot watch's `#DB`

Both handlers register at priority 1 and the AC'97 trap arms first, so it runs first. Its
`EXCEPTION_CONTINUE_EXECUTION` would have resumed the thread with the slot owner still pending —
its page left **open**, its post-value never read. It now releases the `#DB` when the other owner is
pending. **The synthetic overlap caught this; it is a real collision, not a theoretical one.**

### 3. Two counters recorded *which handler ran first* rather than *what was serviced*

The dual-owner and AC'97 counts were taken in the slot handler's "pass it on" branch, so under the
opposite order they read 0 for a step that had plainly been serviced. Both now sit where the work is
done, and the dual count in the shared release helper, where it is symmetric by construction.
**Both VEH orders now converge — asserted, not assumed.**

### 4. A claim this packet made was MEASURED FALSE

On this host a store through **mirror view 1 does not become visible in the canonical view**. The
watch reads the post-value through the *same alias the store used*, so the ledger is correct either
way — and the packet's refusal to rely on the alias for value readback turned out to be the right
call. The fixture **records the measurement** instead of asserting the aliasing.

### 5. The installer control's own definition was too narrow

It required `pre == 0` as well as the encoding. That is an assumption about the title: had anything
touched the slot first, the control would silently not count and the packet's fail-closed rule would
fire on a run where the control **had** been observed. The control is now the encoding plus the slot
hit; the value rides in the step record and is compared offline.

### 6. A guard that could not fail

The collector's `_Static_assert` pins its **own mirror**, so adding a field to the real struct left
it passing. The sufficient check is the runtime `size` comparison — the toolkit stamps
`sizeof(its own struct)` — plus a version equality check. Version 1 → 2.

## The encoding discriminator, proven against the real instructions

A native RIP is not a guest VA, so the control is classified from the **bytes at the recorded RIP**.
The fixture reads those bytes out of the loaded XBE at the packet's own addresses and runs the real
classifier:

| Site | Bytes | Class |
|---|---|---|
| `0x0018CE3A` | `89 81 2C 24 00 00` | **MODRM** — the installer, the required control |
| `0x00199F45` | `89 84 AE EC 03 00 00` | **SIB** — the candidate |

Distinct classes ⇒ the control and the target cannot be confused. A load or an unrelated store falls
to `OTHER`.

## Fixtures — 152 checks, both VEH orders

| Fixture | Result |
|---|---|
| read-without-AV | real load from a real RO page returns the planted value, **0 relevant AV** |
| RO-write AV delivered | real store → real AV → step → post-value `DEADBEEF` → re-protected; second store faults too |
| `ExceptionInformation[0]` filter | read-class / non-AV / foreign-page **rejected**; write-on-owned **admitted** |
| slot vs nearby page | nearby write = **traffic** (+1), slot hits unchanged; mirror-alias store caught and classified a hit |
| encoding classifier | installer=MODRM, candidate=SIB, **distinct**; loads and unrelated stores = OTHER |
| dual `#DB` ownership | masks `0x3`/`0x1`/`0x2`/`0x0` × both entry TF values × **both VEH orders**: each owner serviced exactly once, bits cleared, TF restored exactly, orders converge, unowned `#DB` never consumed |
| fourth-read latch | reads 1–3 latch nothing; read 4 latches the value and ties to the last **slot-hit** write by **event id**; a post-read write does not move it; write-once |
| gate off | ARM **refused**, no page protected, ledger inert |

**Fixture success does not replace the live installer control.** No live ON run was performed.

## Inertness

With `JSRF_TRACE_A2H_SLOTW` unset: **0 `[A2HSLOTW]` lines** in the OFF run's log, and the collector
reports `GUEST_SLOTW unarmed reason=never_initialised` — the ledger is legitimately all zero, which
is reported as *unarmed* and **not** as a layout mismatch.

## ⚠ The ARM precondition, measured — and what it means for the live trial

`MEM32(0x0019DCE0)` is **0 in every probe run** (the title never reaches guest entry) and a
**non-pointer at terminal** in real runs. ARM therefore **defers** until the pointer is plausible,
and the terminal re-derivation reports `term_base_ok`. **A reader must expect `stable=0` on the
terminal row** and treat it as a re-scope, not as a clean comparison — the packet's
*"changed base ⇒ re-scope, never silently compare old VA"* rule, exercised by the actual data.

## Scope and prohibitions

Observation only; every new path is behind an environment gate, trace-only, OFF by default at
closure. No generated-code edits. `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)`
unchanged. No synthetic completion: the APU trap and `0x80` are not suppressed, no allocation is
faked, the arena is not widened, the NULL call is not bypassed, guest error handling is not edited.
`PIO_FREE`, `A4b2-r7`, `A4b2-r8`, `A4b1-r4`, `0xFFFFB3` untouched. **The retired NULL line was not
reopened and no DR record is cited.**

## Controls

| Gate | Result |
|---|---|
| `scripts/build-jsrf.py` | **succeeded** |
| `ctest --test-dir build -C Release` | **22/22 passed** |
| `scripts/test-harness.py` | **19 probes + 9 delivery fixtures + native-#DB control passed** |
| 9 required guards | **all passed** |

## Not done here, and who owns it

- **The live ON trial (N ≤ 5, stop at first qualifying) and every outcome row.** The Session owns
  the ON runs; this record claims no row, and the installer trap's live firing is **not** claimed.
- The runner refuses a strict launch without `RECOMP_GPU_ACK=0`, so no ON run was attempted from
  this Worker.
