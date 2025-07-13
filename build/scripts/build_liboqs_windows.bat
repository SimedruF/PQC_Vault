@echo off
REM ========================================
REM PQC Wallet - Build liboqs on Windows
REM ========================================
REM This script builds liboqs from source on Windows
REM ========================================

echo.
echo ========================================
echo BUILDING LIBOQS FROM SOURCE
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
    pause
    exit /b 1
)

REM Check if Git is available
git --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Git not found. Please install Git.
    pause
    exit /b 1
)

REM Create third_party directory if it doesn't exist
if not exist "third_party" mkdir third_party
cd third_party

REM Clone liboqs if it doesn't exist
if not exist "liboqs" (
    echo Cloning liboqs repository...
    git clone --depth 1 https://github.com/open-quantum-safe/liboqs.git
    if errorlevel 1 (
        echo ERROR: Failed to clone liboqs repository
        pause
        exit /b 1
    )
    echo ✓ liboqs cloned
) else (
    echo ✓ liboqs directory already exists
    cd liboqs
    echo Updating liboqs...
    git pull
    cd ..
)

cd liboqs

REM Create build directory
if not exist "build" mkdir build
cd build

REM Configure liboqs with CMake
echo.
echo Configuring liboqs...
echo.

cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_INSTALL_PREFIX="%cd%\..\install" ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DOQS_BUILD_ONLY_LIB=ON ^
    -DOQS_MINIMAL_BUILD=OFF ^
    -DOQS_ENABLE_KEM_KYBER=ON ^
    -DOQS_ENABLE_SIG_DILITHIUM=ON

if errorlevel 1 (
    echo ERROR: CMake configuration failed for liboqs
    pause
    exit /b 1
)

echo ✓ liboqs configured

REM Build liboqs
echo.
echo Building liboqs...
echo.

cmake --build . --config Release --parallel

if errorlevel 1 (
    echo ERROR: liboqs build failed
    pause
    exit /b 1
)

echo ✓ liboqs built successfully

REM Install liboqs
echo.
echo Installing liboqs...
echo.

cmake --build . --config Release --target install

if errorlevel 1 (
    echo ERROR: liboqs installation failed
    pause
    exit /b 1
)

echo ✓ liboqs installed

REM Set environment variables for the main build
set LIBOQS_ROOT=%cd%\..\install
echo.
echo ========================================
echo LIBOQS BUILD COMPLETED!
echo ========================================
echo.
echo liboqs has been built and installed to:
echo %LIBOQS_ROOT%
echo.
echo To use with PQC Wallet build:
echo 1. Set environment variable: LIBOQS_ROOT=%LIBOQS_ROOT%
echo 2. Or modify CMakeLists.txt to point to this installation
echo.

REM Go back to project root
cd ..\..\..

echo Next steps:
echo 1. Run build_windows.bat to build PQC Wallet
echo 2. If build fails, you may need to update CMakeLists.txt with liboqs paths
echo.

pause
