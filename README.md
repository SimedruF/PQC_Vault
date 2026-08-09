# PQCWallet

PQCWallet is an experimental C++17 desktop application for managing passwords,
encrypted databases, and encrypted file archives. The interface is built with
Dear ImGui, GLFW, and OpenGL. Cryptographic operations use OpenSSL and liboqs.

> [!WARNING]
> This project has not received an independent security audit. Treat it as
> experimental software and do not use it as the only copy of critical data.

## Current security design

- New user records use ML-KEM-768, scrypt (`N=32768`, `r=8`, `p=1`), and
  AES-256-GCM with independent 96-bit nonces.
- The portable user format is currently version 5. Authenticated legacy formats
  are migrated to the current ML-KEM format after a successful login.
- Archive format version 2 uses scrypt and AES-256-GCM with authenticated,
  versioned metadata.
- Encrypted database format version 2 and backup format version 1 use scrypt and
  AES-256-GCM.
- Sensitive writes use atomic replacement and transactional file batches where
  applicable. Path and container validation are covered by dedicated tests.
- Sensitive buffers use explicit cleanup helpers where the implementation can
  control their lifetime.

ML-KEM protects the KEM portion of the user credential format. It does not make
weak master passwords safe, and it does not replace the symmetric encryption
used for archives and databases.

## Features

- multi-user login and first-time setup;
- password manager with encrypted persistence;
- encrypted database backup and restore;
- authenticated encrypted archives;
- archive creation, extraction, and drag-and-drop input;
- configurable themes and local fonts;
- Linux and Windows build coverage;
- security tests, ASan/UBSan presets, and a bounded libFuzzer target.

## Dependencies

Required:

- CMake 3.16 or newer (3.21 or newer for the included presets);
- a C++17 compiler;
- OpenSSL;
- GLFW and OpenGL development packages;
- liboqs with ML-KEM-768 enabled.

The synchronized repository contains the Dear ImGui and ImGuiFileDialog sources
used by the application. `liboqs` remains an external dependency and is not
copied by the synchronization script.

On Ubuntu or Debian, install the system dependencies with:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build git \
  libssl-dev libglfw3-dev libgl1-mesa-dev
```

## Build on Linux

Clone the project and build liboqs inside the project directory:

```bash
git clone https://github.com/SimedruF/PQCWallet-Core.git
cd PQCWallet-Core

git clone https://github.com/open-quantum-safe/liboqs.git liboqs
cmake -S liboqs -B liboqs/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOQS_BUILD_ONLY_LIB=ON \
  -DBUILD_SHARED_LIBS=OFF
cmake --build liboqs/build --parallel 2
```

Configure, compile, and test PQCWallet:

```bash
cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON
cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure
```

Run the application:

```bash
./build/release/PQCWallet
```

## Sanitizer builds

The included CMake presets enable AddressSanitizer and
UndefinedBehaviorSanitizer:

```bash
cmake --preset linux-sanitized-debug
cmake --build --preset linux-sanitized-debug
ctest --preset linux-sanitized-debug
```

The fuzz target requires Clang/libFuzzer:

```bash
cmake --preset linux-fuzz
cmake --build --preset linux-fuzz
./build/linux-fuzz/format_parser_fuzz fuzz/corpus -runs=20000
```

## Windows

The project is built with CMake and is tested in CI on Windows using Ninja,
vcpkg, OpenSSL, GLFW, and a locally built static liboqs. See the files under
`windows/` and the documents under `documentation/` for the current platform
notes.

## Repository layout

```text
src/                         Application sources
test_files/                  Security and regression tests
fuzz/                        Format-parser fuzz target and seed corpus
imgui/                       ImGui files required by GLFW/OpenGL3
third_party/ImGuiFileDialog/ File-dialog dependency
assets/                      Application icons
fonts/                       Optional bundled fonts
documentation/               Design, format, and security notes
windows/                     Windows helper scripts
git_setup/                   Repository synchronization tools
```

## Runtime data

The application can create sensitive data in directories such as `users/`,
`archives/`, `extracted/`, and `config/`. These files must not be committed.
The repository ignore rules cover the known runtime formats, but always inspect
the staged diff before publishing.

Keep independent backups of encrypted data. A forgotten master password or a
damaged authenticated container cannot be recovered by the application.

## Synchronizing `PQCWallet-Core`

Development takes place in the local PQCWallet workspace and the public Git
repository is maintained separately. From the development workspace, preview
the synchronization first:

```bash
cd /home/simedruf/Projects/PQCWallet
./git_setup/sync_core_repo.sh
```

Review the itemized changes, then apply them:

```bash
./git_setup/sync_core_repo.sh --apply
```

The script synchronizes the explicit public-file manifest, removes obsolete
files only inside managed directories, and refuses to apply over a dirty target
repository. It does not create commits or push to GitHub.

Review and publish from the target repository:

```bash
cd ~/PQCWallet-Core
git status --short --branch
git diff --check
git diff

git add -A
git diff --cached --check
git diff --cached

git commit -m "Update PQCWallet sources"
git push origin master
```

Never skip the staged-diff review when cryptographic code or file formats have
changed.
