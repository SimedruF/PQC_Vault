# PQC Wallet - Post-Quantum Cryptography Wallet

A secure wallet application using Dear ImGui with advanced post-quantum cryptography for password protection and encrypted file storage.

## 🔐 Security Features

- **Post-Quantum Cryptography**: Uses Kyber-768 algorithm from liboqs
- **Enhanced Security v2.0**: Multi-layer encryption with AES-256-GCM + Kyber
- **Quantum-Safe**: Resistant to attacks from quantum computers  
- **Encrypted Storage**: All passwords encrypted with Scrypt key derivation
- **Multi-User Support**: Each user has unique quantum-safe key pairs
- **Encrypted Archives**: Secure file storage with post-quantum encryption
- **Authentication Tags**: Integrity verification prevents file tampering
- **Restrictive Permissions**: File system level protection (600/700 permissions)

## ✅ Status: FULLY FUNCTIONAL & SECURITY ENHANCED

- ✅ GUI application with Dear ImGui (docking branch)
- ✅ **ENHANCED**: Multi-layer security implementation (v2.0)
- ✅ First-time setup for password creation with strong key derivation
- ✅ Kyber-768 + AES-256-GCM encryption implementation
- ✅ Login authentication with enhanced encrypted passwords
- ✅ Multi-user support with dropdown selection
- ✅ **WORKING**: Encrypted archives for secure file storage
- ✅ **NEW**: Multiple archives support per user
- ✅ **VERIFIED**: Archive selection and creation functionality
- ✅ **IMPROVED**: ImGuiFileDialog integration for file browsing
- ✅ **NEW**: Advanced font management system with custom fonts
- ✅ **SECURITY**: Legacy attack tools neutralized
- ✅ Cross-platform compatibility (Linux tested, Windows/macOS supported)

## 🔤 Font Management System v1.0

### Customizable Typography
- **Multiple Font Support**: System fonts and custom font loading
- **Real-time Font Switching**: Change fonts without restart
- **Dynamic Font Sizing**: Adjust font size from 8px to 32px
- **Live Preview**: See changes before applying
- **Font Auto-Detection**: Automatically finds system fonts
- **Custom Font Directory**: Load fonts from `fonts/` folder

### Font Features
- **System Integration**: DejaVu Sans, Liberation Sans, Ubuntu fonts
- **Custom Fonts**: Support for TTF font files
- **Font Preview**: Real-time text preview with multiple samples
- **Accessibility**: Adjustable sizing for better readability
- **Cross-Platform**: Works on Linux, macOS, and Windows

## �️ Security Improvements v2.0

### Enhanced Encryption Stack
- **Key Derivation**: Scrypt with strong parameters (N=32768, r=8, p=1)
- **Secret Key Protection**: AES-256-GCM encryption of Kyber secret keys
- **Double Password Encryption**: Kyber + AES layers for maximum security
- **Authentication Tags**: Integrity verification prevents tampering
- **Random Salt & IV**: Unique cryptographic parameters per user
- **File Permissions**: Restrictive OS-level access control

### Security Comparison

| Feature | Version 1.0 | Version 2.0 |
|---------|-------------|-------------|
| Secret Key Storage | ❌ Plaintext | ✅ AES-256-GCM Encrypted |
| Password Encryption | ❌ XOR only | ✅ Kyber + AES-256-GCM |
| Key Derivation | ❌ None | ✅ Scrypt (Strong params) |
| Integrity Check | ❌ None | ✅ Authentication Tags |
| Salt/IV | ❌ None | ✅ Random per user |
| File Permissions | ❌ Default | ✅ Restrictive (600/700) |
| Legacy Tools | ❌ Working | ✅ Neutralized |

## 🛡️ Post-Quantum Security

### Simple Usage Guide
PQC Wallet uses quantum-resistant algorithms to protect your data:

1. **Login**: Your password is protected with quantum-safe encryption
2. **File Storage**: Archives use hybrid encryption (classical + post-quantum)
3. **Data Protection**: All sensitive data is encrypted with multiple layers

### Encryption Algorithms Used

#### Password Protection
```
User Password → Scrypt → AES-256-GCM + Kyber768
```
- **Scrypt**: Key derivation function resistant to hardware attacks
- **AES-256-GCM**: 256-bit authenticated encryption (quantum-vulnerable but still strong)
- **Kyber768**: NIST-selected post-quantum key encapsulation mechanism

