# Acceptance review (stage 1) — `A2h-named-producer-frame-r1`

**Reviewer:** acceptance reviewer, stage 1. **Date:** 2026-09-27.
**Packet:** `docs/packets/a2h-named-producer-frame.md`, frozen SHA-256
`2333B6523B39866F1AEB9C1BEFFE1CCE42176E35BA3ADB86F8965A265BF88887`, 37 lines — **re-hashed in this
review and MATCHES**; 37 lines confirmed.
**Evidence under review:** `docs/reviews/a2h-named-producer-frame-evidence.md` (212 lines).
**Class:** discovery (§5.8). The §5.8 bar is applied: artifacts exist / match the commands / the
correct row is selected and supported. **No strict boot/audio/GPU/liveness bar is applied** — nothing
here claims anything works.

**DISPOSITION: ACCEPT** — with three corrections that must be applied to the record before it is
cited (§"Required corrections"). None of them changes the row.

---

## 1. Replay against real bytes — what I ran

| Command (run from the game root) | Result |
|---|---|
| `Get-FileHash -Algorithm SHA256 docs/packets/a2h-named-producer-frame.md` | `2333B652…F88887` — **MATCHES the frozen digest** |
| `python -X utf8 scripts/check-run-profile.py logs/runs/20260927-160330-655-a4b2-gp-trap-trace` | `STRICT` (strict 1) |
| `python -X utf8 scripts/check-dump-mapping.py logs/runs/20260927-160330-655-a4b2-gp-trap-trace` | `CONTENT_MISMATCH`, `matches: 0 content-mismatch: 1` |
| `python -X utf8 scripts/a2h-oom-slice.py --log …\jsrf_run.log` | 94 invocations, `failing_indices: [93]`, 1 OOM, 1 ICALL |
| `python -X utf8 scripts/a2h-frame-audit.py verify` | **ALL VERIFIED** (8 instructions) |
| `python -X utf8 scripts/a2h-frame-audit.py callsites` | 13 sites, tool says "12 three-arg, 1 two-arg" |
| `python -X utf8 scripts/a2h-frame-audit.py frame 0x00F7FCF0 432` | `E=0x00F7FEA0 ebp=0x00F7FE9C arg2=0x00F7FEAC` |
| `python -X utf8 -m unittest scripts.test_a2h_frame_audit` | **Ran 25 tests, OK** |
| `python -X utf8 -m unittest scripts.test_a2h_oom_slice` | **Ran 31 tests, OK** |
| own capstone decodes via `scripts/a2h-oom-slice.py` helpers | callee `0x001497DC`, helper `0x0017D1F8`, caller `0x0017C900`, getters `0x0014A821`/`0x0014A838`, alloc site `0x00149E4A`, callee tail `0x00149F4B` |
| `python -X utf8 scripts/inspect-jsrf.py memory … 0x00F7FE60 0xA0` | the live stack window below |

Both profile/dump commands reproduce the evidence's reported values exactly.

---

## 2. Criterion 1 — artifacts exist: **AGREED**

All four artifacts exist and are non-empty:
`docs/reviews/a2h-named-producer-frame-evidence.md` (212 lines), `scripts/a2h-frame-audit.py` (369),
`scripts/test_a2h_frame_audit.py` (233), and the pre-existing `scripts/a2h-oom-slice.py` /
`scripts/test_a2h_oom_slice.py`. The new suite runs green (25 tests, exit 0); the pre-existing suite
runs green (31 tests, exit 0). Game HEAD `ea98841` = `c914448` + one commit whose diff is exactly
the evidence file, the plan block, and the two new scripts — so the artifacts are committed, not
stray.

## 3. Criterion 2 — artifacts match the commands: **AGREED** (one advisory scope deviation)

The packet's named commands were run and reproduce. The packet's step 1 commands
(`check-run-profile.py`, `check-dump-mapping.py`, reuse of `a2h-oom-slice.py`) are all honoured.

