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
    
    // Archive management
    std::unique_ptr<ArchiveWindow> archiveWindow;
    
    void DrawMainContent();
    void DrawSettings();
};