#### Archive Encryption
```
Archive Files → AES-256-CTR → Kyber768 KEM → Secure Storage
```
- **AES-256-CTR**: Fast file encryption for large data
- **Kyber768**: Protects the AES keys against quantum attacks
- **Authentication**: HMAC-SHA256 for data integrity

#### Detailed Algorithm Specifications

**Kyber768 (Post-Quantum)**:
- Security Level: 192-bit (equivalent to AES-192)
- Key Size: 2400 bytes (public), 2400 bytes (secret)
- Ciphertext Size: 1088 bytes
- Based on: Module Learning With Errors (M-LWE) problem
- Quantum Resistance: ✅ Proven secure against quantum computers

**Scrypt (Key Derivation)**:
- Parameters: N=32768, r=8, p=1
- Output: 256-bit derived key
- Memory Cost: ~32MB (prevents ASIC attacks)
- Time Cost: Configurable difficulty

**AES-256-GCM (Symmetric)**:
- Key Size: 256 bits
- Block Size: 128 bits
- Authentication: Built-in AEAD (Authenticated Encryption)
- IV Size: 128 bits (randomly generated)

### Security Levels

| Component | Classical Security | Quantum Security |
|-----------|-------------------|------------------|
| Kyber768 | 192-bit | 192-bit ✅ |
| AES-256 | 256-bit | 128-bit ⚠️ |
| Scrypt | Configurable | Memory-hard ✅ |
| HMAC-SHA256 | 256-bit | 128-bit ⚠️ |

**Legend**:
- ✅ = Quantum-resistant
- ⚠️ = Quantum-vulnerable but computationally infeasible

### Why Post-Quantum?

Future quantum computers will break current encryption:
- **RSA**: Vulnerable to Shor's algorithm
- **ECC**: Vulnerable to Shor's algorithm  
- **AES**: Strength halved by Grover's algorithm
- **Kyber**: Immune to known quantum attacks

PQC Wallet protects your data today and in the quantum future.

## �🚀 Quick Start

### Prerequisites
- CMake 3.16+
- OpenGL development libraries
- OpenSSL development libraries (for enhanced security)
- GLFW libraries and headers
- C++17 compatible compiler
- liboqs (automatically installed by setup script)

See the detailed installation guide below for instructions on installing all dependencies.

### Installation

#### Linux/macOS
```bash
# Clone repository with all submodules
git clone --recursive https://github.com/SimedruF/PQCWallet-Core.git
cd PQCWallet

# Make setup script executable and run it
chmod +x setup.sh
./setup.sh
```

#### Windows
```batch
# Clone repository with all submodules
git clone --recursive https://github.com/SimedruF/PQCWallet-Core.git
cd PQCWallet

# Option 1: Use the setup wrapper (recommended)
setup_windows.bat

# Option 2: Run specific scripts manually
build\scripts\setup_dependencies_windows.bat
build\scripts\build_windows.bat
```

For detailed Windows build instructions, see: `build/docs/BUILD_WINDOWS.md`

### Running

#### Linux/macOS
```bash
./run.sh
```

#### Windows
```batch
# From build directory
build\PQCWallet.exe

# Or using the wrapper script
setup_windows.bat
# (select option 2 to build and run)
```

### Creating Desktop Shortcut

#### Linux/macOS
```bash
# Create desktop shortcut after building the application
./create_desktop_shortcut.sh
```

This creates:
- Desktop shortcut for easy access
- Applications menu entry
- Proper file associations

#### Windows
```batch
# Create desktop shortcut after building the application
create_desktop_shortcut_windows.bat

# Or use the wrapper script
setup_windows.bat
# (select option 5 to create shortcut)
```

This creates a desktop shortcut with proper Windows integration.

### Generating Custom Icon

#### Automatic Icon Generation (recommended)
```bash
# Generate a professional PQC-themed icon
./generate_icon.sh
```

#### Simple Icon Creation (fallback)
```bash
# Create a simple SVG-based icon (no ImageMagick required)
./create_simple_icon.sh
```

Both scripts create multiple icon sizes and formats for optimal cross-platform compatibility.

