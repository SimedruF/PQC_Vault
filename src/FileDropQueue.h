#pragma once

#include <filesystem>
#include <vector>

struct FileDropEvent {
    double cursorX = 0.0;
    double cursorY = 0.0;
    std::vector<std::filesystem::path> paths;
};

namespace FileDropQueue {

// GLFW delivers native file drops from a callback. Keep the callback small and
// let the regular ImGui frame validate and import the files.
void Push(double cursorX, double cursorY, int count, const char* const* paths);
std::vector<FileDropEvent> ConsumeAll();
void Clear();

} // namespace FileDropQueue
