#include "CryptoArchive.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <algorithm> // Added for std::transform
#include <openssl/sha.h>
#include <oqs/oqs.h>

CryptoArchive::CryptoArchive(const std::string& username, const std::string& archiveName) 
    : m_username(username), m_archiveName(archiveName), m_isLoaded(false) {
    m_archivePath = GetArchiveFilePath();
    // Ensure the archives directory exists
    std::filesystem::create_directories("archives");
}

CryptoArchive::~CryptoArchive() {
    // Clear sensitive data
    if (!m_encryptionKey.empty()) {
        std::fill(m_encryptionKey.begin(), m_encryptionKey.end(), 0);
    }
}

bool CryptoArchive::InitializeArchive(const std::string& password) {
    if (ArchiveExists()) {
        std::cout << "Archive already exists for user: " << m_username << std::endl;
        return LoadArchive(password);
    }
    
    // Initialize empty archive
    m_files.clear();
    m_isLoaded = true;
    m_password = password; // Store the password for encryption
    
    std::cout << "Initialized new archive for user: " << m_username << std::endl;
    return SaveArchive();
}

bool CryptoArchive::LoadArchive(const std::string& password) {
    std::cout << "\n---------- LOAD ARCHIVE ----------" << std::endl;
    std::cout << "Loading archive for user: " << m_username << std::endl;
    std::cout << "Archive path: " << m_archivePath << std::endl;
    
    if (!ArchiveExists()) {
        std::cout << "Archive does not exist for user: " << m_username << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return false;
    }
    
    // Verifică dimensiunea fișierului arhivei
    auto fileSize = std::filesystem::file_size(m_archivePath);
    std::cout << "Archive file size: " << fileSize << " bytes" << std::endl;
    
    if (fileSize < 16) { // Minimum size for header + data size
        std::cout << "Archive file is too small to be valid (" << fileSize << " bytes)" << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return false;
    }
    
    try {
        std::cout << "Decrypting archive data..." << std::endl;
        std::vector<uint8_t> decryptedData = DecryptArchiveData(password);
        if (decryptedData.empty()) {
            std::cout << "Failed to decrypt archive for user: " << m_username << std::endl;
            std::cout << "---------------------------------\n" << std::endl;
            return false;
        }
        
        std::cout << "Decrypted data size: " << decryptedData.size() << " bytes" << std::endl;
        
        // Resetează starea arhivei înainte de a încerca deserializarea
        m_files.clear();
        m_isLoaded = false;
        
        // Store the password for future save operations
        m_password = password;
        
        std::cout << "Deserializing archive data..." << std::endl;
        if (!DeserializeArchive(decryptedData)) {
            std::cout << "Failed to deserialize archive for user: " << m_username << std::endl;
            std::cout << "---------------------------------\n" << std::endl;
            return false;
        }
        
        // Verifică că există cel puțin un fișier în arhivă sau că este o arhivă nouă validă
        std::cout << "Files in archive after loading: " << m_files.size() << std::endl;
        
        // Verifică dacă fișierele încărcate au date valide
        bool allFilesValid = true;
        for (const auto& file : m_files) {
            if (file.second.data.empty() || file.second.size == 0) {
                std::cout << "WARNING: File '" << file.first << "' has no data or zero size!" << std::endl;
                allFilesValid = false;
            }
            
            if (file.second.data.size() != file.second.size) {
                std::cout << "WARNING: File '" << file.first << "' has size mismatch! " 
                          << "Reported: " << file.second.size << ", Actual: " << file.second.data.size() << std::endl;
            }
        }
        
        if (!allFilesValid) {
            std::cout << "Some files in the archive have invalid data!" << std::endl;
        }
        
        // Setăm arhiva ca încărcată
        m_isLoaded = true;
        std::cout << "Successfully loaded archive for user: " << m_username << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cout << "Error loading archive: " << e.what() << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return false;
    }
}

