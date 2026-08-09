#include "CryptoArchive.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
    }
    return condition;
}

bool WriteBytes(const std::filesystem::path& path,
                const std::vector<std::uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!bytes.empty()) {
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }
    return file.good();
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    const fs::path originalPath = fs::current_path();
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
                          ("pqcwallet_archive_boundaries_" + std::to_string(suffix));
    bool success = true;

    try {
        fs::create_directories(root);
        fs::current_path(root);

        const fs::path emptyPath = root / "empty.bin";
        const fs::path largePath = root / "large.bin";
        const std::vector<std::uint8_t> empty;
        std::vector<std::uint8_t> large(8U * 1024U * 1024U);
        for (std::size_t i = 0; i < large.size(); ++i) {
            large[i] = static_cast<std::uint8_t>((i * 131U + 17U) & 0xffU);
        }
        success &= Expect(WriteBytes(emptyPath, empty), "create empty input file");
        success &= Expect(WriteBytes(largePath, large), "create representative large file");

        const std::string password = "archive boundary password";
        CryptoArchive writer("limits", "security");
        success &= Expect(writer.InitializeArchive(password), "initialize boundary archive");
        success &= Expect(writer.AddFile(emptyPath.string(), "empty.bin"),
                          "store an empty file");
        success &= Expect(writer.AddFile(largePath.string(), "large.bin"),
                          "store an 8 MiB file");

        CryptoArchive reader("limits", "security");
        success &= Expect(reader.LoadArchive(password), "reload large archive");
        std::vector<std::uint8_t> extractedEmpty{0xff};
        success &= Expect(reader.ExtractFileToMemory("empty.bin", extractedEmpty) &&
                              extractedEmpty.empty(),
                          "round-trip empty file in memory");
        success &= Expect(reader.GetFileData("large.bin") == large,
                          "round-trip large file byte-for-byte");

        const fs::path extractionRoot = root / "extracted";
        fs::create_directories(extractionRoot);
        success &= Expect(reader.ExtractFile("empty.bin", extractionRoot.string()),
                          "extract empty file atomically");
        std::error_code sizeError;
        const auto emptySize = fs::file_size(extractionRoot / "empty.bin", sizeError);
        success &= Expect(!sizeError && emptySize == 0,
                          "empty extracted file has zero bytes");
    } catch (const std::exception& error) {
        std::cerr << "FAILED with exception: " << error.what() << std::endl;
        success = false;
    }

    fs::current_path(originalPath);
    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);
    return success ? 0 : 1;
}
