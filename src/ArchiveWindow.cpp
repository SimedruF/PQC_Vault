#include "ArchiveWindow.h"
#include "Settings.h"
#include <imgui.h>
#include "ImGuiFileDialogConfig.h" // Include custom configuration first
#include "ImGuiFileDialog.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <iomanip>

ArchiveWindow::ArchiveWindow(const std::string& username) 
    : m_username(username), m_isVisible(false), m_isLoaded(false), m_selectedFile(-1),
      m_showAddFileDialog(false), m_showExtractDialog(false), m_showFileViewer(false),
      m_statusMessageTime(0.0f), m_previewType(PreviewType::NONE) {
    
    m_archive = std::make_unique<CryptoArchive>(username);
    
    // Clear buffers
    memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
    memset(m_fileNameBuffer, 0, sizeof(m_fileNameBuffer));
    memset(m_extractPathBuffer, 0, sizeof(m_extractPathBuffer));
    
    // Set default extract path
    std::filesystem::path defaultExtractPath = std::filesystem::current_path() / "extracted";
    std::cout << "Setting default extract path: " << defaultExtractPath.string() << std::endl;
    
    // Make sure directory exists
    try {
        std::filesystem::create_directories(defaultExtractPath);
        std::cout << "Created default extract directory" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Could not create extract directory: " << e.what() << std::endl;
    }
    
    strncpy(m_extractPathBuffer, defaultExtractPath.string().c_str(), sizeof(m_extractPathBuffer) - 1);
}

ArchiveWindow::~ArchiveWindow() {
    // Archive cleanup is handled by unique_ptr
}

void ArchiveWindow::Render() {
    if (!m_isVisible) return;
    
    UpdateStatusMessage();
    
    // Get theme-appropriate colors
    Settings& settings = Settings::Instance();
    auto themeColors = settings.GetThemeColors();
    
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Secure Archive", &m_isVisible, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Add Files", "Ctrl+A")) {
                m_showAddFileDialog = true;
            }
            if (ImGui::MenuItem("Extract Selected", "Ctrl+E", false, m_selectedFile >= 0)) {
                m_showExtractDialog = true;
            }
            if (ImGui::MenuItem("Preview Selected", "F3", false, m_selectedFile >= 0 && 
                              (IsTextFile(m_fileList[m_selectedFile].name) || 
                               IsImageFile(m_fileList[m_selectedFile].name)))) {
                ShowFilePreview(m_fileList[m_selectedFile]);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Archive", "Ctrl+S")) {
                if (m_archive->SaveArchive()) {
                    SetStatusMessage("Archive saved successfully!");
                } else {
                    SetStatusMessage("Failed to save archive!", 5.0f);
                }
            }
            if (ImGui::MenuItem("Verify Integrity", "Ctrl+V")) {
                if (m_archive->VerifyIntegrity()) {
                    SetStatusMessage("Archive integrity verified!");
                } else {
                    SetStatusMessage("Archive integrity check failed!", 5.0f);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Archive", nullptr)) {
                ImGui::OpenPopup("Reset Archive Confirmation");
            }
            if (ImGui::BeginPopupModal("Reset Archive Confirmation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextColored(ImVec4(themeColors.errorText[0], themeColors.errorText[1], themeColors.errorText[2], themeColors.errorText[3]), "WARNING: This will delete all files in the archive!");
                ImGui::Text("Are you sure you want to reset the archive?");
                ImGui::Text("This action cannot be undone.");
                ImGui::Separator();
                
                Settings::PushBlackButtonText();
                if (ImGui::Button("Yes, Reset Archive", ImVec2(180, 0))) {
                    if (m_archive->ResetArchive(m_password)) {
                        SetStatusMessage("Archive reset successfully!");
                        RefreshFileList();
                    } else {
                        SetStatusMessage("Failed to reset archive!", 5.0f);
                    }
                    ImGui::CloseCurrentPopup();
                }
                Settings::PopBlackButtonText();
                ImGui::SameLine();
                Settings::PushBlackButtonText();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                Settings::PopBlackButtonText();
                ImGui::EndPopup();
            }
            
            if (ImGui::MenuItem("Reload Archive", nullptr)) {
                std::cout << "Reloading archive..." << std::endl;
                if (m_archive->LoadArchive(m_password)) {
                    RefreshFileList();
                    SetStatusMessage("Archive reloaded successfully!");
                } else {
                    SetStatusMessage("Failed to reload archive!", 5.0f);
                }
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Refresh", "F5")) {
                RefreshFileList();
            }
            if (ImGui::MenuItem("Archive Statistics")) {
                ShowArchiveStats();
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Archive")) {
            if (ImGui::MenuItem("Reset Archive")) {
                ImGui::OpenPopup("Reset Archive?");
            }
            
            if (ImGui::MenuItem("Repair Archive")) {
                if (m_archive->RepairArchive()) {
                    SetStatusMessage("Archive repaired successfully!");
                    RefreshFileList();
                } else {
                    SetStatusMessage("Failed to repair archive!");
                }
            }
            
            if (ImGui::MenuItem("Reload Archive")) {
                ImGui::OpenPopup("Reload Archive?");
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::EndMenuBar();
    }
    
    // Status message
    if (m_statusMessageTime > 0.0f) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]));
        ImGui::Text("%s", m_statusMessage.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
    }
    
    // Main content area
    ImGui::BeginChild("MainContent", ImVec2(0, -30));
    
    // File list
    if (ImGui::BeginTable("FileList", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 240.0f);
        ImGui::TableHeadersRow();
        
        for (int i = 0; i < static_cast<int>(m_fileList.size()); ++i) {
            const FileEntry& entry = m_fileList[i];
            
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            
            // File icon and name
            ImGui::Text("%s  %s", GetFileTypeIcon(entry.name).c_str(), entry.name.c_str());
            
            // Selection
            if (ImGui::IsItemClicked()) {
                m_selectedFile = i;
            }
            
            // Double click to preview
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                ShowFilePreview(entry);
            }
            
            // Highlight on hover for a better experience
            if (ImGui::IsItemHovered() && m_selectedFile != i) {
                // Use theme-appropriate hover color
                ImU32 hover_color = ImGui::GetColorU32(ImVec4(themeColors.accentText[0], themeColors.accentText[1], themeColors.accentText[2], 0.2f));
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, hover_color);
            }
            
            // Context menu (right click)
            if (ImGui::IsItemClicked(1)) { // 1 pentru click dreapta
                m_selectedFile = i;
                ImGui::OpenPopup(("FileContextMenu_" + std::to_string(i)).c_str());
            }
            
            // Display context menu
            if (ImGui::BeginPopup(("FileContextMenu_" + std::to_string(i)).c_str())) {
                // Get fresh theme colors for menu rendering
                Settings& menuSettings = Settings::Instance();
                auto menuThemeColors = menuSettings.GetThemeColors();
                
                // Menu header with file icon
                ImGui::TextColored(ImVec4(menuThemeColors.accentText[0], menuThemeColors.accentText[1], menuThemeColors.accentText[2], menuThemeColors.accentText[3]), "%s  %s", 
                                  GetFileTypeIcon(entry.name).c_str(), entry.name.c_str());
                ImGui::Separator();
                
                // File information
                ImGui::TextDisabled("Size: %s", FormatFileSize(entry.size).c_str());
                ImGui::TextDisabled("Modified: %s", entry.timestamp.c_str());
                ImGui::Separator();
                
                // Action options
                ImGui::Text("Actions:");
                
                // Preview option
                bool canPreview = IsTextFile(entry.name) || IsImageFile(entry.name);
                if (ImGui::MenuItem("[*] Preview File", "F3", false, canPreview)) {
                    ShowFilePreview(entry);
                    ImGui::CloseCurrentPopup();
                }
                
                // If the file cannot be previewed, show the reason
                if (!canPreview) {
                    ImGui::TextDisabled("(Preview not available for this file type)");
                }
                
                ImGui::Separator();
                
                // File management options
                if (ImGui::MenuItem("[>] Extract File", "Ctrl+E")) {
                    m_selectedFile = i;
                    m_showExtractDialog = true;
                    ImGui::CloseCurrentPopup();
                }
                
                if (ImGui::MenuItem("[X] Remove File", "Delete")) {
                    if (m_archive->RemoveFile(entry.name)) {
                        RefreshFileList();
                        SetStatusMessage("File removed successfully!");
                    } else {
                        SetStatusMessage("Failed to remove file!", 5.0f);
                    }
                    ImGui::CloseCurrentPopup();
                }
                
                ImGui::EndPopup();
            }
            
            // Highlight selected row
            if (m_selectedFile == i) {
                ImU32 row_bg_color = ImGui::GetColorU32(ImGuiCol_TableRowBgAlt);
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, row_bg_color);
            }
            
            ImGui::TableSetColumnIndex(1);
            // Extract just the type without brackets for the type column
            std::string typeIcon = GetFileTypeIcon(entry.name);
            std::string typeOnly = typeIcon.substr(1, typeIcon.length() - 2); // Remove [ ]
            ImGui::Text("%s", typeOnly.c_str());
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", FormatFileSize(entry.size).c_str());
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", entry.timestamp.c_str());
            
            ImGui::TableSetColumnIndex(4);
            ImGui::PushID(i);
            // Extract button with download icon
            if (ImGui::SmallButton("[>] Extract")) {
                m_selectedFile = i;
                m_showExtractDialog = true;
            }
            ImGui::SameLine();
            // Preview button with eye icon
            if(ImGui::SmallButton("[*] Preview")) {
                ShowFilePreview(entry);
            }
            ImGui::SameLine();
            // Remove button with trash bin icon
            if (ImGui::SmallButton("[X] Remove")) {
                if (m_archive->RemoveFile(entry.name)) {
                    RefreshFileList();
                    SetStatusMessage("File removed successfully!");
                } else {
                    SetStatusMessage("Failed to remove file!", 5.0f);
                }
            }
            ImGui::PopID();
        }
        
        ImGui::EndTable();
        
        // Context menu for empty area in the table
        if (ImGui::IsMouseClicked(1) && ImGui::IsWindowHovered() && m_fileList.size() > 0) {
            ImGui::OpenPopup("TableContextMenu");
        }
        
        // Display context menu for empty area in the table
        if (ImGui::BeginPopup("TableContextMenu")) {
            ImGui::Text("Archive Actions");
            ImGui::Separator();
            
            if (ImGui::MenuItem("Add Files", "Ctrl+A")) {
                m_showAddFileDialog = true;
                ImGui::CloseCurrentPopup();
            }
            
            if (ImGui::MenuItem("Refresh List", "F5")) {
                RefreshFileList();
                ImGui::CloseCurrentPopup();
            }
            
            if (ImGui::MenuItem("Save Archive", "Ctrl+S")) {
                if (m_archive->SaveArchive()) {
                    SetStatusMessage("Archive saved successfully!");
                } else {
                    SetStatusMessage("Failed to save archive!", 5.0f);
                }
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
    }
    
    ImGui::EndChild();
    
    // Bottom toolbar
    ImGui::Separator();
    Settings::PushBlackButtonText();
    if (ImGui::Button("[+] Add Files")) {
        m_showAddFileDialog = true;
    }
    Settings::PopBlackButtonText();
    ImGui::SameLine();
    Settings::PushBlackButtonText();
    if (ImGui::Button("[>] Extract Selected") && m_selectedFile >= 0) {
        m_showExtractDialog = true;
    }
    Settings::PopBlackButtonText();
    ImGui::SameLine();
    Settings::PushBlackButtonText();
    if (ImGui::Button("[R] Refresh")) {
        RefreshFileList();
    }
    Settings::PopBlackButtonText();
    
    ImGui::SameLine();
    auto stats = m_archive->GetStats();
    ImGui::Text("Files: %zu | Total Size: %s", stats.totalFiles, FormatFileSize(stats.totalSize).c_str());
    
    // Process keyboard shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_F3) && m_selectedFile >= 0) {
        // Check if the selected file can be previewed
        if (m_selectedFile < static_cast<int>(m_fileList.size())) {
            const FileEntry& entry = m_fileList[m_selectedFile];
            if (IsTextFile(entry.name) || IsImageFile(entry.name)) {
                ShowFilePreview(entry);
            } else {
                SetStatusMessage("This file type cannot be previewed!", 3.0f);
            }
        }
    }
    
    // Handle dialogs
    if (m_showAddFileDialog) {
        ShowAddFileDialog();
    }
    
    if (m_showExtractDialog) {
        ShowExtractDialog();
    }
    
    if (m_showFileViewer) {
        ShowFileViewer();
    }
    
    ImGui::End();
}

