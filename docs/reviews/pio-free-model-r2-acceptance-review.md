# `PIO_FREE-model-r2` — Acceptance review (stage 1, `hy4-preview-f`)

**Reviewer role:** Acceptance reviewer, stage 1 (§1 DSH row: `workbuddy-ai/hy4-preview-f` @ high).
**Packet reviewed:** `docs/packets/pio-free-model.md`, revision **`PIO_FREE-model-r2`**.
**Verified packet SHA-256 (this reviewer, `Get-FileHash -Algorithm SHA256`):**
`11D6ECB195D51159785D6E94C98439BD4AA6979FDEE6B8EC79B28A80961DDA9E` — **matches the contract hash**, 36 lines.
**Class:** **discovery** (§5.8). **Acceptance bar applied (§5.8):**
*"reviewer confirms the artifacts exist, match the commands, and select the recorded outcome row."*
**Records reviewed:** `docs/reviews/pio-free-model-execution-evidence.md` (main evidence),
`docs/reviews/pio-free-model-r2-session-closure.md` (closure), plus context
`docs/reviews/pio-free-device-boundary.md` and `docs/reviews/pio-free-strict-horizon.md`.

**Disposition: `ACCEPT`.** Every load-bearing claim was reproduced independently; nothing blocking.

---

## 1. Artifacts exist and match the commands

| Artifact | Exists | Matches the commands |
|---|---|---|
| `docs/reviews/pio-free-model-execution-evidence.md` | yes (226 lines) | yes — indexes Experiments 1–4, the ledger, and the row |
| `docs/reviews/pio-free-model-r2-session-closure.md` | yes (72 lines) | yes — closure discipline table, row justification |
| `docs/reviews/pio-free-device-boundary.md` | yes | yes — Session's prior source/XBE input |
| `docs/reviews/pio-free-strict-horizon.md` | yes | yes — strict-horizon measurement |
| Packet | yes, hash matches | not edited during execution |

**Commands the packet named were run as written** — `inspect-jsrf.py disasm <start> <end>` (two ranges),
`inspect-jsrf.py find 0xFE820010`, XBE hashing, git identity, and generated-source spelling counts. All
produced the recorded values. **No ad-hoc substitution.**

---

## 2. Load-bearing claims — independently verified

### 2.1 Address identity — **AGREED**

| Claim | This reviewer's measurement |
|---|---|
| `0xFE820010` = APU `+0x20010` | `apu_core.c:628-630` dispatch arm `addr >= 0x20000 && addr < 0x30000` → VP; read arm at `:661-662` likewise. APU base `0xFE800000` (`apu_core.c:655`, `apu_mmio_hook.c:26`) |
| = VP `+0x10` | `0xFE820010 - 0xFE800000 - 0x20000 == 0x10` → **`True`** (computed) |
| = `NV1BA0_PIO_FREE` | `apu_regs.h:128`: `#define NV1BA0_PIO_FREE 0x00000010` — **read directly** |

### 2.2 The constant and its comment — **AGREED**

`apu_vp.c:551-562` read verbatim:

```c
uint64_t mcpx_apu_vp_read(void *opaque, hwaddr addr, unsigned int size)
{
    (void)opaque; (void)size;
    switch (addr) {
    case NV1BA0_PIO_FREE:
        return 0x80; /* Always pretend queue is empty */
    default:
        break;
    }
    return 0;
}
```

**Function read in full.** Exactly one non-zero arm; `default:` falls to `return 0`, so **every other VP
offset reads `0`** as claimed.

### 2.3 The synchronous write path — **AGREED**

`apu_vp.c:564-572` read verbatim: `fe_method(d, (uint32_t)addr, (uint32_t)val);` — **inline, no queue, no
asynchronous drain**. `git grep` confirms `mcpx_apu_vp_write` is called from exactly one site
(`apu_core.c:632`) and `mcpx_apu_vp_read` from exactly one (`apu_core.c:662`); **no second VP read or write
path exists**, so "no queue" is a property of the whole model, not just one function.

### 2.4 The two threshold forms — **AGREED, reproduced**

`python -X utf8 scripts\inspect-jsrf.py disasm 0x001A2290 0x001A22B0`:

```text
001A2296 mov      eax, dword ptr [0xfe820010]
001A229B and      eax, 0xfffffffc
001A229E cmp      eax, 4
001A22A1 jb       0x1a2296
```

