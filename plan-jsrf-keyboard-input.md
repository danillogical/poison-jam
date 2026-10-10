# Jet Set Radio Future: keyboard input to press Start

**Status: execution authority** (owner, 2026-10-10).

This plan begins after `plan-jsrf-title-screen.md` reached M15. The title-screen plan remains the
historical record for reaching and reproducing the title; this file owns the next objective:
**accept real host keyboard input and use it to press Start at the JSRF title screen.**

## Authorities

This file does not duplicate the permanent rules owned elsewhere:

- `docs/agent-workflow.md` owns the DSH staffing/workflow when DSH is the active harness.
- `AGENTS.md` owns build, evidence, repository, machine, and push discipline.
- `docs/jsrf-run-profiles.md` owns strict/exploratory/fixture classification.
- `docs/jsrf-technical-record.md` owns established technical facts (`TR §n`).
- `docs/jsrf-compatibility-ledger.md` owns every pragmatic shortcut (`Lnn`).
- `config/stop-chain.json` owns the runtime stop chain.
- `plan-jsrf-title-screen.md` owns the completed M15 history.
- The retired post-title milestone table is available at
  `git show d6b8336:plan-jsrf-bare-minimum.md`.

## Current work (2026-10-10)

**Architecture is measured, not assumed** — see "Measured data-flow" below. Two findings move the plan:

1. **XAPI's own OHCI bring-up runs at the title.** The M15 run connected the guest USB ISR
   (`KeConnectInterrupt: vector 1 -> routine 0x001C288F`) and built its controller object at
   `0x01092408` holding `FED00000 / 00001000 / 80081000`. The historical "input init is dead"
   result is about `xbox_OhciInit`/`xbox_InputInit`, and it is still true that nothing calls them —
   but the *guest* path that needs them is live. The gap is the model behind the aperture, not the
   guest code.
2. **Route B is the cheap route.** The missing pieces were exactly two, and both are now in place:
   `xbox_OhciInit()` called from the game's init (toolkit `xbox_usb`, `RECOMP_USB`-gated), and the
   `0xFED00000` register fault routed to `xbox_OhciHandleMmio`. The toolkit's OHCI model now
   registers its own VEH, so the routing is self-contained; the game VEH also carries an explicit
   branch so the behaviour does not depend on handler order. Route C (a high-level XID bridge) is
   **not** needed unless measurement shows the OHCI model cannot carry the report.

**Done this turn, on the toolkit side (`xboxrecomp`):**

- `src/input/keyboard_pad.{c,h}` — the Enter→Start mapping in one table, plus the report builder.
  `xinput_device.c` now calls it instead of holding a private copy, so the test and the runtime
  exercise the same code.
- `xbox_OhciInit` registers its own vectored exception handler; `ohci.h`'s stale "does not walk the
  descriptor lists" comment is gone, and the embedder contract is stated as it now is.
- `include/xbox/xboxrecomp.h` exposes `ohci.h` and `usb_gamepad.h`.
- `tests/keyboard_pad/keyboard_pad_test.c` — T1–T5, registered as ctest `keyboard_pad`.

**Next action:** close stop 34 (`0x001BE689`), the indirect-call target the driver enters immediately
after `SET_CONFIGURATION`. It is the same class as stops 29–32 and needs the real entry extent
recovered from the XBE. Once the driver finishes configuring the pad, the port-0 interrupt endpoint
(0x81) is what carries `usb_gamepad_report`, and that is the point at which a keyboard press can reach
the guest — R2.

### Measured: the model is driven, and the descriptor walk was the blocker (2026-10-10)

Run `20261010-020002-826-k16-ohci-enumerate` (423 s, `RECOMP_USB=1`) shows the guest's own XAPI driving
the model — `[OHCI0] read +0x00 = 00000010` (HcRevision, a value that exists nowhere in guest RAM and
comes from `ohci_reset`), the `HcControl`/`HCCA`/`HcFmInterval` writes, `periodic list enabled`, and
`device arriving on port 1`. `ordinal 44` (`HalGetInterruptVector`) returns `1` with the call site
`ret=0x001BD137`, the instruction after the `0xFED00000` store, so XAPI's bring-up is what ran.
**Route B is live; Route C is not needed.**

