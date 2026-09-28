# `A2h-oom-causal-slice-r1` — Acceptance review (**stage 2**)

**Reviewer:** Acceptance reviewer, stage 2 (`workbuddy-ai/deepseek-v4.1-flash` @ `max` — contract role, §2.2).
**Date:** 2026-09-27. **Harness:** DSH.
**Contract:** `docs/packets/a2h-oom-causal-slice.md`, revision **`A2h-oom-causal-slice-r1`**, **class: discovery
(§5.8)**.
**Frozen SHA-256 — verified by me:** `E9CDB1B39066CE5F5626FF74246E5EAD34C1612BC31BE622341B50FCC541108C`
(**16 198 bytes**, 47 lines by newline count). **Matches the revision, the hash the Session recorded, the hash
`plan-jsrf-bare-minimum.md` carries, and the hash stage 1 verified.** The packet was **not** edited for this
review; I did not edit it either.

> **ROUND 2 (re-verification after repair commit `dd96963`).** The Session repaired criterion 2. **Six of my six
> original locations are corrected, plus one more (`plan:202`). One additional survivor remains, at
> `plan-jsrf-bare-minimum.md:255`** — a location outside my original six, whose phrasing (`NO trap` /
> `the trap is not the cause`) the Session's grep pattern could not match. **Disposition is unchanged:
> `NOT ACCEPTED`.** The round-2 record, with the full re-verification, is in the
> **"Stage 2 re-verification (round 2)"** section at the end of this file. The body below is the round-1
> record as written against `e1dd3e2` and is preserved unedited (§2.4.7).

**Scope (§2.2, two-stage).** Stage 1 returned `NOT ACCEPTED` with **two** mandatory criteria `DISAGREED`. Per
§2.2.3 I re-review **only those two**. Its other eleven `AGREED` criteria are **not** re-opened and are not
reproduced here. Stage-1 findings are **leads, not evidence**; every claim below is something I measured in
this session.

**Discovery acceptance bar (§5.8), applied literally:**

> *reviewer confirms the artifacts exist, match the commands, and select the recorded outcome row*

A discovery packet **never satisfies a strict criterion and never claims anything works.** Nothing below
treats any finding as evidence that anything works.

## Method

Everything below is a measurement I performed in this session: `Get-FileHash`, `Get-Content`,
`Select-String`, `git log` / `git show` / `git blame`, `python -X utf8 -m unittest`,
`scripts/a2h-oom-slice.py` (both `--verify` probes and the recorded binding command), `scripts/inspect-jsrf.py
disasm`, raw XBE section reads, and three scratch census scripts executed through `python -X utf8 -` from
stdin (**no file written to either repository**). I fetched nothing external, ran no guest, no build, and
modified no file except this review. `git status --porcelain` was **empty in the game tree** before and after.

---

## Criterion 1 — the mid-instruction disassembly fixture

### 1.1 The tool now has a real disassembly surface — **observed**

`scripts/a2h-oom-slice.py` (14 714 B, up from the 9 044 B stage 1 measured) now contains:

| Element | Location | What it does |
|---|---|---|
| `import capstone` | `:32` | the import stage 1 recorded as absent |
| `load_sections()` | `:191-208` | reads the XBE section table from `mygame_analysis.json` |
| `xbe_window()` | `:211-225` | reads `n` bytes at a guest VA, clamped to the containing section |
| `verify_instruction()` | `:228-270` | the disassembly surface; **raises `LogError`** on any guard failure |
| `decode_from()` / `_md()` | `:273-281` | capstone x86-32 decode, `detail = True` |
| `--verify VA:BYTES` CLI | `:292-294`, `:303-321` | repeatable; exits **2** on any rejection |

This is a genuine disassembly surface, not a renamed log parser: it decodes from the original XBE through
capstone and returns the decoded mnemonic. **Confirmed by execution** — `--verify 0x00149E24:8345dc20` returns
`{"va": "0x00149E24", "bytes": "8345dc20", "length": 4, "text": "add dword ptr [ebp - 0x24], 0x20"}`.

### 1.2 The tests — **31, and they pass**

```
python -X utf8 -m unittest scripts.test_a2h_oom_slice
→ Ran 31 tests in 0.067s
→ OK
→ REAL_EXIT=0
```

**Count confirmed three ways:** the runner reports `Ran 31 tests`; `Select-String '^\s+def test_'` on
`scripts/test_a2h_oom_slice.py` counts **31**; and the new `InstructionBoundaryTests` class alone reports
`Ran 9 tests ... OK`. **22 + 9 = 31**, matching the Session's claim and the 9 new boundary tests.
*(A PowerShell `NativeCommandError` appears on stderr because unittest writes progress dots to stderr; the
real exit code is 0. That is a shell artifact, not a failure.)*

### 1.3 Mid-instruction start against the **REAL XBE** — **reproduced, and it rejects**

I ran the task's own example, against `game/default.xbe` with the real section table:

```
python -X utf8 scripts\a2h-oom-slice.py --verify 0x00149E23:8345dc20
→ instruction verification failed: 0x00149E23: bytes are 008345dc, expected 8345dc20
→ exit=2
```

**Which guard fired — measured, not assumed.** The task asks whether the rejection is real or passes for the
wrong reason. I discriminated the two guards by isolating them:

