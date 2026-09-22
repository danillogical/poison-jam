$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Path logs -Force | Out-Null
    & cmake -S . -B build *> logs\configure-current.log
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed; see logs/configure-current.log.' }
    & python -X utf8 scripts\recover-functions.py
    if ($LASTEXITCODE -ne 0) { throw 'Recovery generation failed; refusing to compile older generated output.' }
    & python -X utf8 scripts\generate-lifter-tests.py
    if ($LASTEXITCODE -ne 0) { throw 'Lifter regression generation failed.' }
    & python scripts\build-identity.py before
    if ($LASTEXITCODE -ne 0) { throw 'Source fingerprint failed.' }
    # Every target CTest runs, not a subset.
    #
    # This list used to stop at six targets, which left the other test
    # executables to whatever build happened to have produced them. A target
    # whose link fails is deleted by MSBuild, and a later successful build that
    # does not name it never brings it back -- so CTest reports that test as
    # "Not Run" with nothing in the build log to explain it, and the next
    # session finds a failing suite it cannot reproduce. Building the full set
    # costs a few seconds and removes that whole failure mode.
    & cmake --build build --config Release --target jsrf_recomp jsrf_collect jsrf_crt_test jsrf_lifter_test jsrf_nv2a_test jsrf_recovery_11c1_test jsrf_service_chain_test jsrf_callback_reentry_test jsrf_nv2a_hal_test jsrf_inplace_event_test jsrf_gpu_smoke --parallel 4 *> logs\build-current.log
    if ($LASTEXITCODE -ne 0) { Get-Content logs\build-current.log -Tail 20; throw 'Build failed. Game was not launched.' }
    & python scripts\build-identity.py after
    if ($LASTEXITCODE -ne 0) { throw 'Build identity validation failed.' }
    Write-Output 'Build succeeded; source/executable identity recorded. See logs/build-current.log.'
} finally { Pop-Location }