But enumeration livelocked: `setups 0 reports 0`, `HcInterruptStatus=WDH` re-raised thousands of times.
Root cause, measured by a `bus_resolve` trace in `20261010-021053-776`: the driver hands the controller
**physical** addresses (`HcHCCA=0x00081000`, `HcControlHeadED=0x000819A0`), and `bus_resolve`'s image
exception returned them unchanged because `0x00081000` is inside `.text`'s VA range — a coincidence of
the contiguous allocator numbering from 0. Every ED then read back code bytes, `ohci_pad_for()` found no
device, and `ohci_do_td` moved nothing.

Fix: `bus_resolve` now tests membership in a live contiguous allocation first
(`xbox_ContiguousBlockSize(0x80000000 + addr) != 0`), then the image, then the high-water mark. That is
the only test that separates JSRF's low descriptors (allocated) from Burnout 3's in-image static
(`0x0041A904`, never allocated).

## Handoff from M15

M15 is complete and reproduced.

The current title plan records two runs that presented the real title screen by content:

- `20261009-225354-960-title012-m15-long`
- `20261009-232934-172-title012-m15-repro2`

The displayed screen contains the JSRF emblem and `PLEASE PRESS START TO BEGIN`.

The retired plan defined the next two relevant milestones as:

- **M16 — Controller input:** a real host input reaches the guest's own XID/controller state.
- **M17 — Main menu navigation:** host input drives Start/options/back and changes the guest menu state.

This plan takes an owner-directed keyboard route to the same functional boundary. The first target is
smaller than full controller support: **press one real keyboard key and make the guest observe Xbox
Start at the title screen.**

Historical input findings in the retired plan say that, at that revision, `xbox_OhciInit` and
`xbox_InputInit` were not called. Treat that as a lead, not a current-tree fact: re-measure the present
tree before designing around it.

`docs/jsrf-run-profiles.md` currently classifies `RECOMP_PAD_PRESS`, `RECOMP_PAD_SCRIPT`, and
`RECOMP_PAD_LIVE` as synthetic input and therefore exploratory. They are useful probes, not the
finished keyboard-input path.

## Objective: K16 — real keyboard Start

**A physical keyboard press in the recomp window causes the guest to observe Xbox Start and leave the
`PLEASE PRESS START TO BEGIN` title state through the game's normal input logic.**

Initial default mapping:

```text
Enter / Return -> Xbox Start
```

Do not hard-code a numeric Xbox button bit from memory. Use the current toolkit's XID/controller
definitions as the source of truth.

### K16 acceptance

K16 is met when one completed runtime record establishes all of the following:

1. The recomp is at the real M15 title screen.
2. No synthetic `RECOMP_PAD_PRESS`, `RECOMP_PAD_SCRIPT`, or `RECOMP_PAD_LIVE` input is supplying the
   accepted Start event.
3. A real host keyboard transition is observed by the runtime.
4. That transition changes the emulated/translated guest controller report through the chosen durable
   input path.
5. The guest's own title input state observes Start.
6. The title responds by advancing beyond `PLEASE PRESS START TO BEGIN`.
7. Releasing the keyboard key clears the guest Start state; no stuck key remains.
8. A second run reproduces the transition.

Preserve enough evidence to distinguish these links:

```text
physical key
  -> host key event/state
  -> keyboard-to-Xbox mapping
  -> guest-visible XID/controller report
  -> game's title input state
  -> title advances
```

A log saying `Start pressed` on the host is not enough.

### Stretch: K17 — keyboard menu navigation

Only after K16 is real and stable, extend the same mechanism far enough to operate the first menu:

