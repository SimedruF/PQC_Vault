#include "WalletWindow.h"
#include "imgui.h"
#include <cstring>
#include <iostream>
#include <vector>

WalletWindow::WalletWindow() : shouldClose(false), showSettings(false), showArchive(false) {
    // Constructor simplificat - au fost eliminate variabilele legate de tranzacții și balanță
}

WalletWindow::~WalletWindow() {
}

void WalletWindow::SetUserInfo(const std::string& username, const std::string& password) {
    std::cout << "---------- WALLET WINDOW SET USER INFO ----------" << std::endl;
    std::cout << "Setting user info for: " << username << std::endl;
    
    currentUser = username;
    userPassword = password;
    
    // Initialize archive window
    std::cout << "Creating ArchiveWindow instance..." << std::endl;
    archiveWindow = std::make_unique<ArchiveWindow>(username);
    
    std::cout << "Initializing archive..." << std::endl;
    bool success = archiveWindow->Initialize(password);
    std::cout << "Archive initialization result: " << (success ? "Success" : "Failed") << std::endl;
    
    std::cout << "Archive loaded state: " << (archiveWindow->IsLoaded() ? "Yes" : "No") << std::endl;
    std::cout << "----------------------------------------------" << std::endl;
}

void WalletWindow::Draw() {
    // Configure window for docking compatibility
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_FirstUseEver);
    
    // Allow docking but keep window functional
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;
    
    if (ImGui::Begin("PQC Wallet", nullptr, window_flags)) {
        
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Arhiva Securizata", "Ctrl+A")) {
                    if (archiveWindow) {
                        archiveWindow->Show();
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Logout", "Ctrl+L")) {
                    shouldClose = true;
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Setari", "Ctrl+S")) {
                    showSettings = true;
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) {
                    // Show about dialog
                }
                ImGui::EndMenu();
            }
            
            ImGui::EndMenuBar();
        }
        
        // Top bar
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        if (ImGui::BeginChild("TopBar", ImVec2(0, 60), true)) {
            ImGui::SetCursorPosY(15);
            ImGui::Indent(20);
            
            ImGui::Text("PQC Wallet - Arhivă Criptată Post-Quantum");
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);
            ImGui::Text("Utilizator: %s", currentUser.c_str());
            
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 80);
            if (ImGui::Button("Logout", ImVec2(60, 30))) {
                shouldClose = true;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        
        // Main content area
        if (ImGui::BeginChild("MainContent", ImVec2(0, 0), false)) {
            DrawMainContent();
        }
        ImGui::EndChild();
    }
    ImGui::End();
    
    // Modal windows
    if (showSettings) {
        DrawSettings();
    }
    
    // Archive window
    if (archiveWindow) {
        archiveWindow->Render();
    }
}

void WalletWindow::DrawMainContent() {
    // Folosim doar o singură coloană pentru interfață simplificată
    ImGui::Columns(1, "MainColumns", false);
    
    // Titlu principal
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Portofel Securizat PQC");
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Secțiunea de arhivă - principala funcționalitate
    ImGui::Text("Arhivă Securizată");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.6f, 0.4f, 1.0f));
    
    if (ImGui::Button("Deschide Arhiva Securizată", ImVec2(300, 50))) {
        if (archiveWindow) {
            archiveWindow->Show();
        }
    }
    
    ImGui::PopStyleColor(3);
    
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Setări
    if (ImGui::Button("Setari", ImVec2(200, 40))) {
        showSettings = true;
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Security info
    ImGui::Text("Securitate Post-Quantum");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.3f, 0.3f));
    if (ImGui::BeginChild("SecurityInfo", ImVec2(0, 150), true)) {
        ImGui::SetCursorPosY(10);
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "✓ Algoritm: CRYSTALS-Kyber");
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "✓ Semnatura: CRYSTALS-Dilithium");
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "✓ Rezistent la calculatoarele cuantice");
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.5f, 1.0f), "⚠ Versiune Beta");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// Funcția DrawSendForm a fost eliminată deoarece nu mai este utilizată

// Funcția DrawTransactions a fost eliminată deoarece această funcționalitate nu este implementată

void WalletWindow::DrawSettings() {
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Setari", &showSettings)) {
        ImGui::Text("Setari aplicatie");
        ImGui::Separator();
        ImGui::Spacing();
        
        static bool enableNotifications = true;
        static bool enableAutoBackup = false;
        static int securityLevel = 2;
        
        ImGui::Checkbox("Activeaza notificarile", &enableNotifications);
        ImGui::Checkbox("Backup automat", &enableAutoBackup);
        
        ImGui::Spacing();
        ImGui::Text("Nivel securitate:");
        ImGui::RadioButton("Standard", &securityLevel, 1);
        ImGui::RadioButton("Ridicat", &securityLevel, 2);
        ImGui::RadioButton("Maxim", &securityLevel, 3);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Salveaza", ImVec2(100, 30))) {
            // Save settings logic here
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Inchide", ImVec2(100, 30))) {
            showSettings = false;
        }
    }
    ImGui::End();
}
