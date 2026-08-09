#ifndef ENCRYPTED_DATABASE_H
#define ENCRYPTED_DATABASE_H

#include <string>
#include <vector>
#include "SecureMemory.h"
#include <map>
#include <sstream>

// Simple JSON-like structure for our database
struct SimpleJSON {
    std::map<std::string, std::string> data;
    
    std::string& operator[](const std::string& key) {
        return data[key];
    }
    
    const std::string& operator[](const std::string& key) const {
        static std::string empty;
        auto it = data.find(key);
        return it != data.end() ? it->second : empty;
    }
    
    bool isMember(const std::string& key) const {
        return data.find(key) != data.end();
    }
    
    std::vector<std::string> getMemberNames() const {
        std::vector<std::string> names;
        for (const auto& pair : data) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    std::string toJsonString() const {
        std::stringstream ss;
        ss << "{";
        bool first = true;
        for (const auto& pair : data) {
            if (!first) ss << ",";
            ss << "\"" << pair.first << "\":\"" << pair.second << "\"";
            first = false;
        }
        ss << "}";
        return ss.str();
    }
    
    // Simple JSON parser for basic key-value pairs
    bool parseFromString(const std::string& json_str) {
        data.clear();
        
        // Simple but effective JSON parsing for our specific format
        size_t pos = 0;
        while (pos < json_str.length()) {
            // Find opening quote for key
            size_t key_start = json_str.find("\"", pos);
            if (key_start == std::string::npos) break;
            key_start++;
            
            // Find closing quote for key
            size_t key_end = json_str.find("\"", key_start);
            if (key_end == std::string::npos) break;
            
            std::string key = json_str.substr(key_start, key_end - key_start);
            
            // Find colon
            size_t colon_pos = json_str.find(":", key_end);
            if (colon_pos == std::string::npos) break;
            
            // Skip whitespace after colon
            size_t value_start = colon_pos + 1;
            while (value_start < json_str.length() && isspace(json_str[value_start])) {
                value_start++;
            }
            
            if (value_start >= json_str.length()) break;
            
            std::string value;
            if (json_str[value_start] == '"') {
                // String value
                value_start++; // Skip opening quote
                size_t value_end = value_start;
                
                // Find closing quote, handling nested JSON
                int brace_depth = 0;
                bool escaped = false;
                
                while (value_end < json_str.length()) {
                    char c = json_str[value_end];
                    
                    if (escaped) {
                        escaped = false;
                        value_end++;
                        continue;
                    }
                    
                    if (c == '\\') {
                        escaped = true;
                        value_end++;
                        continue;
                    }
                    
                    if (c == '{') brace_depth++;
                    else if (c == '}') brace_depth--;
                    else if (c == '"' && brace_depth == 0) {
                        // Found the closing quote
                        break;
                    }
                    
                    value_end++;
                }
                
                if (value_end < json_str.length()) {
                    value = json_str.substr(value_start, value_end - value_start);
                    pos = value_end + 1;
                } else {
                    break;
                }
            } else {
                // Non-string value
                size_t value_end = json_str.find_first_of(",}", value_start);
                if (value_end == std::string::npos) {
                    value = json_str.substr(value_start);
                    pos = json_str.length();
                } else {
                    value = json_str.substr(value_start, value_end - value_start);
                    pos = value_end;
                }
                
                // Trim whitespace
                while (!value.empty() && isspace(value.back())) {
                    value.pop_back();
                }
            }
            
            data[key] = value;
            
            // Move to next key-value pair
            pos = json_str.find(",", pos);
            if (pos == std::string::npos) break;
            pos++;
        }
        
        return !data.empty();
    }
};

/**
 * @brief Encrypted Database for storing user credentials using Post-Quantum Cryptography
 * 
 * This class provides a secure encrypted database for storing user information including:
 * - Usernames
 * - Email addresses  
 * - Encrypted passwords
 * - User metadata
 * 
 * Uses scrypt for key derivation and AES-256-GCM for authenticated encryption.
 */
class EncryptedDatabase {
public:
    /**
     * @brief Structure representing a user record
     */
    struct UserRecord {
        std::string username;
        std::string email;
        std::string website;
        std::string encrypted_password;
        std::string salt;
        std::string created_at;
        std::string last_login;
        std::map<std::string, std::string> metadata;
        