bool CryptoArchive::SaveArchive() {
    std::cout << "\n---------- SAVE ARCHIVE ----------" << std::endl;
    
    if (!m_isLoaded) {
        std::cout << "Cannot save - archive not loaded!" << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return false;
    }
    
    try {
        // Verifică dacă există fișiere în arhivă
        std::cout << "Files in archive: " << m_files.size() << std::endl;
        
        // Listează toate fișierele și dimensiunile pentru debugging
        std::cout << "Files to be saved:" << std::endl;
        size_t totalDataSize = 0;
        for (const auto& file : m_files) {
            totalDataSize += file.second.data.size();
            std::cout << "  - '" << file.first << "' (size: " << file.second.data.size() << " bytes)" << std::endl;
        }
        std::cout << "Total data size to be saved: " << totalDataSize << " bytes" << std::endl;
        
        // Verifică fiecare fișier pentru date valide
        bool allFilesValid = true;
        for (const auto& file : m_files) {
            if (file.second.data.empty() && file.second.size > 0) {
                std::cerr << "WARNING: File '" << file.first << "' has empty data but non-zero size!" << std::endl;
                allFilesValid = false;
            }
            if (file.second.size != file.second.data.size()) {
                std::cerr << "WARNING: File '" << file.first << "' has size mismatch! "
                          << "Reported: " << file.second.size << ", Actual: " << file.second.data.size() << " bytes" << std::endl;
                allFilesValid = false;
            }
        }
        if (!allFilesValid) {
            std::cerr << "Some files have inconsistent state! This might cause problems when loading the archive later." << std::endl;
            std::cerr << "Consider using ResetArchive() if problems persist." << std::endl;
        }
        
        // Serializează datele
        std::cout << "Serializing archive data..." << std::endl;
        std::vector<uint8_t> serializedData = SerializeArchive();
        if (serializedData.empty()) {
            std::cerr << "Failed to serialize archive - empty data returned" << std::endl;
            std::cout << "---------------------------------\n" << std::endl;
            return false;
        }
        
        std::cout << "Serialized data size: " << serializedData.size() << " bytes" << std::endl;
        
        // Ensure the archive directory exists
        std::filesystem::create_directories("archives");
        
        // Check if directory exists
        if (!std::filesystem::exists("archives")) {
            std::cerr << "Archive directory does not exist after attempting to create it!" << std::endl;
            std::cerr << "Trying to create directory manually..." << std::endl;
            
            try {
                bool created = std::filesystem::create_directory("archives");
                std::cout << "Directory creation result: " << (created ? "Success" : "Failed") << std::endl;
                
                // Check again
                if (!std::filesystem::exists("archives")) {
                    std::cerr << "Failed to create archives directory! Check permissions." << std::endl;
                    std::cout << "---------------------------------\n" << std::endl;
                    return false;
                }
            } catch (const std::exception& e) {
                std::cerr << "Exception while creating directory: " << e.what() << std::endl;
                std::cout << "---------------------------------\n" << std::endl;
                return false;
            }
        }
        
        // Deschide fișierul pentru scriere
        std::cout << "Opening archive file for writing: " << m_archivePath << std::endl;
        std::ofstream file(m_archivePath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "Failed to open archive file for writing: " << m_archivePath << std::endl;
            std::cerr << "Error: " << strerror(errno) << std::endl;
            std::cout << "---------------------------------\n" << std::endl;
            return false;
        }
        
        // For simplicity in this version, we'll use a magic header to indicate this is a simple encrypted archive
        const char* magicHeader = "PQCENC01"; // 8 bytes magic identifier
        file.write(magicHeader, 8);
        
        if (file.fail()) {
            std::cerr << "Error writing magic header!" << std::endl;
            file.close();
            std::cout << "---------------------------------\n" << std::endl;
            return false;
        }
        
        // Derive a simple encryption key from the password
        std::vector<uint8_t> key;
        if (!m_password.empty()) {
            std::cout << "Deriving key from password..." << std::endl;
            key = DeriveKeyFromPassword(m_password);
        } else {
            std::cout << "WARNING: No password provided, using default key (INSECURE)" << std::endl;
            // Default key if no password (insecure, but prevents crash)
            key.resize(32, 0);
        }
        
        std::cout << "Key size: " << key.size() << " bytes" << std::endl;
        
        // Simple XOR encryption of the data
        std::cout << "Encrypting data..." << std::endl;
        std::vector<uint8_t> encryptedData = serializedData;
        for (size_t i = 0; i < serializedData.size(); i++) {
            encryptedData[i] = serializedData[i] ^ key[i % key.size()];
        }
        
        // Write the data size
        uint64_t dataSize = encryptedData.size();
        std::cout << "Writing encrypted data size: " << dataSize << " bytes" << std::endl;
        file.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
        
        if (file.fail()) {
            std::cerr << "Error writing data size!" << std::endl;
            file.close();
            std::cout << "---------------------------------\n" << std::endl;
            return false;
        }
        
        // Write the encrypted data
        std::cout << "Writing encrypted data..." << std::endl;
        file.write(reinterpret_cast<const char*>(encryptedData.data()), encryptedData.size());
        
        if (file.fail()) {
            std::cerr << "Error writing encrypted data!" << std::endl;
            file.close();
            std::cout << "---------------------------------\n" << std::endl;
            return false;
        }
        
        file.flush();
        file.close();
        
        // Verifică dacă fișierul a fost scris cu succes
        if (std::filesystem::exists(m_archivePath)) {
            auto fileSize = std::filesystem::file_size(m_archivePath);
            std::cout << "Archive file size on disk: " << fileSize << " bytes" << std::endl;
            std::cout << "Expected size: " << (8 + sizeof(dataSize) + encryptedData.size()) << " bytes" << std::endl;
        } else {
            std::cerr << "WARNING: Archive file doesn't exist after saving!" << std::endl;
        }
        
        std::cout << "Archive saved with encryption to: " << m_archivePath << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving archive: " << e.what() << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return false;
    }
}

