#include "PasswordManager.h"
#include "AtomicFile.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <oqs/oqs.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

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

uint32_t ReadVersion(const std::filesystem::path& path) {
    const auto data = ReadAll(path);
    const std::array<uint8_t, 8> magic = {'P', 'Q', 'C', 'U', 'S', 'R', '0', '5'};
    if (data.size() >= 12 && std::equal(magic.begin(), magic.end(), data.begin())) {
        uint32_t version = 0;
        for (size_t i = 8; i < 12; ++i) {
            version = (version << 8U) | data[i];
        }
        return version;
    }
    if (data.size() < sizeof(uint32_t)) {
        return 0;
    }
    uint32_t version = 0;
    std::memcpy(&version, data.data(), sizeof(version));
    return version;
}

bool HasV5Magic(const std::filesystem::path& path) {
    const auto data = ReadAll(path);
    const std::array<uint8_t, 8> magic = {'P', 'Q', 'C', 'U', 'S', 'R', '0', '5'};
    return data.size() >= magic.size() &&
           std::equal(magic.begin(), magic.end(), data.begin());
}

uint64_t ReadBigEndian64(const std::vector<uint8_t>& data, size_t offset) {
    if (offset > data.size() || data.size() - offset < 8) {
        return 0;
    }
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value = (value << 8U) | data[offset + i];
    }
    return value;
}

bool ReadV5Nonces(const std::filesystem::path& path,
                  std::vector<uint8_t>& secretKeyNonce,
                  std::vector<uint8_t>& passwordNonce) {
    const auto data = ReadAll(path);
    if (!HasV5Magic(path) || data.size() < 24 || ReadVersion(path) != 5 ||
        ReadBigEndian64(data, 12) != data.size()) {
        return false;
    }
    size_t offset = 24;
    auto readComponent = [&](std::vector<uint8_t>& value) {
        const uint64_t size = ReadBigEndian64(data, offset);
        offset += 8;
        if (size == 0 || size > data.size() - offset) {
            return false;
        }
        value.assign(data.begin() + static_cast<std::ptrdiff_t>(offset),
                     data.begin() + static_cast<std::ptrdiff_t>(offset + size));
        offset += static_cast<size_t>(size);
        return true;
    };
    std::vector<uint8_t> salt;
    return readComponent(salt) && readComponent(secretKeyNonce) &&
           readComponent(passwordNonce);
}

bool WriteAll(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    return file.good();
}

std::vector<uint8_t> DeriveKey(const std::string& password,
                               const std::vector<uint8_t>& salt) {
    std::vector<uint8_t> key(32);
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> context(
        EVP_PKEY_CTX_new_id(EVP_PKEY_SCRYPT, nullptr), EVP_PKEY_CTX_free);
    if (!context || EVP_PKEY_derive_init(context.get()) <= 0 ||
        EVP_PKEY_CTX_set1_pbe_pass(context.get(), password.data(), password.size()) <= 0 ||
        EVP_PKEY_CTX_set1_scrypt_salt(context.get(), salt.data(), salt.size()) <= 0 ||
        EVP_PKEY_CTX_set_scrypt_N(context.get(), 32768) <= 0 ||
        EVP_PKEY_CTX_set_scrypt_r(context.get(), 8) <= 0 ||
        EVP_PKEY_CTX_set_scrypt_p(context.get(), 1) <= 0) {
        return {};
    }

    size_t keyLength = key.size();
    if (EVP_PKEY_derive(context.get(), key.data(), &keyLength) <= 0 ||
        keyLength != key.size()) {
        return {};
    }
    return key;
}

bool EncryptGcm(const std::vector<uint8_t>& plaintext,
                const std::vector<uint8_t>& key,
                const std::vector<uint8_t>& iv,
                std::vector<uint8_t>& ciphertext,
                std::vector<uint8_t>& tag) {
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> context(
        EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!context ||
        EVP_EncryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(iv.size()), nullptr) != 1 ||
        EVP_EncryptInit_ex(context.get(), nullptr, nullptr, key.data(), iv.data()) != 1) {
        return false;
    }

    ciphertext.resize(plaintext.size() + 16);
    int length = 0;
    int totalLength = 0;
    if (EVP_EncryptUpdate(context.get(), ciphertext.data(), &length, plaintext.data(),
                          static_cast<int>(plaintext.size())) != 1) {
        return false;
    }
    totalLength = length;
    if (EVP_EncryptFinal_ex(context.get(), ciphertext.data() + totalLength, &length) != 1) {
        return false;
    }
    totalLength += length;
    ciphertext.resize(static_cast<size_t>(totalLength));
    tag.resize(16);
    return EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_GET_TAG, 16, tag.data()) == 1;
}