`python -X utf8 scripts\inspect-jsrf.py disasm 0x001A2A70 0x001A2A98`:

```text
001A2A7F mov      edx, dword ptr [0xfe820010]
001A2A85 shr      edx, 2
001A2A88 cmp      edx, ecx
001A2A8A jb       0x1a2a7f
```

**Both forms reproduced byte-for-byte**: constant `val & ~3` vs `4`, and **variable** `val >> 2` vs register
`ecx` (not a constant). The variable form is preserved as variable in the record — the packet's prohibition
on replacing it with `0x80` is **honoured**. `ecx` is computed at `001A2A7C` (`lea ecx,[eax+eax]`), so the
threshold is genuinely input-dependent.

**Poll-then-push corroborated:** `disasm 0x001A22A3 0x001A22D8` shows `mov dword ptr [0xfe820280], eax`
immediately after the poll — VP `+0x280` = `NV1BA0_PIO_SET_HRTF_HEADROOM` (`apu_regs.h:158`).

### 2.5 The 28-site population and both spellings — **AGREED, all three numbers reproduced**

| Check | Recorded | This reviewer |
|---|---|---|
| `inspect-jsrf.py find 0xFE820010` | 28 | **28 occurrence(s)**, `001A2297` … `001A611D`, all `DSOUND` |
| `MEM32(0xFE820010u)` | 10 | **10** |
| `MEM32(-25034736)` | 18 | **18** |
| Sum | 28 | **28** |

`-25034736 & 0xFFFFFFFF == 0xFE820010` → **`True`** (computed).

**Stronger than the record:** a normalised scan parsing every `MEM8/MEM16/MEM32` integer literal in
`src/recomp/gen/*.c` and reducing mod 2^32 finds **only two spellings** of `0xFE820010` (18 decimal +
10 hex), and **no third spelling** at any address ≥ `0x80000000`. The 10+18 split therefore **exhausts** the
generated population and reconciles one-to-one with the XBE's 28. This closes the §6.1 two-spelling hazard
positively rather than by assertion.

### 2.6 The ledger's five `UNKNOWN` leaves — **AGREED, genuinely unsupported**

I asked whether each is **unsupported by the cited sources**, not merely unstated by the Session. Per
`docs/jsrf-run-profiles.md:245-267`, guest code and our own implementation are **not** admitted sources, so
the only admissible evidence is external documentation — and §2.7 below establishes that no external source
documents any of these. Each leaf is therefore **unsupported on the merits**:

| Leaf | Status | Why genuinely unsupported (not just unstated) |
|---|---|---|
| Units / bit encoding | `UNKNOWN` | only the guest `>> 2` implies units of 4 — **guest code is not an admitted source**; no external source states any encoding |
| Capacity | `UNKNOWN` | no source gives a depth; `0x80>>2=32` is arithmetic on a pretence, not evidence |
| Drain / completion | `UNKNOWN` | the model has **no drain** (`fe_method` is synchronous); hardware drain is unstated by any admitted source |
| Overflow / backpressure | `UNKNOWN` | no model, no source |
| Read-vs-write ordering | `UNKNOWN` | model is synchronous so no hardware ordering is observable; unstated externally |

The three `RESOLVED`/`PARTIAL` leaves are also correct: alias resolves to `apu_regs.h:128`;
`apu_core.c:543` is `calloc(1, sizeof(MCPXAPUState))` (read directly) — a real reset state that the stub
**overrides**, exactly as the record says.

### 2.7 The sourcing inadequacy — **AGREED, and I tried hard to falsify it**

This is the claim that selects the row. I re-fetched and re-counted independently, and searched for sources
the Session may have missed.

**The wiki silence — reproduced with a positive control.** Raw wikitext
`https://xboxdevwiki.net/index.php?title=APU&action=raw`:

| Term | Recorded | This reviewer |
|---|---|---|
| bytes | 14 155 | **14 155** (non-empty → the fetch really landed) |
| `PIO_FREE` | 0 | **0** |
| `queue` | 0 | **0** |
| `free` (any case) | 0 | **0** |
| `depth` | 0 | **0** |
| `NV1BA0` | 2 | **2** |