bool CryptoArchive::AddFile(const std::string& filePath, const std::string& name) {
    std::cout << "---------- CRYPTO ARCHIVE ADD FILE ----------" << std::endl;
    std::cout << "AddFile called with path: '" << filePath << "'" << std::endl;
    std::cout << "Display name: '" << name << "'" << std::endl;
    std::cout << "Archive loaded state: " << (m_isLoaded ? "Yes" : "No") << std::endl;
    
    if (!m_isLoaded) {
        std::cout << "Archive not loaded, returning false" << std::endl;
        std::cout << "-------------------------------------------" << std::endl;
        return false;
    }
    
    // Print diagnostics about the archive state before adding
    std::cout << "Current archive state:" << std::endl;
    std::cout << "Files in archive: " << m_files.size() << std::endl;
    
    try {
        // Check if file exists before trying to open it
        if (!std::filesystem::exists(filePath)) {
            std::cout << "File doesn't exist: " << filePath << std::endl;
            std::cout << "-------------------------------------------" << std::endl;
            return false;
        }
        
        // Check if file is readable
        if (!std::filesystem::is_regular_file(filePath)) {
            std::cout << "Path is not a regular file: " << filePath << std::endl;
            std::cout << "-------------------------------------------" << std::endl;
            return false;
        }
        
        // Read file data
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            std::cout << "Failed to open file: " << filePath << std::endl;
            std::cout << "Error state: " << strerror(errno) << std::endl;
            std::cout << "-------------------------------------------" << std::endl;
            return false;
        }
        
        std::cout << "File opened successfully" << std::endl;
        
        // Get file size
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::cout << "File size: " << fileSize << " bytes" << std::endl;
        
        // Read file content
        std::vector<uint8_t> fileData(fileSize);
        file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
        if (!file) {
            std::cout << "Failed to read entire file. Only read " << file.gcount() << " bytes" << std::endl;
            file.close();
            std::cout << "-------------------------------------------" << std::endl;
            return false;
        }
        file.close();
        
        std::cout << "File read successfully" << std::endl;
        
        // Create file entry
        FileEntry entry;
        entry.name = name.empty() ? std::filesystem::path(filePath).filename().string() : name;
        entry.path = filePath;
        entry.data = fileData;
        entry.size = fileSize;
        entry.timestamp = GetCurrentTimestamp();
        entry.hash = CalculateFileHash(fileData);
        
        std::cout << "Entry created with name: " << entry.name << std::endl;
        
        // Add to archive
        m_files[entry.name] = entry;
        
        std::cout << "Added file to archive: " << entry.name << " (" << fileSize << " bytes)" << std::endl;
        std::cout << "Files in archive after adding: " << m_files.size() << std::endl;
        
        // Verify addition was successful
        auto verifyIt = m_files.find(entry.name);
        if (verifyIt != m_files.end()) {
            std::cout << "Verified: File '" << entry.name << "' exists in archive" << std::endl;
            std::cout << "Stored data size: " << verifyIt->second.data.size() << " bytes" << std::endl;
        } else {
            std::cout << "WARNING: Failed to verify file was added properly!" << std::endl;
        }
        
        // Save the archive to ensure the file is persisted
        std::cout << "Saving archive after adding file..." << std::endl;
        bool saveResult = SaveArchive();
        if (!saveResult) {
            std::cout << "WARNING: Failed to save archive after adding file!" << std::endl;
        }
        
        std::cout << "-------------------------------------------" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cout << "Error adding file to archive: " << e.what() << std::endl;
        std::cout << "-------------------------------------------" << std::endl;
        return false;
    }
}

bool CryptoArchive::ExtractFile(const std::string& name, const std::string& outputPath) {
    std::cout << "\n---------- EXTRACT FILE ----------" << std::endl;
    std::cout << "Extracting file: '" << name << "' to path: '" << outputPath << "'" << std::endl;
    
    if (!m_isLoaded) {
        std::cout << "Archive not loaded!" << std::endl;
        std::cout << "----------------------------------\n" << std::endl;
        return false;
    }
    
    // Diagnosticarea completă a arhivei pentru a vedea starea sa
    DiagnoseArchive();
    
    std::cout << "Searching for file '" << name << "' in archive..." << std::endl;
    std::cout << "Number of files in archive: " << m_files.size() << std::endl;
    
    // Debugging - list all files in archive
    std::cout << "Files in archive:" << std::endl;
    for (const auto& file : m_files) {
        std::cout << "  - '" << file.first << "' (size: " << file.second.data.size() << " bytes)" << std::endl;
    }
    
    // Căutare explicită, caz-insensitivă pentru mai multă reziliență
    const FileEntry* foundEntry = nullptr;
    // Prima încercare - potrivire exactă
    auto it = m_files.find(name);
    if (it != m_files.end()) {
        foundEntry = &(it->second);
        std::cout << "File found with exact match: '" << name << "'" << std::endl;
    } else {
        // A doua încercare - verifică toate cheile, poate există diferențe de majuscule/minuscule
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        for (const auto& file : m_files) {
            std::string lowerKey = file.first;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
            
            if (lowerKey == lowerName) {
                foundEntry = &(file.second);
                std::cout << "File found with case-insensitive match. Requested: '" << name 
                          << "', Found: '" << file.first << "'" << std::endl;
                break;
            }
        }
    }
    
    if (!foundEntry) {
        std::cout << "File not found in archive: '" << name << "'" << std::endl;
        std::cout << "----------------------------------\n" << std::endl;
        return false;
    }
    
    std::cout << "File found! Size: " << foundEntry->data.size() << " bytes" << std::endl;
    std::cout << "File data empty? " << (foundEntry->data.empty() ? "Yes" : "No") << std::endl;
    
    try {
        // Verifică dacă datele fișierului sunt valide
        if (foundEntry->data.empty()) {
            std::cout << "ERROR: File data is empty!" << std::endl;
            std::cout << "----------------------------------\n" << std::endl;
            return false;
        }
        
        // Parse the output path
        std::filesystem::path path(outputPath);
        std::string finalPath = outputPath;
        
        // Check if the output path is a directory or ends with a directory separator
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            // If it's a directory, append the filename to create a valid file path
            std::cout << "Output path is a directory, appending filename: " << name << std::endl;
            finalPath = (path / name).string();
            std::cout << "New output path: " << finalPath << std::endl;
        } else if (outputPath.back() == '/' || outputPath.back() == '\\') {
            // If path ends with a separator, it's meant to be a directory, append filename
            std::cout << "Output path ends with a separator, appending filename: " << name << std::endl;
            std::filesystem::path dirPath(outputPath);
            finalPath = (dirPath / name).string();
            std::cout << "New output path: " << finalPath << std::endl;
        }
        
        // Create parent directories if they don't exist
        std::filesystem::path finalPathObj(finalPath);
        std::filesystem::path parentPath = finalPathObj.parent_path();
        if (!parentPath.empty()) {
            std::cout << "Creating parent directories: " << parentPath << std::endl;
            try {
                std::filesystem::create_directories(parentPath);
            } catch (const std::exception& e) {
                std::cout << "Failed to create directories: " << e.what() << std::endl;
            }
        }
        
        std::cout << "Opening output file: " << finalPath << std::endl;
        std::ofstream file(finalPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            std::cout << "Failed to create output file: " << finalPath << std::endl;
            std::cout << "Error: " << strerror(errno) << std::endl;
            std::cout << "----------------------------------\n" << std::endl;
            return false;
        }
        
        std::cout << "Writing " << foundEntry->data.size() << " bytes to file..." << std::endl;
        file.write(reinterpret_cast<const char*>(foundEntry->data.data()), foundEntry->data.size());
        
        // Verificați starea fișierului după scriere
        if (file.fail()) {
            std::cout << "ERROR: Failed to write data to file!" << std::endl;
            std::cout << "Error state: " << strerror(errno) << std::endl;
            file.close();
            std::cout << "----------------------------------\n" << std::endl;
            return false;
        }
        
        file.flush();
        file.close();
        
        // Verify the file was written successfully
        if (std::filesystem::exists(finalPath)) {
            auto fileSize = std::filesystem::file_size(finalPath);
            std::cout << "File successfully written. Size on disk: " << fileSize << " bytes" << std::endl;
            if (fileSize != foundEntry->data.size()) {
                std::cout << "WARNING: File size mismatch between disk (" << fileSize << ") and memory (" 
                          << foundEntry->data.size() << ")!" << std::endl;
            }
        } else {
            std::cout << "WARNING: File doesn't exist after writing!" << std::endl;
        }
        
        std::cout << "Extracted file: '" << name << "' to '" << finalPath << "'" << std::endl;
        std::cout << "----------------------------------\n" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cout << "Error extracting file: " << e.what() << std::endl;
        std::cout << "----------------------------------\n" << std::endl;
        return false;
    }
}

