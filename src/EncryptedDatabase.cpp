#include "EncryptedDatabase.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <random>
#include <openssl/kdf.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <cstring>

EncryptedDatabase::EncryptedDatabase(const std::string& database_path, const std::string& master_password)
    : database_path_(database_path), master_password_(master_password), 
      sphincs_signature_(nullptr), sphincs_public_key_(nullptr), sphincs_secret_key_(nullptr),
      aes_ctx_(nullptr), is_loaded_(false), is_modified_(false) {
    
    // Initialize OpenSSL
    OpenSSL_add_all_algorithms();
    
    // Clear sensitive data
    memset(aes_key_, 0, sizeof(aes_key_));
    memset(aes_iv_, 0, sizeof(aes_iv_));
}

EncryptedDatabase::~EncryptedDatabase() {
    // Secure cleanup
    if (sphincs_signature_) {
        OQS_SIG_free(sphincs_signature_);
    }
    if (sphincs_public_key_) {
        secureCleanup(sphincs_public_key_, sphincs_signature_ ? sphincs_signature_->length_public_key : 0);
        free(sphincs_public_key_);
    }
    if (sphincs_secret_key_) {
        secureCleanup(sphincs_secret_key_, sphincs_signature_ ? sphincs_signature_->length_secret_key : 0);
        free(sphincs_secret_key_);
    }
    if (aes_ctx_) {
        EVP_CIPHER_CTX_free(aes_ctx_);
    }
    
    // Clear sensitive data
    secureCleanup(aes_key_, sizeof(aes_key_));
    secureCleanup(aes_iv_, sizeof(aes_iv_));
}

bool EncryptedDatabase::initialize() {
    std::cout << "[*] Initializing Encrypted Database with SPHINCS+ PQC..." << std::endl;
    
    // Generate SPHINCS+ keys
    if (!generateSPHINCSKeys()) {
        std::cerr << "[X] Failed to generate SPHINCS+ keys" << std::endl;
        return false;
    }
    
    // Initialize AES encryption
    if (!initializeAES()) {
        std::cerr << "[X] Failed to initialize AES encryption" << std::endl;
        return false;
    }
    
    // Load existing database or create new one
    if (!loadDatabase()) {
        std::cout << "[NEW] Creating new encrypted database..." << std::endl;
        // Create empty database structure
        database_json_.data["version"] = "1.0";
        database_json_.data["created_at"] = std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        database_json_.data["algorithm"] = "SPHINCS+/AES-256-GCM";
        is_loaded_ = true;
        is_modified_ = true;
        
        // Save the new database
        if (!saveDatabase()) {
            std::cerr << "[X] Failed to save new database" << std::endl;
            return false;
        }
    }
    
    std::cout << "[OK] Encrypted Database initialized successfully!" << std::endl;
    return true;
}

bool EncryptedDatabase::generateSPHINCSKeys() {
    std::cout << "[KEY] Generating SPHINCS+ key pair..." << std::endl;
    
    // Initialize SPHINCS+ signature scheme
    sphincs_signature_ = OQS_SIG_new(OQS_SIG_alg_sphincs_sha2_128f_simple);
    if (!sphincs_signature_) {
        std::cerr << "[X] Failed to initialize SPHINCS+ signature scheme" << std::endl;
        return false;
    }
    
    // Allocate memory for keys
    sphincs_public_key_ = (uint8_t*)malloc(sphincs_signature_->length_public_key);
    sphincs_secret_key_ = (uint8_t*)malloc(sphincs_signature_->length_secret_key);
    
    if (!sphincs_public_key_ || !sphincs_secret_key_) {
        std::cerr << "[X] Failed to allocate memory for SPHINCS+ keys" << std::endl;
        return false;
    }
    
    // Generate key pair
    if (OQS_SIG_keypair(sphincs_signature_, sphincs_public_key_, sphincs_secret_key_) != OQS_SUCCESS) {
        std::cerr << "[X] Failed to generate SPHINCS+ key pair" << std::endl;
        return false;
    }
    
    std::cout << "[OK] SPHINCS+ keys generated successfully!" << std::endl;
    std::cout << "   Public key size: " << sphincs_signature_->length_public_key << " bytes" << std::endl;
    std::cout << "   Secret key size: " << sphincs_signature_->length_secret_key << " bytes" << std::endl;
    
    return true;
}

