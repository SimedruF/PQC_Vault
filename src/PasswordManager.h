#pragma once
#include <string>
#include <vector>
#include <memory>

class EncryptedDatabase;

class PasswordManager {
public:
    struct EncryptedPassword {
        std::vector<uint8_t> salt;                    // Random salt for key derivation
        std::vector<uint8_t> secret_key_nonce;        // GCM nonce for the secret key
        std::vector<uint8_t> password_nonce;          // Independent GCM nonce for the password
        std::vector<uint8_t> ciphertext;              // KEM ciphertext (ML-KEM or legacy Kyber)
        std::vector<uint8_t> public_key;              // KEM public key
        std::vector<uint8_t> encrypted_secret_key;    // AES encrypted secret key
        std::vector<uint8_t> encrypted_password;      // Double encrypted password
        std::vector<uint8_t> secret_key_auth_tag;     // GCM tag for the secret key
        std::vector<uint8_t> password_auth_tag;       // GCM tag for the password
        uint32_t version = 0;                         // File format version
    };
    
    PasswordManager();
    ~PasswordManager();
    
    // Check if user exists
    bool UserExists(const std::string& username) const;
    
    // Encrypt and save password for new user
    bool CreateUser(const std::string& username, const std::string& password);
    
    // Verify password for existing user
    bool VerifyPassword(const std::string& username, const std::string& password) const;
    
    // Legacy password verification for old format
    bool VerifyPasswordLegacy(const std::string& username, const std::string& password) const;
    
    // Check if any users exist (for first-time setup)
    bool HasAnyUsers() const;
    
    // Get list of usernames
    std::vector<std::string> GetUsernames() const;
    
    // Security enhancement: Change master password
    bool ChangeMasterPassword(const std::string& username, const std::string& oldPassword, const std::string& newPassword);
    bool ChangeMasterPassword(const std::string& username,
                              const std::string& oldPassword,
                              const std::string& newPassword,
                              EncryptedDatabase* database);
    
private:
    bool transaction_recovery_ready_ = false;

    static const uint32_t CURRENT_VERSION = 5;         // Portable ML-KEM-768 format
    static const uint32_t ML_KEM_VERSION = 4;          // Native-endian ML-KEM-768 format
    static const uint32_t AES_GCM_VERSION = 3;         // Legacy Kyber-768 + independent nonces
    static const uint32_t PREVIOUS_VERSION = 2;        // Legacy Kyber-768 + reused IV
    static const size_t SALT_SIZE = 32;          // 256-bit salt
    static const size_t NONCE_SIZE = 12;         // Recommended GCM nonce size
    static const size_t TAG_SIZE = 16;           // 128-bit authentication tag
    
    // Authenticated encryption used together with ML-KEM-768
    std::vector<uint8_t> AESEncrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv, std::vector<uint8_t>& tag) const;
    std::vector<uint8_t> AESDecrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv, const std::vector<uint8_t>& tag) const;
    
    // Key derivation from password + salt
    std::vector<uint8_t> DeriveKey(const std::string& password, const std::vector<uint8_t>& salt) const;
    
    // Generate random bytes
    std::vector<uint8_t> GenerateRandomBytes(size_t length) const;
    
    // Legacy support
    std::vector<uint8_t> XOREncrypt(const std::string& data, const std::vector<uint8_t>& key) const;
    // File operations with enhanced security
    bool BuildEncryptedPassword(const std::string& password, EncryptedPassword& data) const;
    bool ValidateEncryptedPassword(const EncryptedPassword& data,
                                   const std::string& password) const;
    bool EncodeEncryptedData(const EncryptedPassword& data,
                             std::vector<uint8_t>& encoded) const;
    bool SaveEncryptedData(const std::string& username, const EncryptedPassword& data) const;
    EncryptedPassword LoadEncryptedData(const std::string& username) const;
    
    // Get file path for user
    std::string GetUserFilePath(const std::string& username) const;
    
    // Ensure users directory exists
    void EnsureUsersDirectory() const;
};