**Advisory deviation (not blocking):** step 2 says *"Extend `scripts/a2h-oom-slice.py` plus
`scripts/test_a2h_oom_slice.py` (rather than creating an ad-hoc parser)"*, and the write scope names
*"the named existing A2h tool and its tests"*. The Session instead added a **new** checked-in tool
plus its own 25-test suite. It is not ad-hoc parsing (it imports and reuses `a2h-oom-slice.py` for
every XBE/verification primitive, and `a2h-oom-slice.py` itself is **unmodified**), it adds no
production code, and it satisfies the step's stated purpose. I record it as a literal scope
deviation for the parent to decide, not as a criterion failure.

## 4. Criterion 3 — correct row selected: **AGREED**; `O-OPEN` is right and no other row is better supported

Checked against the packet's own row definitions at lines 26-31:

* `O-IDENTITY` — correctly rejected. XBE SHA-256 matches; toolkit `c151d4e` clean and unchanged;
  profile STRICT; the failing invocation is rebound uniquely (index 93 of 94, size 598869040
  requested exactly once in the whole log).
* `O-FRAME-ARG` — correctly rejected. It requires *"a proved reaching call-site write to that exact
  `[ebp+0x10]` address"* **and** the actual read/value. No invocation-bound read value exists (§6).
* `O-FRAME-OTHER` — correctly rejected. Same missing witness, and the no-competing-writer result
  (§7) means there is no "other witnessed writer" to attribute to.
* `O-OPEN` — **selected, and correct.** Its triggers include *"coverage gap"* and *"unsafe seam"*.
  Both are present: the failing activation's `arg2` value is not in any artifact, and the only seam
  that sees bridge calls with `esp` is toolkit-side.

I independently verified the `O-OPEN` **core** (which does not depend on the arithmetic corrected in
§5): the log tail is

```
#5553 ordinal 277 esp=0x00F7FD00 ret=0x0014982E      <- activation A, ICALL at 0x00149828 (survived)
#5554 ordinal 184 esp=0x00F7FCF0 ret=0x00149E50      <- activation A, ALLOC 598869040 -> OOM
#5555 ordinal 294 esp=0x00F7FCFC ret=0x00149F5D      <- activation A, tail ICALL in sub_00149F4B
[ICALL] invalid target 0x00000000 esp=00F7FD00 return=0014982E   <- activation B, CRASH
```

`0x00149828` (whose return is `0x0014982E`) precedes `0x00149E4A` in program order inside one
activation, and I confirmed **no branch in `0x001497DC..0x00149F48` targets ≤ `0x00149830`** other
than the internal `jne 0x149808`, so there is no loop that could put the crash back in activation A.
`0x00149F5D` lies in `sub_00149F4B` (`0x00149F4B..0x00149F5E`), outside the callee — so activation A
ran to its tail and returned. **Activation B is a later activation at the same depth
(`esp=0x00F7FD00` identical), which is exactly the "same addresses reused" mechanism the evidence
describes.** The row is supported independently of every number I correct below.

## 5. Frame arithmetic independently recomputed: **I also get `E = 0x00F7FEA0` — AGREED, and more strongly corroborated than the evidence claims**

Verified prologue: `push 0x178` → `E−4`; `push 0x1E0BE8` → `E−8`; `call 0x17D1F8` → `E−12`;
helper `push 0x1804A0` → `E−16`; `push eax` → `E−20`; `lea ebp,[esp+0x10]` → `ebp = E−4`.
Hence `arg0=[E+4]`, `arg1=[E+8]`, **`arg2=[E+12]=0x00F7FEAC`**. Confirmed against the generated
code (`recomp_0004.c:1578 ebp = esp + 0x10` / `:1589 g_seh_ebp = ebp`; `recomp_0003.c:20122
ebp = g_seh_ebp` / `:20132 eax = MEM32(ebp + 0x10)`) and, decisively, against the **guest bytes**:
`0017D213 8d6c2410`, `00149800 8b4510`, `001497DC 6878010000`, `001497E1 68e80b1e00`,
`001497E6 e80d3a0300` — all `ALL VERIFIED` by the tool and re-decoded by me.

* Fact 1: `0x00F7FD00 + 0x1A0 = 0x00F7FEA0` ✓ (post-prologue `esp = E−0x198`; one push + the call
  before the ICALL = `−8` → `E−0x1A0`; I derived `0x198` myself from `sub esp,0x178` + 4 pushes +
  `ret`.)
