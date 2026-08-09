#include "EncryptedDatabase.h"
#include "AtomicFile.h"
#include "FormatValidation.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace {

constexpr std::array<uint8_t, 8> DATABASE_MAGIC = {'P', 'Q', 'C', 'D', 'B', '0', '0', '2'};
constexpr std::array<uint8_t, 8> BACKUP_MAGIC = {'P', 'Q', 'C', 'B', 'K', 'P', '0', '1'};
constexpr char LEGACY_DATABASE_HEADER[] = "PQCWALLET_DB_v1.0\n";
constexpr uint32_t DATABASE_FORMAT_VERSION = 2;
constexpr uint32_t BACKUP_FORMAT_VERSION = 1;
constexpr uint32_t KDF_SCRYPT = 1;
constexpr uint64_t SCRYPT_N = 32768;
constexpr uint32_t SCRYPT_R = 8;
constexpr uint32_t SCRYPT_P = 1;
constexpr uint64_t SCRYPT_MAX_MEMORY = 128ULL * 1024ULL * 1024ULL;
constexpr size_t KEY_SIZE = 32;
constexpr size_t SALT_SIZE = 32;
constexpr size_t NONCE_SIZE = 12;
constexpr size_t TAG_SIZE = 16;
constexpr size_t FIXED_HEADER_SIZE = 52;
constexpr uint64_t MAX_DATABASE_FILE_SIZE = 1024ULL * 1024ULL * 1024ULL;
constexpr uint64_t MAX_BACKUP_FILE_SIZE = 1024ULL * 1024ULL * 1024ULL;

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

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
    if (offset > input.size() || input.size() - offset < sizeof(value)) {
        return false;
    }
    value = 0;
    for (size_t i = 0; i < sizeof(value); ++i) {
        value = (value << 8U) | input[offset++];
    }
    return true;
}

bool ReadUint64(const std::vector<uint8_t>& input, size_t& offset, uint64_t& value) {
    if (offset > input.size() || input.size() - offset < sizeof(value)) {
        return false;
    }
    value = 0;
    for (size_t i = 0; i < sizeof(value); ++i) {
        value = (value << 8U) | input[offset++];
    }
    return true;
}

void Cleanse(std::vector<uint8_t>& data) {
    if (!data.empty()) {
        OPENSSL_cleanse(data.data(), data.size());
    }
}

bool DeriveDatabaseKey(const std::string& password,
                       const std::vector<uint8_t>& salt,
                       uint64_t n,
                       uint64_t r,
                       uint64_t p,
                       std::vector<uint8_t>& key) {
    if (password.empty() || salt.size() != SALT_SIZE || n != SCRYPT_N ||
        r != SCRYPT_R || p != SCRYPT_P) {
        return false;
    }

    key.assign(KEY_SIZE, 0);
    if (EVP_PBE_scrypt(password.data(), password.size(), salt.data(), salt.size(),
                       n, r, p, SCRYPT_MAX_MEMORY, key.data(), key.size()) != 1) {
        Cleanse(key);
        key.clear();
        return false;
    }
    return true;
}

std::vector<uint8_t> BuildAuthenticatedHeader(const std::array<uint8_t, 8>& magic,
                                              uint32_t formatVersion,
                                              uint64_t ciphertextSize,
                                              const std::vector<uint8_t>& salt,
                                              const std::vector<uint8_t>& nonce) {
    std::vector<uint8_t> header;
    header.reserve(FIXED_HEADER_SIZE + salt.size() + nonce.size());
    header.insert(header.end(), magic.begin(), magic.end());
    AppendUint32(header, formatVersion);
    AppendUint32(header, KDF_SCRYPT);
    AppendUint64(header, SCRYPT_N);
    AppendUint32(header, SCRYPT_R);
    AppendUint32(header, SCRYPT_P);
    AppendUint32(header, static_cast<uint32_t>(salt.size()));
    AppendUint32(header, static_cast<uint32_t>(nonce.size()));
    AppendUint32(header, static_cast<uint32_t>(TAG_SIZE));
    AppendUint64(header, ciphertextSize);
    header.insert(header.end(), salt.begin(), salt.end());
    header.insert(header.end(), nonce.begin(), nonce.end());
    return header;
}

