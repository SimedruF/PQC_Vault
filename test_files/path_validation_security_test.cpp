#include "CryptoArchive.h"
#include "PathSecurity.h"

#include <chrono>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
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

template <typename Integer>
void AppendNative(std::vector<unsigned char>& output, Integer value) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
    output.insert(output.end(), bytes, bytes + sizeof(value));
}

bool WriteLegacyArchive(const std::filesystem::path& path,
                        const std::string& password,
                        const std::vector<std::string>& names) {
    std::vector<unsigned char> plaintext;
    AppendNative(plaintext, static_cast<std::uint32_t>(names.size()));
    for (const auto& name : names) {
        AppendNative(plaintext, static_cast<std::uint32_t>(name.size()));
        plaintext.insert(plaintext.end(), name.begin(), name.end());
        AppendNative(plaintext, std::uint64_t{1});
        plaintext.push_back(0x42);
        AppendNative(plaintext, std::uint32_t{0});
        AppendNative(plaintext, std::uint32_t{0});
    }

    std::array<unsigned char, 32> key{};
    unsigned int keySize = 0;
    if (EVP_Digest(password.data(), password.size(), key.data(), &keySize,
                   EVP_sha256(), nullptr) != 1 || keySize != key.size()) {
        return false;
    }
    for (std::size_t i = 0; i < plaintext.size(); ++i) {
        plaintext[i] ^= key[i % key.size()];
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    const std::uint64_t size = plaintext.size();
    file.write("PQCENC01", 8);
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
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
        ("pqcwallet_path_security_" + std::to_string(suffix));

    bool success = true;
    try {
        fs::create_directories(testRoot / "users");
        fs::create_directories(testRoot / "archives");
        fs::create_directories(testRoot / "outside");
        fs::create_directories(testRoot / "extracted");
        fs::current_path(testRoot);

        success &= Expect(PathSecurity::ValidateUsername("alice"),
                          "accept a normal username");
        success &= Expect(PathSecurity::ValidateArchiveName(u8"Seif-Ștefan"),
                          "accept safe UTF-8 names");
        success &= Expect(!PathSecurity::ValidateUsername(""), "reject empty username");
        success &= Expect(!PathSecurity::ValidateUsername("../alice"),
                          "reject username traversal");
        success &= Expect(!PathSecurity::ValidateArchiveName("folder/archive"),
                          "reject forward separator");
        success &= Expect(!PathSecurity::ValidateArchiveName("folder\\archive"),
                          "reject backslash separator");
        success &= Expect(!PathSecurity::ValidateArchiveName("bad\nname"),
                          "reject control characters");
        success &= Expect(!PathSecurity::ValidateArchiveName(std::string(129, 'a')),
                          "reject overlong archive name");
        success &= Expect(!PathSecurity::ValidateArchiveName(std::string("bad\xc3", 4)),
                          "reject malformed UTF-8");
        success &= Expect(!PathSecurity::ValidateArchiveName(u8"safe\u202Egnp.exe"),
                          "reject bidirectional override");
        success &= Expect(!PathSecurity::ValidateArchiveName(u8"Cafe\u0301"),
                          "reject decomposed combining form");
        success &= Expect(PathSecurity::NamesCollide("Vault", "vault"),
                          "detect portable case collision");

        fs::path resolved;
        success &= Expect(!PathSecurity::ResolveContainedPath("users", "../outside.enc",
                                                              resolved),
                          "reject relative traversal");
        success &= Expect(!PathSecurity::ResolveContainedPath("users", "/tmp/outside.enc",
                                                              resolved),
                          "reject absolute path");
        success &= Expect(PathSecurity::UserFilePath("alice", resolved) &&
                          resolved.parent_path() == fs::weakly_canonical(testRoot / "users"),
                          "keep user file in canonical users directory");
        success &= Expect(PathSecurity::ArchiveFilePath("alice", "vault", resolved) &&
                          resolved.parent_path() == fs::weakly_canonical(testRoot / "archives"),
                          "keep archive in canonical archives directory");

        std::error_code symlinkError;
        fs::create_directory_symlink(testRoot / "outside", testRoot / "users/link",
                                     symlinkError);
        if (!symlinkError) {
            success &= Expect(!PathSecurity::ResolveContainedPath("users", "link/escape.enc",
                                                                  resolved),
                              "reject escape through directory symlink");
        }

        success &= Expect(!PathSecurity::ResolveExtractionPath(
                              testRoot / "extracted", "../escape.bin", resolved),
                          "reject malicious stored extraction name");
        success &= Expect(PathSecurity::ResolveExtractionPath(
                              testRoot / "extracted", "safe.bin", resolved) &&
                          resolved.parent_path() == fs::weakly_canonical(testRoot / "extracted"),
                          "resolve extraction inside selected directory");

        const fs::path payloadPath = testRoot / "payload.bin";
        const std::vector<unsigned char> payload = {1, 2, 3, 4};
        {
            std::ofstream file(payloadPath, std::ios::binary);
            file.write(reinterpret_cast<const char*>(payload.data()),
                       static_cast<std::streamsize>(payload.size()));
        }

        const std::string password = "correct horse battery staple";
        success &= Expect(!CryptoArchive::CreateNewArchive("../mallory", password, "vault"),
                          "backend rejects unsafe archive owner");
        success &= Expect(CryptoArchive::CreateNewArchive("alice", password, "Vault"),
                          "create valid archive");
        success &= Expect(!CryptoArchive::CreateNewArchive("alice", password, "vault"),
                          "reject archive name collision");

        CryptoArchive archive("alice", "Vault");
        success &= Expect(archive.LoadArchive(password), "load valid archive");
        success &= Expect(!archive.AddFile(payloadPath.string(), "../escape.bin"),
                          "reject traversal as stored filename");
        success &= Expect(!archive.AddFile(payloadPath.string(), "/absolute.bin"),
                          "reject absolute stored filename");
        success &= Expect(archive.AddFile(payloadPath.string(), "Report.bin"),
                          "add safe stored filename");
        success &= Expect(!archive.AddFile(payloadPath.string(), "report.bin"),
                          "reject case-colliding stored filename");
        success &= Expect(archive.ExtractFile("Report.bin",
                                             (testRoot / "extracted").string()),
                          "extract safe file to selected directory");
        success &= Expect(fs::is_regular_file(testRoot / "extracted/Report.bin") &&
                          !fs::exists(testRoot / "escape.bin"),
                          "extraction cannot escape selected directory");

        success &= Expect(WriteLegacyArchive(testRoot / "archives/bob_legacy.enc",
                                             password, {"../escape.bin"}),
                          "create authenticated-input parser traversal fixture");
        CryptoArchive unsafeLegacy("bob", "legacy");
        success &= Expect(!unsafeLegacy.LoadArchive(password),
                          "reject unsafe filename while deserializing an archive");

        success &= Expect(WriteLegacyArchive(testRoot / "archives/carol_legacy.enc",
                                             password, {"Report.bin", "report.bin"}),
                          "create parser collision fixture");
        CryptoArchive collidingLegacy("carol", "legacy");
        success &= Expect(!collidingLegacy.LoadArchive(password),
                          "reject colliding names while deserializing an archive");
    } catch (const std::exception& exception) {
        std::cerr << "FAILED with exception: " << exception.what() << std::endl;
        success = false;
    }

    fs::current_path(originalPath);
    std::error_code cleanupError;
    fs::remove_all(testRoot, cleanupError);
    if (cleanupError) {
        std::cerr << "Warning: could not remove test directory: "
                  << cleanupError.message() << std::endl;
    }
    return success ? 0 : 1;
}
