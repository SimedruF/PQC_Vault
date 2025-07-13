#include "PasswordManager.h"
#include <oqs/oqs.h>
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

bool PasswordManager::CreateUser(const std::string& username, const std::string& password) {
    if (UserExists(username)) {
        std::cerr << "User already exists: " << username << std::endl;
        return false;
    }
    
    // Initialize Kyber KEM
    OQS_KEM* kem = OQS_KEM_new(OQS_KEM_alg_kyber_768);
    if (!kem) {
        std::cerr << "Failed to initialize Kyber KEM" << std::endl;
        return false;
    }
    
    EncryptedPassword encData;
    
    // Generate key pair
    encData.public_key.resize(kem->length_public_key);
    encData.secret_key.resize(kem->length_secret_key);
    
    if (OQS_KEM_keypair(kem, encData.public_key.data(), encData.secret_key.data()) != OQS_SUCCESS) {
        std::cerr << "Failed to generate key pair" << std::endl;
        OQS_KEM_free(kem);
        return false;
    }
    
    // Encapsulate (generate shared secret)
    encData.ciphertext.resize(kem->length_ciphertext);
    std::vector<uint8_t> shared_secret(kem->length_shared_secret);
    
    if (OQS_KEM_encaps(kem, encData.ciphertext.data(), shared_secret.data(), encData.public_key.data()) != OQS_SUCCESS) {
        std::cerr << "Failed to encapsulate" << std::endl;
        OQS_KEM_free(kem);
        return false;
    }
    
    // Encrypt password with shared secret
    encData.encrypted_password = XOREncrypt(password, shared_secret);
    
    // Save to file
    bool success = SaveEncryptedData(username, encData);
    
    OQS_KEM_free(kem);
    
    if (success) {
        std::cout << "User created successfully: " << username << std::endl;
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
    
    // Initialize Kyber KEM
    OQS_KEM* kem = OQS_KEM_new(OQS_KEM_alg_kyber_768);
    if (!kem) {
        std::cerr << "Failed to initialize Kyber KEM" << std::endl;
        return false;
    }
    
    // Decapsulate to get shared secret
    std::vector<uint8_t> shared_secret(kem->length_shared_secret);
    if (OQS_KEM_decaps(kem, shared_secret.data(), encData.ciphertext.data(), encData.secret_key.data()) != OQS_SUCCESS) {
        std::cerr << "Failed to decapsulate" << std::endl;
        OQS_KEM_free(kem);
        return false;
    }
    
    // Decrypt password
    std::string decrypted_password = XORDecrypt(encData.encrypted_password, shared_secret);
    
    OQS_KEM_free(kem);
    
    bool match = (decrypted_password == password);
    if (match) {
        std::cout << "Password verified successfully for user: " << username << std::endl;
    } else {
        std::cerr << "Password verification failed for user: " << username << std::endl;
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
    
    // Save sizes first, then data
    size_t size = data.ciphertext.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(data.ciphertext.data()), size);
    
    size = data.public_key.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(data.public_key.data()), size);
    
    size = data.secret_key.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(data.secret_key.data()), size);
    
    size = data.encrypted_password.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(data.encrypted_password.data()), size);
    
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
    
    // Load data with sizes
    size_t size;
    
    // Load ciphertext
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (file.gcount() != sizeof(size)) return data;
    data.ciphertext.resize(size);
    file.read(reinterpret_cast<char*>(data.ciphertext.data()), size);
    if (file.gcount() != static_cast<std::streamsize>(size)) return data;
    
    // Load public key
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (file.gcount() != sizeof(size)) return data;
    data.public_key.resize(size);
    file.read(reinterpret_cast<char*>(data.public_key.data()), size);
    if (file.gcount() != static_cast<std::streamsize>(size)) return data;
    
    // Load secret key
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (file.gcount() != sizeof(size)) return data;
    data.secret_key.resize(size);
    file.read(reinterpret_cast<char*>(data.secret_key.data()), size);
    if (file.gcount() != static_cast<std::streamsize>(size)) return data;
    
    // Load encrypted password
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (file.gcount() != sizeof(size)) return data;
    data.encrypted_password.resize(size);
    file.read(reinterpret_cast<char*>(data.encrypted_password.data()), size);
    if (file.gcount() != static_cast<std::streamsize>(size)) return data;
    
    return data;
}

std::string PasswordManager::GetUserFilePath(const std::string& username) const {
    return "users/" + username + ".enc";
}

void PasswordManager::EnsureUsersDirectory() const {
    if (!std::filesystem::exists("users")) {
        std::filesystem::create_directories("users");
    }
}
