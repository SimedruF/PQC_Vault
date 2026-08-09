#include "WalletWindow.h"
#include "Settings.h"
#include "PasswordManager.h"
#include "PathSecurity.h"
#include "imgui.h"
#include <cstring>
#include <iostream>
#include <vector>
#include <filesystem>
#include <algorithm> // for std::find

WalletWindow::WalletWindow() : shouldClose(false), showSettings(false), showArchive(false), 
                               showCreateArchiveDialog(false), showRenameArchiveDialog(false),
                               showFontSettings(false),
                               showChangePasswordDialog(false), showDatabaseManager(false),
                               showOldPassword(false), showNewPassword(false),
                               m_fontManager(nullptr), selectedFontIndex(0),
                               fontSizeSlider(16.0f), selectedArchiveIndex(-1) {
    // Simplified constructor - transaction and balance related variables have been removed
    memset(newArchiveNameBuffer, 0, sizeof(newArchiveNameBuffer));
    memset(renameArchiveNameBuffer, 0, sizeof(renameArchiveNameBuffer));
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
    ClearSensitiveSession();
}

void WalletWindow::SetUserInfo(const std::string& username, const std::string& password) {
    std::cout << "---------- WALLET WINDOW SET USER INFO ----------" << std::endl;
    std::cout << "Setting user info for: " << username << std::endl;
    
    ClearSensitiveSession();
    shouldClose = false;
    std::filesystem::path databasePath;
    if (!PathSecurity::ValidateUsername(username) ||
        !PathSecurity::UserDatabasePath(username, databasePath)) {
        std::cerr << "Refusing to open a session with an unsafe username" << std::endl;
        return;
    }
    currentUser = username;
    if (!userPassword.assign(password)) {
        std::cerr << "Failed to retain the session credential securely" << std::endl;
        return;
    }
    
    // Initialize encrypted database
    std::cout << "Initializing encrypted database..." << std::endl;
    std::string db_path = databasePath.string();
    encryptedDatabase = std::make_shared<EncryptedDatabase>(db_path, userPassword.get());
    
    if (!encryptedDatabase->initialize()) {
        std::cerr << "Warning: Failed to initialize encrypted database" << std::endl;
        encryptedDatabase.reset();
        databaseManagerWindow.reset();
    } else {
        std::cout << "[OK] Encrypted database initialized successfully" << std::endl;
        // Initialize database manager window
        databaseManagerWindow = std::make_unique<DatabaseManagerWindow>(encryptedDatabase);
    }
    
    // Initialize archive window
    std::cout << "Creating ArchiveWindow instance..." << std::endl;
    archiveWindow = std::make_unique<ArchiveWindow>(username);
    
    std::cout << "Initializing archive..." << std::endl;
    bool success = archiveWindow->Initialize(userPassword.get());
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
                    SecureMemory::Cleanse(oldPasswordBuffer);
                    SecureMemory::Cleanse(newPasswordBuffer);
                    SecureMemory::Cleanse(confirmPasswordBuffer);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Logout", "Ctrl+L")) {
                    RequestLogout();
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
        
        const ImVec4 topBarBg(
            topBarThemeColors.surfaceElevated[0],
            topBarThemeColors.surfaceElevated[1],
            topBarThemeColors.surfaceElevated[2],
            topBarThemeColors.surfaceElevated[3]);
        
        ImGui::PushStyleColor(ImGuiCol_ChildBg, topBarBg);
        const auto& metrics = Settings::Metrics();
        const float topBarHeight = metrics.buttonHeight + metrics.windowPadding * 2.0f;
        if (ImGui::BeginChild("TopBar", ImVec2(0, topBarHeight), true)) {
            const float rowY = (ImGui::GetWindowHeight() - metrics.buttonHeight) * 0.5f;
            ImGui::SetCursorPosY(rowY);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("PQC Wallet");

            const float settingsWidth = 82.0f;
            const float logoutWidth = 82.0f;
            const std::string userLabel = "User: " + currentUser;
            const float rightGroupWidth = ImGui::CalcTextSize(userLabel.c_str()).x +
                settingsWidth + logoutWidth + metrics.itemSpacing * 2.0f;
            const float rightStart = ImGui::GetWindowWidth() -
                rightGroupWidth - metrics.windowPadding;
            ImGui::SameLine();
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), rightStart));
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(userLabel.c_str());
            ImGui::SameLine();

            if (topBarSettings.Button("Settings", Settings::ButtonVariant::Ghost, 82.0f)) {
                showSettings = true;
            }
            ImGui::SameLine();
            if (topBarSettings.Button("Logout", Settings::ButtonVariant::Danger, 82.0f)) {
                RequestLogout();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        
        // Main workspace: fixed navigation sidebar and responsive content.
        if (ImGui::BeginChild("Sidebar", ImVec2(metrics.sidebarWidth, 0), true)) {
            DrawSidebar();
        }
        ImGui::EndChild();

        ImGui::SameLine();
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

    if (showRenameArchiveDialog) {
        ShowRenameArchiveDialog();
    }
    
    // Change password dialog
    if (showChangePasswordDialog) {
        ShowChangePasswordDialog();
    }
    
    // Archive window
    if (archiveWindow) {
        archiveWindow->Render();
    }
    
    // Database Manager window
    if (databaseManagerWindow) {
        databaseManagerWindow->render();
    }
}

