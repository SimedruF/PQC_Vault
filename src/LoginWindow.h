#pragma once
#include <string>
#include <vector>

class LoginWindow {
public:
    LoginWindow();
    ~LoginWindow();
    
    void Draw();
    bool IsLoginAttempted() const { return loginAttempted; }
    const std::string& GetUsername() const { return username; }
    const std::string& GetPassword() const { return password; }
    void ResetLoginAttempt() { loginAttempted = false; }
    bool IsLoginSuccessful() const { return loginSuccessful; }
    void ResetLoginStatus() { loginSuccessful = false; }
    
private:
    char usernameBuffer[256];
    char passwordBuffer[256];
    std::string username;
    std::string password;
    bool loginAttempted;
    bool loginSuccessful;
    bool showPassword;
    std::string errorMessage;
    std::vector<std::string> availableUsers;
    int selectedUser;
    
    void LoadAvailableUsers();
    void ClearBuffers();
};
