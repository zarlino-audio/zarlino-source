@echo off
rem ===========================================================================
rem  Build ZarlinoSource targets from anywhere.
rem
rem      tools\build.cmd ZarlinoSource ZarlinoSource_Tests
rem
rem  Configures build\ (Ninja, Release) on first use, then builds the requested
rem  targets. Absolute tool paths keep the build independent of the shell's
rem  current directory; the MSVC environment is initialised for every call.
rem ===========================================================================
setlocal

set "REPO=%~dp0.."
set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=C:\Users\khrit\AppData\Local\Python\pythoncore-3.14-64\Scripts\cmake.exe"

call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
    echo [build] failed to initialise the MSVC environment
    exit /b 1
)

pushd "%REPO%"

if not exist "build\build.ninja" (
    echo [build] configuring build\
    "%CMAKE%" -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    if errorlevel 1 (
        popd
        exit /b 1
    )
)

"%CMAKE%" --build build --target %* --parallel
set "RC=%ERRORLEVEL%"

popd
exit /b %RC%
