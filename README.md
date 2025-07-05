# PQC Wallet - Post-Quantum Cryptography Wallet

A secure wallet application using Dear ImGui with post-quantum cryptography (Kyber algorithm) for password protection.

## 🔐 Security Features

- **Post-Quantum Cryptography**: Uses Kyber-768 algorithm from liboqs
- **Quantum-Safe**: Resistant to attacks from quantum computers
- **Encrypted Storage**: All passwords encrypted at rest
- **Multi-User Support**: Each user has unique quantum-safe key pairs
- **Encrypted Archive**: Secure file storage with post-quantum encryption
- **Graphical File Browser**: ImGuiFileDialog for intuitive file management

## ✅ Status: FULLY FUNCTIONAL & TESTED

- ✅ GUI application with Dear ImGui (docking branch)
- ✅ First-time setup for password creation
- ✅ Kyber-768 encryption implementation
- ✅ Login authentication with encrypted passwords
- ✅ Multi-user support with dropdown selection
- ✅ **WORKING**: Encrypted archives for secure file storage
- ✅ **NEW**: Multiple archives support per user
- ✅ **VERIFIED**: Archive selection and creation functionality
- ✅ **IMPROVED**: ImGuiFileDialog integration for file browsing
- ✅ Cross-platform compatibility (Linux tested)

## 🚀 Quick Start

### Prerequisites
- CMake 3.16+
- OpenGL development libraries
- OpenSSL development libraries
- GLFW libraries and headers
- C++17 compatible compiler
- liboqs (automatically installed by setup script)

See the detailed installation guide below for instructions on installing all dependencies.

### Installation
```bash
# Clone repository with all submodules
git clone --recursive https://github.com/SimedruF/PQCWallet-Core.git
cd PQCWallet

# Make setup script executable and run it
chmod +x setup.sh
./setup.sh
```

### Running
```bash
./run.sh
```

## 📁 Project Structure

```
PQCWallet/
├── src/
│   ├── main.cpp                 # Application entry point
│   ├── LoginWindow.cpp/.h       # Login interface
│   ├── WalletWindow.cpp/.h      # Main wallet interface
│   ├── PasswordManager.cpp/.h   # Kyber encryption manager
│   ├── CryptoArchive.cpp/.h     # Encrypted archive manager
│   ├── ArchiveWindow.cpp/.h     # Archive GUI interface
│   └── FirstTimeSetupWindow.cpp/.h # Initial setup interface
├── third_party/
│   └── ImGuiFileDialog/         # File dialog library
├── users/                       # Encrypted password storage
├── archives/                    # Encrypted file archives (img.enc)
├── build/                       # Build artifacts
├── setup.sh                     # Installation script
├── run.sh                       # Application launcher
├── README.md                    # This file
├── USAGE.md                     # Detailed usage instructions
├── EXAMPLES.md                  # Code examples
├── ARCHIVE_GUIDE.md             # Archive usage guide
├── IMGUI_FILE_DIALOG_GUIDE.md   # File dialog integration guide
└── TEST_RESULTS.md              # Test verification results
```

## 🔧 Technical Details

### Post-Quantum Cryptography Implementation
- **Algorithm**: Kyber-768 (NIST PQC standardized)
- **Library**: liboqs (Open Quantum Safe)
- **Key Size**: 1184 bytes public key, 2400 bytes secret key
- **Security Level**: Equivalent to AES-192 against quantum attacks

### Password Security
- **Storage**: Passwords never stored in plaintext
- **Encryption**: XOR with Kyber-derived shared secret
- **Verification**: Decryption-based authentication
- **File Format**: Binary encrypted files (~4.7KB per user)

### GUI Implementation
- **Framework**: Dear ImGui with docking support
- **Graphics**: OpenGL 3.0+ with GLFW
- **Windows**: Login, FirstTimeSetup, and Wallet interfaces
- **Styling**: Custom dark theme with modern appearance

## 📋 Usage Workflow

1. **First Run**: Application detects no users and shows setup window
2. **User Creation**: Enter username and password, encrypted with Kyber
3. **Login**: Select user from dropdown and enter password
4. **Authentication**: Password decrypted and verified using Kyber
5. **Wallet Access**: Main wallet interface opens upon successful login
6. **Archive Access**: Click "Arhiva Securizata" to access encrypted file storage