bool WriteLegacyVector(std::ofstream& file, const std::vector<uint8_t>& value) {
    const size_t size = value.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(value.data()),
               static_cast<std::streamsize>(value.size()));
    return file.good();
}

bool WriteModernVector(std::ofstream& file, const std::vector<uint8_t>& value) {
    const uint64_t size = value.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(value.data()),
               static_cast<std::streamsize>(value.size()));
    return file.good();
}

bool WriteV2User(const std::filesystem::path& path, const std::string& password) {
    std::vector<uint8_t> salt(32);
    std::vector<uint8_t> iv(16);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1 ||
        RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
        return false;
    }

    std::vector<uint8_t> key = DeriveKey(password, salt);
    std::unique_ptr<OQS_KEM, decltype(&OQS_KEM_free)> kem(
        OQS_KEM_new(OQS_KEM_alg_kyber_768), OQS_KEM_free);
    if (key.empty() || !kem) {
        return false;
    }

    std::vector<uint8_t> publicKey(kem->length_public_key);
    std::vector<uint8_t> secretKey(kem->length_secret_key);
    if (OQS_KEM_keypair(kem.get(), publicKey.data(), secretKey.data()) != OQS_SUCCESS) {
        return false;
    }

    std::vector<uint8_t> encryptedSecretKey;
    std::vector<uint8_t> secretKeyTag;
    if (!EncryptGcm(secretKey, key, iv, encryptedSecretKey, secretKeyTag)) {
        return false;
    }

    std::vector<uint8_t> kemCiphertext(kem->length_ciphertext);
    std::vector<uint8_t> sharedSecret(kem->length_shared_secret);
    if (OQS_KEM_encaps(kem.get(), kemCiphertext.data(), sharedSecret.data(),
                       publicKey.data()) != OQS_SUCCESS) {
        return false;
    }

    std::vector<uint8_t> xorEncrypted(password.size());
    for (size_t i = 0; i < password.size(); ++i) {
        xorEncrypted[i] = static_cast<uint8_t>(password[i]) ^ sharedSecret[i % sharedSecret.size()];
    }

    std::vector<uint8_t> encryptedPassword;
    std::vector<uint8_t> passwordTag;
    if (!EncryptGcm(xorEncrypted, key, iv, encryptedPassword, passwordTag)) {
        return false;
    }

    std::vector<uint8_t> combinedTags = passwordTag;
    combinedTags.insert(combinedTags.end(), secretKeyTag.begin(), secretKeyTag.end());

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    const uint32_t version = 2;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    return WriteLegacyVector(file, salt) && WriteLegacyVector(file, iv) &&
           WriteLegacyVector(file, kemCiphertext) && WriteLegacyVector(file, publicKey) &&
           WriteLegacyVector(file, encryptedSecretKey) &&
           WriteLegacyVector(file, encryptedPassword) && WriteLegacyVector(file, combinedTags);
}