bool CryptoArchive::ExtractFileToMemory(const std::string& name, std::vector<uint8_t>& outData) {
    std::cout << "\n---------- EXTRACT FILE TO MEMORY ----------" << std::endl;
    std::cout << "ExtractFileToMemory called for file: '" << name << "'" << std::endl;
    
    if (!m_isLoaded) {
        std::cerr << "Archive not loaded!" << std::endl;
        std::cout << "------------------------------------------\n" << std::endl;
        return false;
    }
    
    // Ensure outData is empty initially
    outData.clear();
    
    // Diagnosticarea completă a arhivei pentru a vedea starea sa
    DiagnoseArchive();
    
    // Detailed debug information
    std::cout << "Looking for file: '" << name << "'" << std::endl;
    
    // Căutare explicită, caz-insensitivă pentru mai multă reziliență
    const FileEntry* foundEntry = nullptr;
    // Prima încercare - potrivire exactă
    auto it = m_files.find(name);
    if (it != m_files.end()) {
        foundEntry = &(it->second);
        std::cout << "File found with exact match: '" << name << "'" << std::endl;
    } else {
        // A doua încercare - verifică toate cheile, poate există diferențe de majuscule/minuscule
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        for (const auto& file : m_files) {
            std::string lowerKey = file.first;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
            
            if (lowerKey == lowerName) {
                foundEntry = &(file.second);
                std::cout << "File found with case-insensitive match. Requested: '" << name 
                          << "', Found: '" << file.first << "'" << std::endl;
                break;
            }
        }
    }
    
    if (!foundEntry) {
        std::cerr << "File not found: '" << name << "'" << std::endl;
        
        // Print all file names to help debug
        std::cout << "Available files in archive:" << std::endl;
        for (const auto& file : m_files) {
            std::cout << "  - '" << file.first << "' (size: " << file.second.data.size() << " bytes)" << std::endl;
        }
        
        std::cout << "------------------------------------------\n" << std::endl;
        return false;
    }
    
    std::cout << "File found! Name: " << foundEntry->name << ", Size: " << foundEntry->data.size() << " bytes" << std::endl;
    std::cout << "File data empty? " << (foundEntry->data.empty() ? "Yes" : "No") << std::endl;
    
    // Verifică dacă datele fișierului sunt valide
    if (foundEntry->data.empty()) {
        std::cerr << "ERROR: File data is empty!" << std::endl;
        std::cout << "------------------------------------------\n" << std::endl;
        return false;
    }
    
    try {
        // Copiază datele fișierului în buffer-ul de ieșire
        outData = foundEntry->data;
        
        // Verifică încă o dată că datele au fost copiate corect
        if (outData.empty()) {
            std::cerr << "ERROR: Failed to copy file data to output buffer!" << std::endl;
            std::cout << "------------------------------------------\n" << std::endl;
            return false;
        }
        
        std::cout << "Data copied to output buffer, size: " << outData.size() << " bytes" << std::endl;
        
        // Afișează primele bytes pentru debugging
        std::cout << "First 16 bytes of data: ";
        for (int i = 0; i < std::min(16, (int)outData.size()); i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)outData[i] << " ";
        }
        std::cout << std::dec << std::endl;
        
        std::cout << "------------------------------------------\n" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception during data copy: " << e.what() << std::endl;
        std::cout << "------------------------------------------\n" << std::endl;
        return false;
    }
}

