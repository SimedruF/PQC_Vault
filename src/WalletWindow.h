#pragma once
#include <string>
#include <memory>
#include "ArchiveWindow.h"
#include "FontManager.h"
#include "Settings.h"

class WalletWindow {
public:
    WalletWindow();
    ~WalletWindow();
    
    void Draw();
    void SetUserInfo(const std::string& username, const std::string& password);
    void SetFontManager(FontManager* fontManager);
    bool ShouldClose() const { return shouldClose; }
    
private:
    std::string currentUser;
    std::string userPassword;
    bool shouldClose;
    bool showSettings;
    bool showArchive;
    bool showCreateArchiveDialog;
    bool showFontSettings;
    bool showChangePasswordDialog;
    
    // Change password dialog state
    char oldPasswordBuffer[256];
    char newPasswordBuffer[256];
    char confirmPasswordBuffer[256];
    bool showOldPassword;
    bool showNewPassword;
    
    // Font management
    FontManager* m_fontManager;
    int selectedFontIndex;
    float fontSizeSlider;
    
    // Settings UI state
    bool tempEnableNotifications;
    bool tempEnableAutoBackup;
    int tempSecurityLevel;
    int tempBackupRetentionDays;
    bool tempEnableLogging;
    int tempThemeIndex;
    
    // User's archives list
    std::vector<std::string> userArchives;
    int selectedArchiveIndex;
    
    // New archive creation
    char newArchiveNameBuffer[256];
    
    // Archive management
    std::unique_ptr<ArchiveWindow> archiveWindow;
    
    // Load the list of user archives
    void LoadUserArchives();
    void OpenSelectedArchive();
    void CreateNewArchive();
    void ShowCreateArchiveDialog();
    
    void DrawMainContent();
    void DrawSettings();
    void DrawFontSettings();
    void ShowChangePasswordDialog();
    void LoadSettingsToUI();
};
