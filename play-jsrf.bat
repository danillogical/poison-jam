@echo off
rem Play JSRF in a visible window: the title-path switches, a private save folder, no collector.
rem Run from anywhere; it switches to the repository root, where game\default.xbe must be.
setlocal
cd /d "%~dp0"

if not exist "build\Release\jsrf_recomp.exe" (
    echo build\Release\jsrf_recomp.exe not found. Build first with: just build
    exit /b 1
)
if not exist "game\default.xbe" (
    echo game\default.xbe not found. The original game files go in the game folder.
    exit /b 1
)

rem The switches every title-path run uses (see just title-run), plus the window.
set RECOMP_APU_TRAP=1
set RECOMP_PB_EXEC=1
set RECOMP_FB_WINDOW=1

rem Saves go here, so the default disk images in %%LOCALAPPDATA%%\xboxrecomp are left alone.
set SAVE_ROOT=%~dp0logs\play-save
if not exist "%SAVE_ROOT%" mkdir "%SAVE_ROOT%"

echo Starting JSRF. The title screen takes about 25 minutes; close the window or press Ctrl+C to stop.
"build\Release\jsrf_recomp.exe" --save-root="%SAVE_ROOT%" %*
echo jsrf_recomp.exe exited with code %ERRORLEVEL%
endlocal
