#include "PasswordManager.h"
#include <oqs/oqs.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstring>

PasswordManager::PasswordManager() {
    EnsureUsersDirectory();
}

PasswordManager::~PasswordManager() {
}

bool PasswordManager::UserExists(const std::string& username) const {
    return std::filesystem::exists(GetUserFilePath(username));
}

std::vector<uint8_t> PasswordManager::GenerateRandomBytes(size_t length) const {
    std::vector<uint8_t> bytes(length);
    if (RAND_bytes(bytes.data(), length) != 1) {
        std::cerr << "Failed to generate random bytes" << std::endl;
        return {};
    }
    return bytes;
}

std::vector<uint8_t> PasswordManager::DeriveKey(const std::string& password, const std::vector<uint8_t>& salt) const {
    std::vector<uint8_t> key(32); // 256-bit key
    
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_SCRYPT, nullptr);
    if (!ctx) return {};
    
    if (EVP_PKEY_derive_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return {};
    }
    
    if (EVP_PKEY_CTX_set1_pbe_pass(ctx, password.c_str(), password.length()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return {};
    }
    
    if (EVP_PKEY_CTX_set1_scrypt_salt(ctx, salt.data(), salt.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return {};
    }
    
    // Scrypt parameters: N=32768, r=8, p=1 (strong parameters)
    if (EVP_PKEY_CTX_set_scrypt_N(ctx, 32768) <= 0 ||
        EVP_PKEY_CTX_set_scrypt_r(ctx, 8) <= 0 ||
        EVP_PKEY_CTX_set_scrypt_p(ctx, 1) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return {};
    }
    
    size_t keylen = key.size();
    if (EVP_PKEY_derive(ctx, key.data(), &keylen) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return {};
    }
    
    EVP_PKEY_CTX_free(ctx);
    return key;
}

std::vector<uint8_t> PasswordManager::AESEncrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key, 
                                                 const std::vector<uint8_t>& iv, std::vector<uint8_t>& tag) const {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
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
    
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, data.data(), data.size()) != 1) {
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
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
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
    
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, data.data(), data.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    plaintext_len = len;
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(), const_cast<uint8_t*>(tag.data())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {}; // Authentication failed
    }
    plaintext_len += len;
    
    EVP_CIPHER_CTX_free(ctx);
    plaintext.resize(plaintext_len);
    return plaintext;
}

