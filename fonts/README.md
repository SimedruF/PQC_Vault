# Fonts Directory

This directory contains custom fonts for the PQC Wallet application.

## Supported Fonts

The application will automatically try to load fonts in the following order:

1. **System Fonts** (automatically detected):
   - Linux: `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf`
   - Arch Linux: `/usr/share/fonts/TTF/DejaVuSans.ttf`
   - macOS: `/System/Library/Fonts/Arial.ttf`
   - Windows: `C:\Windows\Fonts\arial.ttf`

2. **Local Fonts** (place in this directory):
   - `DejaVuSans.ttf` - Clean, readable sans-serif font
   - `Roboto-Regular.ttf` - Modern Google font
   - `OpenSans-Regular.ttf` - Open Source sans-serif font

## Adding Custom Fonts

To add a custom font:

1. Copy your `.ttf` font file to this directory
2. Modify the `fontPaths` vector in `src/main.cpp` to include your font
3. Rebuild the application

## Recommended Fonts

For the best user experience, we recommend:

- **DejaVu Sans** - Excellent Unicode support, very readable
- **Roboto** - Modern, clean design
- **Open Sans** - Good for UI applications
- **Source Sans Pro** - Adobe's open source font

## Font Size

The default font size is 16px. You can modify the `fontSize` variable in `main.cpp` to change this.

## Downloading Fonts

You can download free fonts from:
- [Google Fonts](https://fonts.google.com/)
- [DejaVu Fonts](https://dejavu-fonts.github.io/)
- [Adobe Source Fonts](https://adobe-fonts.github.io/source-sans-pro/)

## License

Make sure any fonts you add are properly licensed for your use case.
