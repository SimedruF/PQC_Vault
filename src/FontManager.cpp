#include "FontManager.h"
#include <iostream>
#include <fstream>
#include <filesystem>

FontManager::FontManager() : m_currentFontName("Default"), m_isInitialized(false) {
}

FontManager::~FontManager() {
    // ImGui handles font cleanup automatically
}

bool FontManager::Initialize() {
    if (m_isInitialized) {
        return true;
    }
    
    std::cout << "Initializing Font Manager..." << std::endl;
    
    // Load default fonts first
    LoadDefaultFonts();
    
    // Try to find and load system fonts
    auto systemFonts = FindSystemFonts();
    for (const auto& fontPath : systemFonts) {
        std::string fontName = std::filesystem::path(fontPath).stem().string();
        LoadFont(fontName, fontPath, 16.0f);
    }
    
    // Try to load local fonts from fonts/ directory
    std::vector<std::string> localFonts = {
        "./fonts/DejaVuSans.ttf",
        "./fonts/Roboto-Regular.ttf",
        "./fonts/OpenSans-Regular.ttf",
        "./fonts/SourceSansPro-Regular.ttf"
    };
    
    for (const auto& fontPath : localFonts) {
        if (std::filesystem::exists(fontPath)) {
            std::string fontName = std::filesystem::path(fontPath).stem().string();
            LoadFont(fontName, fontPath, 16.0f);
        }
    }
    
    // Build the font atlas
    ImGui::GetIO().Fonts->Build();
    
    m_isInitialized = true;
    
    std::cout << "Font Manager initialized with " << m_fonts.size() << " fonts" << std::endl;
    for (const auto& [name, info] : m_fonts) {
        std::cout << "  - " << name << " (" << info.size << "px)" << std::endl;
    }
    
    return true;
}

bool FontManager::LoadFont(const std::string& name, const std::string& path, float size) {
    // Check if font file exists
    if (!std::filesystem::exists(path)) {
        std::cout << "Font file not found: " << path << std::endl;
        return false;
    }
    
    // Load the font
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), size);
    if (!font) {
        std::cout << "Failed to load font: " << path << std::endl;
        return false;
    }
    
    // Store font info
    FontInfo fontInfo;
    fontInfo.font = font;
    fontInfo.path = path;
    fontInfo.size = size;
    fontInfo.isDefault = false;
    
    m_fonts[name] = fontInfo;
    
    std::cout << "Font loaded successfully: " << name << " from " << path << std::endl;
    return true;
}

bool FontManager::SetActiveFont(const std::string& name) {
    auto it = m_fonts.find(name);
    if (it == m_fonts.end()) {
        std::cout << "Font not found: " << name << std::endl;
        return false;
    }
    
    // Set the font as current
    ImGui::GetIO().FontDefault = it->second.font;
    m_currentFontName = name;
    
    std::cout << "Active font set to: " << name << std::endl;
    return true;
}

std::vector<std::string> FontManager::GetAvailableFonts() const {
    std::vector<std::string> fontNames;
    for (const auto& [name, info] : m_fonts) {
        fontNames.push_back(name);
    }
    return fontNames;
}

std::string FontManager::GetCurrentFontName() const {
    return m_currentFontName;
}

float FontManager::GetCurrentFontSize() const {
    auto it = m_fonts.find(m_currentFontName);
    if (it != m_fonts.end()) {
        return it->second.size;
    }
    return 16.0f; // Default size
}

bool FontManager::ChangeFontSize(float newSize) {
    auto it = m_fonts.find(m_currentFontName);
    if (it == m_fonts.end()) {
        return false;
    }
    
    // Reload the font with new size
    std::string path = it->second.path;
    bool isDefault = it->second.isDefault;
    
    // Remove old font (ImGui will handle cleanup)
    m_fonts.erase(it);
    
    // Load with new size
    if (isDefault) {
        // For default font, create a new config
        ImFontConfig fontConfig;
        fontConfig.SizePixels = newSize;
        ImFont* font = ImGui::GetIO().Fonts->AddFontDefault(&fontConfig);
        
        FontInfo fontInfo;
        fontInfo.font = font;
        fontInfo.path = "default";
        fontInfo.size = newSize;
        fontInfo.isDefault = true;
        
        m_fonts[m_currentFontName] = fontInfo;
    } else {
        return LoadFont(m_currentFontName, path, newSize);
    }
    
    // Rebuild font atlas
    ImGui::GetIO().Fonts->Build();
    
    // Set as active again
    return SetActiveFont(m_currentFontName);
}

void FontManager::ResetToDefault() {
    SetActiveFont("Default");
}

std::vector<std::string> FontManager::FindSystemFonts() {
    std::vector<std::string> systemFonts;
    
    // Linux font paths
    std::vector<std::string> linuxPaths = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf"
    };
    
    // macOS font paths
    std::vector<std::string> macosPaths = {
        "/System/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Times.ttc"
    };
    
    // Windows font paths
    std::vector<std::string> windowsPaths = {
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\calibri.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf"
    };
    
    // Check all paths
    std::vector<std::string> allPaths;
    allPaths.insert(allPaths.end(), linuxPaths.begin(), linuxPaths.end());
    allPaths.insert(allPaths.end(), macosPaths.begin(), macosPaths.end());
    allPaths.insert(allPaths.end(), windowsPaths.begin(), windowsPaths.end());
    
    for (const auto& path : allPaths) {
        if (std::filesystem::exists(path)) {
            systemFonts.push_back(path);
        }
    }
    
    return systemFonts;
}

void FontManager::LoadDefaultFonts() {
    // Load ImGui's default font
    ImFont* defaultFont = ImGui::GetIO().Fonts->AddFontDefault();
    
    FontInfo fontInfo;
    fontInfo.font = defaultFont;
    fontInfo.path = "default";
    fontInfo.size = 13.0f; // ImGui default size
    fontInfo.isDefault = true;
    
    m_fonts["Default"] = fontInfo;
    
    // Load larger default font
    ImFontConfig fontConfig;
    fontConfig.SizePixels = 16.0f;
    ImFont* largeFont = ImGui::GetIO().Fonts->AddFontDefault(&fontConfig);
    
    FontInfo largeFontInfo;
    largeFontInfo.font = largeFont;
    largeFontInfo.path = "default";
    largeFontInfo.size = 16.0f;
    largeFontInfo.isDefault = true;
    
    m_fonts["Default Large"] = largeFontInfo;
}
