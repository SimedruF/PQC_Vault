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
}

void LoginWindow::Draw() {
    // Center the window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Size.x * 0.5f, viewport->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_FirstUseEver);
    
    // Window styling
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
    
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
        if (showPassword) {
            ImGui::InputText("##password", passwordBuffer, sizeof(passwordBuffer));
        } else {
            ImGui::InputText("##password", passwordBuffer, sizeof(passwordBuffer), ImGuiInputTextFlags_Password);
        }
        
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
        float buttonWidth = 120.0f;
        float buttonHeight = 30.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
        Settings::PushBlackButtonText();
        
        if (ImGui::Button("Login", ImVec2(buttonWidth, buttonHeight))) {
            username = std::string(usernameBuffer);
            password = std::string(passwordBuffer);
            loginAttempted = true;
            
            // Verify password using PasswordManager
            PasswordManager pm;
            if (pm.VerifyPassword(username, password)) {
                loginSuccessful = true;
                errorMessage.clear();
            } else {
                loginSuccessful = false;
                errorMessage = "Invalid username or password!";
            }
        }
        
        Settings::PopBlackButtonText();
        ImGui::PopStyleColor(3);
        
        ImGui::Spacing();
        
        // Status message
        if (loginAttempted && !loginSuccessful) {
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Authentication failed...").x) * 0.5f);
            ImGui::TextColored(ImVec4(themeColors.errorText[0], themeColors.errorText[1], themeColors.errorText[2], themeColors.errorText[3]), "Authentication failed...");
        } else if (loginSuccessful) {
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Login successful!").x) * 0.5f);
            ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "Login successful!");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Enhanced security information section
        ImGui::Spacing();
        
        // Security header
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("[SHIELD] Post-Quantum Security").x) * 0.5f);
        ImGui::TextColored(ImVec4(themeColors.accentText[0], themeColors.accentText[1], themeColors.accentText[2], themeColors.accentText[3]), "[SHIELD] Post-Quantum Security");
        
        ImGui::Spacing();
        
        // Security details in a compact format
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "[+] Kyber768: Quantum-resistant encryption");
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "[+] Scrypt: Hardware attack protection");
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "[+] AES-256-GCM: Password encryption");
        
        ImGui::Spacing();
        
        // Status indicator
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("[LOCK] Your data is protected against quantum computers").x) * 0.5f);
        ImGui::TextColored(ImVec4(themeColors.infoText[0], themeColors.infoText[1], themeColors.infoText[2], themeColors.infoText[3]), "[LOCK] Your data is protected against quantum computers");
        
        // Add tooltip for more technical details
        if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
            ImGui::Text("Security Technology Details:");
            ImGui::Separator();
            ImGui::Text("• Kyber768: NIST-approved quantum-resistant algorithm");
            ImGui::Text("• 192-bit security level against quantum attacks");
            ImGui::Text("• Multi-layer encryption protects all sensitive data");
            ImGui::Text("• Future-proof against quantum computer threats");
            ImGui::EndTooltip();
        }
    }
    
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void LoginWindow::LoadAvailableUsers() {
    PasswordManager pm;
    availableUsers = pm.GetUsernames();
}

void LoginWindow::ClearBuffers() {
    memset(usernameBuffer, 0, sizeof(usernameBuffer));
    memset(passwordBuffer, 0, sizeof(passwordBuffer));
    errorMessage.clear();
}
