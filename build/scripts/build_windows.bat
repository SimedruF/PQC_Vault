@echo off
REM ========================================
REM PQC Wallet - Windows Build Script
REM ========================================
REM This script builds the PQC Wallet application on Windows
REM 
REM Prerequisites:
REM - Visual Studio with C++ support installed
REM - CMake 3.16 or higher
REM - vcpkg package manager (recommended for dependencies)
REM - Git (for submodules)
REM
REM Dependencies needed:
REM - OpenGL (usually available on Windows)
REM - GLFW3
REM - OpenSSL
REM - liboqs (quantum-safe cryptography library)
REM ========================================

echo.
echo ========================================
echo PQC WALLET - WINDOWS BUILD SCRIPT
echo ========================================
echo.

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0
REM Get the project root directory (two levels up from script location)
set PROJECT_ROOT=%SCRIPT_DIR%..\..
cd /d "%PROJECT_ROOT%"

echo Working from project root: %CD%
echo.

REM Check if CMake is available
cmake --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found. Please install CMake and add it to PATH.
    echo Download from: https://cmake.org/download/
    pause
    exit /b 1
)

echo ✓ CMake found

REM Check if Git is available (for submodules)
git --version >nul 2>&1
if errorlevel 1 (
    echo WARNING: Git not found. Some dependencies might not be available.
) else (
    echo ✓ Git found
)

REM Initialize and update submodules if .gitmodules exists
if exist ".gitmodules" (
    echo.
    echo Updating Git submodules...
    git submodule update --init --recursive
    if errorlevel 1 (
        echo WARNING: Failed to update submodules
    ) else (
        echo ✓ Submodules updated
    )
)

REM Create build directory
echo.
echo Creating build directory...
if not exist "build" mkdir build
cd build

REM Check for vcpkg integration
set VCPKG_CMAKE=""
if defined VCPKG_ROOT (
    set VCPKG_CMAKE=-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
    echo ✓ vcpkg integration detected: %VCPKG_ROOT%
) else (
    echo WARNING: VCPKG_ROOT not set. You may need to install dependencies manually.
    echo.
    echo To use vcpkg:
    echo 1. Install vcpkg: https://github.com/Microsoft/vcpkg
    echo 2. Set VCPKG_ROOT environment variable
    echo 3. Install dependencies: vcpkg install glfw3 openssl liboqs
    echo.
)

REM Configure with CMake
echo.
echo Configuring project with CMake...
echo.

if "%VCPKG_CMAKE%"=="" (
    cmake .. -G "Visual Studio 17 2022" -A x64
) else (
    cmake .. -G "Visual Studio 17 2022" -A x64 %VCPKG_CMAKE%
)

if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed!
    echo.
    echo Common solutions:
    echo 1. Install missing dependencies using vcpkg:
    echo    vcpkg install glfw3:x64-windows openssl:x64-windows
    echo.
    echo 2. Install liboqs manually:
    echo    - Download from: https://github.com/open-quantum-safe/liboqs
    echo    - Build and install following their Windows instructions
    echo.
    echo 3. Check that Visual Studio 2022 is installed with C++ support
    echo.
    pause
    exit /b 1
)

echo ✓ CMake configuration successful

REM Build the project
echo.
echo Building PQC Wallet...
echo.

cmake --build . --config Release --parallel

if errorlevel 1 (
    echo.
    echo ERROR: Build failed!
    echo.
    echo Check the output above for specific error messages.
    echo Common issues:
    echo - Missing dependencies (install via vcpkg)
    echo - Compiler errors (check source code)
    echo - Linker errors (check library paths)
    echo.
    pause
    exit /b 1
)

echo.
echo ✓ Build successful!

REM Check if executable was created
if exist "Release\PQCWallet.exe" (
    echo ✓ Executable created: Release\PQCWallet.exe
) else if exist "Debug\PQCWallet.exe" (
    echo ✓ Executable created: Debug\PQCWallet.exe
) else (
    echo WARNING: Executable not found in expected location
)

echo.
echo ========================================
echo BUILD COMPLETED SUCCESSFULLY!
echo ========================================
echo.
echo Next steps:
echo 1. The executable is in the build\Release or build\Debug directory
echo 2. Copy any required DLLs to the same directory as the executable
echo 3. Test the application
echo.
echo To run the application:
cd Release 2>nul || cd Debug
if exist "PQCWallet.exe" (
    echo   %cd%\PQCWallet.exe
) else (
    echo   Navigate to build directory and run PQCWallet.exe
)
echo.

pause