### Font Setup (Optional)
For additional fonts, download popular font packages:
```bash
# Download and install popular fonts
./download_fonts.sh

# Or manually add TTF fonts to fonts/ directory
cp your-font.ttf fonts/
```

Access font settings via `View → Font Settings` in the application.

### Security Migration
For existing users, migrate to enhanced security:
```bash
# Compile and run the migration tool
g++ -std=c++17 -I. migrate_security.cpp src/PasswordManager.cpp -loqs -lssl -lcrypto -o migrate_security
./migrate_security

# Test security improvements
./demo_security.sh
```

## 📁 Project Structure

```
PQCWallet/
├── src/
│   ├── main.cpp                 # Application entry point
│   ├── LoginWindow.cpp/.h       # Login interface with user selection
│   ├── WalletWindow.cpp/.h      # Main wallet interface with archive management
│   ├── PasswordManager.cpp/.h   # Enhanced Kyber+AES encryption manager
│   ├── CryptoArchive.cpp/.h     # Multi-archive encrypted storage manager
│   ├── ArchiveWindow.cpp/.h     # Archive GUI interface with file management
│   ├── FontManager.cpp/.h       # Font management and customization system
│   └── FirstTimeSetupWindow.cpp/.h # Initial setup interface
├── third_party/
│   └── ImGuiFileDialog/         # File dialog library for intuitive browsing
├── fonts/                       # Custom fonts directory (TTF files)
├── users/                       # Enhanced encrypted password storage (v2.0)
├── archives/                    # Multiple encrypted file archives per user
├── build/                       # Build artifacts
├── tools/                       # Security analysis and migration tools
├── docs/                        # Documentation and guides
├── setup.sh                     # Enhanced installation script
├── run.sh                       # Application launcher
├── download_fonts.sh            # Font download script
├── migrate_security.cpp         # Security migration tool
├── demo_security.sh            # Security demonstration script
├── README.md                    # This file
├── FONT_GUIDE.md               # Font management guide
├── SECURITY_IMPROVEMENTS.md     # Detailed security analysis
├── PASSWORD_EXTRACTION_GUIDE.md # Security research documentation
├── USAGE.md                     # Detailed usage instructions
├── EXAMPLES.md                  # Code examples
├── ARCHIVE_GUIDE.md             # Archive usage guide
├── IMGUI_FILE_DIALOG_GUIDE.md   # File dialog integration guide
└── TEST_RESULTS.md              # Test verification results
```

## 🔧 Technical Details

### Post-Quantum Cryptography Implementation v2.0
- **Primary Algorithm**: Kyber-768 (NIST PQC standardized)
- **Secondary Encryption**: AES-256-GCM for additional protection
- **Library**: liboqs (Open Quantum Safe) + OpenSSL
- **Key Derivation**: Scrypt with parameters N=32768, r=8, p=1
- **Key Sizes**: 1184 bytes public key, 2400 bytes secret key
- **Security Level**: Equivalent to AES-192 against quantum attacks + classical protection
- **File Format**: Enhanced binary encrypted files (~4.8KB per user)

### Enhanced Password Security
- **Storage**: Passwords never stored in plaintext
- **Key Derivation**: Scrypt with random 256-bit salt
- **Primary Encryption**: XOR with Kyber-derived shared secret
- **Secondary Encryption**: AES-256-GCM with authentication tags
- **Secret Key Protection**: Kyber secret keys encrypted with AES-256-GCM
- **Verification**: Multi-layer decryption-based authentication
- **Integrity**: Authentication tags prevent file tampering
- **File Permissions**: Restrictive OS-level protection (600 for files, 700 for directories)

### GUI Implementation
- **Framework**: Dear ImGui with docking support
- **Graphics**: OpenGL 3.0+ with GLFW
- **Windows**: Enhanced Login, FirstTimeSetup, Wallet, and Archive interfaces
- **Archive Management**: Multiple archives per user with intuitive switching
- **File Operations**: Graphical file browser with ImGuiFileDialog
- **User Experience**: Settings moved to TopBar, improved navigation
- **Styling**: Custom dark theme with modern appearance

## 📋 Usage Workflow

