#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace PathSecurity {

constexpr std::size_t MAX_USERNAME_BYTES = 64;
constexpr std::size_t MAX_ARCHIVE_NAME_BYTES = 128;
constexpr std::size_t MAX_STORED_FILENAME_BYTES = 255;

bool ValidateUsername(const std::string& name, std::string* error = nullptr);
bool ValidateArchiveName(const std::string& name, std::string* error = nullptr);
bool ValidateStoredFilename(const std::string& name, std::string* error = nullptr);

// Portable collision rule used even on case-sensitive filesystems.
bool NamesCollide(const std::string& left, const std::string& right);

// Resolve a relative path below baseDirectory after canonicalizing existing
// components. Absolute paths, traversal and symlink escapes are rejected.
bool ResolveContainedPath(const std::filesystem::path& baseDirectory,
                          const std::filesystem::path& relativePath,
                          std::filesystem::path& resolvedPath,
                          std::string* error = nullptr);

bool UserFilePath(const std::string& username,
                  std::filesystem::path& resolvedPath,
                  std::string* error = nullptr);
bool UserDatabasePath(const std::string& username,
                      std::filesystem::path& resolvedPath,
                      std::string* error = nullptr);
bool ArchiveFilePath(const std::string& username,
                     const std::string& archiveName,
                     std::filesystem::path& resolvedPath,
                     std::string* error = nullptr);

// outputPath may be an explicit filename or a directory. When it is a
// directory, storedFilename is appended only after containment validation.
bool ResolveExtractionPath(const std::filesystem::path& outputPath,
                           const std::string& storedFilename,
                           std::filesystem::path& resolvedPath,
                           std::string* error = nullptr);

} // namespace PathSecurity
