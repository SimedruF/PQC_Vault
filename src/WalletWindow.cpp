#include "WalletWindow.h"
#include "Settings.h"
#include "PasswordManager.h"
#include "imgui.h"
#include <cstring>
#include <iostream>
#include <vector>
#include <filesystem>
#include <algorithm> // for std::find

WalletWindow::WalletWindow() : shouldClose(false), showSettings(false), showArchive(false), 
                               showCreateArchiveDialog(false), showFontSettings(false),
                               showChangePasswordDialog(false),
                               selectedArchiveIndex(-1), m_fontManager(nullptr), 
                               selectedFontIndex(0), fontSizeSlider(16.0f), 
                               showOldPassword(false), showNewPassword(false) {
    // Simplified constructor - transaction and balance related variables have been removed
    memset(newArchiveNameBuffer, 0, sizeof(newArchiveNameBuffer));
    memset(oldPasswordBuffer, 0, sizeof(oldPasswordBuffer));
    memset(newPasswordBuffer, 0, sizeof(newPasswordBuffer));
    memset(confirmPasswordBuffer, 0, sizeof(confirmPasswordBuffer));
    
    // Initialize settings UI variables
    try {
        LoadSettingsToUI();
        std::cout << "Settings initialized successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not initialize settings: " << e.what() << std::endl;
        // Use default values if settings fail to load
        tempEnableNotifications = true;
        tempEnableAutoBackup = false;
        tempSecurityLevel = 2;
        tempBackupRetentionDays = 30;
        tempEnableLogging = true;
        tempThemeIndex = 0; // Dark theme
    }
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

void WalletWindow::SetFontManager(FontManager* fontManager) {
    m_fontManager = fontManager;
    if (m_fontManager) {
        auto fonts = m_fontManager->GetAvailableFonts();
        std::string currentFont = m_fontManager->GetCurrentFontName();
        
        // Find current font index
        auto it = std::find(fonts.begin(), fonts.end(), currentFont);
        if (it != fonts.end()) {
            selectedFontIndex = std::distance(fonts.begin(), it);
        }
        
        // Set current font size
        fontSizeSlider = m_fontManager->GetCurrentFontSize();
    }
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
                if (ImGui::MenuItem("Change Password", "Ctrl+P")) {
                    showChangePasswordDialog = true;
                    // Clear password buffers when opening dialog
                    memset(oldPasswordBuffer, 0, sizeof(oldPasswordBuffer));
                    memset(newPasswordBuffer, 0, sizeof(newPasswordBuffer));
                    memset(confirmPasswordBuffer, 0, sizeof(confirmPasswordBuffer));
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
                if (ImGui::MenuItem("Font Settings", "Ctrl+F")) {
                    showFontSettings = true;
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("Settings Info")) {
                    showSettings = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("About")) {
                    // Show about dialog
                }
                ImGui::EndMenu();
            }
            
            ImGui::EndMenuBar();
        }
        
        // Top bar
        // Get theme-appropriate colors for TopBar
        Settings& topBarSettings = Settings::Instance();
        auto topBarThemeColors = topBarSettings.GetThemeColors();
        
        // Use accent color with reduced alpha for subtle background
        ImVec4 topBarBg = ImVec4(
            topBarThemeColors.accentText[0] * 0.3f,  // Darker accent color
            topBarThemeColors.accentText[1] * 0.3f,
            topBarThemeColors.accentText[2] * 0.3f,
            0.8f  // Slightly transparent
        );
        
        ImGui::PushStyleColor(ImGuiCol_ChildBg, topBarBg);
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
            Settings::PushBlackButtonText();
            if (ImGui::Button("Settings", ImVec2(60, 30))) {
                showSettings = true;
            }
            Settings::PopBlackButtonText();
            
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 80);
            Settings::PushBlackButtonText();
            if (ImGui::Button("Logout", ImVec2(60, 30))) {
                shouldClose = true;
            }
            Settings::PopBlackButtonText();
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
    
    // Font settings window
    if (showFontSettings) {
        DrawFontSettings();
    }
    
    // Create archive dialog
    if (showCreateArchiveDialog) {
        ShowCreateArchiveDialog();
    }
    
    // Change password dialog
    if (showChangePasswordDialog) {
        ShowChangePasswordDialog();
    }
    
    // Archive window
    if (archiveWindow) {
        archiveWindow->Render();
    }
}

