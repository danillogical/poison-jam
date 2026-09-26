# A4b1-r4: Session verification of the Worker's steps 2–6 implementation

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Worker:** child `f9d0e439-58ea-4819-8317-94a965d6a1e9`, `workbuddy-ai/deepseek-v4.1-flash` @ `max`
(contract role, §2.2). It implemented steps 2–6 uncommitted in toolkit `src/apu/**`.
**Scripts:** `logs/a4b1/verify-acport.py`, `check-halt.py`, `check-realtime.py`, `check-structural.py`,
`check-ds3-ds5.py`.

The Session **reproduced the load-bearing claims itself** rather than accepting the Worker's summary.
Everything below is **MEASURED by the Session**.

## Build and tests — reproduced, not taken on trust

| Check | Result |
|---|---|
| `python -X utf8 scripts\build-jsrf.py` | **"Build succeeded (success)"**, 8 s, regeneration disabled |
| exe SHA-256 | **`CD9038188287C0A6CB9A7AFA161ECAD17DEE73560735D66DF9DC6D970C77412A`** — **matches the Worker's report exactly** |
| `ctest --test-dir build -C Release` | **100% tests passed out of 12** |
| `ctest -N` | **12** — the baseline count; the fixture adds 2 more at step 8, as `AC-PORT` requires |

## `AC-PORT` — the two searches, **with the positive control the criterion demands**

The criterion (`docs/packets/a4b1-gp-core-port.md:259`) says: *"the `A4s` baseline exe is the known-good
for the string search, since the string is present in it."* **An absence check without that control
proves nothing** — a broken search also returns nothing — so the control was run first.

| Check | Result |
|---|---|
| **Positive control** — `RECOMP_APU_DSP_ACK` in the archived `A4s` baseline exe | **1 occurrence** (15181824 bytes) → **control SATISFIED** |
| Search 1 — `git grep -E 'RECOMP_APU_DSP_ACK\|dsp_ack_'` over `src/apu` | **0 hits** |
| Search 2 — `RECOMP_APU_DSP_ACK` in the post-port exe | **0 occurrences** (15330304 bytes) |
| `git ls-files --eol src/apu/dsp` | **18 files, all `attr/-text`** |
| Every vendored file byte-exact **at the vendor commit** | **17/17 match the pin record** |

**A Session error found and corrected here, recorded because it nearly produced a false result.** My
first run of this script globbed `*.exe` in the run directory and picked up **`jsrf_collect.exe`** — the
34 KB collector, which legitimately has no APU strings — so the control reported **False** and the script
flagged the absence as unproven. The control the criterion names is the **baseline `jsrf_recomp.exe`**,
which *is* archived in the `A4s` run directory and *does* contain the string. Fixed; the control now
passes and the zero-hit result is meaningful.

**Local modifications to vendored files:** the **vendor commit** matches the pin for all 17; the
**working tree** shows **5** locally modified (`dsp.c`, `dsp_c.c`, `dsp_internal.h`,
`interp/dsp_cpu.c`, `gp_ep.c`) — permitted, and each must be listed in the pin record (step 7).

## The Worker's flagged items — Session checks

### Item 2: the frame loop is unbounded — **CONFIRMED, and the fixture has TWO escape paths**

The Worker reported that nothing in the pinned interpreter sets `is_idle` except a DSP program writing
bit 0 of peripheral `0xFFFFC4`. **Verified from the pinned source:**

- `emu_stop` (`dsp_emu.c.inc:8010`) and `emu_wait` (`:8118`) are **literal no-ops** — bodies containing
  only a `DPRINTF`;
- the only setter is `dsp_set_halt_requested`, reached from `dsp.c:96-99`: `case 0xFFFFC4: if (value & 1)
  dsp_set_halt_requested(dsp, true);`
- the loop is `do { dsp_run(d->gp.dsp, 1000); } while (!dsp_get_halt_requested(d->gp.dsp) && d->gp.realtime);`
  (`gp_ep.c:625-627`).

**So the loop exits on either `halt_requested` OR `!realtime`** — and `realtime` is set at
`gp_ep.c:36-45` from `g_config.audio.use_dsp`. **Both are available to a fixture**, which means
`AC-FIX` (vi) **can** be bounded and the packet's `UNKNOWN` clause (L336) need not be taken. The Worker
correctly identified the hazard and correctly bounded its own harness; the Session records that the
*fixture* has the same two levers, so **(vi) should be implemented, not declared UNKNOWN**.

### Item 1: `apu_dsp.c` deleted — **CONFIRMED, and all three entry points are the pinned definitions**

`src/apu/apu_dsp.c` no longer exists; `apu_mixdown.c` holds the surviving EP monitor mixdown. The three
entry points are defined in the pinned files:

| Entry point | Definition |
|---|---|
| `mcpx_apu_dsp_frame` | **`src/apu/dsp/gp_ep.c:599`** |
| `mcpx_apu_update_dsp_preference` | **`src/apu/dsp/gp_ep.c:31`** |
| `mcpx_apu_play_test_tone` | `src/apu/apu_core.c:749` (toolkit-side, pre-existing) |

So the port replaced the synthetic frame path with the pinned one, which is what `DS2` requires. **The
Session accepts the deletion**: `DS2` + `DS4` left `apu_dsp.c` an empty translation unit, and keeping a
file with no purpose is worse than removing it. The path is in git if it is ever wanted.

### Item 6: the mixbin passthrough placement — **CONFIRMED and the reasoning is right**

`gp_ep.c:641-647` places `mcpx_apu_monitor_mixdown(d, mixbins)` **outside** the GP branch, with a comment
citing `DS2` (*"the EP keeps the existing mixbin passthrough"*). Upstream performed this inside its
`MON_GP` block. **The Worker's choice is correct**: putting it inside the GP branch would silence this
toolkit's audio until the guest enabled the GP — a behaviour change `DS2` does not authorize.

### Item 8: no APU write path bypasses the choke point — **CONFIRMED**

| Evidence | Result |
|---|---|
| `apu_gp_dma_write` definition | `apu_watch.c:385` |
| **call sites** | **exactly one: `src/apu/dsp/gp_ep.c:144`** |
| other writers (permitted by the packet) | `FEMEMDATA` (2), VP `stl_le_phys` (5), `monitor.frame_buf` (18 — host monitor storage, not guest memory) |

### Items 4 and 5: the two resolved ambiguities — **Session accepts both**

- **`src` parameter added to the choke point.** The packet's `DS5` lists three parameters but also
  requires *"the payload for that dword"*, which is not among them. Adding `const uint8_t *src` is the
  minimal reading that satisfies both sentences, and the code comments the reasoning. **Accepted.**
- **`translated_va` out-parameter on `apu_guest_dma_ptr`.** `DS3` names one function; `DS6` needs the
  translated address (for the DMA region class and the `W_va` comparison). An out-parameter keeps
  **one** translation function, whereas a second accessor could disagree with it — which is exactly what
  `DS3`'s "exactly one" requirement exists to prevent. **Accepted.**

## `DS3` and `DS5` — read line by line against the ruling

**`DS3` (`apu_watch.c:306-349`) implements the Advisor's four-case inverse in the mandated order:**

1. **window-VA first** (`:319-324`) — `addr >= XBOX_CONTIG_BASE && addr + len <= base + SIZE` → used as is;
2. **high-water** (`:326-333`) — `high_water != 0 && addr < high_water && addr + len <= XBOX_CONTIG_SIZE`
   → `XBOX_CONTIG_BASE + addr`, **and increments `gpdma_ambiguous` when the range is also mapped low RAM**;
3. **mapped low-RAM identity** (`:336-338`);
4. **fail closed** (`:340-343`) — `note_unmapped(addr, len)`, return NULL.

The comment block (`:255-305`) **cites `dma_resolve` (`nv2a_pb_exec.c:88`) and
`xbox_memory_layout.c:2694`**, states the ordering is load-bearing and *why* (the double-translation
example `0x8000A6C0` → `0x8000A6C0 + 0x80000000`), explains that **`surface_hits_image` is not imported**
and why, and states the **`& 0x03FFFFFF` prohibition** with the reason. The claim limit is recorded.

**`DS5` (`apu_watch.c:385-...`) implements the CAS sequence exactly**, including the D1 retry loop
(`:432-453`): `o = CAS(wp, 0, 3)`; `o == 3` → classify `observed=3`; `o == 0` → classify `observed=0`;
otherwise `o2 = CAS(wp, 0, o)` and repeat from 1. The dword at `W_va` is **never ordinary-stored**
(`:456-467` writes only the head and tail around it), so guest memory ends up identical to a plain write.

### A Session checker error, recorded

`check-ds3-ds5.py` reported **four FAILs** on `DS3` (`surface_hits_image` "imported", `0x03FFFFFF`
"used", `:843` "not cited", case-1 ordering "wrong"). **All four were the checker's own crude greps
matching explanatory comments that say the opposite** — the comment *"surface_hits_image() … is NOT
imported here"* contains the symbol; *"No & 0x03FFFFFF anywhere in this function"* contains the mask;
and the ordering check compared line numbers of a comment mention against the code. Reading the function
settled it. **This is the same over-broad-text-match failure mode recorded repeatedly in `A4s`, and it is
now the fourth occurrence: the lesson is that a grep is not a check for a semantic requirement.**