- directional navigation;
- accept;
- back/cancel;
- Start where appropriate.

Use existing Xbox input constants and guest semantics rather than inventing new button values.

K17 is met when keyboard input visibly changes the guest's own menu selection/state and can perform at
least one forward navigation and one back/cancel action.

K17 is useful but **not required to close K16**.

## Stance

Take the cheapest honest path.

A high-level keyboard-to-XID bridge is acceptable if it is the shortest reliable route. Full USB/OHCI
fidelity is not required merely to press Start.

If the implementation bypasses hardware that the original Xbox would have used, record that
approximation in `docs/jsrf-compatibility-ledger.md` and list its ledger ID in the accepting run.

Do not confuse a pragmatic shortcut with fake user input:

- **Allowed finished path:** a real human keypress is translated into the guest controller state.
- **Diagnostic only:** a timer, environment variable, script, or unconditional pulse manufactures the
  Start press without live host input.

The first is an input backend. The second is synthetic completion.

## First question: what input path exists now?

Before implementing, trace the current tree.

Answer these with source evidence:

1. Where does the Windows host window/event loop receive keyboard messages today?
2. Is there already a durable host key-state structure?
3. Where is the current Xbox/XID controller report built or stored?
4. What code serves guest controller reads/probes?
5. Are `xbox_OhciInit` and `xbox_InputInit` called on the current tree?
6. What does `RECOMP_USB` do on the current tree, if it still exists?
7. Where do `RECOMP_PAD_PRESS`, `RECOMP_PAD_SCRIPT`, and `RECOMP_PAD_LIVE` enter the pipeline?
8. Can live keyboard input reuse the same final guest-visible controller state without inheriting the
   synthetic environment-variable mechanism?
9. Which thread owns host input state and which thread reads guest input?
10. What synchronization already protects that state?

Produce a short call/data-flow before choosing the implementation point.

Do not assume the historical "input init is dead" result still applies.

## Measured data-flow (2026-10-10, this tree)

Every claim below is a measurement on `poison-jam` `c132f6c` / `xboxrecomp` `60bf20a`; the run cited is
`logs/runs/20261009-232934-172-title012-m15-repro2` (the M15 reproduction).