Page identity confirmed via the MediaWiki API: `revid 7415`, `2025-07-22T20:47:32Z`, `size 14156` —
matching the record's "page rev `7415`, 2025-07-22". I read the page in full: it documents 256 voices,
32 bins, a `0x80`-byte voice structure, envelopes, HRTF, LFO, DLS2, MIXBUF and the GP ringbuffer, and has
**empty `Frontend Engine (FE)` and `Memory map` sections** — corroborating "silence", not omission by
truncation.

**Related pages — reproduced.** `MCPX` (1 684 bytes) and `DSP` (9 655 bytes): `PIO_FREE`=0, `queue`=0,
`free`=0, `NV1BA0`=0. Byte counts match the record exactly.

**Site-wide search — new, and it strengthens the claim.** A full-text wiki search for `PIO_FREE` returns
*"There were no results matching the query"* — **no page anywhere on xboxdevwiki mentions the register**.

**The NVIDIA 404 — partially superseded, conclusion unchanged (advisory).** The record says the 2001 NVIDIA
nForce MCP technical brief *"now returns HTTP 404."* That is true for
`http://www.nvidia.com/attach/9004` (I reproduced **404**), but **an archived copy is retrievable**:
`https://web.archive.org/web/2019id_/http://www.nvidia.com/attach/9004` returns **HTTP 200, 378 982 bytes,
`application/pdf`**. I downloaded it and extracted its text (7 124 strings / 36 275 joined chars — a real
extraction, title reads *"NVIDIA nForce MCP Audio Processing Unit"*). Term counts on the extracted text:
`PIO_FREE`=0, `NV1BA0`=0, `PIO`=0, `queue`/`Queue`/`QUEUE`=0, `free`/`Free`/`FREE`=0, `FIFO`=0, `depth`=0,
`front end`/`frontend`=0, with **`Voice Processor`=21 and `register`=2 as positive controls** proving the
document is the right one and the search terms were live. **So the primary specification exists in archive
form and is nevertheless silent on `PIO_FREE`** — the sourcing conclusion is *unchanged and actually
better-supported* than "404". The record's "404" is inaccurate as to availability but not as to result;
this is **A-1**, advisory (it does not change the row).

**Sources I searched for that the Session did not — all negative (advisory enrichment only):**

- **NVIDIA legacy APU page** `http://www.nvidia.com/object/apu.html` — live (499 261 bytes); `PIO_FREE`=0,
  `NV1BA0`=0, `register`=0.
- **JayFoxRox/xbox-tools** (linked from the wiki as *"Scripts to inspect APU registers"*), the most likely
  missed independent source: `inspect_apu_vp.py` and `trace_apu_mixbuf.py` — **`PIO_FREE`=0, `NV1BA0`=0,
  `queue`=0, `free`=0, `0x80`=0** in both.
