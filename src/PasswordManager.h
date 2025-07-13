#pragma once
#include <string>
#include <vector>
#include <memory>

class PasswordManager {
public:
    struct EncryptedPassword {
        std::vector<uint8_t> salt;                    // Random salt for key derivation
        std::vector<uint8_t> iv;                      // Initialization vector for AES
        std::vector<uint8_t> ciphertext;              // Kyber KEM ciphertext
        std::vector<uint8_t> public_key;              // Kyber public key
        std::vector<uint8_t> encrypted_secret_key;    // AES encrypted secret key
        std::vector<uint8_t> encrypted_password;      // Double encrypted password
        std::vector<uint8_t> auth_tag;                // Authentication tag for integrity
        uint32_t version;                             // File format version
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
    
private:
    static const uint32_t CURRENT_VERSION = 2;  // Updated file format version
    static const size_t SALT_SIZE = 32;          // 256-bit salt
    static const size_t IV_SIZE = 16;            // 128-bit IV for AES
    static const size_t TAG_SIZE = 16;           // 128-bit authentication tag
    
    // Enhanced encryption using AES-GCM + Kyber
    std::vector<uint8_t> AESEncrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv, std::vector<uint8_t>& tag) const;
    std::vector<uint8_t> AESDecrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv, const std::vector<uint8_t>& tag) const;
    
    // Key derivation from password + salt
    std::vector<uint8_t> DeriveKey(const std::string& password, const std::vector<uint8_t>& salt) const;
    
    // Generate random bytes
    std::vector<uint8_t> GenerateRandomBytes(size_t length) const;
    
    // Legacy support
    std::vector<uint8_t> XOREncrypt(const std::string& data, const std::vector<uint8_t>& key) const;
    std::string XORDecrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key) const;
    
    // File operations with enhanced security
    bool SaveEncryptedData(const std::string& username, const EncryptedPassword& data) const;
    EncryptedPassword LoadEncryptedData(const std::string& username) const;
    
    // Migration from old format
    bool MigrateOldFormat(const std::string& username, const std::string& password);
    
    // Get file path for user
    std::string GetUserFilePath(const std::string& username) const;
    
    // Ensure users directory exists
    void EnsureUsersDirectory() const;
};