bool PasswordManager::CreateUser(const std::string& username, const std::string& password) {
    if (UserExists(username)) {
        std::cerr << "User already exists: " << username << std::endl;
        return false;
    }
    
    std::cout << "Creating user with enhanced security: " << username << std::endl;
    
    // Initialize Kyber KEM
    OQS_KEM* kem = OQS_KEM_new(OQS_KEM_alg_kyber_768);
    if (!kem) {
        std::cerr << "Failed to initialize Kyber KEM" << std::endl;
        return false;
    }
    
    EncryptedPassword encData;
    encData.version = CURRENT_VERSION;
    
    // Generate random salt and IV
    encData.salt = GenerateRandomBytes(SALT_SIZE);
    encData.iv = GenerateRandomBytes(IV_SIZE);
    
    if (encData.salt.empty() || encData.iv.empty()) {
        std::cerr << "Failed to generate random bytes" << std::endl;
        OQS_KEM_free(kem);
        return false;
    }
    
    // Derive key from password and salt
    std::vector<uint8_t> derived_key = DeriveKey(password, encData.salt);
    if (derived_key.empty()) {
        std::cerr << "Failed to derive key" << std::endl;
        OQS_KEM_free(kem);
        return false;
    }
    
    // Generate Kyber key pair
    encData.public_key.resize(kem->length_public_key);
    std::vector<uint8_t> secret_key(kem->length_secret_key);
    
    if (OQS_KEM_keypair(kem, encData.public_key.data(), secret_key.data()) != OQS_SUCCESS) {
        std::cerr << "Failed to generate key pair" << std::endl;
        OQS_KEM_free(kem);
        return false;
    }
    
    // Encrypt the secret key with AES-GCM
    std::vector<uint8_t> secret_key_tag;
    encData.encrypted_secret_key = AESEncrypt(secret_key, derived_key, encData.iv, secret_key_tag);
    if (encData.encrypted_secret_key.empty()) {
        std::cerr << "Failed to encrypt secret key" << std::endl;
        OQS_KEM_free(kem);
        return false;
    }
    
    // Encapsulate to generate shared secret
    encData.ciphertext.resize(kem->length_ciphertext);
    std::vector<uint8_t> shared_secret(kem->length_shared_secret);
    
    if (OQS_KEM_encaps(kem, encData.ciphertext.data(), shared_secret.data(), encData.public_key.data()) != OQS_SUCCESS) {
        std::cerr << "Failed to encapsulate" << std::endl;
        OQS_KEM_free(kem);
        return false;
    }
    
    // Double encrypt the password: first with Kyber shared secret, then with AES
    std::vector<uint8_t> xor_encrypted = XOREncrypt(password, shared_secret);
    encData.encrypted_password = AESEncrypt(xor_encrypted, derived_key, encData.iv, encData.auth_tag);
    
    if (encData.encrypted_password.empty()) {
        std::cerr << "Failed to encrypt password" << std::endl;
        OQS_KEM_free(kem);
        return false;
    }
    
    // Combine all authentication tags
    encData.auth_tag.insert(encData.auth_tag.end(), secret_key_tag.begin(), secret_key_tag.end());
    
    // Save to file with restrictive permissions
    bool success = SaveEncryptedData(username, encData);
    
    // Set restrictive file permissions
    if (success) {
        std::string filepath = GetUserFilePath(username);
        std::filesystem::permissions(filepath, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    }
    
    OQS_KEM_free(kem);
    
    if (success) {
        std::cout << "User created successfully with enhanced security: " << username << std::endl;
    } else {
        std::cerr << "Failed to save user data: " << username << std::endl;
    }
    
    return success;
}

bool PasswordManager::VerifyPassword(const std::string& username, const std::string& password) const {
    if (!UserExists(username)) {
        std::cerr << "User does not exist: " << username << std::endl;
        return false;
    }
    
    auto encData = LoadEncryptedData(username);
    if (encData.ciphertext.empty()) {
        std::cerr << "Failed to load user data: " << username << std::endl;
        return false;
    }
    
    // Check if it's old format and needs migration
    if (encData.version != CURRENT_VERSION) {
        std::cout << "Attempting to verify password with legacy format..." << std::endl;
        return VerifyPasswordLegacy(username, password);
    }
    
    std::cout << "Verifying password with enhanced security..." << std::endl;
    
    // Derive key from password and salt
    std::vector<uint8_t> derived_key = DeriveKey(password, encData.salt);
    if (derived_key.empty()) {
        std::cerr << "Failed to derive key" << std::endl;
        return false;
    }
    
    // Split authentication tags
    if (encData.auth_tag.size() != TAG_SIZE * 2) {
        std::cerr << "Invalid authentication tag size" << std::endl;
        return false;
    }
    
    std::vector<uint8_t> password_tag(encData.auth_tag.begin(), encData.auth_tag.begin() + TAG_SIZE);
    std::vector<uint8_t> secret_key_tag(encData.auth_tag.begin() + TAG_SIZE, encData.auth_tag.end());
    
    // Decrypt the secret key
    std::vector<uint8_t> secret_key = AESDecrypt(encData.encrypted_secret_key, derived_key, encData.iv, secret_key_tag);
    if (secret_key.empty()) {
        std::cerr << "Failed to decrypt secret key - wrong password" << std::endl;
        return false;
    }
    
    // Initialize Kyber KEM
    OQS_KEM* kem = OQS_KEM_new(OQS_KEM_alg_kyber_768);
    if (!kem) {
        std::cerr << "Failed to initialize Kyber KEM" << std::endl;
        return false;
    }
    
    // Decapsulate to get shared secret
    std::vector<uint8_t> shared_secret(kem->length_shared_secret);
    if (OQS_KEM_decaps(kem, shared_secret.data(), encData.ciphertext.data(), secret_key.data()) != OQS_SUCCESS) {
        std::cerr << "Failed to decapsulate" << std::endl;
        OQS_KEM_free(kem);
        return false;
    }
    
    // Decrypt the password
    std::vector<uint8_t> aes_decrypted = AESDecrypt(encData.encrypted_password, derived_key, encData.iv, password_tag);
    if (aes_decrypted.empty()) {
        std::cerr << "Failed to decrypt password with AES - authentication failed" << std::endl;
        OQS_KEM_free(kem);
        return false;
    }
    
    std::string decrypted_password = XORDecrypt(aes_decrypted, shared_secret);
    
    OQS_KEM_free(kem);
    
    bool match = (decrypted_password == password);
    if (match) {
        std::cout << "Password verified successfully with enhanced security for user: " << username << std::endl;
    } else {
        std::cerr << "Password verification failed for user: " << username << std::endl;
    }
    
    return match;
}

// Legacy support for old format
bool PasswordManager::VerifyPasswordLegacy(const std::string& username, const std::string& password) const {
    std::cout << "Using legacy verification for old format file..." << std::endl;
    
    // Load old format data
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
    
    // Load old format data
    size_t size;
    
    // Load ciphertext
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (file.gcount() != sizeof(size)) return false;
    oldData.ciphertext.resize(size);
    file.read(reinterpret_cast<char*>(oldData.ciphertext.data()), size);
    if (file.gcount() != static_cast<std::streamsize>(size)) return false;
    
    // Load public key
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (file.gcount() != sizeof(size)) return false;
    oldData.public_key.resize(size);
    file.read(reinterpret_cast<char*>(oldData.public_key.data()), size);
    if (file.gcount() != static_cast<std::streamsize>(size)) return false;
    
    // Load secret key
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (file.gcount() != sizeof(size)) return false;
    oldData.secret_key.resize(size);
    file.read(reinterpret_cast<char*>(oldData.secret_key.data()), size);
    if (file.gcount() != static_cast<std::streamsize>(size)) return false;
    
    // Load encrypted password
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (file.gcount() != sizeof(size)) return false;
    oldData.encrypted_password.resize(size);
    file.read(reinterpret_cast<char*>(oldData.encrypted_password.data()), size);
    if (file.gcount() != static_cast<std::streamsize>(size)) return false;
    
    // Initialize Kyber KEM
    OQS_KEM* kem = OQS_KEM_new(OQS_KEM_alg_kyber_768);
    if (!kem) return false;
    
    // Decapsulate to get shared secret
    std::vector<uint8_t> shared_secret(kem->length_shared_secret);
    if (OQS_KEM_decaps(kem, shared_secret.data(), oldData.ciphertext.data(), oldData.secret_key.data()) != OQS_SUCCESS) {
        OQS_KEM_free(kem);
        return false;
    }
    
    // Decrypt password
    std::string decrypted_password = XORDecrypt(oldData.encrypted_password, shared_secret);
    
    OQS_KEM_free(kem);
    
    bool match = (decrypted_password == password);
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
        if (entry.is_regular_file() && entry.path().extension() == ".enc") {
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
        if (entry.is_regular_file() && entry.path().extension() == ".enc") {
            std::string filename = entry.path().stem().string();
            usernames.push_back(filename);
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

std::string PasswordManager::XORDecrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key) const {
    std::string result(data.size(), 0);
    for (size_t i = 0; i < data.size(); ++i) {
        result[i] = static_cast<char>(data[i] ^ key[i % key.size()]);
    }
    return result;
}

bool PasswordManager::SaveEncryptedData(const std::string& username, const EncryptedPassword& data) const {
    std::string filepath = GetUserFilePath(username);
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file for writing: " << filepath << std::endl;
        return false;
    }
    
    // Save version first
    file.write(reinterpret_cast<const char*>(&data.version), sizeof(data.version));
    
    // Save sizes and data for new format
    size_t size = data.salt.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(data.salt.data()), size);
    
    size = data.iv.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(data.iv.data()), size);
    
    size = data.ciphertext.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(data.ciphertext.data()), size);
    
    size = data.public_key.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(data.public_key.data()), size);
    
    size = data.encrypted_secret_key.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(data.encrypted_secret_key.data()), size);
    
    size = data.encrypted_password.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(data.encrypted_password.data()), size);
    
    size = data.auth_tag.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(data.auth_tag.data()), size);
    
    file.close();
    return file.good();
}

