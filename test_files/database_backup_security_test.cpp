#include "AtomicFile.h"
#include "EncryptedDatabase.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
    }
    return condition;
}

std::vector<uint8_t> ReadAll(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                                std::istreambuf_iterator<char>());
}

bool WriteAll(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    return file.good();
}

bool ContainsText(const std::vector<uint8_t>& data, const std::string& text) {
    return std::search(data.begin(), data.end(), text.begin(), text.end()) != data.end();
}

EncryptedDatabase::UserRecord MakeRecord(const std::string& username) {
    EncryptedDatabase::UserRecord record;
    record.username = username;
    record.email = username + "@backup.test";
    record.website = "https://backup.test/" + username;
    record.encrypted_password = "verifier-for-" + username;
    record.salt = "salt-for-" + username;
    record.created_at = "100";
    record.last_login = "200";
    return record;
}

} // namespace

int main() {
    namespace fs = std::filesystem;

    const fs::path originalPath = fs::current_path();
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path testRoot = fs::temp_directory_path() /
                              ("pqcwallet_backup_security_" + std::to_string(suffix));
    bool success = true;

    try {
        fs::create_directories(testRoot);
        fs::current_path(testRoot);

        const fs::path databasePath = testRoot / "vault.pqc";
        const fs::path backupPath = testRoot / "vault-backup.pqcbak";
        const std::string masterPassword = "current database master password";

        EncryptedDatabase database(databasePath.string(), masterPassword);
        success &= Expect(database.initialize(), "create source database");
        const auto alice = MakeRecord("alice");
        const auto bob = MakeRecord("bob");
        success &= Expect(database.addUser(alice), "add source record");

        std::string recoveryKey;
        success &= Expect(database.generateRecoveryKey(recoveryKey) &&
                          recoveryKey.rfind("PQC-RK1-", 0) == 0 &&
                          recoveryKey.size() == 72,
                          "generate printable 256-bit recovery key");
        success &= Expect(database.exportBackup(backupPath.string(), recoveryKey),
                          "export authenticated backup");
        const std::vector<uint8_t> validBackup = ReadAll(backupPath);
        success &= Expect(validBackup.size() > 8 &&
                          std::string(validBackup.begin(), validBackup.begin() + 8) == "PQCBKP01",
                          "write PQCBKP01 magic and versioned container");
        success &= Expect(!ContainsText(validBackup, alice.email) &&
                          !ContainsText(validBackup, alice.encrypted_password),
                          "backup does not expose database fields in plaintext");

        AtomicFile::Testing::FailNextWriteBeforeReplace();
        success &= Expect(!database.exportBackup(backupPath.string(), recoveryKey),
                          "simulate interrupted backup export");
        success &= Expect(ReadAll(backupPath) == validBackup,
                          "interrupted export preserves previous backup byte-for-byte");
        success &= Expect(!database.exportBackup(databasePath.string(), recoveryKey),
                          "refuse to overwrite live database with a backup");

        success &= Expect(database.addUser(bob), "change live database after backup");
        const std::vector<uint8_t> databaseBeforeFailures = ReadAll(databasePath);

        success &= Expect(!database.importBackup(backupPath.string(), "wrong recovery key"),
                          "reject wrong backup password");
        success &= Expect(ReadAll(databasePath) == databaseBeforeFailures,
                          "wrong password does not modify live database");

        std::vector<uint8_t> tamperedBackup = validBackup;
        tamperedBackup.back() ^= 0x01;
        const fs::path tamperedPath = testRoot / "tampered.pqcbak";
        success &= Expect(WriteAll(tamperedPath, tamperedBackup), "write tampered fixture");
        success &= Expect(!database.importBackup(tamperedPath.string(), recoveryKey),
                          "reject modified authentication tag");
        success &= Expect(ReadAll(databasePath) == databaseBeforeFailures,
                          "tampered backup does not modify live database");

        const fs::path truncatedPath = testRoot / "truncated.pqcbak";
        std::vector<uint8_t> truncated(validBackup.begin(), validBackup.begin() + 20);
        success &= Expect(WriteAll(truncatedPath, truncated), "write truncated fixture");
        success &= Expect(!database.importBackup(truncatedPath.string(), recoveryKey),
                          "reject truncated backup");
        success &= Expect(ReadAll(databasePath) == databaseBeforeFailures,
                          "truncated backup does not modify live database");

        AtomicFile::Testing::FailNextWriteBeforeReplace();
        success &= Expect(!database.importBackup(backupPath.string(), recoveryKey),
                          "simulate interruption during authenticated import");
        success &= Expect(ReadAll(databasePath) == databaseBeforeFailures,
                          "interrupted import preserves live database byte-for-byte");
        EncryptedDatabase::UserRecord bobBeforeRestore;
        success &= Expect(database.getUser("bob", bobBeforeRestore),
                          "failed imports preserve in-memory database state");

        success &= Expect(database.importBackup(backupPath.string(), recoveryKey),
                          "restore valid authenticated backup");
        EncryptedDatabase::UserRecord restoredAlice;
        EncryptedDatabase::UserRecord removedBob;
        success &= Expect(database.getUser("alice", restoredAlice) &&
                          restoredAlice.email == alice.email,
                          "restore backed-up record in live database owner");
        success &= Expect(!database.getUser("bob", removedBob),
                          "replace records that were not present in backup");

        EncryptedDatabase reopened(databasePath.string(), masterPassword);
        success &= Expect(reopened.initialize(),
                          "restored database remains encrypted with current master password");
        EncryptedDatabase::UserRecord reopenedAlice;
        success &= Expect(reopened.getUser("alice", reopenedAlice) &&
                          reopenedAlice.website == alice.website,
                          "restored database survives encrypted round-trip");
    } catch (const std::exception& error) {
        std::cerr << "FAILED with exception: " << error.what() << std::endl;
        success = false;
    }

    fs::current_path(originalPath);
    std::error_code cleanupError;
    fs::remove_all(testRoot, cleanupError);
    return success ? 0 : 1;
}