* Fact 2: `0x00F7FCF0 + 0x1B0 = 0x00F7FEA0` ✓ (alloc site: 5 pushes + call = `0x18` below `E−0x198`).
* Fact 3: `0x00F7FD08 + 0x198 = 0x00F7FEA0` ✓.

**I additionally corroborated `E` from the dump layout**, which the evidence does not do, and it
matches on every slot:

| Address | Dump | Expected under `E = 0x00F7FEA0` |
|---|---|---|
| `E−20` = `0x00F7FE8C` | `1C788B18` | helper's `push eax` (old `fs:[0]`) |
| `E−16` = `0x00F7FE90` | **`001804A0`** | helper's `push 0x1804A0` — **exact** |
| `E−12` = `0x00F7FE94` | **`001E0BE8`** | helper's `mov [ebp-8],eax` relocates the callee's `0x1E0BE8` here — **exact** |
| `E−8`  = `0x00F7FE98` | `00000000` | callee's `and dword [ebp-4],0` (`recomp_0003.c:20147`) — **exact** |
| `E−4`  = `0x00F7FE9C` | `00F7FE4C` | helper's `mov [esp+0x10],ebp` (saved inherited ebp) — plausible stack value |
| `E`      = `0x00F7FEA0` | **`0017C926`** | return address = `call@0x0017C921 + 5` — **exact** |
| `E+12` = `0x00F7FEAC` | `4C000010` | **arg2, the read** |

**Over-claim to correct:** the evidence calls facts 1-3 *"three **independent** facts"*. They are not.
`0x00F7FD00 − 0x00F7FCF0 = 0x10` and `0x1B0 − 0x1A0 = 0x10`, so facts 1 and 2 are the *same*
relation displaced by one slot; fact 3 re-uses fact 1's `esp` reading. They are one consistency
check of the depth model, not three determinations. **The conclusion is still correct** — and the
dump-slot corroboration above (two exact constants plus the return address) makes `E` *better*
supported than the "three facts" framing does.

## 6. Argument counts independently checked: **I falsified the tool — it is 13 of 13, not "12 of 13 with one passing two"**

* **13 direct call sites: CONFIRMED.** Byte-scan for `E8` + verify, cross-checked against the
  recompiler's own list — 8 `RECOMP_ABI_CALL` in `gen/` and 6 in `recovered/recovered.c`,
  overlapping on `0x0014A6D6`, union = 13, **exact set equality** with the tool. I also scanned the
  whole image for the 4-byte little-endian constant `0x001497DC`: **zero occurrences**, so there is
  no table-driven reference I can find.
* **The "one site passes two" claim is FALSE.** The tool reports `call@0x0016B912 … args=2`.
  `_safe_start` picked `0x0016B8EC` by scanning backward for a `C3` byte — but the `C3` it found at
  `0x0016B8EB` is the **ModRM byte of `add ebx,0x12` (`83 c3 12`) at `0x0016B8EA`**, not a `ret`.
  Decoding from `0x0016B8EC` yields `adc al,[ebp+0x501874c0]` (6 bytes), which **swallows the
  `push eax` at `0x0016B8F1`** — hence 2 instead of 3.
  Decoding instead from the **verified** generated-function boundary `0x0016B8D0`
  (`recovered.c`, `0x0016B8D0..0x0016B960`) gives `push ebx; push 0; call 0x14a838 (consumed=0);
  push eax` = **3**. My independent walk agrees on all 13 sites.
  **Independent sanity check:** the callee is generated as `CC: cdecl, 3 params` and ends
  `esp += 16; return;` (= `ret 12`, cleaning 12 bytes). A genuine 2-argument call site would be an
  ABI contradiction. So: **13 of 13 call sites pass three arguments.**
* The tool's *correct* result for the bound caller — `call@0x0017C921 … args=3` — is **CONFIRMED**:
  `0017C918 push eax / 0017C919 push 0 / 0017C91B call 0x14a838 / 0017C920 push eax /
  0017C921 e8 b6 ce fc ff call 0x1497dc`, and `0x14a838` is genuinely `a1 d4 dc 27 00; c3`
  (`mov eax,[0x27dcd4]; ret`), a zero-consumption getter. The Session's self-reported
  "one argument" error and its correction are both real and correctly described.