- **Cxbx-Reloaded** (a *second, independent* emulator — the strongest candidate for the "two independent
  secondary sources" path): `src/devices/audio/APUDevice.cpp`, `DSOUND/DirectSound/DirectSound.cpp`,
  `DSOUND/common/XbInternalDSVoice.cpp`, `XbDSoundTypes.h` — **`PIO_FREE`=0, `NV1BA0`=0, `queue`=0** in all
  four. **No second emulator documents the register either.**
- **DuckDuckGo html + lite**: *"No results found for `NV1BA0_PIO_FREE`"*. **Bing** (22 hits) and **Brave**
  (21) return only query echoes in meta tags and unrelated Raspberry-Pi `PIO` pages. **Mojeek** 403;
  **Startpage** 0. GitHub code search and grep.app were unavailable (401/429).

**Conclusion:** the sourcing rule (`docs/jsrf-run-profiles.md:245-267`) requires one primary spec **or** two
independent secondary sources. There is **no primary spec that documents the register** (the NVIDIA brief is
retrievable and silent), and the **only** admissible secondary source is silent. **Neither prong is met.**
The inadequacy is real, and my independent searching *reduced* the chance it is a search failure rather
than a genuine gap. **This is what selects `O-UNKNOWN`.**

### 2.8 The negative control — **AGREED, confirmed and correctly excluded**

Fetched `hw/xbox/mcpx/apu/vp/vp.c` from `xemu-project/xemu`. `vp_read` (spill lines 582-597) reads:

```c
switch (addr) {
case NV1BA0_PIO_FREE:
    /* we don't simulate the queue for now,
     * pretend to always be empty */
    return 0x80;
default:
    break;
}
return 0;
```

**Verbatim match**, including the two-line comment. The reading is sound: it states a queue exists, is
unsimulated, and `0x80` is a pretence.

**Correctly excluded as independent support — the ancestry is literal, not assumed.** I verified it:
toolkit `src/apu/README.md:3` says *"extracted from [xemu]"*, and `src/apu/apu_vp.c:2` says *"Standalone
extraction from xemu"*; 35 files under `src/` reference xemu, including a vendored
`src/apu/dsp/shim/hw/xbox/mcpx/apu/apu_regs.h`. So the toolkit's `0x80` **is** xemu's `0x80` — the two are
one provenance, and per §"the sources are independent of our own `xboxrecomp` implementation" it cannot
count. **The exclusion is right and evidence-backed.**

### 2.9 The Session's recorded error — **AGREED, correctly handled**

The record and closure both disclose the spill-file false positive (`PIO_FREE`=1 from a buffer holding xemu
source) and its correction. **This was disclosed, not quietly fixed** (§2.4.1/§2.2.8). The final figure is
computed on the wiki's own raw wikitext, which I reproduced. **Non-blocking.**

---

## 3. Row selection — first-match evaluation of all four rows

| Row | Applicable? | This reviewer's independent basis |
|---|---|---|
| `O-IDENTITY` | **No — AGREED** | XBE `FD190557…3EF9C` matches pinned baseline; toolkit `c151d4e32a782e4e5adcecbc68afe61ed5fc7e52` **clean**; A4p population reconciles (28=10+18); no premise changed. Nothing stale/malformed/unreadable |
| `O-CONFLICT` | **No — AGREED** | see §4 |
| `O-SPEC` | **No — AGREED** | five leaves `UNKNOWN` **and** sourcing unmet — two independent grounds, either sufficient |
| **`O-UNKNOWN`** | **YES — AGREED** | *"inadequate independent sourcing, unresolved unit/drain/reset/alias/variable-threshold leaf or missing coverage"* — both grounds present |

**`O-UNKNOWN` is correctly selected as the first applicable row.** The outcome table's own next action
(a focused `PIO_FREE` source/queue-interface discovery) is recorded.

---

## 4. `O-CONFLICT` rejection — scrutinised, and correct

The row requires *"authenticated independent sources or route/guest evidence give incompatible, concrete
meanings or impossible queue predictions."* The evidence set is: **one admission** (xemu — same provenance
as the toolkit, so excluded) and **one silence** (the wiki; now also NVIDIA-brief silence, Cxbx silence,
xbox-tools silence).

**The rejection is right, and it is right on construction, not just on the facts.** `O-CONFLICT` is a
conjunction requiring **two** sources with **concrete** meanings that are **incompatible**. Silence supplies
no meaning at all, so there is no pair to compare and nothing to be incompatible with. A conflict row
reached from one source plus silence would require treating "says nothing" as "says the opposite" — which
would make `O-CONFLICT` swallow every `O-UNKNOWN`, collapsing the table. The reading preserves the
distinction the outcome table is built on.

**Should this have gone to the Advisor?** No, and §2.2 does not require it. Escalation (§4.2) is triggered
by an **ambiguous or contradicted criterion**, or by two credible measurements contradicting each other.
Here there is **no contradiction and no ambiguity in the criterion's text** — only a **single** source and
silences. Sending it to the Advisor would ask the judgment layer to manufacture a conflict out of an
absence, which §2.4.3 forbids in substance and which would risk a *false* `O-CONFLICT`. The Session's
reasoning is a correct application of the frozen text, not a private reinterpretation.

**I also checked the harder variant:** could the "impossible queue predictions" limb fire from the model's
*synchronous* `fe_method` versus hardware's queue? It cannot — that is a **model-vs-hardware gap**, i.e.
unmodelled behaviour, which is exactly `O-UNKNOWN`'s subject, and the packet expressly warns *"do not …
assert a synchronous queue is hardware truth."* **No Advisor referral was owed.**

---

## 5. `A2h` — correctly NOT named