| # | Answer | Evidence |
|---|---|---|
| 1 | `src/video/fb_present.c` `fb_wndproc`: `WM_KEYDOWN`/`WM_SYSKEYDOWN` set, `WM_KEYUP`/`WM_SYSKEYUP` clear, `WM_KILLFOCUS` memsets. Window is `CreateWindowExA(..., WS_OVERLAPPEDWINDOW | WS_VISIBLE, ...)`, created by `xbox_FramebufferWindowStart` only when `RECOMP_FB_WINDOW` is set. | toolkit `src/video/fb_present.c:315-345`, `:451-452`, `:551-565` |
| 2 | Yes: `static volatile unsigned char s_key_down[256]`, read by `xbox_FramebufferKeyDown(vk)`. | `src/video/fb_present.c:276`, `:300-305` |
| 3 | **Two** producers, both feeding one guest-visible device report. Host: `xbox_InputGetState` (XInput, plus the `RECOMP_KEYBOARD` keyboard overlay). Guest-visible: `usb_gamepad_report()` builds the 20-byte Xbox XID report. | `src/input/xinput_device.c:136-216`; `src/usb/usb_gamepad.c:559-631` |
| 4 | The guest serves its own reads: `XAPILIB__XInputOpen` `0x001C3BA1`, `XInputGetState` `0x001C3E14`, `XInputPoll` `0x001C3EBD` are lifted guest code in the XPP section (`0x001BC7C0..0x001C3F58`). They talk to the OHCI registers at `0xFED00000` directly. | `config/xdk-symbols.json` symbols; `src/recomp/gen/recomp_0003.c:81455`, `recomp_0005.c:67525`, `:67664` |
| 5 | **`xbox_InputInit` is not called** (nothing in either tree calls it). **`xbox_OhciInit` is not called** either, and the game's VEH has no OHCI branch. | grep of both trees; `src/main.c:302-330` routes only NV2A `0xFD000000` and APU `0xFE800000` |
| 6 | `RECOMP_USB` still gates the OHCI model: `xbox_OhciInit` is a no-op without it, and it then `VirtualProtect`s both 4 KiB blocks `PAGE_NOACCESS` so register accesses fault. **The toolkit installs no VEH** — the embedder must route the fault. | `src/usb/ohci.c:1263-1264`, `:1305-1316`; `ohci.c:1289-1290` states the contract |
| 7 | All three are inside `usb_gamepad_report`: `RECOMP_PAD_PRESS` via `synthetic_buttons()`, `RECOMP_PAD_SCRIPT`/`RECOMP_PAD_LIVE` via `pad_script_apply()`. They are OR-ed into the same report byte 2 as real input. | `src/usb/usb_gamepad.c:247-255`, `:411`, `:447-461`, `:612-618`, `:630` |
| 8 | Yes — the keyboard overlay in `keyboard_state()` already produces exactly the `XBOX_INPUT_STATE` that `usb_gamepad_report` consumes. It is the same `xbox_InputGetState` port-0 path the real pad uses. | `src/input/xinput_device.c:87-125`, `:200-216`; `usb_gamepad.c:611` |
| 9 | Host key state is owned by the **window thread**; read by the OHCI controller thread (through `usb_gamepad_report`) and by the guest thread. | `fb_present.c:315-345` vs `ohci.c:1016-1240` |
| 10 | Deliberately none, and documented as safe: one byte per key, written by one thread and read by another, so a press seen a frame late is indistinguishable from one made a frame later. `WM_KILLFOCUS` clears the whole array. | `src/video/fb_present.c:269-276`, `:344-345` |

### What the M15 run actually did

**XAPI's own OHCI bring-up DID run, and it connected the OHCI ISR.** Two independent measurements:

- `jsrf_run.log:688` — `[KERNEL] KeConnectInterrupt: vector 1 -> routine 0x001C288F context 0x01092408`.
  That print is unconditional (not budget-gated), and vector 1 is `OHCI_VECTOR` (`ohci.c:52`).
- The guest controller object at that context: `0x01092408: FED00000 00001000 80081000` — OHCI base,
  size `0x1000`, contiguous DMA address. Read from the run's own dump.

`0x001C288F` is therefore the guest's OHCI ISR, and it reads `[ecx+0x10]`/`[ecx+0xc]` — OHCI
`HcInterruptEnable`/`HcInterruptStatus` — then tests bit 31 (`HcInterruptMasterEnable`).

**The histogram is not evidence of absence.** `[KERNEL] summary` prints the top six ordinals and the
per-call `#n:` lines stop at the 200-call budget (`kernel_bridge.c:10074-10098`, `:470`), so a
once-per-boot call to ordinal 44/98/177 cannot appear there. `ordinal 98 x0` in a summary means "not in
the top six", not "never called".

**The gap is the model behind the aperture, not the guest path.** With `xbox_OhciInit` never called,
`0xFED00000` is plain committed RAM, so the ISR's `HcRevision` read returns 0, no root-hub port ever
reports a device, and the pad is never enumerated. That is a two-part fix, and both parts are the
toolkit's own documented contract:

```text
guest XAPI (already runs)
  -> reads/writes 0xFED00000          (already runs)
  -> fault                                 <-- MISSING: the two blocks are not PAGE_NOACCESS
  -> embedder VEH -> xbox_OhciHandleMmio   <-- MISSING: no OHCI branch in the game VEH
  -> OHCI model + usb_gamepad_report
  -> xbox_InputGetState -> keyboard_state -> s_key_down   (already implemented)
```

## Preferred architecture

Prefer the smallest architecture that preserves a clean ownership boundary:

