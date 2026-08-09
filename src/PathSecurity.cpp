#include "PathSecurity.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <system_error>
#include <unordered_set>

namespace PathSecurity {
namespace {

void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
}

bool DecodeUtf8(const std::string& value, std::size_t& offset, char32_t& codepoint) {
    const auto first = static_cast<unsigned char>(value[offset]);
    if (first <= 0x7fU) {
        codepoint = first;
        ++offset;
        return true;
    }

    std::size_t length = 0;
    char32_t minimum = 0;
    if (first >= 0xc2U && first <= 0xdfU) {
        length = 2;
        codepoint = first & 0x1fU;
        minimum = 0x80;
    } else if (first >= 0xe0U && first <= 0xefU) {
        length = 3;
        codepoint = first & 0x0fU;
        minimum = 0x800;
    } else if (first >= 0xf0U && first <= 0xf4U) {
        length = 4;
        codepoint = first & 0x07U;
        minimum = 0x10000;
    } else {
        return false;
    }

    if (offset + length > value.size()) {
        return false;
    }
    for (std::size_t i = 1; i < length; ++i) {
        const auto continuation = static_cast<unsigned char>(value[offset + i]);
        if ((continuation & 0xc0U) != 0x80U) {
            return false;
        }
        codepoint = (codepoint << 6U) | (continuation & 0x3fU);
    }
    offset += length;
    return codepoint >= minimum && codepoint <= 0x10ffffU &&
           !(codepoint >= 0xd800U && codepoint <= 0xdfffU);
}

bool IsProblematicUnicode(char32_t codepoint) {
    return (codepoint >= 0x80U && codepoint <= 0x9fU) ||
           (codepoint >= 0x300U && codepoint <= 0x36fU) ||
           (codepoint >= 0x1ab0U && codepoint <= 0x1affU) ||
           (codepoint >= 0x1dc0U && codepoint <= 0x1dffU) ||
           (codepoint >= 0x200bU && codepoint <= 0x200fU) ||
           codepoint == 0x2028U || codepoint == 0x2029U ||
           (codepoint >= 0x202aU && codepoint <= 0x202eU) ||
           (codepoint >= 0x2060U && codepoint <= 0x206fU) ||
           (codepoint >= 0x20d0U && codepoint <= 0x20ffU) ||
           codepoint == 0x2215U || codepoint == 0x29f8U ||
           codepoint == 0xfeffU || codepoint == 0xff0fU ||
           codepoint == 0xff3cU;
}

bool IsWindowsReservedName(const std::string& name) {
    std::string stem = name.substr(0, name.find('.'));
    std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });

    static const std::unordered_set<std::string> reserved = {
        "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5",
        "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4",
        "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };
    return reserved.find(stem) != reserved.end();
}

bool ValidateName(const std::string& name, std::size_t maximumBytes,
                  std::string* error) {
    if (name.empty()) {
        SetError(error, "Name must not be empty.");
        return false;
    }
    if (name.size() > maximumBytes) {
        SetError(error, "Name is too long.");
        return false;
    }
    if (name == "." || name.find("..") != std::string::npos) {
        SetError(error, "Name must not contain '.' traversal components.");
        return false;
    }
    if (name.front() == ' ' || name.back() == ' ' || name.back() == '.') {
        SetError(error, "Name must not start/end with spaces or end with a dot.");
        return false;
    }
    if (name.find_first_of("/\\:*?\"<>|") != std::string::npos) {
        SetError(error, "Name contains a path separator or a non-portable character.");
        return false;
    }

    std::size_t offset = 0;
    while (offset < name.size()) {
        char32_t codepoint = 0;
        if (!DecodeUtf8(name, offset, codepoint)) {
            SetError(error, "Name is not valid UTF-8.");
            return false;
        }
        if (codepoint < 0x20U || codepoint == 0x7fU ||
            IsProblematicUnicode(codepoint)) {
            SetError(error, "Name contains a control, invisible or ambiguous Unicode character.");
            return false;
        }
    }

    if (IsWindowsReservedName(name)) {
        SetError(error, "Name is reserved by the operating system.");
        return false;
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

std::filesystem::path CanonicalPath(const std::filesystem::path& path,
                                    std::error_code& error) {
    const std::filesystem::path absolute = std::filesystem::absolute(path, error);
    if (error) {
        return {};
    }
    return std::filesystem::weakly_canonical(absolute, error);
}

bool IsStrictlyWithin(const std::filesystem::path& base,
                      const std::filesystem::path& candidate) {
    auto basePart = base.begin();
    auto candidatePart = candidate.begin();
    for (; basePart != base.end() && candidatePart != candidate.end();
         ++basePart, ++candidatePart) {
        if (*basePart != *candidatePart) {
            return false;
        }
    }
    return basePart == base.end() && candidatePart != candidate.end();
}

} // namespace

bool ValidateUsername(const std::string& name, std::string* error) {
    return ValidateName(name, MAX_USERNAME_BYTES, error);
}

bool ValidateArchiveName(const std::string& name, std::string* error) {
    return ValidateName(name, MAX_ARCHIVE_NAME_BYTES, error);
}

bool ValidateStoredFilename(const std::string& name, std::string* error) {
    return ValidateName(name, MAX_STORED_FILENAME_BYTES, error);
}

bool NamesCollide(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        const auto lhs = static_cast<unsigned char>(left[i]);
        const auto rhs = static_cast<unsigned char>(right[i]);
        if (lhs < 0x80U && rhs < 0x80U) {
            if (std::tolower(lhs) != std::tolower(rhs)) {
                return false;
            }
        } else if (lhs != rhs) {
            return false;
        }
    }
    return true;
}

