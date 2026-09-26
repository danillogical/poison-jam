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

---

## Steps 2–6 — the port: **COMPLETE**, implemented by a Worker, verified by the Session

**Worker:** child `f9d0e439-58ea-4819-8317-94a965d6a1e9`, `workbuddy-ai/deepseek-v4.1-flash` @ `max`.
**Commit:** `8f8f6e4` (24 files, +2492/−485). Full verification record:
`docs/reviews/a4b1-r4-implementation-verification.md`.

**The Session reproduced the load-bearing claims rather than accepting the Worker's summary:**

| Check | Result |
|---|---|
| `python -X utf8 scripts\build-jsrf.py` | **succeeded**, 8 s |
| exe SHA-256 | `CD9038188287C0A6CB9A7AFA161ECAD17DEE73560735D66DF9DC6D970C77412A` — **matches the Worker's report exactly** |
| `ctest` | **100% passed out of 12** |
| `AC-PORT` step 2 — `git ls-files --eol src/apu/dsp` | **18 files, all `attr/-text`** |
| `AC-PORT` step 3 — every vendored file byte-exact **at the vendor commit** | **17/17 match the pin** |
| `AC-PORT` step 6 — `git grep` over `src/apu` | **0 hits** |
| `AC-PORT` step 6 — the string in the exe | **0 occurrences** |
| `AC-PORT` step 6 — **positive control** | the archived `A4s` baseline `jsrf_recomp.exe` contains `RECOMP_APU_DSP_ACK` **once** → **control SATISFIED** |

**The positive control mattered and nearly produced a false result.** The first run of the Session's
checker globbed `*.exe` in the run directory and picked up **`jsrf_collect.exe`** — the 34 KB collector,
which legitimately has no APU strings — so the control reported **False** and the script correctly
refused to treat the zero-hit result as proven. The control the criterion names is the baseline
**recompiler**, which is archived in the `A4s` run directory and does contain the string. Fixed.

**Local modifications:** the **vendor commit** matches the pin for all 17 files; the **working tree**
shows **5** locally modified (`dsp.c`, `dsp_c.c`, `dsp_internal.h`, `interp/dsp_cpu.c`, `gp_ep.c`),
carrying **25 in-source `A4b1 LOCAL MODIFICATION` markers**. The first real build blocker was that the
pinned `gp_ep.c` uses **GNU case ranges** (`case A ... B:`), which MSVC rejects, so all four MMIO
switches were rewritten as `if`/`else if` with identical tests, order and bodies.

**`DS3` was read line by line against the Advisor's ruling** (`apu_watch.c:306-349`): the four cases in
the **mandated order** — window-VA first, then the high-water mark via `xbox_ContiguousAllocatedBytes()`,
then mapped low-RAM identity, then fail closed — citing `dma_resolve` and `xbox_memory_layout.c:2694`,
**not** importing `surface_hits_image`, and **not** using `& 0x03FFFFFF`, with `GPDMA_AMBIGUOUS` and the
aliasing claim limit. **`DS5`** (`apu_watch.c:385+`) implements the exact CAS sequence including the D1
retry loop, and the dword at `W_va` is **never ordinary-stored**.

**One Worker decision the Session had to make:** the Worker **deleted `src/apu/apu_dsp.c`**. `DS2`+`DS4`
had left it an empty translation unit (all three entry points are now the pinned `gp_ep.c` definitions;
the ack is gone), and its surviving EP monitor mixdown moved to `apu_mixdown.c`. **The Session accepts
the deletion** — a file with no purpose is worse than no file, and the path is in git if ever wanted.

**Two Worker-reported ambiguities, both accepted:** the choke point gained a `const uint8_t *src`
parameter (`DS5` names three parameters but also requires "the payload for that dword"), and
`apu_guest_dma_ptr` gained a `translated_va` out-parameter (`DS6` needs the translated address out, and
a second accessor could disagree with the one translation function `DS3` requires).

**A Session checker error, recorded:** a crude grep-based script reported four `DS3` failures
(`surface_hits_image` "imported", `0x03FFFFFF` "used", `:843` "not cited", case-1 ordering "wrong").
**All four were the checker matching explanatory comments that say the opposite** — the comment
*"surface_hits_image() … is NOT imported here"* contains the symbol, and *"No & 0x03FFFFFF anywhere in
this function"* contains the mask. Reading the function settled it. **A grep is not a check for a
semantic requirement** — the fourth occurrence of this failure mode in the project.

---

## Step 7 — licence bookkeeping: **COMPLETE** (commit `772d723`)

| Deliverable | Result |
|---|---|
| `LICENSES/GPL-2.0.txt` | **added** — verbatim GPL v2 from `https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt`, 17984 bytes |
| `NOTICE` GPL-2.0-or-later section | **added** — each ported file under its **own header's** licence with that header's copyright lines, **read from the files** rather than assumed; the Hatari/ARAnyM lineage is named; `trace.h` recorded as header-less |
| `NOTICE` combined-work statement | **added** — the MIT licence covers this project's own code, **not** the whole binary; a binary linking `xbox_apu` is a **GPL-2.0-or-later combined work**, and the LGPL relinking permission does **not** extend to it |
| `NOTICE` LGPL section | **corrected** for the new file set — `apu_dsp.c` removed, `apu_mixdown.c` recorded as its surviving part, and `apu_watch.*`/`dsp/shim/` noted as this project's own new work |
| `LICENSES/README.md` | **names both texts** and which files each governs |
| `a4b-xemu-pin.md` local-modification list | **added** — the **25** markers across **5** files, extracted from the source (`logs/a4b1/extract-modifications.py`), with what each changes and why |

**Build re-verified green after these changes.**
