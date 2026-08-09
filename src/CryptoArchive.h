#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include "SecureMemory.h"

struct FileEntry {
    std::string name;
    std::string path;
    std::vector<uint8_t> data;
    size_t size;
    std::string timestamp;
    std::string hash;
};

class CryptoArchive {
public:
    CryptoArchive(const std::string& username, const std::string& archiveName = "img");
    ~CryptoArchive();
    
    // Static method to find all archives for a given user
    static std::vector<std::string> FindUserArchives(const std::string& username);
    
    // Static method to create a new archive with a given name
    static bool CreateNewArchive(const std::string& username, const std::string& password, const std::string& archiveName);

    // Rename the encrypted archive file without decrypting or replacing data.
    static bool RenameArchive(const std::string& username,
                              const std::string& currentName,
                              const std::string& newName,
                              std::string* error = nullptr);
    
    // Initialize archive for user
    bool InitializeArchive(const std::string& password);
    
    // Load existing archive
    bool LoadArchive(const std::string& password);

    // Reload using the credential already retained by this archive instance.
    bool ReloadArchive();
    
    // Save archive to encrypted file
    bool SaveArchive();
    
    // Set archive name (changes the target file)
    void SetArchiveName(const std::string& archiveName);
    
    // Get current archive name
    std::string GetArchiveName() const;
    
    // Add file to archive
    bool AddFile(const std::string& filePath, const std::string& name = "");
    
    // Extract file from archive
    bool ExtractFile(const std::string& name, const std::string& outputPath);
    
    // Remove file from archive
    bool RemoveFile(const std::string& name);
    
    // List all files in archive
    std::vector<FileEntry> GetFileList() const;
    
    // Get file data
    std::vector<uint8_t> GetFileData(const std::string& name) const;
    
    // Check if archive exists
    bool ArchiveExists() const;
    
    // Get archive statistics
    struct ArchiveStats {
        size_t totalFiles;
        size_t totalSize;
        std::string lastModified;
    };
    ArchiveStats GetStats() const;
    
    // Change the password/encryption key for the archive
    bool ChangePassword(const std::string& oldPassword, const std::string& newPassword);

    // Build and validate a re-encrypted archive without changing memory or disk.
    bool PreparePasswordChange(const std::string& oldPassword,
                               const std::string& newPassword,
                               std::vector<uint8_t>& replacement) const;
    
    // Verify archive integrity
    bool VerifyIntegrity() const;
        // Extract file to memory
    // This function is used internally to read file data into memory
    // It can also be used externally if needed
    bool ExtractFileToMemory(const std::string& name, std::vector<uint8_t>& outData);
    
    // Diagnostic function to check archive state
    void DiagnoseArchive();
    
    // Reset and recreate archive 
    // This function can be used when the archive is corrupted
    bool ResetArchive(const std::string& password);
    bool ResetArchive();
    
    // Attempt to repair archive issues
    // This will scan for issues and try to fix them
    bool RepairArchive();

    // Utility functions
    std::string GetArchiveFilePath() const;

private:
    // User and archive identity
    std::string m_username;
    std::string m_archiveName;
    std::string m_archivePath;
    bool m_identityValid;
    std::string m_diskRevision;
    bool m_hasDiskRevision;
    
    // Security and state
    SecureMemory::SecureString m_password;
    bool m_isLoaded;
    
    // Archive content
    std::map<std::string, FileEntry> m_files;
    

    
    // Decrypt archive data
    std::vector<uint8_t> DecryptArchiveData(const std::string& password,
                                            bool* legacyFormat = nullptr,
                                            std::string* diskRevision = nullptr) const;
    
    // Serialize archive to binary
    std::vector<uint8_t> SerializeArchive() const;

    bool BuildEncryptedArchive(const std::string& password,
                               std::vector<uint8_t>& output) const;
    
    // Deserialize archive from binary
    bool DeserializeArchive(const std::vector<uint8_t>& data);
    
    // Calculate file hash
    std::string CalculateFileHash(const std::vector<uint8_t>& data) const;
    
    // Get timestamp
    std::string GetCurrentTimestamp() const;

    void ClearDecryptedData() noexcept;

};
