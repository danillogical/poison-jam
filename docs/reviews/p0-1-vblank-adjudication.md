# P0.1 VBLANK adjudication and bounded correction — 2026-09-23

Resolves the P0.1-AC1/AC2/AC3 review disagreement recorded in
`docs/reviews/p0-1-final-review.md`. Advisor call is final under
`docs/agent-workflow.md` §2.5. Contract revision remains **`P0-AC-r1`**,
SHA-256 `E91A44E0F2DC2CB15FA217ED79B5DC17C846A68270A2DD8A2B865667E55C2C10`,
**unchanged** — no criterion text was rewritten.

Advisor: `workbuddy-ai/kimi-k3`, child `9a744bd6-d8fe-4689-8480-be00bffcf006`
(persistent advisor for this session; substituted for `codex:gpt-6-astra` by
direct user instruction after Astra exhausted its tokens). Marker
`P0-KIMI-ADVISOR-MARKER-20260923-QX7T4` verified on the same child across three
turns. Route read from the child's own `subagent/descriptor`.

## The disagreement

| Position | Claim |
|---|---|
| Session / classifier author | `RECOMP_VBLANK` has no runtime semantics left, so it cannot make a run exploratory; AC1 asks for *actual* semantics. Correct the stale policy text. |
| Acceptance reviewer (DISAGREED on AC1/AC2/AC3) | `AGENTS.md` is active policy and still names `RECOMP_VBLANK` as synthetic completion. The strict gate consumes the classifier enumeration, so `RECOMP_GPU_ACK=0` + `RECOMP_VBLANK=1` is accepted as strict. That is a real coverage gap. |

**Both positions were partly right, and the reviewer's measurement was correct.**
Measured this session before the ruling:

- `classify_settings([{RECOMP_GPU_ACK:'0'}, {RECOMP_VBLANK:'1'}])` → `strict`, `reasons=[]`.
- A versioned archive recording those effective settings reclassifies as `strict`, `reasons=[]`.
- `tests/test_run_profiles.py` contained **0** references to `VBLANK` or any retired name.

## The advisor's ruling

1. **The classifier keeps its five-variable semantic enumeration.** Encoding
   `RECOMP_VBLANK` as permanently exploratory would contradict AC1's own stated
   method, and AC1's evidence enumeration already omits it.
2. **The launch sentinel fails closed on retired names**, which closes the
   reviewer's real gap by name-recognition rather than by semantic fiction.
3. **`AGENTS.md:22` is the actual defect** and is corrected to point at
   `docs/jsrf-run-profiles.md` as the single authority for override classification.
4. **Archive handling is revision-aware**: a retired override's contribution is
   decided by whether the *recorded binary* could still honor it.
5. **This is an implementation change inside frozen `P0-AC-r1`, not a criterion
   change.** AC1 commands the semantics-driven method; AC2's "prohibited effective
   settings" does not reach `RECOMP_VBLANK` because it is not an *effective* setting
   in the current binary; AC3 fidelity is improved without altering verdicts. The
   reviewer's concern is valid and is closed with controls; the reviewer's proposed
   *remedy* (encode VBLANK in the classifier) is rejected as contradicting AC1's
   method. The gap is the contract's own "insufficient exercise/coverage" →
   `UNKNOWN` sub-condition, not a `FAIL` requiring criterion edits.

### "Retired" is a conjunction, and deliberately narrower than "unknown"

The advisor's first wording was "reject any retired/**unknown** name". The session
measured a counterexample and put it back to the advisor, which **withdrew the
unknown clause** (option A confirmed). The measurements that decided it:

- The current toolkit runtime reads **43** `getenv` names (**39** `RECOMP_*`) —
  more than the profile document's 32. A name-based allow-list would reject real
  current flags (`RECOMP_KERNEL_WATCH_ALL`, `RECOMP_CS_MODE`, `RECOMP_PB_EXEC_VERBOSE`, …).
- `AGENTS.md` itself instructs every session to set `RECOMP_KERNEL_LOG_BUDGET`,
  which is not in the classifier's enumeration.
- Across all 649 archived runs, **18** distinct `RECOMP_*`/`JSRF_*` names appear,
  **7** of which the profile document never names — including `JSRF_COLLECTED`
  and `JSRF_LOG_PATH`, which `run-jsrf.py` sets itself.

So a name the runtime does not read **cannot** make a run exploratory. A retired
name qualifies only when **all** hold: documented as removed; still asserted as
synthetic by active policy; verified inert in the current binary; verified honored
in an archived binary. `RECOMP_VBLANK` is currently the only such name.

