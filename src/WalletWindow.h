#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include "ArchiveWindow.h"
#include "FontManager.h"
#include "Settings.h"
#include "EncryptedDatabase.h"
#include "DatabaseManagerWindow.h"
#include "SecureMemory.h"

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
    SecureMemory::SecureString userPassword;
    bool shouldClose;
    bool showSettings;
    bool showArchive;
    bool showCreateArchiveDialog;
    bool showRenameArchiveDialog;
    bool showFontSettings;
    bool showChangePasswordDialog;
    bool showDatabaseManager;
    
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
    std::unordered_map<std::string, float> archiveCardHoverAnimation;
    
    // New archive creation
    char newArchiveNameBuffer[256];

    // Archive rename dialog
    char renameArchiveNameBuffer[256];
    std::string archiveBeingRenamed;
    std::string renameArchiveError;
    
    // Archive management
    std::unique_ptr<ArchiveWindow> archiveWindow;
    
    // Database management
    std::shared_ptr<EncryptedDatabase> encryptedDatabase;
    std::unique_ptr<DatabaseManagerWindow> databaseManagerWindow;
    
    // Load the list of user archives
    void LoadUserArchives();
    void OpenSelectedArchive();
    void CreateNewArchive();
    void ShowCreateArchiveDialog();
    void BeginRenameArchive(const std::string& archiveName);
    void ShowRenameArchiveDialog();
    
    void DrawSidebar();
    void DrawMainContent();
    void DrawSettings();
    void DrawFontSettings();
    void ShowChangePasswordDialog();
    void LoadSettingsToUI();
    void ClearSensitiveSession();
    void RequestLogout();
};