bool ArchiveWindow::Initialize(const std::string& password) {
    std::cout << "---------- ARCHIVE WINDOW INITIALIZE ----------" << std::endl;
    std::cout << "Initializing archive for user: " << m_username << std::endl;
    
    m_password = password;
    
    bool success = false;
    bool tryRecreate = false;
    
    // First try to load if archive exists
    if (m_archive->ArchiveExists()) {
        std::cout << "Archive exists, loading..." << std::endl;
        success = m_archive->LoadArchive(password);
        
        if (!success) {
            std::cout << "Loading failed, archive might be corrupted. Will try to recreate..." << std::endl;
            tryRecreate = true;
        }
    } else {
        std::cout << "Archive does not exist, creating new..." << std::endl;
        tryRecreate = true;
    }
    
    // If loading failed or archive doesn't exist, try to create a new one
    if (tryRecreate) {
        // If file exists but is corrupted, delete it first
        if (m_archive->ArchiveExists()) {
            std::cout << "Removing corrupted archive file..." << std::endl;
            std::string archivePath = "archives/" + m_username + "_img.enc";
            std::filesystem::remove(archivePath);
        }
        
        std::cout << "Creating new archive..." << std::endl;
        success = m_archive->InitializeArchive(password);
    }
    
    std::cout << "Archive initialization result: " << (success ? "Success" : "Failed") << std::endl;
    
    if (success) {
        m_isLoaded = true;
        std::cout << "m_isLoaded set to true" << std::endl;
        
        // Run diagnostic to check archive state
        m_archive->DiagnoseArchive();
        
        // Refresh the file list
        RefreshFileList();
        
        if (tryRecreate) {
            SetStatusMessage("Created new archive successfully!");
        } else {
            SetStatusMessage("Archive loaded successfully!");
        }
    } else {
        std::cout << "m_isLoaded remains false" << std::endl;
        SetStatusMessage("Failed to initialize archive!", 5.0f);
    }
    
    std::cout << "Archive loaded state: " << (m_isLoaded ? "Yes" : "No") << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    
    return success;
}