void WalletWindow::DrawMainContent() {
    // Get theme-appropriate colors
    Settings& settings = Settings::Instance();
    auto themeColors = settings.GetThemeColors();
    
    // Folosim doar o singură coloană pentru interfață simplificată
    ImGui::Columns(1, "MainColumns", false);
    
    // Main title
    ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "PQC Secure Wallet");
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();
    
    // User Archives List
    ImGui::Text("Your Archives");
    ImGui::Separator();
    
    if (userArchives.empty()) {
        ImGui::TextColored(ImVec4(themeColors.secondaryText[0], themeColors.secondaryText[1], themeColors.secondaryText[2], themeColors.secondaryText[3]), "No archives found for this user");
    } else {
        // Create a child window for the list with a scrollbar if needed
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 0.5f));
        if (ImGui::BeginChild("ArchivesList", ImVec2(0, 150), true)) {
            for (size_t i = 0; i < userArchives.size(); i++) {
                // Highlight the item if it's selected
                bool isSelected = (selectedArchiveIndex == static_cast<int>(i));
                
                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]));
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
    Settings::PushBlackButtonText();
    if (ImGui::Button("Open Selected Archive", ImVec2(150, 30))) {
        OpenSelectedArchive();
    }
    Settings::PopBlackButtonText();
    
    ImGui::SameLine();
    
    Settings::PushBlackButtonText();
    if (ImGui::Button("Create New Archive", ImVec2(150, 30))) {
        CreateNewArchive();
    }
    Settings::PopBlackButtonText();
    
    ImGui::SameLine();
    
    Settings::PushBlackButtonText();
    if (ImGui::Button("Refresh Archives List", ImVec2(150, 30))) {
        LoadUserArchives();
    }
    Settings::PopBlackButtonText();
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
    if (ImGui::BeginChild("SecurityInfo", ImVec2(0, 220), true)) {
        ImGui::SetCursorPosY(8);
        ImGui::SetCursorPosX(10);
        
        // Usage guide section
        ImGui::TextColored(ImVec4(themeColors.accentText[0], themeColors.accentText[1], themeColors.accentText[2], themeColors.accentText[3]), "🛡️ How Your Data is Protected:");
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImVec4(themeColors.primaryText[0], themeColors.primaryText[1], themeColors.primaryText[2], themeColors.primaryText[3]), "• Login: Password protected with quantum-safe encryption");
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImVec4(themeColors.primaryText[0], themeColors.primaryText[1], themeColors.primaryText[2], themeColors.primaryText[3]), "• Files: Archives use hybrid classical + post-quantum encryption");
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImVec4(themeColors.primaryText[0], themeColors.primaryText[1], themeColors.primaryText[2], themeColors.primaryText[3]), "• Security: Multiple encryption layers protect against quantum attacks");
        
        ImGui::Spacing();
        ImGui::SetCursorPosX(10);
        
        // Algorithms section
        ImGui::TextColored(ImVec4(themeColors.accentText[0], themeColors.accentText[1], themeColors.accentText[2], themeColors.accentText[3]), "🔐 Encryption Algorithms:");
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "✓ Kyber768: Post-quantum key encapsulation (192-bit security)");
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "✓ AES-256-GCM: Authenticated encryption for passwords");
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "✓ Scrypt: Hardware-resistant key derivation (N=32768)");
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "✓ HMAC-SHA256: Data integrity verification");
        
        ImGui::Spacing();
        ImGui::SetCursorPosX(10);
        
        // Security status
        ImGui::TextColored(ImVec4(themeColors.accentText[0], themeColors.accentText[1], themeColors.accentText[2], themeColors.accentText[3]), "🚀 Security Status:");
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "✓ Quantum-resistant encryption active");
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "✓ Legacy attack tools neutralized");
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "✓ File permissions secured (600/700)");
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImVec4(themeColors.warningText[0], themeColors.warningText[1], themeColors.warningText[2], themeColors.warningText[3]), "⚠ Enhanced Security v2.0 - Production Ready");
        
        // Add a help section at the bottom
        ImGui::Spacing();
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(themeColors.infoText[0], themeColors.infoText[1], themeColors.infoText[2], themeColors.infoText[3]), "💡 Hover over algorithms for technical details");
        
        // Optional: Add tooltips for the algorithm names when hovered
        if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
            ImGui::Text("Technical Details:");
            ImGui::Separator();
            ImGui::Text("• Kyber768: Module Learning With Errors (M-LWE) problem");
            ImGui::Text("• AES-256-GCM: 256-bit key, 128-bit authentication tag");
            ImGui::Text("• Scrypt: Memory-hard function, ~32MB memory cost");
            ImGui::Text("• HMAC-SHA256: SHA-256 based message authentication");
            ImGui::EndTooltip();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// Funcția DrawSendForm a fost eliminată deoarece nu mai este utilizată