bool EncryptAuthenticatedPayload(const std::array<uint8_t, 8>& magic,
                                 uint32_t formatVersion,
                                 const std::string& password,
                                 const std::string& plaintext,
                                 std::vector<uint8_t>& output) {
    if (plaintext.empty() || plaintext.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    std::vector<uint8_t> salt(SALT_SIZE);
    std::vector<uint8_t> nonce(NONCE_SIZE);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1 ||
        RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) != 1) {
        return false;
    }

    std::vector<uint8_t> key;
    SecureMemory::ScopedCleanse keyGuard(key);
    if (!DeriveDatabaseKey(password, salt, SCRYPT_N, SCRYPT_R, SCRYPT_P, key)) {
        return false;
    }

    std::vector<uint8_t> header =
        BuildAuthenticatedHeader(magic, formatVersion, plaintext.size(), salt, nonce);
    CipherContext context(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!context ||
        EVP_EncryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(nonce.size()), nullptr) != 1 ||
        EVP_EncryptInit_ex(context.get(), nullptr, nullptr, key.data(), nonce.data()) != 1) {
        Cleanse(key);
        return false;
    }

    int outputLength = 0;
    if (EVP_EncryptUpdate(context.get(), nullptr, &outputLength, header.data(),
                          static_cast<int>(header.size())) != 1) {
        Cleanse(key);
        return false;
    }

    std::vector<uint8_t> ciphertext(plaintext.size() + TAG_SIZE);
    int ciphertextLength = 0;
    if (EVP_EncryptUpdate(context.get(), ciphertext.data(), &outputLength,
                          reinterpret_cast<const uint8_t*>(plaintext.data()),
                          static_cast<int>(plaintext.size())) != 1) {
        Cleanse(key);
        return false;
    }
    ciphertextLength = outputLength;

    if (EVP_EncryptFinal_ex(context.get(), ciphertext.data() + ciphertextLength,
                            &outputLength) != 1) {
        Cleanse(key);
        return false;
    }
    ciphertextLength += outputLength;
    ciphertext.resize(static_cast<size_t>(ciphertextLength));

    std::vector<uint8_t> tag(TAG_SIZE);
    if (EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_GET_TAG,
                            static_cast<int>(tag.size()), tag.data()) != 1) {
        Cleanse(key);
        return false;
    }
    Cleanse(key);

    output = std::move(header);
    output.insert(output.end(), ciphertext.begin(), ciphertext.end());
    output.insert(output.end(), tag.begin(), tag.end());
    return true;
}

