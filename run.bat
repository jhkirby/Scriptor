@echo off
REM Build Scriptor and, on success, launch it.
setlocal

REM Build first (reuses the existing build script).
call "%~dp0build.bat"
if errorlevel 1 (
    echo.
    echo Not launching: build failed.
    exit /b 1
)

echo.
echo Launching Scriptor...
start "" "%~dp0build\Scriptor.exe"

endlocal
