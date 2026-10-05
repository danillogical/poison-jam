# Turn plan (start) — `title-001`

Turn Planner baseline. Immutable after execution begins. Working artifact, not history.
Owner of the turn: the Orchestrator. It may reject or reorder anything below once evidence supports another route.

## 1. Objective for this turn

Advance **DoD-BOOT / M15** — reach the title screen and present the title frame — by clearing the
current fatal `[ICALL] Failed to resolve VA 0x0013B750`, and then, if that clears, by
characterising and attacking the **second, independent** reason the title frame has never appeared:
presents and GPU flips both stop dead at **exactly 1000** while the graffiti disclaimer is up.

The title screen must **not** be claimed reached without direct framebuffer evidence — a saved BMP
(or window capture) of the title frame plus its hash, in a run whose record lists the ledger IDs it
relied on. A resolved indirect call, a returning recovered body, or a moving guest counter is
**not** title-screen evidence.

## 2. Measured state and blocker

### Independently verified this turn (observed, with the command that shows it)

1. **Dispatch site confirmed.** `disasm 0x142C70 0x142CC0`: `0x142C9E call dword ptr [esi+0x88]`,
   `0x142CA4 push esi` next — so `0x142CA4` is that call's return address, inside `sub_00142C70`
   (span `0x00142C70..0x00142D0A`).
2. **`0x13B750` is a complete function**, `stack_args 0`: `push ebp`/`push esi` at `0x13B750`, **two**
   balanced exits at `0x13B7EE` and `0x13B808` (each with `pop edi`/`pop esi`/`pop ebp`). A 6-byte
   `nop` pad at `0x13B74A..0x13B74F`; next prologue `0x13B810`.
3. **Unowned in the analysis database.** `sub_0013B610` ends `0x13B74A`; no entry contains `0x13B750`;
   next is `sub_0013B810`. 198 bytes unowned.
4. **NEW — guest code installs the address; no table holds it.** In `sub_0013BB00`:
   `0x13BBDF push esi` / `0x13BBE0 push 0x13B750` / `0x13BBE5 push eax` / `0x13BBE6 call 0x142810`.
   `0x142810` is `mov [eax+0x88],ecx; mov [eax+0x8c],edx; ret` — **`obj->0x88 = fn`,
   `obj->0x8c = ctx`** — closing causally with `call [esi+0x88]`. The dword `0x0013B750` occurs
   **exactly once in the XBE**, at file offset `0x12BBE1` (VA `0x13BBE0`) — **misaligned**.
5. **Not the same sub-class as `0x13FAD0`.** `0x13FAD0` is a `.data` table slot at `0x22DB38+0x20`
   (f15 dump: `0022DB58: 0013FAD0`); `0x13B750` is a **`.text` immediate**. `check-table-targets.py`
   scans only `.data`/`.rdata`, so it **structurally cannot** list `0x13B750`. "Same class" is right in
   spirit but would misdirect a worker into re-running a pointer-table sweep.
6. **Dump corroboration** (f15, mapping gate 1 match / 0 mismatch): `memory 0x0026A528` =
   `0013B750 0027C720` → `esi = 0x26A4A0`, `[esi+0x88]=0x13B750`, `[esi+0x8c]=0x27C720`;
   `memory 0x01220F60` = `00142CA4 0027C720 …`. Also `[0x277180] = 0x22DB38`.
7. **Testable prediction.** The new body's inner call is `call [edx+0x18]` with `edx = 0x22DB38`, i.e.
   `0x13F9E0`, which **already resolves** (`recovered`). So the body should not immediately re-trap —
   the cheapest falsifier of "this was the only missing body on the path".
8. **The presents/1000 freeze is independent of `0x13B750`.** f9 (1203 s, `diagnostic_deadline`) has
   **zero** `[ICALL]`, zero `[ABI FAILURE]`, zero `[UNIMPL]`, and presents frozen at exactly 1000 from
   t=223 s to t=1193 s. f10–f13 likewise have zero ICALL and freeze at 1000; only f14/f15 have one
   ICALL each. **A run that never reaches `0x13B750` still never passes 1000 presents.**
9. **Flips stop with presents.** Last `[GPU] flips` is 983 (f9) / 976 (f15); that report runs on a 10 s
   cadence from the commit consumer (`nv2a_pb_exec.c:1028`). Its cessation means **no NV097 methods
   were committed afterwards** — the guest stopped submitting, stronger than "the host copy stopped".
10. **No host cap at 1000.** `s_present_serial` is an unbounded `InterlockedIncrement`
    (`fb_present.c:106`); the only literal `1000` there is the `[FBPHASE]` sample at line 174.
