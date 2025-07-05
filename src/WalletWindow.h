#pragma once
#include <string>
#include <memory>
#include "ArchiveWindow.h"

class WalletWindow {
public:
    WalletWindow();
    ~WalletWindow();
    
    void Draw();
    void SetUserInfo(const std::string& username, const std::string& password);
    bool ShouldClose() const { return shouldClose; }
    
private:
    std::string currentUser;
    std::string userPassword;
    bool shouldClose;
    bool showSettings;
    bool showArchive;
    bool showCreateArchiveDialog;
    
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
};