        // Convert to/from JSON
        SimpleJSON toJson() const;
        static UserRecord fromJson(const SimpleJSON& json);
    };

    /**
     * @brief Constructor
     * @param database_path Path to the encrypted database file
     * @param master_password Master password for database encryption
     */
    EncryptedDatabase(const std::string& database_path, const std::string& master_password);
    
    /**
     * @brief Destructor - ensures secure cleanup
     */
    ~EncryptedDatabase();

    /**
     * @brief Initialize the database (create if doesn't exist)
     * @return true if successful, false otherwise
     */
    bool initialize();

    /**
     * @brief Add a new user record
     * @param record User record to add
     * @return true if successful, false otherwise
     */
    bool addUser(const UserRecord& record);

    /**
     * @brief Get a user record by username
     * @param username Username to search for
     * @param record Output parameter for the found record
     * @return true if found, false otherwise
     */
    bool getUser(const std::string& username, UserRecord& record);

    /**
     * @brief Update an existing user record
     * @param username Username to update
     * @param record New record data
     * @return true if successful, false otherwise
     */
    bool updateUser(const std::string& username, const UserRecord& record);

    /**
     * @brief Delete a user record
     * @param username Username to delete
     * @return true if successful, false otherwise
     */
    bool deleteUser(const std::string& username);

    /**
     * @brief Get all usernames
     * @return Vector of all usernames
     */
    std::vector<std::string> getAllUsernames();

    /**
     * @brief Verify user credentials
     * @param username Username to verify
     * @param password Password to verify
     * @return true if credentials are valid, false otherwise
     */
    bool verifyCredentials(const std::string& username, const std::string& password);

    /**
     * @brief Change user password
     * @param username Username
     * @param old_password Current password
     * @param new_password New password
     * @return true if successful, false otherwise
     */
    bool changePassword(const std::string& username, const std::string& old_password, const std::string& new_password);

    // Re-encrypt the complete database container under a new master password.
    bool changeMasterPassword(const std::string& old_password, const std::string& new_password);

    // Transaction support: prepare and authenticate the replacement without
    // changing either the current file or the retained in-memory password.
    bool prepareMasterPasswordChange(const std::string& old_password,
                                     const std::string& new_password,
                                     std::vector<uint8_t>& replacement) const;
    bool completeMasterPasswordChange(const std::string& new_password);
    const std::string& getDatabasePath() const noexcept { return database_path_; }

    /**
     * @brief Get database statistics
     * @return Map with database statistics
     */
    std::map<std::string, std::string> getStatistics();

    /**
     * @brief Export database to encrypted backup
     * @param backup_path Path for backup file
     * @param backup_password Password for backup encryption
     * @return true if successful, false otherwise
     */
    bool exportBackup(const std::string& backup_path, const std::string& backup_password);

    /**
     * @brief Import database from encrypted backup
     * @param backup_path Path to backup file
     * @param backup_password Password for backup decryption
     * @return true if successful, false otherwise
     */
    bool importBackup(const std::string& backup_path, const std::string& backup_password);

    // Generate a high-entropy printable key suitable as an offline backup key.
    bool generateRecoveryKey(std::string& recovery_key);

    /**
     * @brief Hash password using SHA-256 (public for UI usage)
     * @param password Password to hash
     * @param salt Salt for hashing
     * @param hash Output hash
     * @return true if successful, false otherwise
     */
    bool hashPassword(const std::string& password, const std::string& salt, std::string& hash);

    /**
     * @brief Generate random salt (public for UI usage)
     * @param salt Output salt
     * @return true if successful, false otherwise
     */
    bool generateSalt(std::string& salt);

private:
    std::string database_path_;
    SecureMemory::SecureString master_password_;

    // In-memory database
    SimpleJSON database_json_;
    bool is_loaded_;
    bool is_modified_;

    /**
     * @brief Load database from encrypted file
     * @return true if successful, false otherwise
     */
    bool loadDatabase();

    /**
     * @brief Save database to encrypted file
     * @return true if successful, false otherwise
     */
    bool saveDatabase();

};

#endif // ENCRYPTED_DATABASE_H
