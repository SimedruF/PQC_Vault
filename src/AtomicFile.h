#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace AtomicFile {

// Writes into a private temporary file in the same directory, synchronizes it,
// and only then atomically replaces the destination.
bool Write(const std::filesystem::path& destination,
           const uint8_t* data,
           size_t size);

inline bool Write(const std::filesystem::path& destination,
                  const std::vector<uint8_t>& data) {
    return Write(destination, data.data(), data.size());
}

inline bool Write(const std::filesystem::path& destination,
                  const std::string& data) {
    return Write(destination,
                 reinterpret_cast<const uint8_t*>(data.data()),
                 data.size());
}

// Renames a regular file within one directory without replacing an existing
// destination. The operation is atomic on supported platforms and preserves
// the source if the destination already exists.
bool RenameNoReplace(const std::filesystem::path& source,
                     const std::filesystem::path& destination);

namespace Testing {

// One-shot fault injection used to prove that an interrupted write leaves the
// old destination untouched. It is intentionally not used by production code.
void FailNextWriteBeforeReplace();

// Simulates an inability to create the temporary file (for example a
// permission error or exhausted storage) before any destination is changed.
void FailNextWriteBeforeTemporaryCreate();

} // namespace Testing
} // namespace AtomicFile
