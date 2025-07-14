#!/bin/bash

# ========================================
# PQC Wallet - Desktop Shortcut Creator
# ========================================
# This script creates a desktop shortcut for PQC Wallet
# Works on Linux with GNOME, KDE, XFCE, and other desktop environments
# ========================================

echo "========================================="
echo "PQC WALLET - DESKTOP SHORTCUT CREATOR"
echo "========================================="
echo

# Get the absolute path of the project directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
EXECUTABLE_PATH="$PROJECT_ROOT/build/PQCWallet"

echo "Project directory: $PROJECT_ROOT"
echo "Executable path: $EXECUTABLE_PATH"
echo

# Check if the executable exists
if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo "❌ ERROR: PQCWallet executable not found at: $EXECUTABLE_PATH"
    echo
    echo "Please build the application first:"
    echo "  ./build.sh"
    echo
    exit 1
fi

echo "✅ PQCWallet executable found"

# Create the desktop shortcut content
DESKTOP_FILE_CONTENT="[Desktop Entry]
Version=1.0
Type=Application
Name=PQC Wallet
Comment=Post-Quantum Cryptography Wallet - Secure wallet with quantum-safe encryption
Exec=$EXECUTABLE_PATH
Icon=$PROJECT_ROOT/assets/pqcwallet-icon.png
Terminal=false
StartupNotify=true
Categories=Office;Finance;Security;
Keywords=wallet;crypto;quantum;security;encryption;password;
StartupWMClass=PQCWallet"

# Determine desktop directory
if [ -d "$HOME/Desktop" ]; then
    DESKTOP_DIR="$HOME/Desktop"
elif [ -d "$HOME/Scriitor" ]; then  # Romanian desktop
    DESKTOP_DIR="$HOME/Scriitor"
else
    DESKTOP_DIR="$HOME/Desktop"
    mkdir -p "$DESKTOP_DIR"
fi

DESKTOP_FILE="$DESKTOP_DIR/PQCWallet.desktop"

# Create the desktop shortcut
echo "$DESKTOP_FILE_CONTENT" > "$DESKTOP_FILE"

# Make it executable
chmod +x "$DESKTOP_FILE"

echo "✅ Desktop shortcut created at: $DESKTOP_FILE"

# Also create in applications menu
APPLICATIONS_DIR="$HOME/.local/share/applications"
mkdir -p "$APPLICATIONS_DIR"
APPLICATIONS_FILE="$APPLICATIONS_DIR/PQCWallet.desktop"

echo "$DESKTOP_FILE_CONTENT" > "$APPLICATIONS_FILE"
chmod +x "$APPLICATIONS_FILE"

echo "✅ Application menu entry created at: $APPLICATIONS_FILE"

# Update desktop database
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPLICATIONS_DIR" 2>/dev/null || true
    echo "✅ Desktop database updated"
fi

echo
echo "========================================="
echo "DESKTOP SHORTCUT CREATION COMPLETED!"
echo "========================================="
echo
echo "The PQC Wallet shortcut has been created:"
echo "• Desktop shortcut: $DESKTOP_FILE"
echo "• Applications menu: $APPLICATIONS_FILE"
echo
echo "You can now:"
echo "1. Double-click the desktop icon to launch PQC Wallet"
echo "2. Find PQC Wallet in your applications menu"
echo "3. Pin it to your taskbar/dock for quick access"
echo

# Check if we need to create an icon
if [ ! -f "$PROJECT_ROOT/assets/pqcwallet-icon.png" ]; then
    echo "📝 NOTE: No icon found at assets/pqcwallet-icon.png"
    echo "To add a custom icon:"
    echo "1. Create the assets/ directory: mkdir -p assets"
    echo "2. Add your icon: cp your-icon.png assets/pqcwallet-icon.png"
    echo "3. Run this script again to update the shortcut"
    echo
fi

echo "Done! 🚀"
