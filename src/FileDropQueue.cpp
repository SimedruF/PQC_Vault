#include "FileDropQueue.h"

#include <algorithm>
#include <mutex>
#include <utility>

namespace {

constexpr int kMaximumPathsPerDrop = 256;
constexpr std::size_t kMaximumPendingDrops = 16;
std::mutex g_dropMutex;
std::vector<FileDropEvent> g_pendingDrops;

} // namespace

namespace FileDropQueue {

void Push(double cursorX, double cursorY, int count, const char* const* paths) {
    if (count <= 0 || paths == nullptr) {
        return;
    }

    FileDropEvent event;
    event.cursorX = cursorX;
    event.cursorY = cursorY;
    event.paths.reserve(static_cast<std::size_t>(
        std::min(count, kMaximumPathsPerDrop)));

    for (int index = 0; index < count && index < kMaximumPathsPerDrop; ++index) {
        if (paths[index] != nullptr && paths[index][0] != '\0') {
            event.paths.emplace_back(std::filesystem::u8path(paths[index]));
        }
    }

    if (event.paths.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_dropMutex);
    if (g_pendingDrops.size() >= kMaximumPendingDrops) {
        g_pendingDrops.erase(g_pendingDrops.begin());
    }
    g_pendingDrops.push_back(std::move(event));
}

std::vector<FileDropEvent> ConsumeAll() {
    std::lock_guard<std::mutex> lock(g_dropMutex);
    std::vector<FileDropEvent> result;
    result.swap(g_pendingDrops);
    return result;
}

void Clear() {
    std::lock_guard<std::mutex> lock(g_dropMutex);
    g_pendingDrops.clear();
}

} // namespace FileDropQueue
