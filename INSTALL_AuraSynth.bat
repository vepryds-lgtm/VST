@echo off
setlocal EnableExtensions EnableDelayedExpansion
title AuraSynth + AuraDrums - installer til FL Studio
cd /d "%~dp0"

rem ---- 1) Kraev administrator (self-elevate) ----
net session >nul 2>&1
if %errorlevel% neq 0 (
  echo Beder om administrator-rettigheder...
  powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)

echo ============================================================
echo  AuraSynth + AuraDrums - installerer alt og bygger pluginsene til FL Studio
echo  Foerste gang kan det tage 15-40 min (henter Visual Studio Build Tools).
echo ============================================================
echo.

rem Er computeren en ARM-PC (Windows on ARM / Snapdragon)?
set "ISARM="
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "ISARM=1"
if /i "%PROCESSOR_ARCHITEW6432%"=="ARM64" set "ISARM=1"
if defined ISARM echo Opdaget: ARM64-PC. Bygger baade x64- og ARM64-version i samme plugin.

where winget >nul 2>&1
if %errorlevel% neq 0 (
  echo FEJL: "winget" findes ikke. Opdater Windows 10/11 ^(App Installer i Microsoft Store^) og proev igen.
  goto :fail
)

set "GIT=C:\Program Files\Git\cmd\git.exe"
set "CMAKE=C:\Program Files\CMake\bin\cmake.exe"
set "VSINST=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
set "VSWHERE=%VSINST%\vswhere.exe"

rem ---- 2) Installer manglende vaerktoejer ----
if not exist "%GIT%" (
  echo [1/6] Installerer Git...
  winget install -e --id Git.Git --silent --accept-package-agreements --accept-source-agreements
)
if not exist "%GIT%" ( echo FEJL: Git blev ikke installeret. & goto :fail )

if not exist "%CMAKE%" (
  echo [2/6] Installerer CMake...
  winget install -e --id Kitware.CMake --silent --accept-package-agreements --accept-source-agreements
)
if not exist "%CMAKE%" ( echo FEJL: CMake blev ikke installeret. & goto :fail )

