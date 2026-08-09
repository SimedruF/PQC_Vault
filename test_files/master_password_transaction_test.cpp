#include "CryptoArchive.h"
#include "EncryptedDatabase.h"
#include "PasswordManager.h"
#include "TransactionalFileBatch.h"

#include <chrono>
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

bool OpensAll(const std::string& password) {
    PasswordManager manager;
    if (!manager.VerifyPassword("alice", password)) {
        return false;
    }

    EncryptedDatabase database("users/alice_database.pqc", password);
    if (!database.initialize()) {
        return false;
    }

    CryptoArchive first("alice", "first");
    CryptoArchive second("alice", "second");
    return first.LoadArchive(password) && second.LoadArchive(password);
}

bool RejectsAll(const std::string& password) {
    PasswordManager manager;
    const bool userRejected = !manager.VerifyPassword("alice", password);

    EncryptedDatabase database("users/alice_database.pqc", password);
    const bool databaseRejected = !database.initialize();

    CryptoArchive first("alice", "first");
    CryptoArchive second("alice", "second");
    return userRejected && databaseRejected &&
           !first.LoadArchive(password) && !second.LoadArchive(password);
}

} // namespace

int main() {
    namespace fs = std::filesystem;

    const fs::path originalPath = fs::current_path();
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path testRoot = fs::temp_directory_path() /
                              ("pqcwallet_master_transaction_" + std::to_string(suffix));
    bool success = true;

    try {
        fs::create_directories(testRoot);
        fs::current_path(testRoot);

        const std::string oldPassword = "old transactional password";
        const std::string newPassword = "new transactional password";

        PasswordManager manager;
        success &= Expect(manager.CreateUser("alice", oldPassword), "create user fixture");

        EncryptedDatabase database("users/alice_database.pqc", oldPassword);
        success &= Expect(database.initialize(), "create database fixture");
        EncryptedDatabase::UserRecord record;
        record.username = "credential";
        record.email = "alice@example.test";
        record.website = "https://example.test";
        record.encrypted_password = "verifier";
        record.salt = "salt";
        record.created_at = "1";
        record.last_login = "2";
        success &= Expect(database.addUser(record), "populate database fixture");

        const fs::path payloadPath = testRoot / "payload.bin";
        {
            std::ofstream payload(payloadPath, std::ios::binary);
            payload << "transaction payload";
        }
        CryptoArchive first("alice", "first");
        CryptoArchive second("alice", "second");
        success &= Expect(first.InitializeArchive(oldPassword) &&
                          first.AddFile(payloadPath.string(), "first.bin"),
                          "create first archive fixture");
        success &= Expect(second.InitializeArchive(oldPassword) &&
                          second.AddFile(payloadPath.string(), "second.bin"),
                          "create second archive fixture");

        // Entry order is user, database, then the two archives. Inject a failure
        // before every publication point and require a complete rollback.
        for (size_t target = 0; target < 4; ++target) {
            TransactionalFileBatch::Testing::FailBeforeTargetWrite(target);
            success &= Expect(!manager.ChangeMasterPassword(
                                  "alice", oldPassword, newPassword, &database),
                              "fail transaction before target " + std::to_string(target));
            success &= Expect(OpensAll(oldPassword),
                              "old password opens every store after failure " +
                                  std::to_string(target));
            success &= Expect(RejectsAll(newPassword),
                              "new password opens no store after failure " +
                                  std::to_string(target));
        }

        // Simulate termination after two destinations have already changed.
        TransactionalFileBatch::Testing::SimulateCrashAfterTargetWrite(1);
        success &= Expect(!manager.ChangeMasterPassword(
                              "alice", oldPassword, newPassword, &database),
                          "simulate process stop during publication");
        success &= Expect(fs::exists(".pqcwallet_transactions"),
                          "crash simulation leaves a persistent journal");
        {
            PasswordManager startupRecoveryTrigger;
            (void)startupRecoveryTrigger;
        }
        success &= Expect(!fs::exists(".pqcwallet_transactions"),
                          "startup automatically recovers and removes the journal");
        success &= Expect(OpensAll(oldPassword),
                          "journal recovery restores every old-password file");
        success &= Expect(RejectsAll(newPassword),
                          "journal recovery removes every partial new-password file");

        success &= Expect(manager.ChangeMasterPassword(
                              "alice", oldPassword, newPassword, &database),
                          "commit complete master-password transaction");
        success &= Expect(OpensAll(newPassword),
                          "new password opens user, database, and both archives");
        success &= Expect(RejectsAll(oldPassword),
                          "old password is rejected everywhere after commit");

        EncryptedDatabase::UserRecord loadedRecord;
        success &= Expect(database.getUser("credential", loadedRecord) &&
                          loadedRecord.email == record.email,
                          "live database owner retains its data after transactional rekey");
        success &= Expect(!fs::exists(".pqcwallet_transactions"),
                          "successful commit removes its journal");
    } catch (const std::exception& error) {
        std::cerr << "FAILED with exception: " << error.what() << std::endl;
        success = false;
    }

    fs::current_path(originalPath);
    std::error_code cleanupError;
    fs::remove_all(testRoot, cleanupError);
    return success ? 0 : 1;
}
