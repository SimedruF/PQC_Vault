#include "EncryptedDatabase.h"
#include "AtomicFile.h"

#include <algorithm>
#include <array>
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

bool HasMagic(const std::filesystem::path& path, const std::string& expected) {
    const std::vector<uint8_t> data = ReadAll(path);
    return data.size() >= expected.size() &&
           std::equal(expected.begin(), expected.end(), data.begin());
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

EncryptedDatabase::UserRecord MakeRecord(const std::string& username) {
    EncryptedDatabase::UserRecord record;
    record.username = username;
    record.email = username + "@example.test";
    record.website = "https://example.test";
    record.encrypted_password = "test-password-verifier";
    record.salt = "test-salt";
    record.created_at = "100";
    record.last_login = "200";
    return record;
}

bool WriteLegacyDatabase(const std::filesystem::path& path,
                         const EncryptedDatabase::UserRecord& record) {
    SimpleJSON root;
    root["version"] = "1.0";
    root["created_at"] = "100";
    root["algorithm"] = "legacy-placeholder";
    root["user_" + record.username] = record.toJson().toJsonString();

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << "PQCWALLET_DB_v1.0\n" << root.toJsonString();
    file.flush();
    return file.good();
}

} // namespace

int main() {
    namespace fs = std::filesystem;

    const fs::path originalPath = fs::current_path();
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path testRoot = fs::temp_directory_path() /
                              ("pqcwallet_database_security_" + std::to_string(suffix));
    bool success = true;

    try {
        fs::create_directories(testRoot);
        fs::current_path(testRoot);

        const std::string password = "database master password";
        const fs::path databasePath = testRoot / "vault.pqc";
        EncryptedDatabase database(databasePath.string(), password);
        success &= Expect(database.initialize(), "create encrypted database");
        success &= Expect(HasMagic(databasePath, "PQCDB002"), "write PQCDB002 header");

        const std::vector<uint8_t> emptyDatabaseEncryption = ReadAll(databasePath);
        const auto record = MakeRecord("alice");
        AtomicFile::Testing::FailNextWriteBeforeReplace();
        success &= Expect(!database.addUser(record), "simulate interrupted database update");
        success &= Expect(ReadAll(databasePath) == emptyDatabaseEncryption,
                          "interrupted update preserves encrypted database byte-for-byte");
        EncryptedDatabase::UserRecord absentRecord;
        success &= Expect(!database.getUser("alice", absentRecord),
                          "interrupted update restores the in-memory database state");

        success &= Expect(database.addUser(record), "add and persist credential record");
        const std::vector<uint8_t> populatedDatabaseEncryption = ReadAll(databasePath);
        success &= Expect(emptyDatabaseEncryption != populatedDatabaseEncryption,
                          "generate new salt and nonce on database save");

        EncryptedDatabase validReader(databasePath.string(), password);
        success &= Expect(validReader.initialize(), "open database with correct password");
        EncryptedDatabase::UserRecord loadedRecord;
        success &= Expect(validReader.getUser("alice", loadedRecord), "load encrypted record");
        success &= Expect(loadedRecord.email == record.email && loadedRecord.website == record.website,
                          "preserve record through encrypted round-trip");

        const std::string newPassword = "new database master password";
        success &= Expect(validReader.changeMasterPassword(password, newPassword),
                          "re-encrypt database under new master password");
        EncryptedDatabase oldMasterReader(databasePath.string(), password);
        success &= Expect(!oldMasterReader.initialize(), "reject previous master password after rekey");
        EncryptedDatabase newMasterReader(databasePath.string(), newPassword);
        success &= Expect(newMasterReader.initialize(), "open database with new master password");
        EncryptedDatabase::UserRecord rekeyedRecord;
        success &= Expect(newMasterReader.getUser("alice", rekeyedRecord) &&
                          rekeyedRecord.email == record.email,
                          "preserve records during master-password rekey");

        const std::vector<uint8_t> beforeWrongPassword = ReadAll(databasePath);
        EncryptedDatabase wrongPasswordReader(databasePath.string(), "wrong password");
        success &= Expect(!wrongPasswordReader.initialize(), "reject incorrect master password");
        success &= Expect(ReadAll(databasePath) == beforeWrongPassword,
                          "do not overwrite database after wrong password");

        success &= Expect(TamperLastByte(databasePath), "modify database authentication tag");
        const std::vector<uint8_t> tamperedData = ReadAll(databasePath);
        EncryptedDatabase tamperedReader(databasePath.string(), newPassword);
        success &= Expect(!tamperedReader.initialize(), "reject modified database");
        success &= Expect(ReadAll(databasePath) == tamperedData,
                          "do not overwrite modified database");

        const fs::path legacyPath = testRoot / "legacy.pqc";
        const auto legacyRecord = MakeRecord("legacy");
        success &= Expect(WriteLegacyDatabase(legacyPath, legacyRecord),
                          "create legacy plaintext database fixture");
        EncryptedDatabase legacyReader(legacyPath.string(), password);
        success &= Expect(legacyReader.initialize(), "load and migrate legacy database");
        success &= Expect(HasMagic(legacyPath, "PQCDB002"),
                          "rewrite plaintext database as PQCDB002");
        EncryptedDatabase::UserRecord migratedRecord;
        success &= Expect(legacyReader.getUser("legacy", migratedRecord),
                          "retain legacy record during migration");
        success &= Expect(migratedRecord.email == legacyRecord.email,
                          "preserve legacy record contents");
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