bool ArchiveWindow::IsLoaded() const {
    return m_isLoaded;
}

void ArchiveWindow::Show() {
    m_isVisible = true;
}

void ArchiveWindow::Hide() {
    m_isVisible = false;
}

bool ArchiveWindow::IsVisible() const {
    return m_isVisible;
}

void ArchiveWindow::RefreshFileList() {
    std::cout << "\n---------- REFRESH FILE LIST ----------" << std::endl;
    std::cout << "Archive loaded status: " << (m_isLoaded ? "Yes" : "No") << std::endl;
    
    if (!m_isLoaded) {
        std::cout << "Cannot refresh file list - archive not loaded!" << std::endl;
        std::cout << "--------------------------------------\n" << std::endl;
        return;
    }
    
    // Run diagnostic to check archive state
    m_archive->DiagnoseArchive();
    
    // Attempt to repair any issues
    m_archive->RepairArchive();
    
    // Get the file list
    m_fileList = m_archive->GetFileList();
    
    std::cout << "Retrieved " << m_fileList.size() << " files from archive" << std::endl;
    
    // Check each entry in the list for validity and remove invalid entries
    bool allEntriesValid = true;
    std::vector<FileEntry> validEntries;
    validEntries.reserve(m_fileList.size());
    
    // Detailed diagnostic of all files
    std::cout << "\n=== DETAILED FILE DIAGNOSTICS ===\n" << std::endl;
    std::cout << "Total files in list: " << m_fileList.size() << std::endl;
    
    for (size_t i = 0; i < m_fileList.size(); i++) {
        const auto& entry = m_fileList[i];
        std::cout << "\nFile #" << i << " diagnostics:" << std::endl;
        std::cout << "  Name: '" << entry.name << "'" << std::endl;
        std::cout << "  Path: '" << entry.path << "'" << std::endl;
        std::cout << "  Size field: " << entry.size << " bytes" << std::endl;
        std::cout << "  Data vector size: " << entry.data.size() << " bytes" << std::endl;
        std::cout << "  Timestamp: " << entry.timestamp << std::endl;
        std::cout << "  Hash: " << entry.hash << std::endl;
        
        bool isEntryValid = true;
        
        if (entry.name.empty()) {
            std::cout << "  WARNING: Entry has empty name!" << std::endl;
            isEntryValid = false;
            allEntriesValid = false;
        }
        
        if (entry.data.empty()) {
            std::cout << "  WARNING: File '" << entry.name << "' has empty data!" << std::endl;
            isEntryValid = false;
            allEntriesValid = false;
        }
        
        if (entry.size == 0) {
            std::cout << "  WARNING: File '" << entry.name << "' has zero size!" << std::endl;
            isEntryValid = false;
            allEntriesValid = false;
        }
        
        if (entry.data.size() != entry.size) {
            std::cout << "  WARNING: File '" << entry.name << "' has size mismatch! " 
                      << "Reported: " << entry.size << ", Actual: " << entry.data.size() << " bytes" << std::endl;
        }
        
        // Display validity decision
        std::cout << "  Entry is " << (isEntryValid ? "VALID" : "INVALID") << std::endl;
        
        // Add entry to valid list only if it passes all checks
        if (isEntryValid) {
            validEntries.push_back(entry);
            std::cout << "  Added to valid entries list" << std::endl;
        } else {
            std::cout << "  Skipping invalid entry: '" << entry.name << "'" << std::endl;
        }
    }
    
    std::cout << "\n=== VALIDATION SUMMARY ===\n" << std::endl;
    std::cout << "Original files count: " << m_fileList.size() << std::endl;
    std::cout << "Valid files count: " << validEntries.size() << std::endl;
    std::cout << "Invalid files count: " << (m_fileList.size() - validEntries.size()) << std::endl;
    
    if (!allEntriesValid) {
        std::cout << "Some files in the archive list have invalid data! This may cause problems with previewing or extracting." << std::endl;
        std::cout << "Filtered out invalid entries. Original count: " << m_fileList.size() 
                  << ", Valid count: " << validEntries.size() << std::endl;
        
        // Update file list with only valid entries
        m_fileList = std::move(validEntries);
    }
    
    // To solve the problem with files not being displayed, we can add an option to force display all files,
    // even those considered invalid. Normally, this flag should be user configurable,
    // but for diagnostics, we can activate it here.
    bool showAllFiles = true; // Set to true to show all files regardless of validity
    
    if (showAllFiles && !allEntriesValid) {
        std::cout << "\nFORCING DISPLAY OF ALL FILES REGARDLESS OF VALIDITY\n" << std::endl;
        // Reset the list to include all files
        m_fileList = m_archive->GetFileList();
    }
    
    // Sort by name
    std::sort(m_fileList.begin(), m_fileList.end(), 
              [](const FileEntry& a, const FileEntry& b) {
                  return a.name < b.name;
              });
              
    // Log the sorted file list
    std::cout << "\n=== FINAL FILE LIST ===\n" << std::endl;
    for (size_t i = 0; i < m_fileList.size(); i++) {
        std::cout << "[" << i << "] " << m_fileList[i].name 
                  << " (" << m_fileList[i].size << " bytes, data size: " << m_fileList[i].data.size() << " bytes)" << std::endl;
    }
    
    m_selectedFile = -1;  // Reset selection to avoid out-of-bounds access
    std::cout << "Selection reset" << std::endl;
    std::cout << "--------------------------------------\n" << std::endl;
}
// Helper function for displaying file dialogs with a consistent size
void ArchiveWindow::drawGui() { 
  // open Dialog Simple
  if (ImGui::Button("Open File Dialog")) {
    IGFD::FileDialogConfig config;
    config.path = ".";
    // Add flags to prevent flickering
    config.flags = ImGuiFileDialogFlags_Modal;
    ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".cpp,.h,.hpp", config);
  }
  
  // Get standard dimensions for dialog
  ImVec2 dialogSize = GetStandardDialogSize();
  ImVec2 dialogPos = GetStandardDialogPosition();
  
  // Set the position and size of the window before displaying it
  if (ImGuiFileDialog::Instance()->IsOpened("ChooseFileDlgKey")) {
    ImGui::SetNextWindowPos(dialogPos);
    ImGui::SetNextWindowSize(dialogSize);
  }
  
  // Afișăm dialogul cu flag-uri care previn flickering-ul
  if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", 
                                          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize, 
                                          dialogSize, 
                                          dialogPos)) {
    if (ImGuiFileDialog::Instance()->IsOk()) { // action if OK
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
      std::cout << "Selected file: " << filePathName << std::endl;
      // action
    }
    ImGuiFileDialog::Instance()->Close();
  }
}

