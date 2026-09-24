# Hardware-model evidence: AC'97 primary-codec-ready bit

**Model:** AC'97 primary-codec-ready, `GLOB_STA` bit 8, guest VA `0xFEC00130`.

**Purpose.** Satisfy the evidence rule in `docs/jsrf-run-profiles.md` §"Unconditional modeled
hardware causes": either one credible primary hardware source, or **at least two independent
corroborating secondary sources of meaningfully different provenance**, with at least one
specific to Xbox/MCPX or a demonstrably relevant implementation, all independent of our own
`xboxrecomp` implementation, with exact source/version/commit/line evidence recorded.

This record is **model-scoped, not packet-scoped**: it documents the hardware, and any packet
modelling this bit may cite it.

**Result: the secondary-source path is satisfied.** Two independent implementations of the same
register interface agree, and one of them is the reference **Xbox** emulator.

**No primary source was found.** No public MCPX/ACI datasheet exists; the Intel ICH and AC'97 2.x
specifications are primary for *ICH* semantics but only **analogous** for MCPX. This is recorded
as a known gap, and the policy admits the secondary path explicitly.

**Guest behaviour is recorded separately and is NOT counted as one of the two sources** (per the
owner's rule): see §4.

---

## 1. Source A — xemu, the reference Xbox emulator (Xbox/MCPX-specific)

xemu is an independent open-source Xbox emulator. Its AC'97 device model is a QEMU/VirtualBox
lineage file (`Copyright (C) 2006 InnoTek Systemberatung GmbH`, VirtualBox OSE), and its
MCPX Audio Codec Interface wrapper is Xbox-specific.

| Item | Value |
|---|---|
| Repository | `https://github.com/xemu-project/xemu` |
| `hw/audio/ac97.c` commit | **`2799183ecc5119269be01c340d0c9465dbb04d02`** ("hw/audio/ac97: Use format macros in dolog messages", 2026-01-19) |
| `hw/audio/ac97.c` SHA-256 | `BD2A95FDD9F90F0C7391FD0C83EBC7768179296A50B6A056839A22EC49EE554B` (1,395 lines) |
| `hw/xbox/mcpx/aci.c` commit | **`704ece9ac661f325aa51bb0b28d326063633227b`** ("Merge QEMU v10.2.0", 2026-01-18) |
| `hw/xbox/mcpx/aci.c` SHA-256 | `9A7848267F709AE0BD1DED57C8803C551C09E69F28EB0798A51EC9D1199986D2` (113 lines) |
| `hw/xbox/xbox.c` commit | `704ece9ac661f325aa51bb0b28d326063633227b` |

**Exact lines (`hw/audio/ac97.c`):**

```text
  65: #define GS_S0CR  (1 << 8)       /* ro */
 135:     GLOB_STA = 0x30,
 780:     case GLOB_CNT:
 784:     case GLOB_STA:
 785:         val = s->glob_sta | GS_S0CR;
```

**Exact lines (`hw/xbox/mcpx/aci.c`) — the Xbox register map:**

```text
  48:     memory_region_init(&d->mmio, OBJECT(dev), "mcpx-aci-mmio", 0x1000);
  64:     memory_region_add_subregion(&d->mmio, 0x0, &d->io_nam);
  65:     memory_region_add_subregion(&d->mmio, 0x100, &d->io_nabm);
```

**Exact line (`hw/xbox/xbox.c`) — the Xbox instantiates this device:**

```text
 333:     /* ACI! */
 334:     pci_create_simple(pci_bus, PCI_DEVFN(6, 0), "mcpx-aci");
```

**What this establishes.** `GLOB_STA` (offset `0x30` in the NABM block) has a **read-only bit 8**
named `GS_S0CR` — the *primary codec ready* status — and xemu's `GLOB_STA` read returns it
**unconditionally** (`s->glob_sta | GS_S0CR`), i.e. the reference Xbox emulator reports the
primary codec as ready without any modelled handshake.

**Verified negative control on the `XBOX` conditional.** `hw/audio/ac97.c` contains `#ifdef XBOX`
/ `#ifndef XBOX` at lines 154, 225, 235, 305, 329, 348, 360, 376, 386, 431, 997, 1110. **None
encloses line 785**, so the unconditional `GS_S0CR` report is not an Xbox-build-specific
divergence — it holds in the generic build too, and the Xbox-specific device is wired to exactly
this implementation.

**Register arithmetic (derived from the lines above, not assumed):**

```text
NABM base 0x100 + GLOB_CNT 0x2C = 0x12C      -> MCPX ACI BAR + 0x12C = 0xFEC0012C
NABM base 0x100 + GLOB_STA 0x30 = 0x130      -> MCPX ACI BAR + 0x130 = 0xFEC00130
```

with the MCPX ACI BAR at `0xFEC00000` — the address the guest's own PCI enumeration assigns, and
the address the title uses. Our toolkit independently records the same base
(`xboxrecomp/src/kernel/xbox_memory_layout.c:1554`, `0xFEC00000  AC97`).

---

## 2. Source B — Linux ALSA `intel8x0` (Intel ICH), independent provenance

A different project, language, license and hardware lineage: the OS driver for Intel ICH
southbridges, which implement the same AC'97 controller interface that MCPX reproduces.

| Item | Value |
|---|---|
| Repository | `https://github.com/torvalds/linux` |
| Version | tag **`v6.6`** |
| Path | `sound/pci/intel8x0.c` |
| SHA-256 | `F5F1AE46661C848CCD29C2B1368DB38A159EC8650209E54FB2974996F50B5FFC` (3,207 lines) |

**Exact lines — the register map and the bit's meaning:**

```text
 119: #define ICH_REG_GLOB_CNT		0x2c	/* dword - global control */
 138: #define   ICH_ACLINK		0x00000008	/* AClink shut off */
 140: #define   ICH_AC97COLD		0x00000002	/* AC'97 cold reset */
 142: #define ICH_REG_GLOB_STA		0x30	/* dword - global status */
 163: #define   ICH_PCR		0x00000100	/* primary (AC_SDIN0) codec ready */
```

`ICH_PCR` is bit 8 of `GLOB_STA` (`0x00000100`), and its documented meaning is **"primary
(AC_SDIN0) codec ready"** — the same bit, at the same offset, with the same meaning as xemu's
`GS_S0CR`. This is the *bit's meaning* established by an independent source.

**Exact lines — the reset is a pulse, and the polarity is active-low:**

```text
2336: 	/* do cold reset - the full ac97 powerdown may leave the controller
2337: 	 * in a warm state but actually it cannot communicate with the codec.
2338: 	 */
2339: 	iputdword(chip, ICHREG(GLOB_CNT), cnt & ~ICH_AC97COLD);   <- write 0
2340: 	cnt = igetdword(chip, ICHREG(GLOB_CNT));
2341: 	udelay(10);
2342: 	iputdword(chip, ICHREG(GLOB_CNT), cnt | ICH_AC97COLD);    <- write 1
2343: 	msleep(1);
```

```text
2360: 	/* finish cold or do warm reset */
2361: 	cnt |= (cnt & ICH_AC97COLD) == 0 ? ICH_AC97COLD : ICH_AC97WARM;
```

```text
2295: 	/* clear the cold-reset bit for the next chance */
2297: 		iputdword(chip, ICHREG(GLOB_CNT),
2298: 			  igetdword(chip, ICHREG(GLOB_CNT)) & ~ICH_AC97COLD);
```

**What this establishes.** L2361 is decisive: when `ICH_AC97COLD` reads **0** the driver **sets**
it in order to *finish* the cold reset. So **0 = reset in progress, 1 = released/running** — bit 1
is an active-low `Cold Reset#`. L2295-2298 corroborates: clearing the bit *re-arms* the reset
"for the next chance", which is only meaningful if 0 means asserted. The driver then **waits for**
`ICH_PCR` (L167's meaning) to report the codec ready — i.e. ready is expected after reset is
released.

---

## 3. Independence assessment (the owner's explicit requirement)

| Requirement | Assessment |
|---|---|
| At least one source specific to Xbox/MCPX or a demonstrably relevant implementation | **Met** — xemu is the reference Xbox emulator; `hw/xbox/mcpx/aci.c` is the Xbox MCPX ACI device and `hw/xbox/xbox.c:334` instantiates it |
| Independent of our own `xboxrecomp` implementation | **Met for the AC'97 path** — our toolkit contains **no AC'97 device model**: no AC'97/ACI register-level implementation, and a filename search over `src/` for `ac97`/`aci` returns nothing. It does contain AC'97 **constants and an override block** (`xbox_memory_layout.c:1580-1620`: `MCPX_AC97_CODEC_STATUS`/`MCPX_AC97_CODEC_READY` and the `RECOMP_AC97_READY` setter), which is exactly the shortcut this model replaces — but that block **asserts** a bit and cites no hardware, so it is not a source and cannot corroborate anything. xemu's `ac97.c` is a QEMU/VirtualBox lineage file we did not author |
| Meaningfully different provenance between the two | **Met** — an Xbox emulator's device model vs. the Linux kernel's OS driver for Intel ICH hardware; different projects, licenses, languages and target hardware |
| Exact source/version/commit/line evidence recorded | **Met** — §1 and §2 above |
| Guest behaviour counted as a source | **No** — recorded separately in §4 |

> **Honest caveats, recorded because they qualify the independence claim.**
>
> 1. **Shared lineage in general.** `xboxrecomp` **does** extract other subsystems from xemu —
>    `src/apu/*` ("Extracted from xemu (LGPL v2+), adapted for standalone use"), `src/nv2a/*`, and
>    parts of `src/d3d/*`. So the two projects share a lineage **in general**. They do **not**
>    share the AC'97 path: the toolkit has no AC'97 device model, and the bit semantics cited here
>    come from xemu's own AC'97 device, not from anything we contributed or modified. The claim is
>    therefore scoped to the modelled subsystem, and stated that way rather than asserted globally.
> 2. **What each source actually establishes, and its limit.** xemu's **Xbox-specific** part is
>    the *address map* — `hw/xbox/mcpx/aci.c` placing NAM at `+0x0` and NABM at `+0x100`, which is
>    what puts `GLOB_STA` at `0xFEC00130`. The **bit-8 behaviour** in `hw/audio/ac97.c` is generic
>    QEMU/VirtualBox code, not Xbox-specific, and its `GLOB_CNT` write handler carries an explicit
>    `/* TODO: Handle WR or CR being set (warm/cold reset requests) */` — an *unmodelled* feature.
>    Both sources ultimately descend from the Intel AC'97/ICH specification rather than from an
>    independent measurement of MCPX silicon. **This is why the admission rests on the owner's
>    secondary-source rule and not on a claim of primary evidence**, and why the policy section
>    records the absent-datasheet limitation explicitly.

---

## 4. Guest behaviour (corroboration only — NOT counted as a source)

The title's own code is consistent with the model, and is recorded here as corroboration:

- `sub_001A6C94` reads `GC` (`0xFEC0012C`), and if bit 1 is clear it **sets** it (`or eax, 2`),
  then clears bits 2-3 (`and eax, 0xFFFFFFF3`; bit 3 is `ACLINK` shut-off, so clearing it turns
  the link **on**), and then polls `GS` (`0xFEC00130`) bit 8 up to 1000 times.
- Under the active-low reading this is coherent: assert-then-release reset, enable the link, wait
  for ready. Under an active-high reading it would hold the codec in reset and then wait for it
  to report ready — incoherent for code that works on retail hardware.
- Linux carries a module parameter `xbox`, *"Set to 1 for Xbox, if you have problems with the
  AC'97 codec detection"* — implying an Xbox exposes an AC'97 codec detectable through this bit.

**This is `INFERRED` corroboration from guest code and is explicitly not one of the two
independent hardware sources.**

---

## 5. What these sources do and do not establish

**Establish:**

- `0xFEC0012C` = AC'97 NABM `GLOB_CNT` (`0x100 + 0x2C`); `0xFEC00130` = `GLOB_STA` (`0x100 + 0x30`).
- `GLOB_STA` bit 8 (`0x100`) is the **primary codec ready** status, read-only.
- `GLOB_CNT` bit 1 (`0x2`) is `Cold Reset#`, **active-low**.
- On retail hardware a codec is **present** and becomes **ready** once reset is released.
- The reference Xbox emulator reports that ready bit **unconditionally**.

**Do NOT establish:**

- That `GS.bit8` *follows* `GC.bit1` as a level. **No source states that rule.** It is a stated
  modelling assumption with a named falsifier, and the packet says so.
- Any MCPX-specific timing, reset latency, or electrical detail. The MCPX ACI is *not* an Intel
  ICH; the Intel documents are primary for ICH semantics and only **analogous** for MCPX.
- Anything about W1C interrupt semantics on this register, or about codec behaviour beyond the
  ready bit.