bool DecryptAuthenticatedPayload(const std::array<uint8_t, 8>& expectedMagic,
                                 uint32_t expectedVersion,
                                 const std::string& password,
                                 const std::vector<uint8_t>& input,
                                 std::string& plaintext) {
    if (input.size() < FIXED_HEADER_SIZE + SALT_SIZE + NONCE_SIZE + TAG_SIZE ||
        !std::equal(expectedMagic.begin(), expectedMagic.end(), input.begin())) {
        return false;
    }

    size_t offset = expectedMagic.size();
    uint32_t version = 0;
    uint32_t kdf = 0;
    uint64_t n = 0;
    uint32_t r = 0;
    uint32_t p = 0;
    uint32_t saltSize = 0;
    uint32_t nonceSize = 0;
    uint32_t tagSize = 0;
    uint64_t ciphertextSize = 0;
    if (!ReadUint32(input, offset, version) || !ReadUint32(input, offset, kdf) ||
        !ReadUint64(input, offset, n) || !ReadUint32(input, offset, r) ||
        !ReadUint32(input, offset, p) || !ReadUint32(input, offset, saltSize) ||
        !ReadUint32(input, offset, nonceSize) || !ReadUint32(input, offset, tagSize) ||
        !ReadUint64(input, offset, ciphertextSize)) {
        return false;
    }

    if (version != expectedVersion || kdf != KDF_SCRYPT ||
        n != SCRYPT_N || r != SCRYPT_R || p != SCRYPT_P ||
        saltSize != SALT_SIZE || nonceSize != NONCE_SIZE || tagSize != TAG_SIZE ||
        ciphertextSize == 0 || ciphertextSize > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    const uint64_t variableSize = static_cast<uint64_t>(saltSize) + nonceSize +
                                  ciphertextSize + tagSize;
    if (variableSize > input.size() - offset ||
        offset + static_cast<size_t>(variableSize) != input.size()) {
        return false;
    }

    std::vector<uint8_t> salt(input.begin() + static_cast<std::ptrdiff_t>(offset),
                              input.begin() + static_cast<std::ptrdiff_t>(offset + saltSize));
    offset += saltSize;
    std::vector<uint8_t> nonce(input.begin() + static_cast<std::ptrdiff_t>(offset),
                               input.begin() + static_cast<std::ptrdiff_t>(offset + nonceSize));
    offset += nonceSize;
    const size_t headerSize = offset;
    std::vector<uint8_t> ciphertext(
        input.begin() + static_cast<std::ptrdiff_t>(offset),
        input.begin() + static_cast<std::ptrdiff_t>(offset + ciphertextSize));
    offset += static_cast<size_t>(ciphertextSize);
    std::vector<uint8_t> tag(input.begin() + static_cast<std::ptrdiff_t>(offset), input.end());

    std::vector<uint8_t> key;
    SecureMemory::ScopedCleanse keyGuard(key);
    if (!DeriveDatabaseKey(password, salt, n, r, p, key)) {
        return false;
    }

    CipherContext context(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!context ||
        EVP_DecryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(nonce.size()), nullptr) != 1 ||
        EVP_DecryptInit_ex(context.get(), nullptr, nullptr, key.data(), nonce.data()) != 1) {
        Cleanse(key);
        return false;
    }

    int outputLength = 0;
    if (EVP_DecryptUpdate(context.get(), nullptr, &outputLength, input.data(),
                          static_cast<int>(headerSize)) != 1) {
        Cleanse(key);
        return false;
    }

    std::vector<uint8_t> decrypted(ciphertext.size() + TAG_SIZE);
    SecureMemory::ScopedCleanse decryptedGuard(decrypted);
    int plaintextLength = 0;
    if (EVP_DecryptUpdate(context.get(), decrypted.data(), &outputLength,
                          ciphertext.data(), static_cast<int>(ciphertext.size())) != 1) {
        Cleanse(key);
        Cleanse(decrypted);
        return false;
    }
    plaintextLength = outputLength;

    if (EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_TAG,
                            static_cast<int>(tag.size()), tag.data()) != 1 ||
        EVP_DecryptFinal_ex(context.get(), decrypted.data() + plaintextLength,
                            &outputLength) != 1) {
        Cleanse(key);
        Cleanse(decrypted);
        return false;
    }
    Cleanse(key);
    plaintextLength += outputLength;
    plaintext.assign(reinterpret_cast<const char*>(decrypted.data()),
                     static_cast<size_t>(plaintextLength));
    Cleanse(decrypted);
    return true;
}

bool EncryptDatabasePayload(const std::string& password,
                            const std::string& plaintext,
                            std::vector<uint8_t>& output) {
    return EncryptAuthenticatedPayload(DATABASE_MAGIC, DATABASE_FORMAT_VERSION,
                                       password, plaintext, output);
}

bool DecryptDatabasePayload(const std::string& password,
                            const std::vector<uint8_t>& input,
                            std::string& plaintext) {
    return FormatValidation::ValidateDatabaseV2(input.data(), input.size()) &&
           DecryptAuthenticatedPayload(DATABASE_MAGIC, DATABASE_FORMAT_VERSION,
                                       password, input, plaintext);
}

bool EncryptBackupPayload(const std::string& password,
                          const std::string& plaintext,
                          std::vector<uint8_t>& output) {
    return EncryptAuthenticatedPayload(BACKUP_MAGIC, BACKUP_FORMAT_VERSION,
                                       password, plaintext, output);
}

bool DecryptBackupPayload(const std::string& password,
                          const std::vector<uint8_t>& input,
                          std::string& plaintext) {
    return DecryptAuthenticatedPayload(BACKUP_MAGIC, BACKUP_FORMAT_VERSION,
                                       password, input, plaintext);
}

bool ReadBackupFile(const std::filesystem::path& path, std::vector<uint8_t>& output) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        return false;
    }
    const uintmax_t rawSize = std::filesystem::file_size(path, error);
    if (error || rawSize == 0 || rawSize > MAX_BACKUP_FILE_SIZE ||
        rawSize > static_cast<uintmax_t>(std::numeric_limits<size_t>::max()) ||
        rawSize > static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    output.resize(static_cast<size_t>(rawSize));
    file.read(reinterpret_cast<char*>(output.data()),
              static_cast<std::streamsize>(output.size()));
    return file.good() ||
           (file.eof() && file.gcount() == static_cast<std::streamsize>(output.size()));
}

void CleanseJson(SimpleJSON& json) {
    for (auto& [key, value] : json.data) {
        (void)key;
        SecureMemory::Cleanse(value);
    }
    json.data.clear();
}