```text
Windows keyboard event
        |
        v
host keyboard state
        |
        v
keyboard -> Xbox mapping
        |
        v
existing emulated controller/XID state
        |
        v
guest input API/report
```

The keyboard layer should not patch the game's title-state global directly.

It should enter at the lowest existing controller abstraction that is cheap and stable enough for the
project.

### Preferred route A — reuse an existing guest-visible controller backend

If the current toolkit already has a functional controller/XID report path that is merely missing a
live keyboard producer:

- feed keyboard state into that backend;
- keep device/report semantics there;
- avoid duplicating controller packet construction in the game repo.

This is the preferred route.

### Route B — initialize an existing USB/OHCI path

If the current host controller stack is already implemented and the only missing piece is initialization,
wire the required initialization at the correct runtime startup point.

Do this only if measurement shows it is genuinely the cheaper route.

Do not add a large USB bring-up project merely because the original console used USB.

### Route C — narrow high-level XID bridge

If USB/OHCI initialization is substantially more work than needed, add a small high-level keyboard-to-XID
bridge.

Requirements:

- real keyboard state is the source;
- the guest sees the same controller-report shape expected by its normal input code;
- the shortcut is ledgered;
- it is not implemented as a title-screen-specific memory poke;
- it remains useful for K17 and later gameplay controls.

For the pragmatic project stance, Route C is preferable to weeks of device-fidelity work.

## Keyboard event rules

For the first implementation:

- Map Enter/Return to Xbox Start.
- Track **down and up** transitions.
- Avoid autorepeat changing semantic state unexpectedly.
- Clear held keys when the window loses focus or input is otherwise invalidated.
- Do not intercept unrelated system/global keyboard input.
- Input should be active when the game window is the intended target.
- Keep the mapping table centralized so K17 can extend it without rewriting the backend.

If the current window layer exposes scan codes rather than virtual keys, use the existing host abstraction
instead of forcing a new one.

## Tests before the long runtime

Add cheap tests around the durable boundary.

At minimum:

### T1 — Start down

A simulated host Enter-down transition produces a controller report/state with Start asserted.

### T2 — Start up

Enter-up clears Start.

### T3 — no sticky state

Focus loss/reset clears a held Start.

### T4 — unrelated key

An unrelated key does not assert Start.

### T5 — mutation/control

Demonstrate the test can fail if the Enter-to-Start mapping or report write is removed/reversed.

Tests should exercise the same mapping/report code used by the real window path, not a second test-only
implementation.

If the current architecture makes one of these tests meaningless, replace it with a stronger equivalent
and record why.

## Runtime proving sequence

Do not begin with another 1800-second blind run if a shorter probe can prove input delivery.

### R1 — host event probe

Reach a window state where keyboard events can be observed and prove:

```text
Enter down
Enter up
```

are captured with bounded logging.

This proves only the host side.

### R2 — guest-visible report probe

Show that the same physical key transition changes the controller/XID state the guest reads.

Prefer a bounded counter/state trace over unbounded per-poll logging.

The deciding observation is guest-visible state, not the host log.

### R3 — title run

Use the supported title path to reach the real M15 title.

Once `PLEASE PRESS START TO BEGIN` is visibly present:

1. press Enter once;
2. release it;
3. observe the guest input state;
4. preserve the presented frame/state before the press;
5. preserve the first state/frame proving the title advanced.

Do not inject Start before the title is actually ready unless specifically testing pre-title behavior.

### R4 — reproduction

Repeat on the same durable tree.

K16 closes only after the second successful live-keyboard run.

## Evidence profile

The accepted K16 event should come from **real host keyboard input**.

Do not use these as the accepting input source:

```text
RECOMP_PAD_PRESS
RECOMP_PAD_SCRIPT
RECOMP_PAD_LIVE
```

They may be used in exploratory diagnostics when useful, with their normal profile classification.