11. **Frozen state reproduces.** `memory 0x01063A70` in f10/f11/f15 all read `+0x10 = 0`,
    `+0x24 = 0x002A336E` at the deadline, with `[0x22FCE0] = 0x01063A70`. At the 1000th present `+0x24`
    is still 0, so that store happens **after** it. Do not clear `+0x24`.
12. **Class size re-measured.** `check-table-targets.py` reports **122** candidates (55 swallowed,
    67 uncovered), not the "~151" carried in the plan. `KNOWN_OPEN` is exactly **80** entries.
13. **`.text` installer pointers censused.** `push <codeptr>; [pushes]; call <fn>` gives 338 sites /
    53 distinct pointers; **10 remain unresolvable**: `0x20000`, `0x20034`, `0x30000`, `0x459e4`,
    `0x80000`, `0x100000`, `0x12f220`, `0x17c310`, `0x18c950`, `0x18ca10`. Several are plainly not code
    (`0x80000`/`0x100000` read as sizes; `0x20000`/`0x30000` decode as data); `0x18c950` and `0x12f220`
    look genuine. **A triage list, not 10 bugs.**

### CRITICAL — the carried-in task is already done; only the measurement is missing

**The Orchestrator has already implemented and built the `0x13B750` entry.** Uncommitted working tree:

- `config/recovered-functions.json` — new entry `0x0013B750..0x0013B810`, `routine`, `stack_args: 0`.
- `config/manual-functions.json` — `"0x0013B750": "sub_0013B750"` appended.
- `src/recomp/recovered/recovered.c` (gitignored) — `body_0013B750` + `case 0x0013B750u`; **3096**
  bodies / **3096** cases (was 3095).
- `build/Release/jsrf_recomp.exe`/`.map` (built 23:55:23) contain `body_0013B750`, `sub_0013B750` and
  the string `[RECOVERED] 0x0013B750 returned` — **the fix is in the built binary.**
- `resolution_starts` now reports `0x13b750 -> recovered`; both provenance records are amended.
- **No run has used that binary** — newest archive is f15 (21:22) vs exe (23:55). **Built but unexercised.**

