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
- ✅ **WORKING**: Encrypted archive (img.enc) for secure file storage
- ✅ **VERIFIED**: Archive window opens correctly after login
- ✅ **NEW**: ImGuiFileDialog integration for file browsing
- ✅ Cross-platform compatibility (Linux tested)

## 🚀 Quick Start

### Prerequisites
- CMake 3.16+
- OpenGL development libraries
- OpenSSL development libraries
- liboqs (automatically installed by setup script)

### Installation
```bash
git clone [your-repo-url]
cd PQCWallet
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

### Build Dependencies
- CMake 3.16+
- C++17 compatible compiler
- OpenGL development headers
- GLFW development headers
- OpenSSL development headers

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
- Check `USAGE.md` for detailed instructions
- Review `TEST_RESULTS.md` for troubleshooting
- Examine `EXAMPLES.md` for code examples

---

**Note**: This implementation provides quantum-safe password storage using the Kyber algorithm. The cryptographic implementation follows NIST PQC standards and uses the liboqs library for quantum-resistant security.
