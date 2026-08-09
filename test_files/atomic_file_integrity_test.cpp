#include "AtomicFile.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
    }
    return condition;
}

std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
}

} // namespace

int main() {
    namespace fs = std::filesystem;

    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path testRoot = fs::temp_directory_path() /
                              ("pqcwallet_atomic_file_" + std::to_string(suffix));
    const fs::path destination = testRoot / "vault.enc";
    bool success = true;

    try {
        fs::create_directories(testRoot);

        success &= Expect(AtomicFile::Write(destination, std::string("old-complete-data")),
                          "create initial file");
        const std::string original = ReadAll(destination);

        AtomicFile::Testing::FailNextWriteBeforeReplace();
        success &= Expect(!AtomicFile::Write(destination, std::string("new-data")),
                          "simulate failure before replacement");
        success &= Expect(ReadAll(destination) == original,
                          "failed write preserves the old destination byte-for-byte");

        size_t temporaryFileCount = 0;
        for (const auto& entry : fs::directory_iterator(testRoot)) {
            if (entry.path().filename().string().find("vault.enc.tmp.") == 0) {
                ++temporaryFileCount;
            }
        }
        success &= Expect(temporaryFileCount == 0, "failed write cleans its temporary file");

        success &= Expect(AtomicFile::Write(destination, std::string("new-complete-data")),
                          "replace file atomically");
        success &= Expect(ReadAll(destination) == "new-complete-data",
                          "successful write publishes all new bytes");

#ifndef _WIN32
        const auto permissions = fs::status(destination).permissions();
        const auto groupOrOther = fs::perms::group_all | fs::perms::others_all;
        success &= Expect((permissions & groupOrOther) == fs::perms::none,
                          "destination is private to its owner");
#endif
    } catch (const std::exception& error) {
        std::cerr << "FAILED with exception: " << error.what() << std::endl;
        success = false;
    }

    std::error_code cleanupError;
    fs::remove_all(testRoot, cleanupError);
    return success ? 0 : 1;
}
