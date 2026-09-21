# JSRF GPU setup: reviewed contract and integration gaps

Status: milestone 11a contract audit, 2026-09-13, with 11c1 setup recovery
through framebuffer publish as of 2026-09-21. This is instruction/source
evidence, not a completed GPU implementation. Ordinary startup now reaches
missing `0x001918E0` inside device initializer `0x00192090` after WBINVD.
The game device is `0x0019B200`; the hardware context passed in ECX is
device + `0x2268`. Confirm pointers from each new capture.

Every NV2A address written or read by the four setup functions is now decoded
to a named register or a standard/vendor-specific PCI field (see Decoded
register map). Open items are limited
to: the electrical effect of the `0x80C0` GPIO write, the meaning of the
vendor-specific PCI bytes `0x4C..0x4F`, the identity of constant `0xFE502A`,
and the 7th argument of `KeInitializeInterrupt`. No rebuild was needed for
this documentation audit.

## Original setup contract

Use `python -X utf8 scripts/inspect-jsrf.py disasm <start> <end>` from the game
root. End addresses below are exclusive. XBE identity is pinned in AGENTS.md.

| Original span/address | Confirmed operation | Required host/model behavior |
|---|---|---|
| `194ADD..194C3F` | Thiscall-style ECX=context, RET with no stack arguments, boolean EAX return. | Preserve original code and guest objects; review all helpers before claiming success. |
| `194AE7..194AFA` | Initialize DPC at context+84 with callback 194480 and context. | Guest callback/queue structure, correct calling convention and lifecycle. |
| `194AFA..194B43` | Initialize two dispatcher/list structures near context+1C8/+1D8. | Keep actual field writes; do not replace them with native pointer layouts. |
| `19460A..194635` | Store FD000000 at context+0; `NV_PBUS_PCI_NV_1` (FD001804) OR= 4 (PCI command bus-master enable); `NV_PCRTC_INTR_EN_0` (FD600140 = PCRTC block 0x600000 + 0x140) = 0; `NV_PTIMER_INTR_EN_0` (FD009140) = 0. | Bus-master on, vblank and PTIMER-alarm interrupt sources off. All three registers are named (Decoded register map). |
| `194635..194676` | Read `NV_PBUS_PCI_NV_0` (FD001800) high word masked FFFC into context+A4 (device ID), FD001808 low byte into +BC (`NV_PBUS_PCI_NV_2_REVISION_ID`), FD10020C into +A8 (`NV_PFB_CSTATUS`, modeled VRAM size in bytes); store constant FE502A at +B8. | The GPU's own PCI identity and revision, plus visible RAM size, through the PBUS/PFB mirrors. The 0x1800 read is NOT bus 0/dev 3 PCI config. Never infer them from zero-filled aperture reads. |
| `194B5E..194B94` | HalGetInterruptVector(3), KeInitializeInterrupt at context+14 with ISR 193C50 (7 pushes; 7th-argument semantics unconfirmed), KeConnectInterrupt, fail if connection returns false. | Correct vector/IRQL and guest callback execution; recording a pointer alone is not tested interrupt delivery. |
| `194B94..194BAF` | Register shutdown callback 1941E0 through the object at context+1A0. | Preserve registration/lifetime; shutdown is not exercised yet. |
| `194676..194780` | `NV_PBUS+0x830` (PCI configuration offset 0x30, expansion-ROM BAR) = 0; `NV_PBUS+0x80C` (PCI configuration dword 0x0C: cache-line/latency/header/BIST) = 0xF800, which sets the latency-timer byte to 0xF8; fraction-reduce a ratio against the 31.25 MHz constant 0x1DCD650; `NV_PTIMER_NUMERATOR` (FD009200) = n, `NV_PTIMER_DENOMINATOR` (FD009210) = d; `NV_PTIMER_ALARM_0` (FD009420) = 0xFFFFFFFF; context+B0 = 1, +130 = 2; calls 196800/196967/196A65/194533/196B34/196B6A/196B9A. | Expansion ROM disabled and PCI latency timer programmed; master-timer divisors programmed. This is the PTIMER master clock, not the core PLL (core PLL is `NV_PRAMDAC_NVPLL_COEFF` at 0x680500, which this span does not touch). Calls are still unrecovered. |
| `194780..1948B9` | Temporarily clear mask 0x00000300 of FD00184C and restore the original dword; `NV_PMC_ENABLE` (FD000200) = 0xFFFFFFFF; `NV_PMC_INTR_EN_0` (FD000140) = context+B0 (1 = hardware interrupt enable); call 196E80; KeQuerySystemTime + RtlTimeToTimeFields + calendar formula; write `NV_PTIMER_TIME_0/TIME_1` (FD009400/9410) = 0/computed-value; `NV_PGRAPH_FIFO` (FD400720) = 0; call 197C50; `NV_PGRAPH_INTR` (FD400100) = 0xFFFFFFFF (W1C clear), `NV_PGRAPH_INTR_EN` (FD400140) = 0xFFFFFFFF; call 197BCF; `NV_PFIFO_INTR_0` (FD002100) = 0xFFFFFFFF (W1C clear), `NV_PFIFO_INTR_EN_0` (FD002140) = context+118. | Enable all GPU blocks and the hardware interrupt, perform the observed PTIMER writes, disable PGRAPH FIFO access before helper 197C50, clear pending interrupts, then enable PGRAPH and selected PFIFO sources. The meaning of the calendar-derived PTIMER value and vendor dword pulse remains unproven. In the baseline capture context+118 is 0, so PFIFO sources start disabled. |
| `194BC3..194BF9` | Write two RGB identity ramps, 256 bytes per channel. | Six real 256-byte outputs beginning at context+214. |
| `194C01..194C0A` | OUT DX,AL: port 80C0, one byte, value 1. | PM GPIO[0] (PM IO base 0x8000 + GPIO 0xC0, presumed). xemu models GPIO[0] reads as a toggling field-pin bit 5 and models only the 0x16 aspect write; all other GPIO writes are ignored (FIXME in xemu). Exact electrical effect remains unproven. |
| `194C0A..194C27` | Read four bytes from PCI bus 1, slot 0, offset 0x4C; OR 1F into byte 3 (offset 0x4F); write four bytes back. | Bus 1/dev 0 is the NV2A endpoint behind the AGP bridge; 0x4C..0x4F is its vendor-specific config region, not bridge secondary status. The nearby PBUS 0x84C access is consistent with a mirror, but shared backing is an 11b design requirement rather than a fact established by the XBE or current emulators. xemu models neither path (reads 0, writes ignored); the current HAL stub does not satisfy the read-modify-write. |
| `194C27..194C3F` | Store 1 at context+A0, call 194780, normalize return to boolean, restore registers. | Actual completed setup result. Do not guess successful EAX. |