The `O-UNKNOWN` row names `A2h` **only** *"if the only missing witness requires trapped strict time beyond
the available pre-OOM prefix."* The five missing witnesses are **units, capacity, drain, overflow,
ordering** — all **interface documentation**. No amount of trapped guest execution yields a vendor's
queue-depth figure or drain rule; a run can at best show observed values, and observed guest behaviour "may
corroborate an interpretation but does not count as one of the two independent sources"
(`docs/jsrf-run-profiles.md:260-262`). **The condition is not met, so NOT naming `A2h` is correct**, and the
successor is properly a sourcing/interface discovery rather than a run.

---

## 6. Identity, write scope, and §5.8 closure

| Item | Recorded | This reviewer |
|---|---|---|
| XBE SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` | **match** |
| Toolkit | `c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`, clean | **match; `git status --porcelain` empty (0 entries), incl. no untracked** |
| Game | `d54b5ff` (execution); HEAD `a3300f1` | execution commit is `d54b5ff`; HEAD's only later delta is the evidence/closure/plan documents |

**No toolkit or game code changed** — `git log --name-only` over the last five commits yields **zero** paths
under `src/`, `game/`, `config/` or `scripts/`; the only changed paths are the evidence record, the closure,
and the plan. Working tree is clean (`git status --porcelain --untracked-files=all` → empty). **The
packet's write scope (evidence record only) was honoured.**

**§5.8 closure:** no instrumentation existed, so nothing is enabled at closure; no build, no guest run.
**No strict criterion is claimed and nothing is claimed to work** — stated explicitly in both records.
`0xFFFFB3` stays `UNRESOLVED`; `A4b1-r4` untouched; no toolkit change, so P4's bridge is not re-opened.

---

## 7. Advisories (outside the contract; do not change the disposition)

- **A-1 — the NVIDIA "404" is inaccurate as to availability.** The brief is retrievable from the Wayback
  Machine (`web/2019id_/http://www.nvidia.com/attach/9004`, HTTP 200, 378 982-byte PDF). Its text is silent
  on `PIO_FREE`, so **the sourcing conclusion is unchanged and strengthened** —but the record should say
  "404 live; archived copy retrieved and silent", not "404", so a future reader does not treat the primary
  source as unobtainable. Correcting a non-packet record needs no re-review (§3.2).
- **A-2 — the record's sourcing section is under-specified for reproduction.** It cites "raw wikitext,
  14 155 bytes; page rev `7415`" but not the exact URL used (`?action=raw`). I reproduced it by inference.
  Recording the URL and the MediaWiki API identity query would make the term counts re-runnable verbatim.
- **A-3 — the extracted NVIDIA text is partial.** My extraction recovered 36 275 characters from 36
  decompressible streams; some glyph runs decoded as mojibake. `PIO_FREE`/`queue`/`free`/`depth` were all
  zero with live positive controls, but a cleaner extraction (or a page-by-page read) would firm this up if
  the brief is ever relied on as a primary source.
- **A-4 — minor: the evidence record's `find` range boundary.** The record says the 28 sites run
  "`001A2297` … `001A611D`". My `find` output reproduces exactly that range, but note the site *addresses*
  are `001A2296`/`001A2A7F` (the instruction start); the listed values are the **operand offsets**. This is
  consistent with `A4p` and harmless, but a reader could mistake them for instruction VAs.
- **A-5 — the game HEAD (`a3300f1`) differs from the revision the closure names (`d54b5ff`).** Both are
  documentation-only; the closure records the execution-time revision correctly. Worth a one-line note so
  the reviewed tree is unambiguous (§2.4.7: reviews bind to bytes).

## 8. Blocking issues

**NONE.**

## 9. What would reverse this disposition

- Any **admissible** external source (primary spec, or a second independent secondary source — e.g. a driver
  or an emulator other than xemu) that **documents** `PIO_FREE`'s units, capacity, drain, overflow or
  ordering. If found, `O-SPEC`'s sourcing prong and the affected ledger leaves would both move, and the row
  would require re-selection.
- Evidence that the xboxdevwiki `APU` page **did** document the register at revision `7415` (I measured 0 on
  its raw wikitext and 0 on a site-wide full-text search).
- Any toolkit or game code change, or a change to the XBE/toolkit identity, which would reopen `O-IDENTITY`.

---

**DISPOSITION: `ACCEPT`** — for a discovery packet under §5.8, the artifacts exist, match the packet's
commands, and the recorded outcome row (`O-UNKNOWN`) is correctly selected by first match.
