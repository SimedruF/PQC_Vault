#pragma once
#include <string>
#include <vector>
#include <memory>

class PasswordManager {
public:
    struct EncryptedPassword {
        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> public_key;
        std::vector<uint8_t> secret_key;
        std::vector<uint8_t> encrypted_password;
    };
    
    PasswordManager();
    ~PasswordManager();
    
    // Check if user exists
    bool UserExists(const std::string& username) const;
    
    // Encrypt and save password for new user
    bool CreateUser(const std::string& username, const std::string& password);
    
    // Verify password for existing user
    bool VerifyPassword(const std::string& username, const std::string& password) const;
    
    // Check if any users exist (for first-time setup)
    bool HasAnyUsers() const;
    
    // Get list of usernames
    std::vector<std::string> GetUsernames() const;
    
private:
    // Encrypt data using XOR with key
    std::vector<uint8_t> XOREncrypt(const std::string& data, const std::vector<uint8_t>& key) const;
    
    // Decrypt data using XOR with key
    std::string XORDecrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key) const;
    
    // Save encrypted data to file
    bool SaveEncryptedData(const std::string& username, const EncryptedPassword& data) const;
    
    // Load encrypted data from file
    EncryptedPassword LoadEncryptedData(const std::string& username) const;
    
    // Get file path for user
    std::string GetUserFilePath(const std::string& username) const;
    
    // Ensure users directory exists
    void EnsureUsersDirectory() const;
};
