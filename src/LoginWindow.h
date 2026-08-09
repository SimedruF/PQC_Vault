#pragma once
#include <string>
#include <vector>
#include "SecureMemory.h"

class LoginWindow {
public:
    LoginWindow();
    ~LoginWindow();
    
    void Draw();
    bool IsLoginAttempted() const { return loginAttempted; }
    const std::string& GetUsername() const { return username; }
    const std::string& GetPassword() const { return password.get(); }
    void ResetLoginAttempt() { loginAttempted = false; }
    bool IsLoginSuccessful() const { return loginSuccessful; }
    void ResetLoginStatus();
    
private:
    char usernameBuffer[256];
    char passwordBuffer[256];
    std::string username;
    SecureMemory::SecureString password;
    bool loginAttempted;
    bool loginSuccessful;
    bool showPassword;
    std::string errorMessage;
    std::vector<std::string> availableUsers;
    int selectedUser;
    
    void LoadAvailableUsers();
    void ClearBuffers();
};
