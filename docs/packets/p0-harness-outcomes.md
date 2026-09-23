# P0.6 executable interface amendment

Revision `P0.6-interface-r1`, proposed for adequacy review. Implement only after
P0.5 acceptance; early P0.S save isolation stays a prerequisite already reviewed
under its own contract. This supplies concrete procedures for P0-AC-r1 AC1–AC4.

- Add an importable `scripts/jsrf_preflight.py` with scratch and disk access
  checks, used by `run-jsrf.py` before collector creation. `--scratch-root` selects
  a scratch parent (default run directory). Only a unique newly created probe
  file is written and removed; verify cleanup. Errors are ENVIRONMENT_BLOCKED,
  with operation/path/error preserved; no alternate API or location retry.
- Existing disk images: open read/write without truncation, close, compare hashes
  before/after. Do not touch the user profile during acceptance; positive and
  denied controls use disposable fixtures. A missing image in a newly created
  disposable root is explicitly NOT_CREATED, not a verified existing image.
- `tests/test_harness_permissions.py`: permitted scratch and image controls,
  denied scratch and image controls, missing/unusable root, cleanup failure,
  before/after bytes, and sentinel showing no collector after blocked preflight.
  Assert an operation/API/path trace: denial is attempted once against the chosen
  item and causes no retry through another API or path.
  The native P0.S ACL denial is complementary real Windows denial evidence.
- Disk denominator is the selected directory plus `Partition0.img` through
  `Partition5.img`. Emit a result for each. The seeded existing-disk positive
  fixture contains all six and each hash stays identical. A missing member of
  that declared existing set fails; the distinct new-empty-root mode reports all
  six NOT_CREATED, never silently omits them or claims existing-disk PASS.
- `scripts/jsrf_probes.py` owns the probe/checkpoint map; runner and test-harness
  import it. `tests/test_probe_expectations.py` enumerates every map entry plus
  ordinary launch, unknown probe, explicit override. Check map coverage against
  native dispatcher names so a newly added probe cannot disappear from tests.
- `result.json` adds a versioned structured outcome with independent launch,
  capture, profile and semantic fields. Launch = STARTED/ENVIRONMENT_BLOCKED/
  FAILED/NOT_STARTED; capture = COMPLETE/FAILED/NOT_ATTEMPTED; profile = the
  classifier result; semantic = NOT_EVALUATED unless a named semantic oracle
  actually evaluated it. Existing collector outcome stays raw evidence. A
  deadline or normal exit with zero does not set semantic PASS or liveness.
- Fixtures assert each axis, including normal exit with no semantic oracle,
  deadline, launch failure, blocked access, missing collector result, profile
  rejection and malformed result. Keep original raw artifacts and exit behavior
  documented; an environment block is nonzero and cannot count as test success.

Parent runs both named test files, the profile regression suite, and
`C:\Python313\python.exe -X utf8 scripts/test-harness.py`; retain the full
probe denominator and every result. Reviewer reproduces focused tests and checks
all archived probe results/root witnesses. Missing fixtures/artifacts are UNKNOWN.
