cmake_minimum_required(VERSION 3.16)
project(PQCWallet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PQCWALLET_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(PQCWALLET_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(PQCWALLET_BUILD_FUZZERS "Build libFuzzer security targets" OFF)

if(PQCWALLET_ENABLE_ASAN OR PQCWALLET_ENABLE_UBSAN)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "ASan/UBSan require GCC or Clang in this project")
    endif()

    set(PQCWALLET_SANITIZER_FLAGS)
    if(PQCWALLET_ENABLE_ASAN)
        list(APPEND PQCWALLET_SANITIZER_FLAGS -fsanitize=address)
    endif()
    if(PQCWALLET_ENABLE_UBSAN)
        list(APPEND PQCWALLET_SANITIZER_FLAGS -fsanitize=undefined)
    endif()
    add_compile_options(${PQCWALLET_SANITIZER_FLAGS} -fno-omit-frame-pointer)
    add_link_options(${PQCWALLET_SANITIZER_FLAGS})
endif()

# Find packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Find liboqs - try different methods for cross-platform compatibility
if(WIN32)
    # Windows-specific paths
    find_path(OQS_INCLUDE_DIR oqs/oqs.h PATHS 
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/liboqs/install/include
        ${LIBOQS_ROOT}/include
        $ENV{LIBOQS_ROOT}/include
        $ENV{VCPKG_ROOT}/installed/x64-windows/include
        /usr/local/include
    )
    find_library(OQS_LIBRARY 
        NAMES oqs liboqs
        PATHS 
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/liboqs/install/lib
        ${LIBOQS_ROOT}/lib
        $ENV{LIBOQS_ROOT}/lib
        $ENV{VCPKG_ROOT}/installed/x64-windows/lib
        /usr/local/lib
    )
else()
    # Unix/Linux paths
    find_path(OQS_INCLUDE_DIR oqs/oqs.h PATHS 
        ${CMAKE_CURRENT_SOURCE_DIR}/liboqs/build/include
        /usr/local/include
        /usr/include
    )
    find_library(OQS_LIBRARY oqs PATHS 
        ${CMAKE_CURRENT_SOURCE_DIR}/liboqs/build/lib
        /usr/local/lib
        /usr/lib
    )
endif()

if(OQS_INCLUDE_DIR AND OQS_LIBRARY)
    set(OQS_FOUND TRUE)
    set(OQS_INCLUDE_DIRS ${OQS_INCLUDE_DIR})
    set(OQS_LIBRARIES ${OQS_LIBRARY})
    message(STATUS "Found liboqs: ${OQS_LIBRARY}")
    message(STATUS "liboqs include dir: ${OQS_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "liboqs not found. Please install it or build from source.
    
Windows users: 
  - Try running setup_dependencies_windows.bat or build_liboqs_windows.bat
  - Or install via vcpkg: vcpkg install liboqs:x64-windows

Linux users:
  - Install via package manager or build from source
  - Make sure liboqs is installed in a standard location")
endif()

# Dear ImGui files
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# ImGuiFileDialog files
set(IMGUI_FILE_DIALOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiFileDialog)
set(IMGUI_FILE_DIALOG_SOURCES
    ${IMGUI_FILE_DIALOG_DIR}/ImGuiFileDialog.cpp
)

# Source files
set(SOURCES
    src/main.cpp
    src/FileDropQueue.cpp
    src/AtomicFile.cpp
    src/PathSecurity.cpp
    src/TransactionalFileBatch.cpp
    src/LoginWindow.cpp
    src/WalletWindow.cpp
    src/PasswordManager.cpp
    src/FirstTimeSetupWindow.cpp
    src/CryptoArchive.cpp
    src/ArchiveWindow.cpp
    src/FontManager.cpp
    src/Settings.cpp
    src/EncryptedDatabase.cpp
    src/FormatValidation.cpp
    src/DatabaseManagerWindow.cpp
    ${IMGUI_SOURCES}
    ${IMGUI_FILE_DIALOG_SOURCES}
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})


# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMGUI_FILE_DIALOG_DIR}
    ${OQS_INCLUDE_DIRS}
    src
)

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    OpenGL::GL
    glfw
    ${OQS_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Platform-specific libraries and settings
if(WIN32)
    # Windows-specific libraries
    target_link_libraries(${PROJECT_NAME} 
        ws2_32
        crypt32
        advapi32
    )
    
    # Set Windows-specific properties
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    
    # Copy OpenSSL DLLs to output directory if using vcpkg
    if(VCPKG_TARGET_TRIPLET)
        find_file(OPENSSL_CRYPTO_DLL 
            NAMES libcrypto-3-x64.dll libcrypto-1_1-x64.dll
            PATHS $ENV{VCPKG_ROOT}/installed/x64-windows/bin
        )
        find_file(OPENSSL_SSL_DLL 
            NAMES libssl-3-x64.dll libssl-1_1-x64.dll
            PATHS $ENV{VCPKG_ROOT}/installed/x64-windows/bin
        )
        
        if(OPENSSL_CRYPTO_DLL AND OPENSSL_SSL_DLL)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${OPENSSL_CRYPTO_DLL}
                    ${OPENSSL_SSL_DLL}
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>
            )
        endif()
    endif()