void ArchiveWindow::ShowAddFileDialog() {
    static std::string selectedFile = "";
    ImGui::OpenPopup("Add Files to Archive");
    
    if (ImGui::BeginPopupModal("Add Files to Archive", &m_showAddFileDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Add files to the secure archive:");
        ImGui::Separator();
        
        ImGui::Text("File Path:");
        ImGui::InputText("##filepath", m_filePathBuffer, sizeof(m_filePathBuffer));
        
        ImGui::Text("Display Name (optional):");
        ImGui::InputText("##filename", m_fileNameBuffer, sizeof(m_fileNameBuffer));
        
        ImGui::Separator();
        
        if (ImGui::Button("[F] Browse")) {
            IGFD::FileDialogConfig config;
	        config.path = ".";
            
            // Add flags for better functionality
            config.flags = ImGuiFileDialogFlags_Modal;
            
            // Filter for common file types
            const char* filters = "All files (*.*){.*},Image files (*.png *.jpg *.jpeg *.bmp){.png,.jpg,.jpeg,.bmp},Text files (*.txt *.md){.txt,.md},Source files (*.cpp *.h){.cpp,.h}";
            
            ImGuiFileDialog::Instance()->OpenDialog(
                "FileOpenDialog", "Choose a file", filters, config);
            
            std::cout << "File browse dialog opened at path: " << config.path << std::endl;
            
            // Use more flags to ensure dialog displays correctly
           // config.flags = ImGuiFileDialogFlags_Modal |
           //               ImGuiFileDialogFlags_ReadOnlyFileNameField;
            
            // Filter for common file types
            //const char* filters = "Image files (*.png *.jpg *.jpeg *.bmp){.png,.jpg,.jpeg,.bmp},"
             //                    "Text files (*.txt *.md *.cpp *.h){.txt,.md,.cpp,.h},"
             //                  "All files (*.*){.*}";
            
            // Open dialog with the correct signature
            //ImGuiFileDialog::Instance()->OpenDialog(
            //    "ChooseFileDlgKey", 
            //    "Choose File", 
            //    filters,
            //    config);
            
           // std::cout << "File dialog opened at path: " << config.path << std::endl;
        }
        // Display ImGuiFileDialog for file selection
        ImVec2 dialogSize = GetStandardDialogSize();
        ImVec2 dialogPos = GetStandardDialogPosition();

        // Stabilize the dialog by setting the window size and position
        if (ImGuiFileDialog::Instance()->IsOpened("FileOpenDialog")) {
            ImGui::SetNextWindowPos(dialogPos);
            ImGui::SetNextWindowSize(dialogSize);
        }
        
        // Display the dialog
        if (ImGuiFileDialog::Instance()->Display("FileOpenDialog", 
                                               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize, 
                                               dialogSize, 
                                               dialogPos))
        {
            if (ImGuiFileDialog::Instance()->IsOk())
            {
                selectedFile = ImGuiFileDialog::Instance()->GetFilePathName();
                
                // Set the selected file path in the buffer
                strncpy(m_filePathBuffer, selectedFile.c_str(), sizeof(m_filePathBuffer) - 1);
                m_filePathBuffer[sizeof(m_filePathBuffer) - 1] = '\0';
                
                // Set default filename (just the filename without path)
                std::string fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();
                strncpy(m_fileNameBuffer, fileName.c_str(), sizeof(m_fileNameBuffer) - 1);
                m_fileNameBuffer[sizeof(m_fileNameBuffer) - 1] = '\0';
                
                std::cout << "Selected file: " << selectedFile << std::endl;
            }
            ImGuiFileDialog::Instance()->Close();
        }
        }

        // Afișare fișier selectat
        if (!selectedFile.empty())
        {
            ImGui::Text("Selected file:");
            ImGui::TextWrapped("%s", selectedFile.c_str());
        }

        ImGui::SameLine();
        Settings::PushBlackButtonText();
        if (ImGui::Button("[OK] Add File")) {
            std::string filePath = m_filePathBuffer;
            std::string fileName = m_fileNameBuffer;
            
            std::cout << "---------- FILE ADDITION DEBUG ----------" << std::endl;
            std::cout << "Add File button clicked" << std::endl;
            std::cout << "File path: '" << filePath << "'" << std::endl;
            std::cout << "File name: '" << fileName << "'" << std::endl;
            
            // Check file existence in detail
            bool fileExists = false;
            try {
                fileExists = std::filesystem::exists(filePath);
                std::cout << "File exists check result: " << (fileExists ? "Yes" : "No") << std::endl;
                
                if (fileExists) {
                    auto fileSize = std::filesystem::file_size(filePath);
                    std::cout << "File size: " << fileSize << " bytes" << std::endl;
                    std::cout << "Is regular file: " << (std::filesystem::is_regular_file(filePath) ? "Yes" : "No") << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "Exception during file check: " << e.what() << std::endl;
            }
            
            // Try opening the file to make sure we have read access
            std::ifstream testOpen(filePath, std::ios::binary);
            std::cout << "File can be opened for reading: " << (testOpen.good() ? "Yes" : "No") << std::endl;
            if (testOpen) {
                testOpen.close();
            }
            
            // Check archive status
            std::cout << "Archive loaded status: " << (m_isLoaded ? "Yes" : "No") << std::endl;
            
            // Extra validation - avoid empty strings or paths with only spaces
            if (filePath.empty() || filePath.find_first_not_of(" \t\n\r") == std::string::npos) {
                std::cout << "File path is empty or only contains whitespace" << std::endl;
                SetStatusMessage("Please select a valid file!", 3.0f);
                std::cout << "----------------------------------------" << std::endl;
                return;
            }
            
            if (!fileExists) {
                std::cout << "File doesn't exist at path: '" << filePath << "'" << std::endl;
                SetStatusMessage("Please select a valid file!", 3.0f);
                std::cout << "----------------------------------------" << std::endl;
                return;
            }
            
            // Try to add the file with detailed logging
            std::cout << "Calling AddFile on archive..." << std::endl;
            bool addResult = m_archive->AddFile(filePath, fileName);
            std::cout << "AddFile result: " << (addResult ? "Success" : "Failed") << std::endl;
            
            if (addResult) {
                std::cout << "AddFile successful" << std::endl;
                RefreshFileList();
                SetStatusMessage("File added successfully!");
                
                // Clear buffers
                memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
                memset(m_fileNameBuffer, 0, sizeof(m_fileNameBuffer));
                
                m_showAddFileDialog = false;
            } else {
                std::cout << "AddFile failed" << std::endl;
                SetStatusMessage("Failed to add file!", 5.0f);
            }
            
            std::cout << "----------------------------------------" << std::endl;
        }
        Settings::PopBlackButtonText();
        
        ImGui::SameLine();
        Settings::PushBlackButtonText();
        if (ImGui::Button("[C] Cancel")) {
            m_showAddFileDialog = false;
        }
        Settings::PopBlackButtonText();
        
    // Display ImGuiFileDialog - use a specific size to make sure it's visible
    ImVec2 dialogSize = GetStandardDialogSize();
    ImVec2 dialogPos = GetStandardDialogPosition();

    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey",
                                            ImGuiWindowFlags_NoCollapse,
                                            dialogSize,
                                            dialogPos)) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
            std::string fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();
            
            // Debug output
            std::cout << "File selection OK" << std::endl;
            std::cout << "Path+Name: " << filePathName << std::endl;
            std::cout << "Path only: " << filePath << std::endl;
            std::cout << "File name: " << fileName << std::endl;
            
            // Set the selected file path in the buffer
            strncpy(m_filePathBuffer, filePathName.c_str(), sizeof(m_filePathBuffer) - 1);
            m_filePathBuffer[sizeof(m_filePathBuffer) - 1] = '\0';
            
            // Set default filename (just the filename without path)
            strncpy(m_fileNameBuffer, fileName.c_str(), sizeof(m_fileNameBuffer) - 1);
            m_fileNameBuffer[sizeof(m_fileNameBuffer) - 1] = '\0';
            
            std::cout << "Buffer set to: " << m_filePathBuffer << std::endl;
        } else {
            std::cout << "File dialog canceled" << std::endl;
        }
        
        // Close the file dialog
        ImGuiFileDialog::Instance()->Close();
    }
    ImGui::EndPopup();
}