// Funcția DrawTransactions a fost eliminată deoarece această funcționalitate nu este implementată

void WalletWindow::DrawSettings() {
    ImGui::SetNextWindowSize(ImVec2(500, 450), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), 
                           ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    
    if (ImGui::Begin("Application Settings", &showSettings, ImGuiWindowFlags_NoResize)) {
        
        // Get theme-appropriate colors
        Settings& settings = Settings::Instance();
        auto themeColors = settings.GetThemeColors();
        
        ImGui::TextColored(ImVec4(themeColors.accentText[0], themeColors.accentText[1], themeColors.accentText[2], themeColors.accentText[3]), "⚙️ PQC Wallet Configuration");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Notification Settings
        ImGui::TextColored(ImVec4(themeColors.secondaryText[0], themeColors.secondaryText[1], themeColors.secondaryText[2], themeColors.secondaryText[3]), "🔔 Notifications");
        ImGui::Checkbox("Enable notifications", &tempEnableNotifications);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Show system notifications for important events");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Backup Settings
        ImGui::TextColored(ImVec4(themeColors.secondaryText[0], themeColors.secondaryText[1], themeColors.secondaryText[2], themeColors.secondaryText[3]), "💾 Backup & Recovery");
        ImGui::Checkbox("Automatic backup", &tempEnableAutoBackup);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Automatically create encrypted backups of archives");
        }
        
        ImGui::Text("Backup retention (days):");
        ImGui::SliderInt("##backupDays", &tempBackupRetentionDays, 1, 365, "%d days");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("How long to keep backup files before automatic cleanup");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Security Settings
        ImGui::TextColored(ImVec4(themeColors.secondaryText[0], themeColors.secondaryText[1], themeColors.secondaryText[2], themeColors.secondaryText[3]), "🛡️ Security Level");
        ImGui::RadioButton("Standard##security", &tempSecurityLevel, 1);
        ImGui::SameLine();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Basic post-quantum security (faster)");
        }
        
        ImGui::RadioButton("High##security", &tempSecurityLevel, 2);
        ImGui::SameLine();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enhanced security with stronger parameters (recommended)");
        }
        
        ImGui::RadioButton("Maximum##security", &tempSecurityLevel, 3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Maximum security with highest protection (slower)");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Logging Settings
        ImGui::TextColored(ImVec4(themeColors.secondaryText[0], themeColors.secondaryText[1], themeColors.secondaryText[2], themeColors.secondaryText[3]), "📝 System Logging");
        ImGui::Checkbox("Enable security logging", &tempEnableLogging);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Log security events for audit purposes");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Theme Settings
        ImGui::TextColored(ImVec4(themeColors.secondaryText[0], themeColors.secondaryText[1], themeColors.secondaryText[2], themeColors.secondaryText[3]), "🎨 Interface Theme");
        const char* themes[] = { "Dark", "Light", "Auto" };
        ImGui::Combo("Theme", &tempThemeIndex, themes, IM_ARRAYSIZE(themes));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Interface color scheme (requires restart)");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Current Status
        ImGui::TextColored(ImVec4(themeColors.infoText[0], themeColors.infoText[1], themeColors.infoText[2], themeColors.infoText[3]), "💡 Current Status:");
        ImGui::Text("Security Level: %s", 
                   tempSecurityLevel == 1 ? "Standard" : 
                   tempSecurityLevel == 2 ? "High" : "Maximum");
        ImGui::Text("Theme: %s", themes[tempThemeIndex]);
        ImGui::Text("Backups: %s", tempEnableAutoBackup ? "Enabled" : "Disabled");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Action Buttons
        float buttonWidth = 120.0f;
        float totalWidth = buttonWidth * 3 + ImGui::GetStyle().ItemSpacing.x * 2;
        float startX = (ImGui::GetWindowWidth() - totalWidth) * 0.5f;
        
        ImGui::SetCursorPosX(startX);
        
        // Save Settings Button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
        Settings::PushBlackButtonText();
        
        if (ImGui::Button("Save Settings", ImVec2(buttonWidth, 30))) {
            // Apply settings to the Settings instance
            Settings& settings = Settings::Instance();
            
            settings.SetEnableNotifications(tempEnableNotifications);
            settings.SetEnableAutoBackup(tempEnableAutoBackup);
            settings.SetSecurityLevel(tempSecurityLevel);
            settings.SetBackupRetentionDays(tempBackupRetentionDays);
            settings.SetEnableLogging(tempEnableLogging);
            
            // Convert theme index to string
            const char* themeNames[] = { "Dark", "Light", "Auto" };
            if (tempThemeIndex >= 0 && tempThemeIndex < 3) {
                settings.SetTheme(themeNames[tempThemeIndex]);
            }
            
            // Save to file
            if (settings.SaveSettings()) {
                std::cout << "Settings saved successfully!" << std::endl;
                
                // Apply theme immediately after saving and notify all components
                settings.NotifyThemeChanged();
                std::cout << "Theme applied and notifications sent!" << std::endl;
            } else {
                std::cout << "Error: Failed to save settings!" << std::endl;
            }
        }
        
        Settings::PopBlackButtonText();
        ImGui::PopStyleColor(3);
        
        ImGui::SameLine();
        
        // Reset to Defaults Button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.4f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.5f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.3f, 0.1f, 1.0f));
        Settings::PushBlackButtonText();
        
        if (ImGui::Button("Reset Defaults", ImVec2(buttonWidth, 30))) {
            Settings& settings = Settings::Instance();
            settings.ResetToDefaults();
            LoadSettingsToUI(); // Reload UI from reset settings
            std::cout << "Settings reset to defaults" << std::endl;
        }
        
        Settings::PopBlackButtonText();
        ImGui::PopStyleColor(3);
        
        ImGui::SameLine();
        
        // Close Button
        Settings::PushBlackButtonText();
        if (ImGui::Button("Close", ImVec2(buttonWidth, 30))) {
            showSettings = false;
        }
        Settings::PopBlackButtonText();
        
        ImGui::Spacing();
        
        // Help text
        ImGui::Separator();
        ImGui::TextWrapped("💡 Tip: Settings are automatically saved to config/settings.conf with restricted permissions. Some settings may require application restart to take effect.");
    }
    
    ImGui::End();
}