endif()

# Compiler flags
if(UNIX)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
elseif(WIN32)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3)
    # Enable UTF-8 support
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(crypto_archive_security_test
        test_files/crypto_archive_security_test.cpp
        src/AtomicFile.cpp
        src/FormatValidation.cpp
        src/PathSecurity.cpp
        src/CryptoArchive.cpp
    )

    target_include_directories(crypto_archive_security_test PRIVATE src)
    target_link_libraries(crypto_archive_security_test PRIVATE OpenSSL::Crypto)

    if(UNIX)
        target_compile_options(crypto_archive_security_test PRIVATE -Wall -Wextra)
    elseif(WIN32)
        target_compile_options(crypto_archive_security_test PRIVATE /W3 /utf-8)
    endif()

    add_test(NAME crypto_archive_security COMMAND crypto_archive_security_test)

    add_executable(encrypted_database_security_test
        test_files/encrypted_database_security_test.cpp
        src/AtomicFile.cpp
        src/FormatValidation.cpp
        src/EncryptedDatabase.cpp
    )

    target_include_directories(encrypted_database_security_test PRIVATE src)
    target_link_libraries(encrypted_database_security_test PRIVATE OpenSSL::Crypto)

    if(UNIX)
        target_compile_options(encrypted_database_security_test PRIVATE -Wall -Wextra)
    elseif(WIN32)
        target_compile_options(encrypted_database_security_test PRIVATE /W3 /utf-8)
    endif()

    add_test(NAME encrypted_database_security COMMAND encrypted_database_security_test)

    add_executable(password_manager_gcm_test
        test_files/password_manager_gcm_test.cpp
        src/AtomicFile.cpp
        src/FormatValidation.cpp
        src/PathSecurity.cpp
        src/TransactionalFileBatch.cpp
        src/PasswordManager.cpp
        src/CryptoArchive.cpp
        src/EncryptedDatabase.cpp
    )

    target_include_directories(password_manager_gcm_test PRIVATE src ${OQS_INCLUDE_DIRS})
    target_link_libraries(password_manager_gcm_test PRIVATE ${OQS_LIBRARIES} OpenSSL::Crypto)

    if(UNIX)
        target_compile_options(password_manager_gcm_test PRIVATE -Wall -Wextra)
    elseif(WIN32)
        target_compile_options(password_manager_gcm_test PRIVATE /W3 /utf-8)
    endif()

    add_test(NAME password_manager_gcm COMMAND password_manager_gcm_test)

    add_executable(secure_memory_test
        test_files/secure_memory_test.cpp
    )

    target_include_directories(secure_memory_test PRIVATE src)
    target_link_libraries(secure_memory_test PRIVATE OpenSSL::Crypto)

    if(UNIX)
        target_compile_options(secure_memory_test PRIVATE -Wall -Wextra)
    elseif(WIN32)
        target_compile_options(secure_memory_test PRIVATE /W3 /utf-8)
    endif()

    add_test(NAME secure_memory COMMAND secure_memory_test)

    add_executable(atomic_file_integrity_test
        test_files/atomic_file_integrity_test.cpp
        src/AtomicFile.cpp
    )

    target_include_directories(atomic_file_integrity_test PRIVATE src)

    if(UNIX)
        target_compile_options(atomic_file_integrity_test PRIVATE -Wall -Wextra)
    elseif(WIN32)
        target_compile_options(atomic_file_integrity_test PRIVATE /W3 /utf-8)
    endif()

    add_test(NAME atomic_file_integrity COMMAND atomic_file_integrity_test)

    add_executable(master_password_transaction_test
        test_files/master_password_transaction_test.cpp
        src/AtomicFile.cpp
        src/FormatValidation.cpp
        src/PathSecurity.cpp
        src/TransactionalFileBatch.cpp
        src/PasswordManager.cpp
        src/CryptoArchive.cpp
        src/EncryptedDatabase.cpp
    )

    target_include_directories(master_password_transaction_test PRIVATE src ${OQS_INCLUDE_DIRS})
    target_link_libraries(master_password_transaction_test PRIVATE
        ${OQS_LIBRARIES}
        OpenSSL::Crypto
    )

    if(UNIX)
        target_compile_options(master_password_transaction_test PRIVATE -Wall -Wextra)
    elseif(WIN32)
        target_compile_options(master_password_transaction_test PRIVATE /W3 /utf-8)
    endif()

    add_test(NAME master_password_transaction COMMAND master_password_transaction_test)

    add_executable(database_backup_security_test
        test_files/database_backup_security_test.cpp
        src/AtomicFile.cpp
        src/FormatValidation.cpp
        src/EncryptedDatabase.cpp
    )

    target_include_directories(database_backup_security_test PRIVATE src)
    target_link_libraries(database_backup_security_test PRIVATE OpenSSL::Crypto)

    if(UNIX)
        target_compile_options(database_backup_security_test PRIVATE -Wall -Wextra)
    elseif(WIN32)
        target_compile_options(database_backup_security_test PRIVATE /W3 /utf-8)
    endif()

    add_test(NAME database_backup_security COMMAND database_backup_security_test)

    add_executable(path_validation_security_test
        test_files/path_validation_security_test.cpp
        src/AtomicFile.cpp
        src/FormatValidation.cpp
        src/PathSecurity.cpp
        src/CryptoArchive.cpp
    )

    target_include_directories(path_validation_security_test PRIVATE src)
    target_link_libraries(path_validation_security_test PRIVATE OpenSSL::Crypto)

    if(UNIX)
        target_compile_options(path_validation_security_test PRIVATE -Wall -Wextra)
    elseif(WIN32)
        target_compile_options(path_validation_security_test PRIVATE /W3 /utf-8)
    endif()

    add_test(NAME path_validation_security COMMAND path_validation_security_test)

    add_executable(archive_transaction_test
        test_files/archive_transaction_test.cpp
        src/AtomicFile.cpp
        src/FormatValidation.cpp
        src/PathSecurity.cpp
        src/CryptoArchive.cpp
    )

    target_include_directories(archive_transaction_test PRIVATE src)
    target_link_libraries(archive_transaction_test PRIVATE OpenSSL::Crypto Threads::Threads)

    if(UNIX)
        target_compile_options(archive_transaction_test PRIVATE -Wall -Wextra)
    elseif(WIN32)
        target_compile_options(archive_transaction_test PRIVATE /W3 /utf-8)
    endif()

    add_test(NAME archive_transaction COMMAND archive_transaction_test)

    add_executable(format_validation_security_test
        test_files/format_validation_security_test.cpp
        src/FormatValidation.cpp
    )
    target_include_directories(format_validation_security_test PRIVATE src)

    if(UNIX)
        target_compile_options(format_validation_security_test PRIVATE -Wall -Wextra)
    elseif(WIN32)
        target_compile_options(format_validation_security_test PRIVATE /W3 /utf-8)
    endif()

    add_test(NAME format_validation_security COMMAND format_validation_security_test)

    add_executable(archive_boundary_security_test
        test_files/archive_boundary_security_test.cpp
        src/AtomicFile.cpp
        src/FormatValidation.cpp
        src/PathSecurity.cpp
        src/CryptoArchive.cpp
    )
    target_include_directories(archive_boundary_security_test PRIVATE src)
    target_link_libraries(archive_boundary_security_test PRIVATE OpenSSL::Crypto Threads::Threads)

    if(UNIX)
        target_compile_options(archive_boundary_security_test PRIVATE -Wall -Wextra)
    elseif(WIN32)
        target_compile_options(archive_boundary_security_test PRIVATE /W3 /utf-8)
    endif()

    add_test(NAME archive_boundary_security COMMAND archive_boundary_security_test)
endif()

if(PQCWALLET_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "The fuzzing target requires Clang/libFuzzer")
    endif()

    add_executable(format_parser_fuzz
        fuzz/format_parser_fuzz.cpp
        src/FormatValidation.cpp
    )
    target_include_directories(format_parser_fuzz PRIVATE src)
    target_compile_options(format_parser_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(format_parser_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
