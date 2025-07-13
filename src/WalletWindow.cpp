#include "WalletWindow.h"
#include "imgui.h"
#include <cstring>
#include <iostream>
#include <vector>
#include <filesystem>
#include <algorithm> // for std::find

WalletWindow::WalletWindow() : shouldClose(false), showSettings(false), showArchive(false), 
                               showCreateArchiveDialog(false), selectedArchiveIndex(-1) {
    // Simplified constructor - transaction and balance related variables have been removed
    memset(newArchiveNameBuffer, 0, sizeof(newArchiveNameBuffer));
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
    
    // Load list of user archives
    LoadUserArchives();
    
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
                if (ImGui::MenuItem("Secure Archive", "Ctrl+A")) {
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
                if (ImGui::MenuItem("Settings", "Ctrl+S")) {
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
        if (ImGui::BeginChild("TopBar", ImVec2(0, 90), true)) {
            ImGui::SetCursorPosY(15);
            ImGui::Indent(20);
            
            ImGui::Text("PQC Wallet - Post-Quantum Encrypted Archive");
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);
            ImGui::Text("User: %s", currentUser.c_str());
            
            ImGui::NewLine();
            
            // Settings button în TopBar, înainte de butonul Logout
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 150);
            if (ImGui::Button("Settings", ImVec2(60, 30))) {
                showSettings = true;
            }
            
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
    
    // Create archive dialog
    if (showCreateArchiveDialog) {
        ShowCreateArchiveDialog();
    }
    
    // Archive window
    if (archiveWindow) {
        archiveWindow->Render();
    }
}

void WalletWindow::DrawMainContent() {
    // Folosim doar o singură coloană pentru interfață simplificată
    ImGui::Columns(1, "MainColumns", false);
    
    // Main title
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "PQC Secure Wallet");
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Archive section - main functionality
    ImGui::Text("Secure Archive");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.6f, 0.4f, 1.0f));
    
    if (ImGui::Button("Open Secure Archive", ImVec2(300, 50))) {
        if (archiveWindow) {
            archiveWindow->Show();
        }
    }
    
    ImGui::PopStyleColor(3);
    
    ImGui::Spacing();
    ImGui::Spacing();
    
    // User Archives List
    ImGui::Text("Your Archives");
    ImGui::Separator();
    
    if (userArchives.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No archives found for this user");
    } else {
        // Create a child window for the list with a scrollbar if needed
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 0.5f));
        if (ImGui::BeginChild("ArchivesList", ImVec2(0, 150), true)) {
            for (size_t i = 0; i < userArchives.size(); i++) {
                // Highlight the item if it's selected
                bool isSelected = (selectedArchiveIndex == static_cast<int>(i));
                
                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                }
                
                // Create a selectable item for each archive
                char label[256];
                snprintf(label, sizeof(label), "%s##%zu", userArchives[i].c_str(), i);
                
                if (ImGui::Selectable(label, isSelected)) {
                    // Set the selected archive index
                    selectedArchiveIndex = static_cast<int>(i);
                }
                
                if (isSelected) {
                    ImGui::PopStyleColor();
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    
    // Buttons for archive management
    ImGui::BeginGroup();
    if (ImGui::Button("Open Selected Archive", ImVec2(150, 30))) {
        OpenSelectedArchive();
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Create New Archive", ImVec2(150, 30))) {
        CreateNewArchive();
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Refresh Archives List", ImVec2(150, 30))) {
        LoadUserArchives();
    }
    ImGui::EndGroup();
    
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Butonul "Settings" a fost mutat în TopBar
    
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Security info
    ImGui::Text("Post-Quantum Security");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.3f, 0.3f));
    if (ImGui::BeginChild("SecurityInfo", ImVec2(0, 150), true)) {
        ImGui::SetCursorPosY(10);
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "✓ Algorithm: CRYSTALS-Kyber");
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "✓ Signature: CRYSTALS-Dilithium");
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "✓ Resistant to quantum computers");
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.5f, 1.0f), "⚠ Beta Version");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// Funcția DrawSendForm a fost eliminată deoarece nu mai este utilizată

// Funcția DrawTransactions a fost eliminată deoarece această funcționalitate nu este implementată

void WalletWindow::DrawSettings() {
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", &showSettings)) {
        ImGui::Text("Application Settings");
        ImGui::Separator();
        ImGui::Spacing();
        
        static bool enableNotifications = true;
        static bool enableAutoBackup = false;
        static int securityLevel = 2;
        
        ImGui::Checkbox("Enable notifications", &enableNotifications);
        ImGui::Checkbox("Automatic backup", &enableAutoBackup);
        
        ImGui::Spacing();
        ImGui::Text("Security level:");
        ImGui::RadioButton("Standard", &securityLevel, 1);
        ImGui::RadioButton("High", &securityLevel, 2);
        ImGui::RadioButton("Maximum", &securityLevel, 3);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Save", ImVec2(100, 30))) {
            // Save settings logic here
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Close", ImVec2(100, 30))) {
            showSettings = false;
        }
    }
    ImGui::End();
}

