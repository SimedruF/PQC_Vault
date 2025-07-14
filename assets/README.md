# PQC Wallet Assets

This directory contains assets for the PQC Wallet application.

## Icons

### Desktop Icon
- **File**: `pqcwallet-icon.png`
- **Purpose**: Used for desktop shortcuts and application menu
- **Recommended size**: 128x128 pixels or higher
- **Format**: PNG with transparency support

### How to add a custom icon:

#### Option 1: Use the automatic icon generator (recommended)
```bash
# Generate a custom PQC-themed icon using ImageMagick
./generate_icon.sh
```

#### Option 2: Use the simple SVG icon creator
```bash
# Create a simple SVG icon (works without ImageMagick)
./create_simple_icon.sh
```

#### Option 3: Manual icon creation
1. Create or find a suitable icon (PNG format recommended)
2. Resize it to 128x128 pixels (or higher, keeping square aspect ratio)
3. Save it as `pqcwallet-icon.png` in this directory
4. Run the desktop shortcut script again:
   - Linux: `./create_desktop_shortcut.sh`
   - Windows: `create_desktop_shortcut_windows.bat`

## Other Assets

You can also add other assets here such as:
- Splash screen images
- Additional icons for different contexts
- UI graphics
- Documentation images

## Icon Design Guidelines

For best results, the PQC Wallet icon should:
- Be easily recognizable at small sizes (16x16, 32x32)
- Use clear, simple shapes
- Incorporate security/crypto themes (locks, shields, quantum symbols)
- Work well on both light and dark backgrounds
- Use the application's color scheme

## Example Icon Sources

Free icon resources:
- [IconFinder](https://www.iconfinder.com/) - Free and premium icons
- [Flaticon](https://www.flaticon.com/) - Vector icons
- [Material Design Icons](https://material.io/icons/) - Google's icon set
- [Feather Icons](https://feathericons.com/) - Simple, clean icons

For quantum/crypto themes, search for:
- Lock, shield, security icons
- Quantum, atom, science icons
- Wallet, finance, money icons
- Technology, digital, cyber icons
