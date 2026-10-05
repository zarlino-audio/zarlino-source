@echo off
rem ===========================================================================
rem  Build ZarlinoSource targets from anywhere.
rem
rem      tools\build.cmd ZarlinoSource_All ZarlinoSource_Tests
rem
rem  Configures build\ (Ninja, Release) on first use, then builds the requested
rem  targets.
rem
rem  The toolchain is DISCOVERED, not hardcoded: this script used to name one
rem  laptop's Visual Studio path and one user profile's cmake.exe, so on every
rem  other machine it printed "failed to initialise the MSVC environment" and
rem  still exited 0 - a silent failure that made a broken build look green.
rem  Discovery order: ZS_VCVARS / ZS_CMAKE overrides, then PATH, then the
rem  standard install locations (including VS 2022 Build Tools). If the
rem  environment cannot be initialised the script fails LOUDLY with exit 2.
rem ===========================================================================
setlocal enabledelayedexpansion

set "REPO=%~dp0.."
set "VCVARS="
set "CMAKE="

rem --- cmake: override, then PATH, then the usual install roots ---------------
if defined ZS_CMAKE set "CMAKE=%ZS_CMAKE%"
if not defined CMAKE (
    for /f "delims=" %%C in ('where cmake 2^>nul') do (
        if not defined CMAKE set "CMAKE=%%C"
    )
)
if not defined CMAKE (
    for /f "delims=" %%C in ('dir /b /s "%ProgramFiles%\CMake\bin\cmake.exe" 2^>nul') do (
        if not defined CMAKE set "CMAKE=%%C"
    )
)
if not defined CMAKE (
    for /f "delims=" %%C in ('dir /b /s "%LocalAppData%\Programs\CMake\bin\cmake.exe" 2^>nul') do (
        if not defined CMAKE set "CMAKE=%%C"
    )
)

rem --- MSVC environment: override, then vswhere, then the standard layout ----
if defined ZS_VCVARS set "VCVARS=%ZS_VCVARS%"

if not defined VCVARS (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%V in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
            if not defined VCVARS if exist "%%V\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%V\VC\Auxiliary\Build\vcvars64.bat"
        )
    )
)

if not defined VCVARS (
    for %%R in (
        "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
        "%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
        "%ProgramFiles%\Microsoft Visual Studio\2022\Community"
        "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
        "%ProgramFiles%\Microsoft Visual Studio\18\Community"
        "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
    ) do (
        if not defined VCVARS if exist "%%~R\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%~R\VC\Auxiliary\Build\vcvars64.bat"
    )
)

if not defined CMAKE (
    echo [build] FAILED: cmake.exe not found. Install CMake or set ZS_CMAKE.
    exit /b 2
)

if not defined VCVARS (
    echo [build] FAILED: no MSVC toolchain found (vcvars64.bat^).
    echo [build] Install Visual Studio Build Tools with the C++ workload, or set ZS_VCVARS.
    exit /b 2
)

echo [build] vcvars: %VCVARS%
echo [build] cmake : %CMAKE%

call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
    echo [build] FAILED: could not initialise the MSVC environment from "%VCVARS%"
    exit /b 2
)

where cl.exe >nul 2>&1
if errorlevel 1 (
    echo [build] FAILED: vcvars ran but cl.exe is not on PATH - the toolchain is incomplete.
    exit /b 2
)

pushd "%REPO%"

if not exist "build\build.ninja" (
    echo [build] configuring build\
    "%CMAKE%" -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    if errorlevel 1 (
        echo [build] FAILED: cmake configure failed
        popd
        exit /b 1
    )
)

"%CMAKE%" --build build --target %* --parallel
set "RC=%ERRORLEVEL%"

popd

if not "%RC%"=="0" (
    echo [build] FAILED: build exited with %RC%
    exit /b %RC%
)

exit /b 0