bool CryptoArchive::RemoveFile(const std::string& name) {
    if (!m_isLoaded) {
        return false;
    }
    
    auto it = m_files.find(name);
    if (it == m_files.end()) {
        return false;
    }
    
    m_files.erase(it);
    std::cout << "Removed file from archive: " << name << std::endl;
    return true;
}

std::vector<FileEntry> CryptoArchive::GetFileList() const {
    std::vector<FileEntry> fileList;
    for (const auto& pair : m_files) {
        fileList.push_back(pair.second);
    }
    return fileList;
}

std::vector<uint8_t> CryptoArchive::GetFileData(const std::string& name) const {
    if (!m_isLoaded) {
        return {};
    }
    
    auto it = m_files.find(name);
    if (it == m_files.end()) {
        return {};
    }
    
    return it->second.data;
}

bool CryptoArchive::ArchiveExists() const {
    return std::filesystem::exists(m_archivePath);
}

CryptoArchive::ArchiveStats CryptoArchive::GetStats() const {
    ArchiveStats stats;
    stats.totalFiles = m_files.size();
    stats.totalSize = 0;
    stats.lastModified = GetCurrentTimestamp();
    
    for (const auto& pair : m_files) {
        stats.totalSize += pair.second.size;
    }
    
    return stats;
}

bool CryptoArchive::VerifyIntegrity() const {
    if (!m_isLoaded) {
        return false;
    }
    
    for (const auto& pair : m_files) {
        const FileEntry& entry = pair.second;
        std::string calculatedHash = CalculateFileHash(entry.data);
        if (calculatedHash != entry.hash) {
            std::cout << "Integrity check failed for file: " << entry.name << std::endl;
            return false;
        }
    }
    
    return true;
}

std::vector<uint8_t> CryptoArchive::DeriveKeyFromPassword(const std::string& password) const {
    // Using PBKDF2 would be better, but for simplicity we're using SHA256
    // In a production environment, use a proper key derivation function with salt and iterations
    std::vector<uint8_t> key(32);
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, password.c_str(), password.length());
    SHA256_Final(hash, &sha256);
    
    std::copy(hash, hash + 32, key.begin());
    return key;
}

bool CryptoArchive::EncryptArchiveData(const std::vector<uint8_t>& data, const std::string& password) {
    // This function is no longer used directly - encryption is now done in SaveArchive
    // to simplify the implementation and ensure a consistent format
    m_encryptionKey = DeriveKeyFromPassword(password);
    return true;
}

std::vector<uint8_t> CryptoArchive::DecryptArchiveData(const std::string& password) const {
    try {
        // Derive the key from the password
        std::vector<uint8_t> key = DeriveKeyFromPassword(password);
        
        std::ifstream file(m_archivePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file for reading" << std::endl;
            return {};
        }
        
        // Check file size first
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        if (fileSize < 16) {  // Magic + size at minimum
            std::cerr << "Error: File is too small to be a valid archive" << std::endl;
            return {};
        }
        
        // Check for magic header
        char magicHeader[9] = {0};
        file.read(magicHeader, 8);
        
        if (std::string(magicHeader) == "PQCENC01") {
            // New format with simple encryption
            
            // Read data size
            uint64_t dataSize;
            file.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));
            
            if (dataSize > fileSize - 16 || dataSize == 0) {
                std::cerr << "Error: Invalid data size in file" << std::endl;
                return {};
            }
            
            // Read encrypted data
            std::vector<uint8_t> encryptedData(dataSize);
            file.read(reinterpret_cast<char*>(encryptedData.data()), dataSize);
            file.close();
            
            // Decrypt with XOR
            std::vector<uint8_t> decryptedData = encryptedData;
            for (size_t i = 0; i < encryptedData.size(); i++) {
                decryptedData[i] = encryptedData[i] ^ key[i % key.size()];
            }
            
            std::cout << "Archive decrypted successfully!" << std::endl;
            return decryptedData;
        } else {
            // Old format or not encrypted - just return the raw data
            file.seekg(0, std::ios::beg);
            std::vector<uint8_t> rawData(fileSize);
            file.read(reinterpret_cast<char*>(rawData.data()), fileSize);
            file.close();
            
            std::cout << "Warning: File does not have encryption header, returning raw data" << std::endl;
            return rawData;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during decryption: " << e.what() << std::endl;
        return {};
    }
}

std::vector<uint8_t> CryptoArchive::SerializeArchive() const {
    std::vector<uint8_t> data;
    
    // Simple serialization format:
    // [num_files:4][file1_name_len:4][file1_name][file1_size:8][file1_data][timestamp_len:4][timestamp][hash_len:4][hash]...
    
    try {
        // Write number of files
        uint32_t numFiles = static_cast<uint32_t>(m_files.size());
        data.insert(data.end(), reinterpret_cast<const uint8_t*>(&numFiles), reinterpret_cast<const uint8_t*>(&numFiles) + 4);
        
        // Write each file
        for (const auto& pair : m_files) {
            const FileEntry& entry = pair.second;
            
            // Write file name length and name
            uint32_t nameLen = static_cast<uint32_t>(entry.name.length());
            data.insert(data.end(), reinterpret_cast<const uint8_t*>(&nameLen), reinterpret_cast<const uint8_t*>(&nameLen) + 4);
            data.insert(data.end(), entry.name.begin(), entry.name.end());
            
            // Write file size and data
            uint64_t fileSize = static_cast<uint64_t>(entry.size);
            data.insert(data.end(), reinterpret_cast<const uint8_t*>(&fileSize), reinterpret_cast<const uint8_t*>(&fileSize) + 8);
            data.insert(data.end(), entry.data.begin(), entry.data.end());
            
            // Write timestamp
            uint32_t timestampLen = static_cast<uint32_t>(entry.timestamp.length());
            data.insert(data.end(), reinterpret_cast<const uint8_t*>(&timestampLen), reinterpret_cast<const uint8_t*>(&timestampLen) + 4);
            data.insert(data.end(), entry.timestamp.begin(), entry.timestamp.end());
            
            // Write hash
            uint32_t hashLen = static_cast<uint32_t>(entry.hash.length());
            data.insert(data.end(), reinterpret_cast<const uint8_t*>(&hashLen), reinterpret_cast<const uint8_t*>(&hashLen) + 4);
            data.insert(data.end(), entry.hash.begin(), entry.hash.end());
        }
        
        return data;
    } catch (const std::exception& e) {
        std::cout << "Error serializing archive: " << e.what() << std::endl;
        return {};
    }
}