### Enhanced Security Workflow v2.0
1. **First Run**: Application detects no users and shows setup window
2. **User Creation**: Enter username and password, encrypted with enhanced Kyber+AES
3. **Key Derivation**: Scrypt generates strong encryption keys from password
4. **Secure Storage**: All cryptographic material stored with authentication tags
5. **Login**: Select user from dropdown and enter password
6. **Authentication**: Multi-layer password verification using Kyber+AES
7. **Wallet Access**: Main wallet interface opens upon successful login
8. **Archive Management**: Access multiple encrypted archives per user
9. **Settings Access**: Use TopBar settings button for configuration

### Legacy User Migration
- **Automatic Detection**: System detects old format files
- **Secure Migration**: Backup and upgrade to enhanced security
- **Verification**: Confirm migration success before removing backup
- **Tool Neutralization**: Old extraction tools no longer work

## 🗃️ Enhanced Encrypted Archive Features

### Secure File Storage v2.0
- **Post-Quantum Encryption**: Files encrypted with Kyber-768 + AES-256-GCM
- **Multiple Archives**: Create and manage multiple archives per user
- **Archive Switching**: Seamlessly switch between different archives
- **File Management**: Add, extract, preview, and remove files securely
- **Integrity Verification**: SHA-256 hash verification for each file
- **User Isolation**: Each user has their own encrypted archive collection
- **Graphical File Browser**: Enhanced ImGuiFileDialog for intuitive file selection
- **Debug Capabilities**: Comprehensive diagnostic tools for troubleshooting

### Archive Operations
- **Create Archives**: Create new archives with custom names
- **Archive Selection**: Choose from available archives in dropdown list
- **Add Files**: Import files using enhanced graphical file picker
- **Extract Files**: Export files using improved folder selection dialog
- **File Preview**: View text files and basic image information
- **Archive Statistics**: View total files, size, and last modified time
- **Password Management**: Change archive passwords securely
- **Archive Diagnostics**: Built-in repair and diagnostic tools

### Multiple Archives Management
- **Archive Creation**: Create new archives for better organization
- **Archive Listing**: View all available archives for current user
- **Default Archive**: "img" archive created automatically for new users
- **Archive Switching**: Load different archives without restarting application
- **Isolated Storage**: Each archive has independent encryption and file storage
- **Archive Naming**: Custom names for better organization (e.g., "Documents", "Photos", "Work")

### Enhanced File Dialog Features
- **Visual Navigation**: Browse filesystem with improved interface
- **File Type Filtering**: Advanced filtering by file extensions
- **Path Validation**: Automatic path validation and correction
- **Multi-platform Support**: Consistent experience across operating systems
- **Drag & Drop**: Enhanced drag and drop support (implementation in progress)
- **Folder Selection**: Dedicated folder picker for extract operations
- **Recent Paths**: Remember frequently used paths

### Usage Examples
```bash
# After successful login, select from your archives in the main interface
# Choose from available archives or create new ones via the menu
# Click "Add Files" and use the enhanced file browser
# Files stored in archives/username_archivename.enc
# Use "Extract Selected" with improved folder picker
# Switch between archives using the archive selection interface
```

### Multiple Archives Implementation Details

#### Archive Organization
Each user can create and manage multiple archives for better file organization:

```
archives/username_archivename.enc
```

Examples:
```
archives/john_img.enc         # Default "img" archive for user "john"
archives/john_documents.enc  # "documents" archive for user "john"  
archives/john_photos.enc     # "photos" archive for user "john"
archives/john_work.enc        # "work" archive for user "john"
```

#### Usage Workflow
1. After authentication, you'll see the list of available archives
2. Select the desired archive from the list
3. Click "Open Selected Archive" to open the selected archive
4. Or click "Create New Archive" to create a new archive
5. Enter a name for the new archive and confirm

Each archive is independent and can contain its own set of files, all protected by the same user password.

#### Archive Management Features
- **Independent Storage**: Each archive maintains separate file storage and metadata
- **Seamless Switching**: Switch between archives without application restart
- **Custom Organization**: Group files by purpose, project, or category
- **Shared Security**: All archives use the same user authentication
- **Scalable Design**: No limit on number of archives per user
- **Efficient Navigation**: Quick archive selection from dropdown interface
## 🧪 Enhanced Testing & Validation

