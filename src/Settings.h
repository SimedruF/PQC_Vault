#pragma once
#include <string>

class Settings {
public:
    Settings();
    ~Settings();
    
    // Load settings from file
    bool LoadSettings();
    
    // Save settings to file
    bool SaveSettings();
    
    // Reset to default values
    void ResetToDefaults();
    
    // Getters
    bool GetEnableNotifications() const { return enableNotifications; }
    bool GetEnableAutoBackup() const { return enableAutoBackup; }
    int GetSecurityLevel() const { return securityLevel; }
    int GetBackupRetentionDays() const { return backupRetentionDays; }
    bool GetEnableLogging() const { return enableLogging; }
    std::string GetTheme() const { return theme; }
    
    // Setters
    void SetEnableNotifications(bool value) { enableNotifications = value; }
    void SetEnableAutoBackup(bool value) { enableAutoBackup = value; }
    void SetSecurityLevel(int value) { securityLevel = value; }
    void SetBackupRetentionDays(int value) { backupRetentionDays = value; }
    void SetEnableLogging(bool value) { enableLogging = value; }
    void SetTheme(const std::string& value) { theme = value; themeChanged = true; }
    
    // Theme application
    void ApplyTheme() const;
    
    // Force theme refresh for all UI components
    void NotifyThemeChanged() const;
    
    // Check if theme has changed and needs reapplication
    bool HasThemeChanged() const { return themeChanged; }
    void ClearThemeChanged() const { themeChanged = false; }
    
    // Theme-aware color helpers
    struct ThemeColors {
        float primaryText[4];     // Main text color
        float secondaryText[4];   // Secondary/gray text
        float accentText[4];      // Accent/colored text
        float successText[4];     // Success/green text
        float warningText[4];     // Warning/yellow text
        float errorText[4];       // Error/red text
        float infoText[4];        // Info/blue text
    };
    
    ThemeColors GetThemeColors() const;
    
    // Helper for black button text
    static void PushBlackButtonText();
    static void PopBlackButtonText();
    
    // Static instance
    static Settings& Instance();
    
private:
    // Settings values
    bool enableNotifications;
    bool enableAutoBackup;
    int securityLevel;          // 1=Standard, 2=High, 3=Maximum
    int backupRetentionDays;
    bool enableLogging;
    std::string theme;          // "Dark", "Light", "Auto"
    
    // Theme change tracking
    mutable bool themeChanged;
    
    // File management
    std::string GetSettingsFilePath() const;
    bool ParseSettingsLine(const std::string& line);
    
    // Singleton
    static Settings* instance;
};
