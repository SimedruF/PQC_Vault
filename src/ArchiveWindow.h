#pragma once
#include <string>
#include <vector>
#include <memory>
#include <imgui.h>
#include "CryptoArchive.h"

class ArchiveWindow {
public:
    // Enum pentru tipul de previzualizare
    enum class PreviewType {
        NONE,
        TEXT,
        IMAGE
    };

    ArchiveWindow(const std::string& username);
    ~ArchiveWindow();
    
    // Render the archive window
    void Render();
    
    // Initialize archive for user
    bool Initialize(const std::string& password);
    
    // Check if archive is loaded
    bool IsLoaded() const;
    
    // Show/hide window
    void Show();
    void Hide();
    bool IsVisible() const;
    // Helper function to display file dialog with consistent size
    void drawGui();
    
    // Helper to get standard dialog size (80% of display)
    ImVec2 GetStandardDialogSize() const;
    
    // Helper to get standard dialog position (centered - 10% from edges)
    ImVec2 GetStandardDialogPosition() const;
private:
    std::string m_username;
    std::string m_password;
    std::unique_ptr<CryptoArchive> m_archive;
    bool m_isVisible;
    bool m_isLoaded;
    
    // UI state
    std::vector<FileEntry> m_fileList;
    int m_selectedFile;
    char m_filePathBuffer[512];
    char m_fileNameBuffer[256];
    char m_extractPathBuffer[512];
    bool m_showAddFileDialog;
    bool m_showExtractDialog;
    bool m_showFileViewer;
   // PreviewType m_previewType;  // Tipul de previzualizare curent
    std::vector<uint8_t> m_textPreviewData;  // Date pentru previzualizare text
    std::vector<uint8_t> m_imagePreviewData; // Date pentru previzualizare imagine
    std::string m_statusMessage;
    float m_statusMessageTime;

    // Preview data
    PreviewType m_previewType;
    std::vector<uint8_t> m_previewData;
    
    // File operations
    void RefreshFileList();
    void ShowAddFileDialog();
    void ShowExtractDialog();
    void ShowFileViewer();
    void ShowArchiveStats();
    void HandleDragDrop();

    
    // Utility functions
    std::string FormatFileSize(size_t bytes) const;
    std::string GetFileTypeIcon(const std::string& filename) const;
    void SetStatusMessage(const std::string& message, float duration = 3.0f);
    void UpdateStatusMessage();
    
    // File type detection
    bool IsImageFile(const std::string& filename) const;
    bool IsTextFile(const std::string& filename) const;
    bool IsDocumentFile(const std::string& filename) const;
    
    // Preview functionality
    void ShowFilePreview(const FileEntry& entry);
    void ShowTextPreview(const std::vector<uint8_t>& data);
    void ShowImagePreview(const std::vector<uint8_t>& data);
    
    // Helper pentru resetarea variabilelor de previzualizare
    void ResetPreview() {
        m_showFileViewer = false;
        m_previewType = PreviewType::NONE;
        m_textPreviewData.clear();
        m_imagePreviewData.clear();
    }
    
    // Helper pentru afișarea textului selectabil
    void DisplaySelectableText(const std::string& text, const ImVec2& size);
};
