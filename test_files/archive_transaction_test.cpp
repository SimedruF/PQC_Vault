#include "AtomicFile.h"
#include "CryptoArchive.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <openssl/evp.h>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
    }
    return condition;
}

std::vector<std::uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

bool WritePayload(const std::filesystem::path& path,
                  const std::vector<std::uint8_t>& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    return file.good();
}

std::size_t CountTemporaryFiles(const std::filesystem::path& root) {
    std::size_t count = 0;
    std::error_code error;
    for (std::filesystem::recursive_directory_iterator iterator(root, error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (iterator->path().filename().string().find(".tmp.") != std::string::npos) {
            ++count;
        }
    }
    return error ? static_cast<std::size_t>(-1) : count;
}

template <typename Integer>
void AppendNative(std::vector<std::uint8_t>& output, Integer value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    output.insert(output.end(), bytes, bytes + sizeof(value));
}

bool WriteLegacyRepairFixture(const std::filesystem::path& path,
                              const std::string& password) {
    const std::string name = "damaged.bin";
    const std::vector<std::uint8_t> data = {0x31, 0x32, 0x33};
    const std::string invalidHash = "invalid-hash";

    std::vector<std::uint8_t> plaintext;
    AppendNative(plaintext, std::uint32_t{1});
    AppendNative(plaintext, static_cast<std::uint32_t>(name.size()));
    plaintext.insert(plaintext.end(), name.begin(), name.end());
    AppendNative(plaintext, static_cast<std::uint64_t>(data.size()));
    plaintext.insert(plaintext.end(), data.begin(), data.end());
    AppendNative(plaintext, std::uint32_t{0});
    AppendNative(plaintext, static_cast<std::uint32_t>(invalidHash.size()));
    plaintext.insert(plaintext.end(), invalidHash.begin(), invalidHash.end());

    std::array<std::uint8_t, 32> key{};
    unsigned int keySize = 0;
    if (EVP_Digest(password.data(), password.size(), key.data(), &keySize,
                   EVP_sha256(), nullptr) != 1 || keySize != key.size()) {
        return false;
    }
    for (std::size_t i = 0; i < plaintext.size(); ++i) {
        plaintext[i] ^= key[i % key.size()];
    }

    const std::uint64_t encryptedSize = plaintext.size();
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write("PQCENC01", 8);
    file.write(reinterpret_cast<const char*>(&encryptedSize), sizeof(encryptedSize));
    file.write(reinterpret_cast<const char*>(plaintext.data()),
               static_cast<std::streamsize>(plaintext.size()));
    return file.good();
}

} // namespace