bool CryptoArchive::DeserializeArchive(const std::vector<uint8_t>& data) {
    try {
        m_files.clear();
        
        if (data.size() < 4) {
            std::cerr << "Data size too small for deserialization: " << data.size() << " bytes" << std::endl;
            return false;
        }
        
        // Debug info
        std::cout << "Deserializing data of size: " << data.size() << " bytes" << std::endl;
        std::cout << "First 16 bytes: ";
        for (int i = 0; i < std::min(16, (int)data.size()); i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
        }
        std::cout << std::dec << std::endl;
        
        size_t offset = 0;
        
        // Read number of files
        uint32_t numFiles;
        std::memcpy(&numFiles, data.data() + offset, 4);
        offset += 4;
        
        std::cout << "Number of files in archive: " << numFiles << std::endl;
        
        // Sanity check - if numFiles is very large, it's probably corrupted data
        if (numFiles > 1000) {
            std::cerr << "Unreasonable number of files: " << numFiles << ", data likely corrupted" << std::endl;
            return false;
        }
        
        // Read each file
        for (uint32_t i = 0; i < numFiles; ++i) {
            if (offset + 4 > data.size()) {
                std::cerr << "Data overflow at file " << i << " name length" << std::endl;
                return false;
            }
            
            // Read file name
            uint32_t nameLen;
            std::memcpy(&nameLen, data.data() + offset, 4);
            offset += 4;
            
            // Sanity check
            if (nameLen > 1024) {
                std::cerr << "Unreasonable filename length: " << nameLen << ", data likely corrupted" << std::endl;
                return false;
            }
            
            if (offset + nameLen > data.size()) {
                std::cerr << "Data overflow at file " << i << " name" << std::endl;
                return false;
            }
            
            std::string name(reinterpret_cast<const char*>(data.data() + offset), nameLen);
            offset += nameLen;
            
            std::cout << "Found file: " << name << std::endl;
            
            // Read file size
            if (offset + 8 > data.size()) {
                std::cerr << "Data overflow at file " << i << " size" << std::endl;
                return false;
            }
            
            uint64_t fileSize;
            std::memcpy(&fileSize, data.data() + offset, 8);
            offset += 8;
            
            // Sanity check for file size
            if (fileSize > data.size()) {
                std::cerr << "Unreasonable file size: " << fileSize << ", data likely corrupted" << std::endl;
                return false;
            }
            
            std::cout << "File size: " << fileSize << " bytes" << std::endl;
            
            // Read file data
            if (offset + fileSize > data.size()) {
                std::cerr << "Data overflow at file " << i << " data" << std::endl;
                return false;
            }
            
            std::vector<uint8_t> fileData(fileSize);
            std::memcpy(fileData.data(), data.data() + offset, fileSize);
            offset += fileSize;
            
            // Read timestamp
            if (offset + 4 > data.size()) {
                std::cerr << "Data overflow at file " << i << " timestamp length" << std::endl;
                return false;
            }
            
            uint32_t timestampLen;
            std::memcpy(&timestampLen, data.data() + offset, 4);
            offset += 4;
            
            // Sanity check
            if (timestampLen > 64) {
                std::cerr << "Unreasonable timestamp length: " << timestampLen << ", data likely corrupted" << std::endl;
                return false;
            }
            
            if (offset + timestampLen > data.size()) {
                std::cerr << "Data overflow at file " << i << " timestamp" << std::endl;
                return false;
            }
            
            std::string timestamp(reinterpret_cast<const char*>(data.data() + offset), timestampLen);
            offset += timestampLen;
            
            std::cout << "Timestamp: " << timestamp << std::endl;
            
            // Read hash
            if (offset + 4 > data.size()) {
                std::cerr << "Data overflow at file " << i << " hash length" << std::endl;
                return false;
            }
            
            uint32_t hashLen;
            std::memcpy(&hashLen, data.data() + offset, 4);
            offset += 4;
            
            // Sanity check
            if (hashLen > 128) {
                std::cerr << "Unreasonable hash length: " << hashLen << ", data likely corrupted" << std::endl;
                return false;
            }
            
            if (offset + hashLen > data.size()) {
                std::cerr << "Data overflow at file " << i << " hash" << std::endl;
                return false;
            }
            
            std::string hash(reinterpret_cast<const char*>(data.data() + offset), hashLen);
            offset += hashLen;
            
            std::cout << "Hash: " << hash << std::endl;
            
            // Create file entry
            FileEntry entry;
            entry.name = name;
            entry.data = fileData;
            entry.size = fileSize;
            entry.timestamp = timestamp;
            entry.hash = hash;
            
            m_files[name] = entry;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "Error deserializing archive: " << e.what() << std::endl;
        return false;
    }
}

std::string CryptoArchive::CalculateFileHash(const std::vector<uint8_t>& data) const {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.data(), data.size());
    SHA256_Final(hash, &sha256);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

std::string CryptoArchive::GetCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string CryptoArchive::GetArchiveFilePath() const {
    return "archives/" + m_username + "_" + m_archiveName + ".enc";
}

void CryptoArchive::DiagnoseArchive() {
    std::cout << "\n========== ARCHIVE DIAGNOSTIC ==========\n" << std::endl;
    
    // Archive state
    std::cout << "Username: " << m_username << std::endl;
    std::cout << "Archive path: " << m_archivePath << std::endl;
    std::cout << "Archive loaded: " << (m_isLoaded ? "Yes" : "No") << std::endl;
    std::cout << "Archive exists on disk: " << (std::filesystem::exists(m_archivePath) ? "Yes" : "No") << std::endl;
    
    // Check if file member variable is properly initialized
    std::cout << "m_files valid: " << (m_files.empty() ? "Empty" : "Has entries") << std::endl;
    std::cout << "m_files.size(): " << m_files.size() << std::endl;
    
    // Try to fix empty data issues
    int emptyDataFixed = 0;
    for (auto& pair : m_files) {
        FileEntry& entry = pair.second;
        // If size is non-zero but data is empty, something went wrong
        if (entry.size > 0 && entry.data.empty()) {
            std::cout << "WARNING: File '" << entry.name << "' has size " << entry.size 
                      << " but empty data. Attempting to fix..." << std::endl;
            // Create dummy data of the right size
            entry.data.resize(entry.size, 0);
            emptyDataFixed++;
        }
    }
    if (emptyDataFixed > 0) {
        std::cout << "Fixed " << emptyDataFixed << " files with empty data" << std::endl;
    }
    
    if (std::filesystem::exists(m_archivePath)) {
        try {
            auto fileSize = std::filesystem::file_size(m_archivePath);
            std::cout << "Archive file size: " << fileSize << " bytes" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Error getting archive file size: " << e.what() << std::endl;
        }
    }
    
    // Files in memory
    std::cout << "\nFiles in memory: " << m_files.size() << std::endl;
    int count = 0;
    for (const auto& pair : m_files) {
        const FileEntry& entry = pair.second;
        std::cout << "\n[" << count++ << "] File: " << entry.name << std::endl;
        std::cout << "  Size: " << entry.size << " bytes" << std::endl;
        std::cout << "  Data vector size: " << entry.data.size() << " bytes" << std::endl;
        std::cout << "  Timestamp: " << entry.timestamp << std::endl;
        std::cout << "  Hash: " << entry.hash << std::endl;
        
        // Check data integrity
        if (entry.size != entry.data.size()) {
            std::cout << "  WARNING: Size mismatch between entry.size and data.size()" << std::endl;
        }
        
        // Display first few bytes if data exists
        if (!entry.data.empty()) {
            std::cout << "  Data preview (first 16 bytes): ";
            for (int i = 0; i < std::min(16, (int)entry.data.size()); i++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)entry.data[i] << " ";
            }
            std::cout << std::dec << std::endl;
        } else {
            std::cout << "  WARNING: File data is empty!" << std::endl;
        }
    }
    
    std::cout << "\n=========================================\n" << std::endl;
}

