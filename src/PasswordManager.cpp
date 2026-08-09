#include "PasswordManager.h"
#include "AtomicFile.h"
#include "CryptoArchive.h"
#include "EncryptedDatabase.h"
#include "FormatValidation.h"
#include "PathSecurity.h"
#include "SecureMemory.h"
#include "TransactionalFileBatch.h"
#include <oqs/oqs.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <openssl/crypto.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <array>

namespace {

constexpr uint64_t MAX_COMPONENT_SIZE = 16ULL * 1024ULL * 1024ULL;
constexpr uint64_t MAX_USER_FILE_SIZE = 64ULL * 1024ULL * 1024ULL;
constexpr std::array<uint8_t, 8> USER_V5_MAGIC = {'P', 'Q', 'C', 'U', 'S', 'R', '0', '5'};
constexpr uint32_t USER_V5_COMPONENT_COUNT = 9;
constexpr size_t USER_V5_FIXED_HEADER_SIZE = 24;

void Cleanse(std::vector<uint8_t>& data) {
    if (!data.empty()) {
        OPENSSL_cleanse(data.data(), data.size());
    }
}

void AppendUint32(std::vector<uint8_t>& output, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        output.push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
}

void AppendUint64(std::vector<uint8_t>& output, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output.push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
}

bool ReadUint32(const std::vector<uint8_t>& input, size_t& offset, uint32_t& value) {
    if (offset > input.size() || input.size() - offset < sizeof(uint32_t)) {
        return false;
    }
    value = 0;
    for (size_t i = 0; i < sizeof(uint32_t); ++i) {
        value = (value << 8U) | input[offset++];
    }
    return true;
}

bool ReadUint64(const std::vector<uint8_t>& input, size_t& offset, uint64_t& value) {
    if (offset > input.size() || input.size() - offset < sizeof(uint64_t)) {
        return false;
    }
    value = 0;
    for (size_t i = 0; i < sizeof(uint64_t); ++i) {
        value = (value << 8U) | input[offset++];
    }
    return true;
}

bool ReadPortableComponent(const std::vector<uint8_t>& input, size_t& offset,
                           std::vector<uint8_t>& value) {
    uint64_t size = 0;
    if (!ReadUint64(input, offset, size) || size == 0 || size > MAX_COMPONENT_SIZE ||
        size > input.size() - offset) {
        return false;
    }
    value.assign(input.begin() + static_cast<std::ptrdiff_t>(offset),
                 input.begin() + static_cast<std::ptrdiff_t>(offset + size));
    offset += static_cast<size_t>(size);
    return true;
}

bool DecodeV5(const std::vector<uint8_t>& input,
              PasswordManager::EncryptedPassword& data) {
    if (input.size() < USER_V5_FIXED_HEADER_SIZE ||
        input.size() > MAX_USER_FILE_SIZE ||
        !std::equal(USER_V5_MAGIC.begin(), USER_V5_MAGIC.end(), input.begin())) {
        return false;
    }

    size_t offset = USER_V5_MAGIC.size();
    uint32_t version = 0;
    uint64_t totalSize = 0;
    uint32_t componentCount = 0;
    if (!ReadUint32(input, offset, version) ||
        !ReadUint64(input, offset, totalSize) ||
        !ReadUint32(input, offset, componentCount) ||
        version != 5 || totalSize != input.size() ||
        componentCount != USER_V5_COMPONENT_COUNT) {
        return false;
    }

    PasswordManager::EncryptedPassword candidate;
    candidate.version = version;
    if (!ReadPortableComponent(input, offset, candidate.salt) ||
        !ReadPortableComponent(input, offset, candidate.secret_key_nonce) ||
        !ReadPortableComponent(input, offset, candidate.password_nonce) ||
        !ReadPortableComponent(input, offset, candidate.ciphertext) ||
        !ReadPortableComponent(input, offset, candidate.public_key) ||
        !ReadPortableComponent(input, offset, candidate.encrypted_secret_key) ||
        !ReadPortableComponent(input, offset, candidate.encrypted_password) ||
        !ReadPortableComponent(input, offset, candidate.secret_key_auth_tag) ||
        !ReadPortableComponent(input, offset, candidate.password_auth_tag) ||
        offset != input.size()) {
        return false;
    }
    data = std::move(candidate);
    return true;
}

bool ReadVector(std::ifstream& file, std::vector<uint8_t>& value) {
    uint64_t size = 0;
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (!file || size > MAX_COMPONENT_SIZE ||
        size > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max())) {
        return false;
    }