void ArchiveWindow::ShowExtractDialog() {
    if (m_selectedFile < 0 || m_selectedFile >= static_cast<int>(m_fileList.size())) {
        m_showExtractDialog = false;
        return;
    }
    
    ImGui::OpenPopup("Extract File");
    
    if (ImGui::BeginPopupModal("Extract File", &m_showExtractDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        const FileEntry& entry = m_fileList[m_selectedFile];
        
        ImGui::Text("Extract file: %s", entry.name.c_str());
        ImGui::Text("Size: %s", FormatFileSize(entry.size).c_str());
        ImGui::Separator();
        
        ImGui::Text("Extract to:");
        ImGui::InputText("##extractpath", m_extractPathBuffer, sizeof(m_extractPathBuffer));
        
        Settings::PushBlackButtonText();
        if (ImGui::Button("[F] Browse Folder")) {
            // Open ImGuiFileDialog for folder selection
            IGFD::FileDialogConfig config;
            config.path = std::filesystem::current_path().string();
            config.countSelectionMax = 1;
            
            // Pentru selectarea directoarelor, folosim nullptr ca filtru
            // și eliminăm ReadOnlyFileNameField pentru a permite navigarea directoarelor
            config.flags = ImGuiFileDialogFlags_Modal |
                          ImGuiFileDialogFlags_DontShowHiddenFiles;
                          
            std::cout << "Opening folder selection dialog at path: " << config.path << std::endl;
            std::cout << "Dialog should only allow directory selection" << std::endl;
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFolderDlgKey", "Choose Destination Folder", 
                nullptr, config);  // nullptr pentru directoare
        }
        Settings::PopBlackButtonText();
        
        ImGui::Separator();
        
        Settings::PushBlackButtonText();
        if (ImGui::Button("[>] Extract")) {
            std::string extractPath = m_extractPathBuffer;
            
            std::cout << "\n---------- FILE EXTRACTION ----------" << std::endl;
            std::cout << "Extracting file: " << entry.name << std::endl;
            std::cout << "Extract path: '" << extractPath << "'" << std::endl;
            std::cout << "Archive loaded status: " << (m_isLoaded ? "Yes" : "No") << std::endl;
            
            // Check if extract path is valid
            if (extractPath.empty()) {
                std::cout << "Extract path is empty!" << std::endl;
                SetStatusMessage("Please specify extract path!", 3.0f);
                std::cout << "--------------------------------------\n" << std::endl;
                return;
            }
            
            // Check if the path exists and is a directory without a filename
            std::filesystem::path extractPathObj(extractPath);
            if (std::filesystem::exists(extractPathObj) && 
                std::filesystem::is_directory(extractPathObj) && 
                extractPathObj.filename().empty()) {
                // Automatically append the filename
                extractPath = (extractPathObj / entry.name).string();
                std::cout << "Adjusted extract path to include filename: " << extractPath << std::endl;
            }
                // Create directory if it doesn't exist
                try {
                    std::filesystem::path parentPath = std::filesystem::path(extractPath).parent_path();
                    std::cout << "Creating parent directory: " << parentPath << std::endl;
                    std::filesystem::create_directories(parentPath);
                    std::cout << "Directory creation result: " << (std::filesystem::exists(parentPath) ? "Success" : "Failed") << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "Error creating directories: " << e.what() << std::endl;
                }
                
                std::cout << "Calling ExtractFile..." << std::endl;
                bool extractResult = m_archive->ExtractFile(entry.name, extractPath);
                std::cout << "Extract result: " << (extractResult ? "Success" : "Failed") << std::endl;
                
                if (extractResult) {
                    // Try to get the actual path where the file was saved
                    // The ExtractFile function might have modified the path internally
                    std::filesystem::path extractPathObj(extractPath);
                    std::string checkPath = extractPath;
                    
                    // If extractPath is a directory, check if the file was created inside it
                    if (std::filesystem::is_directory(extractPathObj)) {
                        checkPath = (extractPathObj / entry.name).string();
                        std::cout << "Checking for file in directory: " << checkPath << std::endl;
                    }
                    
                    // Verify the extracted file exists
                    bool fileExists = std::filesystem::exists(checkPath);
                    std::cout << "Extracted file exists at " << checkPath << ": " << (fileExists ? "Yes" : "No") << std::endl;
                    if (fileExists) {
                        try {
                            auto fileSize = std::filesystem::file_size(checkPath);
                            std::cout << "Extracted file size: " << fileSize << " bytes" << std::endl;
                        } catch (const std::exception& e) {
                            std::cout << "Error checking file size: " << e.what() << std::endl;
                        }
                    } else {
                        // Try to check the original path as fallback
                        fileExists = std::filesystem::exists(extractPath);
                        std::cout << "Checking original path " << extractPath << ": " << (fileExists ? "Yes" : "No") << std::endl;
                    }
                    
                    SetStatusMessage("File extracted successfully!");
                    m_showExtractDialog = false;
                } else {
                    SetStatusMessage("Failed to extract file!", 5.0f);
                }
            } else {
                std::cout << "Extract path is empty!" << std::endl;
                SetStatusMessage("Please specify extract path!", 3.0f);
            }
            std::cout << "--------------------------------------\n" << std::endl;
        }
        Settings::PopBlackButtonText();
        
        ImGui::SameLine();
        Settings::PushBlackButtonText();
        if (ImGui::Button("[C] Cancel")) {
            m_showExtractDialog = false;
        }
        Settings::PopBlackButtonText();
        

    
    // Display ImGuiFileDialog for folder selection - use a specific size to make sure it's visible
    static ImVec2 dialogSize = GetStandardDialogSize();
    static ImVec2 dialogPos = GetStandardDialogPosition();
    
    if (ImGuiFileDialog::Instance()->IsOpened("ChooseFolderDlgKey")) {
        // Setăm poziția și dimensiunea fixă pentru dialog
        ImGui::SetNextWindowPos(dialogPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(dialogSize, ImGuiCond_Always);
    }
    
    if (ImGuiFileDialog::Instance()->Display("ChooseFolderDlgKey", 
                                            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize, 
                                            dialogSize,
                                            dialogPos)) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            // For directory selection, we'll just use the current path
            // since our version doesn't have direct directory selection
            std::string folderPath = ImGuiFileDialog::Instance()->GetCurrentPath();
            
            // Debug all available info
            std::cout << "Current directory: " << folderPath << std::endl;
            
            // Check if we have any selections
            auto selection = ImGuiFileDialog::Instance()->GetSelection();
            if (!selection.empty()) {
                std::cout << "Selection found: " << selection.size() << " items" << std::endl;
                for (auto& it : selection) {
                    std::cout << "  Selected: " << it.first << " -> " << it.second << std::endl;
                }
            }
            
            std::cout << "Folder selection OK" << std::endl;
            std::cout << "Selected path: " << folderPath << std::endl;
            
            // Use std::filesystem::path to properly combine paths
            const FileEntry& entry = m_fileList[m_selectedFile];
            std::filesystem::path destPath(folderPath);
            std::string fullPath = (destPath / entry.name).string();
            
            std::cout << "Full extraction path: " << fullPath << std::endl;
            
            // Set the extract path in the buffer
            strncpy(m_extractPathBuffer, fullPath.c_str(), sizeof(m_extractPathBuffer) - 1);
            m_extractPathBuffer[sizeof(m_extractPathBuffer) - 1] = '\0';
            
            std::cout << "Extract path buffer set to: " << m_extractPathBuffer << std::endl;
        } else {
            std::cout << "Folder dialog canceled" << std::endl;
        }
        
        // Close the file dialog
        ImGuiFileDialog::Instance()->Close();
    }
    ImGui::EndPopup();
}

