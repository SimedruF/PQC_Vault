#pragma once
#include "imgui.h"
#include <string>
#include <vector>
#include <map>

class FontManager {
public:
    FontManager();
    ~FontManager();
    
    // Initialize the font system
    bool Initialize();
    
    // Load a specific font
    bool LoadFont(const std::string& name, const std::string& path, float size = 16.0f);
    
    // Set the current active font
    bool SetActiveFont(const std::string& name);
    
    // Get available fonts
    std::vector<std::string> GetAvailableFonts() const;
    
    // Get current font name
    std::string GetCurrentFontName() const;
    
    // Get current font size
    float GetCurrentFontSize() const;
    
    // Change font size for current font
    bool ChangeFontSize(float newSize);
    
    // Reset to default font
    void ResetToDefault();
    
    // Check if font system is ready
    bool IsReady() const { return m_isInitialized; }
    
private:
    struct FontInfo {
        ImFont* font;
        std::string path;
        float size;
        bool isDefault;
    };
    
    std::map<std::string, FontInfo> m_fonts;
    std::string m_currentFontName;
    bool m_isInitialized;
    
    // Try to find system fonts
    std::vector<std::string> FindSystemFonts();
    
    // Load default fonts
    void LoadDefaultFonts();
};