This is precisely the *"misaligned disassembly producing plausible garbage"* failure mode, and it
**survived into the accepted record**: the tool's fail-closed design works for `verify()` but
`_safe_start`'s `ret`-byte heuristic is not fail-closed. The load-bearing claim (the bound caller
passes three) is unaffected.

## 7. No-writer claim, both spellings: **CONFIRMED**

Within the generated `sub_001497DC` body (my measure: lines 20105-20952, 848 lines — the evidence
says 841; immaterial): `MEM32(ebp + 0x10)` occurs **5** times, `MEM32(ebp + 16)` **0** times, and
writes `MEM32(ebp + 0x10) =` total **0** in either spelling. I also checked `MEM8`/`MEM16` writes to
`ebp+0x10`: none. Positive control present (the same pattern finds the 5 reads). Note for context:
the callee **does** write `MEM32(ebp + 0xC) = ecx` — i.e. arg1 — so the slot distinction is real.
**AGREED.** *(Caveat, inherent to the method: this is a text-level enumeration of generated code, not
an XBE-instruction-stream enumeration as §6.1 demands for a "every access" claim. It is adequate for
"no writer inside this one generated body", which is what is claimed.)*

## 8. The `O-OPEN` "decisive arithmetic": **the conclusion holds; one supporting calculation is wrong**

The producer, from verified bytes: `mov eax,[ebp+0x10]; test; jne; inc eax; add eax,0x1f;
and eax,0xfffffff0; mov [ebp-0x24],eax; mov edi,eax; shr edi,4; mov [ebp-0x28],edi`, and at
`0x00149E24` `add dword [ebp-0x24],0x20` immediately before the allocation.

With `V(x) = (x + 0x1F) & ~0xF` (and `x==0 → 1`):

| Quantity | My result |
|---|---|
| `V(0x4C000010)` | `0x4C000020` — **= crash `eax`** ✓ |
| `0x4C000020 >> 4` | `0x04C00002` — **= crash `edi`** ✓ |
| pre-image of `0x4C000020` | **`[0x4C000001, 0x4C000010]`** (16 values) |
| dump slot `0x00F7FEAC` | `0x4C000010` — **inside that interval** |
| failing size 598869040 = `0x23B20430` ⇒ `V(a)+0x20` ⇒ `V(a)=0x23B20410` ⇒ pre-image | `[0x23B203F1, 0x23B20400]` |