void WalletWindow::LoadSettingsToUI() {
    Settings& settings = Settings::Instance();
    
    tempEnableNotifications = settings.GetEnableNotifications();
    tempEnableAutoBackup = settings.GetEnableAutoBackup();
    tempSecurityLevel = settings.GetSecurityLevel();
    tempBackupRetentionDays = settings.GetBackupRetentionDays();
    tempEnableLogging = settings.GetEnableLogging();
    
    // Convert theme string to index
    std::string theme = settings.GetTheme();
    if (theme == "Dark") {
        tempThemeIndex = 0;
    } else if (theme == "Light") {
        tempThemeIndex = 1;
    } else {
        tempThemeIndex = 2; // Auto
    }
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
        
        Settings::PushBlackButtonText();
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
        Settings::PopBlackButtonText();
        
        ImGui::Spacing();
        
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 100) * 0.5f);
        Settings::PushBlackButtonText();
        if (ImGui::Button("Cancel", ImVec2(100, 25))) {
            errorMsg.clear();
            memset(newArchiveNameBuffer, 0, sizeof(newArchiveNameBuffer));
            showCreateArchiveDialog = false;
        }
        Settings::PopBlackButtonText();
    }
    
    ImGui::End();
}

void WalletWindow::CreateNewArchive() {
    showCreateArchiveDialog = true;
}