bool ResolveContainedPath(const std::filesystem::path& baseDirectory,
                          const std::filesystem::path& relativePath,
                          std::filesystem::path& resolvedPath,
                          std::string* error) {
    resolvedPath.clear();
    if (baseDirectory.empty() || relativePath.empty() || relativePath.is_absolute() ||
        relativePath.has_root_name() || relativePath.has_root_directory()) {
        SetError(error, "Path must be relative to its permitted directory.");
        return false;
    }
    for (const auto& component : relativePath) {
        if (component == ".." || component == "." || component.empty()) {
            SetError(error, "Path traversal is not permitted.");
            return false;
        }
    }

    std::error_code pathError;
    const auto baseStatus = std::filesystem::symlink_status(baseDirectory, pathError);
    const bool baseMissing =
        baseStatus.type() == std::filesystem::file_type::not_found ||
        pathError == std::make_error_code(std::errc::no_such_file_or_directory);
    if ((!baseMissing && pathError) || std::filesystem::is_symlink(baseStatus) ||
        (!baseMissing && std::filesystem::exists(baseStatus) &&
         !std::filesystem::is_directory(baseStatus))) {
        SetError(error, "Permitted directory is unavailable or is a symbolic link.");
        return false;
    }
    pathError.clear();
    const std::filesystem::path canonicalBase = CanonicalPath(baseDirectory, pathError);
    if (pathError || canonicalBase.empty()) {
        SetError(error, "Could not canonicalize the permitted directory.");
        return false;
    }
    const std::filesystem::path canonicalCandidate =
        CanonicalPath(canonicalBase / relativePath, pathError);
    if (pathError || canonicalCandidate.empty() ||
        !IsStrictlyWithin(canonicalBase, canonicalCandidate)) {
        SetError(error, "Resolved path escapes its permitted directory.");
        return false;
    }

    resolvedPath = canonicalCandidate;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool UserFilePath(const std::string& username,
                  std::filesystem::path& resolvedPath,
                  std::string* error) {
    return ValidateUsername(username, error) &&
           ResolveContainedPath("users", username + ".enc", resolvedPath, error);
}

bool UserDatabasePath(const std::string& username,
                      std::filesystem::path& resolvedPath,
                      std::string* error) {
    return ValidateUsername(username, error) &&
           ResolveContainedPath("users", username + "_database.pqc", resolvedPath, error);
}

bool ArchiveFilePath(const std::string& username,
                     const std::string& archiveName,
                     std::filesystem::path& resolvedPath,
                     std::string* error) {
    return ValidateUsername(username, error) && ValidateArchiveName(archiveName, error) &&
           ResolveContainedPath("archives", username + "_" + archiveName + ".enc",
                                resolvedPath, error);
}

bool ResolveExtractionPath(const std::filesystem::path& outputPath,
                           const std::string& storedFilename,
                           std::filesystem::path& resolvedPath,
                           std::string* error) {
    resolvedPath.clear();
    if (!ValidateStoredFilename(storedFilename, error) || outputPath.empty()) {
        if (outputPath.empty()) {
            SetError(error, "Extraction path must not be empty.");
        }
        return false;
    }

    std::error_code filesystemError;
    const bool isDirectory = std::filesystem::is_directory(outputPath, filesystemError);
    if (filesystemError) {
        SetError(error, "Could not inspect the extraction path.");
        return false;
    }
    const std::string rawPath = outputPath.string();
    const bool hasTrailingSeparator = !rawPath.empty() &&
        (rawPath.back() == '/' || rawPath.back() == '\\');
    if (isDirectory || hasTrailingSeparator) {
        return ResolveContainedPath(outputPath, storedFilename, resolvedPath, error);
    }

    if (outputPath.filename().empty()) {
        SetError(error, "Extraction destination must include a filename.");
        return false;
    }
    const std::filesystem::path parent = outputPath.parent_path().empty()
        ? std::filesystem::path(".") : outputPath.parent_path();
    return ResolveContainedPath(parent, outputPath.filename(), resolvedPath, error);
}

} // namespace PathSecurity