## The measured basis for revision-awareness

| Fact | Evidence |
|---|---|
| `e3caa37` (2026-09-07) added `RECOMP_VBLANK` as a real `getenv` gate | `git show 18a0837:src/kernel/kernel_bridge.c` line 2054; `git show e3caa37` |
| `7cfbe55` (2026-09-22) removed it | toolkit commit subject; source grep 0 matches at `484887b` |
| The archived A2 run's binary honored it | recorded toolkit revision `18a0837`; `e3caa37` **is** an ancestor (exit 0), `7cfbe55` is **not** (exit 1); the string is present in its archived `jsrf_recomp.exe` |
| The current binary does not | `484887b` **is** a descendant of `7cfbe55` (exit 0); string absent from `build\Release\jsrf_recomp.exe`, present in the archived one |

## Bounded correction applied

`scripts/jsrf_run_profile.py`:

- `RETIRED_OVERRIDES` — one dated row with add/remove boundary commits and a note.
- `resolve_retired_overrides()` — `git merge-base --is-ancestor` against the
  boundary commits; returns `honored_by_that_binary` as `True`/`False`/`None`.
  `None` (unreachable toolkit, unknown revision, non-hex revision) is **distinct
  from `False`** and yields `UNKNOWN`, never a silent clean.
- `classify_settings()` — annotates retired names without changing the verdict,
  because current-binary semantics are unchanged by them.
- `validate_launch_profile()` — **strict launch refuses a retired name**, before
  any child starts. Unknown-but-unread names still launch.
- `apply_retired_overrides()` — archive side: honored ⇒ contributes `EXPLORATORY`;
  already-inert ⇒ benign annotation, verdict stands; undecidable ⇒ `UNKNOWN`.
- `recorded_toolkit_revision()` — reads either metadata schema shape
  (top-level `toolkit_revision`, or `repository_identities.toolkit.revision`).

`docs/jsrf-run-profiles.md` — new "Retired overrides" section; named single
authority. `AGENTS.md` — stale sentence corrected to three still-active names.
`tests/test_run_profiles.py` — 19 → **27** tests: registry population, annotation
without verdict flip, launch rejection, **over-rejection controls** (observation
and unread names must still launch), revision-following annotation, unresolvable
revision ⇒ `UNKNOWN`, the three legacy-archive outcomes, and an inert-when-absent
control.

## Second defect, found by this session's own falsification pass

The re-review brief asked whether a strict record could still carry a retired name
in its *effective* but not *inherited* settings. It could:

```
make_profile_record('strict', inherited=[GPU_ACK=0], effective=[GPU_ACK=0, RECOMP_VBLANK=1])
  -> RECORDED strict        # before the fix
```

The launch gate inspected only inherited settings, and `_same_profile_settings`
covers only the five semantic variables — so an effective setting that *gained* a
retired name was recorded as strict. Fixed in both directions:

- `retired_names_in()` helper;
- `make_profile_record()` refuses a strict record whose effective settings carry a
  retired name;
- `_valid_new_archive()` reclassifies such a strict archive as `UNKNOWN`.

Controls preserved: clean strict still records; a strict record carrying
`RECOMP_KERNEL_LOG_BUDGET` (observation, not retired) still records; an
**exploratory** record legitimately carrying `RECOMP_VBLANK` still records. Two
regression tests cover it; the suite is now **29 tests**.

## Independent re-review — AGREED on AC1/AC2/AC3

Reviewer `workbuddy-ai/hy4-preview-f` @ `high` (child
`6aeaa0fa-9335-43e4-b516-2f8ae6915952`), a different model family from the
session, reviewing revision `E13050B8…` / `A64AECB9…`:

| Criterion | Disposition | Basis |
|---|---|---|
| P0.1-AC1 | **AGREED** | Per-variable semantics encoded at `jsrf_run_profile.py:380-413`; retired names correctly *excluded* from the semantic enumeration and resolved revision-relatively |
| P0.1-AC2 | **AGREED** | Negative control reproduced (retired name → `ProfileError`); over-rejection control passes (`RECOMP_KERNEL_LOG_BUDGET`, `RECOMP_MMIO_TRACE`, and an undocumented name all still launch strict); rejection precedes **any** child — `run-jsrf.py:236-240` is before build-identity, mkdir, archive and `Popen` |
| P0.1-AC3 | **AGREED** | Reclassification reproduced; `honored_by_that_binary` is `None` — never a silent `False` — for a missing tree, `None`, or a garbage revision |

