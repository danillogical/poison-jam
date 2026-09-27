# `A4b2-r8` execution — preconditions P1–P4 (all PASS)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-gp-clears-pending-word.md`, revision **`A4b2-r8`**, frozen SHA-256
**`4D4AFC304F571971EB180C19D6832D56A2CC62FF6EAAFB9E4928716125397C62`** — verified before execution, **not
edited**. **Build/run owner:** Session. **Write scope:** game only.

---

## P1 — XBE and reachability boundary — **PASS**

| Check | Value |
|---|---|
| `Get-FileHash game\default.xbe -Algorithm SHA256` | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` |
| Required (A4p-r1 `O-GATE`) | identical |

**Exact match.** The `A4p` caveats carried by the packet stand unchanged: the 28 direct reads of
`0xFE820010` are threshold re-polls with no use of the polled value; the finding rests on an inferred
register-convention premise checked at its boundaries, is blind to register-indirect/computed accesses
beyond E3, and establishes neither timing nor a true hardware `0x80` value.

## P2 — accepted implementation and exact decision fields — **PASS**

| Check | Value |
|---|---|
| `A4b1-r4` accepted identity | `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d` — exists, is an **ancestor** of HEAD |
| **Required r8 toolkit** | **`c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`** (the discovery-final commit) |
| Actual toolkit HEAD | **`c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`** — **MATCH** |
| Toolkit tree | **clean** |

**The packet's condition is met exactly**: the build identity is the **discovery-final** toolkit, **not**
the earlier accepted commit and **not** a later `HEAD`. **No source-delta analysis is needed and none is
attempted.** The accepted `A4b1` observer map persists at this commit (verified in P3's positive controls
and by the packet's own citation of the accepted identity).

## P3 — EP cannot impersonate a GP clear at the build identity — **PASS**

Command (exactly as specified, with the verified full SHA):

```
git -C C:\Users\logic\Repos\xboxrecomp grep -n -E 'ep_ops|ep\.regs\[|0x50000' c151d4e32a782e4e5adcecbc68afe61ed5fc7e52 -- src/apu
```

**14 hits, all read and classified. Every one is a permitted form:**

| # | Site | Form | Permitted? |
|---|---|---|---|
| 1 | `apu_core.c:30` | comment mentioning `gp_ops/ep_ops` | **yes** — comment |
| 2 | `apu_state.h:374` | comment | **yes** — comment |
| 3 | `gp_ep.c:586` | `const MemoryRegionOps ep_ops = {` | **yes** — the definition |
| 4 | `gp_ep.h:48` | `extern const MemoryRegionOps ep_ops;` | **yes** — the declaration |
| 5 | `gp_ep.c:544` | `r = d->ep.regs[addr];` | **yes** — a **read**, inside `ep_read` |
| 6–8 | `gp_ep.c:576`, `577`, `580` | `d->ep.regs[...] = ...` | **yes** — **writes, all inside `ep_write`** (starts `:551`) |
| 9–12 | `gp_ep.c:630`, `631`, `668`, `669` | EPRST **reads** in the gate conditions | **yes** — reads |
| 13 | `apu_core.c:648` | `/* EP (0x50000) stays unrouted: ... */` | **yes** — **comment, not an arm** |
| 14 | `apu_core.c:674` | `/* ... the EP (0x50000) still has no model. ... */` | **yes** — **comment, not an arm** |

**Positive controls, all visible:**

- the `ep_ops` **definition** (`gp_ep.c:586`) — visible;
- the `ep_ops` **declaration** (`gp_ep.h:48`) — visible;
- the `ep_ops` **comments** (`apu_core.c:30`, `apu_state.h:374`) — visible;
- **`ep_write`'s EPRST write** (`gp_ep.c:576` `proc_rst_write(d->ep.dsp, d->ep.regs[NV_PAPU_EPRST], val)` and `:577`) — visible.

**`ep_ops` is never registered or called.** There is no `&ep_ops` and no `memory_region_init_io` with it
anywhere in `src/apu` — the grep for `&ep_ops|ep_ops)` returns **empty**.

**`apu_core.c` has no `0x50000` EP routing arm.** Both dispatch functions were read in full:
`mcpx_apu_dispatch_mmio` routes `0x20000–0x2FFFF` (VP) and `0x30000–0x3FFFF` (GP) and `< 0x20000` (main
registers), then ends with the explicit comment that EP stays unrouted;
`mcpx_apu_mmio_read_quiet` mirrors the same three arms and returns 0 otherwise.

**`MCPXAPUState` is `calloc`'d** (`apu_core.c:543`) — so `ep.regs` starts zeroed. **EP execution is
EPRST-gated** (`gp_ep.c:668-669`). **Unrouted MMIO plus no caller therefore leaves EPRST unset**, and an EP
exchange cannot reach the shared DMA choke point to falsely latch `GP_CLEAR` as GP work.

**Build identities are the same commit** (P2's `c151d4e`), so the check applies to the actual build.

## P4 — discovery transfer / causal boundary — **PASS**

### P4.1 — manifest hashes verified against the files

| Check | Result |
|---|---|
| Input artifacts hashed and compared | **9 ok, 0 missing, 0 mismatched** |
| `SCRIPT` entries in manifest | **8** |
| Scripts on disk | **8** |
| Outputs on disk | **8** |
| Any nonzero script exit recorded | **none** |

### P4.2 — the archived latch/ordinal artifact

`independent-block24.out.txt` contains the latch line
`[GPDMADESC] GP_CLEAR produced by block_addr=0018 (dsp_addr=000800)`, **and that same line is present in the
original run log** (`20260927-145605-040-a4b2-nr-blocklink/jsrf_run.log`). **The archived output and the
original artifact agree.**

### P4.3 — `O-TWO-LEG` against the terminal ruling and the first-exchange V2 header

- The ruling contains verbatim: **`SELECT O-TWO-LEG upon V2 clean`** — present.
- The V2 result records **CLEAN**.
- **The first-exchange header**, extracted from the block after the `first-exchange` terminal:

```
# exec_total=37895 gp_exec_total=37895 ngp_exec_total=0 first_pc=0000 is_gp_seen=1
  gp_pc_range=0000..0172 ngp_pc_range=0000..0000 gp_first_high=0000
