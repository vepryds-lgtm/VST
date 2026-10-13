@echo off
rem Copies the built AuraSynth.vst3 and AuraDrums.vst3 (x64 + ARM64) from %USERPROFILE%\AuraSynth-build
rem to the system VST3 folder. Right-click and choose "Run as administrator". Close FL Studio first.
setlocal
set "OUT=%USERPROFILE%\AuraSynth-build"
set "DEST=%CommonProgramFiles%\VST3"
net session >nul 2>&1 || (echo This script must be run as administrator. & pause & exit /b 1)
for %%P in (AuraSynth AuraDrums) do (
  for %%A in (x64 ARM64) do (
    robocopy "%OUT%\%%A\%%P_artefacts\Release\VST3\%%P.vst3" "%DEST%\%%P.vst3" /E /R:1 /W:1 /NP /NFL /NDL /NJH /NJS
    if errorlevel 8 goto :err
  )
)
echo.
echo Done. In FL Studio: Options - Manage plugins - Find installed plugins.
pause
exit /b 0
:err
echo COPY FAILED - is FL Studio still open?
pause
exit /b 1
