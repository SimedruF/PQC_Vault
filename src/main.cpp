#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <thread>
#include "LoginWindow.h"
#include "WalletWindow.h"
#include "FirstTimeSetupWindow.h"
#include "PasswordManager.h"
#include "FontManager.h"
#include "Settings.h"

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main() {
    // Setup GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return 1;
    }

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(800, 600, "PQC Wallet - Post-Quantum Cryptography Wallet", nullptr, nullptr);
    if (window == nullptr) {
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // Enable docking
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // Keep ViewportsEnable disabled for now
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Setup Dear ImGui style - Apply theme from settings
    Settings& settings = Settings::Instance();
    settings.ApplyTheme();
    
    // Additional style customization moved to ApplyTheme() method

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initialize Font Manager
    FontManager fontManager;
    if (!fontManager.Initialize()) {
        std::cerr << "Warning: Failed to initialize font manager, using default fonts" << std::endl;
    }
    
    // Set a good default font if available
    auto availableFonts = fontManager.GetAvailableFonts();
    if (!availableFonts.empty()) {
        // Prefer DejaVu Sans or other good fonts
        for (const auto& fontName : {"DejaVuSans", "Roboto-Regular", "Default Large"}) {
            if (std::find(availableFonts.begin(), availableFonts.end(), fontName) != availableFonts.end()) {
                fontManager.SetActiveFont(fontName);
                std::cout << "Set active font to: " << fontName << std::endl;
                break;
            }
        }
    }

    // Create windows
    LoginWindow loginWindow;
    WalletWindow walletWindow;
    FirstTimeSetupWindow setupWindow;
    
    // Set font manager for wallet window
    walletWindow.SetFontManager(&fontManager);
    
    // Check if this is first-time setup
    PasswordManager pm;
    bool needsSetup = !pm.HasAnyUsers();
    bool isLoggedIn = false;
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Check if theme has changed and reapply if needed
        if (settings.HasThemeChanged()) {
            printf("Theme change detected - reapplying theme\n");
            settings.ApplyTheme();
            settings.ClearThemeChanged();
        }

        // Enable docking
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        if (needsSetup) {
            // Show first-time setup
            setupWindow.Draw();
            
            if (setupWindow.IsSetupComplete()) {
                needsSetup = false;
                printf("First-time setup completed!\n");
            }
        } else if (!isLoggedIn) {
            // Draw login window
            loginWindow.Draw();
            
            // Check for login attempt
            if (loginWindow.IsLoginAttempted()) {
                printf("Login attempt:\n");
                printf("Username: %s\n", loginWindow.GetUsername().c_str());
                
                if (loginWindow.IsLoginSuccessful()) {
                    isLoggedIn = true;
                    walletWindow.SetUserInfo(loginWindow.GetUsername(), loginWindow.GetPassword());
                    printf("Login successful!\n");
                    printf("Welcome, %s!\n", loginWindow.GetUsername().c_str());
                } else {
                    printf("Login failed!\n");
                }
                
                loginWindow.ResetLoginAttempt();
                loginWindow.ResetLoginStatus();
            }
        } else {
            // Draw wallet window
            walletWindow.Draw();
            
            // Check if user wants to logout
            if (walletWindow.ShouldClose()) {
                isLoggedIn = false;
                printf("User logged out.\n");
            }
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
