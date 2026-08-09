#include "FirstTimeSetupWindow.h"
#include "PasswordManager.h"
#include "PathSecurity.h"
#include "Settings.h"
#include "imgui.h"
#include <cstring>

FirstTimeSetupWindow::FirstTimeSetupWindow() : setupComplete(false), showPassword(false) {
    ClearBuffers();
}

FirstTimeSetupWindow::~FirstTimeSetupWindow() {
    ClearBuffers();
}

void FirstTimeSetupWindow::Draw() {
    // Center the window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Size.x * 0.5f, viewport->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_Always);
    
    if (ImGui::Begin("First Time Setup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove)) {
        
        // Get theme-appropriate colors
        Settings& settings = Settings::Instance();
        auto themeColors = settings.GetThemeColors();
        
        // Header
        ImGui::PushFont(nullptr); // Use default font, but you can load a custom one
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Welcome to PQC Wallet!").x) * 0.5f);
        ImGui::TextColored(ImVec4(themeColors.accentText[0], themeColors.accentText[1], themeColors.accentText[2], themeColors.accentText[3]), "Welcome to PQC Wallet!");
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
        std::string usernameError;
        if (usernameBuffer[0] != '\0' &&
            !PathSecurity::ValidateUsername(usernameBuffer, &usernameError)) {
            ImGui::TextColored(
                ImVec4(themeColors.errorText[0], themeColors.errorText[1],
                       themeColors.errorText[2], themeColors.errorText[3]),
                "%s", usernameError.c_str());
        }
        
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
            ImGui::TextColored(ImVec4(themeColors.errorText[0], themeColors.errorText[1], themeColors.errorText[2], themeColors.errorText[3]), "%s", errorMessage.c_str());
            ImGui::Spacing();
        }
        
        if (!successMessage.empty()) {
            ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "%s", successMessage.c_str());
            ImGui::Spacing();
        }
        
        // Real-time password validation
        if (strlen(confirmPasswordBuffer) > 0) {
            if (std::strcmp(passwordBuffer, confirmPasswordBuffer) != 0) {
                ImGui::TextColored(ImVec4(themeColors.errorText[0], themeColors.errorText[1], themeColors.errorText[2], themeColors.errorText[3]), "Passwords do not match!");
                ImGui::Spacing();
            } else {
                ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "Passwords match!");
                ImGui::Spacing();
            }
        }
        
        // Create account button
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 200) * 0.5f);
        
        bool canCreate = ValidateInput();
        if (!canCreate) {
            ImGui::BeginDisabled(true);
        }
        
        if (settings.Button("Create Account", Settings::ButtonVariant::Primary,
                            200.0f, Settings::Metrics().largeButtonHeight)) {
            if (canCreate) {
                SecureMemory::SecureString submittedPassword(passwordBuffer);
                SecureMemory::Cleanse(passwordBuffer);
                SecureMemory::Cleanse(confirmPasswordBuffer);
                PasswordManager pm;
                if (!submittedPassword.empty() &&
                    pm.CreateUser(std::string(usernameBuffer), submittedPassword.get())) {
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
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Security information
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.8f, 1.0f, 1.0f));
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("[LOCK] Your password will be protected using ML-KEM-768").x) * 0.5f);
        ImGui::Text("[LOCK] Your password will be protected using ML-KEM-768");
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Post-Quantum Cryptography Algorithm").x) * 0.5f);
        ImGui::Text("Post-Quantum Cryptography Algorithm");
        ImGui::PopStyleColor();
    }
    
    ImGui::End();
}

bool FirstTimeSetupWindow::ValidateInput() const {
    return PathSecurity::ValidateUsername(usernameBuffer) &&
           strlen(passwordBuffer) > 0 && 
           std::strcmp(passwordBuffer, confirmPasswordBuffer) == 0;
}

void FirstTimeSetupWindow::ClearBuffers() {
    memset(usernameBuffer, 0, sizeof(usernameBuffer));
    SecureMemory::Cleanse(passwordBuffer);
    SecureMemory::Cleanse(confirmPasswordBuffer);
    errorMessage.clear();
    successMessage.clear();
}
