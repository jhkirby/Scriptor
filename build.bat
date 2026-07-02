@echo off
REM Build Scriptor with MSVC (Visual Studio 2022). Produces build\Scriptor.exe.
setlocal

REM Locate the Visual Studio install (works across editions / machines).
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Is Visual Studio 2022 installed?
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo ERROR: could not find a Visual Studio install with the C++ tools.
    exit /b 1
)

REM Set up the MSVC compiler environment (x64).
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo ERROR: failed to initialize the MSVC environment.
    exit /b 1
)

if not exist build mkdir build

REM Compile. /SUBSYSTEM:WINDOWS makes it a GUI app (no console window).
cl /nologo /W4 /EHsc /std:c++17 /DUNICODE /D_UNICODE ^
   /Fobuild\ /Febuild\Scriptor.exe ^
   src\main.cpp ^
   /link /SUBSYSTEM:WINDOWS

if errorlevel 1 (
    echo.
    echo BUILD FAILED.
    exit /b 1
)

echo.
echo BUILD OK -^> build\Scriptor.exe
endlocal
