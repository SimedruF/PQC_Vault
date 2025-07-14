@echo off
REM ========================================
REM PQC Wallet - Windows Desktop Shortcut Creator
REM ========================================
REM This script creates a desktop shortcut for PQC Wallet on Windows
REM ========================================

echo.
echo =========================================
echo PQC WALLET - DESKTOP SHORTCUT CREATOR
echo =========================================
echo.

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0
REM Remove trailing backslash
if "%SCRIPT_DIR:~-1%"=="\" set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

set EXECUTABLE_PATH=%SCRIPT_DIR%\build\PQCWallet.exe

echo Project directory: %SCRIPT_DIR%
echo Executable path: %EXECUTABLE_PATH%
echo.

REM Check if the executable exists
if not exist "%EXECUTABLE_PATH%" (
    echo ERROR: PQCWallet.exe not found at: %EXECUTABLE_PATH%
    echo.
    echo Please build the application first:
    echo   build\scripts\build_windows.bat
    echo.
    pause
    exit /b 1
)

echo ✓ PQCWallet.exe found
echo.

REM Create VBScript to create the shortcut
set VBS_FILE=%TEMP%\create_pqc_shortcut.vbs

echo Set oWS = WScript.CreateObject("WScript.Shell") > "%VBS_FILE%"
echo sLinkFile = "%USERPROFILE%\Desktop\PQC Wallet.lnk" >> "%VBS_FILE%"
echo Set oLink = oWS.CreateShortcut(sLinkFile) >> "%VBS_FILE%"
echo oLink.TargetPath = "%EXECUTABLE_PATH%" >> "%VBS_FILE%"
echo oLink.WorkingDirectory = "%SCRIPT_DIR%" >> "%VBS_FILE%"
echo oLink.Description = "Post-Quantum Cryptography Wallet - Secure wallet with quantum-safe encryption" >> "%VBS_FILE%"
echo oLink.IconLocation = "%SCRIPT_DIR%\assets\pqcwallet-icon.ico,0" >> "%VBS_FILE%"
echo oLink.Save >> "%VBS_FILE%"

REM Execute the VBScript
cscript //nologo "%VBS_FILE%"

REM Clean up
del "%VBS_FILE%"

if exist "%USERPROFILE%\Desktop\PQC Wallet.lnk" (
    echo ✓ Desktop shortcut created successfully!
    echo.
    echo Location: %USERPROFILE%\Desktop\PQC Wallet.lnk
    echo.
    echo You can now:
    echo 1. Double-click the desktop shortcut to launch PQC Wallet
    echo 2. Pin it to your taskbar for quick access
    echo 3. Move it to your Start Menu if desired
) else (
    echo ERROR: Failed to create desktop shortcut
)

echo.
echo =========================================
echo DESKTOP SHORTCUT CREATION COMPLETED!
echo =========================================
echo.

pause