bool ValidateDatabaseJson(const SimpleJSON& json) {
    if (!json.isMember("version") || json["version"] != "2.0" ||
        !json.isMember("created_at") || json["created_at"].empty() ||
        !json.isMember("algorithm") || json["algorithm"] != "scrypt/AES-256-GCM") {
        return false;
    }

    for (const auto& [key, value] : json.data) {
        if (key == "version" || key == "created_at" || key == "algorithm") {
            continue;
        }
        if (key.rfind("user_", 0) != 0 || key.size() <= 5 || value.empty()) {
            return false;
        }
        SimpleJSON record;
        if (!record.parseFromString(value) || !record.isMember("username") ||
            record["username"] != key.substr(5) || !record.isMember("email") ||
            !record.isMember("website") || !record.isMember("encrypted_password") ||
            record["encrypted_password"].empty() || !record.isMember("salt") ||
            record["salt"].empty() || !record.isMember("created_at") ||
            !record.isMember("last_login")) {
            CleanseJson(record);
            return false;
        }
        CleanseJson(record);
    }
    return true;
}

} // namespace

EncryptedDatabase::EncryptedDatabase(const std::string& database_path, const std::string& master_password)
    : database_path_(database_path), is_loaded_(false), is_modified_(false) {
    master_password_.assign(master_password);
}

EncryptedDatabase::~EncryptedDatabase() {
    CleanseJson(database_json_);
    master_password_.clear();
}

bool EncryptedDatabase::initialize() {
    if (master_password_.empty()) {
        std::cerr << "[X] Database master password cannot be empty" << std::endl;
        return false;
    }

    const bool databaseExists = std::filesystem::exists(database_path_);
    if (databaseExists) {
        if (!loadDatabase()) {
            std::cerr << "[X] Existing database could not be authenticated; it was not modified"
                      << std::endl;
            return false;
        }

        if (is_modified_ && !saveDatabase()) {
            std::cerr << "[X] Failed to migrate legacy database" << std::endl;
            return false;
        }
    } else {
        std::cout << "[NEW] Creating encrypted database..." << std::endl;
        database_json_.data["version"] = "2.0";
        database_json_.data["created_at"] = std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        database_json_.data["algorithm"] = "scrypt/AES-256-GCM";
        is_loaded_ = true;
        is_modified_ = true;

        if (!saveDatabase()) {
            std::cerr << "[X] Failed to save new database" << std::endl;
            return false;
        }
    }

    std::cout << "[OK] Encrypted Database initialized successfully!" << std::endl;
    return true;
}

bool EncryptedDatabase::addUser(const UserRecord& record) {
    if (!is_loaded_) {
        std::cerr << "[X] Database not loaded" << std::endl;
        return false;
    }
    
    std::cout << "[USER] Adding user: " << record.username << std::endl;
    
    // Check if user already exists
    std::string user_key = "user_" + record.username;
    if (database_json_.isMember(user_key)) {
        std::cerr << "[X] User already exists: " << record.username << std::endl;
        return false;
    }
    
    // Create user record
    SimpleJSON user_data;
    user_data["username"] = record.username;
    user_data["email"] = record.email;
    user_data["website"] = record.website;
    user_data["encrypted_password"] = record.encrypted_password;
    user_data["salt"] = record.salt;
    user_data["created_at"] = record.created_at;
    user_data["last_login"] = record.last_login;
    
    const bool previousModifiedState = is_modified_;

    // Add to database
    database_json_.data[user_key] = user_data.toJsonString();
    
    is_modified_ = true;
    
    // Save database
    if (!saveDatabase()) {
        database_json_.data.erase(user_key);
        is_modified_ = previousModifiedState;
        std::cerr << "[X] Failed to save database after adding user" << std::endl;
        return false;
    }
    
    std::cout << "[OK] User added successfully: " << record.username << std::endl;
    return true;
}

bool EncryptedDatabase::getUser(const std::string& username, UserRecord& record) {
    if (!is_loaded_) {
        std::cerr << "[X] Database not loaded" << std::endl;
        return false;
    }
    
    std::string user_key = "user_" + username;
    if (!database_json_.isMember(user_key)) {
        return false;
    }
    
    // Parse the JSON data
    std::string user_data_str = database_json_[user_key];
    
    SimpleJSON user_data;
    if (!user_data.parseFromString(user_data_str)) {
        return false;
    }
    
    // Parse user record from JSON
    record = UserRecord::fromJson(user_data);
    
    return true;
}