bool CryptoArchive::ResetArchive(const std::string& password) {
    std::cout << "\n---------- RESET ARCHIVE ----------" << std::endl;
    std::cout << "Resetting archive for user: " << m_username << std::endl;
    
    // Verifică dacă fișierul arhivei există și îl șterge
    if (ArchiveExists()) {
        std::cout << "Removing existing archive file: " << m_archivePath << std::endl;
        try {
            std::filesystem::remove(m_archivePath);
        } catch (const std::exception& e) {
            std::cout << "Error removing archive file: " << e.what() << std::endl;
            // Continuă totuși
        }
    }
    
    // Resetează starea internă
    m_files.clear();
    m_isLoaded = false;
    
    // Inițializează o nouă arhivă goală
    std::cout << "Creating new empty archive..." << std::endl;
    bool success = InitializeArchive(password);
    
    if (success) {
        std::cout << "Archive successfully reset and reinitialized!" << std::endl;
    } else {
        std::cout << "Failed to reset and reinitialize archive!" << std::endl;
    }
    
    std::cout << "---------------------------------\n" << std::endl;
    return success;
}

bool CryptoArchive::RepairArchive() {
    std::cout << "\n---------- REPAIR ARCHIVE ----------" << std::endl;
    std::cout << "Attempting to repair archive for user: " << m_username << std::endl;
    
    if (!m_isLoaded) {
        std::cout << "Cannot repair - archive not loaded!" << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return false;
    }
    
    int issuesFixed = 0;
    std::vector<std::string> keysToRemove;
    
    std::cout << "Scanning for issues in " << m_files.size() << " files..." << std::endl;
    
    // First pass - identify issues
    for (auto& pair : m_files) {
        const std::string& name = pair.first;
        FileEntry& entry = pair.second;
        bool hasIssues = false;
        
        // Check for empty name
        if (name.empty()) {
            std::cout << "ERROR: Found file with empty name - marking for removal" << std::endl;
            keysToRemove.push_back(name);
            continue;
        }
        
        // Check size/data mismatch
        if (entry.size != entry.data.size()) {
            std::cout << "ISSUE: File '" << name << "' has size mismatch. "
                      << "Reported: " << entry.size << ", Actual: " << entry.data.size() << " bytes" << std::endl;
            // Fix the size to match the actual data
            entry.size = entry.data.size();
            hasIssues = true;
            issuesFixed++;
        }
        
        // Check for empty data but non-zero size
        if (entry.data.empty() && entry.size > 0) {
            std::cout << "ISSUE: File '" << name << "' has empty data but non-zero size" << std::endl;
            // Fix by creating dummy data or resetting size
            entry.data.resize(entry.size, 0);
            hasIssues = true;
            issuesFixed++;
        }
        
        // Check for valid hash
        std::string calculatedHash = CalculateFileHash(entry.data);
        if (calculatedHash != entry.hash) {
            std::cout << "ISSUE: File '" << name << "' has invalid hash" << std::endl;
            entry.hash = calculatedHash;
            hasIssues = true;
            issuesFixed++;
        }
        
        if (hasIssues) {
            std::cout << "Fixed issues with file: '" << name << "'" << std::endl;
        }
    }
    
    // Remove problematic files
    for (const auto& key : keysToRemove) {
        m_files.erase(key);
        std::cout << "Removed invalid file entry with key: '" << key << "'" << std::endl;
        issuesFixed++;
    }
    
    // Save the repaired archive
    if (issuesFixed > 0) {
        std::cout << "Fixed " << issuesFixed << " issues. Saving repaired archive..." << std::endl;
        if (SaveArchive()) {
            std::cout << "Archive successfully repaired and saved!" << std::endl;
            std::cout << "---------------------------------\n" << std::endl;
            return true;
        } else {
            std::cout << "Failed to save repaired archive!" << std::endl;
            std::cout << "---------------------------------\n" << std::endl;
            return false;
        }
    } else {
        std::cout << "No issues found in the archive." << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return true;
    }
}