So the first real action is a **run**, not more reverse engineering: the carried-in gate ("read the
call site first") is satisfied by §2.1/§2.4 and by the entry's own evidence string.

**Disclosure.** While verifying tooling I invoked `recover-functions.py` without a subcommand and it
re-ran. It was **idempotent**: `recovered.c` SHA-256 is `50b96fe9…5070a`, exactly the `new_sha256`
the provenance record already carried; `recomp_stubs_recovery.c` is unchanged. Content is unchanged —
but the mtime now exceeds `recovered.obj`, which **forces a rebuild**.

### Taken on trust (not independently re-derived this turn)

- That toolkit `6e6e056` (APC stack restore) and `8f6c597` (deferred file APCs) behave as recorded.
  Toolkit HEAD is `6e6e056` with a **clean** working tree; I did not re-run `xbox_file_apc_test`.
- That `0x13FAD0`'s split at game commit `d77474d` is correct. HEAD is `d77474d`; I read the table
  dword and the f15 `[RECOVERED] 0x0013FAD0 returned; ABI verified` line, but did not re-derive the span.
- The exact cause of the ~950 s disc-error dialog.

## 3. Proposed execution order

Critical path, cheapest-decisive first.

1. **Rebuild and re-verify the tree** (required anyway, see disclosure). `just build` then `just test`.
   Confirm the build is identity-checked and that `recovered.c`'s hash still equals the provenance
   `new_sha256`. This is a chore, not a finding.
2. **Run the already-built fix, long enough to get past 1000 presents.** Exploratory is fine for the
   bare minimum, but set `RECOMP_GPU_ACK` explicitly and record it. Use the raised cap
   (`MAX_RUN_SECONDS` 1800) and `RECOMP_FB_PRESENT_DUMP_EVERY` so the disclaimer-cleared frame, if it
   happens, is captured as a BMP. Expect `[RECOVERED] 0x0013B750 returned; ABI verified` and then
   either a new `[ICALL]` (path continues; read the next call site) or the present freeze (path ends
   at the independent blocker).
3. **Only if the run shows the freeze**: attack the freeze with the deciding measurements in §5.
   Do **not** return to `0x13B750` unless the run contradicts §2.7.
4. **Parallel (independent of 1–3):** worker A classifies the 10 remaining installed pointers and the
   `.text`-immediate class; worker B builds the freeze timeline from the already-archived f9–f15 dumps
   and logs. Neither blocks the run.
5. **Record.** Update the plan's **Current work**; add a ledger entry for any shortcut actually relied
   on; commit and push per `AGENTS.md` (toolkit first, then game). The `0x13B750` config change is
   uncommitted and must not be left across a session boundary.

## 4. Useful worker delegations

Workers run `workbuddy-ai/deepseek-v4.1-flash` @ `high`. Give each a bounded objective and a disjoint
output. A worker summary is a lead, not evidence — the Orchestrator re-checks load-bearing facts.

- **W1 — installed-pointer classification.** For each of the 10 addresses in §2.13: disassemble from
  the address and decide *function prologue / internal label / data misread as a pointer / kernel thunk
  / invalid*, naming which runtime table (if any) answers it. Deliverable: a table with bytes quoted.
  Do not add entries.
- **W2 — present-freeze timeline.** From the f9–f15 archives only (no new run): per run, the wall-clock
  second of the last present, last flip, last committed-method report and `[FATAL-*]` line, plus
  whether presents ever exceed 1000. Deliverable: a per-run table with line numbers. This is what makes
  H4's independence load-bearing.
- **W3 — `.text`-immediate census hardening.** Re-run the §2.13 scan restricted to targets passing a
  real-prologue test, and report the filtered count and false-positive rate with one hand-checked
  positive control. Replaces the stale "~151" figure with a defensible number.
- **W4 — submit-loop stop localisation.** From the f15 dump, name the call chain of the thread that last
  committed a method and whether it is blocked in `NtDelayExecution` (as f9's main thread was, under
  `sub_00013F80` from `0x6FA3C`). Deliverable: frozen stack words with addresses, explicitly not
  unwound frames.

## 5. Deciding measurements / tests

Each should be able to fail.

- **M1 — does the new body return?** Run the rebuilt binary to ≥ 400 s. **Falsifies §2.7** if the next
  log line is another `[ICALL] Failed to resolve` (the body's inner call did not resolve) or an
  `[ABI FAILURE]` on `0x0013B750`. **Confirms the fix** if
  `[RECOVERED] 0x0013B750 returned; ABI verified` appears and the guest continues past `0x142CAA`.
- **M2 — is the freeze reachable without the ICALL?** Already answered **yes** by f9 (§2.8). The
  remaining question is only whether *after* `0x13B750` returns the run still freezes at 1000. If it
  does, the fix is a real advance that did **not** unblock the title screen, and the freeze is the
  critical path.
- **M3 — guest-side vs host-side freeze.** Read `[GPU] flips` and `[FBPRESENT]` in the same run. If
  flips keep advancing while presents stay at 1000, the host present path is at fault; if **both**
  stop (as in f9/f15), the guest stopped submitting and the blocker is in guest state. This single
  comparison collapses the "1000 cap" family of hypotheses.
- **M4 — what stores `+0x24`?** `[FBPHASE]` reads `+0x24 = 0` at the 1000th present, while the deadline
  dump reads `0x2A336E`. Set an observation-only watch (`RECOMP_WATCH`, no guest write) on
  `0x01063A70+0x24` and record the storing instruction. **Falsifies** "the idle branch at `+0x24 != 0`
  stops the presents" (f12 already argues against it) and names the real transition.
- **M5 — is the freeze a timeout?** Compare the wall-clock gap between the last present and the last
  committed method against the 240 s pending-I/O threshold and the 720-update hold. If the gap is
  ≈240 s, the disc-error/pending-I/O path is implicated; if the update counters stop advancing at the
  same instant, the render loop itself stopped.
- **M6 — title-frame evidence.** Any claim of M15 requires a BMP of the title frame plus its hash, and
  an independent check that the frame is not the disclaimer (`5bdaea576b8509f5`). A hash change alone
  is not enough — it must be inspected.

## 6. Important competing hypotheses

Live, including ones the carried-in narrative under-weights.

- **H1 — `0x13B750` is a missing recovery entry whose boundary comes from the XBE.** *This is now the
  implemented state* (§2). Boundary `0x13B750..0x13B810`, `stack_args 0`, is byte-supported by two
  balanced `ret` exits and the trailing nop pad. Residual risk is only the end convention (`0x13B810`
  vs `0x13B809`) — low impact, since no pointer lands in `0x13B610..0x13B74A`.
- **H2 — the nop pad makes it a continuation or label rather than a function.** **Refuted.** No branch
  or call enters it from `sub_0013B610`; the pad follows a `ret` at `0x13B749`; and it is referenced
  separately from `0x13BBE0`. It is a distinct function.
- **H3 — the whole class of pointer-referenced methods is missing.** *Partially confirmed, smaller than
  feared.* Real but **not one defect**: `.data`/`.rdata` tables and `.text` immediates need different
  detectors, and the `.text` census is mostly false positives. **Under-weighted:** `check-table-targets.py`
  has a *structural blind spot* for `.text`-embedded pointers — the exact class that produced this
  blocker. Worth recording as a tooling finding.
- **H4 — the presents-stopping-at-1000 freeze is an independent second blocker gating the title screen
  even after `0x13B750` resolves.** **Strongly supported** (§2.8–2.11): f9 ran 1203 s with zero ICALL
  and never passed 1000; flips stop with presents. The carried-in narrative treats 1000 as a symptom of
  the ADX/ICALL path; the archive says otherwise. **The most under-weighted hypothesis, and the more
  likely owner of the title screen.**
- **H5 — the freeze is the disc-error/pending-I/O path.** Live. f9 reaches `[FATAL-FILE]` for
  `JSRF_FATAL.ERR`, but ~90 s *after* flips stopped, so the fatal is a consequence at most. The 240 s
  threshold is a candidate clock for the gap.
- **H6 — the freeze is the 720-update hold / the `+0x24` idle branch.** Weakened by f12/f15
  (`+0x24 = 0` at the 1000th present, update counters moving) but not excluded: the deadline dump shows
  a store to `+0x24` later in that same update.
- **H7 — the freeze is a rendering/submit defect unrelated to state-machine timing.** Live and
  under-weighted: the committed-method report stops too, pointing at the guest's render loop.

## 7. Likely places where Advisor input could help

Persistent Advisor is `claude/claude-opus-5-5` @ `xhigh`. These are suggestions, not gates.

- **If M2 shows the freeze survives the fix**: two credible root causes (H5 timeout vs H7 render-loop
  stop) remain after first measurements — a canonical Advisor trigger. Brief it with the M3/M5 tables
  and the f9 timeline.
- **Class-level decision**: whether the `.text`-embedded pointer class becomes a systematic recovery
  pass (and how to bound it honestly), or stays case-by-case. This is an architectural choice that is
  expensive to reverse and affects the `KNOWN_OPEN` policy.
- **If the fix returns but a *new* ICALL appears**: whether to keep walking the path or to switch to
  the freeze, i.e. which is the true critical path to M15.
- **Tooling blind spot**: whether `check-table-targets.py` should gain a `.text`-immediate mode, given
  it missed the address that caused this blocker.

## 8. Completion criteria for this turn

**Genuinely useful** (any of these, with the artifact named):

- The rebuilt binary runs and the log shows `[RECOVERED] 0x0013B750 returned; ABI verified`, with the
  guest advancing past `0x142CAA` — or a measured, evidence-backed refutation of §2.7.
- The presents/flips freeze is characterised with a **deciding** measurement (M3 and/or M5): a named
  mechanism, or a stated reason why the candidate mechanisms are excluded, backed by log/dump lines.
- The `0x13B750` config change, its provenance amendment, and the freeze finding are committed and
  pushed in the correct repositories, with the plan's **Current work** updated.

**A real (not synthetic) advance toward the title screen:**

- The fatal `[ICALL]` is cleared **and** the guest is measurably further along the boot path (a named
  new site reached, or a new file/method committed), **or**
- the freeze is reduced to a single named mechanism with a concrete next experiment.

**Not an advance, and must not be recorded as one:** an added dispatch entry with no run; a run where
the entry resolves but the guest state is unchanged; `byte+1 = 1` forced; `0x101` returned; a
`+0x24` clear; a widened run that only re-observes the disclaimer.

**Do not claim M15 reached without a title-frame BMP plus its hash**, and do not claim
"disclaimer-cleared" from a present count, a hash change, or a phase-object field.

## 9. Constraints restated (binding)

Do not use `JSRF_ALLOW_UNRESOLVED`; no guessed stack corrections; no arbitrary guest-memory writes; no
synthetic state advancement presented as a fix; no undocumented stubs. Any pragmatic shortcut must be
recorded in `docs/jsrf-compatibility-ledger.md` and listed by the run's record. Do not reopen the
`title.adx` file-APC fixes (toolkit `8f6c597`, `6e6e056`) or the `0x13FAD0` split (game `d77474d`)
without contrary evidence. Do not set `RECOMP_ASYNC_IO`; do not patch `kernel_file.c` or `0x1401B0`.