### Security Testing v2.0
```bash
# Test enhanced security migration
./migrate_security

# Demonstrate security improvements  
./demo_security.sh

# Verify old tools are neutralized
./extract_password  # Should fail with enhanced format
```

### Build Testing
```bash
cd build && cmake .. && make -j$(nproc)
```

### Encryption Testing
```bash
# Test enhanced password encryption
./test_encryption_v2

# Test legacy compatibility
./test_legacy_support
```

### Archive Testing
```bash
# Test multiple archives functionality
./test_multi_archives

# Test archive switching
./test_archive_switching
```

### GUI Testing
```bash
# Test complete application
./run.sh

# Test with multiple users
./test_multi_user
```

See `TEST_RESULTS.md` for complete test verification.

## 🔍 Enhanced Security Validation v2.0

### Security Features Verification
- **Multi-Layer Encryption**: Kyber-768 + AES-256-GCM verified working
- **Enhanced Key Derivation**: Scrypt with strong parameters (N=32768, r=8, p=1)
- **Authenticated Encryption**: AES-GCM provides both encryption and authentication
- **Legacy Tool Neutralization**: Old extraction tools confirmed non-functional
- **File Permissions**: Restrictive OS-level permissions (600/700) enforced
- **Migration Safety**: Secure upgrade path from v1.0 to v2.0 format
- **Quantum Resistance**: Kyber-768 provides security against quantum attacks
- **Encrypted Files**: All password data encrypted with enhanced security
- **Encrypted Archives**: User files stored in quantum-safe archives
- **Unique Keys**: Each user has independent cryptographic material
- **Access Control**: Enhanced file-based user isolation
- **No Plaintext**: Passwords never stored or transmitted in plaintext
- **Integrity Protection**: Authentication tags prevent file tampering

### Security Analysis Tools
- **Migration Tool**: `migrate_security.cpp` - Safely upgrade legacy users
- **Security Demo**: `demo_security.sh` - Demonstrate security improvements
- **Analysis Tools**: Various tools for security research and validation

## 📚 Enhanced Documentation

### Primary Documentation
- **README.md**: This comprehensive guide
- **SECURITY_IMPROVEMENTS.md**: Detailed security analysis and improvements
- **PASSWORD_EXTRACTION_GUIDE.md**: Security research documentation
- **USAGE.md**: Detailed usage instructions
- **EXAMPLES.md**: Code examples and API documentation
- **TEST_RESULTS.md**: Complete test verification results
- **ARCHIVE_GUIDE.md**: Multi-archive usage instructions
- **IMGUI_FILE_DIALOG_GUIDE.md**: File dialog integration details

### Security Documentation
- Analysis of security vulnerabilities in v1.0
- Detailed implementation of security improvements in v2.0
- Migration procedures and compatibility considerations
- Security testing methodologies and results

## 🛠️ Enhanced Dependencies

### Runtime Dependencies v2.0
- OpenGL 3.0+ (graphics rendering)
- GLFW 3.3+ (window management)
- **OpenSSL 3.0+** (enhanced cryptographic operations)
- liboqs 0.8+ (post-quantum cryptography)
- Dear ImGui (included as submodule)
- ImGuiFileDialog (included as submodule)
- stb_image (included in third_party)

### Build Dependencies
- CMake 3.16+
- C++17 compatible compiler (GCC 8+, Clang 10+, MSVC 2019+)
- OpenGL development headers
- GLFW development headers
- **OpenSSL development headers** (required for enhanced security)
- pkg-config (for dependency resolution)

## 📦 Detailed Installation Guide

### Required Libraries Installation

#### Debian/Ubuntu:
```bash
# Update package lists
sudo apt update

# Install base build tools
sudo apt install -y build-essential git cmake

# Install OpenGL dependencies
sudo apt install -y libgl1-mesa-dev libglu1-mesa-dev

# Install GLFW dependencies
sudo apt install -y libglfw3-dev

# Install OpenSSL
sudo apt install -y libssl-dev

# Install pkg-config (needed by build system)
sudo apt install -y pkg-config
```

