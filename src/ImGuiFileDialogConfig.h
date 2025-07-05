#pragma once

// Custom ImGuiFileDialog configuration
// Include this before including ImGuiFileDialog.h

// Enable optional features
#define USE_THUMBNAILS
#define USE_BOOKMARK
#define USE_EXPLORATION_BY_KEYS

// Set config flags
#define IMGUI_TOGGLE_BUTTON ToggleButton
#define fileNameString std::string
#define PathString std::string

// Dezactivează redimensionarea dialogului pentru a preveni flickering-ul
#define ImGuiFileDialogFlags_NoResize ImGuiWindowFlags_NoResize

// Folosim flag-uri suplimentare pentru a stabiliza dialogul
#define IGFD_DEFAULT_WINDOW_FLAGS (ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)