void ArchiveWindow::ShowFileViewer() {
    std::cout << "ShowFileViewer called, preview type: " << 
        (m_previewType == PreviewType::TEXT ? "TEXT" : 
         m_previewType == PreviewType::IMAGE ? "IMAGE" : "NONE") << std::endl;
    
    // Check if we have data to display
    if (m_previewType == PreviewType::TEXT && !m_textPreviewData.empty()) {
        // Convert binary data to text
        std::string text;
        
        // Add a null terminator to ensure the text is valid
        std::vector<uint8_t> textData = m_textPreviewData;
        textData.push_back(0); // null terminator
        
        // Convert to character string
        text = reinterpret_cast<const char*>(textData.data());
        
        // Open a modal window for preview
        ImGui::OpenPopup("Text Preview");
        
        // Set a reasonable size for the preview window
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImVec2 previewSize = ImVec2(ImGui::GetIO().DisplaySize.x * 0.7f, ImGui::GetIO().DisplaySize.y * 0.7f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(previewSize, ImGuiCond_Appearing);
        
        if (ImGui::BeginPopupModal("Text Preview", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
            // Get theme-appropriate colors
            Settings& settings = Settings::Instance();
            auto themeColors = settings.GetThemeColors();
            
            // Add menu bar
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Close", "Esc")) {
                        ResetPreview();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
            
            // Indicator că textul poate fi selectat
            ImGui::TextColored(ImVec4(themeColors.infoText[0], themeColors.infoText[1], themeColors.infoText[2], themeColors.infoText[3]), "You can select text and press Ctrl+C to copy");
            
            // Display text in a scrollable area
            ImGui::BeginChild("TextContent", ImVec2(0, -60), true, ImGuiWindowFlags_HorizontalScrollbar);
            
            // Use our helper function to display selectable text
            DisplaySelectableText(text, ImGui::GetContentRegionAvail());
            
            ImGui::EndChild();
            
            ImGui::Separator();
            
            // Display information about file size
            ImGui::Text("Size: %s (%zu bytes)", FormatFileSize(m_textPreviewData.size()).c_str(), m_textPreviewData.size());
            
            // Button for copying all text with button styling and theme-appropriate colors
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(themeColors.accentText[0], themeColors.accentText[1], themeColors.accentText[2], 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(themeColors.accentText[0] * 1.2f, themeColors.accentText[1] * 1.2f, themeColors.accentText[2] * 1.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(themeColors.accentText[0] * 1.4f, themeColors.accentText[1] * 1.4f, themeColors.accentText[2] * 1.4f, 1.0f));
            Settings::PushBlackButtonText();
            
            if (ImGui::Button("[C] Copy All Text", ImVec2(160, 0))) {
                ImGui::SetClipboardText(text.c_str());
                SetStatusMessage("Text copied to clipboard!", 2.0f);
            }
            
            Settings::PopBlackButtonText();
            ImGui::PopStyleColor(3);
            
            ImGui::SameLine();
            
            // Close button
            Settings::PushBlackButtonText();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                ResetPreview();
            }
            Settings::PopBlackButtonText();
            
            ImGui::EndPopup();
        }
    } 
    else if (m_previewType == PreviewType::IMAGE && !m_imagePreviewData.empty()) {
        // Deschidem o fereastră modală pentru previzualizare imagini
        ImGui::OpenPopup("Image Preview");
        
        // Stabilim o dimensiune rezonabilă pentru fereastra de previzualizare
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImVec2 previewSize = ImVec2(ImGui::GetIO().DisplaySize.x * 0.7f, ImGui::GetIO().DisplaySize.y * 0.7f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(previewSize, ImGuiCond_Appearing);
        
        if (ImGui::BeginPopupModal("Image Preview", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
            // Get theme-appropriate colors
            Settings& settings = Settings::Instance();
            auto themeColors = settings.GetThemeColors();
            
            // Add menu bar
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Close", "Esc")) {
                        ResetPreview();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
            
            // Centrul ferestrei
            ImGui::BeginChild("ImageContent", ImVec2(0, -30), true, ImGuiWindowFlags_HorizontalScrollbar);
            
            // Notă: Pentru previzualizarea propriu-zisă a imaginii ar trebui să folosim o bibliotecă
            // precum stb_image sau să integrăm OpenGL/texturi ImGui
            // Deocamdată afișăm un mesaj informativ
            ImGui::TextColored(ImVec4(themeColors.warningText[0], themeColors.warningText[1], themeColors.warningText[2], themeColors.warningText[3]), "Image preview is not fully implemented yet");
            ImGui::TextWrapped("This feature requires loading the image data into a texture.");
            ImGui::TextWrapped("Image size: %zu bytes", m_imagePreviewData.size());
            
            ImGui::EndChild();
            
            ImGui::Separator();
            
            // Display information about file size
            ImGui::Text("Size: %s (%zu bytes)", FormatFileSize(m_imagePreviewData.size()).c_str(), m_imagePreviewData.size());
            
            // Close button
            Settings::PushBlackButtonText();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                ResetPreview();
            }
            Settings::PopBlackButtonText();
            
            ImGui::EndPopup();
        }
    }
}

void ArchiveWindow::ShowArchiveStats() {
    auto stats = m_archive->GetStats();
    
    ImGui::OpenPopup("Archive Statistics");
    
    if (ImGui::BeginPopupModal("Archive Statistics", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Archive Statistics");
        ImGui::Separator();
        
        ImGui::Text("Total Files: %zu", stats.totalFiles);
        ImGui::Text("Total Size: %s", FormatFileSize(stats.totalSize).c_str());
        ImGui::Text("Last Modified: %s", stats.lastModified.c_str());
        ImGui::Text("Archive Path: archives/%s_img.enc", m_username.c_str());
        
        ImGui::Separator();
        
        Settings::PushBlackButtonText();
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        Settings::PopBlackButtonText();
        
        ImGui::EndPopup();
    }
}

void ArchiveWindow::HandleDragDrop() {
    // TODO: Implement drag and drop functionality
}

std::string ArchiveWindow::FormatFileSize(size_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << size << " " << units[unit];
    return ss.str();
}

std::string ArchiveWindow::GetFileTypeIcon(const std::string& filename) const {
    std::string ext = std::filesystem::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Image files
    if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".gif" || ext == ".bmp" || ext == ".tiff" || ext == ".webp") {
        return "[IMG]";
    } 
    // Text files
    else if (ext == ".txt" || ext == ".log" || ext == ".md" || ext == ".csv") {
        return "[TXT]";
    }
    // Code files
    else if (ext == ".cpp" || ext == ".h" || ext == ".c" || ext == ".hpp" || ext == ".js" || 
             ext == ".ts" || ext == ".py" || ext == ".java" || ext == ".cs" || ext == ".php") {
        return "[CODE]";
    }
    // Document files
    else if (ext == ".pdf") {
        return "[PDF]";
    } 
    else if (ext == ".doc" || ext == ".docx") {
        return "[DOC]";
    }
    else if (ext == ".xls" || ext == ".xlsx" || ext == ".ods") {
        return "[XLS]";
    }
    else if (ext == ".ppt" || ext == ".pptx") {
        return "[PPT]";
    }
    // Archive files
    else if (ext == ".zip" || ext == ".rar" || ext == ".7z" || ext == ".tar" || 
             ext == ".gz" || ext == ".bz2" || ext == ".xz") {
        return "[ZIP]";
    }
    // Executable files
    else if (ext == ".exe" || ext == ".msi" || ext == ".app" || ext == ".sh" || 
             ext == ".bat" || ext == ".cmd") {
        return "[EXE]";
    }
    // Media files
    else if (ext == ".mp3" || ext == ".wav" || ext == ".ogg" || ext == ".flac") {
        return "[AUD]";
    }
    else if (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || ext == ".mov" || ext == ".wmv") {
        return "[VID]";
    }
    // HTML/Web files
    else if (ext == ".html" || ext == ".htm" || ext == ".css" || ext == ".xml") {
        return "[WEB]";
    }
    // Directory (though likely not used in this archive system)
    else if (ext == "" && filename.back() == '/') {
        return "[DIR]";
    }
    // Default for other file types
    else {
        return "[FILE]";
    }
}

void ArchiveWindow::SetStatusMessage(const std::string& message, float duration) {
    m_statusMessage = message;
    m_statusMessageTime = duration;
}

void ArchiveWindow::UpdateStatusMessage() {
    if (m_statusMessageTime > 0.0f) {
        m_statusMessageTime -= ImGui::GetIO().DeltaTime;
        if (m_statusMessageTime <= 0.0f) {
            m_statusMessage.clear();
        }
    }
}

bool ArchiveWindow::IsImageFile(const std::string& filename) const {
    std::string ext = std::filesystem::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".gif" || ext == ".bmp";
}

bool ArchiveWindow::IsTextFile(const std::string& filename) const {
    std::string ext = std::filesystem::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return ext == ".txt" || ext == ".log" || ext == ".md" || ext == ".cpp" || ext == ".h" || ext == ".py";
}

bool ArchiveWindow::IsDocumentFile(const std::string& filename) const {
    std::string ext = std::filesystem::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return ext == ".pdf" || ext == ".doc" || ext == ".docx";
}

void ArchiveWindow::ShowImagePreview(const std::vector<uint8_t>& data) {
    std::cout << "ShowImagePreview called with " << data.size() << " bytes" << std::endl;
    
    // Save data for display in the rendering cycle
    m_imagePreviewData = data;
    m_previewType = PreviewType::IMAGE;
    m_showFileViewer = true;
    
    std::cout << "Image preview prepared, m_showFileViewer set to true" << std::endl;
    
    // This section was moved to ShowFileViewer to avoid code duplication
    // and to ensure consistent handling of previews
}

void ArchiveWindow::ShowTextPreview(const std::vector<uint8_t>& data) {
    std::cout << "ShowTextPreview called with " << data.size() << " bytes" << std::endl;
    
    // Save data for display in the rendering cycle
    m_textPreviewData = data;
    m_previewType = PreviewType::TEXT;
    m_showFileViewer = true;
    
    std::cout << "Text preview prepared, m_showFileViewer set to true" << std::endl;
    
    // This section was moved to ShowFileViewer to avoid code duplication
    // and to ensure consistent handling of previews
}

void ArchiveWindow::ShowFilePreview(const FileEntry& entry) {
    std::cout << "\n---------- SHOW FILE PREVIEW ----------" << std::endl;
    std::cout << "File: " << entry.name << ", Size: " << entry.size << " bytes" << std::endl;
    
    // Verificăm dacă arhiva este încărcată
    std::cout << "Archive loaded status: " << (m_isLoaded ? "Yes" : "No") << std::endl;
    if (!m_isLoaded) {
        std::cout << "Cannot preview - archive not loaded!" << std::endl;
        SetStatusMessage("Cannot preview file: Archive not loaded!", 3.0f);
        return;
    }
    
    // Attempt to repair archive before previewing
    if (entry.size > 0 && entry.data.empty()) {
        std::cout << "File entry has inconsistent state, attempting to repair..." << std::endl;
        m_archive->RepairArchive();
    }
    
    // Check if the file has valid data in the entry object
    if (entry.data.empty()) {
        std::cout << "WARNING: Entry has empty data in FileEntry object!" << std::endl;
        std::cout << "Will attempt to extract from archive anyway..." << std::endl;
    } else {
        std::cout << "Entry has data of size: " << entry.data.size() << " bytes" << std::endl;
    }
    
    // Run diagnostic to check archive state
    m_archive->DiagnoseArchive();
    
    // Extract file data into memory
    std::vector<uint8_t> fileData;
    std::cout << "Calling ExtractFileToMemory for file: " << entry.name << std::endl;
    bool success = m_archive->ExtractFileToMemory(entry.name, fileData);
    std::cout << "ExtractFileToMemory result: " << (success ? "Success" : "Failed") << std::endl;
    std::cout << "Data size received: " << fileData.size() << " bytes" << std::endl;
    
    if (!success || fileData.empty()) {
        std::cout << "Failed to extract file data - trying to fix the archive..." << std::endl;
        
        // Try to repair the archive
        if (m_archive->RepairArchive()) {
            std::cout << "Archive repaired, trying extraction again..." << std::endl;
            
            // Try extraction again after repair
            success = m_archive->ExtractFileToMemory(entry.name, fileData);
            std::cout << "Second extraction attempt result: " << (success ? "Success" : "Failed") << std::endl;
            std::cout << "Data size received on retry: " << fileData.size() << " bytes" << std::endl;
            
            if (!success || fileData.empty()) {
                std::cout << "Failed to extract file even after repair" << std::endl;
                SetStatusMessage("Failed to extract file data for preview!", 3.0f);
                m_showFileViewer = false;
                return;
            }
        } else {
            std::cout << "Failed to repair archive" << std::endl;
            SetStatusMessage("Failed to extract file data for preview!", 3.0f);
            m_showFileViewer = false;
            return;
        }
    }
    
    if (!success) {
        std::cout << "File extraction failed" << std::endl;
        SetStatusMessage("Failed to extract file for preview!", 3.0f);
        return;
    }
    
    if (fileData.empty()) {
        std::cout << "File extraction returned empty data" << std::endl;
        SetStatusMessage("File appears to be empty!", 3.0f);
        return;
    }
    
    // Setăm flag-ul pentru a arăta previzualizarea
    m_showFileViewer = true;
    
    // Depending on the file type, choose the appropriate preview method
    std::cout << "File type checks - IsText: " << (IsTextFile(entry.name) ? "Yes" : "No") 
              << ", IsImage: " << (IsImageFile(entry.name) ? "Yes" : "No") << std::endl;
              
    if (IsTextFile(entry.name)) {
        std::cout << "Showing text preview" << std::endl;
        ShowTextPreview(fileData);
    } else if (IsImageFile(entry.name)) {
        std::cout << "Showing image preview" << std::endl;
        ShowImagePreview(fileData);
    } else {
        std::cout << "Unsupported file type for preview" << std::endl;
        SetStatusMessage("Preview not available for this file type!", 3.0f);
        m_showFileViewer = false;
    }
    
    std::cout << "--------------------------------------\n" << std::endl;
}


// Helper methods for consistent dialog sizing
ImVec2 ArchiveWindow::GetStandardDialogSize() const {
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    // Use fixed dimensions to prevent recalculation which can cause flickering
    return ImVec2(displaySize.x * 0.99f, displaySize.y * 0.8f);
}

ImVec2 ArchiveWindow::GetStandardDialogPosition() const {
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 dialogSize = GetStandardDialogSize();
    // Centered on screen
    return ImVec2((displaySize.x - dialogSize.x) * 0.5f, 
                  (displaySize.y - dialogSize.y) * 0.5f);
}

// Helper implementation for multi-line selectable text
// Helper for displaying selectable text
void ArchiveWindow::DisplaySelectableText(const std::string& text, const ImVec2& size) {
    // Use static variables to keep memory allocated between frames
    static char* buffer = nullptr;
    static size_t buffer_size = 0;
    static bool showCopySuccessMsg = false;
    static float copyMsgTimer = 0.0f;
    
    // Get theme-appropriate colors
    Settings& settings = Settings::Instance();
    auto themeColors = settings.GetThemeColors();
    
    // Allocate or reallocate buffer if needed
    if (buffer_size < text.size() + 1) {
        delete[] buffer;
        buffer_size = text.size() + 1;
        buffer = new char[buffer_size];
    }
    
    // Copy the text to buffer
    std::copy(text.begin(), text.end(), buffer);
    buffer[text.size()] = '\0';
    
    // Display text as readonly input that allows selection with theme-appropriate background
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(themeColors.accentText[0] * 0.1f, themeColors.accentText[1] * 0.1f, themeColors.accentText[2] * 0.1f, 0.5f));
    ImGui::InputTextMultiline("##TextPreviewContent", 
                             buffer, 
                             buffer_size,
                             size, 
                             ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleColor();
    
    // Display an explanatory tooltip when text is hovered
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Select text and use Ctrl+C to copy");
        ImGui::EndTooltip();
    }
    
    // Button for copying all text with user feedback
    ImGui::PushID("CopyAllTextButton");
    Settings::PushBlackButtonText();
    if (ImGui::Button("Copy All Text", ImVec2(140, 0))) {
        ImGui::SetClipboardText(text.c_str());
        showCopySuccessMsg = true;
        copyMsgTimer = 2.0f; // Show message for 2 seconds
    }
    Settings::PopBlackButtonText();
    ImGui::PopID();
    
    // Show success message when text is copied via button
    if (showCopySuccessMsg) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(themeColors.successText[0], themeColors.successText[1], themeColors.successText[2], themeColors.successText[3]), "Text copied to clipboard!");
        
        // Update timer and hide message when time is up
        copyMsgTimer -= ImGui::GetIO().DeltaTime;
        if (copyMsgTimer <= 0.0f) {
            showCopySuccessMsg = false;
        }
    }
}