## 🗃️ Encrypted Archive Features

### Secure File Storage
- **Post-Quantum Encryption**: Files encrypted with Kyber-768
- **File Management**: Add, extract, and remove files securely
- **Integrity Verification**: SHA-256 hash verification for each file
- **User Isolation**: Each user has their own encrypted archive
- **Graphical File Browser**: ImGuiFileDialog for intuitive file selection

### Archive Operations
- **Add Files**: Import files using graphical file picker
- **Extract Files**: Export files using folder selection dialog
- **File Preview**: View file contents (implementation in progress)
- **Archive Statistics**: View total files, size, and last modified
- **Drag & Drop**: Support for dragging files into archive (planned)

### New File Dialog Features
- **Visual Navigation**: Browse filesystem graphically
- **File Filtering**: Filter by file type
- **Path Validation**: Automatic path validation
- **Multi-platform**: Consistent experience across operating systems

### Usage Example
```bash
# After successful login, click "Arhiva Securizata"
# Click "Add Files" and use the file browser to select files
# Files are stored in archives/username_img.enc
# Use "Extract Selected" with folder picker to export files
```

See `ARCHIVE_GUIDE.md` for detailed archive usage instructions.
See `IMGUI_FILE_DIALOG_GUIDE.md` for file dialog integration details.

## 🗂️ Multiple Archives Support

PQC Wallet acum suportă gestionarea mai multor arhive per utilizator:

### Caracteristici de gestionare a arhivelor multiple
- **Creare de arhive noi**: Creați arhive multiple pentru organizare mai bună
- **Listare arhive**: Vizualizați toate arhivele disponibile pentru utilizatorul curent
- **Selectare arhive**: Comutați între diferite arhive în funcție de necesități
- **Nume personalizate**: Fiecare arhivă poate avea un nume unic pentru identificare ușoară

### Utilizare

1. După autentificare, veți vedea lista arhivelor disponibile
2. Selectați arhiva dorită din listă
3. Apăsați butonul "Open Selected Archive" pentru a deschide arhiva selectată
4. Sau apăsați "Create New Archive" pentru a crea o arhivă nouă
5. Introduceți un nume pentru noua arhivă și confirmați

Fiecare arhivă este independentă și poate conține propriul set de fișiere, toate protejate de aceeași parolă de utilizator.

### Format fișier
Arhivele sunt stocate în directorul `archives/` cu următorul format:
```
archives/username_archivename.enc
```

De exemplu:
```
archives/john_img.enc       # Arhiva default "img" pentru utilizatorul "john"
archives/john_documents.enc # Arhiva "documents" pentru utilizatorul "john"
archives/john_photos.enc    # Arhiva "photos" pentru utilizatorul "john"
```
## 🧪 Testing

### Build Test
```bash
cd build && cmake .. && make
```

### Encryption Test
```bash
./test_encryption
```

### Archive Test
```bash
./test_archive
```

### GUI Test
```bash
./run.sh
```

See `TEST_RESULTS.md` for complete test verification.

## 🔍 Security Validation

- **Quantum Resistance**: Kyber-768 provides security against quantum attacks
- **Encrypted Files**: All password data encrypted at rest
- **Encrypted Archive**: User files stored in quantum-safe archives
- **Unique Keys**: Each user has independent Kyber key pair
- **Access Control**: File-based user isolation
- **No Plaintext**: Passwords never stored or transmitted in plaintext

## 📚 Documentation

- **USAGE.md**: Detailed usage instructions
- **EXAMPLES.md**: Code examples and API documentation
- **TEST_RESULTS.md**: Complete test verification results

## 🛠️ Dependencies

### Runtime Dependencies
- OpenGL 3.0+
- GLFW 3.3+
- OpenSSL 3.0+
- liboqs 0.8+
- Dear ImGui (included as submodule)
- ImGuiFileDialog (included as submodule)
- stb_image (included in third_party)

### Build Dependencies
- CMake 3.16+
- C++17 compatible compiler
- OpenGL development headers
- GLFW development headers
- OpenSSL development headers

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