## Primary implementation references

Reference revision: xemu `75650bd8cd91945f7b79774e2cee0b200ca373ff`, retrieved
2026-09-12. Copies are in `logs/references/gpu/`; these are emulator implementation
evidence, not original hardware datasheets.

- The PBUS handlers route offsets 800/804/808 to GPU PCI identity, command and
  class/revision. This agrees with the toolkit's local register definitions.
  Offsets 80C/830/84C have no dedicated xemu handler: reads return zero and
  writes are ignored. Standard PCI layout identifies 80C as configuration
  dword 0x0C and 830 as the expansion-ROM BAR; 84C remains vendor-specific.
  [xemu pbus.c](https://github.com/xemu-project/xemu/blob/75650bd8cd91945f7b79774e2cee0b200ca373ff/hw/xbox/nv2a/pbus.c)
- NV2A device setup: 16 MiB BAR0 MMIO (matching the toolkit aperture), PCI
  vendor NVIDIA, revision 0xA1, interrupt pin INTA, subsystem id 0. No
  vendor-specific config-space handler: offsets 0x40..0x7F are QEMU-reserved
  (read 0, write ignored). `nv2a_reset` writes PLL coefficient 00011C01 and a
  233333324 Hz core clock. The PTIMER block at 0x009000 defines NUMERATOR 0x200
  and DENOMINATOR 0x210, matching the game's FD009200/FD009210 writes.
  [xemu nv2a.c](https://github.com/xemu-project/xemu/blob/75650bd8cd91945f7b79774e2cee0b200ca373ff/hw/xbox/nv2a/nv2a.c)
- xemu's PM device sits on bus 0/dev 1 (LPC) with the GPIO subregion at PM
  offset 0xC0, 26 bytes long; the PM IO base in this revision comes from QEMU
  BAR allocation because the configuration-register base code (config+0x84)
  is compiled out (`#if 0`). xemu therefore does not guarantee that guest
  port 0x80C0 maps to its GPIO block.
  [xemu xbox_pci.c](https://github.com/xemu-project/xemu/blob/75650bd8cd91945f7b79774e2cee0b200ca373ff/hw/xbox/xbox_pci.c)
- PFB CSTATUS returns the modeled VRAM size; WBC reports no flush pending in
  that implementation. This does not prove rendering completion.
  [xemu pfb.c](https://github.com/xemu-project/xemu/blob/75650bd8cd91945f7b79774e2cee0b200ca373ff/hw/xbox/nv2a/pfb.c)
- xemu places GPIO at PM offset C0. Its offset-zero read synthesizes a field
  bit; writes at that offset have no modeled effect. Matching guest port 80C0
  to that GPIO block assumes PM base 8000. The exact write-bit behavior remains
  open; xemu's omission is not proof that the bit is irrelevant on hardware.
  [xemu acpi_xbox.c](https://github.com/xemu-project/xemu/blob/75650bd8cd91945f7b79774e2cee0b200ca373ff/hw/xbox/acpi_xbox.c)
- xemu routes the AGP bridge through PIRQC to IRQ 3, consistent with the guest
  request. This supports the interrupt source association, not our bridge's
  complete IRQL/delivery behavior.
  [xemu xbox_pci.c](https://github.com/xemu-project/xemu/blob/75650bd8cd91945f7b79774e2cee0b200ca373ff/hw/xbox/xbox_pci.c)

## Local toolkit audit

Read code instead of treating README integration snippets as executable proof.

1. Milestone 11b1 now installs the core as the single register-aperture owner.
   The existing 16 MiB mapping is permanently `PAGE_NOACCESS` while active;
   actual accesses fault through the project VEH and are serialized by one
   owner lock. The legacy acknowledgement worker is atomically quiesced before
   protection. Its clock duties remain active. The hook still allocates a
   detached 64 MiB VRAM buffer and 1 MiB RAMIN, so DMA access to this game's
   real pushbuffer allocation is now the only mapping accepted by 11b4a.
2. `src/nv2a/nv2a_core.c` has useful PBUS/PFB/timer/PLL handlers, while PFIFO is
   register storage. PRAMIN now dispatches the full 1 MiB aperture but remains
   inaccessible until the validated GPU-instance claim binds its exact range;
   MMIO and DMA-object reads then share that backing. 11b4a routes exact USER
   DMA GET/PUT through bounded packet intake, but records methods without applying
   renderer effects. IRQ aggregation calls shim PCI functions, not a verified
   guest ISR. Calling the MMIO initializer alone does not complete GPU work.
3. `src/kernel/xbox_memory_layout.c` maps canonical RAM and its real aliases,
   but allocates the 80000000 contiguous window separately. Pushbuffers and
   reserved GPU instance memory live there. A single detached VRAM pointer,
   or masking every address into canonical RAM, loses these buffers.
4. The old acknowledgement worker can advance DMA_GET and idle/busy state
   without running commands, but 11b1 disables those GPU mutations before the
   register owner becomes active. Optional software execution and D3D11 method
   translation remain separate incomplete paths. 11b4/11b5 must select one
   submission/completion owner before either path is enabled.
5. `src/kernel/kernel_hal.c:xbox_HalReadWritePCISpace` zeroes reads and ignores
   writes for unsupported endpoints. Milestone 11b2 now routes bus 1/slot 0 to
   serialized NV2A configuration and deliberately shares its vendor backing
   with PBUS FD00184C; the original code does not itself prove that the views
   alias. Immutable vendor/device and class/revision bytes are preserved.
6. The toolkit block table (`nv2a_core.c`) and xemu's agree: PMC 0x0, PBUS
   0x1000, PFIFO 0x2000, PTIMER 0x9000, PFB 0x100000, PGRAPH 0x400000,
   PCRTC 0x600000, PRAMDAC 0x680000, USER 0x800000. This confirms the
   acknowledgement worker's offsets are new-map registers: 0x100410 =
   `NV_PFB_WBC` flush bit, 0x2100 = `NV_PFIFO_INTR_0`, 0x400100 =
   `NV_PGRAPH_INTR`. The game's setup code uses the same map, so the worker
   and the register model can share one owner.
7. The VEH decoder handles selected host instruction forms, not all x86-64.
   Before relying on it, verify emitted Release accesses, operand widths and
   flags, including read/modify/write. Decode failure must remain diagnostic;
   falling back to zero pages silently bypasses the device model.

**Decision: Reuse the core's reviewed register handlers behind a project-aware
adapter, while implementing missing PCI, memory, submission and interrupt
contracts explicitly.** Milestone 11b1 implements the single register owner,
diagnostic publication and teardown. Guest physical-memory access still needs
a mapping interface that accounts for the current separate allocations;
reconcile aliases before claiming unified hardware RAM semantics. PCI sharing,
command processing and interrupt delivery remain later packets.

## Decoded register map

All offsets are relative to the 16 MiB NV2A aperture at `0xFD000000`, except
the PCRTC entry, which is the absolute address `0xFD600140` (PCRTC block
`0x600000` + `0x140`). Register names are from `nv2a_regs.h`, which matches
the pinned xemu definitions.

| Aperture offset | Register | Operation in 11a code | Meaning |
|---|---|---|---|
| `0x000140` | `NV_PMC_INTR_EN_0` | `= context+B0` (1) | Enable hardware interrupt sources. |
| `0x000200` | `NV_PMC_ENABLE` | `= 0xFFFFFFFF` | Enable all GPU blocks. |
| `0x001800` | `NV_PBUS_PCI_NV_0` | read | GPU PCI vendor (low) / device (high) word; device ID masked `0xFFFC` to context+A4. |
| `0x001804` | `NV_PBUS_PCI_NV_1` | `|= 4` | GPU PCI command register: bus-master enable. |
| `0x001808` | `NV_PBUS_PCI_NV_2` | read `& 0xFF` | GPU PCI revision (xemu models 0xA1) to context+BC. |
| `0x00180C` | PCI configuration dword 0x0C | `= 0x0000F800` | Set the latency-timer byte (offset 0x0D) to 0xF8; cache-line, header-type and BIST bytes remain zero. |
| `0x001830` | PCI expansion-ROM BAR | `= 0` | Disable/clear the expansion-ROM mapping. |
| `0x00184C` | PCI 0x4C..0x4F mirror | clear mask `0x00000300`, then restore | Temporarily clear vendor-specific dword bits 8 and 9; effect unmodeled and unproven. |
| `0x002100` | `NV_PFIFO_INTR_0` | `= 0xFFFFFFFF` | Clear all pending PFIFO interrupts (W1C). |
| `0x002140` | `NV_PFIFO_INTR_EN_0` | `= context+118` | PFIFO interrupt enable from device field (0 in baseline capture). |
| `0x009140` | `NV_PTIMER_INTR_EN_0` | `= 0` | Disable PTIMER alarm interrupt. |
| `0x009200` | `NV_PTIMER_NUMERATOR` | `= n` | Master-timer divisor numerator (reduced ratio vs 31.25 MHz). |
| `0x009210` | `NV_PTIMER_DENOMINATOR` | `= d` | Master-timer divisor denominator. |
| `0x009400` | `NV_PTIMER_TIME_0` | `= 0` | Low 27 timer bits encoded in bits 5..31; the local owner now applies the write as a counter offset while preserving the high half. |
| `0x009410` | `NV_PTIMER_TIME_1` | `= calendar-derived value` | High 29 timer bits; the local owner applies the write as a counter offset while preserving the low half. The game's calendar-to-counter intent remains original-code evidence rather than a wall-clock claim. |
| `0x009420` | `NV_PTIMER_ALARM_0` | `= 0xFFFFFFFF` | Alarm value; interrupt enable is off, so the alarm is disabled. |
| `0x10020C` | `NV_PFB_CSTATUS` | read | Modeled VRAM size in bytes (xemu returns the VRAM region size) to context+A8. |
| `0x400100` | `NV_PGRAPH_INTR` | `= 0xFFFFFFFF` | Clear all pending PGRAPH interrupts (W1C). |
| `0x400140` | `NV_PGRAPH_INTR_EN` | `= 0xFFFFFFFF` | Enable all PGRAPH interrupt sources. |
| `0x400720` | `NV_PGRAPH_FIFO` | `= 0` | Clear `NV_PGRAPH_FIFO_ACCESS`; disable PGRAPH FIFO access before helper `0x197C50`. |
| `FD600140` | `NV_PCRTC_INTR_EN_0` | `= 0` | Disable vblank interrupt. |

Sequence read as a whole: bus-master on and interrupt sources off (`19460A`),
identity and memory size queried (`194635`), PCI ROM/latency configured and
master-clock divisors programmed (`194676`), blocks enabled, PTIMER time
registers written from a calendar calculation and
interrupts armed (`194780`), then the unrecovered channel helpers.

## Facts versus hypotheses

Facts (instruction, register-definition, or capture evidence):

- The aperture offsets, operations and register names above, from the
  original disassembly and pinned register definitions.
- `0xFD000000` is the NV2A register aperture in both xemu (16 MiB BAR0) and
  the toolkit (`XBOX_NV2A_BASE`, 16 MiB). The PCRTC block is reached through
  the same aperture at `0x600000`, so `0xFD600140` is not a second window.
- xemu and the toolkit model only PCI identity/command/class-revision in PBUS;
  reads at 0x80C, 0x830 and 0x84C return zero and writes are ignored.
- xemu models no NV2A vendor-specific PCI config; `0xFD00184C` and
  `HalReadWritePCISpace(1, 0, 0x4C, ...)` are separate unimplemented accesses
  there. Treating them as aliases is the proposed 11b contract, not a fact.
- Baseline capture `20260912-231756-543-gpu-harness-final`: context+118 and
  the surrounding device fields are 0 pre-run, because the run stops at
  `0x00194ADD` before any 11a write executes.

Hypotheses (not yet provable from local evidence):

- The electrical effect of the `0x80C0` GPIO[0] write. Plausible readings
  (TV/field control, PM latch) are unconfirmed; xemu ignores the write.
- The meaning of the vendor-specific bytes `0x4C..0x4F` and the temporary
  clearing of dword mask `0x00000300`.
- The game's intended epoch/meaning for its calendar-derived PTIMER value. The
  split register encoding and writable counter-offset effect are implemented
  from pinned xemu commit `f9b14039e5bb56ae2d8f028e31e7cc19f13f7e12`.
- The identity of constant `0xFE502A` stored at context+B8.
- The 7th argument of `KeInitializeInterrupt` (mode/fast flag, per XDK 4134
  header convention) is unconfirmed.
- PM IO base `0x8000` is presumed from the game's fixed port; the hardware
  LPC/PM base is set through a configuration register that xemu compiles out.

## Proposed single state owner

**Decision: the toolkit `src/nv2a` register model is the single owner of the
`0xFD000000` aperture and of all NV2A PCI state, with a separate small PM
owner for the `0x8000` IO region.** Concretely:

1. One NV2A state structure owns PMC/PBUS/PFIFO/PFB/PGRAPH/PCRTC/PTIMER
   storage; the PBUS `0x800..0x8FF` mirror and `HalReadWritePCISpace(1, 0, ...)`
   for bus 1/dev 0 read and write the same config bytes, including the
   vendor-specific `0x40..0x7F` window. There is no second copy of PCI
   command/status.
2. PTIMER owns the master timer: running counter scaled by
   NUMERATOR/DENOMINATOR, alarm, and reviewed TIME_0/TIME_1 write semantics.
   Do not assume ordinary low/high 32-bit halves; establish the encoding from
   hardware evidence or compatible software before implementing the writes.
3. `PFB_CSTATUS` reports the modeled VRAM size; VRAM addressing must use the
   toolkit's real guest allocations (contiguous window + canonical RAM), not a
   detached buffer.
4. The PM owner owns `0x8000+0xC0` GPIO storage and records the observed
   writes; GPIO[0] read synthesizes the field-pin bit 5 per xemu. Its
   unmodeled-write behavior is documented, not guessed.
5. The acknowledgement worker's table operates on this one state; entries
   whose meaning is now named (PFB WBC, PFIFO/PGRAPH/PBUS/PMC/PCRTC interrupt
   registers) are reconciled with the model, and the worker is retired for
   the registers it covers once the model is live.

This is a proposal for 11b implementation; no code was changed in this audit.

## Concrete defect fixed and verification

Standalone reset used PLL coefficient 00011C01 (n=28, m=1, p=1) but initialized
frequency without dividing by two. Rewriting the unchanged coefficient changed
the clock from 466666648 to 233333324 Hz. Initialization now uses the existing
PLL write handler, keeping both paths consistent.

`tests/test_nv2a_contract.c` compiles the real core with a renderer guard that
fails if a draw is attempted. It checks 11 register/clock contracts, including
the actual early JSRF register accesses, unchanged-PLL stability, divider zero
and restoration. Before/after: `logs/gpu-registers-before.txt` and
`logs/gpu-registers-after.txt`. CTest target: `jsrf_nv2a_registers`.
This verifies a cold standalone register model, not guest GPU execution.

## Next acceptance gates

- 11a acceptance is met as a documentation audit: exact behavior/address
  evidence for the whole setup sequence and a proposed single state owner.
  The GPIO electrical effect, vendor-specific 0x4C meaning, 0xFE502A identity
  and the KeInitializeInterrupt 7th argument stay recorded as open
  hypotheses; they do not block 11b, which implements the proposed owner.
- 11b1 is verified: one serialized register owner handles actual 8/16/32-bit
  faulting accesses, legacy GPU acknowledgements are quiesced, frozen snapshots
  use generation-validated publication, deliberate unreadability remains
  explicit, and teardown restores the aperture before release. This does not
  establish DMA, command execution or rendering.
- 11b2 is verified: PBUS configuration offsets and the real
  `HalReadWritePCISpace(1,0,...)` path share one serialized backing by explicit
  design. Identity/class fields stay immutable; the setup latency/ROM/vendor
  accesses, byte 0x4F RMW, invalid routes and mirror boundaries are tested.
- 11b3 is verified against pinned xemu commit
  `f9b14039e5bb56ae2d8f028e31e7cc19f13f7e12`: split TIME writes, divisor and
  offset boundaries, alarm epochs, late enable, W1C/PMC aggregation and the
  wakeable service lifecycle have deterministic contracts. The runtime probe
  uses real trapped MMIO and observes expiry through a stable frozen snapshot
  without another guest PTIMER access. The service and publication share the
  11b1 owner lock and stop before aperture restoration.
- 11b4a is verified: USER GET/PUT are physical byte offsets into the separately
  mapped 64 MiB contiguous window; the installed PAGE_NOACCESS owner decodes
  bounded increment/non-increment and control packets into a fixed method record
  stream. GET, sink, last method and accepted-stream count commit only after the
  complete PUT range validates. Reserved, truncated, unreadable, looping,
  out-of-range, over-budget and capacity failures preserve prior state and expose
  an exact diagnostic. This is packet intake, not method execution or completion.
- 11b4b1 is verified: because object/class binding is not modeled yet, only
  method `0x0100` on subchannel 0 is accepted as a no-op. Any other method records
  exact subchannel/method/parameter fields and atomically preserves GET, sink,
  last-method and accepted-stream state. No method reaches PGRAPH/D3D11 and no
  completion or interrupt state changes.
- 11b4b2 is verified only for the fixture contract: an explicitly enabled
  handle-to-class lookup lets transactional SET_OBJECT bind NV097 per subchannel.
  Bound clip H/V accept their complete packed two-16-bit-field domain. Format,
  pitch, color offset and zeta offset remain unsupported and roll back the full
  stream. The real game path still needs RAMIN/DMA object lookup; the fixture
  seam cannot satisfy that acceptance boundary.
- 11b4b3 production SET_OBJECT walks claimed PRAMIN RAMHT. The hash is the
  original `0x001945D6` 11-bit fold. The class is the low byte of the RAMIN
  object at `tag<<4`. The fixture binding is opt-in for 11b4b2 tests. Kick
  `0x001918E0` remains fatal.
- 11c1 now has two reviewed recovered leaves: `0x0019460A` performs the exact
  NV2A-base, PCI-command and interrupt-disable writes; `0x00194635` reads the
  modeled PCI identity/revision and visible-memory size into the guest context.
  Their generated wrappers pass a direct 19-check ABI/MMIO fixture. The parent
  `0x00194ADD` remains fatal and neither leaf establishes RAMIN/RAMHT state.
- Implement bounded PFIFO/USER submission with observed packet formats and
  explicit unsupported-method failures. A stalled command must not advance its
  completion signal. Only execute/acknowledge commands with implemented effects.
- Deliver ISR/DPC callbacks with verified guest stack/register/IRQL isolation.
- Recover the remaining setup code and require 192090 to return with actual
  initialized state before moving to a real game clear/present.
- Dumps now include the separately backed physical window and GPU aperture.
  Once registers become trapped, add a frozen model-state snapshot; a skipped
  unreadable aperture is explicit and must not be mistaken for captured state.