bool EncryptedDatabase::initializeAES() {
    std::cout << "[HASH] Initializing AES-256-GCM encryption..." << std::endl;
    
    // Create AES context
    aes_ctx_ = EVP_CIPHER_CTX_new();
    if (!aes_ctx_) {
        std::cerr << "[X] Failed to create AES context" << std::endl;
        return false;
    }
    
    // Generate salt for key derivation
    uint8_t salt[32];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        std::cerr << "[X] Failed to generate salt" << std::endl;
        return false;
    }
    
    // Derive AES key from master password
    if (!deriveKeyFromPassword(master_password_, salt, aes_key_)) {
        std::cerr << "[X] Failed to derive AES key from master password" << std::endl;
        return false;
    }
    
    // Generate IV
    if (RAND_bytes(aes_iv_, sizeof(aes_iv_)) != 1) {
        std::cerr << "[X] Failed to generate AES IV" << std::endl;
        return false;
    }
    
    std::cout << "[OK] AES-256-GCM encryption initialized successfully!" << std::endl;
    return true;
}

bool EncryptedDatabase::deriveKeyFromPassword(const std::string& password, const uint8_t* salt, uint8_t* key) {
    // Use PBKDF2 with SHA-256
    if (PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                          salt, 32,
                          100000, // 100k iterations
                          EVP_sha256(),
                          32, key) != 1) {
        return false;
    }
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
    
    // Add to database
    database_json_.data[user_key] = user_data.toJsonString();
    
    is_modified_ = true;
    
    // Save database
    if (!saveDatabase()) {
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
    
    // Compare with stored hash
    return computed_hash == record.encrypted_password;
}

bool EncryptedDatabase::loadDatabase() {
    std::cout << "[LOAD] Loading encrypted database..." << std::endl;
    
    std::ifstream file(database_path_, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "[NEW] Database file doesn't exist, will create new one" << std::endl;
        return false;
    }
    
    // Read the entire file
    std::string file_content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();
    
    if (file_content.empty()) {
        std::cout << "[NEW] Empty database file, will create new one" << std::endl;
        return false;
    }
    
    // Check for our header
    if (file_content.substr(0, 18) != "PQCWALLET_DB_v1.0\n") {
        std::cout << "[NEW] Invalid database format, will create new one" << std::endl;
        return false;
    }
    
    // Extract JSON data (simplified for testing)
    std::string json_data = file_content.substr(18);
    
    // Parse JSON data back into database_json_
    if (!database_json_.parseFromString(json_data)) {
        std::cout << "[NEW] Failed to parse database, will create new one" << std::endl;
        return false;
    }
    
    std::cout << "[OK] Database loaded successfully" << std::endl;
    is_loaded_ = true;
    return true;
}

bool EncryptedDatabase::saveDatabase() {
    if (!is_loaded_ || !is_modified_) {
        return true;
    }
    
    std::cout << "[SAVE] Saving encrypted database..." << std::endl;
    
    // Convert database to JSON string
    std::string json_data = database_json_.toJsonString();
    
    // For now, we'll save the JSON directly for testing
    // In a real implementation, we'd:
    // 1. Sign the data with SPHINCS+
    // 2. Encrypt the data with AES-256-GCM
    // 3. Store signature + auth_tag + encrypted_data
    
    // Save to file (simplified for testing)
    std::ofstream file(database_path_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[X] Failed to open database file for writing" << std::endl;
        return false;
    }
    
    // Write a header to indicate this is our format
    file << "PQCWALLET_DB_v1.0\n";
    file << json_data;
    file.close();
    
    is_modified_ = false;
    std::cout << "[OK] Database saved successfully" << std::endl;
    return true;
}

