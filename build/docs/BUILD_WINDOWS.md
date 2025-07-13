# PQC Wallet - Windows Build Instructions

This document provides detailed instructions for building PQC Wallet on Windows.

## Prerequisites

### Required Software

1. **Visual Studio 2022** (Community Edition or higher)
   - Install with "Desktop development with C++" workload
   - Download from: https://visualstudio.microsoft.com/downloads/

2. **CMake 3.16 or higher**
   - Download from: https://cmake.org/download/
   - Make sure to add CMake to PATH during installation

3. **Git** (for cloning repositories and submodules)
   - Download from: https://git-scm.com/download/win

### Optional but Recommended

4. **vcpkg** (C++ package manager)
   ```cmd
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg integrate install
   ```
   - Set `VCPKG_ROOT` environment variable to the vcpkg directory

## Quick Start

### Option 1: Automated Build (Recommended)

1. **Install Dependencies**
   ```cmd
   setup_dependencies_windows.bat
   ```

2. **Build the Application**
   ```cmd
   build_windows.bat
   ```

### Option 2: Manual Build

1. **Install Dependencies with vcpkg**
   ```cmd
   vcpkg install glfw3:x64-windows openssl:x64-windows
   ```

2. **Build liboqs** (if not available in vcpkg)
   ```cmd
   build_liboqs_windows.bat
   ```

3. **Configure and Build**
   ```cmd
   mkdir build
   cd build
   cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
   cmake --build . --config Release --parallel
   ```

## Dependencies

### Core Dependencies

- **OpenGL** - Usually available on Windows
- **GLFW3** - Window management library
- **OpenSSL** - Cryptographic library
- **liboqs** - Post-quantum cryptography library

### Installing Dependencies

#### Using vcpkg (Recommended)

```cmd
# Install vcpkg if not already installed
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Install dependencies
vcpkg install glfw3:x64-windows openssl:x64-windows

# Try to install liboqs (may not be available)
vcpkg search liboqs
vcpkg install liboqs:x64-windows
```

#### Manual Installation

If vcpkg doesn't have a package, you'll need to build from source:

1. **GLFW3**: Download from https://www.glfw.org/
2. **OpenSSL**: Download from https://slproweb.com/products/Win32OpenSSL.html
3. **liboqs**: Use `build_liboqs_windows.bat` or build manually

## Building liboqs from Source

liboqs is the most complex dependency. Use the provided script:

```cmd
build_liboqs_windows.bat
```

Or build manually:

```cmd
git clone https://github.com/open-quantum-safe/liboqs.git
cd liboqs
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_INSTALL_PREFIX=../install
cmake --build . --config Release --parallel
cmake --build . --config Release --target install
```

## Troubleshooting

### Common Issues

1. **CMake can't find dependencies**
   - Make sure vcpkg is properly integrated: `vcpkg integrate install`
   - Set environment variables manually if needed
   - Check that dependencies are installed for the correct architecture (x64)

2. **liboqs not found**
   - Run `build_liboqs_windows.bat`
   - Or set `LIBOQS_ROOT` environment variable to installation path
   - Update CMakeLists.txt with correct paths

3. **OpenSSL errors**
   - Install OpenSSL via vcpkg: `vcpkg install openssl:x64-windows`
   - Or download pre-built binaries from https://slproweb.com/products/Win32OpenSSL.html

4. **GLFW3 errors**
   - Install via vcpkg: `vcpkg install glfw3:x64-windows`
   - Or download from https://www.glfw.org/

5. **Visual Studio version issues**
   - Update the generator in scripts to match your VS version:
     - VS 2019: `"Visual Studio 16 2019"`
     - VS 2022: `"Visual Studio 17 2022"`

### Verifying Installation

After building, you should have:
- `build/Release/PQCWallet.exe` (or `build/Debug/PQCWallet.exe`)
- Required DLLs in the same directory (automatically copied if using vcpkg)

### Running the Application

```cmd
cd build/Release
PQCWallet.exe
```

## Environment Variables

Useful environment variables for building:

- `VCPKG_ROOT` - Path to vcpkg installation
- `LIBOQS_ROOT` - Path to liboqs installation (if built manually)
- `OPENSSL_ROOT_DIR` - Path to OpenSSL installation (if not using vcpkg)

## Build Configuration

The build system supports both Debug and Release configurations:

```cmd
# Debug build
cmake --build . --config Debug

# Release build (recommended for distribution)
cmake --build . --config Release
```

## Distribution

When distributing the application, make sure to include:

1. `PQCWallet.exe`
2. Required DLLs (OpenSSL, etc.)
3. Any configuration files
4. Visual C++ Redistributable (if not already installed on target system)

The build scripts automatically copy required DLLs when using vcpkg.

## Support

If you encounter issues:

1. Check this README for common solutions
2. Verify all prerequisites are installed
3. Check CMake output for specific error messages
4. Ensure you're using the correct architecture (x64)

For additional help, refer to the main project documentation or create an issue in the project repository.
