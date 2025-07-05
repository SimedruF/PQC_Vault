#pragma once
#include <string>

class FirstTimeSetupWindow {
public:
    FirstTimeSetupWindow();
    ~FirstTimeSetupWindow();
    
    void Draw();
    bool IsSetupComplete() const { return setupComplete; }
    void ResetSetup() { setupComplete = false; }
    
private:
    char usernameBuffer[256];
    char passwordBuffer[256];
    char confirmPasswordBuffer[256];
    bool setupComplete;
    bool showPassword;
    std::string errorMessage;
    std::string successMessage;
    
    bool ValidateInput() const;
    void ClearBuffers();
};