```

**`gp_pc_range` high = `0x0172`** (must be < `0x173` — **yes**) and **`gp_first_high = 0000`** (must be
`0000` — **yes**). So at the exchange **only image `I` had executed**.

### P4.4 — `L2`, the leaf table, and the identity

`L2 = INVARIANT` is recorded in `a4b2-nonreliance-discovery-evidence.md`; the erratum is present and
**radix-corrected** (`block 24`, `%04X`). **The superseded `P 0007` identity is not used** — it is
explicitly retired in the erratum and in r8's own text.

### P4.5 — unchanged XBE and the six CPU observation sites

| Check | Result |
|---|---|
| XBE | P1's exact match |
| `src/diagnostics.c` forwarder | present: `jsrf_watch_store` with the one-shot `apu_watch_set_anchor_site(0x001A18CEu)` then `apu_watch_cpu_store` |
| `extern` declarations | `recomp_0000.c:12`, `recomp_0005.c:12` |
| **Six call sites** | `recomp_0000.c:135287` (`0x0006DACE`, `eax + 0x810`, `ecx`), `135411` (`0x0006DBBA`, `ecx + 0x810`, `eax`), `135877` (`0x0006DEF3`, `ecx + 0x810`, `eax`); `recomp_0005.c:6528` (`0x001A1751`, `edi + 0x810`, `0`), `6751` (`0x001A18CE`, `ebx`, `eax`), `8094` (`0x001A1FA7`, `edi + 0x10`, `ebp`) |
| Required comment | `A4b observation — re-apply after regeneration` present at each |

**All six sites are the specified ones with the specified VAs and values.** The step-1 edits are
**already applied and match the specification**, so per the packet they are **preserved rather than
duplicated**.

## P4's own boundary conditions, restated so they are not silently transported

- The discovery latch is **not** a new-r8 runtime measurement, and **a tuple match alone cannot establish
  block identity** — the new R1 must itself bind its clear VA, tuple and image under `AC-BOOT`/`AC-CLEAR`.
- **Descriptor-region counters and DMA chain-restart theory are NOT used** to bridge identity.
- If the observed tuple or region no longer matches the proved exchange, `AC-INPUTS` is **UNKNOWN**, not a
  blanket waiver.

**All four preconditions PASS. Proceeding to the strict runs.**
