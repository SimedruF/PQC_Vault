#include "ArchiveWindow.h"
#include "Settings.h"
#include "FileDropQueue.h"
#include "PathSecurity.h"
#include <imgui.h>
#include "ImGuiFileDialogConfig.h" // Include custom configuration first
#include "ImGuiFileDialog.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

ArchiveWindow::ArchiveWindow(const std::string& username) 
    : m_username(username), m_isVisible(false), m_isLoaded(false), m_selectedFile(-1),
      m_showAddFileDialog(false), m_showExtractDialog(false), m_showFileViewer(false),
      m_showArchiveStats(false), m_showResetConfirmation(false),
      m_showReloadConfirmation(false), m_openRemoveConfirmation(false),
      m_statusMessageTime(0.0f), m_statusMessageDuration(0.0f),
      m_statusMessageKind(NotificationKind::Info),
      m_dropZoneMin(0.0f, 0.0f), m_dropZoneMax(0.0f, 0.0f),
      m_dropZoneValid(false), m_dropFeedbackTime(0.0f),
      m_previewType(PreviewType::NONE) {
    
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
    ResetPreview();
    for (auto& entry : m_fileList) {
        SecureMemory::Cleanse(entry.data);
    }
    m_fileList.clear();
    m_archive.reset();
}

void ArchiveWindow::Render() {
    if (!m_isVisible) {
        FileDropQueue::Clear();
        return;
    }
    
    UpdateStatusMessage();
    
    // Get theme-appropriate colors
    Settings& settings = Settings::Instance();
    
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Secure Archive", &m_isVisible, ImGuiWindowFlags_MenuBar)) {
        FileDropQueue::Clear();
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
                m_showResetConfirmation = true;
            }
            
            if (ImGui::MenuItem("Reload Archive", nullptr)) {
                std::cout << "Reloading archive..." << std::endl;
                if (m_archive->ReloadArchive()) {
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
                m_showArchiveStats = true;
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Archive")) {
            if (ImGui::MenuItem("Reset Archive")) {
                m_showResetConfirmation = true;
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
                m_showReloadConfirmation = true;
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::EndMenuBar();
    }
    
    // Reserve enough room for the bottom toolbar using the shared GUI metrics.
    const auto& guiMetrics = Settings::Metrics();
    const float bottomToolbarHeight = guiMetrics.buttonHeight +
        ImGui::GetTextLineHeightWithSpacing() + guiMetrics.itemSpacing * 2.0f;
    ImGui::BeginChild("MainContent", ImVec2(0, -bottomToolbarHeight));
    m_dropZoneMin = ImGui::GetWindowPos();
    m_dropZoneMax = ImVec2(m_dropZoneMin.x + ImGui::GetWindowSize().x,
                           m_dropZoneMin.y + ImGui::GetWindowSize().y);
    m_dropZoneValid = true;
    
    // Compact file table. File type is conveyed by the name prefix, while all
    // actions live in one toolbar for the selected row.
    if (m_fileList.empty()) {
        ImGui::SetCursorPosY(40.0f);
        const char* emptyTitle = "This archive is empty";
        const char* emptyDescription =
            "Add files, or drop them here, to begin using this encrypted archive.";
        ImGui::SetCursorPosX(std::max(guiMetrics.windowPadding,
            (ImGui::GetWindowWidth() - ImGui::CalcTextSize(emptyTitle).x) * 0.5f));
        ImGui::TextUnformatted(emptyTitle);
        ImGui::SetCursorPosX(std::max(guiMetrics.windowPadding,
            (ImGui::GetWindowWidth() - ImGui::CalcTextSize(emptyDescription).x) * 0.5f));
        ImGui::TextDisabled("%s", emptyDescription);
        ImGui::SetCursorPosX(std::max(guiMetrics.windowPadding,
            (ImGui::GetWindowWidth() - 120.0f) * 0.5f));
        if (settings.IconButton("Add files", Settings::UiIcon::File,
                                Settings::ButtonVariant::Primary, 120.0f)) {
            m_showAddFileDialog = true;
        }
    } else if (ImGui::BeginTable(
                   "FileList", 3,
                   ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuter |
                   ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                   ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableHeadersRow();
        
        for (int i = 0; i < static_cast<int>(m_fileList.size()); ++i) {
            const FileEntry& entry = m_fileList[i];
            const bool selected = m_selectedFile == i;
            const bool canPreview = IsTextFile(entry.name) || IsImageFile(entry.name);

            ImGui::PushID(i);
            ImGui::TableNextRow(0, guiMetrics.buttonHeight);
            ImGui::TableSetColumnIndex(0);

            const std::string rowLabel = GetFileTypeIcon(entry.name) + "  " +
                                         entry.name + "##file";
            if (ImGui::Selectable(
                    rowLabel.c_str(), selected,
                    ImGuiSelectableFlags_SpanAllColumns |
                        ImGuiSelectableFlags_AllowDoubleClick,
                    ImVec2(0.0f, guiMetrics.buttonHeight))) {
                m_selectedFile = i;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (canPreview) {
                        ShowFilePreview(entry);
                    } else {
                        SetStatusMessage("Preview is not available for this file type.", 3.0f);
                    }
                }
            }

            if (ImGui::BeginPopupContextItem("FileActions")) {
                m_selectedFile = i;
                if (ImGui::MenuItem("Preview", "F3", false, canPreview)) {
                    ShowFilePreview(entry);
                }
                if (ImGui::MenuItem("Extract", "Ctrl+E")) {
                    m_showExtractDialog = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Remove", "Delete")) {
                    m_filePendingRemoval = entry.name;
                    m_openRemoveConfirmation = true;
                }
                ImGui::EndPopup();
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(FormatFileSize(entry.size).c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(entry.timestamp.c_str());
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
    
    ImGui::EndChild();
    HandleDragDrop();
    
    // Unified toolbar for the selected file.
    ImGui::Separator();
    const bool hasSelectedFile = m_selectedFile >= 0 &&
        m_selectedFile < static_cast<int>(m_fileList.size());
    const bool canPreviewSelected = hasSelectedFile &&
        (IsTextFile(m_fileList[m_selectedFile].name) ||
         IsImageFile(m_fileList[m_selectedFile].name));

    if (settings.IconButton("Add files", Settings::UiIcon::File,
                            Settings::ButtonVariant::Primary, 120.0f)) {
        m_showAddFileDialog = true;
    }
    ImGui::SameLine();

    if (!canPreviewSelected) {
        ImGui::BeginDisabled();
    }
    if (settings.IconButton("Preview", Settings::UiIcon::Info,
                            Settings::ButtonVariant::Secondary, 100.0f)) {
        ShowFilePreview(m_fileList[m_selectedFile]);
    }
    if (!canPreviewSelected) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (!hasSelectedFile) {
        ImGui::BeginDisabled();
    }
    if (settings.IconButton("Extract", Settings::UiIcon::Folder,
                            Settings::ButtonVariant::Secondary, 100.0f)) {
        m_showExtractDialog = true;
    }
    ImGui::SameLine();

    if (settings.IconButton("Remove", Settings::UiIcon::Error,
                            Settings::ButtonVariant::Danger, 100.0f)) {
        m_filePendingRemoval = m_fileList[m_selectedFile].name;
        m_openRemoveConfirmation = true;
    }
    if (!hasSelectedFile) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    const bool refreshRequested = settings.IconButton(
        "Refresh", Settings::UiIcon::Archive,
        Settings::ButtonVariant::Ghost, 100.0f);

    const auto stats = m_archive->GetStats();
    const std::string selectionText = hasSelectedFile
        ? "Selected: " + m_fileList[m_selectedFile].name
        : "Drop files into the list, or select a file to enable actions.";
    const std::string statsText = "Files: " + std::to_string(stats.totalFiles) +
        "  |  Total: " + FormatFileSize(stats.totalSize);
    ImGui::TextDisabled("%s", selectionText.c_str());
    ImGui::SameLine();
    const float statsStart = ImGui::GetWindowWidth() -
        ImGui::CalcTextSize(statsText.c_str()).x - guiMetrics.windowPadding;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), statsStart));
    ImGui::TextDisabled("%s", statsText.c_str());

    if (m_openRemoveConfirmation) {
        ImGui::OpenPopup("Remove file");
        m_openRemoveConfirmation = false;
    }

    ImGui::SetNextWindowSize(ImVec2(460.0f, 245.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                           ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Remove file", nullptr,
                               ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoSavedSettings)) {
        settings.DialogHeader(Settings::UiIcon::Warning, "Remove file",
                              "This action updates the encrypted archive immediately.");
        ImGui::Text("Remove '%s' from this archive?", m_filePendingRemoval.c_str());
        ImGui::Spacing();

        const float buttonGroupWidth = 110.0f + 100.0f + guiMetrics.itemSpacing;
        ImGui::SetCursorPosX(
            (ImGui::GetWindowWidth() - buttonGroupWidth) * 0.5f);
        if (settings.IconButton("Remove", Settings::UiIcon::Error,
                                Settings::ButtonVariant::Danger, 110.0f)) {
            if (m_archive->RemoveFile(m_filePendingRemoval)) {
                RefreshFileList();
                SetStatusMessage("File removed successfully.");
            } else {
                SetStatusMessage("Failed to remove file.", 5.0f);
            }
            m_filePendingRemoval.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (settings.Button("Cancel", Settings::ButtonVariant::Ghost, 100.0f)) {
            m_filePendingRemoval.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (m_showResetConfirmation && !ImGui::IsPopupOpen("Reset archive")) {
        ImGui::OpenPopup("Reset archive");
    }
    ImGui::SetNextWindowSize(ImVec2(480.0f, 270.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                           ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Reset archive", &m_showResetConfirmation,
                               ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoSavedSettings)) {
        settings.DialogHeader(Settings::UiIcon::Warning, "Reset archive",
                              "Every file in this archive will be removed.");
        ImGui::TextWrapped(
            "This operation cannot be undone. The archive itself will remain available.");
        ImGui::Spacing();

        const float buttonGroupWidth = 130.0f + 100.0f + guiMetrics.itemSpacing;
        ImGui::SetCursorPosX(
            (ImGui::GetWindowWidth() - buttonGroupWidth) * 0.5f);
        if (settings.IconButton("Reset", Settings::UiIcon::Warning,
                                Settings::ButtonVariant::Danger, 130.0f)) {
            if (m_archive->ResetArchive()) {
                RefreshFileList();
                SetStatusMessage("Archive reset successfully.");
            } else {
                SetStatusMessage("Failed to reset archive.", 5.0f);
            }
            m_showResetConfirmation = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (settings.Button("Cancel", Settings::ButtonVariant::Ghost, 100.0f)) {
            m_showResetConfirmation = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (m_showReloadConfirmation && !ImGui::IsPopupOpen("Reload archive")) {
        ImGui::OpenPopup("Reload archive");
    }
    ImGui::SetNextWindowSize(ImVec2(480.0f, 250.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                           ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Reload archive", &m_showReloadConfirmation,
                               ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoSavedSettings)) {
        settings.DialogHeader(Settings::UiIcon::Archive, "Reload archive",
                              "Read the latest authenticated data from disk.");
        ImGui::TextWrapped("The current selection and open preview will be cleared.");
        ImGui::Spacing();

        const float buttonGroupWidth = 130.0f + 100.0f + guiMetrics.itemSpacing;
        ImGui::SetCursorPosX(
            (ImGui::GetWindowWidth() - buttonGroupWidth) * 0.5f);
        if (settings.IconButton("Reload", Settings::UiIcon::Archive,
                                Settings::ButtonVariant::Primary, 130.0f)) {
            if (m_archive->ReloadArchive()) {
                RefreshFileList();
                ResetPreview();
                SetStatusMessage("Archive reloaded successfully.");
            } else {
                SetStatusMessage("Failed to reload archive.", 5.0f);
            }
            m_showReloadConfirmation = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (settings.Button("Cancel", Settings::ButtonVariant::Ghost, 100.0f)) {
            m_showReloadConfirmation = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (refreshRequested) {
        RefreshFileList();
    }

    DrawToastNotification();
    
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

    if (m_showArchiveStats) {
        ShowArchiveStats();
    }
    
    ImGui::End();
    if (!m_isVisible) {
        ResetPreview();
    }
}

bool ArchiveWindow::Initialize(const std::string& password) {
    std::cout << "---------- ARCHIVE WINDOW INITIALIZE ----------" << std::endl;
    std::cout << "Initializing archive for user: " << m_username << std::endl;
    
    bool success = false;
    bool createdNewArchive = false;
    
    // First try to load if archive exists
    if (m_archive->ArchiveExists()) {
        std::cout << "Archive exists, loading..." << std::endl;
        success = m_archive->LoadArchive(password);
        
        if (!success) {
            std::cout << "Loading failed; the existing archive was left untouched." << std::endl;
        }
    } else {
        std::cout << "Archive does not exist, creating new..." << std::endl;
        createdNewArchive = true;
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
        
        if (createdNewArchive) {
            SetStatusMessage("Created new archive successfully!");
        } else {
            SetStatusMessage("Archive loaded successfully!");
        }
    } else {
        m_isLoaded = false;
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

std::string ArchiveWindow::GetArchiveName() const {
    return m_archive ? m_archive->GetArchiveName() : std::string();
}

void ArchiveWindow::Show() {
    m_isVisible = true;
}

void ArchiveWindow::Hide() {
    m_isVisible = false;
    ResetPreview();
}

bool ArchiveWindow::IsVisible() const {
    return m_isVisible;
}

void ArchiveWindow::RefreshFileList() {
    if (!m_isLoaded) {
        return;
    }

    // GetFileList intentionally returns metadata only. Decrypted file contents stay
    // in one owner (CryptoArchive) and are copied only for an explicit operation.
    for (auto& entry : m_fileList) {
        SecureMemory::Cleanse(entry.data);
    }
    m_fileList = m_archive->GetFileList();
    m_fileList.erase(
        std::remove_if(m_fileList.begin(), m_fileList.end(),
                       [](const FileEntry& entry) {
                           return entry.name.empty() || entry.size == 0;
                       }),
        m_fileList.end());

    std::sort(m_fileList.begin(), m_fileList.end(), 
              [](const FileEntry& a, const FileEntry& b) {
                  return a.name < b.name;
              });

    m_selectedFile = -1;
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
    Settings& settings = Settings::Instance();
    const auto themeColors = settings.GetThemeColors();
    const auto& metrics = Settings::Metrics();

    if (!ImGui::IsPopupOpen("Add file")) {
        ImGui::OpenPopup("Add file");
    }
    ImGui::SetNextWindowSize(ImVec2(560.0f, 360.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                           ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Add file", &m_showAddFileDialog,
                               ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoSavedSettings)) {
        settings.DialogHeader(Settings::UiIcon::File, "Add file",
                              "Choose a file and the name stored inside the archive.");

        ImGui::TextUnformatted("Source file");
        const float browseWidth = 100.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x -
                                browseWidth - metrics.itemSpacing);
        ImGui::InputText("##addFilePath", m_filePathBuffer, sizeof(m_filePathBuffer));
        ImGui::SameLine();
        if (settings.IconButton("Browse", Settings::UiIcon::Folder,
                                Settings::ButtonVariant::Secondary,
                                browseWidth)) {
            IGFD::FileDialogConfig config;
            config.path = ".";
            config.flags = ImGuiFileDialogFlags_Modal;
            const char* filters =
                "All files (*.*){.*},Image files (*.png *.jpg *.jpeg *.bmp){.png,.jpg,.jpeg,.bmp},"
                "Text files (*.txt *.md){.txt,.md},Source files (*.cpp *.h){.cpp,.h}";
            ImGuiFileDialog::Instance()->OpenDialog(
                "FileOpenDialog", "Choose a file", filters, config);
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Name in archive");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##addFileName", "Uses the original filename when empty",
                                 m_fileNameBuffer, sizeof(m_fileNameBuffer));

        if (!m_addFileError.empty()) {
            const ImVec4 errorColor(themeColors.errorText[0], themeColors.errorText[1],
                                    themeColors.errorText[2], themeColors.errorText[3]);
            ImGui::Spacing();
            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                ImVec4(errorColor.x, errorColor.y, errorColor.z, 0.08f));
            ImGui::PushStyleColor(
                ImGuiCol_Border,
                ImVec4(errorColor.x, errorColor.y, errorColor.z, 0.45f));
            if (ImGui::BeginChild("AddFileError", ImVec2(0.0f, 58.0f), true,
                                  ImGuiWindowFlags_NoScrollbar)) {
                settings.DrawIcon(Settings::UiIcon::Error, errorColor, 18.0f);
                ImGui::SameLine(0.0f, metrics.itemSpacing);
                ImGui::TextWrapped("%s", m_addFileError.c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
        }

        const float footerY = ImGui::GetWindowHeight() - metrics.windowPadding -
                              metrics.buttonHeight;
        if (ImGui::GetCursorPosY() < footerY) {
            ImGui::SetCursorPosY(footerY);
        }
        const float footerWidth = 100.0f + 120.0f + metrics.itemSpacing;
        ImGui::SetCursorPosX(
            (ImGui::GetWindowWidth() - footerWidth) * 0.5f);

        const bool cancelRequested =
            settings.Button("Cancel", Settings::ButtonVariant::Ghost, 100.0f);
        ImGui::SameLine();
        const bool addRequested = settings.IconButton(
            "Add file", Settings::UiIcon::File,
            Settings::ButtonVariant::Primary, 120.0f);

        if (cancelRequested) {
            m_showAddFileDialog = false;
            m_addFileError.clear();
            memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
            memset(m_fileNameBuffer, 0, sizeof(m_fileNameBuffer));
            ImGui::CloseCurrentPopup();
        } else if (addRequested) {
            const std::string filePath(m_filePathBuffer);
            const std::string fileName(m_fileNameBuffer);
            std::error_code fileError;
            if (filePath.empty()) {
                m_addFileError = "Select a source file.";
            } else if (!std::filesystem::is_regular_file(filePath, fileError) || fileError) {
                m_addFileError = "The selected path is not a readable regular file.";
            } else if (!m_archive->AddFile(filePath, fileName)) {
                m_addFileError =
                    "The file could not be added. Check its name, size, and whether it already exists.";
            } else {
                RefreshFileList();
                SetStatusMessage("File added successfully.");
                m_showAddFileDialog = false;
                m_addFileError.clear();
                memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
                memset(m_fileNameBuffer, 0, sizeof(m_fileNameBuffer));
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    const ImVec2 dialogSize = GetStandardDialogSize();
    const ImVec2 dialogPos = GetStandardDialogPosition();
    if (ImGuiFileDialog::Instance()->IsOpened("FileOpenDialog")) {
        ImGui::SetNextWindowPos(dialogPos);
        ImGui::SetNextWindowSize(dialogSize);
    }
    if (ImGuiFileDialog::Instance()->Display(
            "FileOpenDialog",
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize,
            dialogSize, dialogPos)) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            const std::string selectedPath =
                ImGuiFileDialog::Instance()->GetFilePathName();
            const std::string selectedName =
                ImGuiFileDialog::Instance()->GetCurrentFileName();
            strncpy(m_filePathBuffer, selectedPath.c_str(),
                    sizeof(m_filePathBuffer) - 1);
            m_filePathBuffer[sizeof(m_filePathBuffer) - 1] = '\0';
            strncpy(m_fileNameBuffer, selectedName.c_str(),
                    sizeof(m_fileNameBuffer) - 1);
            m_fileNameBuffer[sizeof(m_fileNameBuffer) - 1] = '\0';
            m_addFileError.clear();
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (!m_showAddFileDialog &&
        !ImGuiFileDialog::Instance()->IsOpened("FileOpenDialog")) {
        m_addFileError.clear();
    }
}

void ArchiveWindow::ShowExtractDialog() {
    if (m_selectedFile < 0 || m_selectedFile >= static_cast<int>(m_fileList.size())) {
        m_showExtractDialog = false;
        return;
    }

    Settings& settings = Settings::Instance();
    const auto themeColors = settings.GetThemeColors();
    const auto& metrics = Settings::Metrics();
    const FileEntry& entry = m_fileList[m_selectedFile];

    if (!ImGui::IsPopupOpen("Extract file")) {
        ImGui::OpenPopup("Extract file");
    }
    ImGui::SetNextWindowSize(ImVec2(560.0f, 330.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                           ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Extract file", &m_showExtractDialog,
                               ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoSavedSettings)) {
        const std::string subtitle =
            entry.name + "  -  " + FormatFileSize(entry.size);
        settings.DialogHeader(Settings::UiIcon::Folder, "Extract file",
                              subtitle.c_str());

        ImGui::TextUnformatted("Destination");
        const float browseWidth = 100.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x -
                                browseWidth - metrics.itemSpacing);
        ImGui::InputText("##extractPath", m_extractPathBuffer,
                         sizeof(m_extractPathBuffer));
        ImGui::SameLine();
        if (settings.IconButton("Browse", Settings::UiIcon::Folder,
                                Settings::ButtonVariant::Secondary,
                                browseWidth)) {
            IGFD::FileDialogConfig config;
            config.path = std::filesystem::current_path().string();
            config.countSelectionMax = 1;
            config.flags = ImGuiFileDialogFlags_Modal |
                           ImGuiFileDialogFlags_DontShowHiddenFiles;
            ImGuiFileDialog::Instance()->OpenDialog(
                "ChooseFolderDlgKey", "Choose destination folder", nullptr, config);
        }
        ImGui::TextDisabled("Choose a folder or enter the complete destination filename.");

        if (!m_extractFileError.empty()) {
            const ImVec4 errorColor(themeColors.errorText[0], themeColors.errorText[1],
                                    themeColors.errorText[2], themeColors.errorText[3]);
            ImGui::Spacing();
            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                ImVec4(errorColor.x, errorColor.y, errorColor.z, 0.08f));
            ImGui::PushStyleColor(
                ImGuiCol_Border,
                ImVec4(errorColor.x, errorColor.y, errorColor.z, 0.45f));
            if (ImGui::BeginChild("ExtractFileError", ImVec2(0.0f, 58.0f), true,
                                  ImGuiWindowFlags_NoScrollbar)) {
                settings.DrawIcon(Settings::UiIcon::Error, errorColor, 18.0f);
                ImGui::SameLine(0.0f, metrics.itemSpacing);
                ImGui::TextWrapped("%s", m_extractFileError.c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
        }

        const float footerY = ImGui::GetWindowHeight() - metrics.windowPadding -
                              metrics.buttonHeight;
        if (ImGui::GetCursorPosY() < footerY) {
            ImGui::SetCursorPosY(footerY);
        }
        const float footerWidth = 100.0f + 120.0f + metrics.itemSpacing;
        ImGui::SetCursorPosX(
            (ImGui::GetWindowWidth() - footerWidth) * 0.5f);

        const bool cancelRequested =
            settings.Button("Cancel", Settings::ButtonVariant::Ghost, 100.0f);
        ImGui::SameLine();
        const bool extractRequested = settings.IconButton(
            "Extract", Settings::UiIcon::Folder,
            Settings::ButtonVariant::Primary, 120.0f);

        if (cancelRequested) {
            m_showExtractDialog = false;
            m_extractFileError.clear();
            ImGui::CloseCurrentPopup();
        } else if (extractRequested) {
            const std::string destination(m_extractPathBuffer);
            if (destination.empty()) {
                m_extractFileError = "Choose an extraction destination.";
            } else if (!m_archive->ExtractFile(entry.name, destination)) {
                m_extractFileError =
                    "The file could not be extracted to this destination.";
            } else {
                m_showExtractDialog = false;
                m_extractFileError.clear();
                SetStatusMessage("File extracted successfully.");
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    const ImVec2 dialogSize = GetStandardDialogSize();
    const ImVec2 dialogPos = GetStandardDialogPosition();
    if (ImGuiFileDialog::Instance()->IsOpened("ChooseFolderDlgKey")) {
        ImGui::SetNextWindowPos(dialogPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(dialogSize, ImGuiCond_Always);
    }
    if (ImGuiFileDialog::Instance()->Display(
            "ChooseFolderDlgKey",
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize,
            dialogSize, dialogPos)) {
        if (ImGuiFileDialog::Instance()->IsOk() &&
            m_selectedFile >= 0 &&
            m_selectedFile < static_cast<int>(m_fileList.size())) {
            const std::filesystem::path destination =
                std::filesystem::path(
                    ImGuiFileDialog::Instance()->GetCurrentPath()) /
                m_fileList[m_selectedFile].name;
            const std::string destinationText = destination.string();
            strncpy(m_extractPathBuffer, destinationText.c_str(),
                    sizeof(m_extractPathBuffer) - 1);
            m_extractPathBuffer[sizeof(m_extractPathBuffer) - 1] = '\0';
            m_extractFileError.clear();
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (!m_showExtractDialog &&
        !ImGuiFileDialog::Instance()->IsOpened("ChooseFolderDlgKey")) {
        m_extractFileError.clear();
    }
}
void ArchiveWindow::ShowFileViewer() {
    std::cout << "ShowFileViewer called, preview type: " << 
        (m_previewType == PreviewType::TEXT ? "TEXT" : 
         m_previewType == PreviewType::IMAGE ? "IMAGE" : "NONE") << std::endl;
    
    // Check if we have data to display
    if (m_previewType == PreviewType::TEXT && !m_textPreviewData.empty()) {
        // Convert binary data to text
        std::string text;
        SecureMemory::ScopedCleanse textGuard(text);
        
        // Add a null terminator to ensure the text is valid
        std::vector<uint8_t> textData = m_textPreviewData;
        SecureMemory::ScopedCleanse textDataGuard(textData);
        textData.push_back(0); // null terminator
        
        // Convert to character string
        text.assign(reinterpret_cast<const char*>(textData.data()),
                    m_textPreviewData.size());
        
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
    if (!m_archive) {
        m_showArchiveStats = false;
        return;
    }

    Settings& settings = Settings::Instance();
    const auto themeColors = settings.GetThemeColors();
    const auto& metrics = Settings::Metrics();
    const ImVec4 accent(themeColors.accentText[0], themeColors.accentText[1],
                        themeColors.accentText[2], themeColors.accentText[3]);
    const ImVec4 success(themeColors.successText[0], themeColors.successText[1],
                         themeColors.successText[2], themeColors.successText[3]);
    const ImVec4 cardBackground(themeColors.surfaceElevated[0],
                                themeColors.surfaceElevated[1],
                                themeColors.surfaceElevated[2],
                                themeColors.surfaceElevated[3]);

    const auto stats = m_archive->GetStats();
    const auto files = m_archive->GetFileList();
    const std::string archiveName = m_archive->GetArchiveName();
    const std::string archivePath = m_archive->GetArchiveFilePath();
    const size_t averageSize = stats.totalFiles == 0
        ? 0
        : stats.totalSize / stats.totalFiles;

    const FileEntry* largestFile = nullptr;
    for (const auto& file : files) {
        if (largestFile == nullptr || file.size > largestFile->size) {
            largestFile = &file;
        }
    }

    struct CategoryStats {
        std::string name;
        size_t count = 0;
        size_t bytes = 0;
    };
    std::vector<CategoryStats> categories = {
        {"Images"}, {"Text"}, {"Code"}, {"Documents"}, {"Media"},
        {"Compressed"}, {"Web"}, {"Other"}
    };
    for (const auto& file : files) {
        const std::string type = GetFileTypeIcon(file.name);
        size_t categoryIndex = 7;
        if (type == "[IMG]") {
            categoryIndex = 0;
        } else if (type == "[TXT]") {
            categoryIndex = 1;
        } else if (type == "[CODE]") {
            categoryIndex = 2;
        } else if (type == "[PDF]" || type == "[DOC]" || type == "[XLS]" ||
                   type == "[PPT]") {
            categoryIndex = 3;
        } else if (type == "[AUD]" || type == "[VID]") {
            categoryIndex = 4;
        } else if (type == "[ZIP]") {
            categoryIndex = 5;
        } else if (type == "[WEB]") {
            categoryIndex = 6;
        }
        ++categories[categoryIndex].count;
        categories[categoryIndex].bytes += file.size;
    }

    if (!ImGui::IsPopupOpen("Archive statistics")) {
        ImGui::OpenPopup("Archive statistics");
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float maximumWidth = std::max(360.0f, viewport->WorkSize.x - 48.0f);
    const float maximumHeight = std::max(420.0f, viewport->WorkSize.y - 48.0f);
    const ImVec2 dialogSize(std::min(720.0f, maximumWidth),
                            std::min(620.0f, maximumHeight));
    const ImVec2 minimumSize(std::min(560.0f, maximumWidth),
                             std::min(500.0f, maximumHeight));
    ImGui::SetNextWindowSize(dialogSize, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(minimumSize,
                                        ImVec2(maximumWidth, maximumHeight));
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                           ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Archive statistics", &m_showArchiveStats,
                               ImGuiWindowFlags_NoSavedSettings)) {
        settings.DialogHeader(Settings::UiIcon::Archive, archiveName.c_str(),
                              "Storage and content overview");
        settings.DrawIcon(Settings::UiIcon::Success, success, 18.0f);
        ImGui::SameLine(0.0f, metrics.itemSpacing);
        ImGui::TextColored(success, "Authenticated archive");
        ImGui::Spacing();

        if (ImGui::BeginChild(
                "ArchiveStatsContent",
                ImVec2(0.0f, -(metrics.buttonHeight + metrics.itemSpacing * 2.0f)),
                false)) {

        const float summaryWidth = (ImGui::GetContentRegionAvail().x -
                                    metrics.itemSpacing * 2.0f) / 3.0f;
        const auto drawSummary = [&](const char* id, const char* label,
                                     const std::string& value) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, cardBackground);
            if (ImGui::BeginChild(id, ImVec2(summaryWidth, 92.0f), true,
                                  ImGuiWindowFlags_NoScrollbar)) {
                ImGui::TextDisabled("%s", label);
                ImGui::Spacing();
                ImGui::TextColored(accent, "%s", value.c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        };

        drawSummary("StatsFiles", "FILES", std::to_string(stats.totalFiles));
        ImGui::SameLine();
        drawSummary("StatsSize", "TOTAL SIZE", FormatFileSize(stats.totalSize));
        ImGui::SameLine();
        drawSummary("StatsAverage", "AVERAGE SIZE", FormatFileSize(averageSize));

        ImGui::Spacing();
        ImGui::TextUnformatted("Details");
        ImGui::Separator();
        ImGui::TextDisabled("Largest file");
        if (largestFile != nullptr) {
            ImGui::SameLine(150.0f);
            ImGui::Text("%s  (%s)", largestFile->name.c_str(),
                        FormatFileSize(largestFile->size).c_str());
        } else {
            ImGui::SameLine(150.0f);
            ImGui::TextUnformatted("None");
        }
        ImGui::TextDisabled("Last modified");
        ImGui::SameLine(150.0f);
        ImGui::TextUnformatted(stats.lastModified.c_str());
        ImGui::TextDisabled("Protection");
        ImGui::SameLine(150.0f);
        ImGui::TextUnformatted("AES-256-GCM authenticated encryption");

        ImGui::Spacing();
        ImGui::TextUnformatted("File types");
        ImGui::Separator();
        if (stats.totalFiles == 0) {
            ImGui::TextDisabled("No files in this archive.");
        } else if (ImGui::BeginTable(
                       "ArchiveTypeStats", 3,
                       ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                           ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableHeadersRow();
            for (const auto& category : categories) {
                if (category.count == 0) {
                    continue;
                }
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(category.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%zu", category.count);
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(FormatFileSize(category.bytes).c_str());
            }
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Archive path");
        ImGui::TextWrapped("%s", archivePath.c_str());

        }
        ImGui::EndChild();
        ImGui::Separator();

        const float closeWidth = 100.0f;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - closeWidth - metrics.windowPadding);
        if (settings.Button("Close", Settings::ButtonVariant::Primary, closeWidth)) {
            m_showArchiveStats = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ArchiveWindow::HandleDragDrop() {
    const float deltaTime = ImGui::GetIO().DeltaTime;
    m_dropFeedbackTime = std::max(0.0f, m_dropFeedbackTime - deltaTime);

    const std::vector<FileDropEvent> dropEvents = FileDropQueue::ConsumeAll();
    std::size_t addedCount = 0;
    std::size_t skippedCount = 0;
    std::string firstFailure;
    std::string firstAddedName;
    std::vector<std::string> knownNames;
    knownNames.reserve(m_fileList.size());
    for (const FileEntry& entry : m_fileList) {
        knownNames.push_back(entry.name);
    }

    const bool popupOpen =
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    for (const FileDropEvent& event : dropEvents) {
        const bool insideDropZone = !popupOpen && m_dropZoneValid &&
            event.cursorX >= static_cast<double>(m_dropZoneMin.x) &&
            event.cursorX <= static_cast<double>(m_dropZoneMax.x) &&
            event.cursorY >= static_cast<double>(m_dropZoneMin.y) &&
            event.cursorY <= static_cast<double>(m_dropZoneMax.y);

        if (!insideDropZone) {
            skippedCount += event.paths.size();
            if (firstFailure.empty()) {
                firstFailure = popupOpen
                    ? "close the open dialog before adding files"
                    : "drop files inside the archive list";
            }
            continue;
        }

        m_dropFeedbackTime = 0.8f;
        for (const std::filesystem::path& path : event.paths) {
            std::error_code fileError;
            if (!std::filesystem::is_regular_file(path, fileError) || fileError) {
                ++skippedCount;
                if (firstFailure.empty()) {
                    firstFailure = "only regular files can be added";
                }
                continue;
            }

            const std::string fileName = path.filename().u8string();
            std::string validationError;
            if (!PathSecurity::ValidateStoredFilename(fileName, &validationError)) {
                ++skippedCount;
                if (firstFailure.empty()) {
                    firstFailure = validationError.empty()
                        ? "a filename is invalid"
                        : validationError;
                }
                continue;
            }

            const bool alreadyExists = std::any_of(
                knownNames.begin(), knownNames.end(),
                [&fileName](const std::string& existingName) {
                    return PathSecurity::NamesCollide(existingName, fileName);
                });
            if (alreadyExists) {
                ++skippedCount;
                if (firstFailure.empty()) {
                    firstFailure = "a file with the same name already exists";
                }
                continue;
            }

            if (!m_archive || !m_isLoaded ||
                !m_archive->AddFile(path.u8string(), fileName)) {
                ++skippedCount;
                if (firstFailure.empty()) {
                    firstFailure = "the archive could not store a file";
                }
                continue;
            }

            if (firstAddedName.empty()) {
                firstAddedName = fileName;
            }
            knownNames.push_back(fileName);
            ++addedCount;
        }
    }

    if (addedCount > 0) {
        RefreshFileList();
        if (!firstAddedName.empty()) {
            const auto selected = std::find_if(
                m_fileList.begin(), m_fileList.end(),
                [&firstAddedName](const FileEntry& entry) {
                    return entry.name == firstAddedName;
                });
            if (selected != m_fileList.end()) {
                m_selectedFile = static_cast<int>(
                    std::distance(m_fileList.begin(), selected));
            }
        }

        if (skippedCount == 0) {
            SetStatusMessage(
                std::to_string(addedCount) +
                (addedCount == 1 ? " file added successfully."
                                 : " files added successfully."));
        } else {
            SetStatusMessage(
                "Added " + std::to_string(addedCount) + ", skipped " +
                std::to_string(skippedCount) + ": " + firstFailure + ".",
                5.0f);
        }
    } else if (skippedCount > 0) {
        SetStatusMessage("File drop failed: " + firstFailure + ".", 5.0f);
    }

    if (m_dropFeedbackTime > 0.0f && m_dropZoneValid) {
        const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        const float alpha = std::min(1.0f, m_dropFeedbackTime * 2.5f);
        ImVec4 borderColor = accent;
        borderColor.w *= alpha;
        ImGui::GetWindowDrawList()->AddRect(
            m_dropZoneMin, m_dropZoneMax, ImGui::ColorConvertFloat4ToU32(borderColor),
            Settings::Metrics().frameRounding, 0, 2.0f);
    }
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
    m_statusMessageDuration = std::max(0.1f, duration);
    m_statusMessageTime = m_statusMessageDuration;

    std::string normalized = message;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    if (normalized.find("fail") != std::string::npos ||
        normalized.find("cannot") != std::string::npos ||
        normalized.find("invalid") != std::string::npos ||
        normalized.find("wrong") != std::string::npos) {
        m_statusMessageKind = NotificationKind::Error;
    } else if (normalized.find("please") != std::string::npos ||
               normalized.find("not available") != std::string::npos ||
               normalized.find("empty") != std::string::npos ||
               normalized.find("skipped") != std::string::npos) {
        m_statusMessageKind = NotificationKind::Warning;
    } else if (normalized.find("success") != std::string::npos ||
               normalized.find("verified") != std::string::npos ||
               normalized.find("loaded") != std::string::npos ||
               normalized.find("created") != std::string::npos ||
               normalized.find("saved") != std::string::npos ||
               normalized.find("repaired") != std::string::npos ||
               normalized.find("copied") != std::string::npos ||
               normalized.find("extracted") != std::string::npos ||
               normalized.find("removed") != std::string::npos) {
        m_statusMessageKind = NotificationKind::Success;
    } else {
        m_statusMessageKind = NotificationKind::Info;
    }
}

void ArchiveWindow::UpdateStatusMessage() {
    if (m_statusMessageTime > 0.0f) {
        m_statusMessageTime -= ImGui::GetIO().DeltaTime;
        if (m_statusMessageTime <= 0.0f) {
            m_statusMessage.clear();
            m_statusMessageDuration = 0.0f;
        }
    }
}

void ArchiveWindow::DrawToastNotification() {
    if (m_statusMessage.empty() || m_statusMessageTime <= 0.0f ||
        m_statusMessageDuration <= 0.0f) {
        return;
    }

    const float elapsed = m_statusMessageDuration - m_statusMessageTime;
    float alpha = std::min(1.0f, elapsed / 0.18f);
    alpha = std::min(alpha, std::min(1.0f, m_statusMessageTime / 0.30f));
    alpha = alpha * alpha * (3.0f - 2.0f * alpha);

    Settings& settings = Settings::Instance();
    const auto colors = settings.GetThemeColors();
    const auto& metrics = Settings::Metrics();
    ImVec4 accent(colors.infoText[0], colors.infoText[1], colors.infoText[2], alpha);
    Settings::UiIcon icon = Settings::UiIcon::Info;
    const char* kindLabel = "Notice";
    if (m_statusMessageKind == NotificationKind::Success) {
        accent = ImVec4(colors.successText[0], colors.successText[1],
                        colors.successText[2], alpha);
        icon = Settings::UiIcon::Success;
        kindLabel = "Success";
    } else if (m_statusMessageKind == NotificationKind::Warning) {
        accent = ImVec4(colors.warningText[0], colors.warningText[1],
                        colors.warningText[2], alpha);
        icon = Settings::UiIcon::Warning;
        kindLabel = "Attention";
    } else if (m_statusMessageKind == NotificationKind::Error) {
        accent = ImVec4(colors.errorText[0], colors.errorText[1],
                        colors.errorText[2], alpha);
        icon = Settings::UiIcon::Error;
        kindLabel = "Error";
    }

    const float toastWidth = std::min(370.0f,
        std::max(260.0f, ImGui::GetWindowWidth() - metrics.windowPadding * 2.0f));
    const float textWidth = toastWidth - 72.0f;
    const ImVec2 messageSize = ImGui::CalcTextSize(
        m_statusMessage.c_str(), nullptr, false, textWidth);
    const float toastHeight = std::max(68.0f,
        messageSize.y + ImGui::GetTextLineHeight() + metrics.windowPadding);
    const float slideOffset = (1.0f - alpha) * 18.0f;
    const ImVec2 windowPosition = ImGui::GetWindowPos();
    const ImVec2 toastPosition(
        windowPosition.x + ImGui::GetWindowWidth() - toastWidth - metrics.windowPadding +
            slideOffset,
        windowPosition.y + 72.0f);
    const ImVec2 toastMaximum(toastPosition.x + toastWidth,
                              toastPosition.y + toastHeight);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec4 background(colors.surfaceElevated[0], colors.surfaceElevated[1],
                            colors.surfaceElevated[2], 0.97f * alpha);
    const ImVec4 textColor(colors.primaryText[0], colors.primaryText[1],
                           colors.primaryText[2], alpha);
    drawList->AddRectFilled(toastPosition, toastMaximum,
                            ImGui::GetColorU32(background), metrics.childRounding);
    drawList->AddRect(toastPosition, toastMaximum, ImGui::GetColorU32(accent),
                      metrics.childRounding, 0, 1.0f);
    drawList->AddRectFilled(
        toastPosition, ImVec2(toastPosition.x + 4.0f, toastMaximum.y),
        ImGui::GetColorU32(accent), metrics.childRounding,
        ImDrawFlags_RoundCornersLeft);
    settings.DrawIconAt(icon, accent,
                        ImVec2(toastPosition.x + 18.0f, toastPosition.y + 20.0f),
                        22.0f);

    const ImVec2 labelPosition(toastPosition.x + 54.0f,
                               toastPosition.y + 13.0f);
    const ImVec2 messagePosition(labelPosition.x,
                                 labelPosition.y + ImGui::GetTextLineHeight() + 3.0f);
    drawList->AddText(labelPosition, ImGui::GetColorU32(accent), kindLabel);
    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), messagePosition,
                      ImGui::GetColorU32(textColor), m_statusMessage.c_str(), nullptr,
                      textWidth);
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
    SecureMemory::Cleanse(m_imagePreviewData);
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
    SecureMemory::Cleanse(m_textPreviewData);
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
    
    // Extract file data into memory
    std::vector<uint8_t> fileData;
    SecureMemory::ScopedCleanse fileDataGuard(fileData);
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
    static bool showCopySuccessMsg = false;
    static float copyMsgTimer = 0.0f;
    
    // Get theme-appropriate colors
    Settings& settings = Settings::Instance();
    auto themeColors = settings.GetThemeColors();
    
    std::vector<char> buffer(text.begin(), text.end());
    buffer.push_back('\0');
    SecureMemory::ScopedCleanse bufferGuard(buffer);
    
    // Display text as readonly input that allows selection with theme-appropriate background
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(themeColors.accentText[0] * 0.1f, themeColors.accentText[1] * 0.1f, themeColors.accentText[2] * 0.1f, 0.5f));
    ImGui::InputTextMultiline("##TextPreviewContent", 
                             buffer.data(),
                             buffer.size(),
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
    bool success = false;
    
    // Log the expected file path for debugging
    std::string expectedPath = m_archive->GetArchiveFilePath();
    std::cout << "Expected archive file path: " << expectedPath << std::endl;
    std::cout << "File exists check: " << (std::filesystem::exists(expectedPath) ? "Yes" : "No") << std::endl;
    
    // First try to load if archive exists
    if (m_archive->ArchiveExists()) {
        std::cout << "Archive exists, loading..." << std::endl;
        success = m_archive->LoadArchive(password);
        
        if (success) {
            std::cout << "Successfully loaded archive: " << archiveName << std::endl;
            m_isLoaded = true;
            // Reset UI state for the new archive
            m_selectedFile = -1; // Reset selected file
            ResetPreview();
            m_previewType = PreviewType::NONE; // Reset preview type
            RefreshFileList();
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