bool ArchiveWindow::LoadArchive(const std::string& archiveName, const std::string& password) {
    std::cout << "\n---------- ARCHIVE WINDOW LOAD ARCHIVE ----------" << std::endl;
    std::cout << "Loading archive: " << archiveName << " for user " << m_username << std::endl;
    
    // Create a new archive object with the specified archive name
    m_archive = std::make_unique<CryptoArchive>(m_username, archiveName);
    m_password = password;
    
    bool success = false;
    
    // Log the expected file path for debugging
    std::string expectedPath = "archives/" + m_username + "_" + archiveName + ".enc";
    std::cout << "Expected archive file path: " << expectedPath << std::endl;
    std::cout << "File exists check: " << (std::filesystem::exists(expectedPath) ? "Yes" : "No") << std::endl;
    
    // First try to load if archive exists
    if (m_archive->ArchiveExists()) {
        std::cout << "Archive exists, loading..." << std::endl;
        success = m_archive->LoadArchive(password);
        
        if (success) {
            std::cout << "Successfully loaded archive: " << archiveName << std::endl;
            // Reset UI state for the new archive
            m_selectedFile = -1; // Reset selected file
            m_previewData.clear(); // Clear preview data
            m_previewType = PreviewType::NONE; // Reset preview type
            SetStatusMessage("Archive '" + archiveName + "' loaded successfully");
        } else {
            std::cout << "Loading archive " << archiveName << " failed. Archive might be corrupted or password is wrong." << std::endl;
            SetStatusMessage("Failed to load archive '" + archiveName + "'");
        }
    } else {
        std::cout << "Archive " << archiveName << " does not exist." << std::endl;
        SetStatusMessage("Archive '" + archiveName + "' does not exist");
        std::cout << "------------------------------------------------\n" << std::endl;
        return false;
    }
    
    m_isLoaded = success;
    std::cout << "Archive loaded state: " << (m_isLoaded ? "Yes" : "No") << std::endl;
    std::cout << "------------------------------------------------\n" << std::endl;
    return success;
}

