@echo off
REM ========================================
REM PQC Wallet - Clean Build Files
REM ========================================
REM This script cleans build files and cache
REM ========================================

echo.
echo ========================================
echo CLEANING PQC WALLET BUILD FILES
echo ========================================
echo.

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0
REM Get the project root directory (two levels up from script location)
set PROJECT_ROOT=%SCRIPT_DIR%..\..
cd /d "%PROJECT_ROOT%"

echo Working from project root: %CD%
echo.

echo Removing build directory...
if exist "build" (
    rmdir /s /q "build"
    echo ✓ Build directory removed
) else (
    echo Build directory not found
)

echo.
echo Removing CMake cache files...
if exist "CMakeCache.txt" del "CMakeCache.txt"
if exist "CMakeFiles" rmdir /s /q "CMakeFiles"

echo.
echo Removing temporary files...
if exist "*.tmp" del "*.tmp"
if exist "*.log" del "*.log"

echo.
echo ========================================
echo CLEANUP COMPLETED!
echo ========================================
echo.
echo You can now run build_windows.bat for a fresh build.
echo.

pause