bool EncryptedDatabase::verifyCredentials(const std::string& username, const std::string& password) {
    UserRecord record;
    if (!getUser(username, record)) {
        return false;
    }
    
    // Hash the provided password with the stored salt
    std::string computed_hash;
    if (!hashPassword(password, record.salt, computed_hash)) {
        return false;
    }
    
    const bool matches = computed_hash.size() == record.encrypted_password.size() &&
        (computed_hash.empty() ||
         CRYPTO_memcmp(computed_hash.data(), record.encrypted_password.data(),
                       computed_hash.size()) == 0);
    SecureMemory::Cleanse(computed_hash);
    return matches;
}

bool EncryptedDatabase::loadDatabase() {
    std::error_code sizeError;
    const uintmax_t rawSize = std::filesystem::file_size(database_path_, sizeError);
    if (sizeError || rawSize == 0 || rawSize > MAX_DATABASE_FILE_SIZE ||
        rawSize > static_cast<uintmax_t>(std::numeric_limits<size_t>::max()) ||
        rawSize > static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        std::cerr << "[X] Database file size is invalid" << std::endl;
        return false;
    }

    std::ifstream file(database_path_, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::vector<uint8_t> fileContent(static_cast<size_t>(rawSize));
    file.read(reinterpret_cast<char*>(fileContent.data()),
              static_cast<std::streamsize>(fileContent.size()));
    if (!file || file.gcount() != static_cast<std::streamsize>(fileContent.size())) {
        return false;
    }
    file.close();

    std::string jsonData;
    if (fileContent.size() >= DATABASE_MAGIC.size() &&
        std::equal(DATABASE_MAGIC.begin(), DATABASE_MAGIC.end(), fileContent.begin())) {
        if (!DecryptDatabasePayload(master_password_.get(), fileContent, jsonData)) {
            std::cerr << "[X] Database authentication failed: wrong password or modified data"
                      << std::endl;
            return false;
        }
        is_modified_ = false;
    } else {
        const size_t legacyHeaderSize = sizeof(LEGACY_DATABASE_HEADER) - 1;
        if (fileContent.size() <= legacyHeaderSize ||
            !std::equal(std::begin(LEGACY_DATABASE_HEADER),
                        std::end(LEGACY_DATABASE_HEADER) - 1, fileContent.begin())) {
            std::cerr << "[X] Unknown database format" << std::endl;
            return false;
        }

        jsonData.assign(reinterpret_cast<const char*>(fileContent.data() + legacyHeaderSize),
                        fileContent.size() - legacyHeaderSize);
        is_modified_ = true;
        std::cout << "[MIGRATE] Loaded legacy plaintext database; converting to PQCDB002"
                  << std::endl;
    }

    if (!database_json_.parseFromString(jsonData)) {
        OPENSSL_cleanse(jsonData.data(), jsonData.size());
        std::cerr << "[X] Decrypted database payload is invalid" << std::endl;
        return false;
    }

    OPENSSL_cleanse(jsonData.data(), jsonData.size());
    database_json_.data["version"] = "2.0";
    database_json_.data["algorithm"] = "scrypt/AES-256-GCM";
    std::cout << "[OK] Database loaded successfully" << std::endl;
    is_loaded_ = true;
    return true;
}

bool EncryptedDatabase::saveDatabase() {
    if (!is_loaded_ || !is_modified_) {
        return true;
    }
    
    std::string jsonData = database_json_.toJsonString();
    std::vector<uint8_t> encryptedData;
    if (!EncryptDatabasePayload(master_password_.get(), jsonData, encryptedData)) {
        OPENSSL_cleanse(jsonData.data(), jsonData.size());
        std::cerr << "[X] Failed to encrypt database" << std::endl;
        return false;
    }
    OPENSSL_cleanse(jsonData.data(), jsonData.size());

    if (!AtomicFile::Write(database_path_, encryptedData)) {
        std::cerr << "[X] Failed to atomically write encrypted database" << std::endl;
        return false;
    }

    is_modified_ = false;
    std::cout << "[OK] Database saved as PQCDB002 (scrypt + AES-256-GCM)" << std::endl;
    return true;
}

bool EncryptedDatabase::hashPassword(const std::string& password, const std::string& salt, std::string& hash) {
    // Kept for compatibility with existing credential records.
    std::string input = password + salt;
    std::array<uint8_t, 32> hashBuffer{};
    unsigned int hashLength = 0;
    if (EVP_Digest(input.data(), input.size(), hashBuffer.data(), &hashLength,
                   EVP_sha256(), nullptr) != 1 || hashLength != hashBuffer.size()) {
        OPENSSL_cleanse(input.data(), input.size());
        OPENSSL_cleanse(hashBuffer.data(), hashBuffer.size());
        return false;
    }
    OPENSSL_cleanse(input.data(), input.size());
    
    std::stringstream ss;
    for (uint8_t byte : hashBuffer) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    hash = ss.str();
    OPENSSL_cleanse(hashBuffer.data(), hashBuffer.size());
    
    return true;
}

bool EncryptedDatabase::generateSalt(std::string& salt) {
    uint8_t salt_buffer[32];
    if (RAND_bytes(salt_buffer, sizeof(salt_buffer)) != 1) {
        OPENSSL_cleanse(salt_buffer, sizeof(salt_buffer));
        return false;
    }
    
    std::stringstream ss;
    for (int i = 0; i < 32; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)salt_buffer[i];
    }
    salt = ss.str();
    OPENSSL_cleanse(salt_buffer, sizeof(salt_buffer));
    
    return true;
}