void WalletWindow::DrawFontSettings() {
    if (!m_fontManager) {
        showFontSettings = false;
        return;
    }
    
    // Center the font settings window
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), 
                           ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Font Settings", &showFontSettings, ImGuiWindowFlags_NoResize)) {
        
        // Get theme-appropriate colors
        Settings& settings = Settings::Instance();
        auto themeColors = settings.GetThemeColors();
        
        ImGui::TextColored(ImVec4(themeColors.accentText[0], themeColors.accentText[1], themeColors.accentText[2], themeColors.accentText[3]), "🔤 Font Configuration");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Font selection
        ImGui::Text("Select Font:");
        auto availableFonts = m_fontManager->GetAvailableFonts();
        
        if (!availableFonts.empty()) {
            std::vector<const char*> fontItems;
            for (const auto& font : availableFonts) {
                fontItems.push_back(font.c_str());
            }
            
            if (ImGui::Combo("##FontCombo", &selectedFontIndex, fontItems.data(), fontItems.size())) {
                if (selectedFontIndex >= 0 && selectedFontIndex < availableFonts.size()) {
                    std::string selectedFont = availableFonts[selectedFontIndex];
                    m_fontManager->SetActiveFont(selectedFont);
                    std::cout << "Font changed to: " << selectedFont << std::endl;
                }
            }
        }
        
        ImGui::Spacing();
        
        // Font size slider
        ImGui::Text("Font Size:");
        if (ImGui::SliderFloat("##FontSize", &fontSizeSlider, 8.0f, 32.0f, "%.1f px")) {
            // Update font size
            m_fontManager->ChangeFontSize(fontSizeSlider);
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Current font info
        ImGui::TextColored(ImVec4(themeColors.secondaryText[0], themeColors.secondaryText[1], themeColors.secondaryText[2], themeColors.secondaryText[3]), "Current Font:");
        ImGui::Text("Name: %s", m_fontManager->GetCurrentFontName().c_str());
        ImGui::Text("Size: %.1f px", m_fontManager->GetCurrentFontSize());
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Preview text
        ImGui::TextColored(ImVec4(themeColors.secondaryText[0], themeColors.secondaryText[1], themeColors.secondaryText[2], themeColors.secondaryText[3]), "Font Preview:");
        ImGui::BeginChild("FontPreview", ImVec2(0, 120), true);
        
        ImGui::Text("The quick brown fox jumps over the lazy dog.");
        ImGui::Text("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        ImGui::Text("abcdefghijklmnopqrstuvwxyz");
        ImGui::Text("0123456789 !@#$%%^&*()_+-=[]{}|;':\",./<>?");
        ImGui::Text("PQC Wallet - Post-Quantum Cryptography");
        ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "✓ This is how the interface text will look");
        ImGui::TextColored(ImVec4(themeColors.errorText[0], themeColors.errorText[1], themeColors.errorText[2], themeColors.errorText[3]), "⚠ Warning messages will appear like this");
        
        ImGui::EndChild();
        
        ImGui::Spacing();
        
        // Buttons
        float buttonWidth = 120.0f;
        float totalWidth = buttonWidth * 3 + ImGui::GetStyle().ItemSpacing.x * 2;
        float startX = (ImGui::GetWindowWidth() - totalWidth) * 0.5f;
        
        ImGui::SetCursorPosX(startX);
        Settings::PushBlackButtonText();
        if (ImGui::Button("Reset to Default", ImVec2(buttonWidth, 30))) {
            m_fontManager->ResetToDefault();
            // Update UI state
            auto fonts = m_fontManager->GetAvailableFonts();
            std::string currentFont = m_fontManager->GetCurrentFontName();
            auto it = std::find(fonts.begin(), fonts.end(), currentFont);
            if (it != fonts.end()) {
                selectedFontIndex = std::distance(fonts.begin(), it);
            }
            fontSizeSlider = m_fontManager->GetCurrentFontSize();
        }
        Settings::PopBlackButtonText();
        
        ImGui::SameLine();
        Settings::PushBlackButtonText();
        if (ImGui::Button("Apply", ImVec2(buttonWidth, 30))) {
            // Font changes are applied immediately
            std::cout << "Font settings applied successfully" << std::endl;
        }
        Settings::PopBlackButtonText();
        
        ImGui::SameLine();
        Settings::PushBlackButtonText();
        if (ImGui::Button("Close", ImVec2(buttonWidth, 30))) {
            showFontSettings = false;
        }
        Settings::PopBlackButtonText();
        
        ImGui::Spacing();
        
        // Help text
        ImGui::Separator();
        ImGui::TextWrapped("💡 Tip: Changes are applied immediately. If you place font files in the 'fonts/' directory, they will be automatically detected on next startup.");
    }
    
    ImGui::End();
}

