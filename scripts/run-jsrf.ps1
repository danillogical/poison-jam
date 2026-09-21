param(
    [ValidateRange(1, 300)][int]$Seconds = 15,
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Label = 'run',
    [ValidateSet('healthy','worker-crash','deadlock','spin','handled','dispatch-race','video','gpu-progress','gpu-stall','gpu-corrupt','gpu-unreadable','gpu-mmio-owner','gpu-mmio-lifecycle','gpu-ptimer-runtime','gpu-submit-supported','gpu-submit-bound','gpu-submit-blocked','')][string]$Probe = '',
    [string[]]$ExpectCheckpoint = @('memory_ready', 'guest_entry')
)
# Forwards to scripts/run-jsrf.py so Windows PowerShell 5.1 and cmd.exe work.
$ErrorActionPreference = 'Stop'
$python = Join-Path $PSScriptRoot 'run-jsrf.py'
$command = @('-X', 'utf8', $python, '--seconds', "$Seconds", '--label', $Label)
if ($Probe) { $command += @('--probe', $Probe) }
foreach ($checkpoint in $ExpectCheckpoint) {
    $command += @('--expect-checkpoint', $checkpoint)
}
& python @command
exit $LASTEXITCODE