    value.resize(static_cast<size_t>(size));
    if (size > 0) {
        file.read(reinterpret_cast<char*>(value.data()), static_cast<std::streamsize>(size));
    }
    return file.good();
}

bool ReadLegacyVector(std::ifstream& file, std::vector<uint8_t>& value) {
    size_t size = 0;
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (!file || size > MAX_COMPONENT_SIZE ||
        size > static_cast<size_t>(std::numeric_limits<std::streamsize>::max())) {
        return false;
    }

    value.resize(size);
    if (size > 0) {
        file.read(reinterpret_cast<char*>(value.data()), static_cast<std::streamsize>(size));
    }
    return file.good();
}

} // namespace

PasswordManager::PasswordManager()
    : transaction_recovery_ready_(
          TransactionalFileBatch::RecoverPendingTransactions()) {
    if (!transaction_recovery_ready_) {
        std::cerr << "Warning: an incomplete password transaction could not be recovered"
                  << std::endl;
    }
    EnsureUsersDirectory();
}

PasswordManager::~PasswordManager() {
}

bool PasswordManager::UserExists(const std::string& username) const {
    const std::string path = GetUserFilePath(username);
    return !path.empty() && std::filesystem::is_regular_file(path);
}

std::vector<uint8_t> PasswordManager::GenerateRandomBytes(size_t length) const {
    if (length == 0 || length > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    std::vector<uint8_t> bytes(length);
    if (RAND_bytes(bytes.data(), static_cast<int>(length)) != 1) {
        std::cerr << "Failed to generate random bytes" << std::endl;
        return {};
    }
    return bytes;
}

std::vector<uint8_t> PasswordManager::DeriveKey(const std::string& password, const std::vector<uint8_t>& salt) const {
    std::vector<uint8_t> key(32); // 256-bit key
    
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_SCRYPT, nullptr);
    if (!ctx) {
        Cleanse(key);
        return {};
    }
    
    if (EVP_PKEY_derive_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        Cleanse(key);
        return {};
    }
    
    if (EVP_PKEY_CTX_set1_pbe_pass(ctx, password.c_str(), password.length()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        Cleanse(key);
        return {};
    }
    
    if (EVP_PKEY_CTX_set1_scrypt_salt(ctx, salt.data(), salt.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        Cleanse(key);
        return {};
    }
    
    // Scrypt parameters: N=32768, r=8, p=1 (strong parameters)
    if (EVP_PKEY_CTX_set_scrypt_N(ctx, 32768) <= 0 ||
        EVP_PKEY_CTX_set_scrypt_r(ctx, 8) <= 0 ||
        EVP_PKEY_CTX_set_scrypt_p(ctx, 1) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        Cleanse(key);
        return {};
    }
    
    size_t keylen = key.size();
    if (EVP_PKEY_derive(ctx, key.data(), &keylen) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        Cleanse(key);
        return {};
    }
    
    EVP_PKEY_CTX_free(ctx);
    return key;
}

std::vector<uint8_t> PasswordManager::AESEncrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key, 
                                                 const std::vector<uint8_t>& iv, std::vector<uint8_t>& tag) const {
    if (data.empty() || key.size() != 32 ||
        (iv.size() != NONCE_SIZE && iv.size() != 16) ||
        data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return {};
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(iv.size()), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    std::vector<uint8_t> ciphertext(data.size() + EVP_CIPHER_block_size(EVP_aes_256_gcm()));
    int len;
    int ciphertext_len;
    
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, data.data(),
                          static_cast<int>(data.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    ciphertext_len = len;
    
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    ciphertext_len += len;
    
    tag.resize(TAG_SIZE);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    EVP_CIPHER_CTX_free(ctx);
    ciphertext.resize(ciphertext_len);
    return ciphertext;
}

std::vector<uint8_t> PasswordManager::AESDecrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key,
                                                 const std::vector<uint8_t>& iv, const std::vector<uint8_t>& tag) const {
    if (data.empty() || key.size() != 32 || tag.size() != TAG_SIZE ||
        (iv.size() != NONCE_SIZE && iv.size() != 16) ||
        data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return {};
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(iv.size()), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    std::vector<uint8_t> plaintext(data.size());
    int len;
    int plaintext_len;
    
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, data.data(),
                          static_cast<int>(data.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        Cleanse(plaintext);
        return {};
    }
    plaintext_len = len;
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
                            static_cast<int>(tag.size()),
                            const_cast<uint8_t*>(tag.data())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        Cleanse(plaintext);
        return {};
    }
    
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        Cleanse(plaintext);
        return {}; // Authentication failed
    }
    plaintext_len += len;
    
    EVP_CIPHER_CTX_free(ctx);
    plaintext.resize(plaintext_len);
    return plaintext;
}

bool PasswordManager::BuildEncryptedPassword(const std::string& password,
                                             EncryptedPassword& data) const {
    if (password.empty()) {
        return false;
    }

    std::unique_ptr<OQS_KEM, decltype(&OQS_KEM_free)> kem(
        OQS_KEM_new(OQS_KEM_alg_ml_kem_768), OQS_KEM_free);
    if (!kem) {
        std::cerr << "Failed to initialize ML-KEM-768" << std::endl;
        return false;
    }

    EncryptedPassword candidate;
    candidate.version = CURRENT_VERSION;
    candidate.salt = GenerateRandomBytes(SALT_SIZE);
    candidate.secret_key_nonce = GenerateRandomBytes(NONCE_SIZE);
    candidate.password_nonce = GenerateRandomBytes(NONCE_SIZE);
    if (candidate.salt.size() != SALT_SIZE ||
        candidate.secret_key_nonce.size() != NONCE_SIZE ||
        candidate.password_nonce.size() != NONCE_SIZE ||
        candidate.secret_key_nonce == candidate.password_nonce) {
        std::cerr << "Failed to generate independent GCM nonces" << std::endl;
        return false;
    }

    std::vector<uint8_t> derivedKey = DeriveKey(password, candidate.salt);
    SecureMemory::ScopedCleanse derivedKeyGuard(derivedKey);
    if (derivedKey.empty()) {
        return false;
    }

    candidate.public_key.resize(kem->length_public_key);
    std::vector<uint8_t> secretKey(kem->length_secret_key);
    SecureMemory::ScopedCleanse secretKeyGuard(secretKey);
    if (OQS_KEM_keypair(kem.get(), candidate.public_key.data(), secretKey.data()) != OQS_SUCCESS) {
        Cleanse(derivedKey);
        Cleanse(secretKey);
        return false;
    }

    candidate.encrypted_secret_key =
        AESEncrypt(secretKey, derivedKey, candidate.secret_key_nonce,
                   candidate.secret_key_auth_tag);
    Cleanse(secretKey);
    if (candidate.encrypted_secret_key.empty()) {
        Cleanse(derivedKey);
        return false;
    }

    candidate.ciphertext.resize(kem->length_ciphertext);
    std::vector<uint8_t> sharedSecret(kem->length_shared_secret);
    SecureMemory::ScopedCleanse sharedSecretGuard(sharedSecret);
    if (OQS_KEM_encaps(kem.get(), candidate.ciphertext.data(), sharedSecret.data(),
                       candidate.public_key.data()) != OQS_SUCCESS) {
        Cleanse(derivedKey);
        Cleanse(sharedSecret);
        return false;
    }

    std::vector<uint8_t> xorEncrypted = XOREncrypt(password, sharedSecret);
    SecureMemory::ScopedCleanse xorEncryptedGuard(xorEncrypted);
    candidate.encrypted_password =
        AESEncrypt(xorEncrypted, derivedKey, candidate.password_nonce,
                   candidate.password_auth_tag);
    Cleanse(derivedKey);
    Cleanse(sharedSecret);
    Cleanse(xorEncrypted);
    if (candidate.encrypted_password.empty()) {
        return false;
    }

    data = std::move(candidate);
    return true;
}

bool PasswordManager::ValidateEncryptedPassword(const EncryptedPassword& data,
                                                const std::string& password) const {
    if (password.empty() || data.version != CURRENT_VERSION ||
        data.salt.size() != SALT_SIZE || data.secret_key_nonce.size() != NONCE_SIZE ||
        data.password_nonce.size() != NONCE_SIZE ||
        data.secret_key_nonce == data.password_nonce ||
        data.secret_key_auth_tag.size() != TAG_SIZE ||
        data.password_auth_tag.size() != TAG_SIZE) {
        return false;
    }

    std::vector<uint8_t> derivedKey = DeriveKey(password, data.salt);
    SecureMemory::ScopedCleanse derivedKeyGuard(derivedKey);
    std::vector<uint8_t> secretKey = AESDecrypt(data.encrypted_secret_key, derivedKey,
                                                data.secret_key_nonce,
                                                data.secret_key_auth_tag);
    SecureMemory::ScopedCleanse secretKeyGuard(secretKey);
    if (derivedKey.empty() || secretKey.empty()) {
        return false;
    }

    std::unique_ptr<OQS_KEM, decltype(&OQS_KEM_free)> kem(
        OQS_KEM_new(OQS_KEM_alg_ml_kem_768), OQS_KEM_free);
    if (!kem || data.ciphertext.size() != kem->length_ciphertext ||
        data.public_key.size() != kem->length_public_key ||
        secretKey.size() != kem->length_secret_key) {
        return false;
    }

    std::vector<uint8_t> sharedSecret(kem->length_shared_secret);
    SecureMemory::ScopedCleanse sharedSecretGuard(sharedSecret);
    if (OQS_KEM_decaps(kem.get(), sharedSecret.data(), data.ciphertext.data(),
                       secretKey.data()) != OQS_SUCCESS) {
        return false;
    }

    std::vector<uint8_t> protectedPassword =
        AESDecrypt(data.encrypted_password, derivedKey, data.password_nonce,
                   data.password_auth_tag);
    SecureMemory::ScopedCleanse protectedPasswordGuard(protectedPassword);
    if (protectedPassword.size() != password.size()) {
        return false;
    }

    uint8_t difference = 0;
    for (size_t i = 0; i < protectedPassword.size(); ++i) {
        const uint8_t recovered =
            protectedPassword[i] ^ sharedSecret[i % sharedSecret.size()];
        difference |= recovered ^ static_cast<uint8_t>(password[i]);
    }
    return difference == 0;
}

bool PasswordManager::CreateUser(const std::string& username, const std::string& password) {
    if (!transaction_recovery_ready_) {
        std::cerr << "Cannot create a user while transaction recovery is incomplete" << std::endl;
        return false;
    }
    std::string validationError;
    if (!PathSecurity::ValidateUsername(username, &validationError)) {
        std::cerr << "Invalid username: " << validationError << std::endl;
        return false;
    }
    for (const auto& existingUsername : GetUsernames()) {
        if (PathSecurity::NamesCollide(username, existingUsername)) {
            std::cerr << "A user with an equivalent name already exists" << std::endl;
            return false;
        }
    }
    if (UserExists(username)) {
        std::cerr << "User already exists: " << username << std::endl;
        return false;
    }

    EncryptedPassword data;
    if (!BuildEncryptedPassword(password, data) || !SaveEncryptedData(username, data)) {
        std::cerr << "Failed to create encrypted user: " << username << std::endl;
        return false;
    }

    std::filesystem::permissions(GetUserFilePath(username),
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);
    std::cout << "User created with ML-KEM-768 and independent AES-GCM nonces: "
              << username << std::endl;
    return true;
}

bool PasswordManager::VerifyPassword(const std::string& username, const std::string& password) const {
    if (!transaction_recovery_ready_) {
        std::cerr << "Cannot authenticate while transaction recovery is incomplete" << std::endl;
        return false;
    }
    if (password.empty() || !PathSecurity::ValidateUsername(username) ||
        !UserExists(username)) {
        std::cerr << "User does not exist: " << username << std::endl;
        return false;
    }
    
    auto encData = LoadEncryptedData(username);
    if (encData.ciphertext.empty()) {
        std::cerr << "Failed to load user data: " << username << std::endl;
        return false;
    }
    
    if (encData.version != CURRENT_VERSION &&
        encData.version != ML_KEM_VERSION &&
        encData.version != AES_GCM_VERSION &&
        encData.version != PREVIOUS_VERSION) {
        std::cout << "Attempting to verify password with legacy format..." << std::endl;
        const bool legacyMatch = VerifyPasswordLegacy(username, password);
        if (legacyMatch) {
            EncryptedPassword migratedData;
            if (BuildEncryptedPassword(password, migratedData) &&
                SaveEncryptedData(username, migratedData)) {
                std::cout << "Migrated v1 Kyber user file to portable v5 with ML-KEM-768"
                          << std::endl;
            } else {
                std::cerr << "Warning: authentication succeeded but v1 migration failed"
                          << std::endl;
            }
        }
        return legacyMatch;
    }

    if (encData.salt.size() != SALT_SIZE ||
        encData.secret_key_nonce.empty() || encData.password_nonce.empty() ||
        encData.secret_key_auth_tag.size() != TAG_SIZE ||
        encData.password_auth_tag.size() != TAG_SIZE) {
        std::cerr << "Invalid encrypted user parameters" << std::endl;
        return false;
    }

    if ((encData.version == CURRENT_VERSION || encData.version == ML_KEM_VERSION ||
         encData.version == AES_GCM_VERSION) &&
        (encData.secret_key_nonce.size() != NONCE_SIZE ||
         encData.password_nonce.size() != NONCE_SIZE ||
         encData.secret_key_nonce == encData.password_nonce)) {
        std::cerr << "Invalid or reused AES-GCM nonce in user file" << std::endl;
        return false;
    }

    std::vector<uint8_t> derivedKey = DeriveKey(password, encData.salt);
    SecureMemory::ScopedCleanse derivedKeyGuard(derivedKey);
    if (derivedKey.empty()) {
        std::cerr << "Failed to derive key" << std::endl;
        return false;
    }

    std::vector<uint8_t> secretKey =
        AESDecrypt(encData.encrypted_secret_key, derivedKey,
                   encData.secret_key_nonce, encData.secret_key_auth_tag);
    SecureMemory::ScopedCleanse secretKeyGuard(secretKey);
    if (secretKey.empty()) {
        Cleanse(derivedKey);
        std::cerr << "Failed to decrypt secret key - wrong password" << std::endl;
        return false;
    }

    const char* kemAlgorithm =
        (encData.version == CURRENT_VERSION || encData.version == ML_KEM_VERSION)
        ? OQS_KEM_alg_ml_kem_768
        : OQS_KEM_alg_kyber_768;
    std::unique_ptr<OQS_KEM, decltype(&OQS_KEM_free)> kem(
        OQS_KEM_new(kemAlgorithm), OQS_KEM_free);
    if (!kem) {
        Cleanse(derivedKey);
        Cleanse(secretKey);
        std::cerr << "Failed to initialize KEM: " << kemAlgorithm << std::endl;
        return false;
    }

    if (encData.ciphertext.size() != kem->length_ciphertext ||
        encData.public_key.size() != kem->length_public_key ||
        secretKey.size() != kem->length_secret_key) {
        Cleanse(derivedKey);
        Cleanse(secretKey);
        std::cerr << "Encrypted user KEM component sizes are invalid" << std::endl;
        return false;
    }

    std::vector<uint8_t> sharedSecret(kem->length_shared_secret);
    SecureMemory::ScopedCleanse sharedSecretGuard(sharedSecret);
    if (OQS_KEM_decaps(kem.get(), sharedSecret.data(), encData.ciphertext.data(),
                       secretKey.data()) != OQS_SUCCESS) {
        Cleanse(derivedKey);
        Cleanse(secretKey);
        Cleanse(sharedSecret);
        std::cerr << "Failed to decapsulate" << std::endl;
        return false;
    }

    std::vector<uint8_t> aesDecrypted =
        AESDecrypt(encData.encrypted_password, derivedKey,
                   encData.password_nonce, encData.password_auth_tag);
    SecureMemory::ScopedCleanse aesDecryptedGuard(aesDecrypted);
    Cleanse(derivedKey);
    Cleanse(secretKey);
    if (aesDecrypted.empty()) {
        Cleanse(sharedSecret);
        std::cerr << "Failed to decrypt password with AES - authentication failed" << std::endl;
        return false;
    }

    bool match = aesDecrypted.size() == password.size();
    uint8_t difference = 0;
    if (match) {
        for (size_t i = 0; i < aesDecrypted.size(); ++i) {
            const uint8_t recovered = aesDecrypted[i] ^ sharedSecret[i % sharedSecret.size()];
            difference |= recovered ^ static_cast<uint8_t>(password[i]);
        }
        match = difference == 0;
    }
    Cleanse(aesDecrypted);
    Cleanse(sharedSecret);

    if (match) {
        std::cout << "Password verified successfully for user: " << username << std::endl;
        if (encData.version != CURRENT_VERSION) {
            EncryptedPassword migratedData;
            if (BuildEncryptedPassword(password, migratedData) &&
                SaveEncryptedData(username, migratedData)) {
                std::cout << "Migrated user file to portable v5 with ML-KEM-768"
                          << std::endl;
            } else {
                std::cerr << "Warning: authentication succeeded but ML-KEM migration failed"
                          << std::endl;
            }
        }
    } else {
        std::cerr << "Password verification failed for user: " << username << std::endl;
    }
    
    return match;
}

// Legacy support for old format
bool PasswordManager::VerifyPasswordLegacy(const std::string& username, const std::string& password) const {
    std::cout << "Using legacy verification for old format file..." << std::endl;
    
    // Load old format data
    if (!PathSecurity::ValidateUsername(username)) {
        return false;
    }
    std::string filepath = GetUserFilePath(username);
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return false;
    
    // Old format structure
    struct OldEncryptedPassword {
        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> public_key;
        std::vector<uint8_t> secret_key;
        std::vector<uint8_t> encrypted_password;
    } oldData;
    SecureMemory::ScopedCleanse oldSecretKeyGuard(oldData.secret_key);
    
    if (!ReadLegacyVector(file, oldData.ciphertext) ||
        !ReadLegacyVector(file, oldData.public_key) ||
        !ReadLegacyVector(file, oldData.secret_key) ||
        !ReadLegacyVector(file, oldData.encrypted_password) ||
        file.peek() != std::char_traits<char>::eof()) {
        return false;
    }
    
    // Initialize Kyber KEM
    OQS_KEM* kem = OQS_KEM_new(OQS_KEM_alg_kyber_768);
    if (!kem) return false;

    if (oldData.ciphertext.size() != kem->length_ciphertext ||
        oldData.public_key.size() != kem->length_public_key ||
        oldData.secret_key.size() != kem->length_secret_key ||
        oldData.encrypted_password.empty()) {
        Cleanse(oldData.secret_key);
        OQS_KEM_free(kem);
        return false;
    }
    
    // Decapsulate to get shared secret
    std::vector<uint8_t> shared_secret(kem->length_shared_secret);
    SecureMemory::ScopedCleanse sharedSecretGuard(shared_secret);
    if (OQS_KEM_decaps(kem, shared_secret.data(), oldData.ciphertext.data(), oldData.secret_key.data()) != OQS_SUCCESS) {
        Cleanse(oldData.secret_key);
        Cleanse(shared_secret);
        OQS_KEM_free(kem);
        return false;
    }
    
    bool match = oldData.encrypted_password.size() == password.size();
    uint8_t difference = 0;
    if (match) {
        for (size_t i = 0; i < oldData.encrypted_password.size(); ++i) {
            const uint8_t recovered =
                oldData.encrypted_password[i] ^ shared_secret[i % shared_secret.size()];
            difference |= recovered ^ static_cast<uint8_t>(password[i]);
        }
        match = difference == 0;
    }

    Cleanse(oldData.secret_key);
    Cleanse(shared_secret);
    OQS_KEM_free(kem);
    if (match) {
        std::cout << "Legacy password verified. Consider migrating to new format." << std::endl;
    }
    
    return match;
}

bool PasswordManager::HasAnyUsers() const {
    if (!std::filesystem::exists("users")) {
        return false;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator("users")) {
        if (entry.is_regular_file() && !entry.is_symlink() &&
            entry.path().extension() == ".enc" &&
            PathSecurity::ValidateUsername(entry.path().stem().string())) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> PasswordManager::GetUsernames() const {
    std::vector<std::string> usernames;
    
    if (!std::filesystem::exists("users")) {
        return usernames;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator("users")) {
        if (entry.is_regular_file() && !entry.is_symlink() &&
            entry.path().extension() == ".enc") {
            std::string filename = entry.path().stem().string();
            if (!PathSecurity::ValidateUsername(filename)) {
                continue;
            }
            const bool collision = std::any_of(
                usernames.begin(), usernames.end(), [&](const std::string& existing) {
                    return PathSecurity::NamesCollide(existing, filename);
                });
            if (!collision) {
                usernames.push_back(filename);
            }
        }
    }
    
    return usernames;
}

std::vector<uint8_t> PasswordManager::XOREncrypt(const std::string& data, const std::vector<uint8_t>& key) const {
    std::vector<uint8_t> result(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        result[i] = static_cast<uint8_t>(data[i]) ^ key[i % key.size()];
    }
    return result;
}

bool PasswordManager::SaveEncryptedData(const std::string& username, const EncryptedPassword& data) const {
    std::vector<uint8_t> encoded;
    if (!EncodeEncryptedData(data, encoded)) {
        return false;
    }

    const std::string filepath = GetUserFilePath(username);
    if (filepath.empty()) {
        return false;
    }
    if (!AtomicFile::Write(filepath, encoded)) {
        std::cerr << "Failed to atomically save encrypted user data: " << filepath << std::endl;
        return false;
    }
    return true;
}

bool PasswordManager::EncodeEncryptedData(const EncryptedPassword& data,
                                          std::vector<uint8_t>& encoded) const {
    if (data.version != CURRENT_VERSION || data.salt.size() != SALT_SIZE ||
        data.secret_key_nonce.size() != NONCE_SIZE ||
        data.password_nonce.size() != NONCE_SIZE ||
        data.secret_key_nonce == data.password_nonce ||
        data.secret_key_auth_tag.size() != TAG_SIZE ||
        data.password_auth_tag.size() != TAG_SIZE) {
        std::cerr << "Refusing to save invalid v5 encrypted user data" << std::endl;
        return false;
    }

    const std::array<const std::vector<uint8_t>*, USER_V5_COMPONENT_COUNT> components = {
        &data.salt,
        &data.secret_key_nonce,
        &data.password_nonce,
        &data.ciphertext,
        &data.public_key,
        &data.encrypted_secret_key,
        &data.encrypted_password,
        &data.secret_key_auth_tag,
        &data.password_auth_tag
    };
    uint64_t totalSize = USER_V5_FIXED_HEADER_SIZE;
    for (const auto* component : components) {
        if (component == nullptr || component->empty() ||
            component->size() > MAX_COMPONENT_SIZE ||
            totalSize > MAX_USER_FILE_SIZE - sizeof(uint64_t) - component->size()) {
            return false;
        }
        totalSize += sizeof(uint64_t) + component->size();
    }

    encoded.clear();
    encoded.reserve(static_cast<size_t>(totalSize));
    encoded.insert(encoded.end(), USER_V5_MAGIC.begin(), USER_V5_MAGIC.end());
    AppendUint32(encoded, CURRENT_VERSION);
    AppendUint64(encoded, totalSize);
    AppendUint32(encoded, USER_V5_COMPONENT_COUNT);
    for (const auto* component : components) {
        AppendUint64(encoded, component->size());
        encoded.insert(encoded.end(), component->begin(), component->end());
    }
    return encoded.size() == totalSize;
}

PasswordManager::EncryptedPassword PasswordManager::LoadEncryptedData(const std::string& username) const {
    EncryptedPassword data{};
    std::string filepath = GetUserFilePath(username);
    if (filepath.empty()) {
        return data;
    }
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file for reading: " << filepath << std::endl;
        return data;
    }
    
    file.seekg(0, std::ios::end);
    const std::streamoff endPosition = file.tellg();
    if (endPosition <= 0 || static_cast<uint64_t>(endPosition) > MAX_USER_FILE_SIZE) {
        return {};
    }
    const size_t fileSize = static_cast<size_t>(endPosition);
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> encoded(fileSize);
    file.read(reinterpret_cast<char*>(encoded.data()),
              static_cast<std::streamsize>(encoded.size()));
    if (!file || file.gcount() != static_cast<std::streamsize>(encoded.size())) {
        return {};
    }

    FormatValidation::UserFormat format = FormatValidation::UserFormat::Invalid;
    if (!FormatValidation::ValidateUserFile(encoded.data(), encoded.size(), &format)) {
        return {};
    }
    if (format == FormatValidation::UserFormat::V5) {
        return DecodeV5(encoded, data) ? data : EncryptedPassword{};
    }

    file.clear();
    file.seekg(0, std::ios::beg);

    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    if (!file) {
        return {};
    }

    if (version == ML_KEM_VERSION || version == AES_GCM_VERSION) {
        data.version = version;
        if (!ReadVector(file, data.salt) ||
            !ReadVector(file, data.secret_key_nonce) ||
            !ReadVector(file, data.password_nonce) ||
            !ReadVector(file, data.ciphertext) ||
            !ReadVector(file, data.public_key) ||
            !ReadVector(file, data.encrypted_secret_key) ||
            !ReadVector(file, data.encrypted_password) ||
            !ReadVector(file, data.secret_key_auth_tag) ||
            !ReadVector(file, data.password_auth_tag) ||
            file.peek() != std::char_traits<char>::eof()) {
            return {};
        }
    } else if (version == PREVIOUS_VERSION) {
        data.version = version;
        std::vector<uint8_t> legacyIv;
        std::vector<uint8_t> combinedTags;
        if (!ReadLegacyVector(file, data.salt) ||
            !ReadLegacyVector(file, legacyIv) ||
            !ReadLegacyVector(file, data.ciphertext) ||
            !ReadLegacyVector(file, data.public_key) ||
            !ReadLegacyVector(file, data.encrypted_secret_key) ||
            !ReadLegacyVector(file, data.encrypted_password) ||
            !ReadLegacyVector(file, combinedTags) ||
            file.peek() != std::char_traits<char>::eof() ||
            data.salt.size() != SALT_SIZE || legacyIv.size() != 16 ||
            combinedTags.size() != TAG_SIZE * 2) {
            return {};
        }

        data.secret_key_nonce = legacyIv;
        data.password_nonce = legacyIv;
        data.password_auth_tag.assign(combinedTags.begin(),
                                      combinedTags.begin() + TAG_SIZE);
        data.secret_key_auth_tag.assign(combinedTags.begin() + TAG_SIZE,
                                        combinedTags.end());
    } else {
        data.version = 1;
        file.seekg(0, std::ios::beg);
        if (!ReadLegacyVector(file, data.ciphertext)) {
            return {};
        }
    }

    return data;
}

std::string PasswordManager::GetUserFilePath(const std::string& username) const {
    std::filesystem::path path;
    return PathSecurity::UserFilePath(username, path) ? path.string() : std::string{};
}

void PasswordManager::EnsureUsersDirectory() const {
    if (!std::filesystem::exists("users")) {
        std::filesystem::create_directories("users");
        // Set restrictive permissions on directory
        std::filesystem::permissions("users", std::filesystem::perms::owner_all);
    }
}

bool PasswordManager::ChangeMasterPassword(const std::string& username, const std::string& oldPassword, const std::string& newPassword) {
    std::filesystem::path databasePath;
    if (!PathSecurity::UserDatabasePath(username, databasePath)) {
        return false;
    }
    const std::string defaultDatabasePath = databasePath.string();
    if (std::filesystem::exists(defaultDatabasePath)) {
        EncryptedDatabase database(defaultDatabasePath, oldPassword);
        if (!database.initialize()) {
            std::cerr << "Cannot include the user's database in password transaction"
                      << std::endl;
            return false;
        }
        return ChangeMasterPassword(username, oldPassword, newPassword, &database);
    }
    return ChangeMasterPassword(username, oldPassword, newPassword, nullptr);
}

bool PasswordManager::ChangeMasterPassword(const std::string& username,
                                           const std::string& oldPassword,
                                           const std::string& newPassword,
                                           EncryptedDatabase* database) {
    std::cout << "\n---------- CHANGE MASTER PASSWORD ----------" << std::endl;
    std::cout << "Changing password for user: " << username << std::endl;

    if (!PathSecurity::ValidateUsername(username) || newPassword.empty() ||
        !TransactionalFileBatch::RecoverPendingTransactions() ||
        !VerifyPassword(username, oldPassword)) {
        std::cout << "Old password verification failed!" << std::endl;
        std::cout << "----------------------------------------\n" << std::endl;
        return false;
    }

    std::vector<TransactionalFileBatch::Entry> replacements;

    // Prepare and cryptographically validate the user file without publishing it.
    EncryptedPassword newPasswordData;
    std::vector<uint8_t> encodedUser;
    if (!BuildEncryptedPassword(newPassword, newPasswordData) ||
        !ValidateEncryptedPassword(newPasswordData, newPassword) ||
        !EncodeEncryptedData(newPasswordData, encodedUser)) {
        std::cout << "Failed to prepare new encrypted user data!" << std::endl;
        return false;
    }
    replacements.push_back({GetUserFilePath(username), std::move(encodedUser)});

    // Prepare and round-trip authenticate the database replacement.
    if (database != nullptr) {
        std::vector<uint8_t> encodedDatabase;
        if (!database->prepareMasterPasswordChange(oldPassword, newPassword,
                                                   encodedDatabase)) {
            std::cout << "Failed to prepare encrypted database replacement" << std::endl;
            return false;
        }
        replacements.push_back({database->getDatabasePath(), std::move(encodedDatabase)});
    }

    // Every archive is fully loaded with the old password, then re-encrypted in
    // memory under the new password. No archive file changes during this phase.
    const std::vector<std::string> userArchives = CryptoArchive::FindUserArchives(username);
    for (const std::string& archiveName : userArchives) {
        CryptoArchive archive(username, archiveName);
        if (!archive.LoadArchive(oldPassword)) {
            std::cout << "Failed to authenticate archive " << archiveName << std::endl;
            return false;
        }

        std::vector<uint8_t> encodedArchive;
        if (!archive.PreparePasswordChange(oldPassword, newPassword, encodedArchive)) {
            std::cout << "Failed to prepare archive " << archiveName << std::endl;
            return false;
        }
        replacements.push_back({archive.GetArchiveFilePath(), std::move(encodedArchive)});
    }

    if (!TransactionalFileBatch::Commit(replacements)) {
        std::cout << "Password transaction failed; original files remain active" << std::endl;
        return false;
    }

    if (database != nullptr && !database->completeMasterPasswordChange(newPassword)) {
        // Disk commit is already durable. Report the state accurately; the UI
        // will recreate this database owner using the newly accepted password.
        std::cerr << "Password transaction committed, but database memory refresh failed"
                  << std::endl;
    }

    std::cout << "Master password transaction committed for user, database and "
              << userArchives.size() << " archive(s)" << std::endl;
    std::cout << "----------------------------------------\n" << std::endl;
    return true;
}
