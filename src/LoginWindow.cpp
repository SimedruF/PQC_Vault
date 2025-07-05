#include "LoginWindow.h"
#include "PasswordManager.h"
#include "imgui.h"
#include <cstring>

LoginWindow::LoginWindow() : loginAttempted(false), loginSuccessful(false), showPassword(false), selectedUser(-1) {
    ClearBuffers();
    LoadAvailableUsers();
}

LoginWindow::~LoginWindow() {
}

void LoginWindow::Draw() {
    // Centrarea ferestrei
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Size.x * 0.5f, viewport->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    
    // Stilizarea ferestrei
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
    
    if (ImGui::Begin("PQC Wallet - Login", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        
        // Titlu
        ImGui::PushFont(nullptr); // Folosește font-ul default, dar poți încărca unul custom
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Autentificare").x) * 0.5f);
        ImGui::Text("Autentificare");
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
        
        // Checkbox pentru afișarea parolei
        ImGui::Checkbox("Show password", &showPassword);
        
        ImGui::Spacing();
        
        // Error message
        if (!errorMessage.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", errorMessage.c_str());
            ImGui::Spacing();
        }
        
        ImGui::Spacing();
        
        // Buton de login centrat
        float buttonWidth = 120.0f;
        float buttonHeight = 30.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
        
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
        
        ImGui::PopStyleColor(3);
        
        ImGui::Spacing();
        
        // Mesaj de status
        if (loginAttempted && !loginSuccessful) {
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Authentication failed...").x) * 0.5f);
            ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "Authentication failed...");
        } else if (loginSuccessful) {
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Login successful!").x) * 0.5f);
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Login successful!");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Footer with encryption info
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("🔒 Protected by Kyber Post-Quantum Cryptography").x) * 0.5f);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "🔒 Protected by Kyber Post-Quantum Cryptography");
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
