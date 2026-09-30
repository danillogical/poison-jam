# JSRF Windows static-recompilation: the mechanical host workflow.
#
# Plan T6.  Every recipe here is the *only* supported spelling of that step, so a
# record can cite `just <recipe>` instead of a command line that drifts.  The
# names are fixed by the plan and `scripts/check-agent-docs.py` fails if one is
# renamed or deleted.
#
#   just build          guarded Release build (identity-checked)
#   just test           build, then the full CTest suite
#   just ctest          CTest only, against the current build
#   just regen          full translation pass (regenerates src/recomp/gen)
#   just strict-run     strict-profile guest run (RECOMP_GPU_ACK=0 is set FOR you)
#   just explore-run    exploratory-profile guest run
#   just probe          bounded fixture-profile probe run
#   just check          every repository checker
#   just ttd-record     WinDbg TTD recording of one strict run (T1)
#   just ttd-writes     query a TTD trace for writes to a guest VA (T1)
#   just doctor         host/run environment report (T12)
#   just analyze        clang-cl and MSVC /analyze configurations (T5)
#   just disk           free-space gate and the retention plan (T14)
#   just secret-audit   scan reachable Git blobs for secret-shaped content
#   just symbols        XDK symbol database from the original XBE (T2)
#   just logq           DuckDB-backed log queries (T8)
#
# Two host facts this file exists to stop rediscovering:
#
#  * **strict runs need `RECOMP_GPU_ACK=0`.**  The runner refuses a strict launch
#    without it and never inserts it.  `just strict-run` sets it in the same
#    process as the launch, so the profile is strict by construction rather than
#    by remembering.  `docs/jsrf-run-profiles.md` is the authority on this.
#  * **builds must go through `scripts/build-jsrf.py`.**  This host exports
#    `Path`/`PATH`/`path`, and MSBuild's CL task dies with MSB6001 on a
#    case-sensitive dictionary built from that block.  Python's `os.environ`
#    collapses the variants, so the guarded path is the only one that works.
#    `just doctor` reports the hazard when it is present.

set windows-shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-Command"]
set shell := ["sh", "-cu"]

python := "python"
toolkit := "../xboxrecomp"

# Show the recipe list.
default:
    @just --list

# Guarded Release build; records source and executable identity.
build:
    {{python}} -X utf8 scripts/build-jsrf.py

# Configure, build, then run the full CTest suite.
test: build
    ctest --test-dir build -C Release --output-on-failure

# CTest only, against whatever is currently built.
ctest:
    ctest --test-dir build -C Release --output-on-failure

# Full translation pass: regenerate src/recomp/gen from the original XBE.
regen:
    $env:PYTHONPATH = "{{toolkit}}"; {{python}} -m tools.recomp game/default.xbe --all --split 1000 --gen-dir src/recomp/gen --game-name "Jet Set Radio Future" --manual-functions config/manual-functions.json --exclude-manual src/recomp_manual.c

# Strict-profile guest run; sets RECOMP_GPU_ACK=0 for you.
strict-run label="strict":
    $env:RECOMP_GPU_ACK = "0"; {{python}} -X utf8 scripts/run-jsrf.py --seconds 5 --label {{label}}

# Exploratory-profile guest run; labels it, adds no override.
explore-run label="explore":
    {{python}} -X utf8 scripts/run-jsrf.py --seconds 5 --label {{label}} --profile exploratory

# Bounded probe run (fixture profile).
probe name label="probe":
    {{python}} -X utf8 scripts/run-jsrf.py --seconds 5 --label {{label}} --probe {{name}}

# Every repository checker; any non-zero exit fails the recipe.
#
# `check-dump-controls.py` is NOT here on purpose. It is a data-quality REPORT over
# the run archive: it exits nonzero when any archived dump has a CONTENT_MISMATCH,
# and 79 historical dumps do. `docs/jsrf-run-profiles.md` is explicit that such a
# dump "is still structurally readable: read it at its actual guest VAs. Do not
# shift reads and do not discard it" -- so a nonzero exit there is a finding about
# the archive, not a failed check on the current tree. It has its own recipe.
check:
    {{python}} -X utf8 scripts/check-agent-docs.py --check; if ($LASTEXITCODE -ne 0) { exit 1 }
    {{python}} -X utf8 scripts/check-merge-structure.py; if ($LASTEXITCODE -ne 0) { exit 1 }
    {{python}} -X utf8 scripts/check-generation-provenance.py --check; if ($LASTEXITCODE -ne 0) { exit 1 }
    {{python}} -X utf8 scripts/check-disk-gate.py --quiet; if ($LASTEXITCODE -ne 0) { exit 1 }
    {{python}} -X utf8 scripts/check-horizon-ledger.py --since 2026-09-29; if ($LASTEXITCODE -ne 0) { exit 1 }
    {{python}} -X utf8 scripts/check-override-drift.py; if ($LASTEXITCODE -ne 0) { exit 1 }
    Write-Output "check: all checkers passed"

