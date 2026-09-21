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
    & cmake --build build --config Release --target jsrf_recomp jsrf_collect jsrf_crt_test jsrf_lifter_test jsrf_nv2a_test jsrf_gpu_smoke --parallel 4 *> logs\build-current.log
    if ($LASTEXITCODE -ne 0) { Get-Content logs\build-current.log -Tail 20; throw 'Build failed. Game was not launched.' }
    & python scripts\build-identity.py after
    if ($LASTEXITCODE -ne 0) { throw 'Build identity validation failed.' }
    Write-Output 'Build succeeded; source/executable identity recorded. See logs/build-current.log.'
} finally { Pop-Location }