The reviewer reproduced the sweep (649 archives, 1 retired, **0 verdicts changed**),
29 tests, the archive-wide tally, and identity verify. It also **withdrew** its own
earlier `make_profile_record` reading as having been taken against the superseded
`5E544F34` bytes — the right handling of a mid-review revision change.

Falsification attempts that failed to break the correction: effective-but-not-
inherited retired name (refused); exploratory/fixture records legitimately carrying
`RECOMP_VBLANK` (still record); a registry-stripped differential over all 649
archives; a forged archive with the retired name in `effective` (→ `UNKNOWN`).

### Residual gap found by the reviewer, and closed

The reviewer reported one **defense-in-depth** gap without raising it to a
`DISAGREE`: `_valid_new_archive` checked `retired_names_in(effective)` but not the
inherited set, so a *hand-forged* archive with `inherited=[GPU_ACK=0, VBLANK=1]`,
`effective=[GPU_ACK=0]` cleared every strict gate — `classify_settings` correctly
reports `strict` there, because a name the current binary never reads is not
"active". It is **unreachable through `run-jsrf.py`** (`make_profile_record` calls
`validate_launch_profile` on the inherited set first, verified by the reviewer),
but the checker must not accept it either.

Closed symmetrically rather than carried as a known hole, because it is the same
guard in the same function. The reviewer noted it could not drive a byte-complete
forged archive all the way to a printed verdict; this session then built one, so
the end-to-end result is **measured rather than inferred**:

```
forged strict archive, inherited=[GPU_ACK=0, VBLANK=1], effective=[GPU_ACK=0]
  -> UNKNOWN, reason: "strict archive records retired override(s): RECOMP_VBLANK"
```

Negative control: the same forged archive **without** the retired name still
classifies `strict`, so the check is not over-broad. Two regression tests cover it
(31 tests total).

## Verification (all re-run after the change)

| Check | Result |
|---|---|
| `tests/test_run_profiles.py` | **31 tests, OK, exit 0** |
| A2 vblank-probe reclassification | **EXPLORATORY**, citing `RECOMP_VBLANK was still honored by recorded toolkit revision 18a0837…` |
| Strict baseline reclassification | **STRICT**, unchanged |
| Archive-wide reclassification | 651 checked; 1 strict, 443 exploratory, 206 unknown, 1 missing — **no verdict change** |
| Retired-layer non-regression sweep | 649 archives: **1** carries a retired override, **0** verdicts changed |
| Strict record/archive with a retired name | **refused / UNKNOWN** on either side (was: accepted) |
| Forged archive, retired name in inherited only | **UNKNOWN**; same archive without it **STRICT** |
| `build-identity.py verify` | exit 0 |
| Release CTest (full suite, re-run after the change) | **12/12 passed**, 21.5 s |

The last three rows are the controls that matter: the layer is *annotative* for
existing evidence and changes no verdict, while the strict paths now reject the
exact combination the reviewer named — including both the effective-settings path
the first correction missed and the inherited-only path the re-review found.

### Evidence hashes at the re-reviewed revision

- `scripts/jsrf_run_profile.py` (re-review revision) `E13050B8464A0EEC991A1D0231DC99D61C4E3AEA2E95D3732A5A6006E10C0BA1`
- `tests/test_run_profiles.py` (re-review revision) `A64AECB90926536DA59FBCB1374EBFF56108F0FEFA1D2E5F2EF070C66DAADDB7`
- `docs/jsrf-run-profiles.md` `AED9918BE188F03306A32BDDB0D786614AEDF63690BF017920407700BA5B9125`
- `AGENTS.md` `409D1D2DE09DBD4A26DA86AA782506A5D168F7F96D3767D21BB3FFE40B2627A4`
- `logs/p0-1-profile-tests-vblank.log` `D1292522D4887A3649E3D37117B2BEF7140ABB7F0C2DD6F69374CF7BA68C4A89`
- `logs/p0-1-vblank-reclassify.log` `5072FD41848DB2646E76D3A85510EA9F819ABD7F7E9AAE540E43D7DA638BB2C5`

**The two source hashes above are the revision the reviewer AGREED on.** The
inherited-side hardening applied afterwards is a further post-review edit; its
own hashes are recorded in `report-deepseek.md`. Under the contract's rule, that
edit reopens the affected IDs again, so P0.1 is **not** accepted on this record —
the AC1/AC2/AC3 AGREEMENT covers the revision the reviewer saw, and the delta must
be assessed before acceptance.

