cmake_minimum_required(VERSION 3.16)
project(PQCWallet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenSSL REQUIRED)

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
    src/LoginWindow.cpp
    src/WalletWindow.cpp
    src/PasswordManager.cpp
    src/FirstTimeSetupWindow.cpp
    src/CryptoArchive.cpp
    src/ArchiveWindow.cpp
    src/FontManager.cpp
    src/Settings.cpp
    src/EncryptedDatabase.cpp
    src/DatabaseManagerWindow.cpp
    ${IMGUI_SOURCES}
    ${IMGUI_FILE_DIALOG_SOURCES}
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Create test executable for encrypted database
add_executable(test_encrypted_database 
    test_encrypted_database.cpp
    src/EncryptedDatabase.cpp
)

# Include directories for test
target_include_directories(test_encrypted_database PRIVATE
    ${OQS_INCLUDE_DIRS}
    src
)

# Link libraries for test
target_link_libraries(test_encrypted_database 
    ${OQS_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Platform-specific libraries for test
if(WIN32)
    target_link_libraries(test_encrypted_database 
        ws2_32
        crypt32
        advapi32
    )
endif()

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
