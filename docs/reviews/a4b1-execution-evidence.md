# A4b1-r4 execution evidence

**Revision:** `A4b1-r4`, frozen SHA-256
`6DD62A57E87445F5C12085210146204AA4E77D26FD316071FD41CAEC76835C38` (445 lines).
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Toolkit baseline at start:** `M` = `3f8bf67c450861aefcbc376698750bc1446bc9dd` (A4s accepted, pushed).

---

## Readiness / `Stop if` gates — all clear

| Gate | Result |
|---|---|
| `A4s` accepted, strict stop is the `loc_001A18D0` spin | **PASS** — `R-SAME`; `B = 0x803C0000`, `W = 3`, `F = 2` |
| `dsp_ack_frame` / `RECOMP_APU_DSP_ACK` present in `src/apu` | **PASS** — `apu_dsp.c:40, 61, 97, 160` |
| GP writes still dropped, reads return 0 | **PASS** — `apu_core.c:618` *"GP (0x30000) and EP (0x50000) regions ignored for now"*; `mcpx_apu_mmio_read_quiet` returns 0 for the GP block |
| Pin record's files at the pinned SHA | **PASS** — all 17 verified, see step 1 |
| No forbidden env vars in the launch environment | verified at the run (step 9) |

---

## Step 1 — Vendor: **COMPLETE**

**Network and pin verified before any write.** All **17** files fetched as **raw bytes** from
`https://raw.githubusercontent.com/xemu-project/xemu/67cc79e663038d1f55448c0f566b37dde016adf6/hw/xbox/mcpx/apu/dsp/`
and their SHA-256 compared against `docs/reviews/a4b-xemu-pin.md`:

| File | Bytes | SHA-256 | Licence (own header) |
|---|---|---|---|
| `dsp.c` | 5163 | `bd40122d…` | GPL-2.0-or-later |
| `dsp.h` | 4478 | `9310695d…` | GPL-2.0-or-later |
| `dsp_c.c` | 9051 | `89ed49bf…` | GPL-2.0-or-later |
| `dsp_internal.h` | 780 | `f63862aa…` | GPL-2.0-or-later |
| `debug.h` | 1016 | `1773c7f3…` | GPL-2.0-or-later |
| `interp/dsp_cpu.c` | 50993 | `b2879f68…` | GPL-2.0-or-later |
| `interp/dsp_cpu.h` | 4434 | `314ab9af…` | GPL-2.0-or-later |
| `interp/dsp_cpu_regs.h` | 3338 | `5373cbe8…` | GPL-2.0-or-later |
| `interp/dsp_emu.c.inc` | 238793 | `ffc744df…` | GPL-2.0-or-later |
| `interp/dsp_dis.c.inc` | 63062 | `2a624a32…` | GPL-2.0-or-later |
| `interp/debug.c` | 10169 | `0a4429c3…` | GPL-2.0-or-later |
| `gp_ep.c` | 17732 | `979044a3…` | LGPL-2.1-or-later |
| `gp_ep.h` | 1625 | `bae4f2be…` | LGPL-2.1-or-later |
| `dsp_dma.c` | 13062 | `4a19ab8e…` | LGPL-2.1-or-later |
| `dsp_dma.h` | 2214 | `117a7db6…` | LGPL-2.1-or-later |
| `dsp_dma_regs.h` | 1542 | `9582796e…` | LGPL-2.1-or-later |
| `trace.h` | 46 | `c4e97d9e…` | no header (bare shim) |

**Total 17 files, 427498 bytes. All hashes match the pin record.**

**Byte-exactness verified three times over:**

1. on the **fetched bytes** (raw `urllib`, not text, so no line-ending re-encoding);
2. on the **written destination files** after copying;
3. on the **staged git blobs** (`git show :<path>`) — **17/17 match**, which is the check that
   would catch a git line-ending transformation.

`git ls-files --eol src/apu/dsp/` reports **`attr/-text`** on every file, so the `.gitattributes`
committed first is doing its job.

**`dsp_jit.*` is absent** (verified by a recursive search) — the JIT backend is a packet non-goal.

**Commits:**

| Commit | Contents |
|---|---|
| `6e8b8b3` | `src/apu/dsp/.gitattributes` — `* -text` (8 bytes: `2a 20 2d 74 65 78 74 0a`) |
| `090682e` | the vendor commit — the 17 pinned files, unmodified |

---

## Step 2 prerequisite — the QEMU shim surface is **measured, not guessed**

The pinned files include QEMU headers that do not exist in this toolkit. The packet requires shims
that "are only no-ops or pass-throughs", so the surface was inventoried rather than assumed
(`logs/a4b1/shim-surface.py`). **The total surface is small:**

| Family | Symbols the pinned code actually uses |
|---|---|
| **QEMU memory/device API** | `address_space_memory`, `MemoryRegion`, `memory_region_size`, `memory_region_set_dirty`, `qemu_mutex_lock`, `qemu_mutex_unlock` |
| **QEMU trace** | `trace_dsp_read_peripheral`, `trace_dsp_write_peripheral`, `trace_dsp56k_execute_instruction`, `trace_dsp56k_execute_instruction_disasm`, `trace_event_get_state` |
| **xemu settings** | `g_config`, `use_dsp_jit` (only to choose the backend — the packet pins `dsp_c_init` unconditionally, so this becomes a no-op) |
| **bswap / endian** | `bswap`, `ldl_le_phys`, `ldl_le_p`, `stl_le_p`, `GET_MASK` |
| **glib allocation** | `g_new0`, `g_free` |
| **logging** | `assert`, `fprintf`, `printf`, `DPRINTF`, `DEBUG_DSP` |

**Per-file QEMU header usage** (which is what the shims must satisfy):

| Pinned file | QEMU headers it includes |
|---|---|
| `dsp.c` | `qemu/osdep.h`, `trace.h`, `ui/xemu-settings.h` |
| `dsp_c.c` | `qemu/osdep.h` |
| `dsp_dma.c` | `qemu/osdep.h`, `qemu/compiler.h` |
| `gp_ep.c` | `hw/xbox/mcpx/apu/apu_int.h` |
| `gp_ep.h` | `qemu/osdep.h`, `hw/hw.h`, `hw/pci/pci.h`, `hw/xbox/mcpx/apu/apu_regs.h` |
| `interp/dsp_cpu.c` | `qemu/osdep.h`, `qemu/bswap.h`, `trace.h` |
| `trace.h` | `trace/trace-hw_xbox_mcpx_apu_dsp.h` |

The pinned code also uses **131 distinct `DSP_*`/`NV_PAPU_*` constants** which this toolkit's
`apu_regs.h` already defines — so the register-file surface needs no new definitions.

**Raw logs:** `logs/a4b1/fetch-pin.py`, `vendor.py`, `shim-surface.py`; fetched bytes under
`logs/a4b1/pin/`.