std::vector<std::string> CryptoArchive::FindUserArchives(const std::string& username) {
    std::cout << "\n---------- FIND USER ARCHIVES ----------" << std::endl;
    std::vector<std::string> archives;
    std::string archivesDir = "archives";
    std::string userPrefix = username + "_";
    
    std::cout << "Looking for archives for user: " << username << std::endl;
    std::cout << "User prefix: " << userPrefix << std::endl;
    std::cout << "Archives directory exists: " << (std::filesystem::exists(archivesDir) ? "Yes" : "No") << std::endl;
    
    // Ensure the archives directory exists
    if (!std::filesystem::exists(archivesDir)) {
        std::cout << "Archives directory does not exist!" << std::endl;
        std::cout << "--------------------------------------\n" << std::endl;
        return archives; // Return empty list if directory doesn't exist
    }
    
    // Iterate through the directory and find all archives that match the username prefix
    std::cout << "Files in archives directory:" << std::endl;
    for (const auto& entry : std::filesystem::directory_iterator(archivesDir)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            std::cout << " - " << filename;
            
            // Check if the file starts with the username prefix
            if (filename.find(userPrefix) == 0) {
                // Extract archive name from filename (remove username_ prefix and .enc extension)
                std::string archiveName = filename.substr(userPrefix.length());
                std::cout << " (matches user prefix)";
                
                size_t extPos = archiveName.rfind(".enc");
                if (extPos != std::string::npos) {
                    archiveName = archiveName.substr(0, extPos);
                    std::cout << ", extracted name: " << archiveName;
                }
                
                archives.push_back(archiveName);
                std::cout << ", added to list" << std::endl;
            } else {
                std::cout << " (no match)" << std::endl;
            }
        } else {
            std::cout << " (not a regular file)" << std::endl;
        }
    }
    
    std::cout << "Found " << archives.size() << " archives for user " << username << std::endl;
    for (size_t i = 0; i < archives.size(); i++) {
        std::cout << " [" << i << "] " << archives[i] << std::endl;
    }
    std::cout << "--------------------------------------\n" << std::endl;
    return archives;
}

void CryptoArchive::SetArchiveName(const std::string& archiveName) {
    m_archiveName = archiveName;
    m_archivePath = GetArchiveFilePath();
}

std::string CryptoArchive::GetArchiveName() const {
    return m_archiveName;
}

bool CryptoArchive::CreateNewArchive(const std::string& username, const std::string& password, const std::string& archiveName) {
    // Verifică dacă există deja o arhivă cu acest nume
    CryptoArchive archive(username, archiveName);
    
    // Dacă arhiva există, returnăm false
    if (archive.ArchiveExists()) {
        return false;
    }
    
    // Inițializăm și salvăm noua arhivă
    return archive.InitializeArchive(password);
}

bool CryptoArchive::ChangePassword(const std::string& oldPassword, const std::string& newPassword) {
    std::cout << "\n---------- CHANGE PASSWORD ----------" << std::endl;
    
    if (!m_isLoaded) {
        std::cout << "Cannot change password - archive not loaded!" << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return false;
    }
    
    // First verify the old password by attempting to decrypt the archive
    std::vector<uint8_t> decryptedData = DecryptArchiveData(oldPassword);
    if (decryptedData.empty()) {
        std::cout << "Invalid old password!" << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return false;
    }
    
    // Old password is correct, update the stored password
    m_password = newPassword;
    std::cout << "Password changed successfully" << std::endl;
    
    // Save the archive with the new password
    bool saveResult = SaveArchive();
    
    std::cout << "---------------------------------\n" << std::endl;
    return saveResult;
}