# W8s allow-list coverage: every roster route must be selectable.
route-check:
    {{python}} -X utf8 scripts/check-route-allowlist.py

# W4/W12s record-hygiene lint: run before a review.
record-check:
    {{python}} -X utf8 scripts/check-record-hygiene.py

# W2s premise-qualification gate: run this BEFORE any Planner call.
qualify run *args:
    {{python}} -X utf8 scripts/qualify-premise.py --run {{run}} {{args}}

# W1s recurrence check: the same criterion blocking two consecutive reviews.
recurrence-check:
    {{python}} -X utf8 scripts/check-review-recurrence.py

# Data-quality report over every archived dump (not a tree check; see `check`).
dump-controls:
    {{python}} -X utf8 scripts/check-dump-controls.py --all

# W15's override-drift check: a document naming an override the toolkit no longer reads.
override-check:
    {{python}} -X utf8 scripts/check-override-drift.py

# W14's ledger lint: a strict run with no ledger line fails.
horizon-check:
    {{python}} -X utf8 scripts/check-horizon-ledger.py --since 2026-09-29

# Record one strict run under WinDbg TTD (T1). Sets RECOMP_GPU_ACK=0 for you,
# exactly as `just strict-run` does: the profile contract puts the strict setting
# on the caller, and the tool refuses rather than inserting it.
ttd-record label="ttd":
    $env:RECOMP_GPU_ACK = "0"; {{python}} -X utf8 tools/ttd/ttd-record.py --label {{label}}

# Query a TTD trace for writes to a guest VA across all 29 aliases (T1).
# Add --terminal-sequence / --value-at-p to supply W11's position P and enable the
# full admission verdict. The command exits nonzero when a W11 condition fails.
ttd-writes trace va:
    {{python}} -X utf8 tools/ttd/ttd-query.py "{{trace}}" {{va}}

# Enumerate every guest access to a VA from the original XBE (T9), with controls.
enumerate va:
    {{python}} -X utf8 scripts/enumerate-accesses.py --value {{va}}

# Record or lint a cited value (T10).
cite-check records:
    {{python}} -X utf8 scripts/cite.py check {{records}}

# Regenerate this session's startup receipt (T13).
receipt:
    {{python}} -X utf8 scripts/gen-startup-receipt.py

# Host/run environment report; --runtime-log writes doctor.json into a run (T12).
doctor run="":
    if ("{{run}}" -eq "") { {{python}} -X utf8 tools/doctor.py --preflight } else { {{python}} -X utf8 tools/doctor.py --runtime-log "{{run}}" }

# clang-cl and MSVC /analyze configurations of the toolkit (T5).
analyze target="baseline":
    {{python}} -X utf8 scripts/analyze-toolkit.py --target {{target}}

# Free-space gate and the retention plan; nothing is deleted (T14).
disk:
    {{python}} -X utf8 scripts/check-disk-gate.py --json
    {{python}} -X utf8 scripts/logs-reclaim-plan.py

# Retention plan listing every candidate directory.
disk-candidates:
    {{python}} -X utf8 scripts/logs-reclaim-plan.py --list-candidates

# Scan every reachable Git blob for secret-shaped content.
secret-audit:
    git rev-list --objects --all | ForEach-Object { ($_ -split ' ')[0] } | Out-File -Encoding ascii logs/blob-shas.txt; {{python}} -X utf8 scripts/secret-audit.py logs/blob-shas.txt

# Build the XDK symbol database from the original XBE (T2).
symbols:
    {{python}} -X utf8 scripts/gen-xdk-symbols.py --write

# DuckDB-backed log queries (T8).
logq run query:
    {{python}} -X utf8 scripts/logq.py "{{run}}" --query {{query}}