bool WriteV1KyberUser(const std::filesystem::path& path, const std::string& password) {
    std::unique_ptr<OQS_KEM, decltype(&OQS_KEM_free)> kem(
        OQS_KEM_new(OQS_KEM_alg_kyber_768), OQS_KEM_free);
    if (!kem) {
        return false;
    }

    std::vector<uint8_t> publicKey(kem->length_public_key);
    std::vector<uint8_t> secretKey(kem->length_secret_key);
    if (OQS_KEM_keypair(kem.get(), publicKey.data(), secretKey.data()) != OQS_SUCCESS) {
        return false;
    }

    std::vector<uint8_t> kemCiphertext(kem->length_ciphertext);
    std::vector<uint8_t> sharedSecret(kem->length_shared_secret);
    if (OQS_KEM_encaps(kem.get(), kemCiphertext.data(), sharedSecret.data(),
                       publicKey.data()) != OQS_SUCCESS) {
        return false;
    }

    std::vector<uint8_t> encryptedPassword(password.size());
    for (size_t i = 0; i < password.size(); ++i) {
        encryptedPassword[i] =
            static_cast<uint8_t>(password[i]) ^ sharedSecret[i % sharedSecret.size()];
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    return WriteLegacyVector(file, kemCiphertext) &&
           WriteLegacyVector(file, publicKey) &&
           WriteLegacyVector(file, secretKey) &&
           WriteLegacyVector(file, encryptedPassword);
}

bool WriteModernKemUser(const std::filesystem::path& path,
                        const std::string& password,
                        const char* algorithm,
                        uint32_t version) {
    std::vector<uint8_t> salt(32);
    std::vector<uint8_t> secretKeyNonce(12);
    std::vector<uint8_t> passwordNonce(12);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1 ||
        RAND_bytes(secretKeyNonce.data(), static_cast<int>(secretKeyNonce.size())) != 1 ||
        RAND_bytes(passwordNonce.data(), static_cast<int>(passwordNonce.size())) != 1 ||
        secretKeyNonce == passwordNonce) {
        return false;
    }

    std::vector<uint8_t> key = DeriveKey(password, salt);
    std::unique_ptr<OQS_KEM, decltype(&OQS_KEM_free)> kem(
        OQS_KEM_new(algorithm), OQS_KEM_free);
    if (key.empty() || !kem) {
        return false;
    }

    std::vector<uint8_t> publicKey(kem->length_public_key);
    std::vector<uint8_t> secretKey(kem->length_secret_key);
    if (OQS_KEM_keypair(kem.get(), publicKey.data(), secretKey.data()) != OQS_SUCCESS) {
        return false;
    }

    std::vector<uint8_t> encryptedSecretKey;
    std::vector<uint8_t> secretKeyTag;
    if (!EncryptGcm(secretKey, key, secretKeyNonce, encryptedSecretKey, secretKeyTag)) {
        return false;
    }

    std::vector<uint8_t> kemCiphertext(kem->length_ciphertext);
    std::vector<uint8_t> sharedSecret(kem->length_shared_secret);
    if (OQS_KEM_encaps(kem.get(), kemCiphertext.data(), sharedSecret.data(),
                       publicKey.data()) != OQS_SUCCESS) {
        return false;
    }

    std::vector<uint8_t> xorEncrypted(password.size());
    for (size_t i = 0; i < password.size(); ++i) {
        xorEncrypted[i] = static_cast<uint8_t>(password[i]) ^ sharedSecret[i % sharedSecret.size()];
    }

    std::vector<uint8_t> encryptedPassword;
    std::vector<uint8_t> passwordTag;
    if (!EncryptGcm(xorEncrypted, key, passwordNonce, encryptedPassword, passwordTag)) {
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    return WriteModernVector(file, salt) &&
           WriteModernVector(file, secretKeyNonce) &&
           WriteModernVector(file, passwordNonce) &&
           WriteModernVector(file, kemCiphertext) &&
           WriteModernVector(file, publicKey) &&
           WriteModernVector(file, encryptedSecretKey) &&
           WriteModernVector(file, encryptedPassword) &&
           WriteModernVector(file, secretKeyTag) &&
           WriteModernVector(file, passwordTag);
}

bool WriteV3KyberUser(const std::filesystem::path& path, const std::string& password) {
    return WriteModernKemUser(path, password, OQS_KEM_alg_kyber_768, 3);
}

bool WriteV4MlKemUser(const std::filesystem::path& path, const std::string& password) {
    return WriteModernKemUser(path, password, OQS_KEM_alg_ml_kem_768, 4);
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

} // namespace

int main() {
    namespace fs = std::filesystem;

    const fs::path originalPath = fs::current_path();
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path testRoot = fs::temp_directory_path() /
                              ("pqcwallet_password_gcm_" + std::to_string(suffix));
    bool success = true;

    try {
        fs::create_directories(testRoot);
        fs::current_path(testRoot);
        PasswordManager manager;

        const std::string password = "correct horse battery staple";
        success &= Expect(OQS_KEM_alg_is_enabled(OQS_KEM_alg_ml_kem_768) == 1,
                          "ML-KEM-768 is available");
        success &= Expect(manager.CreateUser("alice", password), "create v5 ML-KEM user");
        const fs::path alicePath = testRoot / "users/alice.enc";
        success &= Expect(HasV5Magic(alicePath) && ReadVersion(alicePath) == 5,
                          "write portable ML-KEM user format v5");

        std::vector<uint8_t> secretKeyNonce;
        std::vector<uint8_t> passwordNonce;
        success &= Expect(ReadV5Nonces(alicePath, secretKeyNonce, passwordNonce),
                          "read big-endian v5 header and nonce fields");
        success &= Expect(secretKeyNonce.size() == 12 && passwordNonce.size() == 12 &&
                          secretKeyNonce != passwordNonce,
                          "use independent 96-bit GCM nonces");
        success &= Expect(manager.VerifyPassword("alice", password), "verify correct v5 password");
        success &= Expect(!manager.VerifyPassword("alice", "wrong password"),
                          "reject wrong v5 password");

        success &= Expect(manager.CreateUser("tampered", password), "create tamper fixture");
        const fs::path tamperedPath = testRoot / "users/tampered.enc";
        success &= Expect(TamperLastByte(tamperedPath), "modify password authentication tag");
        success &= Expect(!manager.VerifyPassword("tampered", password),
                          "reject modified v5 authentication tag");

        const fs::path v4Path = testRoot / "users/v4_mlkem.enc";
        success &= Expect(WriteV4MlKemUser(v4Path, password),
                          "create native-endian v4 ML-KEM fixture");
        const std::vector<uint8_t> v4BeforeWrongPassword = ReadAll(v4Path);
        success &= Expect(!manager.VerifyPassword("v4_mlkem", "wrong password"),
                          "reject wrong v4 password");
        success &= Expect(ReadAll(v4Path) == v4BeforeWrongPassword,
                          "do not migrate v4 after failed authentication");
        AtomicFile::Testing::FailNextWriteBeforeReplace();
        success &= Expect(manager.VerifyPassword("v4_mlkem", password),
                          "authenticate v4 even if atomic migration is interrupted");
        success &= Expect(ReadAll(v4Path) == v4BeforeWrongPassword,
                          "interrupted migration preserves v4 byte-for-byte");
        success &= Expect(manager.VerifyPassword("v4_mlkem", password),
                          "authenticate and migrate v4 to v5");
        success &= Expect(HasV5Magic(v4Path) && ReadVersion(v4Path) == 5,
                          "publish v4 to v5 migration only after authentication");

        const fs::path v3Path = testRoot / "users/v3_kyber.enc";
        success &= Expect(WriteV3KyberUser(v3Path, password),
                          "create v3 Kyber compatibility fixture");
        const std::vector<uint8_t> v3BeforeWrongPassword = ReadAll(v3Path);
        success &= Expect(!manager.VerifyPassword("v3_kyber", "wrong password"),
                          "reject wrong v3 Kyber password");
        success &= Expect(ReadAll(v3Path) == v3BeforeWrongPassword,
                          "do not migrate v3 Kyber after failed authentication");
        success &= Expect(manager.VerifyPassword("v3_kyber", password),
                          "verify correct v3 Kyber password");
        success &= Expect(HasV5Magic(v3Path) && ReadVersion(v3Path) == 5,
                          "migrate v3 Kyber user to portable v5 ML-KEM");
        success &= Expect(manager.VerifyPassword("v3_kyber", password),
                          "verify migrated v5 ML-KEM user");

        const fs::path legacyPath = testRoot / "users/legacy.enc";
        success &= Expect(WriteV2User(legacyPath, password), "create v2 compatibility fixture");
        const std::vector<uint8_t> v2BeforeWrongPassword = ReadAll(legacyPath);
        success &= Expect(!manager.VerifyPassword("legacy", "wrong password"),
                          "reject wrong v2 password");
        success &= Expect(ReadAll(legacyPath) == v2BeforeWrongPassword,
                          "do not migrate v2 after failed authentication");
        success &= Expect(manager.VerifyPassword("legacy", password),
                          "verify correct v2 password");
        success &= Expect(HasV5Magic(legacyPath) && ReadVersion(legacyPath) == 5,
                          "migrate v2 user file to v5 ML-KEM after authentication");
        success &= Expect(manager.VerifyPassword("legacy", password),
                          "verify v2 fixture after ML-KEM migration");
        secretKeyNonce.clear();
        passwordNonce.clear();
        success &= Expect(ReadV5Nonces(legacyPath, secretKeyNonce, passwordNonce) &&
                          secretKeyNonce != passwordNonce,
                          "migrated file uses independent nonces");

        const fs::path v1Path = testRoot / "users/v1_kyber.enc";
        success &= Expect(WriteV1KyberUser(v1Path, password),
                          "create v1 Kyber compatibility fixture");
        const std::vector<uint8_t> v1BeforeWrongPassword = ReadAll(v1Path);
        success &= Expect(!manager.VerifyPassword("v1_kyber", "wrong password"),
                          "reject wrong v1 Kyber password");
        success &= Expect(ReadAll(v1Path) == v1BeforeWrongPassword,
                          "do not migrate v1 Kyber after failed authentication");
        success &= Expect(manager.VerifyPassword("v1_kyber", password),
                          "verify correct v1 Kyber password");
        success &= Expect(HasV5Magic(v1Path) && ReadVersion(v1Path) == 5,
                          "migrate v1 Kyber user to portable v5 ML-KEM");
        success &= Expect(manager.VerifyPassword("v1_kyber", password),
                          "verify v1 fixture after ML-KEM migration");

        success &= Expect(manager.CreateUser("truncated", password),
                          "create truncated v5 fixture");
        const fs::path truncatedPath = testRoot / "users/truncated.enc";
        auto truncatedData = ReadAll(truncatedPath);
        truncatedData.resize(truncatedData.size() / 2);
        success &= Expect(WriteAll(truncatedPath, truncatedData), "truncate v5 file");
        success &= Expect(!manager.VerifyPassword("truncated", password),
                          "reject truncated v5 file");

        success &= Expect(manager.CreateUser("oversized", password),
                          "create malicious-length v5 fixture");
        const fs::path oversizedPath = testRoot / "users/oversized.enc";
        auto oversizedData = ReadAll(oversizedPath);
        for (size_t i = 24; i < 32; ++i) {
            oversizedData[i] = 0xff;
        }
        success &= Expect(WriteAll(oversizedPath, oversizedData),
                          "inject oversized component length");
        const auto oversizedBeforeVerify = ReadAll(oversizedPath);
        success &= Expect(!manager.VerifyPassword("oversized", password),
                          "reject malicious v5 component length");
        success &= Expect(ReadAll(oversizedPath) == oversizedBeforeVerify,
                          "malicious v5 input is never rewritten");

        success &= Expect(manager.CreateUser("trailing", password),
                          "create trailing-data v5 fixture");
        const fs::path trailingPath = testRoot / "users/trailing.enc";
        auto trailingData = ReadAll(trailingPath);
        trailingData.push_back(0x00);
        success &= Expect(WriteAll(trailingPath, trailingData),
                          "append unexpected byte to v5 file");
        success &= Expect(!manager.VerifyPassword("trailing", password),
                          "reject data after declared v5 total length");

        success &= Expect(manager.CreateUser("bad_total", password),
                          "create total-length v5 fixture");
        const fs::path badTotalPath = testRoot / "users/bad_total.enc";
        auto badTotalData = ReadAll(badTotalPath);
        badTotalData[19] ^= 0x01;
        success &= Expect(WriteAll(badTotalPath, badTotalData),
                          "corrupt declared v5 total length");
        success &= Expect(!manager.VerifyPassword("bad_total", password),
                          "reject inconsistent v5 total length");

        const std::string newPassword = "new correct horse battery staple";
        const std::vector<uint8_t> aliceBeforeInterruptedChange = ReadAll(alicePath);
        AtomicFile::Testing::FailNextWriteBeforeReplace();
        success &= Expect(!manager.ChangeMasterPassword("alice", password, newPassword),
                          "simulate interrupted user-password update");
        success &= Expect(ReadAll(alicePath) == aliceBeforeInterruptedChange,
                          "interrupted password update preserves user file byte-for-byte");
        success &= Expect(manager.VerifyPassword("alice", password),
                          "old password remains valid after interrupted update");
        success &= Expect(!manager.VerifyPassword("alice", newPassword),
                          "new password is not partially published");

        success &= Expect(manager.ChangeMasterPassword("alice", password, newPassword),
                          "change master password using portable v5 ML-KEM format");
        success &= Expect(HasV5Magic(alicePath) && ReadVersion(alicePath) == 5,
                          "retain v5 format after password change");
        success &= Expect(!manager.VerifyPassword("alice", password),
                          "reject old password after change");
        success &= Expect(manager.VerifyPassword("alice", newPassword),
                          "verify new password after change");
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
