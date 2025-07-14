#!/bin/bash

# ========================================
# PQC Wallet - Simple Icon Creator (No ImageMagick)
# ========================================
# Creates a simple SVG icon that can be converted to PNG
# ========================================

echo "🎨 Creating simple SVG icon for PQC Wallet..."

# Create assets directory if it doesn't exist
mkdir -p assets

# Create SVG icon with quantum-crypto theme
cat > assets/pqcwallet-icon.svg << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<svg width="128" height="128" viewBox="0 0 128 128" xmlns="http://www.w3.org/2000/svg">
  <!-- Background rounded rectangle -->
  <rect x="4" y="4" width="120" height="120" rx="12" ry="12" fill="#1e40af" stroke="#3b82f6" stroke-width="2"/>
  
  <!-- Inner frame -->
  <rect x="12" y="12" width="104" height="104" rx="8" ry="8" fill="#3b82f6" stroke="#60a5fa" stroke-width="1"/>
  
  <!-- Quantum particle (center gold circle) -->
  <circle cx="64" cy="48" r="12" fill="#fbbf24" stroke="#f59e0b" stroke-width="2"/>
  
  <!-- Security cross (green) -->
  <rect x="48" y="60" width="32" height="8" fill="#10b981" stroke="#059669" stroke-width="1"/>
  <rect x="60" y="48" width="8" height="32" fill="#10b981" stroke="#059669" stroke-width="1"/>
  
  <!-- Quantum states (red and purple dots) -->
  <circle cx="84" cy="84" r="6" fill="#ef4444" stroke="#dc2626" stroke-width="1"/>
  <circle cx="44" cy="84" r="6" fill="#8b5cf6" stroke="#7c3aed" stroke-width="1"/>
  
  <!-- Additional quantum particles -->
  <circle cx="32" cy="32" r="3" fill="#fbbf24" opacity="0.7"/>
  <circle cx="96" cy="32" r="3" fill="#ef4444" opacity="0.7"/>
  <circle cx="96" cy="96" r="3" fill="#8b5cf6" opacity="0.7"/>
  
  <!-- Lock symbol (small) -->
  <rect x="20" y="100" width="12" height="8" rx="2" fill="#fbbf24" stroke="#f59e0b" stroke-width="1"/>
  <path d="M22 100 L22 96 Q22 92 26 92 Q30 92 30 96 L30 100" fill="none" stroke="#f59e0b" stroke-width="2"/>
  
  <!-- Text label -->
  <text x="64" y="118" font-family="Arial, sans-serif" font-size="12" font-weight="bold" text-anchor="middle" fill="#fbbf24">PQC</text>
</svg>
EOF

echo "✅ SVG icon created: assets/pqcwallet-icon.svg"

# Try to convert SVG to PNG if tools are available
if command -v convert &> /dev/null; then
    echo "🔄 Converting SVG to PNG using ImageMagick..."
    convert assets/pqcwallet-icon.svg assets/pqcwallet-icon.png
    echo "✅ PNG icon created: assets/pqcwallet-icon.png"
    
    # Create additional sizes
    convert assets/pqcwallet-icon.png -resize 16x16 assets/pqcwallet-icon-16.png
    convert assets/pqcwallet-icon.png -resize 32x32 assets/pqcwallet-icon-32.png
    convert assets/pqcwallet-icon.png -resize 64x64 assets/pqcwallet-icon-64.png
    echo "✅ Multiple sizes created"
    
elif command -v inkscape &> /dev/null; then
    echo "🔄 Converting SVG to PNG using Inkscape..."
    inkscape --export-png=assets/pqcwallet-icon.png --export-width=128 --export-height=128 assets/pqcwallet-icon.svg
    echo "✅ PNG icon created: assets/pqcwallet-icon.png"
    
elif command -v rsvg-convert &> /dev/null; then
    echo "🔄 Converting SVG to PNG using rsvg-convert..."
    rsvg-convert -w 128 -h 128 assets/pqcwallet-icon.svg > assets/pqcwallet-icon.png
    echo "✅ PNG icon created: assets/pqcwallet-icon.png"
    
else
    echo "⚠️  No SVG converter found. SVG icon created but PNG conversion skipped."
    echo "You can:"
    echo "1. Use the SVG directly (some systems support it)"
    echo "2. Install ImageMagick: sudo apt install imagemagick"
    echo "3. Install Inkscape: sudo apt install inkscape"
    echo "4. Convert online at: https://cloudconvert.com/svg-to-png"
fi

echo
echo "========================================="
echo "SIMPLE ICON CREATION COMPLETED!"
echo "========================================="
echo
echo "Generated files:"
echo "• assets/pqcwallet-icon.svg - Vector icon (scalable)"
if [ -f "assets/pqcwallet-icon.png" ]; then
    echo "• assets/pqcwallet-icon.png - Raster icon (128x128)"
fi
echo
echo "Icon features:"
echo "🔵 Blue secure background"
echo "🟡 Gold quantum particles"
echo "🟢 Green security cross"
echo "🔴 Red quantum state"
echo "🟣 Purple quantum state"
echo "🔒 Lock symbol"
echo "📝 PQC text label"
echo
echo "Next steps:"
echo "1. Update desktop shortcuts: ./create_desktop_shortcut.sh"
echo "2. The new icon will be automatically used"
echo
echo "Done! 🚀"
