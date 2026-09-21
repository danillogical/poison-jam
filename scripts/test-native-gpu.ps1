param(
    [ValidateSet('none','renderdoc','nsight','all')][string]$Capture = 'none',
    [switch]$Warp
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
& python (Join-Path $PSScriptRoot 'build-identity.py') verify
if ($LASTEXITCODE -ne 0) { throw 'Build identity failed; run scripts/build-jsrf.ps1.' }
$folder = Join-Path $projectRoot ('logs\native-gpu\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $folder -Force | Out-Null
foreach ($name in @('jsrf_gpu_smoke.exe','jsrf_gpu_smoke.pdb','build-source.json')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot "build\Release\$name") -Destination $folder
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'tests\native_gpu_smoke.c') -Destination $folder
$fixture = Join-Path $folder 'jsrf_gpu_smoke.exe'
$fixtureArgs = if ($Warp) { @('--warp') } else { @() }
$metadata = [ordered]@{ kind='native_d3d11_fixture'; game_rendering=$false; capture=$Capture; warp=[bool]$Warp
    exe_sha256=(Get-FileHash $fixture).Hash; tools=@{} }
$renderdoc = Join-Path $env:ProgramFiles 'RenderDoc\renderdoccmd.exe'
$nsight = Get-ChildItem (Join-Path $env:ProgramFiles 'NVIDIA Corporation') -Filter nsys.exe -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.Directory.Name -eq 'target-windows-x64' } | Sort-Object FullName -Descending | Select-Object -First 1
if (Test-Path $renderdoc) {
    $metadata.tools.renderdoc = @{ path=$renderdoc; version=(Get-Item $renderdoc).VersionInfo.FileVersion }
    $header=Join-Path (Split-Path $renderdoc) 'renderdoc_app.h'
    if (Test-Path $header) { Copy-Item $header $folder }
}
if ($nsight) { $metadata.tools.nsight = @{ path=$nsight.FullName; version=$nsight.VersionInfo.FileVersion } }
$metadata | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $folder 'metadata.json') -Encoding utf8
& $fixture @fixtureArgs > (Join-Path $folder 'baseline.json') 2> (Join-Path $folder 'baseline-errors.log')
if ($LASTEXITCODE -ne 0) { throw "Native fixture failed; artifacts: $folder" }
$result=Get-Content (Join-Path $folder 'baseline.json') -Raw | ConvertFrom-Json
if ($result.outcome -ne 'pass' -or $result.pixels_verified -ne 4096 -or $result.debug_errors -ne 0) {
    throw "Native readback/validation failed: $folder"
}
if ($Capture -in @('renderdoc','all')) {
    if (!(Test-Path $renderdoc)) { throw 'Install RenderDoc to use -Capture renderdoc.' }
    & $renderdoc capture -w -d $folder -c (Join-Path $folder 'renderdoc') --opt-api-validation --opt-capture-callstacks $fixture @fixtureArgs *> (Join-Path $folder 'renderdoc.log')
    if ($LASTEXITCODE -ne 0 -or !(Get-ChildItem $folder -Filter '*.rdc')) { throw "RenderDoc capture failed: $folder" }
    if ((Get-Content (Join-Path $folder 'renderdoc.log') -Raw) -notmatch '"renderdoc_capture":true') {
        throw "Fixture did not confirm programmatic capture; reconfigure with the RenderDoc header available: $folder"
    }
}
if ($Capture -in @('nsight','all')) {
    if (!$nsight) { throw 'Install Nsight Systems to use -Capture nsight.' }
    $nsightOutput=Join-Path $folder 'nsight'
    & $nsight.FullName profile --trace=dx11,dx11-annotations --sample=none --cpuctxsw=none --wait=primary "--output=$nsightOutput" $fixture @fixtureArgs *> (Join-Path $folder 'nsight.log')
    if ($LASTEXITCODE -ne 0 -or !(Get-ChildItem $folder -Filter '*.nsys-rep')) { throw "Nsight capture failed: $folder" }
}
Write-Output "Native GPU artifacts: $folder"
Write-Output ($result | ConvertTo-Json -Compress)
