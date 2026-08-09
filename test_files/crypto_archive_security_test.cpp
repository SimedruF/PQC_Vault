#include "CryptoArchive.h"
#include "AtomicFile.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <openssl/evp.h>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
    }
    return condition;
}

std::array<char, 8> ReadMagic(const std::filesystem::path& path) {
    std::array<char, 8> magic{};
    std::ifstream file(path, std::ios::binary);
    file.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    return magic;
}

bool HasMagic(const std::filesystem::path& path, const std::string& expected) {
    const auto magic = ReadMagic(path);
    return std::string(magic.data(), magic.size()) == expected;
}

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                                std::istreambuf_iterator<char>());
}

bool TamperLastByte(const std::filesystem::path& path) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) {
        return false;
    }

    file.seekg(-1, std::ios::end);
    char value = 0;
    file.read(&value, 1);
    if (!file) {
        return false;
    }

    value ^= 0x01;
    file.seekp(-1, std::ios::end);
    file.write(&value, 1);
    file.flush();
    return file.good();
}

bool WriteLegacyEmptyArchive(const std::filesystem::path& path, const std::string& password) {
    std::array<uint8_t, 32> key{};
    unsigned int keyLength = 0;
    if (EVP_Digest(password.data(), password.size(), key.data(), &keyLength,
                   EVP_sha256(), nullptr) != 1 || keyLength != key.size()) {
        return false;
    }

    constexpr uint64_t plaintextSize = 4;
    std::array<uint8_t, plaintextSize> ciphertext{};
    for (size_t i = 0; i < ciphertext.size(); ++i) {
        ciphertext[i] ^= key[i];
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write("PQCENC01", 8);
    file.write(reinterpret_cast<const char*>(&plaintextSize), sizeof(plaintextSize));
    file.write(reinterpret_cast<const char*>(ciphertext.data()),
               static_cast<std::streamsize>(ciphertext.size()));
    file.flush();
    return file.good();
}

} // namespace

int main() {
    namespace fs = std::filesystem;

    const fs::path originalPath = fs::current_path();
    const auto uniqueSuffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path testRoot = fs::temp_directory_path() /
                              ("pqcwallet_archive_security_" + std::to_string(uniqueSuffix));

    bool success = true;
    try {
        fs::create_directories(testRoot);
        fs::current_path(testRoot);

        const std::string password = "correct horse battery staple";
        const std::vector<uint8_t> expectedPayload = {0x00, 0x01, 0x02, 0x7f, 0x80, 0xfe, 0xff};
        const fs::path payloadPath = testRoot / "payload.bin";
        {
            std::ofstream payload(payloadPath, std::ios::binary);
            payload.write(reinterpret_cast<const char*>(expectedPayload.data()),
                          static_cast<std::streamsize>(expectedPayload.size()));
        }

        CryptoArchive archive("alice", "secure");
        success &= Expect(archive.InitializeArchive(password), "create secure archive");
        success &= Expect(archive.AddFile(payloadPath.string(), "payload.bin"),
                          "add and persist payload");

        const fs::path archivePath = testRoot / "archives/alice_secure.enc";
        success &= Expect(HasMagic(archivePath, "PQCENC02"), "write PQCENC02 header");

        const std::vector<uint8_t> firstEncryption = ReadAll(archivePath);
        AtomicFile::Testing::FailNextWriteBeforeReplace();
        success &= Expect(!archive.ResetArchive(), "simulate interrupted archive reset");
        success &= Expect(ReadAll(archivePath) == firstEncryption,
                          "interrupted reset preserves the encrypted archive byte-for-byte");
        success &= Expect(archive.GetFileData("payload.bin") == expectedPayload,
                          "interrupted reset restores the in-memory archive state");

        success &= Expect(archive.SaveArchive(), "save unchanged archive again");
        const std::vector<uint8_t> secondEncryption = ReadAll(archivePath);
        success &= Expect(firstEncryption != secondEncryption,
                          "generate a fresh salt and nonce for every save");

        CryptoArchive validReader("alice", "secure");
        success &= Expect(validReader.LoadArchive(password), "load archive with correct password");
        success &= Expect(validReader.GetFileData("payload.bin") == expectedPayload,
                          "round-trip payload without modification");
        const auto metadata = validReader.GetFileList();
        success &= Expect(metadata.size() == 1 && metadata.front().data.empty() &&
                          metadata.front().size == expectedPayload.size(),
                          "return archive metadata without duplicating decrypted payloads");

        CryptoArchive wrongPasswordReader("alice", "secure");
        success &= Expect(!wrongPasswordReader.LoadArchive("wrong password"),
                          "reject incorrect password");

        success &= Expect(TamperLastByte(archivePath), "modify authentication tag");
        CryptoArchive tamperedReader("alice", "secure");
        success &= Expect(!tamperedReader.LoadArchive(password), "reject modified archive");

        fs::create_directories(testRoot / "archives");
        const fs::path legacyPath = testRoot / "archives/bob_legacy.enc";
        success &= Expect(WriteLegacyEmptyArchive(legacyPath, password),
                          "create legacy compatibility fixture");
        CryptoArchive legacyReader("bob", "legacy");
        success &= Expect(legacyReader.LoadArchive(password), "load legacy PQCENC01 archive");
        success &= Expect(legacyReader.SaveArchive(), "migrate legacy archive on save");
        success &= Expect(HasMagic(legacyPath, "PQCENC02"), "rewrite legacy archive as PQCENC02");

        CryptoArchive renameSource("carol", "photos");
        success &= Expect(renameSource.InitializeArchive(password), "create archive for rename");
        const fs::path renameSourcePath = testRoot / "archives/carol_photos.enc";
        const fs::path renamedPath = testRoot / "archives/carol_vacation.enc";
        const std::vector<uint8_t> bytesBeforeRename = ReadAll(renameSourcePath);
        std::string renameError;
        success &= Expect(CryptoArchive::RenameArchive(
                              "carol", "photos", "vacation", &renameError),
                          "rename archive without decrypting it");
        success &= Expect(!fs::exists(renameSourcePath) && fs::is_regular_file(renamedPath),
                          "move the archive to its validated destination");
        success &= Expect(ReadAll(renamedPath) == bytesBeforeRename,
                          "preserve encrypted archive bytes during rename");
        CryptoArchive renamedReader("carol", "vacation");
        success &= Expect(renamedReader.LoadArchive(password),
                          "load archive using its new name");

        CryptoArchive collision("carol", "existing");
        success &= Expect(collision.InitializeArchive(password),
                          "create rename collision target");
        const fs::path collisionPath = testRoot / "archives/carol_existing.enc";
        const std::vector<uint8_t> collisionBytes = ReadAll(collisionPath);
        success &= Expect(!CryptoArchive::RenameArchive(
                              "carol", "vacation", "existing", &renameError),
                          "reject archive rename collision");
        success &= Expect(fs::is_regular_file(renamedPath) &&
                              ReadAll(collisionPath) == collisionBytes,
                          "preserve both archives after rejected collision");
    } catch (const std::exception& exception) {
        std::cerr << "FAILED with exception: " << exception.what() << std::endl;
        success = false;
    }

    fs::current_path(originalPath);
    std::error_code cleanupError;
    fs::remove_all(testRoot, cleanupError);
    if (cleanupError) {
        std::cerr << "Warning: could not remove test directory: " << cleanupError.message() << std::endl;
    }

    return success ? 0 : 1;
}
