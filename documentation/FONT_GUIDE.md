# Font Management Guide - PQC Wallet

## 🔤 Font System Overview

PQC Wallet now includes a comprehensive font management system that allows you to customize the application's appearance with different fonts and sizes.

## 🚀 Quick Start

1. **Access Font Settings**: Open the application and go to `View → Font Settings`
2. **Choose a Font**: Select from available fonts in the dropdown
3. **Adjust Size**: Use the slider to change font size (8-32px)
4. **Preview**: See how text will look in the preview area
5. **Apply**: Changes are applied immediately

## 📁 Available Fonts

### System Fonts (Automatically Detected)
- **DejaVu Sans** - Excellent Unicode support, very readable
- **Liberation Sans** - Open source alternative to Arial
- **Ubuntu** - Canonical's modern font
- **Arial/Helvetica** (macOS/Windows)

### Local Fonts (fonts/ directory)
- Place `.ttf` font files in the `fonts/` directory
- Restart the application to detect new fonts

## 🎨 Font Features

### Font Selection
- **Dropdown Menu**: Choose from all available fonts
- **Real-time Preview**: See changes immediately
- **System Detection**: Automatically finds system fonts

### Font Size Control
- **Slider Control**: Adjust from 8px to 32px
- **Live Updates**: Changes apply in real-time
- **Pixel Perfect**: Precise size control

### Font Preview
- **Sample Text**: Multiple text samples showing how fonts look
- **Unicode Support**: Test special characters
- **Color Examples**: See how different text colors appear

## 🔧 Advanced Usage

### Adding Custom Fonts

1. **Download Font Files**:
   ```bash
   # Use the provided script
   ./download_fonts.sh
   
   # Or manually download .ttf files
   ```

2. **Place in Fonts Directory**:
   ```bash
   cp your-font.ttf fonts/
   ```

3. **Restart Application**:
   - Fonts are loaded at startup
   - New fonts will appear in the dropdown

### Supported Font Formats
- ✅ **TrueType Fonts (.ttf)** - Fully supported
- ❌ **OpenType Fonts (.otf)** - Not currently supported
- ❌ **WOFF/WOFF2** - Not supported

### Font Quality Guidelines
- **Recommended Size**: 14-18px for best readability
- **UI Fonts**: Sans-serif fonts work best for interfaces
- **Text Fonts**: Serif fonts are good for document reading

## 🎯 Recommended Fonts

### For UI/Interface
1. **DejaVu Sans** - Excellent for all interface elements
2. **Liberation Sans** - Good alternative to system fonts
3. **Ubuntu** - Modern, clean appearance

### For Readability
1. **DejaVu Sans** - Superior Unicode coverage
2. **Liberation Sans** - Good for long text
3. **System Default** - Always reliable

### For Modern Look
1. **Ubuntu** - Contemporary design
2. **Liberation Sans** - Clean and minimal

## 🔍 Troubleshooting

### Font Not Loading
```
Problem: Font file exists but doesn't appear in list
Solution: 
- Check file is valid .ttf format
- Ensure file permissions are readable
- Restart application
- Check console output for error messages
```

### Font Looks Blurry
```
Problem: Text appears blurry or pixelated
Solution:
- Try different font sizes (avoid very small sizes)
- Use fonts designed for screen display
- Check if font file is corrupted
```

### Font Size Too Small/Large
```
Problem: Font size is not comfortable
Solution:
- Use the size slider in Font Settings
- Recommended range: 14-20px for most users
- Test with preview text before applying
```

## 🛠️ Technical Details

### Font Loading Process
1. **System Font Detection**: Scans common system font directories
2. **Local Font Loading**: Reads fonts from `fonts/` directory
3. **Font Validation**: Verifies font files are valid
4. **Atlas Building**: Creates ImGui font atlas
5. **Font Registration**: Makes fonts available for selection

### Font Storage
- **Default Fonts**: Built into ImGui
- **System Fonts**: Located in OS-specific directories
- **Local Fonts**: Stored in `./fonts/` directory
- **Font Cache**: Handled automatically by ImGui

### Performance
- **Memory Usage**: Each font uses ~1-2MB RAM
- **Loading Time**: Fonts loaded once at startup
- **Real-time Changes**: Size changes rebuild font atlas

## 📝 Font Settings UI

### Main Controls
- **Font Dropdown**: Select active font
- **Size Slider**: Adjust font size (8-32px)
- **Preview Area**: See text samples
- **Apply Button**: Confirm changes (auto-applied)
- **Reset Button**: Return to default font
- **Close Button**: Exit font settings

### Preview Content
- **Regular Text**: "The quick brown fox jumps over the lazy dog"
- **Uppercase**: Full alphabet in caps
- **Lowercase**: Full alphabet in lowercase
- **Numbers/Symbols**: Digits and common symbols
- **Application Text**: Sample text as it appears in PQC Wallet
- **Status Messages**: Examples of success/warning text

## 🚀 Future Enhancements

### Planned Features
- **Font Themes**: Predefined font combinations
- **Icon Fonts**: FontAwesome integration
- **Font Scaling**: DPI-aware font scaling
- **Font Fallbacks**: Automatic fallback fonts
- **Font Caching**: Persistent font preferences

### Advanced Options
- **Custom Font Directories**: Multiple font source directories
- **Font Variants**: Bold, italic, light weights
- **Text Rendering**: Subpixel rendering options
- **Font Metrics**: Advanced typography controls

## 📚 Resources

### Font Downloads
- [Google Fonts](https://fonts.google.com/) - Free web fonts
- [Adobe Fonts](https://fonts.adobe.com/) - Professional fonts
- [Font Squirrel](https://www.fontsquirrel.com/) - Free commercial fonts
- [DejaVu Fonts](https://dejavu-fonts.github.io/) - Unicode fonts

### Font Tools
- [FontForge](https://fontforge.org/) - Font editor
- [FontExplorer](https://www.fontexplorerx.com/) - Font manager
- [WhatFont](https://www.whatfont.com/) - Font identifier

## 💡 Tips & Best Practices

1. **Start with System Fonts**: They're optimized for your OS
2. **Test Readability**: Use the preview to ensure text is clear
3. **Consider Context**: Different fonts for different purposes
4. **Size Matters**: Larger fonts for accessibility
5. **Backup Preferences**: Note your preferred settings
6. **Regular Updates**: Check for new font releases

---

*For more information about PQC Wallet's font system, see the technical documentation or contact the development team.*
