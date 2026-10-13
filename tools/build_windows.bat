@echo off
rem Builds AuraSynth.vst3 and AuraDrums.vst3 (Release, x64). Run from "x64 Native Tools Command Prompt for VS 2022" or any prompt with cmake+git+VS 2022.
setlocal
cd /d "%~dp0.."
if not exist third_party\JUCE (
  git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git third_party\JUCE || goto :err
)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DAURA_BUILD_TESTS=OFF || goto :err
cmake --build build --config Release --target AuraSynth_VST3 AuraDrums_VST3 --parallel || goto :err
echo.
echo Done: build\AuraSynth_artefacts\Release\VST3\AuraSynth.vst3
echo   and: build\AuraDrums_artefacts\Release\VST3\AuraDrums.vst3
echo Copy those whole folders to  C:\Program Files\Common Files\VST3\   (needs admin)
exit /b 0
:err
echo BUILD FAILED - see BUILD.md "Troubleshooting".
exit /b 1
