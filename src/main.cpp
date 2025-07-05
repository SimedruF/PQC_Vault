#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <iostream>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <thread>
#include "LoginWindow.h"
#include "WalletWindow.h"
#include "FirstTimeSetupWindow.h"
#include "PasswordManager.h"

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

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Style customization for a more beautiful interface
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    
    // Culori personalizate
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.95f);
    colors[ImGuiCol_Header] = ImVec4(0.2f, 0.6f, 0.8f, 0.8f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.7f, 0.9f, 0.9f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.1f, 0.5f, 0.7f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Create windows
    LoginWindow loginWindow;
    WalletWindow walletWindow;
    FirstTimeSetupWindow setupWindow;
    
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