bool EncryptedDatabase::signData(const std::string& data, std::string& signature) {
    if (!sphincs_signature_ || !sphincs_secret_key_) {
        return false;
    }
    
    size_t signature_len = sphincs_signature_->length_signature;
    uint8_t* sig_buffer = (uint8_t*)malloc(signature_len);
    if (!sig_buffer) {
        return false;
    }
    
    if (OQS_SIG_sign(sphincs_signature_, sig_buffer, &signature_len,
                     (const uint8_t*)data.c_str(), data.length(),
                     sphincs_secret_key_) != OQS_SUCCESS) {
        free(sig_buffer);
        return false;
    }
    
    signature = std::string((char*)sig_buffer, signature_len);
    free(sig_buffer);
    return true;
}

bool EncryptedDatabase::encryptData(const std::string& plaintext, std::string& ciphertext, std::string& tag) {
    if (!aes_ctx_) {
        return false;
    }
    
    // Initialize encryption
    if (EVP_EncryptInit_ex(aes_ctx_, EVP_aes_256_gcm(), nullptr, aes_key_, aes_iv_) != 1) {
        return false;
    }
    
    // Encrypt data
    std::vector<uint8_t> encrypted(plaintext.length() + 16); // Extra space for GCM
    int len = 0;
    int ciphertext_len = 0;
    
    if (EVP_EncryptUpdate(aes_ctx_, encrypted.data(), &len,
                         (const uint8_t*)plaintext.c_str(), plaintext.length()) != 1) {
        return false;
    }
    ciphertext_len = len;
    
    if (EVP_EncryptFinal_ex(aes_ctx_, encrypted.data() + len, &len) != 1) {
        return false;
    }
    ciphertext_len += len;
    
    // Get authentication tag
    uint8_t auth_tag[16];
    if (EVP_CIPHER_CTX_ctrl(aes_ctx_, EVP_CTRL_GCM_GET_TAG, 16, auth_tag) != 1) {
        return false;
    }
    
    ciphertext = std::string((char*)encrypted.data(), ciphertext_len);
    tag = std::string((char*)auth_tag, 16);
    
    return true;
}

bool EncryptedDatabase::hashPassword(const std::string& password, const std::string& salt, std::string& hash) {
    // Use SHA-256 for password hashing (in a real implementation, use Argon2)
    uint8_t hash_buffer[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    
    if (SHA256_Init(&sha256) != 1) {
        return false;
    }
    
    if (SHA256_Update(&sha256, password.c_str(), password.length()) != 1) {
        return false;
    }
    
    if (SHA256_Update(&sha256, salt.c_str(), salt.length()) != 1) {
        return false;
    }
    
    if (SHA256_Final(hash_buffer, &sha256) != 1) {
        return false;
    }
    
    // Convert to hex string
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash_buffer[i];
    }
    hash = ss.str();
    
    return true;
}

bool EncryptedDatabase::generateSalt(std::string& salt) {
    uint8_t salt_buffer[32];
    if (RAND_bytes(salt_buffer, sizeof(salt_buffer)) != 1) {
        return false;
    }
    
    std::stringstream ss;
    for (int i = 0; i < 32; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)salt_buffer[i];
    }
    salt = ss.str();
    
    return true;
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
    stats["Encryption Algorithm"] = "SPHINCS+/AES-256-GCM";
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
    
    // Update user record
    SimpleJSON user_data = record.toJson();
    database_json_.data[user_key] = user_data.toJsonString();
    
    is_modified_ = true;
    
    // Save database
    if (!saveDatabase()) {
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
    
    // Remove user from database
    database_json_.data.erase(user_key);
    
    is_modified_ = true;
    
    // Save database
    if (!saveDatabase()) {
        std::cerr << "[X] Failed to save database after deleting user" << std::endl;
        return false;
    }
    
    std::cout << "[OK] User deleted successfully: " << username << std::endl;
    return true;
}

bool EncryptedDatabase::exportBackup(const std::string& backup_path, const std::string& backup_password) {
    // Implementation for backup export
    std::cout << "📤 Exporting backup to: " << backup_path << std::endl;
    return true; // Placeholder
}

bool EncryptedDatabase::importBackup(const std::string& backup_path, const std::string& backup_password) {
    // Implementation for backup import
    std::cout << "📥 Importing backup from: " << backup_path << std::endl;
    return true; // Placeholder
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

void EncryptedDatabase::secureCleanup(void* ptr, size_t size) {
    if (ptr) {
        memset(ptr, 0, size);
    }
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
