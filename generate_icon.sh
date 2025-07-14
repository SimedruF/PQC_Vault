#!/bin/bash

# ========================================
# PQC Wallet - Icon Generator Script
# ========================================
# This script generates a custom icon for PQC Wallet using ImageMagick
# The icon combines quantum, security, and wallet themes
# ========================================

echo "========================================="
echo "PQC WALLET - ICON GENERATOR"
echo "========================================="
echo

# Check if ImageMagick is installed
if ! command -v convert &> /dev/null; then
    echo "❌ ImageMagick not found. Installing..."
    echo
    
    # Try to install ImageMagick based on the system
    if command -v apt-get &> /dev/null; then
        echo "Installing ImageMagick using apt..."
        sudo apt-get update && sudo apt-get install -y imagemagick
    elif command -v dnf &> /dev/null; then
        echo "Installing ImageMagick using dnf..."
        sudo dnf install -y ImageMagick
    elif command -v pacman &> /dev/null; then
        echo "Installing ImageMagick using pacman..."
        sudo pacman -S imagemagick
    else
        echo "Please install ImageMagick manually:"
        echo "  Ubuntu/Debian: sudo apt-get install imagemagick"
        echo "  Fedora: sudo dnf install ImageMagick"
        echo "  Arch: sudo pacman -S imagemagick"
        exit 1
    fi
fi

echo "✅ ImageMagick found"

# Create the assets directory if it doesn't exist
mkdir -p assets

# Icon dimensions
SIZE=128
HALF_SIZE=64

echo "🎨 Generating PQC Wallet icon..."

# Create a simple but effective icon design
# Base blue rounded rectangle with security elements
convert -size ${SIZE}x${SIZE} xc:transparent \
    -fill "#1e40af" -draw "roundrectangle 4,4 $((SIZE-4)),$((SIZE-4)) 12,12" \
    -fill "#3b82f6" -draw "roundrectangle 8,8 $((SIZE-8)),$((SIZE-8)) 8,8" \
    -fill "#fbbf24" -draw "circle $((HALF_SIZE)),$((HALF_SIZE-16)) $((HALF_SIZE)),$((HALF_SIZE-28))" \
    -fill "#10b981" -stroke "#059669" -strokewidth 2 \
    -draw "rectangle $((HALF_SIZE-16)),$((HALF_SIZE-4)) $((HALF_SIZE+16)),$((HALF_SIZE+4))" \
    -draw "rectangle $((HALF_SIZE-4)),$((HALF_SIZE-16)) $((HALF_SIZE+4)),$((HALF_SIZE+16))" \
    -fill "#ef4444" -draw "circle $((HALF_SIZE+20)),$((HALF_SIZE+20)) $((HALF_SIZE+20)),$((HALF_SIZE+14))" \
    -fill "#8b5cf6" -draw "circle $((HALF_SIZE-20)),$((HALF_SIZE+20)) $((HALF_SIZE-20)),$((HALF_SIZE+14))" \
    assets/pqcwallet-icon.png

echo "✅ Icon generated: assets/pqcwallet-icon.png"

# Create additional icon sizes for different contexts
echo "🔧 Creating additional icon sizes..."

# Create 16x16 for small displays
convert assets/pqcwallet-icon.png -resize 16x16 assets/pqcwallet-icon-16.png

# Create 32x32 for standard displays
convert assets/pqcwallet-icon.png -resize 32x32 assets/pqcwallet-icon-32.png

# Create 64x64 for medium displays
convert assets/pqcwallet-icon.png -resize 64x64 assets/pqcwallet-icon-64.png

# Create 256x256 for high-resolution displays
convert assets/pqcwallet-icon.png -resize 256x256 assets/pqcwallet-icon-256.png

echo "✅ Created multiple icon sizes"

# Create an ICO file for Windows
if command -v convert &> /dev/null; then
    echo "🪟 Creating Windows ICO file..."
    convert assets/pqcwallet-icon-16.png assets/pqcwallet-icon-32.png assets/pqcwallet-icon-64.png assets/pqcwallet-icon.png assets/pqcwallet-icon.ico
    echo "✅ Windows ICO file created: assets/pqcwallet-icon.ico"
fi

echo
echo "========================================="
echo "ICON GENERATION COMPLETED!"
echo "========================================="
echo
echo "Generated files:"
echo "• assets/pqcwallet-icon.png (128x128) - Main icon"
echo "• assets/pqcwallet-icon-16.png (16x16) - Small size"
echo "• assets/pqcwallet-icon-32.png (32x32) - Standard size"
echo "• assets/pqcwallet-icon-64.png (64x64) - Medium size"
echo "• assets/pqcwallet-icon-256.png (256x256) - High resolution"
echo "• assets/pqcwallet-icon.ico - Windows ICO format"
echo
echo "Icon design features:"
echo "🔵 Blue background (security/trust theme)"
echo "🟡 Gold circle (quantum/energy theme)"
echo "🟢 Green cross (health/safety theme)"
echo "🔴 Red dots (quantum states/encryption)"
echo "🟣 Purple dots (quantum states/encryption)"
echo
echo "Next steps:"
echo "1. Update desktop shortcuts: ./create_desktop_shortcut.sh"
echo "2. The new icon will be automatically used"
echo "3. Customize further if needed by editing this script"
echo
echo "Done! 🚀"