set "HASVS="
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "HASVS=%%i"
if not defined HASVS (
  echo [3/6] Installerer Visual Studio 2022 Build Tools med C++ ^(stor download, vent venligst^)...
  winget install -e --id Microsoft.VisualStudio.2022.BuildTools --accept-package-agreements --accept-source-agreements --override "--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --add Microsoft.VisualStudio.Component.VC.Tools.ARM64"
  if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "HASVS=%%i"
)
if not defined HASVS ( echo FEJL: C++ build tools blev ikke installeret. Genstart PC'en og koer filen igen. & goto :fail )

rem ARM-PC: JUCE bygger sit hjaelpevaerktoej som ARM64, saa ARM64-komponenten SKAL vaere installeret
if defined ISARM (
  set "HASARM="
  for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64 -property installationPath`) do set "HASARM=%%i"
  if not defined HASARM (
    echo [3b/6] Tilfoejer ARM64 C++ vaerktoejer til Visual Studio Build Tools...
    start "" /wait "%VSINST%\setup.exe" modify --installPath "!HASVS!" --add Microsoft.VisualStudio.Component.VC.Tools.ARM64 --passive --norestart
    rem Installeren kan vaere videre i baggrunden - vent til den er faerdig, op til ca. 20 min
    for /l %%n in (1,1,80) do (
      if not defined HASARM (
        for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64 -property installationPath`) do set "HASARM=%%i"
        if not defined HASARM (
          tasklist /fi "imagename eq setup.exe" | find /i "setup.exe" >nul
          if errorlevel 1 ( if %%n gtr 2 set "HASARM=none" )
          if not defined HASARM timeout /t 15 /nobreak >nul
        )
      )
    )
    if "!HASARM!"=="none" (
      set "HASARM="
      for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64 -property installationPath`) do set "HASARM=%%i"
    )
  )
  if not defined HASARM ( echo FEJL: ARM64-vaerktoejer blev ikke installeret. Genstart PC'en og koer filen igen. & goto :fail )
)

rem ---- 3) Hent JUCE ----
if not exist "third_party\JUCE\CMakeLists.txt" (
  echo [4/6] Henter JUCE 8.0.8...
  if exist third_party\JUCE rmdir /s /q third_party\JUCE
  "%GIT%" clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git third_party\JUCE
  if %errorlevel% neq 0 ( echo FEJL: kunne ikke hente JUCE ^(tjek internet^). & goto :fail )
)

rem ---- 4) Byg ----
if exist build rmdir /s /q build
if exist build_x64 rmdir /s /q build_x64
if exist build_arm64 rmdir /s /q build_arm64
echo. > build_log.txt

echo [5/6] Bygger x64-version ^(Release^)... det tager et par minutter
"%CMAKE%" -S . -B build_x64 -G "Visual Studio 17 2022" -A x64 -DAURA_BUILD_TESTS=OFF >> build_log.txt 2>&1
if %errorlevel% neq 0 ( echo FEJL i CMake-konfiguration ^(x64^). & goto :showlog )
"%CMAKE%" --build build_x64 --config Release --target AuraSynth_VST3 AuraDrums_VST3 --parallel >> build_log.txt 2>&1
if %errorlevel% neq 0 ( echo FEJL under bygning ^(x64^). & goto :showlog )

set "SRC=build_x64\AuraSynth_artefacts\Release\VST3\AuraSynth.vst3"
set "SRCD=build_x64\AuraDrums_artefacts\Release\VST3\AuraDrums.vst3"
if not exist "%SRC%" ( echo FEJL: AuraSynth-filen blev ikke fundet efter bygning. & goto :showlog )
if not exist "%SRCD%" ( echo FEJL: AuraDrums-filen blev ikke fundet efter bygning. & goto :showlog )

set "ARMOK="
if defined ISARM (
  echo [5b/6] Bygger ARM64-version ^(til native ARM FL Studio^)...
  "%CMAKE%" -S . -B build_arm64 -G "Visual Studio 17 2022" -A ARM64 -DAURA_BUILD_TESTS=OFF >> build_log.txt 2>&1
  if !errorlevel! equ 0 "%CMAKE%" --build build_arm64 --config Release --target AuraSynth_VST3 AuraDrums_VST3 --parallel >> build_log.txt 2>&1
  if !errorlevel! equ 0 if exist "build_arm64\AuraSynth_artefacts\Release\VST3\AuraSynth.vst3\Contents\arm64-win" if exist "build_arm64\AuraDrums_artefacts\Release\VST3\AuraDrums.vst3\Contents\arm64-win" set "ARMOK=1"
  if not defined ARMOK echo Advarsel: ARM64-versionen kunne ikke bygges - x64-versionen installeres alligevel.
)

rem ---- 5) Installer til FL Studio ----
echo [6/6] Installerer til FL Studio...
set "DST=C:\Program Files\Common Files\VST3\AuraSynth.vst3"
set "DSTD=C:\Program Files\Common Files\VST3\AuraDrums.vst3"
if exist "%DST%" rmdir /s /q "%DST%"
if exist "%DSTD%" rmdir /s /q "%DSTD%"
xcopy "%SRC%" "%DST%\" /E /I /Y /Q >nul
if %errorlevel% neq 0 ( echo FEJL: kunne ikke kopiere til %DST% ^(er FL Studio aaben? Luk den.^). & goto :fail )
xcopy "%SRCD%" "%DSTD%\" /E /I /Y /Q >nul
if %errorlevel% neq 0 ( echo FEJL: kunne ikke kopiere til %DSTD% ^(er FL Studio aaben? Luk den.^). & goto :fail )
if defined ARMOK xcopy "build_arm64\AuraSynth_artefacts\Release\VST3\AuraSynth.vst3\Contents\arm64-win" "%DST%\Contents\arm64-win\" /E /I /Y /Q >nul
if defined ARMOK xcopy "build_arm64\AuraDrums_artefacts\Release\VST3\AuraDrums.vst3\Contents\arm64-win" "%DSTD%\Contents\arm64-win\" /E /I /Y /Q >nul

echo.
echo ============================================================
echo  FAERDIG! AuraSynth og AuraDrums er installeret i:
echo  %DST%
echo  %DSTD%
echo.
echo  I FL Studio: Options ^> Manage plugins ^> "Find installed plugins"
echo  Derefter: Add ^> AuraSynth og Add ^> AuraDrums ^(under VST3 / Generators^)
echo ============================================================
pause
exit /b 0

:showlog
echo.
echo ---- Sidste linjer fra build_log.txt ----
powershell -NoProfile -Command "Get-Content -Tail 40 build_log.txt"
:fail
echo.
echo Installationen stoppede. Send filen build_log.txt ^(i denne mappe^) til Claude, saa retter vi den.
pause
exit /b 1
