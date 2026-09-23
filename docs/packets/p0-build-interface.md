# P0.5 safe build interface amendment

Revision `P0.5-interface-r1`, proposed for adequacy review. Implement only after
P0.4 acceptance. Supplements P0-AC-r1, preserving all four criteria.

**Decision, advisor approved:** the ordinary build compiles the specified existing
generated baseline. Remove unconditional recovery and fixture regeneration from
that path; `--regenerate` rejects before mutation until P0.7 provides its guard.
This proves safe build preservation, not reproducibility of old generation.

Before implementation pin all files under `src/recomp/gen/` and
`src/recomp/recovered/`, plus `tests/lifter-regressions.c`, in
`config/generated-build-baseline.json` with relative paths and SHA256. Missing,
added or changed members fail before build. Verify the same inventory after a real
configure/build. Protect instrumentation with an explicit marker inventory,
reviewed before implementation; hash preservation does not validate semantics.
No ordinary CMake/wrapper step may rewrite these production sources.

`jsrf_acceptance_build` is the CMake aggregate. A single CMake registration helper
calls add_test and records the associated target (or script/interpreter for a
Python test). It supplies aggregate dependencies and generates
`build/acceptance-inventory.json`; include game and collector as runtime targets.
Remove the Python wrapper's handwritten TARGETS list. Configured Python must be
the wrapper's explicit interpreter, also used for Python CTests.

Wrapper output `logs/build-acceptance.json` records resolved tools/versions,
configuration, normalized environment policy, target/test inventory, each step's
command/exit/log, and build identity. Compare its test names against
`ctest --test-dir build -C Release --show-only=json-v1`. Parent runs full CTest
with `--output-junit logs/p0-5-ctest.xml`; compare every discovered name exactly
once with reported pass/fail/not-run, retaining blocked tests in the denominator.
No green total can omit a discovered test or its owning native target.

`tests/test_build_jsrf.py` exercises missing tools/dependencies, nonpositive
parallelism, environment normalization, ordinary compile failure with no retry,
measured silent confined failure with at most one serial retry retaining both
logs, omitted aggregate target, NOTRUN/missing result, stale identity, changed
generated file and rejected regeneration before a generator sentinel runs.
Positive fixtures and missing/malformed inputs accompany each checker.

Actual acceptance: parent runs guarded wrapper then full CTest and identity verify;
records complete generated inventory before/after, every test/result, artifacts,
tools and configuration. Reviewer reproduces focused controls and checks retained
real build/test evidence. Unknown inputs/tool failures stay pending. The earlier
P0.S direct configure/build is a prerequisite procedure, not P0.5 acceptance.

Preflight dependency report includes the resolved interpreter/version, importable
capstone version/path (needed by the project tooling), CMake/CTest versions/paths,
and configured generator, compiler/toolchain path/version and Windows SDK from
the configured CMake cache/compiler metadata. Report absent/unknown values
explicitly; do not invent versions. Test missing dependencies before build.

Automatic serial retry requires all measured conditions: parallelism >1, exit 1,
no compiler/linker error diagnostic, terse log ending at `Checking File Globs`,
and retained diagnostic evidence of `ResolveProjectReferences` failure with
`0 Error(s)`. Without that last evidence, report the candidate signature without
retry. Preserve the first log and diagnostic source plus the at-most-one retry.
Ordinary compiler errors and arbitrary silent failures never satisfy this gate.

Before/after build-identity schema and `verify` bind source hashes, complete
artifact hashes (EXE, collector, PDB/map), configuration, tool paths/versions,
configured generator/toolchain and Python interpreter/path/version. Missing,
unsupported or mismatched identities fail closed; fixture-test each category.
The separate build-acceptance report references that identity rather than
substituting for it. Parent-pinned inventory is available for review in
`docs/reviews/p0-full-generated-baseline.json` and marker preservation inventory
in `docs/reviews/p0-abi-marker-inventory.json`; promote to config only once
reviewed. These guard the present instrumentation, not prove the guest ABI correct.
