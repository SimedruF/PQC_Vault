#pragma once

#include <imgui.h>

// Anti-Flickering Dialog Helper
// This file contains utilities for solving the flickering problem in ImGuiFileDialog

namespace DialogHelper {

    // Get standard dialog size (80% of screen)
    inline ImVec2 GetStandardDialogSize() {
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        return ImVec2(displaySize.x * 0.8f, displaySize.y * 0.8f);
    }
    
    // Get standard dialog position (10% from edges)
    inline ImVec2 GetStandardDialogPosition() {
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        return ImVec2(displaySize.x * 0.1f, displaySize.y * 0.1f);
    }
    
    // Helper function to establish a fixed size and position for dialog
    inline void SetupNextWindowForDialog() {
        ImVec2 dialogSize = GetStandardDialogSize();
        ImVec2 dialogPos = GetStandardDialogPosition();
        
        // Set the position and size of the window to prevent flickering
        ImGui::SetNextWindowPos(dialogPos);
        ImGui::SetNextWindowSize(dialogSize);
    }
    
    // Helper function for configuring an ImGuiFileDialog
    template<typename ConfigType>
    inline void ConfigureFileDialogForStability(ConfigType& config) {
        // Set the necessary flags to prevent flickering
        config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_NoResize;
    }
    
    // Recommended flags for displaying the dialog
    inline int GetRecommendedDialogFlags() {
        return ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    }
}

/*
Exemplu de utilizare:

// La deschiderea dialogului
IGFD::FileDialogConfig config;
config.path = ".";
DialogHelper::ConfigureFileDialogForStability(config);
ImGuiFileDialog::Instance()->OpenDialog("DialogId", "Titlu", "filtre", config);

// Before displaying the dialog
if (ImGuiFileDialog::Instance()->IsOpened("DialogId")) {
    DialogHelper::SetupNextWindowForDialog();
}

// When displaying the dialog
ImVec2 dialogSize = DialogHelper::GetStandardDialogSize();
ImVec2 dialogPos = DialogHelper::GetStandardDialogPosition();
if (ImGuiFileDialog::Instance()->Display("DialogId", 
                                      DialogHelper::GetRecommendedDialogFlags(), 
                                      dialogSize, 
                                      dialogPos)) {
    // Process the result...
    ImGuiFileDialog::Instance()->Close();
}
*/