void ArchiveWindow::DiagnoseCurrentState() {
    std::cout << "\n========== ARCHIVE WINDOW DIAGNOSTIC ==========\n" << std::endl;
    std::cout << "Username: " << m_username << std::endl;
    std::cout << "Archive loaded state: " << (m_isLoaded ? "Yes" : "No") << std::endl;
    std::cout << "Window visible state: " << (m_isVisible ? "Yes" : "No") << std::endl;
    
    if (m_archive) {
        std::cout << "Archive object exists" << std::endl;
        std::cout << "Archive name: " << m_archive->GetArchiveName() << std::endl;
        std::string archivePath = m_archive->GetArchiveFilePath();
        std::cout << "Archive path: " << archivePath << std::endl;
        std::cout << "Archive file exists: " << (std::filesystem::exists(archivePath) ? "Yes" : "No") << std::endl;
        m_archive->DiagnoseArchive();
    } else {
        std::cout << "WARNING: Archive object is null!" << std::endl;
    }
    
    std::cout << "\nSelected file index: " << m_selectedFile << std::endl;
    std::cout << "Preview type: " << (m_previewType == PreviewType::NONE ? "None" : 
                                      m_previewType == PreviewType::TEXT ? "Text" : "Image") << std::endl;
    std::cout << "Status message: " << m_statusMessage << std::endl;
    
    std::cout << "\n==============================================\n" << std::endl;
}