| Probe | Command | Result | Guard that fired |
|---|---|---|---|
| **A** (the task's example) | `--verify 0x00149E23:8345dc20` | rejected | **BYTES guard** — the bytes at `0x00149E23` are `00 83 45 dc`, not `83 45 dc 20` |
| **B** (truncation, correct VA) | `--verify 0x00149E24:8345dc` | rejected | **LENGTH guard** — *"expected 3 bytes but the instruction decodes to 4 (add dword ptr [ebp - 0x24], 0x20) -- misaligned start?"* |
| **C** (misaligned, bytes correct for that VA) | `--verify 0x00149E23:008345dc` | rejected | **LENGTH guard** — *"expected 4 bytes but the instruction decodes to 6 (add byte ptr [ebx + 0x6a20dc45], al) -- misaligned start?"* |
| **Positive control** | `--verify 0x00149E24:8345dc20` | **accepted**, exit 0 | — |

**So the honest answer is: the task's example rejects on the BYTES guard, and the ALIGNMENT guard is
independently real.** Probe A never reached the alignment check, because `0x00149E23` holds a different byte
than claimed. That is a legitimate guard, but on its own it would not have discharged the packet's fixture.
Probe C is the one that proves the boundary check itself works: there I supplied **the bytes genuinely present
at `0x00149E23`**, so the bytes guard passed, and the tool still rejected because decoding from `0x00149E23`
yields a **6-byte** instruction against the claimed 4. That is exactly the "misaligned start renders as
plausible garbage" hazard the packet names — and note what it decoded to: `add byte ptr [ebx + 0x6a20dc45],
al`, i.e. plausible garbage, precisely as the docstring warns.

**Both guards are legitimate and both are real.** The raw XBE bytes I read independently confirm the
alignment: `0x00149E1D: 83 a5 d0 fe ff ff 00` (`and dword ptr [ebp-0x130], 0`, 7 bytes) then
`0x00149E24: 83 45 dc 20` (`add dword ptr [ebp-0x24], 0x20`, 4 bytes) — so `0x00149E24` **is** the true
boundary and `0x00149E23` is inside the preceding instruction.

### 1.4 The recorded binding command reproduces, and the fix is **additive**

I ran the exact command at `evidence.md:37`, writing to `%TEMP%` so the record under review was untouched:

```
python -X utf8 scripts\a2h-oom-slice.py --log logs/runs/20260927-160330-655-a4b2-gp-trap-trace/jsrf_run.log \
  --log logs/runs/20260922-224429-003-a2g-304f0-span/jsrf_run.log \
  --verify 0x00149E24:8345dc20 --verify 0x00149E4A:ff15883f1c00 \
  --verify 0x00149FB3:0fb707 --verify 0x0014980E:8945dc \
  --out %TEMP%\s2-binding.json
```

- **Reproduced hash:** `9E265C0F099FF0B00055641D3FEC24923F8C2832FB13C27C28E8E6DEDB281597`
- **Checked-in `docs/reviews/a2h-oom-causal-slice-binding.json`:** `9E265C0F099FF0B00055641D3FEC24923F8C2832FB13C27C28E8E6DEDB281597`
- **Identical.** All four citations verify against the real XBE with the correct decoded text.

**No regression to the load-bearing parser output — I checked this explicitly.** Running the *pre-fix* form of
the command (no `--verify`) still produces `A11C55FEEA70079C8F4258F40A6007333EC5B55E5A28BF731DD9ADD96F8B600C` —
**byte-identical to the hash stage 1 recorded for the original command.** I also compared the two outputs
programmatically: `archives identical: True`, `comparison identical: True`. The disassembly surface was added
**beside** the log parser, not through it. The load-bearing binding stage 1 reproduced is unchanged.

### 1.5 Fixture coverage — every named fixture is now covered

Against the packet's step-3 fixture list (`docs/packets/a2h-oom-causal-slice.md:24`):

| Named fixture | Covered? | Test |
|---|---|---|
| good invocation | ✓ | `test_good_invocation_binds_site_size_and_esp` (+ `test_pairs_the_ret_with_the_following_alloc`) |
| duplicate/conflicting calls | ✓ | `test_duplicate_conflicting_calls_are_visible`, `test_duplicate_calls_at_one_site_are_both_reported` |
| missing / truncated events | ✓ | `test_truncated_invocation_is_fatal_not_skipped`, `test_missing_log_is_fatal`, `test_empty_log_is_fatal`, `test_log_with_no_invocations_is_fatal` |
| wrong thread | ✓ | `test_wrong_thread_is_preserved_verbatim` |
| wrong size | ✓ | `test_wrong_size_does_not_match_the_failing_set` |
| width / signedness | ✓ | `test_large_unsigned_size_is_not_sign_flipped` |
| **mid-instruction disassembly start REJECTED** | **✓ NOW COVERED** | `test_mid_instruction_start_is_rejected` (synthetic), **`test_real_xbe_mid_instruction_is_rejected`** (real XBE, `0x00149E23`), `test_instruction_length_mismatch_is_rejected` (the alignment guard), with positive controls `test_correct_boundary_verifies` / `test_real_xbe_citations_verify` and negatives `test_wrong_bytes_at_a_valid_boundary_are_rejected`, `test_malformed_expected_bytes_are_rejected`, `test_va_outside_any_section_is_rejected`, `test_missing_xbe_is_rejected` |

**The fixture stage 1 found missing is now present, implemented, and exercised against the real XBE.**
`O-OPEN`'s *"missing parser coverage"* condition no longer applies to this surface. **Verdict: AGREED.**

---

## Criterion 2 — the A2g "no-trap" characterisation and the withdrawn claim

### 2.1 The trap state — I read it myself, and stage 1 is right

| Witness | Value |
|---|---|
| `logs/runs/20260922-224429-003-a2g-304f0-span/metadata.json` → `settings` | `{"name": "RECOMP_APU_TRAP", "value": "1"}` |
| A2g `jsrf_run.log` **line 26** | `  APU: 0xFE800000..0xFE880000 trapped for MMIO` |
| `metadata.json` → `run_profile` | `{}` (empty object) |
| `metadata.json` → `run_profile.effective_settings` | **`null`** |

**Confirmed: A2g is trapped.** The Session's original error is fully explained and is exactly as it now
describes it: `run_profile` is empty and `effective_settings` is `null`, so the value was read from the wrong
key and "absent" was reported as a negative measurement. **Both runs are trapped.** The corrected banner's
factual content is accurate.

### 2.2 The census — **reproduced exactly, with a positive control**

I censused independently across all `logs/runs/` directories, reading the trap state from **both** the
`settings` dict and the log's own line:

| Quantity | My measurement | Session's claim | Match |
|---|---|---|---|
| run directories | **719** | — | — |
| directories with a `jsrf_run.log` | **718** (1 is a build-source snapshot with no log) | — | — |
| **runs carrying `598869040`** | **35** | 35 | ✓ |
| **trapped** | **35** | 35 | ✓ |
| **untrapped** | **0** | 0 | ✓ |
| unclear / mixed | **0** | 0 | ✓ |

**The absence claim has a positive control (§2.4.5), which is what makes it admissible.** I checked whether my
method can observe an *untrapped* run at all: **657 runs** have `RECOMP_APU_TRAP` absent-or-`0`. So the census
is not blind to the condition it reports as zero. Of those 657, **0** carry the `598869040` request. I also
cross-validated the two witnesses against each other: the `settings` value and the log's "trapped for MMIO"
line **agree on 716 of 717** runs; the single disagreement
(`20260922-055053-100-p4-ac97`: setting absent, log line present) does not carry the request and does not
affect the population. **The Session's "35 trapped, 0 untrapped" reproduces exactly, and the "reading every
plausible location" method is sound.**

**Does it change the conclusion?** No — it **confirms** it. The archive cannot separate trap-necessity in
either direction, exactly as the correction banner now states.

### 2.3 The narrower claim survives, and I verified its basis

The claim that *should* survive is: **the failure is not trace-caused, and it predates the A4b2 trap work.**
Its basis in `a2h-oom-causal-slice-binding.json` reproduces:

| Field | A2g | R1 | Same |
|---|---|---|---|
| `invocation_count` | 94 | 94 | ✓ |
| `failing_indices` | `[93]` | `[93]` | ✓ |
| `failing_call_site` | `0x00149E50` | `0x00149E50` | ✓ |
| failing `esp` | `0x00F7FCF0` | `0x00F7FCF0` | ✓ |
| `base`/`size`/`type` | `0x00000000`/`598869040`/`0x801000` | identical | ✓ |
| `oom_tuple_same` / `icall_tuple_same` / `failing_alloc_same` | all **True** | | ✓ |

Exe identities confirm the five-day gap and the different build: A2g `2cd0472a256e9dad…` started
**2026-09-23T05:44:29Z**; R1 `bc8e288dd54d8a09…` started **2026-09-27T23:03:31Z**. The build-independence
claim is sound and does **not** depend on trap effectiveness.

### 2.4 The strong claim is withdrawn **in the banners — but it still stands unflagged in the records**

The correction banners are present and honest. `evidence.md:73-107` and `mechanism.md:24-40` both state
plainly that A2g is trapped, that the strong claim is not established, and that the census shows 35/0.
That part of the remedy is real.

**But the remedy is incomplete.** Searching both corrected records for surviving claims, I found the
withdrawn claim **still asserted verbatim in six places**, none of them carrying a pointer to the correction.
`git blame` shows **every one of them is untouched by the fix commit `e1dd3e2`** — the fix added banners and
edited the surrounding section, but did not sweep the document bodies:

| # | Location | Surviving text | Blame |
|---|---|---|---|
| 1 | `a2h-oom-causal-slice-evidence.md:422` | **"Establishes:** the trap is **not a necessary cause** (reproduced without it on an older build)" | `12017740` — **not touched by `e1dd3e2`** |
| 2 | `a2h-oom-causal-slice-evidence.md:112` | heading: *"The causal chain is **semantically identical** across a trap run and a **no-trap** run"* | `12017740` — not touched |
| 3 | `a2h-mechanism.md:1` | document **title**: *"…and it is **not** trap-caused"* | `56b64a65` — not touched |
| 4 | `a2h-mechanism.md:56` | section heading: *"## 3. The full chain, from the **no-trap run**"* | `56b64a65` — not touched |
| 5 | `a2h-mechanism.md:182-183` | *"**The trap is a red herring for causation.** … which is why trapped runs die at 4.77 s and **untrapped ones reach the pending-word hang instead**."* | `56b64a65` — not touched |
| 6 | `a2h-oom-causal-slice-r1-session-verification.md:27` | *"It dropped the trap as a necessary cause. The **no-trap** A2g run…"* | promotion record (historical) |

**Item 1 is the most serious**, and it is why I cannot call the claim genuinely withdrawn. It sits in the
evidence record's **"What this establishes"** conclusions list — the section a reader consults for the
packet's findings — and it re-asserts the exact sentence the banner 300 lines above declares `WITHDRAWN`,
with no cross-reference. **Item 3 is the headline claim of the mechanism record**, and **item 4 heads the
very section the narrower claim rests on.** I verified item 4's subject directly: the chain block at
`mechanism.md:59-66` contains `tid=2948`, which occurs **5×** in A2g's log and **0×** in R1's — so
"section 3, from the no-trap run" **is** describing A2g, the trapped run.

**The records therefore still state the withdrawn claim as established.** The artifact a discovery packet's
acceptance binds to — *"confirm the artifacts exist, match the commands"* — does not yet match reality at
these six points. This is the same defect stage 1 found, reduced but not removed.

### 2.5 The stage-1 caveat — handled correctly, and it does **not** overclaim

Stage 1 recorded two things it could not settle: it could not census all archives, and it could not say
whether A2g's trap was behaviourally *effective*. On the second, I measured the difference myself:

| Witness | A2g | R1 |
|---|---|---|
| APU init line (`L38`) | `[APU] DSP GP/EP initialized (**STUBBED - passthrough mode**)` | `[APU] DSP56300 GP/EP core initialized (**pinned xemu 67cc79e6**, C interpreter…)` |
| `[APU]` lines | 11 | 7 |
| `[APUMMIO]` lines | **0** | **401** |
| `[APU] read of unimplemented GP DSP block at offset 0x30200` | **present** (L12989) | **absent** |

So stage 1's suspicion is correct: the two runs' traps **were behaviourally different**. A2g's trap was
armed and its handler was reached (the GP-block line proves a guest MMIO read routed into `apu_core.c`), but
its APU core was a passthrough stub where R1's is a pinned interpreter.

**Does the corrected record overclaim past that?** **No.** The narrower claim it now makes —
*"the OOM predates the A4b2 work … not introduced by whatever A4b2 changed"* — is a **build-independence**
claim across a different exe five days earlier. It does not depend on how much APU behaviour either trap
intercepted, and it does not assert trap-necessity. **The narrower claim stays inside the caveat.** The
overclaiming is in the six unflagged residual sentences of §2.4, not in the corrected banner's own reasoning.
The corrected records do not address the behavioural difference at all (zero hits for
`STUBBED`/`passthrough`/`pinned`/`DSP56300` in either), which is an omission but not an overclaim.

**Verdict: DISAGREED** — the trap state, the census (35/0, with positive control), and the narrower claim all
reproduce; the correction banners are present and honest; **but the withdrawn strong claim still stands
unflagged in six locations across the two corrected records**, including `evidence.md:422` (a conclusions
list) and `mechanism.md:1` (a document title).

---

## Disposition

**Criterion 1 — `AGREED`.** The disassembly surface is real (`verify_instruction`, `:228-270`; capstone at
`:32`; `--verify` at `:292-321`). The suite is **31 tests, OK, exit 0**. The mid-instruction start rejects
against the **real XBE**; I isolated and confirmed **both** the bytes guard and the alignment guard, and the
alignment guard produces exactly the "plausible garbage" decode the packet exists to prevent. Every named
fixture is covered, including the one stage 1 found missing. The recorded binding command reproduces
byte-for-byte, and the fix is **additive** — the pre-fix parser output is unchanged.

**Criterion 2 — `DISAGREED`.** The remedy is real but **insufficient**. I confirmed the trap state
(`settings.RECOMP_APU_TRAP = 1`; log line 26 trapped), reproduced the census exactly (**35 trapped, 0
untrapped**, with a 657-run positive control proving the method can see an untrapped run), and confirmed the
narrower build-independence claim is well-founded and does not overclaim past stage 1's caveat. But the
withdrawn strong claim — *"the trap is not a necessary cause"* — **still appears unflagged in six locations**,
all untouched by the fix commit, including a conclusions list and a document title.

Per §2.2, `ACCEPT` requires every mandatory criterion `AGREED`; one is `DISAGREED`, so the disposition is
**`NOT ACCEPTED`**. I do **not** classify either finding as blocking or advisory — that is a judgment-layer
decision (§2.2.6) — and I flag both for the Session. Neither is an outside-contract concern; both sit inside
§5.8's *"artifacts exist, match the commands, select the recorded row"* bar.

**DISPOSITION: NOT ACCEPTED**

### Blocking criteria

| # | Criterion | Disposition | Basis |
|---|---|---|---|
| 1 | Parser tests cover the packet's named fixtures | **AGREED** | 31 tests OK, exit 0; 9 new boundary tests; mid-instruction start rejected against the real XBE by **both** a bytes guard and an independently-confirmed alignment guard; all named fixtures covered |
| 2 | Artifacts match the commands — A2g "no-trap" characterisation | **DISAGREED** | Correction banners added and census reproduces (35/0), but the withdrawn claim survives **unflagged** at `evidence.md:422`, `evidence.md:112`, `mechanism.md:1`, `mechanism.md:56`, `mechanism.md:182-183` — all untouched by `e1dd3e2` |

### What the remedy needs (mechanical, no re-execution)

Criterion 2 needs **six line-level corrections**, not a new measurement. The evidence is already correct in
the banners; the bodies were simply not swept. Items 1–5 are in the packet's own write scope
(`docs/reviews/a2h-oom-causal-slice-evidence.md`, `docs/reviews/a2h-mechanism.md`). Item 6 is a historical
promotion record, so it is a judgment call for the Session/Planner whether it is corrected or annotated.

---

## Advisories (outside the contract — do not change the disposition)

1. **`verify_instruction` is a citation verifier, not a boundary oracle.** I found a real coverage limit:
   `--verify 0x00149E1E:a5` is **accepted** (exit 0, decodes to `movsd`), even though `0x00149E1E` is inside
   the 7-byte instruction at `0x00149E1D` (`83 a5 d0 fe ff ff 00`). The guard only catches misalignment when
   the decoded length differs from the claimed length; a caller who claims a 1-byte instruction at a
   non-boundary and gets a 1-byte decode passes. This does **not** affect the packet's fixture (which is
   satisfied), and the tool's contract — "verify this cited instruction" — requires the caller to supply the
   length, so a wrong-boundary caller must already assert a wrong length to slip through. Worth a docstring
   note; `scripts/inspect-jsrf.py disasm` remains the tool for arbitrary-range decoding.
2. **The frozen packet's own premise text is now known-false and cannot be edited.** `a2h-oom-causal-slice.md:7`
   ("Known: **no-trap** A2g and trapped A4b2 runs") and `:9` ("check archived **no-trap** and trapped
   identities") and `:16` ("The archived **no-trap** `…a2g-304f0-span/`"). Per §2.2.4 I must not edit a frozen
   packet and did not. This is **not** a Session defect, but a future reader may take the packet's premise as
   established. Per §5.4 this is a Planner/Advisor matter if it is to be reopened at all — the row selection
   does not depend on trap-necessity, so it likely does not need to be.
3. **`session-verification.md:27`** still says the Planner "dropped the trap as a necessary cause" using the
   "no-trap A2g run". It is a historical promotion record, and it is accurate about what the Planner did at
   that time; but it repeats the refuted label. Item 6 of §2.4.
4. **The behavioural trap difference is now measured but unrecorded.** The corrected records say nothing about
   A2g's `STUBBED - passthrough mode` APU versus R1's pinned DSP56300 core, nor about A2g's 0 `[APUMMIO]`
   lines against R1's 401. Stage 1 listed this as uncertain; it is now measured (§2.5) and would strengthen
   the record if added — as a limit, not as support for any trap-necessity claim.
5. **The census's single witness disagreement.** `20260922-055053-100-p4-ac97` has `RECOMP_APU_TRAP` absent
   from `settings` while its log line 26 reports the trap. It does not carry the request, so the population is
   unaffected, but the "read every plausible location" method is only as good as the key list. A
   log-line-first rule would be strictly more robust.
6. **`mechanism.md` retains non-ASCII mojibake in this shell** (`â€”` for `—`). This is a console encoding
   artifact of `Get-Content` under this code page, not a file defect — the file reads correctly as UTF-8.
   Recorded only so a future reader does not chase it.

---

## Uncertain

1. **Whether A2g's trap was behaviourally effective at the moment of the failing allocation.** I measured that
   the two traps differed (stub vs pinned core; 0 vs 401 `[APUMMIO]` lines) and that A2g's handler was
   demonstrably reached (`[APU] read of unimplemented GP DSP block`, L12989). I did **not** measure how much
   APU behaviour A2g's stub actually intercepted during the 4.95 s prefix, and I cannot from the archive.
   This does not affect either criterion: it is why the narrow claim is the correct one.
2. **The correct disposition of the two findings** (blocking vs advisory) is a judgment-layer decision I am
   not permitted to make (§2.2.6).
3. **No run with `RECOMP_APU_TRAP` absent carries this request anywhere in this archive** — I censused all 719
   run directories, so I can state this for the archive as it stands. I cannot speak to runs that were never
   archived or were deleted; that is outside what any archive can witness.

---

## Reproduction commands

```powershell
# packet hash (must be E9CDB1B3…41108C, 16198 bytes)
(Get-FileHash docs\packets\a2h-oom-causal-slice.md -Algorithm SHA256).Hash

# criterion 1 — tests and count
python -X utf8 -m unittest scripts.test_a2h_oom_slice            # Ran 31 tests ... OK
python -X utf8 -m unittest -v scripts.test_a2h_oom_slice.InstructionBoundaryTests   # Ran 9 tests ... OK
(Select-String -Path scripts\test_a2h_oom_slice.py -Pattern '^\s+def test_').Count   # 31

# criterion 1 — the disassembly surface, against the REAL XBE
python -X utf8 scripts\a2h-oom-slice.py --verify 0x00149E24:8345dc20   # accepted, exit 0 (positive control)
python -X utf8 scripts\a2h-oom-slice.py --verify 0x00149E23:8345dc20   # BYTES guard, exit 2
python -X utf8 scripts\a2h-oom-slice.py --verify 0x00149E23:008345dc   # ALIGNMENT guard, exit 2
python -X utf8 scripts\a2h-oom-slice.py --verify 0x00149E24:8345dc    # LENGTH guard, exit 2
python -X utf8 scripts\inspect-jsrf.py disasm 0x00149E1D 0x00149E2E

# criterion 1 — recorded command reproduces; pre-fix output unchanged
python -X utf8 scripts\a2h-oom-slice.py --log logs/runs/20260927-160330-655-a4b2-gp-trap-trace/jsrf_run.log `
  --log logs/runs/20260922-224429-003-a2g-304f0-span/jsrf_run.log `
  --verify 0x00149E24:8345dc20 --verify 0x00149E4A:ff15883f1c00 `
  --verify 0x00149FB3:0fb707 --verify 0x0014980E:8945dc --out $env:TEMP\s2-binding.json
# → 9E265C0F…B281597, identical to the checked-in binding.json
# without --verify → A11C55FE…F8B600C, identical to the hash stage 1 recorded

# criterion 2 — the trap state
python -X utf8 -c "import json;d=json.load(open('logs/runs/20260922-224429-003-a2g-304f0-span/metadata.json'));print(d['settings']);print('run_profile=',d['run_profile'])"
(Get-Content logs\runs\20260922-224429-003-a2g-304f0-span\jsrf_run.log)[25]   # line 26

# criterion 2 — surviving claims and their provenance
Select-String -Path docs\reviews\a2h-oom-causal-slice-evidence.md -Pattern 'no-trap|not a necessary cause'
Select-String -Path docs\reviews\a2h-mechanism.md -Pattern 'no-trap|not trap-caused|red herring'
git blame -L 422,422 -- docs/reviews/a2h-oom-causal-slice-evidence.md
git blame -L 1,1     -- docs/reviews/a2h-mechanism.md
git show e1dd3e2 -- docs/reviews/a2h-mechanism.md     # the fix touched only the section-2 block

# criterion 2 — the census (35 trapped / 0 untrapped) and its positive control (657 untrapped runs)
#   read `settings` AND the log's "trapped for MMIO" line for every dir under logs/runs/
# criterion 2 — the behavioural difference stage 1 could not measure
Select-String -Path logs\runs\20260922-224429-003-a2g-304f0-span\jsrf_run.log -Pattern 'STUBBED|APUMMIO'
Select-String -Path logs\runs\20260927-160330-655-a4b2-gp-trap-trace\jsrf_run.log -Pattern 'DSP56300|APUMMIO'
```

---

# Stage 2 re-verification (round 2) — repair commit `dd96963`

**Scope:** the Session repaired criterion 2 and asked me to verify **only that repair**. Criterion 1 was
already `AGREED` and I did **not** re-review it (§2.2.3). **Commit under verification: `dd96963`**;
`git status --porcelain` **empty** (clean) before and after.

## R2.1 Integrity of the repair

| Check | Result |
|---|---|
| HEAD | `dd96963` — *"A2h: sweep the document BODIES for the withdrawn claim -- stage 2 was right"* |
| Working tree | **clean** |
| **Frozen packet `E9CDB1B3…41108C`** | **UNCHANGED** — hash re-verified; `dd96963` does **not** touch `docs/packets/a2h-oom-causal-slice.md` (§2.2.4 respected) |
| Files touched | the two review records, `session-verification.md`, `plan-jsrf-bare-minimum.md` |

## R2.2 My six original locations — all six corrected

| # | Location | Now reads | Verdict |
|---|---|---|---|
| 1 | `evidence.md:422` (conclusions list) | *"Establishes: the failure **predates the A4b2 trap work** … and it is **not trace-caused** … **The trap is NOT established as unnecessary** — all 35 archived runs carrying this request are trapped"* | **CORRECTED** |
| 2 | `evidence.md:112` (heading) | *"### The causal chain is **semantically identical** across two runs five days and one build apart"* | **CORRECTED** |
| 3 | `mechanism.md:1` (title) | *"…and it **predates** the A4b2 trap work"* | **CORRECTED** |
| 4 | `mechanism.md:56` (heading) | *"## 3. The full chain, from the A2g run (an earlier build, five days before the a4b2 work)"* | **CORRECTED** |
| 5 | `mechanism.md:182-186` | *"**The trap is NOT established as a red herring.** … **all 35 archived runs carrying this request are trapped**, so this archive **cannot separate trap-necessity either way**. **The withdrawn strong claim is replaced by the narrow one**"* | **CORRECTED** |
| 6 | `session-verification.md:27-36` | *"~~The no-trap A2g run~~ **CORRECTION (acceptance stage 2): the A2g run IS TRAPPED** … **The strong 'trap is not necessary' claim is withdrawn.**"* | **CORRECTED** |

All five of the round-1 residual assertions I named are gone, and the sixth is struck through with an in-place
correction. **`git show dd96963` confirms these are the lines the commit changed.** The new text in each case
states the **narrow** claim and explicitly declines the strong one — no softening, no replacement overclaim.

## R2.3 The plan — one of two rows fixed; **one survivor remains**

The Session found and fixed a seventh location I had **not** reported: `plan-jsrf-bare-minimum.md:202`
(*"The failure predates the A4b2 trap work"*) and `:169`. **Good — that is a genuine addition to my list.**

**But the plan carries a second, independent assertion of the withdrawn claim, at `plan-jsrf-bare-minimum.md:255`,
and it was not swept:**

> | `docs/reviews/a2h-mechanism.md` | … **the earliest OOM run has NO trap** (`20260922-224429-003-a2g-304f0-span`, a2g) so **the trap is not the cause** — it makes an already-broken path reachable sooner; … |

**Evidence this is a real, unflagged survivor:**

- **`git blame -L 255,255` → `b3bf2c1c`** (2026-09-27) — **untouched by `dd96963`**. The commit's hunks for the
  plan are `@@ -130,6 +130,38 @@` and `@@ -167,7 +199,7 @@`; **line 255 is in neither.**
- **It asserts both halves of the withdrawn claim**, not a neutral restatement: A2g "has **NO trap**" (the
  refuted factual premise) **and** "the trap is **not the cause**" (the withdrawn conclusion).
- **Nothing flags it.** The nearest heading is `### What the Session measured before planning (both records
  committed)` (L251), inside the `## Previous — A2h-oom-causal-slice-r1 …` section (L224), which is **not**
  marked superseded or historical. There is no correction banner, no strikethrough, and no cross-reference.
- **It sits above the section's own error ledger, which does not cover it.** L259-261 reads *"Two Session
  errors in that analysis are recorded and corrected in place"* and names exactly two: the *"exactly 2 MB"*
  claim and the horizon record's *"OOM tracks the trap"* reading. **The A2g error is not among them** — so a
  reader arriving at L255 sees an uncorrected "finding" and, six lines later, a list of the section's errors
  that omits it.
- **The Session's own sweep could not have caught it.** Their pattern was
  `no-trap|not trap-caused|red herring|not a necessary cause`. Line 255 spells the claim
  **`NO trap`** (space-separated, bolded) and **`the trap is not the cause`** — I verified the regex does not
  match the line. This is the same failure mode as the original defect: a sweep that does not cover the
  population it claims to cover.

**Weight, stated honestly.** This is the plan's own index of what `a2h-mechanism.md` establishes, and the
plan is the durable document the next session reads first. It is **narrower** than the six round-1 items (it
is a document index rather than the record's conclusions list or its title), so the repair is genuinely
substantial. But it is the **same defect**: an unflagged assertion of a claim the Session has formally
withdrawn, in a live section, contradicting the corrected rows 47 lines above it.

## R2.4 The narrow claim does not overclaim — confirmed

Re-read at `evidence.md:422-426`, `mechanism.md:182-186` and `plan:124-127`: the surviving claim is
**"the failure predates the A4b2 trap work"** (different exe `2cd0472a256e9d` vs `bc8e288dd54d`, five days
earlier, same size/type/OOM tuple/terminal ICALL, all 94 invocation sizes identical) plus **"not
trace-caused"**. Both are **build/trace-independence** claims. Neither asserts trap-necessity, and neither
depends on how deeply A2g's stub intercepted APU behaviour — so **the narrow claim stays inside my own
caveat**, exactly as the Session states. The newly added text at `mechanism.md:182-186` and
`plan:160-163` records the behavioural difference as a **limit**, which is the correct treatment.
**No overclaim found.**

## R2.5 The census still reproduces — confirmed

Re-run independently against the current tree:

| Quantity | Round 1 | Round 2 | Session's claim |
|---|---|---|---|
| run directories | 719 | **719** | — |
| with `jsrf_run.log` | 718 | **718** | — |
| with `metadata.json` | — | **718** | — |
| **carrying `598869040`** | 35 | **35** | 35 |
| **trapped** | 35 | **35** | 35 |
| **untrapped** | 0 | **0** | 0 |
| positive control (trap setting absent/`0`, metadata present) | 657 | **657** | — |
| …of those, carrying the request | 0 | **0** | — |

**Unchanged and correct.** (The one directory lacking both `jsrf_run.log` and `metadata.json` is
`20260921-122436-177-test-healthy-0`, a build-source snapshot; one further run's `metadata.json` is
unparseable JSON. Neither carries the request, so neither affects the population.) The absence claim keeps
the coverage witness §2.4.5 requires.

## R2.6 Round-2 disposition

**(i) No unflagged assertion of the withdrawn claim survives?** **NO — one does:**
**`plan-jsrf-bare-minimum.md:255`**. The six locations I named are all corrected, and the Session
independently found a seventh; but this eighth assertion remains, untouched by `dd96963`, unflagged, in a
live section, asserting both the refuted premise ("NO trap") and the withdrawn conclusion ("not the cause").

**(ii) The surviving claim is the narrow one and does not overclaim?** **YES** — confirmed at
`evidence.md:422`, `mechanism.md:182-186` and `plan:124-127`. It stays inside my caveat, and the behavioural
difference is now recorded as a limit.

**(iii) The census still reproduces?** **YES** — 35 trapped / 0 untrapped, with the 657-run positive control.

**DISPOSITION (round 2): NOT ACCEPTED** — criterion 1 `AGREED` (unchanged, not re-reviewed); criterion 2
still `DISAGREED` on one surviving location.

### What the remaining repair needs

**One line.** `plan-jsrf-bare-minimum.md:255` needs the same treatment `:202` and `:169` already received —
replace *"the earliest OOM run has NO trap … so the trap is not the cause"* with the narrow form, e.g. *"the
earliest OOM run is five days and one build earlier, so the failure predates the A4b2 trap work; the trap is
NOT shown to be unnecessary."* **No re-measurement is required** — the evidence is already correct everywhere
else, and this is the last assertion of the withdrawn claim I can find.

**A note on method, since it is now the second occurrence.** The round-1 lesson was *"a correction banner is
not a correction."* This round adds its corollary: **a sweep is only as good as its pattern.** The Session
searched for the *hyphenated* spelling `no-trap`; the surviving line uses `NO trap`. I found it by sweeping
for the **claim's semantic content** (`NO trap`, `not the cause`, `without the trap`, `trap is not`) rather
than a fixed string list. For a withdrawal, the sweep must enumerate the *claim*, not one spelling of it.

## Round-2 reproduction commands

```powershell
git log --oneline -3                      # HEAD = dd96963
git status --porcelain                    # must be empty
(Get-FileHash docs\packets\a2h-oom-causal-slice.md -Algorithm SHA256).Hash   # E9CDB1B3…41108C (unchanged)
git show --stat dd96963 -- docs/packets/a2h-oom-causal-slice.md              # no output = packet untouched

# the six corrected locations
git show dd96963 -- docs/reviews/a2h-mechanism.md
(Get-Content docs\reviews\a2h-oom-causal-slice-evidence.md)[421..425]        # conclusions list
(Get-Content docs\reviews\a2h-mechanism.md)[181..185]                       # the red-herring bullet
(Get-Content docs\reviews\a2h-oom-causal-slice-r1-session-verification.md)[26..35]

# THE SURVIVOR
(Get-Content plan-jsrf-bare-minimum.md)[254]
git blame -L 255,255 --date=short -- plan-jsrf-bare-minimum.md              # b3bf2c1c, NOT dd96963
git show dd96963 -- plan-jsrf-bare-minimum.md | Select-String '^@@'         # hunks at 130 and 167/199 only
# and the pattern gap:
$l = (Get-Content plan-jsrf-bare-minimum.md)[254]
$l -match 'no-trap|not trap-caused|red herring|not a necessary cause'       # False — the sweep misses it

# census re-verification (35 trapped / 0 untrapped; 657-run positive control)
#   read `settings` AND the log's "trapped for MMIO" line for every dir under logs/runs/
```

---

# Stage 2 re-verification (round 3) — repair commit `7d798af`

**Scope:** the Session fixed my round-2 survivor (`plan:255`), additionally fixed the error ledger I had
flagged as its context, broadened the sweep to a semantic pattern, and asked for the bounded re-check I
offered. **Commit under verification: `7d798af`** (plus `c32cedf`, the evidence-record round-3 entry).
`git status --porcelain` **empty**; **frozen packet `E9CDB1B3…41108C` re-verified and untouched**.

**The withdrawn claim is now fully swept. No surviving assertion remains.** But **the fix commit introduced a
new defect of its own** — a malformed table row — which I report below and which is the reason this round is
still `NOT ACCEPTED`.

## R3.1 The withdrawn claim — **no surviving assertion** (confirmed)

**My own sweep**, run independently with the semantic pattern the Session adopted
(`no[\s\-_.]*trap | not (the|a) cause | trap is not | trap not | without the trap | trap-caused | red herring |
necessary cause`), case-insensitive, across the four documents and then **repo-wide** (excluding `logs/`,
`build/`, the frozen packet, the two prior acceptance reviews, and the operating history):

| Location | Hit | Classification — I read each one |
|---|---|---|
| `evidence.md:53,74-91,104,118,160` | `no-trap`, `not a necessary cause` | **correction banners / the new round-3 lesson block / a quoted WITHDRAWN sentence** — all inside a corrective frame |
| `evidence.md:456` | `NOT established as unnecessary` | **corrected conclusions list** — the narrow form |
| `mechanism.md:24` | `previously claimed A2g had NO trap. That was WRONG` | **correction banner** |
| `mechanism.md:182` | `NOT established as a red herring` | **corrected** — the narrow form |
| `session-verification.md:27` | `~~The no-trap A2g run~~` | **struck through**, corrected in place |
| `session-verification.md:33` | `"trap is not necessary" claim is withdrawn` | **the withdrawal statement itself** |
| `plan:121,124,158,177,261-262` | `no-trap` label | **defect lists / refuted-hypothesis lists / the corrected ledger** |
| `plan:202,255` | `predates the A4b2 trap work` | **corrected** — the narrow form |
| `plan:292` (`no trapped time`), `plan:1365` (`no TRAP/TRACE in env`), `plan:2542` (`without the trap`), `session-verification.md:94` (`assuming the trap caused them`) | pattern false positives | **I agree with all four of the Session's classifications** — each is unrelated text that merely contains the substring; none asserts the withdrawn claim |

**I found no hit the Session misclassified, and no fifth false positive.** Their four hand classifications are
correct. `plan:292` really is the A2h *horizon* (`no trapped time past the prefix`); `plan:1365` really is the
R0 profile statement; `plan:2542` really is the A3-era `U4` frame-pattern note; and
`session-verification.md:94` really describes what the *packet* does ("investigates the cause rather than
counting OOMs or assuming the trap caused them"), which is the opposite of asserting trap causation.

## R3.2 `plan:255` — the claim is fixed, **but the fix introduced a malformed table row**

**The claim itself is correct.** `plan:255` now reads *"the earliest OOM run is five days and one build
earlier … so **the failure predates the A4b2 trap work** … **The trap is NOT shown to be unnecessary** (all 35
runs carrying this request are trapped)"*. `git blame` confirms `7d798af`. **The withdrawn claim is gone.**

**However, the edit left a stray cell separator, and the row is now structurally malformed:**

```
L253: | Record | Establishes |            <- header, 2 columns (pipes=3)
L255: | `docs/reviews/a2h-mechanism.md` | … are trapped) |; the chain is `NtAllocateVirtualMemory` … |   <- pipes=4
L256: | same, §4 | … |                    <- pipes=3
L257: | same, §5 | … |                    <- pipes=3
```

- **Provenance: introduced by `7d798af`.** `plan:255` had **3 pipes at `dd96963`** and has **4 at `7d798af`**.
  The whole table was well-formed before this commit; every other row (L254, L256, L257) still has 3.
- **It is the only such row in the file** — a repo-wide search for `\|\s*;` returns exactly this line.
- **Effect:** the row now declares **three cells against a two-column header**, so the trailing text
  (`; the chain is NtAllocateVirtualMemory … Two stacked defects: an implausible 571 MB commit, and no NULL
  check on the result. The arena behaves correctly`) renders as a **third column** that the header does not
  declare — in a strict CommonMark renderer it is dropped or spills outside the table.
- **Mitigating, and I checked this rather than assuming it:** the **corrected claim is inside cell 2, before
  the stray pipe**, so it renders normally; and the displaced text is **not** a re-assertion of the withdrawn
  claim (it is the chain description and the two-defect summary, both still true and both preserved in
  `a2h-mechanism.md:56-66` and `plan:202-206`). **So this is a presentation defect, not a truth defect.**
- **No content is lost from the repository**, and no criterion depends on this table's rendering.

**Why it still matters enough to report.** This is the **third consecutive round** in which a repair to this
one claim carried a defect of the same family — a change applied to the sentence in front of the author
without checking the structure it sits in. Round 1 fixed banners and missed the bodies; round 2 fixed a
sentence and missed the ledger beneath it; round 3 fixed the sentence and ledger and malformed the table row
above them. The pattern is worth naming for the Session, and the fix is a **one-character deletion**.

## R3.3 The error ledger — fixed, and correctly

`plan:259-264` now reads **"Three Session errors in that analysis are recorded and corrected in place"** and
names all three, including *"**and the 'no-trap A2g' characterisation in this section's own table above — the
A2g run IS trapped, so the strong 'the trap is not necessary' claim is withdrawn, and only the narrower
'predates the A4b2 trap work' and 'not trace-caused' claims survive.**"*

This was **not** one of my reported locations — the Session found it by acting on my observation that the
survivor sat four lines above a ledger that omitted it. **Correct, and the right generalization.** A reader no
longer meets an uncorrected finding followed by an error list that omits it. **Confirmed fixed.**

## R3.4 The census still reproduces — confirmed (third independent run)

| Quantity | R1 | R2 | **R3** |
|---|---|---|---|
| run directories | 719 | 719 | **719** |
| carrying `598869040` | 35 | 35 | **35** |
| **trapped** | 35 | 35 | **35** |
| **untrapped** | 0 | 0 | **0** |
| positive control (trap setting absent/`0`) | 657 | 657 | **657** |
| …of those, carrying the request | 0 | 0 | **0** |

**Unchanged.** The absence claim retains its coverage witness.

## R3.5 Criterion 1 remains `AGREED` and was not reopened

`git diff --stat e1dd3e2..HEAD -- scripts/ src/ config/ CMakeLists.txt` is **empty** — **no code has changed
since I ruled criterion 1 `AGREED`**, so nothing reopens it (§2.4.7). As a sanity check only, the suite still
reports **`Ran 31 tests … OK`, exit 0**. Files touched by `dd96963..HEAD` are
`a2h-oom-causal-slice-evidence.md`, `plan-jsrf-bare-minimum.md` and my own review — documentation only.

## R3.6 Round-3 disposition

**(i) `plan:255` fixed and the ledger names three errors?** **YES to both** — the claim now states the narrow
form and the ledger names the A2g error explicitly. **But the same edit introduced a malformed table row at
`plan:255` (4 pipes against a 2-column header), which is a new defect introduced by the commit under
verification.**

**(ii) No surviving assertion of the withdrawn claim?** **CONFIRMED — none.** My independent semantic sweep,
run repo-wide and with each hit classified, finds every occurrence inside a corrective frame or a
correctly-identified false positive. **I agree with all four of the Session's false-positive classifications
and found no misclassified hit.**

**(iii) The census still reproduces?** **YES** — 35 trapped / 0 untrapped, 657-run positive control, third
independent run.

**DISPOSITION (round 3): NOT ACCEPTED** — criterion 1 `AGREED` (unchanged, not re-reviewed); criterion 2
`DISAGREED` **only** on the newly-introduced malformed row at `plan:255`.

### What the remaining repair needs

**Delete one character.** `plan-jsrf-bare-minimum.md:255` contains `… are trapped) |; the chain is …`. Removing
the stray `|` (so it reads `… are trapped); the chain is …`) restores the 2-column structure and renders the
displaced text inside cell 2, where it belongs. **No re-measurement, no re-sweep of the claim is required** —
the claim itself is correct and fully swept.

**I state plainly what this means for acceptance:** the *substantive* defect that took three rounds — the
false "no-trap" characterisation and its withdrawn strong claim — is **now fully corrected**. Every assertion
of it is either removed, corrected to the narrow form, or inside an explicit correction frame, and the census
holds. The only thing standing between this packet and `ACCEPT` is a malformed markdown table row introduced
by the final fix. If the Session prefers, a Planner or Advisor could reasonably rule that one-character
presentation defect non-blocking (§3.1/§3.2) — **I am not permitted to make that call (§2.2.6), and I am not
making it.** My role is to report it.

## Round-3 reproduction commands

```powershell
git log --oneline -3 ; git status --porcelain                  # HEAD = 7d798af, clean
(Get-FileHash docs\packets\a2h-oom-causal-slice.md -Algorithm SHA256).Hash   # E9CDB1B3…41108C
git diff --stat e1dd3e2..HEAD -- scripts/ src/ config/ CMakeLists.txt        # empty: criterion 1 not reopened

# (i) the fix and the ledger
(Get-Content plan-jsrf-bare-minimum.md)[254]                   # the corrected narrow claim
(Get-Content plan-jsrf-bare-minimum.md)[258..263]              # "Three Session errors …"
git blame -L 255,255 --date=short -- plan-jsrf-bare-minimum.md # 7d798afa

# the NEW defect: pipe count on the row (3 = well-formed, 4 = extra cell)
foreach ($i in 254,255,256) { $l=(Get-Content plan-jsrf-bare-minimum.md)[$i]; "L$($i+1): $(([regex]::Matches($l,'\|')).Count)" }
git show dd96963:plan-jsrf-bare-minimum.md | Select-Object -Skip 254 -First 1   # pipes=3 BEFORE the fix
git show 7d798af:plan-jsrf-bare-minimum.md | Select-Object -Skip 254 -First 1   # pipes=4 AFTER
Select-String -Path plan-jsrf-bare-minimum.md -Pattern '\|\s*;'                 # only plan:255

# (ii) my own semantic sweep, repo-wide
Get-ChildItem . -Recurse -Include *.md -File |
  Where-Object { $_.FullName -notmatch '\\logs\\|build\\|a2h-oom-causal-slice\.md|acceptance-review\.md|operating-history' } |
  ForEach-Object { Select-String -Path $_.FullName -Pattern '(?i)no[\s\-_.]*trap|not (the|a) cause|trap is not|trap not|without the trap|trap-caused|red herring|necessary cause' } |
  ForEach-Object { "$(Split-Path $_.Path -Leaf):$($_.LineNumber)" }

# (iii) census, third run — 35 trapped / 0 untrapped, 657-run positive control
#   read `settings` AND the log's "trapped for MMIO" line for every dir under logs/runs/

# criterion 1 sanity only (not a re-review)
python -X utf8 -m unittest scripts.test_a2h_oom_slice           # Ran 31 tests ... OK
```
