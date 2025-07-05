cmake_minimum_required(VERSION 3.16)
project(PQCWallet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenSSL REQUIRED)

# Find liboqs - try different methods
find_path(OQS_INCLUDE_DIR oqs/oqs.h PATHS /usr/local/include)
find_library(OQS_LIBRARY oqs PATHS /usr/local/lib)

if(OQS_INCLUDE_DIR AND OQS_LIBRARY)
    set(OQS_FOUND TRUE)
    set(OQS_INCLUDE_DIRS ${OQS_INCLUDE_DIR})
    set(OQS_LIBRARIES ${OQS_LIBRARY})
    message(STATUS "Found liboqs: ${OQS_LIBRARY}")
else()
    message(FATAL_ERROR "liboqs not found. Please install it or build from source.")
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

# Compiler flags
if(UNIX)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()