void WalletWindow::DrawSidebar() {
    Settings& settings = Settings::Instance();
    const auto themeColors = settings.GetThemeColors();
    const auto& metrics = Settings::Metrics();

    const ImVec4 accent(themeColors.accentText[0], themeColors.accentText[1],
                        themeColors.accentText[2], themeColors.accentText[3]);
    const ImVec4 success(themeColors.successText[0], themeColors.successText[1],
                         themeColors.successText[2], themeColors.successText[3]);

    ImGui::TextColored(accent, "PQC VAULT");
    ImGui::TextDisabled("Secure workspace");
    ImGui::Spacing();

    if (settings.IconButton("New archive", Settings::UiIcon::Archive,
                            Settings::ButtonVariant::Primary,
                            ImGui::GetContentRegionAvail().x)) {
        CreateNewArchive();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("NAVIGATION");

    ImGui::Selectable("Archives", true, ImGuiSelectableFlags_None,
                      ImVec2(0.0f, metrics.buttonHeight));

    if (ImGui::Selectable("Password vault", false, ImGuiSelectableFlags_None,
                          ImVec2(0.0f, metrics.buttonHeight))) {
        if (databaseManagerWindow) {
            databaseManagerWindow->setVisible(true);
        }
    }

    if (ImGui::Selectable("Settings", false, ImGuiSelectableFlags_None,
                          ImVec2(0.0f, metrics.buttonHeight))) {
        showSettings = true;
    }

    if (ImGui::Selectable("Appearance", false, ImGuiSelectableFlags_None,
                          ImVec2(0.0f, metrics.buttonHeight))) {
        showFontSettings = true;
    }

    ImGui::Spacing();
    ImGui::Separator();

    const float statusY = ImGui::GetWindowHeight() - 58.0f;
    if (ImGui::GetCursorPosY() < statusY) {
        ImGui::SetCursorPosY(statusY);
    }
    ImGui::TextColored(success, "Session protected");
    ImGui::TextDisabled("AES-256-GCM");
}

void WalletWindow::DrawMainContent() {
    Settings& settings = Settings::Instance();
    const auto themeColors = settings.GetThemeColors();
    const auto& metrics = Settings::Metrics();

    const ImVec4 accent(themeColors.accentText[0], themeColors.accentText[1],
                        themeColors.accentText[2], themeColors.accentText[3]);
    const ImVec4 secondary(themeColors.secondaryText[0], themeColors.secondaryText[1],
                           themeColors.secondaryText[2], themeColors.secondaryText[3]);

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(accent, "Your archives");

    const float actionWidth = 136.0f + 100.0f + metrics.itemSpacing;
    const bool compactHeader = ImGui::GetContentRegionAvail().x < 430.0f;
    if (!compactHeader) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
            ImGui::GetWindowWidth() - actionWidth - metrics.windowPadding));
    }

    if (settings.IconButton("New archive", Settings::UiIcon::Archive,
                            Settings::ButtonVariant::Primary, 136.0f)) {
        CreateNewArchive();
    }
    ImGui::SameLine();
    if (settings.IconButton("Refresh", Settings::UiIcon::Archive,
                            Settings::ButtonVariant::Ghost, 100.0f)) {
        LoadUserArchives();
    }

    ImGui::TextColored(secondary, "Select an archive, or double-click its card to open it.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (userArchives.empty()) {
        if (ImGui::BeginChild("EmptyArchives", ImVec2(0, 180.0f), true)) {
            const char* title = "No archives yet";
            const char* description = "Create your first encrypted archive to get started.";
            ImGui::SetCursorPosY(40.0f);
            ImGui::SetCursorPosX(std::max(metrics.windowPadding,
                (ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) * 0.5f));
            ImGui::TextUnformatted(title);
            ImGui::SetCursorPosX(std::max(metrics.windowPadding,
                (ImGui::GetWindowWidth() - ImGui::CalcTextSize(description).x) * 0.5f));
            ImGui::TextColored(secondary, "%s", description);

            const float createWidth = 148.0f;
            ImGui::SetCursorPosX(std::max(metrics.windowPadding,
                (ImGui::GetWindowWidth() - createWidth) * 0.5f));
            if (settings.Button("Create archive", Settings::ButtonVariant::Primary,
                                createWidth)) {
                CreateNewArchive();
            }
        }
        ImGui::EndChild();
        return;
    }

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int columnCount = std::max(1, static_cast<int>(
        (availableWidth + metrics.itemSpacing) /
        (metrics.archiveCardMinWidth + metrics.itemSpacing)));
    const float cardWidth = (availableWidth -
        metrics.itemSpacing * static_cast<float>(columnCount - 1)) /
        static_cast<float>(columnCount);
    const ImGuiStyle& style = ImGui::GetStyle();
    const float cardHeight = std::max(metrics.archiveCardHeight,
        style.WindowPadding.y * 2.0f + ImGui::GetTextLineHeight() * 2.0f +
        style.ItemSpacing.y * 3.0f + metrics.buttonHeight);
    int archiveToOpen = -1;

    for (size_t i = 0; i < userArchives.size(); ++i) {
        const bool selected = selectedArchiveIndex == static_cast<int>(i);
        const ImVec4 cardBackground(
            themeColors.surfaceElevated[0], themeColors.surfaceElevated[1],
            themeColors.surfaceElevated[2], themeColors.surfaceElevated[3]);
        const ImVec4 cardBorder = selected
            ? accent
            : ImVec4(themeColors.border[0], themeColors.border[1],
                     themeColors.border[2], themeColors.border[3]);

        ImGui::PushID(static_cast<int>(i));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, cardBackground);
        ImGui::PushStyleColor(ImGuiCol_Border, cardBorder);
        if (ImGui::BeginChild("ArchiveCard", ImVec2(cardWidth, cardHeight),
                              true, ImGuiWindowFlags_NoScrollbar |
                                    ImGuiWindowFlags_NoScrollWithMouse)) {
            ImGui::TextColored(secondary, "ENCRYPTED ARCHIVE");
            if (selected) {
                const char* selectedLabel = "SELECTED";
                ImGui::SameLine();
                ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                    ImGui::GetWindowWidth() - style.WindowPadding.x -
                    ImGui::CalcTextSize(selectedLabel).x));
                ImGui::TextColored(accent, "%s", selectedLabel);
            }

            ImGui::Spacing();
            ImGui::TextUnformatted(userArchives[i].c_str());
            if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
                ImGui::TextUnformatted(userArchives[i].c_str());
                ImGui::EndTooltip();
            }

            const float actionY = ImGui::GetWindowHeight() - style.WindowPadding.y -
                                  metrics.buttonHeight;
            const float separatorY = actionY - style.ItemSpacing.y;
            const ImVec2 windowPos = ImGui::GetWindowPos();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(windowPos.x + style.WindowPadding.x, windowPos.y + separatorY),
                ImVec2(windowPos.x + ImGui::GetWindowWidth() - style.WindowPadding.x,
                       windowPos.y + separatorY),
                ImGui::GetColorU32(ImGuiCol_Border));

            ImGui::SetCursorPosY(actionY);
            const float actionWidth = ImGui::GetContentRegionAvail().x;
            const float renameWidth = 100.0f;
            const float openWidth = std::max(70.0f,
                actionWidth - renameWidth - style.ItemSpacing.x);
            bool actionHovered = false;
            if (settings.IconButton("Open", Settings::UiIcon::Archive,
                                    selected ? Settings::ButtonVariant::Primary
                                             : Settings::ButtonVariant::Ghost,
                                    openWidth)) {
                selectedArchiveIndex = static_cast<int>(i);
                archiveToOpen = static_cast<int>(i);
            }
            actionHovered = ImGui::IsItemHovered();

            ImGui::SameLine();
            if (settings.IconButton("Rename", Settings::UiIcon::File,
                                    Settings::ButtonVariant::Secondary,
                                    renameWidth)) {
                selectedArchiveIndex = static_cast<int>(i);
                BeginRenameArchive(userArchives[i]);
            }
            actionHovered = actionHovered || ImGui::IsItemHovered();

            const bool cardHovered =
                ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
            float& hoverAnimation = archiveCardHoverAnimation[userArchives[i]];
            const float hoverTarget = cardHovered ? 1.0f : 0.0f;
            hoverAnimation += (hoverTarget - hoverAnimation) *
                std::min(1.0f, ImGui::GetIO().DeltaTime * 12.0f);
            if (!selected && hoverAnimation > 0.01f) {
                const ImVec2 cardPosition = ImGui::GetWindowPos();
                const ImVec2 cardSize = ImGui::GetWindowSize();
                ImVec4 animatedBorder = accent;
                animatedBorder.w *= 0.75f * hoverAnimation;
                ImGui::GetWindowDrawList()->AddRect(
                    ImVec2(cardPosition.x + 0.5f, cardPosition.y + 0.5f),
                    ImVec2(cardPosition.x + cardSize.x - 0.5f,
                           cardPosition.y + cardSize.y - 0.5f),
                    ImGui::GetColorU32(animatedBorder), metrics.childRounding,
                    0, 1.0f + hoverAnimation);
            }

            if (!actionHovered && cardHovered) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    selectedArchiveIndex = static_cast<int>(i);
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    selectedArchiveIndex = static_cast<int>(i);
                    archiveToOpen = static_cast<int>(i);
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopID();

        if ((static_cast<int>(i) + 1) % columnCount != 0 && i + 1 < userArchives.size()) {
            ImGui::SameLine();
        }
    }

    if (archiveToOpen >= 0) {
        selectedArchiveIndex = archiveToOpen;
        OpenSelectedArchive();
    }
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
        
        ImGui::TextColored(ImVec4(themeColors.accentText[0], themeColors.accentText[1], themeColors.accentText[2], themeColors.accentText[3]), "[GEAR] PQC Wallet Configuration");
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
        ImGui::TextColored(ImVec4(themeColors.secondaryText[0], themeColors.secondaryText[1], themeColors.secondaryText[2], themeColors.secondaryText[3]), "[SAVE] Backup & Recovery");
        ImGui::BeginDisabled(true);
        ImGui::Checkbox("Automatic backup (requires OS keychain)", &tempEnableAutoBackup);
        ImGui::Text("Backup retention (days):");
        ImGui::SliderInt("##backupDays", &tempBackupRetentionDays, 1, 365, "%d days");
        ImGui::EndDisabled();
        ImGui::TextDisabled("Manual encrypted database backup is available in Database Manager.");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Security Settings
        ImGui::TextColored(ImVec4(themeColors.secondaryText[0], themeColors.secondaryText[1], themeColors.secondaryText[2], themeColors.secondaryText[3]), "[SHIELD] Security Level");
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
        CryptoArchive selectedArchivePath(currentUser, selectedArchive);
        std::string expectedPath = selectedArchivePath.GetArchiveFilePath();
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
        bool success = archiveWindow->LoadArchive(selectedArchive, userPassword.get());
        
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
            if (archiveWindow->Initialize(userPassword.get())) {
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
    Settings& settings = Settings::Instance();
    const auto themeColors = settings.GetThemeColors();
    const auto& metrics = Settings::Metrics();
    static std::string errorMessage;

    ImGui::SetNextWindowSize(ImVec2(480.0f, 310.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::Begin("Create archive", &showCreateArchiveDialog,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoSavedSettings)) {
        settings.DialogHeader(Settings::UiIcon::Archive, "Create archive",
                              "Create a new authenticated encrypted container.");

        ImGui::TextUnformatted("Archive name");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }
        const bool submittedWithEnter = ImGui::InputText(
            "##archiveName", newArchiveNameBuffer, sizeof(newArchiveNameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);

        if (!errorMessage.empty()) {
            const ImVec4 errorColor(themeColors.errorText[0], themeColors.errorText[1],
                                    themeColors.errorText[2], themeColors.errorText[3]);
            ImGui::Spacing();
            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                ImVec4(errorColor.x, errorColor.y, errorColor.z, 0.08f));
            ImGui::PushStyleColor(
                ImGuiCol_Border,
                ImVec4(errorColor.x, errorColor.y, errorColor.z, 0.45f));
            if (ImGui::BeginChild("CreateArchiveError", ImVec2(0.0f, 58.0f), true,
                                  ImGuiWindowFlags_NoScrollbar)) {
                settings.DrawIcon(Settings::UiIcon::Error, errorColor, 18.0f);
                ImGui::SameLine(0.0f, metrics.itemSpacing);
                ImGui::TextWrapped("%s", errorMessage.c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
        }

        const float footerY = ImGui::GetWindowHeight() - metrics.windowPadding -
                              metrics.buttonHeight;
        if (ImGui::GetCursorPosY() < footerY) {
            ImGui::SetCursorPosY(footerY);
        }
        const float footerWidth = 100.0f + 140.0f + metrics.itemSpacing;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - footerWidth) * 0.5f);
        const bool cancelRequested =
            settings.Button("Cancel", Settings::ButtonVariant::Ghost, 100.0f);
        ImGui::SameLine();
        const bool createButtonPressed = settings.IconButton(
            "Create", Settings::UiIcon::Archive,
            Settings::ButtonVariant::Primary, 140.0f);
        const bool createRequested = submittedWithEnter || createButtonPressed;

        if (cancelRequested) {
            errorMessage.clear();
            memset(newArchiveNameBuffer, 0, sizeof(newArchiveNameBuffer));
            showCreateArchiveDialog = false;
        } else if (createRequested) {
            const std::string archiveName(newArchiveNameBuffer);
            std::string validationError;
            if (!PathSecurity::ValidateArchiveName(archiveName, &validationError)) {
                errorMessage = validationError;
            } else if (!CryptoArchive::CreateNewArchive(
                           currentUser, userPassword.get(), archiveName)) {
                errorMessage = "An archive with this name already exists.";
            } else {
                errorMessage.clear();
                memset(newArchiveNameBuffer, 0, sizeof(newArchiveNameBuffer));
                LoadUserArchives();
                const auto created =
                    std::find(userArchives.begin(), userArchives.end(), archiveName);
                selectedArchiveIndex = created == userArchives.end()
                    ? -1
                    : static_cast<int>(std::distance(userArchives.begin(), created));
                showCreateArchiveDialog = false;
            }
        }
    }
    ImGui::End();

    if (!showCreateArchiveDialog) {
        errorMessage.clear();
        memset(newArchiveNameBuffer, 0, sizeof(newArchiveNameBuffer));
    }
}
void WalletWindow::CreateNewArchive() {
    showCreateArchiveDialog = true;
}