### Delta revision (after the inherited-side hardening)

- `scripts/jsrf_run_profile.py` `D3C13AF75234539BC9875D66D235C0AE500C2FBF433369E1E4657B296E5305E9`
- `tests/test_run_profiles.py` `EAB616F5D9425294CDF296BD4D9FA7E2FDBEA37FB1B6EAB17102E5E326CB2AE3`
- `logs/p0-1-profile-tests-vblank.log` `D4D2FC9C34B096ACEC05DFF0459E6B5158AAF5CC824F1289C25106B1AEF662AE`

Measured on this revision: **31 tests OK, exit 0**; archive-wide tally unchanged
(651 checked; 1 strict / 443 exploratory / 206 unknown / 1 missing); forged
inherited-only archive → `UNKNOWN` with the retired reason, and the same archive
without the retired name → `strict`; `build-identity.py verify` exit 0.

**Delta assessment returned: dispositions UNCHANGED — AC1, AC2 and AC3 all
AGREED, carried over.** The reviewer re-ran its own falsification set against
`D3C13AF7` and independently reproduced both requested confirmations: the
symmetric check is **not** over-broad (its own byte-complete forged archive with
the retired name in `inherited` only → `UNKNOWN`; the same fixture without it →
`strict`; a strict archive carrying the observation override
`RECOMP_KERNEL_LOG_BUDGET` still records strict), and the effective-side rejection
still fires. It reported **no remaining path** by which a strict record or strict
archive can carry a retired name, closing its own residual note — including the
last theoretical route (a retired name in top-level `metadata['settings']` while
absent from `run_profile`), closed by the `top_settings == effective` check
together with the effective-side guard.

## AC4 bookkeeping reconciled

The final review AGREED on AC4 while noting that independent per-artifact
historical-claim disposition remained pending **in the manifest**. That
bookkeeping is now recorded in `docs/reviews/p0-1-historical-profiles.json` as
`per_artifact_disposition`, with an explicit `attribution` block separating what
the reviewer reproduced (classification and hash verification) from what this
session reconciled from the artifacts' own bytes and the original `claims` text.
It **reaccepts nothing**: each row keeps its historical strict-integration claim
WITHDRAWN, and the original `review_disposition` string is retained in
`review_disposition_history` rather than erased.

Verified for that reconciliation: all four recorded metadata SHA-256 values still
match their artifacts (the artifacts were not modified); all four classify
`exploratory` as expected; all four record toolkit revision `484887b`, which
**post-dates** the `7cfbe55` removal, so the revision-aware layer adds no note and
changes no verdict for any of them. The manifest is not hash-bound anywhere in the
evidence records, and its bytes changed only by this addition.

## Consolidated acceptance review — all five criteria AGREED

Reviewer `workbuddy-ai/hy4-preview-f` @ `high` (child
`ee31290c-bfb6-48a1-8305-f331f6098355`), different model family, falsification
pass on the frozen revision. Its first turn died with a **provider error**
(`PI_AI_ERROR`) and produced no dispositions; that is recorded as a route failure,
not a verdict, and the review was resumed on the same child.

`REVIEW_CRITERIA_JSON: [{"id":"P0.1-AC1","disposition":"AGREED"},{"id":"P0.1-AC2","disposition":"AGREED"},{"id":"P0.1-AC3","disposition":"AGREED"},{"id":"P0.1-AC4","disposition":"AGREED"},{"id":"P0.1-AC5","disposition":"AGREED"}]`

| Criterion | Disposition | Reviewer's own reproduction |
|---|---|---|
| P0.1-AC1 | **AGREED** | Semantics checked against the runtime sites it read directly (`xbox_memory_layout.c:684-686,1613`, `apu_dsp.c:61-76`); absent/`0`/`00`/`0x0`/`false`/`""` matrix reproduced; malformed/duplicate/`None` → `UNKNOWN`; `RECOMP_VBLANK` correctly excluded from the semantic enumeration |
| P0.1-AC2 | **AGREED** | `validate_launch_profile` at `jsrf_run_profile.py:445-472`; `run-jsrf.py:237` precedes `mkdir` (`:255/:261`) and `Popen` (`:329`); over-rejection controls pass |
| P0.1-AC3 | **AGREED** | Reclassification reproduced; `honored_by_that_binary` is `None` — never a silent `False` — for `None`, `''`, `not-a-rev`, `ZZZZZZZ` and a nonexistent toolkit root |
| P0.1-AC4 | **AGREED** | All four metadata hashes match; all four `exploratory`; artifacts unmodified (mtimes predate the correction); every row keeps `strict_integration_claim` WITHDRAWN; attribution split correctly and **no overclaim** |
| P0.1-AC5 | **AGREED** | `STRICT` exit 0, identity verify 0; metadata `requested_profile=strict`, inherited `RECOMP_GPU_ACK=0` only, `resolved_defaults` disabled; `result.json` `normal_exit`/0, `checkpoints_passed true`, 1.92 s, `HalReturnToFirmware(2)`; no boot/liveness/GPU wording found anywhere |