#### Fedora/CentOS/RHEL:
```bash
# Install base build tools
sudo dnf install -y gcc g++ git cmake make

# Install OpenGL dependencies
sudo dnf install -y mesa-libGL-devel mesa-libGLU-devel

# Install GLFW
sudo dnf install -y glfw-devel

# Install OpenSSL
sudo dnf install -y openssl-devel

# Install pkg-config
sudo dnf install -y pkgconf
```

#### Arch Linux:
```bash
# Install base build tools
sudo pacman -S base-devel git cmake

# Install OpenGL dependencies
sudo pacman -S mesa

# Install GLFW
sudo pacman -S glfw-x11 # or glfw-wayland for wayland users

# Install OpenSSL
sudo pacman -S openssl

# Install pkg-config
sudo pacman -S pkgconf
```

#### macOS (using Homebrew):
```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install base build tools
brew install cmake

# Install GLFW
brew install glfw

# Install OpenSSL
brew install openssl

# Create symlinks for OpenSSL (required for finding OpenSSL during build)
brew link openssl --force
```

### Installing liboqs (Open Quantum Safe)

The setup script will install liboqs automatically, but if you want to install it manually:

```bash
# Clone liboqs repository
git clone --branch main https://github.com/open-quantum-safe/liboqs.git

# Create build directory
cd liboqs && mkdir build && cd build

# Configure with CMake
cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS=ON ..

# Build
make -j $(nproc)

# Install (requires root privileges)
sudo make install

# Update dynamic linker
sudo ldconfig
```

### Cloning the Repository with Submodules

```bash
# Clone the repository with all submodules
git clone --recursive https://github.com/SimedruF/PQCWallet-Core.git

# If you already cloned without --recursive:
cd PQCWallet
git submodule update --init --recursive
```

### Building from Source Manually

If you prefer to build the project manually instead of using the setup script:

```bash
# Create and enter build directory
mkdir -p build && cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)

# Return to project root
cd ..
```

### CMake Build Options

You can customize the build using the following CMake options:

```bash
# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Specify custom liboqs installation path
cmake -Dliboqs_DIR=/path/to/liboqs/lib/cmake/liboqs ..

# Specify custom OpenSSL path
cmake -DOPENSSL_ROOT_DIR=/path/to/openssl ..

# Build with sanitizers (for development only)
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..
```

### Troubleshooting

#### Cannot Find liboqs
If CMake cannot find liboqs, you may need to specify the path manually:
```bash
cmake -Dliboqs_DIR=/usr/local/lib/cmake/liboqs ..
```

#### OpenSSL Not Found
On some systems, you may need to specify the OpenSSL path:
```bash
cmake -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl ..  # For macOS with Homebrew
```

#### GLFW Issues
If you encounter GLFW-related errors:
```bash
# For Ubuntu/Debian
sudo apt install libglfw3-dev xorg-dev

# For macOS
brew install glfw
```

#### Linker Errors
If you get linker errors about missing libraries:
```bash
# Update dynamic linker cache
sudo ldconfig
```
## 🏗️ Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   GUI Layer     │    │ Authentication  │    │ Crypto Layer    │
│  (Dear ImGui)   │◄──►│   (Login/Setup) │◄──►│  (Kyber/liboqs) │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                        │                        │
         ▼                        ▼                        ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Main Loop     │    │  PasswordManager│    │  File Storage   │
│  (Application)  │◄──►│   (Encryption)  │◄──►│  (users/*.enc)  │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## 🚀 Future Enhancements

- **Additional PQC Algorithms**: Support for Dilithium signatures
- **Hardware Security**: Integration with hardware security modules
- **Network Security**: Quantum-safe network protocols
- **Blockchain Integration**: Post-quantum blockchain interactions
- **Multi-Factor Authentication**: Combine with biometric security
- **Cross-Platform**: Windows and macOS support

## 🤝 Contributing

This project demonstrates post-quantum cryptography implementation. Contributions welcome:

1. Fork the repository
2. Create a feature branch
3. Test thoroughly
4. Submit pull request

## 📄 License

Open source project for educational and development purposes.

## 🆘 Support

For issues or questions:
- [Open an issue on GitHub](https://github.com/SimedruF/PQCWallet-Core/issues) for support or bug reporting


---

**Note**: This implementation provides quantum-safe password storage using the Kyber algorithm. The cryptographic implementation follows NIST PQC standards and uses the liboqs library for quantum-resistant security.
