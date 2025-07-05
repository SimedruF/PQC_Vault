#pragma once

#include <imgui.h>

// Anti-Flickering Dialog Helper
// Acest fișier conține utilitar pentru rezolvarea problemei de flickering în ImGuiFileDialog

namespace DialogHelper {

    // Obține dimensiunea standard pentru dialog (80% din ecran)
    inline ImVec2 GetStandardDialogSize() {
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        return ImVec2(displaySize.x * 0.8f, displaySize.y * 0.8f);
    }
    
    // Obține poziția standard pentru dialog (10% de la margini)
    inline ImVec2 GetStandardDialogPosition() {
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        return ImVec2(displaySize.x * 0.1f, displaySize.y * 0.1f);
    }
    
    // Funcție helper pentru a stabili o dimensiune și poziție fixă pentru dialog
    inline void SetupNextWindowForDialog() {
        ImVec2 dialogSize = GetStandardDialogSize();
        ImVec2 dialogPos = GetStandardDialogPosition();
        
        // Setează poziția și dimensiunea ferestrei pentru a preveni flickering-ul
        ImGui::SetNextWindowPos(dialogPos);
        ImGui::SetNextWindowSize(dialogSize);
    }
    
    // Funcție helper pentru configurarea unui dialog ImGuiFileDialog
    template<typename ConfigType>
    inline void ConfigureFileDialogForStability(ConfigType& config) {
        // Setează flag-urile necesare pentru a preveni flickering-ul
        config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_NoResize;
    }
    
    // Flags recomandate pentru afișarea dialogului
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

// Înainte de afișarea dialogului
if (ImGuiFileDialog::Instance()->IsOpened("DialogId")) {
    DialogHelper::SetupNextWindowForDialog();
}

// La afișarea dialogului
ImVec2 dialogSize = DialogHelper::GetStandardDialogSize();
ImVec2 dialogPos = DialogHelper::GetStandardDialogPosition();
if (ImGuiFileDialog::Instance()->Display("DialogId", 
                                      DialogHelper::GetRecommendedDialogFlags(), 
                                      dialogSize, 
                                      dialogPos)) {
    // Procesează rezultatul...
    ImGuiFileDialog::Instance()->Close();
}
*/