bool EncryptedDatabase::generateRecoveryKey(std::string& recovery_key) {
    std::array<uint8_t, 32> randomBytes{};
    if (RAND_bytes(randomBytes.data(), static_cast<int>(randomBytes.size())) != 1) {
        SecureMemory::Cleanse(randomBytes.data(), randomBytes.size());
        return false;
    }

    static constexpr char HEX[] = "0123456789abcdef";
    recovery_key.assign("PQC-RK1-");
    recovery_key.reserve(72);
    for (const uint8_t value : randomBytes) {
        recovery_key.push_back(HEX[value >> 4U]);
        recovery_key.push_back(HEX[value & 0x0fU]);
    }
    SecureMemory::Cleanse(randomBytes.data(), randomBytes.size());
    return recovery_key.size() == 72;
}

std::vector<std::string> EncryptedDatabase::getAllUsernames() {
    std::vector<std::string> usernames;
    
    if (!is_loaded_) {
        return usernames;
    }
    
    // Get all user keys
    for (const auto& member : database_json_.getMemberNames()) {
        if (member.substr(0, 5) == "user_") {
            usernames.push_back(member.substr(5));
        }
    }
    
    return usernames;
}

std::map<std::string, std::string> EncryptedDatabase::getStatistics() {
    std::map<std::string, std::string> stats;
    
    stats["Total Users"] = std::to_string(getAllUsernames().size());
    stats["Database Path"] = database_path_;
    stats["Encryption Algorithm"] = "scrypt/AES-256-GCM";
    stats["Status"] = is_loaded_ ? "Loaded" : "Not Loaded";
    stats["Modified"] = is_modified_ ? "Yes" : "No";
    
    return stats;
}

bool EncryptedDatabase::updateUser(const std::string& username, const UserRecord& record) {
    if (!is_loaded_) {
        std::cerr << "[X] Database not loaded" << std::endl;
        return false;
    }
    
    std::string user_key = "user_" + username;
    if (!database_json_.isMember(user_key)) {
        std::cerr << "[X] User not found: " << username << std::endl;
        return false;
    }
    
    const std::string previousRecord = database_json_.data[user_key];
    const bool previousModifiedState = is_modified_;

    // Update user record
    SimpleJSON user_data = record.toJson();
    database_json_.data[user_key] = user_data.toJsonString();
    
    is_modified_ = true;
    
    // Save database
    if (!saveDatabase()) {
        database_json_.data[user_key] = previousRecord;
        is_modified_ = previousModifiedState;
        std::cerr << "[X] Failed to save database after updating user" << std::endl;
        return false;
    }
    
    std::cout << "[OK] User updated successfully: " << username << std::endl;
    return true;
}

bool EncryptedDatabase::deleteUser(const std::string& username) {
    if (!is_loaded_) {
        std::cerr << "[X] Database not loaded" << std::endl;
        return false;
    }
    
    std::string user_key = "user_" + username;
    if (!database_json_.isMember(user_key)) {
        std::cerr << "[X] User not found: " << username << std::endl;
        return false;
    }
    
    const bool previousModifiedState = is_modified_;
    auto removedRecord = database_json_.data.extract(user_key);
    
    is_modified_ = true;
    
    // Save database
    if (!saveDatabase()) {
        database_json_.data.insert(std::move(removedRecord));
        is_modified_ = previousModifiedState;
        std::cerr << "[X] Failed to save database after deleting user" << std::endl;
        return false;
    }
    
    std::cout << "[OK] User deleted successfully: " << username << std::endl;
    return true;
}

