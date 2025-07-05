#include "FirstTimeSetupWindow.h"
#include "PasswordManager.h"
#include "imgui.h"
#include <cstring>

FirstTimeSetupWindow::FirstTimeSetupWindow() : setupComplete(false), showPassword(false) {
    ClearBuffers();
}

FirstTimeSetupWindow::~FirstTimeSetupWindow() {
}

void FirstTimeSetupWindow::Draw() {
    // Center the window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Size.x * 0.5f, viewport->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 450), ImGuiCond_Always);
    
    // Style the window
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(30, 30));
    
    if (ImGui::Begin("First Time Setup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove)) {
        
        // Header
        ImGui::PushFont(nullptr); // Use default font, but you can load a custom one
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Welcome to PQC Wallet!").x) * 0.5f);
        ImGui::Text("Welcome to PQC Wallet!");
        ImGui::PopFont();
        
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();
        
        ImGui::Text("Create your secure account protected by post-quantum cryptography:");
        ImGui::Spacing();
        
        // Username field
        ImGui::Text("Username:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##username", usernameBuffer, sizeof(usernameBuffer));
        
        ImGui::Spacing();
        
        // Password field
        ImGui::Text("Password:");
        ImGui::SetNextItemWidth(-1);
        if (showPassword) {
            ImGui::InputText("##password", passwordBuffer, sizeof(passwordBuffer));
        } else {
            ImGui::InputText("##password", passwordBuffer, sizeof(passwordBuffer), ImGuiInputTextFlags_Password);
        }
        
        ImGui::Spacing();
        
        // Confirm password field
        ImGui::Text("Confirm Password:");
        ImGui::SetNextItemWidth(-1);
        if (showPassword) {
            ImGui::InputText("##confirmPassword", confirmPasswordBuffer, sizeof(confirmPasswordBuffer));
        } else {
            ImGui::InputText("##confirmPassword", confirmPasswordBuffer, sizeof(confirmPasswordBuffer), ImGuiInputTextFlags_Password);
        }
        
        // Show password checkbox
        ImGui::Spacing();
        ImGui::Checkbox("Show passwords", &showPassword);
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Validation messages
        if (!errorMessage.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", errorMessage.c_str());
            ImGui::Spacing();
        }
        
        if (!successMessage.empty()) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", successMessage.c_str());
            ImGui::Spacing();
        }
        
        // Real-time password validation
        if (strlen(confirmPasswordBuffer) > 0) {
            if (std::string(passwordBuffer) != std::string(confirmPasswordBuffer)) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Passwords do not match!");
                ImGui::Spacing();
            } else {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Passwords match!");
                ImGui::Spacing();
            }
        }
        
        // Create account button
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 200) * 0.5f);
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
        
        bool canCreate = ValidateInput();
        if (!canCreate) {
            ImGui::BeginDisabled(true);
        }
        
        if (ImGui::Button("Create Account", ImVec2(200, 40))) {
            if (canCreate) {
                PasswordManager pm;
                if (pm.CreateUser(std::string(usernameBuffer), std::string(passwordBuffer))) {
                    successMessage = "Account created successfully! You can now log in.";
                    errorMessage.clear();
                    setupComplete = true;
                } else {
                    errorMessage = "Failed to create account. Please try again.";
                    successMessage.clear();
                }
            }
        }
        
        if (!canCreate) {
            ImGui::EndDisabled();
        }
        
        ImGui::PopStyleColor(3);
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Security information
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.8f, 1.0f, 1.0f));
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("🔒 Your password will be encrypted using Kyber-768").x) * 0.5f);
        ImGui::Text("🔒 Your password will be encrypted using Kyber-768");
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Post-Quantum Cryptography Algorithm").x) * 0.5f);
        ImGui::Text("Post-Quantum Cryptography Algorithm");
        ImGui::PopStyleColor();
    }
    
    ImGui::End();
    ImGui::PopStyleVar(2);
}

bool FirstTimeSetupWindow::ValidateInput() const {
    return strlen(usernameBuffer) > 0 && 
           strlen(passwordBuffer) > 0 && 
           std::string(passwordBuffer) == std::string(confirmPasswordBuffer);
}

void FirstTimeSetupWindow::ClearBuffers() {
    memset(usernameBuffer, 0, sizeof(usernameBuffer));
    memset(passwordBuffer, 0, sizeof(passwordBuffer));
    memset(confirmPasswordBuffer, 0, sizeof(confirmPasswordBuffer));
    errorMessage.clear();
    successMessage.clear();
}