void WalletWindow::ShowChangePasswordDialog() {
    ImGui::SetNextWindowSize(ImVec2(450, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), 
                           ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    
    if (ImGui::Begin("Change User Password", &showChangePasswordDialog, 
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        
        // Get fresh theme-appropriate colors for each dialog render
        Settings& dialogSettings = Settings::Instance();
        auto dialogThemeColors = dialogSettings.GetThemeColors();
        
        ImGui::TextColored(ImVec4(dialogThemeColors.warningText[0], dialogThemeColors.warningText[1], dialogThemeColors.warningText[2], dialogThemeColors.warningText[3]), "Warning: Changing your password affects ALL your archives");
        ImGui::TextWrapped("This will update the password for your user account and all associated archives. Ensure you remember your new password, as there is no recovery option if you forget it.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Old password input
        ImGui::Text("Current password:");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120);
        if (showOldPassword) {
            ImGui::InputText("##oldpass", oldPasswordBuffer, sizeof(oldPasswordBuffer));
        } else {
            ImGui::InputText("##oldpass", oldPasswordBuffer, sizeof(oldPasswordBuffer), ImGuiInputTextFlags_Password);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Show##old", &showOldPassword)) {}
        
        ImGui::Spacing();
        
        // New password input
        ImGui::Text("New password:");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120);
        if (showNewPassword) {
            ImGui::InputText("##newpass", newPasswordBuffer, sizeof(newPasswordBuffer));
        } else {
            ImGui::InputText("##newpass", newPasswordBuffer, sizeof(newPasswordBuffer), ImGuiInputTextFlags_Password);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Show##new", &showNewPassword)) {}
        
        // Confirm new password
        ImGui::Text("Confirm new password:");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120);
        if (showNewPassword) {
            ImGui::InputText("##confirmpass", confirmPasswordBuffer, sizeof(confirmPasswordBuffer));
        } else {
            ImGui::InputText("##confirmpass", confirmPasswordBuffer, sizeof(confirmPasswordBuffer), ImGuiInputTextFlags_Password);
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        static std::string errorMsg;
        
        // Show any error message
        if (!errorMsg.empty()) {
            ImGui::TextColored(ImVec4(dialogThemeColors.errorText[0], dialogThemeColors.errorText[1], dialogThemeColors.errorText[2], dialogThemeColors.errorText[3]), "%s", errorMsg.c_str());
            ImGui::Spacing();
        }
        
        // Calculate button positions for centering
        float windowWidth = ImGui::GetWindowWidth();
        float buttonsWidth = 300; // Total width of both buttons plus spacing
        float startX = (windowWidth - buttonsWidth) * 0.5f;
        
        ImGui::SetCursorPosX(startX);
        Settings::PushBlackButtonText();
        if (ImGui::Button("Change Password", ImVec2(140, 30))) {
            std::string oldPassword(oldPasswordBuffer);
            std::string newPassword(newPasswordBuffer);
            std::string confirmPassword(confirmPasswordBuffer);
            
            // Validate inputs
            if (oldPassword.empty() || newPassword.empty() || confirmPassword.empty()) {
                errorMsg = "All fields are required.";
            } else if (newPassword != confirmPassword) {
                errorMsg = "New passwords do not match.";
            } else if (newPassword.length() < 8) {
                errorMsg = "New password must be at least 8 characters.";
            } else if (oldPassword != userPassword) {
                errorMsg = "Current password is incorrect.";
            } else {
                // Change password using PasswordManager
                PasswordManager pm;
                if (pm.ChangeMasterPassword(currentUser, oldPassword, newPassword)) {
                    // Update the stored password
                    userPassword = newPassword;
                    
                    // Update password in archive window if it exists
                    if (archiveWindow) {
                        // Archive will need to be reloaded with new password
                        std::cout << "Password changed - archive will need to be reopened" << std::endl;
                    }
                    
                    errorMsg.clear();
                    
                    // Clear fields and close dialog
                    memset(oldPasswordBuffer, 0, sizeof(oldPasswordBuffer));
                    memset(newPasswordBuffer, 0, sizeof(newPasswordBuffer));
                    memset(confirmPasswordBuffer, 0, sizeof(confirmPasswordBuffer));
                    showChangePasswordDialog = false;
                    
                    // Show success message (you might want to add a status message system)
                    std::cout << "Password changed successfully for user: " << currentUser << std::endl;
                } else {
                    errorMsg = "Failed to change password. Please try again or check console for details.";
                }
            }
        }
        Settings::PopBlackButtonText();
        
        ImGui::SameLine();
        ImGui::SetCursorPosX(startX + 160);
        Settings::PushBlackButtonText();
        if (ImGui::Button("Cancel", ImVec2(140, 30))) {
            // Clear fields and error message
            memset(oldPasswordBuffer, 0, sizeof(oldPasswordBuffer));
            memset(newPasswordBuffer, 0, sizeof(newPasswordBuffer));
            memset(confirmPasswordBuffer, 0, sizeof(confirmPasswordBuffer));
            errorMsg.clear();
            showChangePasswordDialog = false;
        }
        Settings::PopBlackButtonText();
    }
    
    ImGui::End();
}