bool EncryptedDatabase::exportBackup(const std::string& backup_path, const std::string& backup_password) {
    if (!is_loaded_ || backup_path.empty() || backup_password.empty()) {
        return false;
    }

    std::error_code pathError;
    const auto databaseAbsolute =
        std::filesystem::absolute(database_path_, pathError).lexically_normal();
    if (pathError) {
        return false;
    }
    const auto backupAbsolute =
        std::filesystem::absolute(backup_path, pathError).lexically_normal();
    if (pathError || databaseAbsolute == backupAbsolute) {
        std::cerr << "[X] Backup path must differ from the live database path" << std::endl;
        return false;
    }

    SimpleJSON envelope;
    envelope["backup_version"] = "1";
    envelope["created_at"] = std::to_string(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    std::string databasePayload = database_json_.toJsonString();
    envelope["database_payload"] = databasePayload;

    std::string plaintext = envelope.toJsonString();
    SecureMemory::Cleanse(databasePayload);
    std::vector<uint8_t> encryptedBackup;
    std::string verifiedPlaintext;
    const bool prepared = EncryptBackupPayload(backup_password, plaintext, encryptedBackup) &&
                          DecryptBackupPayload(backup_password, encryptedBackup,
                                               verifiedPlaintext) &&
                          verifiedPlaintext == plaintext;
    SecureMemory::Cleanse(plaintext);
    SecureMemory::Cleanse(verifiedPlaintext);
    CleanseJson(envelope);
    if (!prepared || !AtomicFile::Write(backupAbsolute, encryptedBackup)) {
        std::cerr << "[X] Failed to create authenticated backup" << std::endl;
        return false;
    }

    std::cout << "[OK] Database backup exported as PQCBKP01: " << backupAbsolute
              << std::endl;
    return true;
}

bool EncryptedDatabase::importBackup(const std::string& backup_path, const std::string& backup_password) {
    if (!is_loaded_ || backup_path.empty() || backup_password.empty()) {
        return false;
    }

    std::error_code pathError;
    const auto databaseAbsolute =
        std::filesystem::absolute(database_path_, pathError).lexically_normal();
    if (pathError) {
        return false;
    }
    const auto backupAbsolute =
        std::filesystem::absolute(backup_path, pathError).lexically_normal();
    if (pathError || databaseAbsolute == backupAbsolute) {
        return false;
    }

    std::vector<uint8_t> encryptedBackup;
    std::string envelopeText;
    if (!ReadBackupFile(backupAbsolute, encryptedBackup) ||
        !DecryptBackupPayload(backup_password, encryptedBackup, envelopeText)) {
        SecureMemory::Cleanse(envelopeText);
        std::cerr << "[X] Backup authentication failed" << std::endl;
        return false;
    }

    SimpleJSON envelope;
    const bool envelopeParsed = envelope.parseFromString(envelopeText);
    std::string canonicalEnvelope = envelopeParsed ? envelope.toJsonString() : std::string();
    if (!envelopeParsed || envelope.data.size() != 3 || canonicalEnvelope != envelopeText ||
        !envelope.isMember("backup_version") || envelope["backup_version"] != "1" ||
        !envelope.isMember("created_at") || envelope["created_at"].empty() ||
        !envelope.isMember("database_payload") || envelope["database_payload"].empty()) {
        SecureMemory::Cleanse(envelopeText);
        SecureMemory::Cleanse(canonicalEnvelope);
        CleanseJson(envelope);
        return false;
    }
    SecureMemory::Cleanse(envelopeText);
    SecureMemory::Cleanse(canonicalEnvelope);

    std::string databasePayload = envelope["database_payload"];
    CleanseJson(envelope);
    SimpleJSON importedDatabase;
    const bool databaseParsed = importedDatabase.parseFromString(databasePayload);
    std::string normalizedPayload =
        databaseParsed ? importedDatabase.toJsonString() : std::string();
    if (!databaseParsed || normalizedPayload != databasePayload ||
        !ValidateDatabaseJson(importedDatabase)) {
        SecureMemory::Cleanse(databasePayload);
        SecureMemory::Cleanse(normalizedPayload);
        CleanseJson(importedDatabase);
        std::cerr << "[X] Authenticated backup contains an invalid database" << std::endl;
        return false;
    }

    // Re-encrypt under the current session master password and verify the exact
    // replacement before touching either disk or the current in-memory state.
    SecureMemory::Cleanse(databasePayload);
    databasePayload.swap(normalizedPayload);
    SecureMemory::Cleanse(normalizedPayload);
    std::vector<uint8_t> replacement;
    std::string verifiedPayload;
    const bool replacementValid =
        EncryptDatabasePayload(master_password_.get(), databasePayload, replacement) &&
        DecryptDatabasePayload(master_password_.get(), replacement, verifiedPayload) &&
        verifiedPayload == databasePayload;
    SecureMemory::Cleanse(databasePayload);
    SecureMemory::Cleanse(verifiedPayload);
    if (!replacementValid || !AtomicFile::Write(databaseAbsolute, replacement)) {
        CleanseJson(importedDatabase);
        std::cerr << "[X] Backup was valid, but database replacement failed" << std::endl;
        return false;
    }

    CleanseJson(database_json_);
    database_json_.data.swap(importedDatabase.data);
    CleanseJson(importedDatabase);
    is_modified_ = false;
    std::cout << "[OK] Database restored from authenticated PQCBKP01 backup" << std::endl;
    return true;
}

bool EncryptedDatabase::changePassword(const std::string& username, const std::string& old_password, const std::string& new_password) {
    // Verify old password first
    if (!verifyCredentials(username, old_password)) {
        return false;
    }
    
    // Get user record
    UserRecord record;
    if (!getUser(username, record)) {
        return false;
    }
    
    // Generate new salt and hash new password
    std::string new_salt;
    if (!generateSalt(new_salt)) {
        return false;
    }
    
    std::string new_hash;
    if (!hashPassword(new_password, new_salt, new_hash)) {
        return false;
    }
    
    // Update record
    record.salt = new_salt;
    record.encrypted_password = new_hash;
    
    // Save updated record
    return updateUser(username, record);
}

bool EncryptedDatabase::changeMasterPassword(const std::string& old_password,
                                             const std::string& new_password) {
    std::vector<uint8_t> replacement;
    if (!prepareMasterPasswordChange(old_password, new_password, replacement)) {
        return false;
    }

    if (!AtomicFile::Write(database_path_, replacement)) {
        return false;
    }
    return completeMasterPasswordChange(new_password);
}

bool EncryptedDatabase::prepareMasterPasswordChange(const std::string& old_password,
                                                    const std::string& new_password,
                                                    std::vector<uint8_t>& replacement) const {
    if (!is_loaded_ || new_password.empty() ||
        old_password.size() != master_password_.size() ||
        CRYPTO_memcmp(old_password.data(), master_password_.get().data(),
                      master_password_.size()) != 0) {
        return false;
    }

    std::string plaintext = database_json_.toJsonString();
    std::string verifiedPlaintext;
    if (!EncryptDatabasePayload(new_password, plaintext, replacement) ||
        !DecryptDatabasePayload(new_password, replacement, verifiedPlaintext) ||
        verifiedPlaintext != plaintext) {
        SecureMemory::Cleanse(plaintext);
        SecureMemory::Cleanse(verifiedPlaintext);
        return false;
    }
    SecureMemory::Cleanse(plaintext);
    SecureMemory::Cleanse(verifiedPlaintext);
    return true;
}

bool EncryptedDatabase::completeMasterPasswordChange(const std::string& new_password) {
    if (!is_loaded_ || new_password.empty() || !master_password_.assign(new_password)) {
        return false;
    }
    is_modified_ = false;
    return true;
}

// UserRecord methods
SimpleJSON EncryptedDatabase::UserRecord::toJson() const {
    SimpleJSON json;
    json["username"] = username;
    json["email"] = email;
    json["website"] = website;
    json["encrypted_password"] = encrypted_password;
    json["salt"] = salt;
    json["created_at"] = created_at;
    json["last_login"] = last_login;
    return json;
}

EncryptedDatabase::UserRecord EncryptedDatabase::UserRecord::fromJson(const SimpleJSON& json) {
    UserRecord record;
    record.username = json["username"];
    record.email = json["email"];
    record.website = json.isMember("website") ? json["website"] : "";
    record.encrypted_password = json["encrypted_password"];
    record.salt = json["salt"];
    record.created_at = json["created_at"];
    record.last_login = json["last_login"];
    return record;
}