So the evidence's line 145 (*"the crashing activation's `arg2` must lie in `[0x4C000011,
0x4C000020]`"*) is **arithmetically wrong** — the correct interval is one 16-byte block lower — and
the conclusions drawn from it (lines 146, 148-149: *"below that interval"*, *"the slot agrees with
**neither**"*) are **wrong**. `V(0x4C000011) = 0x4C000030`, not `0x4C000020`. Line 127
(*"`0x4C000010 + 0x20 = 0x4C000020`"*) is also wrong (`= 0x4C000030`) and is inconsistent with the
document's own line 123, which computes `0x4C000040`.

**This is an internal contradiction in the record** — line 128 says the frame *"**is** the crash
activation's, and it is **self-consistent with the crash**"* while lines 145-149 say the slot
*"matches **neither**"*. Line 128 is the correct one; lines 145-149 are the unswept remnant.

**But the row does not change.** With the corrected interval, the dump slot `0x4C000010` is fully
consistent with the *crash* activation (activation B) and **still inconsistent with the failing
allocation** (`~0x23B20xxx`), so "the archived frame is not the failing allocation's activation"
survives — and in fact becomes *cleaner*: activation A's `arg2` was overwritten by activation B's
push at the same address, which is exactly the mechanism the evidence names.

**One over-broad sentence:** line 150-151, *"The `arg2` slot is dead stack by capture time … so it is
not a witness for any activation's `arg2`."* At capture the crash is *inside* activation B with
`esp ≈ 0x00F7FD00`, so `0x00F7FEAC` is live caller argument space holding **activation B's** `arg2`,
and it matches B's registers exactly. It is not a witness for the **failing** activation's `arg2`.
Correct the scope of that sentence.

## 9. Synthetic completion: **CONFIRMED ABSENT**

`RECOMP_APU_TRAP=1` and `RECOMP_GPU_ACK=0` in the run's own `metadata.json`; `RECOMP_APU_DSP_ACK`
and `RECOMP_AC97_READY` absent; no `JSRF_ALLOW_UNRESOLVED`. The commit diff touches only
`docs/reviews/…`, `plan-jsrf-bare-minimum.md`, and the two new `scripts/` files — **no** game source,
**no** generated code, **no** toolkit edit. The trap and `0x80` were not suppressed, the allocation
was not faked, the arena was not widened, the NULL call was not bypassed, and no guest
error-handling change is proposed. `PIO_FREE` remains DEFERRED; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` were
not reopened; `0xFFFFB3` stays UNRESOLVED. **No guest run was performed**, correctly: the packet
allows one only if the offline audit cannot identify the frame *and* a safe non-generated seam
exists, and the only seam is `kernel_thunk_dispatch()` — I verified it is at
`C:\Users\logic\Repos\xboxrecomp\src\kernel\kernel_bridge.c:8944` — i.e. **toolkit**, outside this
packet's write scope. `src/diagnostics.c` is a store-observer for six fixed store sites and does not
see this entry or read.

## 10. Frozen packet / toolkit / generated code unchanged: **CONFIRMED**

Packet hash re-computed and matches, 37 lines. Toolkit `c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`,
`git status --porcelain` **empty**. Game working tree clean; diff `c914448..HEAD` = 4 files, all in
the allowed set. `src/recomp/gen/**` untouched (confirmed by that diff).

---

## Required corrections (do not change the row)

1. **`scripts/a2h-frame-audit.py` / evidence line 108:** "12 of 13 call sites pass three arguments;
   one passes two" → **13 of 13 pass three**. Fix `_safe_start` (a bare `C3`/`C2` byte scan is not a
   reliable basic-block terminator) and pin `call@0x0016B912 args=3` with a test.
2. **Evidence lines 127, 145-149:** replace `[0x4C000011, 0x4C000020]` with
   **`[0x4C000001, 0x4C000010]`**, drop *"matches neither"*, and keep line 128's correct statement
   that the frame is the crash activation's and self-consistent with it. State the surviving point
   plainly: the slot agrees with the **crash** activation and with **neither** the failing
   allocation's implied value nor any value near `0x23B20xxx`.
3. **Evidence line 150-151:** "not a witness for **any** activation's `arg2`" → "not a witness for
   the **failing** activation's `arg2`".

## Advisory (non-blocking)

* Test count understated: the evidence (line 209) and plan (line 151) say **21 tests**; the suite
  actually has **25**.
* `a2h-frame-audit.py:145` docstring still says the broken sweep returned zero "for a target that
  demonstrably has **eight**" — the packet's superseded premise; the test docstring correctly says
  thirteen. Sweep the superseded "eight".
* "Three **independent** facts" (lines 71-77) → "one consistency check, corroborated by the dump
  layout"; keep the dump-slot corroboration, it is stronger.
* Step 2's literal instruction was to extend the existing tool, not add a new one (§3).
* The new tool's fixture set does not include several of step 2's listed fixtures (competing writer,
  mismatched thread/return/stack, ordinal-184 followed by a second ordinal, malformed/truncated/
  duplicate/overflow records, absent/exercised deciding-event control). Defensible because step 3
  was never reached, but the parent may want them before the successor packet leans on this tool.

## Uncertainties — CANNOT VERIFY

* **Indirect/other entries into the callee.** I found no 4-byte `0x001497DC` constant anywhere in the
  image and no indirect call evidence, but I did **not** enumerate every `RECOMP_ICALL` target
  range. This is a coverage gap the evidence also leaves implicit; it does not affect `O-OPEN`.
* **The two withdrawn Session errors** ("one argument", "double-counted getter return address") — I
  can verify the *corrected* result and verify that "one argument" would have been wrong, but a
  withdrawn intermediate state is not independently checkable.
* **`0x0016B912` and four other call sites have no generated-function header** in my scan (they are
  recovered-code sites); I used `recovered.c` ranges for those. Alignment there rests on
  `recovered.c` headers, which I did not independently validate.
* **Dump `XBE`-backed content is displaced** (`CONTENT_MISMATCH`), so guest-stack reads are read at
  their actual guest VA and are not image-content evidence. The evidence states this correctly; the
  frame conclusion does not depend on image content.