PasswordManager::EncryptedPassword PasswordManager::LoadEncryptedData(const std::string& username) const {
    EncryptedPassword data;
    std::string filepath = GetUserFilePath(username);
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file for reading: " << filepath << std::endl;
        return data;
    }
    
    // Check if it's new format by trying to read version
    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    
    if (file.gcount() == sizeof(version) && version == CURRENT_VERSION) {
        // New format
        data.version = version;
        
        size_t size;
        
        // Load salt
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (file.gcount() != sizeof(size)) return {};
        data.salt.resize(size);
        file.read(reinterpret_cast<char*>(data.salt.data()), size);
        if (file.gcount() != static_cast<std::streamsize>(size)) return {};
        
        // Load IV
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (file.gcount() != sizeof(size)) return {};
        data.iv.resize(size);
        file.read(reinterpret_cast<char*>(data.iv.data()), size);
        if (file.gcount() != static_cast<std::streamsize>(size)) return {};
        
        // Load ciphertext
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (file.gcount() != sizeof(size)) return {};
        data.ciphertext.resize(size);
        file.read(reinterpret_cast<char*>(data.ciphertext.data()), size);
        if (file.gcount() != static_cast<std::streamsize>(size)) return {};
        
        // Load public key
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (file.gcount() != sizeof(size)) return {};
        data.public_key.resize(size);
        file.read(reinterpret_cast<char*>(data.public_key.data()), size);
        if (file.gcount() != static_cast<std::streamsize>(size)) return {};
        
        // Load encrypted secret key
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (file.gcount() != sizeof(size)) return {};
        data.encrypted_secret_key.resize(size);
        file.read(reinterpret_cast<char*>(data.encrypted_secret_key.data()), size);
        if (file.gcount() != static_cast<std::streamsize>(size)) return {};
        
        // Load encrypted password
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (file.gcount() != sizeof(size)) return {};
        data.encrypted_password.resize(size);
        file.read(reinterpret_cast<char*>(data.encrypted_password.data()), size);
        if (file.gcount() != static_cast<std::streamsize>(size)) return {};
        
        // Load auth tag
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (file.gcount() != sizeof(size)) return {};
        data.auth_tag.resize(size);
        file.read(reinterpret_cast<char*>(data.auth_tag.data()), size);
        if (file.gcount() != static_cast<std::streamsize>(size)) return {};
        
    } else {
        // Old format - set version to 1 to indicate legacy format
        data.version = 1;
        
        // Reset file position and treat the first 4 bytes as size
        file.seekg(0, std::ios::beg);
        
        size_t size;
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (file.gcount() != sizeof(size)) return {};
        data.ciphertext.resize(size);
        file.read(reinterpret_cast<char*>(data.ciphertext.data()), size);
        if (file.gcount() != static_cast<std::streamsize>(size)) return {};
    }
    
    return data;
}

std::string PasswordManager::GetUserFilePath(const std::string& username) const {
    return "users/" + username + ".enc";
}

void PasswordManager::EnsureUsersDirectory() const {
    if (!std::filesystem::exists("users")) {
        std::filesystem::create_directories("users");
        // Set restrictive permissions on directory
        std::filesystem::permissions("users", std::filesystem::perms::owner_all);
    }
}
