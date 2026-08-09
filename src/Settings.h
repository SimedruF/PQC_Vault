#pragma once
#include <string>

struct ImVec4;
struct ImVec2;

class Settings {
public:
    enum class ButtonVariant {
        Primary,
        Secondary,
        Danger,
        Ghost
    };

    enum class UiIcon {
        Info,
        Success,
        Warning,
        Error,
        Archive,
        File,
        Folder,
        Lock
    };

    struct GuiMetrics {
        float windowPadding = 20.0f;
        float framePaddingX = 12.0f;
        float framePaddingY = 8.0f;
        float itemSpacing = 10.0f;
        float itemInnerSpacing = 8.0f;
        float windowRounding = 10.0f;
        float childRounding = 8.0f;
        float frameRounding = 6.0f;
        float popupRounding = 8.0f;
        float buttonHeight = 36.0f;
        float largeButtonHeight = 42.0f;
        float authWindowWidth = 500.0f;
        float loginWindowHeight = 470.0f;
        float sidebarWidth = 184.0f;
        float archiveCardMinWidth = 210.0f;
        float archiveCardHeight = 174.0f;
    };

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
        float surface[4];         // Panels and cards
        float surfaceElevated[4]; // Elevated controls and top bars
        float border[4];          // Subtle separators and borders
    };
    
    ThemeColors GetThemeColors() const;
    static const GuiMetrics& Metrics();

    // Common button renderer. A width of zero uses the content width.
    bool Button(const char* label, ButtonVariant variant = ButtonVariant::Secondary,
                float width = 0.0f, float height = 0.0f) const;
    bool IconButton(const char* label, UiIcon icon,
                    ButtonVariant variant = ButtonVariant::Secondary,
                    float width = 0.0f, float height = 0.0f) const;

    // Small vector icons that do not depend on an external icon font.
    void DrawIcon(UiIcon icon, const ImVec4& color, float size = 20.0f,
                  bool advanceLayout = true) const;
    void DrawIconAt(UiIcon icon, const ImVec4& color,
                    const ImVec2& screenPosition, float size) const;
    void DialogHeader(UiIcon icon, const char* title, const char* subtitle = nullptr) const;
    
    // Compatibility helper for older views. New code should use Button().
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
