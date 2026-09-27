# `A4b2-NR` — Session mechanical verification and promotion

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-nonreliance-discovery.md`.
**Authoring Planner:** child `2565775c-9aa9-436b-b917-1efb2d01784a`, `codex/gpt-6-sol` @ `high` (§1).
**Class:** discovery (§5.8). **Adequacy:** the **writing Planner's own** review, per §5.8 and §5.3's
discovery path — **`VERDICT: ADEQUATE`**, `BLOCKING: NONE`.

`docs/agent-workflow.md` §5.1.5(2) makes the Session responsible for the mechanical parts and requires it
to **verify that every command runs before submitting the revision**. Every row below is a **Session
observation** — a command actually run, or a file actually read, in this session.

## Revision history within this round

| Revision | SHA-256 | Lines | Disposition |
|---|---|---|---|
| `A4b2-NR-r1` | `A0E8C7B2641CA0B7C55B6B3426BD47BDCAFB21EE8B3624627A95243149F17612` | 33 | **one blocking defect found by the Session** (below); revised |
| **`A4b2-NR-r2`** | **`7EF30508CCC2588748EAA92E9B56B12D038D425CA925CA360AA9059DFAF33E38`** | **33** | **frozen and promoted** |

## The defect the Session found in `r1`, and why it was blocking

`r1` required *"unit/fixture-test each selector (including alias, GP-only, count and rejected mode) before
runs"*, while its declared toolkit write scope was exactly five `src/apu/` files followed by **"no other
toolkit or game source."**

**Measured:** the only host that can fixture-test APU watch / GP-input behaviour is the toolkit's
`tests/apu_watch_fixture_test.c`, built by `CMakeLists.txt:133-145` as `jsrf_apu_watch_fixture_test` with
the two arms `jsrf_apu_watch_fixture_trace_on` / `_trace_off`. That file is **outside** the declared scope,
and adding ctest arms needs `CMakeLists.txt` — also outside scope.

**Why blocking, not advisory (§3.1):** the packet's own Leg 2 text makes the fixture half load-bearing —
*"independently the target selectors' fixture plus live changed counters/fingerprints must establish
input-hook activity; failure of either is INCONCLUSIVE, never invariance."* A literal executor would have
to skip the fixture tests or edit out-of-scope files (§2.2.3 forbids the latter; §2.2.2 makes a
step that cannot be performed as written a blocker). Either way the `L2=INVARIANT` row loses a control the
contract requires for it — a concrete path to a false `O-TWO-LEG` from an unverified hook.

**Repair chosen by the Planner (option (a)):** extend the scope to name the fixture host and the CTest
registrations, and **process-isolate** the cached-env modes so one process exercises one arm.

## Verified commands and prerequisites (`r2`)

| Item | Check | Result |
|---|---|---|
| `run-jsrf.py --profile exploratory` | `--help` shows `--profile {strict,exploratory,fixture}` | **runs as written** |
| All five `src/apu/` scope files exist | `Test-Path` each | **all present** |
| `tests/apu_watch_fixture_test.c` exists | `Test-Path` | **present** (now named in scope) |
| `RECOMP_APU_GP_INPUT_PERTURB` is launchable | `jsrf_run_profile.RETIRED_OVERRIDES` = `{RECOMP_VBLANK}` only | **not refused at launch** |
| The runs will be labelled correctly | classifier records the **requested** profile (`jsrf_run_profile.py:432,511-521`) | **`--profile exploratory` honored** |
| Image `I` reads offline as specified | `default.xbe[0x1A7D60+4i] & 0xFFFFFF` for `i<0x173` | **371 words, 355 nonzero**; first `0BF080 000155 300600 310000 62F400 000800 …` |
| `I[5] = 0x000800` vs the doorbell's `dsp_addr=000800` | measured | **coincides** — the packet correctly treats it as a **lead, not proof** |
| The comparison window exists | r7 R1: `GP_CLEAR` at frame 256, counts continue to frame **768**, crash at line 17434 of 17436 | **substantial pre-crash window** |
| The decoder really is compiled out | `dsp_cpu.c:446` `disasm_instruction()` exists and fills `disasm_str_instr` (`dsp_cpu.h:126`), but its only output is `DPRINTF`, gated `DEBUG_DSP 0` (`debug.h:25`); `dsp_disasm_memory` (`debug.c:41`) is a raw hex dumper behind `#if DEBUG_DSP`, declared in no header | **confirmed** — the packet's "expose it diagnostically" step is necessary |
| No offline DSP disassembler exists | searched both trees | **confirmed** — `tools/disasm` and `inspect-jsrf.py disasm` are x86/XBE (capstone), not DSP56300 |
| Both GP input read paths are hookable in scope | `dsp_cpu.c:905-923` — mixbuffer at `0x1400..0x17ff` **and** its `0xc00..0x0fff` alias; `dsp.c:52-90` `read_peripheral` | **confirmed** — the packet names **both**, including the alias |

## Session findings passed to the Planner during planning

Three Session measurements were sent to the Planner and are reflected in the frozen text:

1. **No offline DSP disassembler exists; the built-in one is compiled out.** The packet now scopes a
   diagnostic env-gated decode of the 371 loaded P words.
2. **Image `I` is readable offline, and `I[5]=0x000800` coincides with `dsp_addr=0x000800`.** The packet
   records this as a **lead, not proof of the binding** — correct.
3. **The `0xc00` mixbuffer alias is a second read path.** The packet requires perturbing **both**
   `0x1400..0x17ff` and `0x0c00..0x0fff`.

It also adopted the Session's r7 §7 **log-reconstruction rule** verbatim in kind: strip *all* injected
`[TAG]` records **including their newline**, bound each block at its next `[GPIN]` header (including
`summary`) and each field at its own key/expected array length, reject non-GPIN trailing text, and **do not
use a hand-written foreign-tag list** — the exact defect that produced three false readings in r7.

## Promotion

`A4b2-NR-r2` at **`7EF30508CCC2588748EAA92E9B56B12D038D425CA925CA360AA9059DFAF33E38`** was **frozen and
promoted into `CURRENT PACKET` in the same step**, byte-identical with no revision (§5.3). The file still
reads `Status: draft` in its own text, exactly as every previously frozen packet does.

**For a discovery packet, §5.8 makes the writing Planner's own review the adequacy verdict** — no second
Planner is spawned, and none was. The Muse Advisor's shape preflight was **not** repeated: the mechanism is
the one the Advisor itself mandated, and no policy/architecture question arose during planning.
