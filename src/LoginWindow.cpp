#include "LoginWindow.h"
#include "PasswordManager.h"
#include "Settings.h"
#include "imgui.h"
#include <cstring>

LoginWindow::LoginWindow() : loginAttempted(false), loginSuccessful(false), showPassword(false), selectedUser(-1) {
    ClearBuffers();
    LoadAvailableUsers();
}

LoginWindow::~LoginWindow() {
    ClearBuffers();
    password.clear();
}

void LoginWindow::Draw() {
    // Center the window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Size.x * 0.5f, viewport->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    const auto& metrics = Settings::Metrics();
    ImGui::SetNextWindowSize(
        ImVec2(metrics.authWindowWidth, metrics.loginWindowHeight),
        ImGuiCond_Always);
    
    if (ImGui::Begin("PQC Wallet - Login", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        
        // Get theme-appropriate colors
        Settings& settings = Settings::Instance();
        auto themeColors = settings.GetThemeColors();
        
        // Title
        ImGui::PushFont(nullptr); // Use default font, but you can load a custom one
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Authentication").x) * 0.5f);
        ImGui::TextColored(ImVec4(themeColors.accentText[0], themeColors.accentText[1], themeColors.accentText[2], themeColors.accentText[3]), "Authentication");
        ImGui::PopFont();
        
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Username field with dropdown if users exist
        ImGui::Text("Username:");
        ImGui::SetNextItemWidth(-1);
        
        if (!availableUsers.empty()) {
            // Show dropdown with existing users
            std::string preview = (selectedUser >= 0 && selectedUser < static_cast<int>(availableUsers.size())) 
                                 ? availableUsers[selectedUser] : "Select user...";
            
            if (ImGui::BeginCombo("##username", preview.c_str())) {
                for (int i = 0; i < static_cast<int>(availableUsers.size()); ++i) {
                    bool isSelected = (selectedUser == i);
                    if (ImGui::Selectable(availableUsers[i].c_str(), isSelected)) {
                        selectedUser = i;
                        strcpy(usernameBuffer, availableUsers[i].c_str());
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        } else {
            // No users exist, show input field
            ImGui::InputText("##username", usernameBuffer, sizeof(usernameBuffer));
        }
        
        ImGui::Spacing();
        
        // Password field
        ImGui::Text("Password:");
        ImGui::SetNextItemWidth(-1);
        ImGuiInputTextFlags passwordFlags = ImGuiInputTextFlags_EnterReturnsTrue;
        if (!showPassword) {
            passwordFlags |= ImGuiInputTextFlags_Password;
        }
        const bool enterPressed = ImGui::InputText(
            "##password", passwordBuffer, sizeof(passwordBuffer), passwordFlags);
        
        // Checkbox for showing password
        ImGui::Checkbox("Show password", &showPassword);
        
        ImGui::Spacing();
        
        // Error message
        if (!errorMessage.empty()) {
            ImGui::TextColored(ImVec4(themeColors.errorText[0], themeColors.errorText[1], themeColors.errorText[2], themeColors.errorText[3]), "%s", errorMessage.c_str());
            ImGui::Spacing();
        }
        
        ImGui::Spacing();
        
        // Centered login button
        const float buttonWidth = 140.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
        const bool loginButtonPressed =
            settings.Button("Login", Settings::ButtonVariant::Primary,
                            buttonWidth, Settings::Metrics().largeButtonHeight);
        if (enterPressed || loginButtonPressed) {
            username = std::string(usernameBuffer);
            const bool passwordCaptured = password.assign(passwordBuffer);
            SecureMemory::Cleanse(passwordBuffer);
            loginAttempted = true;
            
            // Verify password using PasswordManager
            PasswordManager pm;
            if (passwordCaptured && pm.VerifyPassword(username, password.get())) {
                loginSuccessful = true;
                errorMessage.clear();
            } else {
                loginSuccessful = false;
                password.clear();
                errorMessage = "Invalid username or password!";
            }
        }
        
        ImGui::Spacing();
        
        // Status message
        if (loginAttempted && !loginSuccessful) {
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Authentication failed...").x) * 0.5f);
            ImGui::TextColored(ImVec4(themeColors.errorText[0], themeColors.errorText[1], themeColors.errorText[2], themeColors.errorText[3]), "Authentication failed...");
        } else if (loginSuccessful) {
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Login successful!").x) * 0.5f);
            ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "Login successful!");
        }
        
    }
    
    ImGui::End();
}

void LoginWindow::LoadAvailableUsers() {
    PasswordManager pm;
    availableUsers = pm.GetUsernames();
}

void LoginWindow::ClearBuffers() {
    memset(usernameBuffer, 0, sizeof(usernameBuffer));
    SecureMemory::Cleanse(passwordBuffer);
    errorMessage.clear();
}

void LoginWindow::ResetLoginStatus() {
    loginSuccessful = false;
    password.clear();
    SecureMemory::Cleanse(passwordBuffer);
}