void WalletWindow::LoadUserArchives() {
    std::cout << "\n---------- LOAD USER ARCHIVES ----------" << std::endl;
    std::cout << "Finding archives for user: " << currentUser << std::endl;
    
    userArchives = CryptoArchive::FindUserArchives(currentUser);
    
    // Ensure "img" is always the first archive (default)
    auto it = std::find(userArchives.begin(), userArchives.end(), "img");
    if (it != userArchives.end() && it != userArchives.begin()) {
        // Remove and insert at the beginning
        std::string defaultArchive = *it;
        userArchives.erase(it);
        userArchives.insert(userArchives.begin(), defaultArchive);
        std::cout << "Moved 'img' archive to the beginning of the list" << std::endl;
    }
    
    std::cout << "Found " << userArchives.size() << " archives for user: " << currentUser << std::endl;
    for (size_t i = 0; i < userArchives.size(); i++) {
        std::cout << " [" << i << "] " << userArchives[i] << std::endl;
    }
    std::cout << "-------------------------------------\n" << std::endl;
}

void WalletWindow::OpenSelectedArchive() {
    std::cout << "\n---------- OPEN SELECTED ARCHIVE ----------" << std::endl;
    std::cout << "Selected index: " << selectedArchiveIndex << std::endl;
    std::cout << "Number of archives: " << userArchives.size() << std::endl;
    
    // Make sure we have a valid selection
    if (selectedArchiveIndex >= 0 && selectedArchiveIndex < static_cast<int>(userArchives.size())) {
        std::string selectedArchive = userArchives[selectedArchiveIndex];
        std::cout << "Opening archive: '" << selectedArchive << "' for user: " << currentUser << std::endl;
        
        // Debug: Print current archive path before loading
        std::string expectedPath = "archives/" + currentUser + "_" + selectedArchive + ".enc";
        std::cout << "Expected archive file path: " << expectedPath << std::endl;
        std::cout << "File exists: " << (std::filesystem::exists(expectedPath) ? "Yes" : "No") << std::endl;
        
        // IMPORTANT CHANGE: Instead of trying to switch archives in-place, recreate the ArchiveWindow
        // This ensures a clean state with the new archive
        
        // First destroy the old window if it exists
        if (archiveWindow) {
            std::cout << "Destroying existing archive window" << std::endl;
            archiveWindow.reset();
        }
        
        // Create a new archive window with the selected archive name
        std::cout << "Creating new archive window for archive: " << selectedArchive << std::endl;
        archiveWindow = std::make_unique<ArchiveWindow>(currentUser);
        
        // Create the CryptoArchive with the correct archive name inside ArchiveWindow
        std::cout << "Initializing archive with name: " << selectedArchive << std::endl;
        
        // Important: Load the specific archive
        bool success = archiveWindow->LoadArchive(selectedArchive, userPassword);
        
        if (success) {
            std::cout << "Successfully loaded archive: " << selectedArchive << std::endl;
            
            // Diagnose the state after loading
            std::cout << "Archive window state after loading:" << std::endl;
            archiveWindow->DiagnoseCurrentState();
            
            archiveWindow->Show();
        } else {
            std::cout << "Failed to load archive: " << selectedArchive << std::endl;
            // Try to initialize with default if loading specific archive failed
            std::cout << "Attempting to fall back to default initialization..." << std::endl;
            if (archiveWindow->Initialize(userPassword)) {
                archiveWindow->Show();
                std::cout << "Fallback to default archive successful" << std::endl;
            } else {
                std::cout << "Fallback initialization also failed!" << std::endl;
            }
        }
    } else {
        std::cout << "Invalid archive selection index: " << selectedArchiveIndex << std::endl;
    }
    std::cout << "---------------------------------\n" << std::endl;
}

void WalletWindow::ShowCreateArchiveDialog() {
    ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), 
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    
    if (ImGui::Begin("Create New Archive", &showCreateArchiveDialog, 
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        
        ImGui::TextWrapped("Create a new secure archive. Enter a unique name for the archive below:");
        ImGui::Spacing();
        
        ImGui::Text("Archive Name:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##archivename", newArchiveNameBuffer, sizeof(newArchiveNameBuffer));
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        static std::string errorMsg;
        
        // Show any error message
        if (!errorMsg.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", errorMsg.c_str());
            ImGui::Spacing();
        }
        
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 200) * 0.5f);
        
        if (ImGui::Button("Create Archive", ImVec2(200, 30))) {
            std::string archiveName(newArchiveNameBuffer);
            
            // Validare nume arhivă
            if (archiveName.empty()) {
                errorMsg = "Please enter an archive name.";
            } else if (archiveName.find_first_of("/\\:*?\"<>|") != std::string::npos) {
                errorMsg = "Archive name contains invalid characters.";
            } else {
                // Încearcă să creeze noua arhivă
                if (CryptoArchive::CreateNewArchive(currentUser, userPassword, archiveName)) {
                    errorMsg.clear();
                    memset(newArchiveNameBuffer, 0, sizeof(newArchiveNameBuffer));
                    LoadUserArchives(); // Reîncarcă lista de arhive
                    showCreateArchiveDialog = false;
                } else {
                    errorMsg = "An archive with this name already exists.";
                }
            }
        }
        
        ImGui::Spacing();
        
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 100) * 0.5f);
        if (ImGui::Button("Cancel", ImVec2(100, 25))) {
            errorMsg.clear();
            memset(newArchiveNameBuffer, 0, sizeof(newArchiveNameBuffer));
            showCreateArchiveDialog = false;
        }
    }
    
    ImGui::End();
}

void WalletWindow::CreateNewArchive() {
    showCreateArchiveDialog = true;
}
