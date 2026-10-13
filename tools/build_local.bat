@echo off
rem Builds AuraSynth.vst3 and AuraDrums.vst3 (Release) for x64 and ARM64.
rem Build output goes to %USERPROFILE%\AuraSynth-build, outside OneDrive, so ~1 GB of object files is not synced.
setlocal
cd /d "%~dp0.."
set "OUT=%USERPROFILE%\AuraSynth-build"
for %%A in (x64 ARM64) do (
  cmake -S . -B "%OUT%\%%A" -G "Visual Studio 17 2022" -A %%A -DAURA_BUILD_TESTS=OFF || goto :err
  cmake --build "%OUT%\%%A" --config Release --target AuraSynth_VST3 AuraDrums_VST3 --parallel || goto :err
)
echo.
echo Done: %OUT%\x64\AuraSynth_artefacts\Release\VST3   and   %OUT%\ARM64\AuraSynth_artefacts\Release\VST3
echo       (AuraDrums is in the matching AuraDrums_artefacts folders)
echo Copy the whole .vst3 folders to  C:\Program Files\Common Files\VST3\   (needs admin)
exit /b 0
:err
echo BUILD FAILED - see BUILD.md "Troubleshooting".
exit /b 1