If the final keyboard implementation uses a high-level controller bridge or skips USB/OHCI hardware,
that implementation is a pragmatic shortcut and must be ledgered. The physical keypress itself is still
real input; the ledger describes the emulation boundary used to deliver it.

## Instrumentation

Keep input telemetry compact.

Useful bounded events:

```text
[INPUT] host key down <key>
[INPUT] host key up <key>
[INPUT] xbox Start 0->1
[INPUT] xbox Start 1->0
[INPUT] guest report observed Start=<0|1>
```

Use the project's actual logging style rather than these exact strings if an existing convention exists.

Do not emit one line per controller poll for an 1800-second run.

If guest code polls at high frequency, record transitions/counters/maxima instead.

## Ownership: game repo vs toolkit

Prefer toolkit changes for generic Windows/Xbox input translation.

Use the game repo for:

- JSRF-specific probes/evidence;
- run recipes;
- acceptance tooling;
- JSRF-only compatibility records.

A title-specific direct patch to the game's "press Start" state is not the goal.

If the cheapest implementation point is ambiguous, ask the Persistent Advisor when DSH is in use, or make
the corresponding evidence-driven architecture decision in the active harness.

## Keep M15 stable

Input work must not regress title-screen reachability.

Before claiming K16:

- the current title path still reaches the real title;
- the keyboard-enabled build does not require a synthetic Start pulse;
- no default held key skips the title automatically;
- a no-input control can remain at the press-Start screen long enough to demonstrate that input is what
  advances it.

If the game auto-advances for another reason, that is not K16 evidence.

## Main failure branches

### A. Host keyboard event is never seen

Investigate the window/message loop and focus/dispatch path.

Do not touch guest input yet.

### B. Host event is seen, guest report does not change

The mapping/backend boundary is the blocker.

Trace the actual controller state read by the guest.

### C. Guest report changes, title state does not

Then Start may be represented differently, the wrong port/device may be updated, or the title's input path
may not be reading that report.

Trace the guest call/state transition before changing the host mapping.

### D. Title advances but key sticks

Fix key-up/focus-loss/state ownership before accepting K16.

### E. Title advances only with `RECOMP_PAD_*`

That is a useful control showing the guest path can respond, but it does not close K16. Compare the
synthetic path's final controller state with the live keyboard path.

### F. Pressing Start exposes a new ICALL/ABI/runtime stop

That is progress.

Record the stop in `config/stop-chain.json`, preserve the input evidence, close the newly exposed blocker
using the established stop-chain discipline, then rerun K16.

Do not roll back working keyboard input merely because it makes the game reach new code.

## Working loop

1. Inspect current input architecture and write the minimal data-flow.
2. Pick the cheapest route A/B/C and record why.
3. Add mapping/report tests before or alongside implementation.
4. Prove host key delivery.
5. Prove guest-visible Start state.
6. Reach M15 and press Enter live.
7. If Start reveals a new blocker, close it and continue.
8. Reproduce the complete press-Start transition.
9. Update the technical record and compatibility ledger where applicable.
10. Run final tests/checks, commit, push, and verify both repositories according to `AGENTS.md`.

## Completion

### K16 complete

The plan is complete when:

- a real host Enter press is translated to Xbox Start;
- the guest observes that state through its normal controller/input logic or the documented pragmatic XID
  bridge;
- the title advances because of that press;
- release clears the state;
- the result reproduces;
- relevant tests/checks pass;
- any hardware-bypass shortcut is ledgered;
- both repositories are clean and pushed.

### Next

After K16, either:

1. continue immediately into **K17 keyboard menu navigation** if the same backend only needs a small mapping
   extension; or
2. make K17 the next plan if Start uncovers enough new work to deserve its own turn.

The larger retired sequence remains:

```text
M16 input
M17 menu navigation
M18 new game / opening area
M19-M21 scene, rendering, skating and camera
...
```

The immediate goal is deliberately narrower:

> **Press Enter on the real keyboard at the real JSRF title screen and make the game respond as though
> Xbox Start was pressed.**