Falsification attempts that failed: forged archives with the retired name in
inherited / effective / both / top-level `settings`; a registry-stripped
differential over all 649 archives (**0 verdict changes**, 443 exploratory /
205 unknown / 1 strict either way); over-breadth controls including
`RECOMP_KERNEL_LOG_BUDGET`, `RECOMP_MMIO_TRACE`, an undocumented name and the
near-miss `RECOMP_VBLANKX`. **No path was found by which a strict launch, record
or archive carries a retired override.**

### Residual raised by the reviewer, assessed and deliberately NOT "fixed"

The reviewer noted that an environment name with a **trailing space**
(`RECOMP_VBLANK `) is settable on Windows and slips the exact-string launch guard,
and suggested normalizing names in `retired_names_in`. Assessed: this is
**correct behaviour, not a gap.** The runtime calls `getenv("RECOMP_VBLANK")`,
which matches exactly, so `RECOMP_VBLANK ` is a *different name the binary never
reads* — it cannot make a run exploratory, and under the advisor's ruling
(and the over-rejection control the reviewer itself passed) unknown-but-unread
names **must** keep launching. Stripping whitespace would start rejecting inert
names, i.e. re-introduce precisely the over-broad guard the advisor withdrew after
a measured counterexample. No change made; the reasoning is recorded rather than
the suggestion silently adopted.

The reviewer's other residuals are recorded as limitations, not defects: the
adjudication record was edited mid-review (record-only; every reviewed source hash
stayed fixed, and `CMakeLists.txt` has no dependency on the profile scripts); the
AC5 run ends early and is **not** guest-progress evidence; and no fresh guest launch
was performed for this review.

## P0.1 disposition

**All five mandatory criteria are AGREED on the frozen revision** — contract
`P0-AC-r1` `E91A44E0F2DC2CB15FA217ED79B5DC17C846A68270A2DD8A2B865667E55C2C10`,
unchanged. No `DISAGREED` or `CANNOT VERIFY` ID remains. Under
`docs/agent-workflow.md` §2.7 the packet is therefore **accepted**, recorded here
and in `report-deepseek.md` before P0.2 is selected. The acceptance covers the
revision named below; any further edit to these files reopens the affected IDs.

Accepted revision:

- `scripts/jsrf_run_profile.py` `D3C13AF75234539BC9875D66D235C0AE500C2FBF433369E1E4657B296E5305E9`
- `tests/test_run_profiles.py` `EAB616F5D9425294CDF296BD4D9FA7E2FDBEA37FB1B6EAB17102E5E326CB2AE3`
- `docs/jsrf-run-profiles.md` `AED9918BE188F03306A32BDDB0D786614AEDF63690BF017920407700BA5B9125`
- `AGENTS.md` `409D1D2DE09DBD4A26DA86AA782506A5D168F7F96D3767D21BB3FFE40B2627A4`
- `docs/reviews/p0-1-historical-profiles.json` `4F639BDABE2918D59C86C9D094754F0EAD4237E3292AA03761779C63425FA6DD`

All five were re-hashed **after** this disposition was written and are byte-identical
to the revision the reviewer evaluated.

**Self-reference, stated rather than hidden:** this document was
`F72507582E5D59CCC3D8244A7FE9E659B28682562A914B0DF32BCDEEBB630BB6` when the
reviewer accepted it, and is `7C11CAD4740806ED34E521FC1025324C0B87757E7D69372CEBFE4BADD0806040`
now, because the acceptance disposition above was appended to it. That is the
disposition record, not criterion-bearing evidence — the reviewer had already
accepted this file at `F7250758…` as a record-only edit and treated it as
corroboration. The five files listed above carry the criteria and none of them
changed. If a later reader wants the exact reviewed revision of *this* file, it is
`F7250758…`; the delta is the section you are reading.

This acceptance establishes **profile/provenance correctness of the classifier and
gate**. It is not boot, audio, GPU or liveness evidence, and it does not reaccept
any historical strict-integration claim.