void WalletWindow::BeginRenameArchive(const std::string& archiveName) {
    archiveBeingRenamed = archiveName;
    renameArchiveError.clear();
    memset(renameArchiveNameBuffer, 0, sizeof(renameArchiveNameBuffer));
    std::strncpy(renameArchiveNameBuffer, archiveName.c_str(),
                 sizeof(renameArchiveNameBuffer) - 1);
    showRenameArchiveDialog = true;
}

void WalletWindow::ShowRenameArchiveDialog() {
    Settings& settings = Settings::Instance();
    const auto themeColors = settings.GetThemeColors();
    const auto& metrics = Settings::Metrics();

    ImGui::SetNextWindowSize(ImVec2(480.0f, 330.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f,
               ImGui::GetIO().DisplaySize.y * 0.5f),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::Begin("Rename archive", &showRenameArchiveDialog,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        settings.DialogHeader(Settings::UiIcon::Archive, "Rename archive",
                              "The encrypted contents remain unchanged.");
        ImGui::Text("Current name: %s", archiveBeingRenamed.c_str());
        ImGui::Spacing();
        ImGui::TextUnformatted("New archive name");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }
        const bool submittedWithEnter = ImGui::InputText(
            "##renameArchiveName", renameArchiveNameBuffer,
            sizeof(renameArchiveNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

        if (!renameArchiveError.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(
                ImVec4(themeColors.errorText[0], themeColors.errorText[1],
                       themeColors.errorText[2], themeColors.errorText[3]),
                "%s", renameArchiveError.c_str());
        }

        const float buttonY = ImGui::GetWindowHeight() - metrics.windowPadding -
                              metrics.buttonHeight;
        if (ImGui::GetCursorPosY() < buttonY) {
            ImGui::SetCursorPosY(buttonY);
        }

        const float buttonGroupWidth = 120.0f + 100.0f + metrics.itemSpacing;
        ImGui::SetCursorPosX(std::max(metrics.windowPadding,
            (ImGui::GetWindowWidth() - buttonGroupWidth) * 0.5f));
        const bool renameButtonPressed = settings.IconButton(
            "Rename", Settings::UiIcon::File,
            Settings::ButtonVariant::Primary, 120.0f);
        const bool renameRequested = submittedWithEnter || renameButtonPressed;
        ImGui::SameLine();
        const bool cancelRequested =
            settings.Button("Cancel", Settings::ButtonVariant::Ghost, 100.0f);

        if (renameRequested) {
            const std::string newName(renameArchiveNameBuffer);
            const std::string previousName = archiveBeingRenamed;
            std::string validationError;
            if (!PathSecurity::ValidateArchiveName(newName, &validationError)) {
                renameArchiveError = validationError;
            } else if (CryptoArchive::RenameArchive(
                           currentUser, previousName, newName, &renameArchiveError)) {
                const bool reloadArchiveWindow = archiveWindow &&
                    archiveWindow->GetArchiveName() == previousName;
                const bool restoreVisibility = reloadArchiveWindow &&
                    archiveWindow->IsVisible();

                if (reloadArchiveWindow) {
                    archiveWindow.reset();
                    archiveWindow = std::make_unique<ArchiveWindow>(currentUser);
                    if (archiveWindow->LoadArchive(newName, userPassword.get()) &&
                        restoreVisibility) {
                        archiveWindow->Show();
                    }
                }

                LoadUserArchives();
                const auto renamed = std::find(userArchives.begin(), userArchives.end(), newName);
                selectedArchiveIndex = renamed == userArchives.end()
                    ? -1
                    : static_cast<int>(std::distance(userArchives.begin(), renamed));

                showRenameArchiveDialog = false;
                archiveBeingRenamed.clear();
                renameArchiveError.clear();
                memset(renameArchiveNameBuffer, 0, sizeof(renameArchiveNameBuffer));
            }
        } else if (cancelRequested) {
            showRenameArchiveDialog = false;
        }
    }
    ImGui::End();

    if (!showRenameArchiveDialog) {
        archiveBeingRenamed.clear();
        renameArchiveError.clear();
        memset(renameArchiveNameBuffer, 0, sizeof(renameArchiveNameBuffer));
    }
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
                if (selectedFontIndex >= 0 &&
                    selectedFontIndex < static_cast<int>(availableFonts.size())) {
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
        ImGui::TextColored(ImVec4(themeColors.errorText[0], themeColors.errorText[1], themeColors.errorText[2], themeColors.errorText[3]), "[!] Warning messages will appear like this");
        
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
    ImGui::SetNextWindowSize(ImVec2(540.0f, 480.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                           ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::Begin("Change User Password", &showChangePasswordDialog, 
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        
        // Get fresh theme-appropriate colors for each dialog render
        Settings& dialogSettings = Settings::Instance();
        auto dialogThemeColors = dialogSettings.GetThemeColors();

        dialogSettings.DialogHeader(
            Settings::UiIcon::Lock, "Change master password",
            "Re-encrypts the account, password vault, and every archive.");
        ImGui::TextColored(
            ImVec4(dialogThemeColors.warningText[0], dialogThemeColors.warningText[1],
                   dialogThemeColors.warningText[2], dialogThemeColors.warningText[3]),
            "There is no recovery option if the new password is forgotten.");
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
        if (dialogSettings.Button("Change password", Settings::ButtonVariant::Primary,
                                  140.0f)) {
            SecureMemory::SecureString oldPassword(oldPasswordBuffer);
            SecureMemory::SecureString newPassword(newPasswordBuffer);
            SecureMemory::SecureString confirmPassword(confirmPasswordBuffer);
            SecureMemory::Cleanse(oldPasswordBuffer);
            SecureMemory::Cleanse(newPasswordBuffer);
            SecureMemory::Cleanse(confirmPasswordBuffer);
            
            // Validate inputs
            if (oldPassword.empty() || newPassword.empty() || confirmPassword.empty()) {
                errorMsg = "All fields are required.";
            } else if (!newPassword.equals(confirmPassword.get())) {
                errorMsg = "New passwords do not match.";
            } else if (newPassword.size() < 8) {
                errorMsg = "New password must be at least 8 characters.";
            } else if (!userPassword.equals(oldPassword.get())) {
                errorMsg = "Current password is incorrect.";
            } else {
                PasswordManager pm;
                if (pm.ChangeMasterPassword(currentUser, oldPassword.get(),
                                            newPassword.get(), encryptedDatabase.get())) {
                        // Update the stored password
                        userPassword.assign(newPassword.get());

                        // The old archive owner still retains the previous key.
                        // Destroy it so it cannot accidentally overwrite a newly
                        // re-keyed archive. It will be reopened on demand.
                        archiveWindow.reset();
                        showArchive = false;

                        errorMsg.clear();

                        // Clear fields and close dialog
                        SecureMemory::Cleanse(oldPasswordBuffer);
                        SecureMemory::Cleanse(newPasswordBuffer);
                        SecureMemory::Cleanse(confirmPasswordBuffer);
                        showChangePasswordDialog = false;

                        std::cout << "Password changed successfully for user: "
                                  << currentUser << std::endl;
                } else {
                    errorMsg = "Password transaction failed. Existing data still uses the old password.";
                }
            }
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX(startX + 160);
        if (dialogSettings.Button("Cancel", Settings::ButtonVariant::Ghost,
                                  140.0f)) {
            // Clear fields and error message
            SecureMemory::Cleanse(oldPasswordBuffer);
            SecureMemory::Cleanse(newPasswordBuffer);
            SecureMemory::Cleanse(confirmPasswordBuffer);
            errorMsg.clear();
            showChangePasswordDialog = false;
        }
    }
    
    ImGui::End();
    if (!showChangePasswordDialog) {
        SecureMemory::Cleanse(oldPasswordBuffer);
        SecureMemory::Cleanse(newPasswordBuffer);
        SecureMemory::Cleanse(confirmPasswordBuffer);
        showOldPassword = false;
        showNewPassword = false;
    }
}

void WalletWindow::ClearSensitiveSession() {
    databaseManagerWindow.reset();
    encryptedDatabase.reset();
    archiveWindow.reset();
    userPassword.clear();
    SecureMemory::Cleanse(oldPasswordBuffer);
    SecureMemory::Cleanse(newPasswordBuffer);
    SecureMemory::Cleanse(confirmPasswordBuffer);
    memset(renameArchiveNameBuffer, 0, sizeof(renameArchiveNameBuffer));
    archiveBeingRenamed.clear();
    renameArchiveError.clear();
    archiveCardHoverAnimation.clear();
    showRenameArchiveDialog = false;
    showChangePasswordDialog = false;
    showOldPassword = false;
    showNewPassword = false;
}

void WalletWindow::RequestLogout() {
    ClearSensitiveSession();
    currentUser.clear();
    shouldClose = true;
}