int main() {
    namespace fs = std::filesystem;

    const fs::path originalPath = fs::current_path();
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path testRoot = fs::temp_directory_path() /
        ("pqcwallet_archive_transaction_" + std::to_string(suffix));
    bool success = true;

    try {
        fs::create_directories(testRoot);
        fs::current_path(testRoot);

        const std::string password = "transaction test password";
        const fs::path firstPayloadPath = testRoot / "first.bin";
        const fs::path replacementPath = testRoot / "replacement.bin";
        const std::vector<std::uint8_t> firstPayload = {1, 2, 3};
        const std::vector<std::uint8_t> replacementPayload = {9, 8, 7, 6};
        success &= Expect(WritePayload(firstPayloadPath, firstPayload),
                          "write first payload fixture");
        success &= Expect(WritePayload(replacementPath, replacementPayload),
                          "write replacement payload fixture");

        CryptoArchive archive("alice", "transactions");
        success &= Expect(archive.InitializeArchive(password), "create transaction archive");
        success &= Expect(archive.AddFile(firstPayloadPath.string(), "record.bin"),
                          "add initial record");
        const fs::path archivePath = testRoot / "archives/alice_transactions.enc";
        const fs::path lockPath = testRoot / "archives/alice_transactions.enc.lock";
        const std::vector<std::uint8_t> lockMarker = {'P', 'Q', 'C', 'L', 'O', 'C', 'K', '1'};
        success &= Expect(ReadAll(lockPath) == lockMarker,
                          "persistent lock file contains a non-empty format marker");
#ifndef _WIN32
        const auto lockPermissions = fs::status(lockPath).permissions();
        const auto groupOrOther = fs::perms::group_all | fs::perms::others_all;
        success &= Expect((lockPermissions & groupOrOther) == fs::perms::none,
                          "lock file is private to its owner");
#endif

        const auto beforeReplacement = ReadAll(archivePath);
        AtomicFile::Testing::FailNextWriteBeforeReplace();
        success &= Expect(!archive.AddFile(replacementPath.string(), "record.bin"),
                          "rollback failed replacement");
        success &= Expect(ReadAll(archivePath) == beforeReplacement,
                          "failed replacement preserves disk bytes");
        success &= Expect(archive.GetFileData("record.bin") == firstPayload,
                          "failed replacement restores old memory value");
        success &= Expect(CountTemporaryFiles(testRoot) == 0,
                          "replacement failure leaves no temporary files");

        success &= Expect(archive.AddFile(replacementPath.string(), "record.bin"),
                          "commit replacement after rollback");
        const auto beforeRemoval = ReadAll(archivePath);
        AtomicFile::Testing::FailNextWriteBeforeReplace();
        success &= Expect(!archive.RemoveFile("record.bin"),
                          "rollback failed removal");
        success &= Expect(ReadAll(archivePath) == beforeRemoval,
                          "failed removal preserves disk bytes");
        success &= Expect(archive.GetFileData("record.bin") == replacementPayload,
                          "failed removal restores memory entry");
        success &= Expect(CountTemporaryFiles(testRoot) == 0,
                          "removal failure leaves no temporary files");

        const fs::path repairPath = testRoot / "archives/bob_repair.enc";
        success &= Expect(WriteLegacyRepairFixture(repairPath, password),
                          "create repair fixture with invalid metadata");
        CryptoArchive repairArchive("bob", "repair");
        success &= Expect(repairArchive.LoadArchive(password), "load repair fixture");
        const auto repairMetadata = repairArchive.GetFileList();
        success &= Expect(repairMetadata.size() == 1 &&
                          repairMetadata.front().hash == "invalid-hash",
                          "fixture requires metadata repair");
        const auto beforeRepair = ReadAll(repairPath);

        AtomicFile::Testing::FailNextWriteBeforeTemporaryCreate();
        success &= Expect(!repairArchive.RepairArchive(),
                          "rollback repair after simulated permission/storage error");
        const auto rolledBackMetadata = repairArchive.GetFileList();
        success &= Expect(ReadAll(repairPath) == beforeRepair,
                          "failed repair preserves disk bytes");
        success &= Expect(rolledBackMetadata.size() == 1 &&
                          rolledBackMetadata.front().hash == "invalid-hash",
                          "failed repair restores metadata in memory");
        success &= Expect(CountTemporaryFiles(testRoot) == 0,
                          "repair failure leaves no temporary files");
        success &= Expect(repairArchive.RepairArchive(), "commit repaired archive");
        success &= Expect(repairArchive.VerifyIntegrity(),
                          "committed repair has valid hashes");

        CryptoArchive firstInstance("carol", "shared");
        success &= Expect(firstInstance.InitializeArchive(password),
                          "create shared archive");
        success &= Expect(firstInstance.AddFile(firstPayloadPath.string(), "base.bin"),
                          "seed shared archive");
        CryptoArchive secondInstance("carol", "shared");
        success &= Expect(secondInstance.LoadArchive(password), "load second instance");

        const fs::path leftPath = testRoot / "left.bin";
        const fs::path rightPath = testRoot / "right.bin";
        success &= Expect(WritePayload(leftPath, {0x41}), "write left concurrency fixture");
        success &= Expect(WritePayload(rightPath, {0x42}), "write right concurrency fixture");
        bool leftResult = false;
        bool rightResult = false;
        std::thread left([&] {
            leftResult = firstInstance.AddFile(leftPath.string(), "left.bin");
        });
        std::thread right([&] {
            rightResult = secondInstance.AddFile(rightPath.string(), "right.bin");
        });
        left.join();
        right.join();

        success &= Expect(leftResult != rightResult,
                          "exactly one stale concurrent instance may commit");
        CryptoArchive concurrentReader("carol", "shared");
        success &= Expect(concurrentReader.LoadArchive(password),
                          "load archive after concurrent writers");
        success &= Expect(!concurrentReader.GetFileData("base.bin").empty(),
                          "concurrent update preserves original entry");
        success &= Expect((!concurrentReader.GetFileData("left.bin").empty()) == leftResult &&
                          (!concurrentReader.GetFileData("right.bin").empty()) == rightResult,
                          "disk contains only the winning concurrent update");

        CryptoArchive& staleInstance = leftResult ? secondInstance : firstInstance;
        const fs::path& stalePayloadPath = leftResult ? rightPath : leftPath;
        const std::string staleName = leftResult ? "right.bin" : "left.bin";
        success &= Expect(staleInstance.GetFileData(staleName).empty(),
                          "losing instance rolls back its memory change");
        success &= Expect(staleInstance.ReloadArchive(), "reload stale instance");
        success &= Expect(staleInstance.AddFile(stalePayloadPath.string(), staleName),
                          "commit losing update after reload");

        CryptoArchive finalReader("carol", "shared");
        success &= Expect(finalReader.LoadArchive(password), "load final shared archive");
        success &= Expect(!finalReader.GetFileData("base.bin").empty() &&
                          !finalReader.GetFileData("left.bin").empty() &&
                          !finalReader.GetFileData("right.bin").empty(),
                          "reload and retry preserves both concurrent updates");
        success &= Expect(CountTemporaryFiles(testRoot) == 0,
                          "all transaction paths leave no temporary files");
    } catch (const std::exception& exception) {
        std::cerr << "FAILED with exception: " << exception.what() << std::endl;
        success = false;
    }

    fs::current_path(originalPath);
    std::error_code cleanupError;
    fs::remove_all(testRoot, cleanupError);
    if (cleanupError) {
        std::cerr << "Warning: cleanup failed: " << cleanupError.message() << std::endl;
    }
    return success ? 0 : 1;
}
