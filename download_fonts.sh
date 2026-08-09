#!/bin/bash

# Download Popular Fonts Script for PQC Wallet
# This script downloads free, open-source fonts for use with the application

FONTS_DIR="fonts"
mkdir -p "$FONTS_DIR"

echo "🔤 Downloading fonts for PQC Wallet..."

# Function to download font
download_font() {
    local url="$1"
    local filename="$2"
    local description="$3"
    
    if [ ! -f "$FONTS_DIR/$filename" ]; then
        echo "📥 Downloading $description..."
        if curl -L -o "$FONTS_DIR/$filename" "$url"; then
            echo "✅ Downloaded: $filename"
        else
            echo "❌ Failed to download: $filename"
        fi
    else
        echo "⏭️ Already exists: $filename"
    fi
}

# DejaVu Sans - Excellent Unicode support
download_font "https://github.com/dejavu-fonts/dejavu-fonts/releases/download/version_2_37/dejavu-fonts-ttf-2.37.zip" "dejavu-fonts.zip" "DejaVu Sans Font Family"

# Extract DejaVu if downloaded as zip
if [ -f "$FONTS_DIR/dejavu-fonts.zip" ]; then
    echo "📦 Extracting DejaVu fonts..."
    cd "$FONTS_DIR"
    unzip -o dejavu-fonts.zip "dejavu-fonts-ttf-2.37/ttf/DejaVuSans.ttf" -j
    unzip -o dejavu-fonts.zip "dejavu-fonts-ttf-2.37/ttf/DejaVuSans-Bold.ttf" -j
    rm dejavu-fonts.zip
    cd ..
fi

# Roboto - Google's font
download_font "https://github.com/google/fonts/raw/main/apache/roboto/Roboto-Regular.ttf" "Roboto-Regular.ttf" "Roboto Regular"
download_font "https://github.com/google/fonts/raw/main/apache/roboto/Roboto-Bold.ttf" "Roboto-Bold.ttf" "Roboto Bold"

# Open Sans - Popular web font
download_font "https://github.com/google/fonts/raw/main/apache/opensans/OpenSans-Regular.ttf" "OpenSans-Regular.ttf" "Open Sans Regular"
download_font "https://github.com/google/fonts/raw/main/apache/opensans/OpenSans-Bold.ttf" "OpenSans-Bold.ttf" "Open Sans Bold"

# Source Sans Pro - Adobe's font
download_font "https://github.com/adobe-fonts/source-sans-pro/releases/download/3.046R/source-sans-pro-3.046R.zip" "source-sans-pro.zip" "Source Sans Pro"

# Extract Source Sans Pro if downloaded as zip
if [ -f "$FONTS_DIR/source-sans-pro.zip" ]; then
    echo "📦 Extracting Source Sans Pro fonts..."
    cd "$FONTS_DIR"
    unzip -o source-sans-pro.zip "*/TTF/SourceSansPro-Regular.ttf" -j
    unzip -o source-sans-pro.zip "*/TTF/SourceSansPro-Bold.ttf" -j
    rm source-sans-pro.zip
    cd ..
fi

# Ubuntu Font - Canonical's font
download_font "https://github.com/canonical/ubuntu-font-family-sources/releases/download/0.83/ubuntu-font-family-0.83.zip" "ubuntu-font.zip" "Ubuntu Font Family"

# Extract Ubuntu if downloaded as zip
if [ -f "$FONTS_DIR/ubuntu-font.zip" ]; then
    echo "📦 Extracting Ubuntu fonts..."
    cd "$FONTS_DIR"
    unzip -o ubuntu-font.zip "ubuntu-font-family-0.83/Ubuntu-R.ttf" -j
    unzip -o ubuntu-font.zip "ubuntu-font-family-0.83/Ubuntu-B.ttf" -j
    rm ubuntu-font.zip
    cd ..
fi

echo ""
echo "✨ Font download complete!"
echo ""
echo "📁 Available fonts in $FONTS_DIR:"
ls -la "$FONTS_DIR"/*.ttf 2>/dev/null || echo "No TTF files found."

echo ""
echo "🚀 You can now run PQC Wallet and access Font Settings from the View menu!"
echo "💡 Restart the application to see all newly downloaded fonts."
