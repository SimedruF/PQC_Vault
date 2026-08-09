@echo off
REM ========================================
REM PQC Wallet - Windows Setup Wrapper
REM ========================================
REM This script provides easy access to Windows setup and build scripts
REM ========================================

echo.
echo ========================================
echo PQC WALLET - WINDOWS SETUP
echo ========================================
echo.
echo This wrapper script provides access to:
echo 1. setup_dependencies_windows.bat - Install dependencies
echo 2. build_windows.bat - Build the project
echo 3. clean_windows.bat - Clean build files
echo 4. build_liboqs_windows.bat - Build liboqs from source
echo 5. create_desktop_shortcut_windows.bat - Create desktop shortcut
echo.
echo All scripts are located in: build\scripts\
echo Documentation is in: build\docs\BUILD_WINDOWS.md
echo.

set /p choice="Which script would you like to run? (1-5, or 'q' to quit): "

if "%choice%"=="1" (
    echo.
    echo Running dependency setup...
    call "build\scripts\setup_dependencies_windows.bat"
) else if "%choice%"=="2" (
    echo.
    echo Running build script...
    call "build\scripts\build_windows.bat"
) else if "%choice%"=="3" (
    echo.
    echo Running clean script...
    call "build\scripts\clean_windows.bat"
) else if "%choice%"=="4" (
    echo.
    echo Running liboqs build script...
    call "build\scripts\build_liboqs_windows.bat"
) else if "%choice%"=="5" (
    echo.
    echo Creating desktop shortcut...
    call "create_desktop_shortcut_windows.bat"
) else if "%choice%"=="q" (
    echo Exiting...
    exit /b 0
) else (
    echo Invalid choice. Please run the script again and select 1-5 or 'q'.
    pause
    exit /b 1
)

echo.
echo Script completed.
pause
