@echo off
REM ========================================
REM PQC Wallet - Windows Dependencies Setup
REM ========================================
REM This script helps install dependencies for PQC Wallet on Windows
REM using vcpkg package manager
REM 
REM Location: build/scripts/setup_dependencies_windows.bat
REM Can be run from any directory
REM ========================================

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0
REM Get the project root directory (two levels up from script location)
set PROJECT_ROOT=%SCRIPT_DIR%..\..
cd /d "%PROJECT_ROOT%"

echo.
echo ========================================
echo PQC WALLET - DEPENDENCIES SETUP
echo ========================================
echo.

REM Check if vcpkg is available
if not defined VCPKG_ROOT (
    echo ERROR: VCPKG_ROOT environment variable not set.
    echo.
    echo Please install vcpkg first:
    echo 1. Clone vcpkg: git clone https://github.com/Microsoft/vcpkg.git
    echo 2. Run bootstrap: .\vcpkg\bootstrap-vcpkg.bat
    echo 3. Set VCPKG_ROOT environment variable to vcpkg directory
    echo 4. Add vcpkg to PATH
    echo.
    pause
    exit /b 1
)

echo ✓ vcpkg found at: %VCPKG_ROOT%

REM Check if vcpkg executable exists
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo ERROR: vcpkg.exe not found at %VCPKG_ROOT%
    echo Please run bootstrap-vcpkg.bat in the vcpkg directory
    pause
    exit /b 1
)

echo.
echo Installing dependencies for PQC Wallet...
echo.

REM Install GLFW3
echo Installing GLFW3...
"%VCPKG_ROOT%\vcpkg.exe" install glfw3:x64-windows
if errorlevel 1 (
    echo ERROR: Failed to install GLFW3
    pause
    exit /b 1
)
echo ✓ GLFW3 installed

REM Install OpenSSL
echo.
echo Installing OpenSSL...
"%VCPKG_ROOT%\vcpkg.exe" install openssl:x64-windows
if errorlevel 1 (
    echo ERROR: Failed to install OpenSSL
    pause
    exit /b 1
)
echo ✓ OpenSSL installed

REM Check if liboqs is available in vcpkg
echo.
echo Checking for liboqs in vcpkg...
"%VCPKG_ROOT%\vcpkg.exe" search liboqs
if errorlevel 1 (
    echo WARNING: liboqs not found in vcpkg
    echo You will need to build liboqs manually:
    echo.
    echo 1. Download liboqs: https://github.com/open-quantum-safe/liboqs
    echo 2. Follow their Windows build instructions
    echo 3. Make sure it's installed where CMake can find it
    echo.
) else (
    echo Installing liboqs...
    "%VCPKG_ROOT%\vcpkg.exe" install liboqs:x64-windows
    if errorlevel 1 (
        echo WARNING: Failed to install liboqs via vcpkg
        echo You may need to build it manually
    ) else (
        echo ✓ liboqs installed
    )
)

REM Integrate vcpkg with Visual Studio
echo.
echo Integrating vcpkg with Visual Studio...
"%VCPKG_ROOT%\vcpkg.exe" integrate install
if errorlevel 1 (
    echo WARNING: vcpkg integration failed
) else (
    echo ✓ vcpkg integrated with Visual Studio
)

echo.
echo ========================================
echo DEPENDENCY SETUP COMPLETED!
echo ========================================
echo.
echo Installed packages:
"%VCPKG_ROOT%\vcpkg.exe" list
echo.
echo Next steps:
echo 1. If liboqs installation failed, build it manually using:
echo    build\scripts\build_liboqs_windows.bat
echo 2. Run build\scripts\build_windows.bat to build PQC Wallet
echo 3. Use build\scripts\clean_windows.bat to clean build files if needed
echo.
echo For detailed instructions, see: build\docs\BUILD_WINDOWS.md
echo.

pause
