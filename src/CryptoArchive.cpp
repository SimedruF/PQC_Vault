#include "CryptoArchive.h"
#include "AtomicFile.h"
#include "FormatValidation.h"
#include "PathSecurity.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <thread>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

constexpr std::array<uint8_t, 8> SECURE_ARCHIVE_MAGIC = {'P', 'Q', 'C', 'E', 'N', 'C', '0', '2'};
constexpr std::array<uint8_t, 8> LEGACY_ARCHIVE_MAGIC = {'P', 'Q', 'C', 'E', 'N', 'C', '0', '1'};
constexpr uint32_t ARCHIVE_FORMAT_VERSION = 2;
constexpr uint32_t KDF_SCRYPT = 1;
constexpr uint64_t SCRYPT_N = 32768;
constexpr uint32_t SCRYPT_R = 8;
constexpr uint32_t SCRYPT_P = 1;
constexpr uint64_t SCRYPT_MAX_MEMORY = 128ULL * 1024ULL * 1024ULL;
constexpr size_t KEY_SIZE = 32;
constexpr size_t SALT_SIZE = 32;
constexpr size_t NONCE_SIZE = 12;
constexpr size_t TAG_SIZE = 16;
constexpr size_t SECURE_FIXED_HEADER_SIZE = 52;
constexpr uint64_t MAX_ARCHIVE_CONTAINER_SIZE = 1024ULL * 1024ULL * 1024ULL;
constexpr uint64_t MAX_ARCHIVE_ENTRY_SIZE = 512ULL * 1024ULL * 1024ULL;
constexpr auto ARCHIVE_LOCK_TIMEOUT = std::chrono::seconds(5);

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

class ScopedArchiveLock {
public:
    explicit ScopedArchiveLock(const std::filesystem::path& archivePath) {
        if (archivePath.empty()) {
            return;
        }
        lockPath_ = archivePath;
        lockPath_ += ".lock";
        const auto deadline = std::chrono::steady_clock::now() + ARCHIVE_LOCK_TIMEOUT;
#ifdef _WIN32
        do {
            handle_ = CreateFileW(lockPath_.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                                  nullptr, OPEN_ALWAYS,
                                  FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
                                  nullptr);
            if (handle_ != INVALID_HANDLE_VALUE) {
                acquired_ = true;
                LARGE_INTEGER size{};
                if (!GetFileSizeEx(handle_, &size) ||
                    (size.QuadPart == 0 && !WriteMarkerWindows())) {
                    acquired_ = false;
                }
                break;
            }
            if (GetLastError() != ERROR_SHARING_VIOLATION &&
                GetLastError() != ERROR_LOCK_VIOLATION) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (std::chrono::steady_clock::now() < deadline);
#else
        descriptor_ = open(lockPath_.c_str(), O_RDWR | O_CREAT
#ifdef O_CLOEXEC
                           | O_CLOEXEC
#endif
#ifdef O_NOFOLLOW
                           | O_NOFOLLOW
#endif
                           , S_IRUSR | S_IWUSR);
        if (descriptor_ < 0) {
            return;
        }
        (void)fchmod(descriptor_, S_IRUSR | S_IWUSR);
        do {
            if (flock(descriptor_, LOCK_EX | LOCK_NB) == 0) {
                acquired_ = true;
                struct stat status{};
                if (fstat(descriptor_, &status) != 0 ||
                    (status.st_size == 0 && !WriteMarkerPosix())) {
                    acquired_ = false;
                }
                break;
            }
            if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (std::chrono::steady_clock::now() < deadline);
#endif
    }

    ScopedArchiveLock(const ScopedArchiveLock&) = delete;
    ScopedArchiveLock& operator=(const ScopedArchiveLock&) = delete;

    ~ScopedArchiveLock() {
#ifdef _WIN32
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
#else
        if (descriptor_ >= 0) {
            if (acquired_) {
                (void)flock(descriptor_, LOCK_UN);
            }
            close(descriptor_);
        }
#endif
    }

    bool acquired() const noexcept { return acquired_; }

private:
    static constexpr const char* LOCK_MARKER = "PQCLOCK1";
#ifdef _WIN32
    bool WriteMarkerWindows() {
        DWORD written = 0;
        LARGE_INTEGER start{};
        return SetFilePointerEx(handle_, start, nullptr, FILE_BEGIN) &&
               WriteFile(handle_, LOCK_MARKER, 8, &written, nullptr) && written == 8 &&
               FlushFileBuffers(handle_);
    }
#else
    bool WriteMarkerPosix() {
        size_t offset = 0;
        while (offset < 8) {
            const ssize_t written = pwrite(descriptor_, LOCK_MARKER + offset,
                                           8 - offset,
                                           static_cast<off_t>(offset));
            if (written > 0) {
                offset += static_cast<size_t>(written);
            } else if (written < 0 && errno == EINTR) {
                continue;
            } else {
                return false;
            }
        }
        return fsync(descriptor_) == 0;
    }
#endif

    std::filesystem::path lockPath_;
    bool acquired_ = false;
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int descriptor_ = -1;
#endif
};

bool FileRevision(const std::filesystem::path& path, bool& exists,
                  std::string& revision) {
    exists = false;
    revision.clear();
    std::error_code fileError;
    if (!std::filesystem::exists(path, fileError)) {
        return !fileError;
    }
    if (fileError || !std::filesystem::is_regular_file(path, fileError) || fileError) {
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> digest(
        EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!digest || EVP_DigestInit_ex(digest.get(), EVP_sha256(), nullptr) != 1) {
        return false;
    }

    std::array<char, 64 * 1024> buffer{};
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = file.gcount();
        if (count > 0 &&
            EVP_DigestUpdate(digest.get(), buffer.data(), static_cast<size_t>(count)) != 1) {
            return false;
        }
    }
    if (!file.eof()) {
        return false;
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> hash{};
    unsigned int hashSize = 0;
    if (EVP_DigestFinal_ex(digest.get(), hash.data(), &hashSize) != 1) {
        return false;
    }
    std::ostringstream encoded;
    for (unsigned int i = 0; i < hashSize; ++i) {
        encoded << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<unsigned int>(hash[i]);
    }
    revision = encoded.str();
    exists = true;
    return true;
}

void AppendUint32(std::vector<uint8_t>& output, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        output.push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
}

void AppendUint64(std::vector<uint8_t>& output, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output.push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
}

bool ReadUint32(const std::vector<uint8_t>& input, size_t& offset, uint32_t& value) {
    if (offset > input.size() || input.size() - offset < sizeof(uint32_t)) {
        return false;
    }

    value = 0;
    for (size_t i = 0; i < sizeof(uint32_t); ++i) {
        value = (value << 8U) | input[offset++];
    }
    return true;
}

bool ReadUint64(const std::vector<uint8_t>& input, size_t& offset, uint64_t& value) {
    if (offset > input.size() || input.size() - offset < sizeof(uint64_t)) {
        return false;
    }

    value = 0;
    for (size_t i = 0; i < sizeof(uint64_t); ++i) {
        value = (value << 8U) | input[offset++];
    }
    return true;
}

void Cleanse(std::vector<uint8_t>& data) {
    if (!data.empty()) {
        OPENSSL_cleanse(data.data(), data.size());
    }
}

std::string FormatFileModificationTime(const std::filesystem::path& path) {
    std::error_code error;
    const auto fileTime = std::filesystem::last_write_time(path, error);
    if (error) {
        return "Unavailable";
    }

    const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        fileTime - std::filesystem::file_time_type::clock::now() +
        std::chrono::system_clock::now());
    const std::time_t value = std::chrono::system_clock::to_time_t(systemTime);
    std::tm localTime{};
#ifdef _WIN32
    if (localtime_s(&localTime, &value) != 0) {
        return "Unavailable";
    }
#else
    if (localtime_r(&value, &localTime) == nullptr) {
        return "Unavailable";
    }
#endif

    std::ostringstream formatted;
    formatted << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return formatted.str();
}

bool DeriveScryptKey(const std::string& password,
                     const std::vector<uint8_t>& salt,
                     uint64_t n,
                     uint64_t r,
                     uint64_t p,
                     std::vector<uint8_t>& key) {
    if (password.empty() || salt.size() != SALT_SIZE || n != SCRYPT_N ||
        r != SCRYPT_R || p != SCRYPT_P) {
        return false;
    }

    key.assign(KEY_SIZE, 0);
    if (EVP_PBE_scrypt(password.data(), password.size(), salt.data(), salt.size(),
                       n, r, p, SCRYPT_MAX_MEMORY, key.data(), key.size()) != 1) {
        Cleanse(key);
        key.clear();
        return false;
    }
    return true;
}

std::vector<uint8_t> DeriveLegacyKey(const std::string& password) {
    std::vector<uint8_t> key(KEY_SIZE);
    unsigned int digestLength = 0;
    if (EVP_Digest(password.data(), password.size(), key.data(), &digestLength,
                   EVP_sha256(), nullptr) != 1 || digestLength != KEY_SIZE) {
        Cleanse(key);
        return {};
    }
    return key;
}

bool EncryptAesGcm(const std::vector<uint8_t>& plaintext,
                   const std::vector<uint8_t>& key,
                   const std::vector<uint8_t>& nonce,
                   const std::vector<uint8_t>& aad,
                   std::vector<uint8_t>& ciphertext,
                   std::vector<uint8_t>& tag) {
    if (key.size() != KEY_SIZE || nonce.size() != NONCE_SIZE ||
        plaintext.size() > static_cast<size_t>(std::numeric_limits<int>::max()) ||
        aad.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    CipherContext context(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!context ||
        EVP_EncryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(nonce.size()), nullptr) != 1 ||
        EVP_EncryptInit_ex(context.get(), nullptr, nullptr, key.data(), nonce.data()) != 1) {
        return false;
    }

    int outputLength = 0;
    if (!aad.empty() &&
        EVP_EncryptUpdate(context.get(), nullptr, &outputLength, aad.data(),
                          static_cast<int>(aad.size())) != 1) {
        return false;
    }

    ciphertext.assign(plaintext.size() + TAG_SIZE, 0);
    int ciphertextLength = 0;
    if (EVP_EncryptUpdate(context.get(), ciphertext.data(), &outputLength,
                          plaintext.data(), static_cast<int>(plaintext.size())) != 1) {
        return false;
    }
    ciphertextLength = outputLength;

    if (EVP_EncryptFinal_ex(context.get(), ciphertext.data() + ciphertextLength,
                            &outputLength) != 1) {
        return false;
    }
    ciphertextLength += outputLength;
    ciphertext.resize(static_cast<size_t>(ciphertextLength));

    tag.assign(TAG_SIZE, 0);
    return EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_GET_TAG,
                               static_cast<int>(tag.size()), tag.data()) == 1;
}

bool DecryptAesGcm(const std::vector<uint8_t>& ciphertext,
                   const std::vector<uint8_t>& key,
                   const std::vector<uint8_t>& nonce,
                   const std::vector<uint8_t>& aad,
                   const std::vector<uint8_t>& tag,
                   std::vector<uint8_t>& plaintext) {
    if (key.size() != KEY_SIZE || nonce.size() != NONCE_SIZE || tag.size() != TAG_SIZE ||
        ciphertext.size() > static_cast<size_t>(std::numeric_limits<int>::max()) ||
        aad.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    CipherContext context(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!context ||
        EVP_DecryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(nonce.size()), nullptr) != 1 ||
        EVP_DecryptInit_ex(context.get(), nullptr, nullptr, key.data(), nonce.data()) != 1) {
        return false;
    }

    int outputLength = 0;
    if (!aad.empty() &&
        EVP_DecryptUpdate(context.get(), nullptr, &outputLength, aad.data(),
                          static_cast<int>(aad.size())) != 1) {
        return false;
    }

    plaintext.assign(ciphertext.size() + TAG_SIZE, 0);
    int plaintextLength = 0;
    if (EVP_DecryptUpdate(context.get(), plaintext.data(), &outputLength,
                          ciphertext.data(), static_cast<int>(ciphertext.size())) != 1) {
        Cleanse(plaintext);
        plaintext.clear();
        return false;
    }
    plaintextLength = outputLength;

    std::vector<uint8_t> mutableTag = tag;
    if (EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_TAG,
                            static_cast<int>(mutableTag.size()), mutableTag.data()) != 1 ||
        EVP_DecryptFinal_ex(context.get(), plaintext.data() + plaintextLength,
                            &outputLength) != 1) {
        Cleanse(mutableTag);
        Cleanse(plaintext);
        plaintext.clear();
        return false;
    }

    Cleanse(mutableTag);
    plaintextLength += outputLength;
    plaintext.resize(static_cast<size_t>(plaintextLength));
    return true;
}

std::vector<uint8_t> BuildSecureHeader(uint64_t ciphertextSize,
                                       const std::vector<uint8_t>& salt,
                                       const std::vector<uint8_t>& nonce) {
    std::vector<uint8_t> header;
    header.reserve(SECURE_FIXED_HEADER_SIZE + salt.size() + nonce.size());
    header.insert(header.end(), SECURE_ARCHIVE_MAGIC.begin(), SECURE_ARCHIVE_MAGIC.end());
    AppendUint32(header, ARCHIVE_FORMAT_VERSION);
    AppendUint32(header, KDF_SCRYPT);
    AppendUint64(header, SCRYPT_N);
    AppendUint32(header, SCRYPT_R);
    AppendUint32(header, SCRYPT_P);
    AppendUint32(header, static_cast<uint32_t>(salt.size()));
    AppendUint32(header, static_cast<uint32_t>(nonce.size()));
    AppendUint32(header, static_cast<uint32_t>(TAG_SIZE));
    AppendUint64(header, ciphertextSize);
    header.insert(header.end(), salt.begin(), salt.end());
    header.insert(header.end(), nonce.begin(), nonce.end());
    return header;
}

bool DecryptSecureArchiveBytes(const std::vector<uint8_t>& archiveData,
                               const std::string& password,
                               std::vector<uint8_t>& plaintext) {
    if (password.empty() ||
        archiveData.size() < SECURE_FIXED_HEADER_SIZE + SALT_SIZE + NONCE_SIZE + TAG_SIZE ||
        !std::equal(SECURE_ARCHIVE_MAGIC.begin(), SECURE_ARCHIVE_MAGIC.end(),
                    archiveData.begin())) {
        return false;
    }

    size_t offset = SECURE_ARCHIVE_MAGIC.size();
    uint32_t version = 0;
    uint32_t kdf = 0;
    uint64_t n = 0;
    uint32_t r = 0;
    uint32_t p = 0;
    uint32_t saltSize = 0;
    uint32_t nonceSize = 0;
    uint32_t tagSize = 0;
    uint64_t ciphertextSize = 0;
    if (!ReadUint32(archiveData, offset, version) ||
        !ReadUint32(archiveData, offset, kdf) ||
        !ReadUint64(archiveData, offset, n) ||
        !ReadUint32(archiveData, offset, r) ||
        !ReadUint32(archiveData, offset, p) ||
        !ReadUint32(archiveData, offset, saltSize) ||
        !ReadUint32(archiveData, offset, nonceSize) ||
        !ReadUint32(archiveData, offset, tagSize) ||
        !ReadUint64(archiveData, offset, ciphertextSize)) {
        return false;
    }

    if (version != ARCHIVE_FORMAT_VERSION || kdf != KDF_SCRYPT ||
        n != SCRYPT_N || r != SCRYPT_R || p != SCRYPT_P ||
        saltSize != SALT_SIZE || nonceSize != NONCE_SIZE || tagSize != TAG_SIZE ||
        ciphertextSize == 0 ||
        ciphertextSize > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    const uint64_t variableSize = static_cast<uint64_t>(saltSize) + nonceSize +
                                  ciphertextSize + tagSize;
    if (variableSize > archiveData.size() - offset ||
        offset + static_cast<size_t>(variableSize) != archiveData.size()) {
        return false;
    }

    std::vector<uint8_t> salt(archiveData.begin() + static_cast<std::ptrdiff_t>(offset),
                              archiveData.begin() + static_cast<std::ptrdiff_t>(offset + saltSize));
    offset += saltSize;
    std::vector<uint8_t> nonce(archiveData.begin() + static_cast<std::ptrdiff_t>(offset),
                               archiveData.begin() + static_cast<std::ptrdiff_t>(offset + nonceSize));
    offset += nonceSize;
    const size_t headerSize = offset;
    std::vector<uint8_t> ciphertext(
        archiveData.begin() + static_cast<std::ptrdiff_t>(offset),
        archiveData.begin() + static_cast<std::ptrdiff_t>(offset + ciphertextSize));
    offset += static_cast<size_t>(ciphertextSize);
    std::vector<uint8_t> tag(archiveData.begin() + static_cast<std::ptrdiff_t>(offset),
                             archiveData.end());
    std::vector<uint8_t> header(archiveData.begin(),
                                archiveData.begin() + static_cast<std::ptrdiff_t>(headerSize));

    std::vector<uint8_t> key;
    SecureMemory::ScopedCleanse keyGuard(key);
    if (!DeriveScryptKey(password, salt, n, r, p, key)) {
        return false;
    }
    const bool authenticated =
        DecryptAesGcm(ciphertext, key, nonce, header, tag, plaintext);
    Cleanse(key);
    return authenticated;
}

} // namespace

CryptoArchive::CryptoArchive(const std::string& username, const std::string& archiveName) 
    : m_username(username), m_archiveName(archiveName), m_identityValid(false),
      m_hasDiskRevision(false), m_isLoaded(false) {
    m_archivePath = GetArchiveFilePath();
    m_identityValid = !m_archivePath.empty();
    // Ensure the archives directory exists
    if (m_identityValid) {
        std::filesystem::create_directories("archives");
    }
}

CryptoArchive::~CryptoArchive() {
    ClearDecryptedData();
    m_password.clear();
}

bool CryptoArchive::InitializeArchive(const std::string& password) {
    if (!m_identityValid || password.empty()) {
        std::cerr << "Cannot initialize an archive with an empty password" << std::endl;
        return false;
    }

    if (ArchiveExists()) {
        std::cout << "Archive already exists for user: " << m_username << std::endl;
        return LoadArchive(password);
    }
    
    // Initialize empty archive
    ClearDecryptedData();
    m_isLoaded = true;
    if (!m_password.assign(password)) {
        m_isLoaded = false;
        return false;
    }
    
    std::cout << "Initialized new archive for user: " << m_username << std::endl;
    const bool saved = SaveArchive();
    if (!saved) {
        ClearDecryptedData();
        m_password.clear();
        m_isLoaded = false;
    }
    return saved;
}

bool CryptoArchive::LoadArchive(const std::string& password) {
    std::cout << "\n---------- LOAD ARCHIVE ----------" << std::endl;
    std::cout << "Loading archive for user: " << m_username << std::endl;
    std::cout << "Archive path: " << m_archivePath << std::endl;
    
    if (!m_identityValid) {
        return false;
    }
    ScopedArchiveLock archiveLock(m_archivePath);
    if (!archiveLock.acquired()) {
        std::cerr << "Could not acquire archive lock" << std::endl;
        return false;
    }
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
        std::string loadedRevision;
        std::vector<uint8_t> decryptedData =
            DecryptArchiveData(password, nullptr, &loadedRevision);
        SecureMemory::ScopedCleanse decryptedDataGuard(decryptedData);
        if (decryptedData.empty()) {
            std::cout << "Failed to decrypt archive for user: " << m_username << std::endl;
            std::cout << "---------------------------------\n" << std::endl;
            return false;
        }
        
        std::cout << "Decrypted data size: " << decryptedData.size() << " bytes" << std::endl;
        
        // Resetează starea arhivei înainte de a încerca deserializarea
        ClearDecryptedData();
        m_isLoaded = false;
        
        std::cout << "Deserializing archive data..." << std::endl;
        if (!DeserializeArchive(decryptedData)) {
            std::cout << "Failed to deserialize archive for user: " << m_username << std::endl;
            std::cout << "---------------------------------\n" << std::endl;
            return false;
        }

        if (!m_password.assign(password)) {
            ClearDecryptedData();
            return false;
        }
        
        // Verifică că există cel puțin un fișier în arhivă sau că este o arhivă nouă validă
        std::cout << "Files in archive after loading: " << m_files.size() << std::endl;
        
        // Verifică dacă fișierele încărcate au date valide
        bool allFilesValid = true;
        for (const auto& file : m_files) {
            if (file.second.data.size() != file.second.size) {
                std::cout << "WARNING: File '" << file.first << "' has size mismatch! " 
                          << "Reported: " << file.second.size << ", Actual: " << file.second.data.size() << std::endl;
                allFilesValid = false;
            }
        }
        
        if (!allFilesValid) {
            std::cout << "Some files in the archive have invalid data!" << std::endl;
        }
        
        // Setăm arhiva ca încărcată
        m_diskRevision = std::move(loadedRevision);
        m_hasDiskRevision = true;
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
    if (!m_identityValid || !m_isLoaded) {
        std::cerr << "Cannot save an archive that is not loaded" << std::endl;
        return false;
    }

    try {
        ScopedArchiveLock archiveLock(m_archivePath);
        if (!archiveLock.acquired()) {
            std::cerr << "Could not acquire archive lock" << std::endl;
            return false;
        }

        bool diskExists = false;
        std::string currentRevision;
        if (!FileRevision(m_archivePath, diskExists, currentRevision) ||
            (m_hasDiskRevision && (!diskExists || currentRevision != m_diskRevision)) ||
            (!m_hasDiskRevision && diskExists)) {
            std::cerr << "Archive changed on disk; reload before saving" << std::endl;
            return false;
        }

        std::vector<uint8_t> encryptedArchive;
        if (!BuildEncryptedArchive(m_password.get(), encryptedArchive)) {
            return false;
        }
        const std::string newRevision = CalculateFileHash(encryptedArchive);
        if (newRevision.empty()) {
            return false;
        }
        if (!AtomicFile::Write(m_archivePath, encryptedArchive)) {
            std::cerr << "Failed to atomically write archive: " << m_archivePath << std::endl;
            return false;
        }

        m_diskRevision = newRevision;
        m_hasDiskRevision = true;

        std::cout << "Archive saved as PQCENC02 (scrypt + AES-256-GCM): "
                  << m_archivePath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving archive: " << e.what() << std::endl;
        return false;
    }
}

bool CryptoArchive::BuildEncryptedArchive(const std::string& password,
                                          std::vector<uint8_t>& output) const {
    if (password.empty()) {
        std::cerr << "Cannot encrypt an archive without a password" << std::endl;
        return false;
    }
    for (const auto& file : m_files) {
        if (!PathSecurity::ValidateStoredFilename(file.first) ||
            file.first != file.second.name ||
            file.second.size != file.second.data.size()) {
            std::cerr << "Cannot save archive: size mismatch for " << file.first << std::endl;
            return false;
        }
    }

    std::vector<uint8_t> serializedData = SerializeArchive();
    SecureMemory::ScopedCleanse serializedDataGuard(serializedData);
    if (serializedData.empty()) {
        return false;
    }

    std::vector<uint8_t> salt(SALT_SIZE);
    std::vector<uint8_t> nonce(NONCE_SIZE);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1 ||
        RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) != 1) {
        return false;
    }

    std::vector<uint8_t> key;
    SecureMemory::ScopedCleanse keyGuard(key);
    if (!DeriveScryptKey(password, salt, SCRYPT_N, SCRYPT_R, SCRYPT_P, key)) {
        return false;
    }

    std::vector<uint8_t> header = BuildSecureHeader(serializedData.size(), salt, nonce);
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> tag;
    const bool encrypted = EncryptAesGcm(serializedData, key, nonce, header, ciphertext, tag);
    Cleanse(key);
    Cleanse(serializedData);
    if (!encrypted || ciphertext.empty() || tag.size() != TAG_SIZE) {
        return false;
    }

    output.clear();
    output.reserve(header.size() + ciphertext.size() + tag.size());
    output.insert(output.end(), header.begin(), header.end());
    output.insert(output.end(), ciphertext.begin(), ciphertext.end());
    output.insert(output.end(), tag.begin(), tag.end());
    return true;
}

bool CryptoArchive::AddFile(const std::string& filePath, const std::string& name) {
    std::cout << "---------- CRYPTO ARCHIVE ADD FILE ----------" << std::endl;
    std::cout << "AddFile called with path: '" << filePath << "'" << std::endl;
    std::cout << "Display name: '" << name << "'" << std::endl;
    std::cout << "Archive loaded state: " << (m_isLoaded ? "Yes" : "No") << std::endl;
    
    const std::string entryName =
        name.empty() ? std::filesystem::path(filePath).filename().string() : name;
    std::string validationError;
    if (!m_identityValid || !m_isLoaded ||
        !PathSecurity::ValidateStoredFilename(entryName, &validationError)) {
        std::cout << "Invalid archive filename: " << validationError << std::endl;
        std::cout << "Archive not loaded, returning false" << std::endl;
        std::cout << "-------------------------------------------" << std::endl;
        return false;
    }
    
    // Print diagnostics about the archive state before adding
    std::cout << "Current archive state:" << std::endl;
    std::cout << "Files in archive: " << m_files.size() << std::endl;
    
    try {
        for (const auto& existing : m_files) {
            if (existing.first != entryName &&
                PathSecurity::NamesCollide(existing.first, entryName)) {
                std::cout << "A file with an equivalent name already exists" << std::endl;
                return false;
            }
        }

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
        
        std::error_code sizeError;
        const uintmax_t rawFileSize = std::filesystem::file_size(filePath, sizeError);
        if (sizeError || rawFileSize > MAX_ARCHIVE_ENTRY_SIZE ||
            rawFileSize > static_cast<uintmax_t>(std::numeric_limits<size_t>::max()) ||
            rawFileSize > static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
            std::cout << "File exceeds the maximum archive entry size" << std::endl;
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
        
        const size_t fileSize = static_cast<size_t>(rawFileSize);
        
        std::cout << "File size: " << fileSize << " bytes" << std::endl;
        
        // Read file content
        std::vector<uint8_t> fileData(fileSize);
        SecureMemory::ScopedCleanse fileDataGuard(fileData);
        if (fileSize > 0) {
            file.read(reinterpret_cast<char*>(fileData.data()),
                      static_cast<std::streamsize>(fileSize));
        }
        if (!file || file.gcount() != static_cast<std::streamsize>(fileSize)) {
            std::cout << "Failed to read entire file. Only read " << file.gcount() << " bytes" << std::endl;
            file.close();
            std::cout << "-------------------------------------------" << std::endl;
            return false;
        }
        file.close();
        
        std::cout << "File read successfully" << std::endl;
        
        // Create file entry
        FileEntry entry;
        entry.name = entryName;
        entry.path = filePath;
        entry.size = fileSize;
        entry.timestamp = GetCurrentTimestamp();
        entry.hash = CalculateFileHash(fileData);
        entry.data = std::move(fileData);
        std::cout << "Entry created with name: " << entry.name << std::endl;
        
        // Keep a replaced entry until the new archive has been committed so a
        // failed write can restore the exact in-memory state as well.
        auto previousEntry = m_files.extract(entryName);
        m_files[entryName] = std::move(entry);
        
        std::cout << "Added file to archive: " << entryName << " (" << fileSize << " bytes)" << std::endl;
        std::cout << "Files in archive after adding: " << m_files.size() << std::endl;
        
        // Verify addition was successful
        auto verifyIt = m_files.find(entryName);
        if (verifyIt != m_files.end()) {
            std::cout << "Verified: File '" << entryName << "' exists in archive" << std::endl;
            std::cout << "Stored data size: " << verifyIt->second.data.size() << " bytes" << std::endl;
        } else {
            std::cout << "WARNING: Failed to verify file was added properly!" << std::endl;
        }
        
        // Save the archive to ensure the file is persisted
        std::cout << "Saving archive after adding file..." << std::endl;
        bool saveResult = SaveArchive();
        if (!saveResult) {
            std::cout << "WARNING: Failed to save archive after adding file!" << std::endl;
            auto failedEntry = m_files.extract(entryName);
            if (!failedEntry.empty()) {
                SecureMemory::Cleanse(failedEntry.mapped().data);
            }
            if (!previousEntry.empty()) {
                m_files.insert(std::move(previousEntry));
            }
        } else if (!previousEntry.empty()) {
            SecureMemory::Cleanse(previousEntry.mapped().data);
        }
        
        std::cout << "-------------------------------------------" << std::endl;
        return saveResult;
    } catch (const std::exception& e) {
        std::cout << "Error adding file to archive: " << e.what() << std::endl;
        std::cout << "-------------------------------------------" << std::endl;
        return false;
    }
}

bool CryptoArchive::ExtractFile(const std::string& name, const std::string& outputPath) {
    std::cout << "\n---------- EXTRACT FILE ----------" << std::endl;
    std::cout << "Extracting file: '" << name << "' to path: '" << outputPath << "'" << std::endl;
    
    if (!m_identityValid || !m_isLoaded ||
        !PathSecurity::ValidateStoredFilename(name) || outputPath.empty()) {
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
        for (const auto& file : m_files) {
            if (PathSecurity::NamesCollide(file.first, name)) {
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
        std::filesystem::path finalPath;
        std::string validationError;
        if (!PathSecurity::ResolveExtractionPath(outputPath, foundEntry->name,
                                                 finalPath, &validationError)) {
            std::cout << "Unsafe extraction path: " << validationError << std::endl;
            return false;
        }
        
        // Create parent directories if they don't exist
        std::filesystem::path parentPath = finalPath.parent_path();
        if (!parentPath.empty()) {
            std::cout << "Creating parent directories: " << parentPath << std::endl;
            try {
                std::filesystem::create_directories(parentPath);
            } catch (const std::exception& e) {
                std::cout << "Failed to create directories: " << e.what() << std::endl;
            }
        }
        
        std::cout << "Writing " << foundEntry->data.size() << " bytes to file..." << std::endl;
        if (!AtomicFile::Write(finalPath, foundEntry->data)) {
            std::cout << "ERROR: Failed to atomically write extracted file!" << std::endl;
            std::cout << "----------------------------------\n" << std::endl;
            return false;
        }
        
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
    
    if (!m_identityValid || !m_isLoaded ||
        !PathSecurity::ValidateStoredFilename(name)) {
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
        for (const auto& file : m_files) {
            if (PathSecurity::NamesCollide(file.first, name)) {
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
    
    try {
        // Copiază datele fișierului în buffer-ul de ieșire
        outData = foundEntry->data;
        
        std::cout << "Data copied to output buffer, size: " << outData.size() << " bytes" << std::endl;
        
        std::cout << "------------------------------------------\n" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception during data copy: " << e.what() << std::endl;
        std::cout << "------------------------------------------\n" << std::endl;
        return false;
    }
}

bool CryptoArchive::RemoveFile(const std::string& name) {
    if (!m_identityValid || !m_isLoaded ||
        !PathSecurity::ValidateStoredFilename(name)) {
        return false;
    }
    
    auto it = m_files.find(name);
    if (it == m_files.end()) {
        return false;
    }
    
    auto removedEntry = m_files.extract(it);
    if (!SaveArchive()) {
        m_files.insert(std::move(removedEntry));
        std::cerr << "Failed to persist removal; archive state was restored" << std::endl;
        return false;
    }

    SecureMemory::Cleanse(removedEntry.mapped().data);
    std::cout << "Removed file from archive: " << name << std::endl;
    return true;
}

std::vector<FileEntry> CryptoArchive::GetFileList() const {
    std::vector<FileEntry> fileList;
    for (const auto& pair : m_files) {
        FileEntry metadata;
        metadata.name = pair.second.name;
        metadata.path = pair.second.path;
        metadata.size = pair.second.size;
        metadata.timestamp = pair.second.timestamp;
        metadata.hash = pair.second.hash;
        fileList.push_back(std::move(metadata));
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
    return m_identityValid && std::filesystem::is_regular_file(m_archivePath);
}

CryptoArchive::ArchiveStats CryptoArchive::GetStats() const {
    ArchiveStats stats;
    stats.totalFiles = m_files.size();
    stats.totalSize = 0;
    stats.lastModified = m_identityValid
        ? FormatFileModificationTime(m_archivePath)
        : "Unavailable";
    
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

std::vector<uint8_t> CryptoArchive::DecryptArchiveData(const std::string& password,
                                                       bool* legacyFormat,
                                                       std::string* diskRevision) const {
    if (legacyFormat) {
        *legacyFormat = false;
    }
    if (diskRevision) {
        diskRevision->clear();
    }

    if (password.empty()) {
        std::cerr << "Archive password cannot be empty" << std::endl;
        return {};
    }

    try {
        std::ifstream file(m_archivePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Could not open archive for reading" << std::endl;
            return {};
        }

        file.seekg(0, std::ios::end);
        const std::streamoff endPosition = file.tellg();
        if (endPosition <= 0 ||
            static_cast<uint64_t>(endPosition) > MAX_ARCHIVE_CONTAINER_SIZE) {
            std::cerr << "Archive is empty or unreadable" << std::endl;
            return {};
        }
        file.seekg(0, std::ios::beg);

        const size_t fileSize = static_cast<size_t>(endPosition);
        std::vector<uint8_t> archiveData(fileSize);
        file.read(reinterpret_cast<char*>(archiveData.data()),
                  static_cast<std::streamsize>(archiveData.size()));
        if (!file || file.gcount() != static_cast<std::streamsize>(archiveData.size())) {
            std::cerr << "Failed to read complete archive" << std::endl;
            return {};
        }
        file.close();

        if (!FormatValidation::ValidateArchiveFile(archiveData.data(), archiveData.size())) {
            std::cerr << "Invalid or unsupported archive container" << std::endl;
            return {};
        }

        if (diskRevision) {
            *diskRevision = CalculateFileHash(archiveData);
            if (diskRevision->empty()) {
                return {};
            }
        }

        if (archiveData.size() >= SECURE_ARCHIVE_MAGIC.size() &&
            std::equal(SECURE_ARCHIVE_MAGIC.begin(), SECURE_ARCHIVE_MAGIC.end(),
                       archiveData.begin())) {
            std::vector<uint8_t> plaintext;
            if (!DecryptSecureArchiveBytes(archiveData, password, plaintext)) {
                std::cerr << "Archive authentication failed: wrong password or modified data" << std::endl;
                return {};
            }
            return plaintext;
        }

        if (archiveData.size() >= LEGACY_ARCHIVE_MAGIC.size() &&
            std::equal(LEGACY_ARCHIVE_MAGIC.begin(), LEGACY_ARCHIVE_MAGIC.end(),
                       archiveData.begin())) {
            if (archiveData.size() < 16) {
                return {};
            }

            uint64_t dataSize = 0;
            std::memcpy(&dataSize, archiveData.data() + LEGACY_ARCHIVE_MAGIC.size(),
                        sizeof(dataSize));
            if (dataSize == 0 || dataSize != archiveData.size() - 16) {
                std::cerr << "Invalid legacy archive size" << std::endl;
                return {};
            }

            std::vector<uint8_t> key = DeriveLegacyKey(password);
            SecureMemory::ScopedCleanse keyGuard(key);
            if (key.empty()) {
                return {};
            }

            std::vector<uint8_t> plaintext(static_cast<size_t>(dataSize));
            for (size_t i = 0; i < plaintext.size(); ++i) {
                plaintext[i] = archiveData[16 + i] ^ key[i % key.size()];
            }
            Cleanse(key);
            if (legacyFormat) {
                *legacyFormat = true;
            }
            std::cout << "Loaded legacy PQCENC01 archive; the next save will migrate it to PQCENC02"
                      << std::endl;
            return plaintext;
        }

        std::cerr << "Unknown archive format; refusing to treat it as plaintext" << std::endl;
        return {};
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
        ClearDecryptedData();
        const bool parsed = [&]() -> bool {
        if (data.size() < 4) {
            std::cerr << "Data size too small for deserialization: " << data.size() << " bytes" << std::endl;
            return false;
        }
        
        std::cout << "Deserializing data of size: " << data.size() << " bytes" << std::endl;
        
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

            if (!PathSecurity::ValidateStoredFilename(name)) {
                std::cerr << "Unsafe filename in encrypted archive" << std::endl;
                return false;
            }
            for (const auto& existing : m_files) {
                if (PathSecurity::NamesCollide(existing.first, name)) {
                    std::cerr << "Colliding filenames in encrypted archive" << std::endl;
                    return false;
                }
            }
            
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
            if (fileSize > MAX_ARCHIVE_ENTRY_SIZE || fileSize > data.size()) {
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
            if (fileSize > 0) {
                std::memcpy(fileData.data(), data.data() + offset,
                            static_cast<size_t>(fileSize));
            }
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
            entry.data = std::move(fileData);
            entry.size = fileSize;
            entry.timestamp = timestamp;
            entry.hash = hash;
            
            m_files[name] = std::move(entry);
        }

        if (offset != data.size()) {
            std::cerr << "Unexpected trailing data in serialized archive" << std::endl;
            return false;
        }
        return true;
        }();
        if (!parsed) {
            ClearDecryptedData();
        }
        return parsed;
    } catch (const std::exception& e) {
        ClearDecryptedData();
        std::cout << "Error deserializing archive: " << e.what() << std::endl;
        return false;
    }
}

std::string CryptoArchive::CalculateFileHash(const std::vector<uint8_t>& data) const {
    std::array<unsigned char, 32> hash{};
    unsigned int hashLength = 0;
    if (EVP_Digest(data.data(), data.size(), hash.data(), &hashLength,
                   EVP_sha256(), nullptr) != 1 || hashLength != hash.size()) {
        return {};
    }
    
    std::stringstream ss;
    for (size_t i = 0; i < hash.size(); i++) {
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
    std::filesystem::path path;
    return PathSecurity::ArchiveFilePath(m_username, m_archiveName, path)
        ? path.string() : std::string{};
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
        
        if (entry.size != 0 && entry.data.empty()) {
            std::cout << "  WARNING: File data is empty!" << std::endl;
        }
    }
    
    std::cout << "\n=========================================\n" << std::endl;
}

bool CryptoArchive::ResetArchive(const std::string& password) {
    std::cout << "\n---------- RESET ARCHIVE ----------" << std::endl;
    std::cout << "Resetting archive for user: " << m_username << std::endl;
    
    if (password.empty()) {
        return false;
    }

    // Keep the current in-memory state until the empty replacement has been
    // durably committed. SaveArchive itself performs the atomic replacement.
    std::map<std::string, FileEntry> previousFiles;
    previousFiles.swap(m_files);
    SecureMemory::SecureString previousPassword(m_password.get());
    const bool previousLoadedState = m_isLoaded;

    m_files.clear();
    m_isLoaded = true;
    if (!m_password.assign(password)) {
        m_password.assign(previousPassword.get());
        m_files.swap(previousFiles);
        m_isLoaded = previousLoadedState;
        return false;
    }

    std::cout << "Creating new empty archive atomically..." << std::endl;
    const bool success = SaveArchive();
    if (!success) {
        m_password.assign(previousPassword.get());
        m_files.swap(previousFiles);
        m_isLoaded = previousLoadedState;
    } else {
        for (auto& [name, entry] : previousFiles) {
            (void)name;
            SecureMemory::Cleanse(entry.data);
        }
        previousFiles.clear();
    }
    
    if (success) {
        std::cout << "Archive successfully reset and reinitialized!" << std::endl;
    } else {
        std::cout << "Failed to reset and reinitialize archive!" << std::endl;
    }
    
    std::cout << "---------------------------------\n" << std::endl;
    return success;
}

bool CryptoArchive::ResetArchive() {
    if (m_password.empty()) {
        return false;
    }
    return ResetArchive(m_password.get());
}

bool CryptoArchive::RepairArchive() {
    std::cout << "\n---------- REPAIR ARCHIVE ----------" << std::endl;
    std::cout << "Attempting to repair archive for user: " << m_username << std::endl;
    
    if (!m_isLoaded) {
        std::cout << "Cannot repair - archive not loaded!" << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return false;
    }
    
    struct MetadataUndo {
        std::string key;
        size_t size;
        std::string hash;
    };
    int issuesFixed = 0;
    std::vector<MetadataUndo> metadataUndo;
    std::vector<std::string> keysToRemove;
    std::vector<std::map<std::string, FileEntry>::node_type> removedEntries;
    
    std::cout << "Scanning for issues in " << m_files.size() << " files..." << std::endl;
    
    // First pass - identify issues
    for (auto& pair : m_files) {
        const std::string& name = pair.first;
        FileEntry& entry = pair.second;
        bool hasIssues = false;
        
        if (!PathSecurity::ValidateStoredFilename(name) || entry.name != name) {
            std::cout << "ERROR: Found file with invalid name - marking for removal" << std::endl;
            keysToRemove.push_back(name);
            continue;
        }

        const size_t previousSize = entry.size;
        const std::string previousHash = entry.hash;
        
        // Check size/data mismatch
        if (entry.size != entry.data.size()) {
            std::cout << "ISSUE: File '" << name << "' has size mismatch. "
                      << "Reported: " << entry.size << ", Actual: " << entry.data.size() << " bytes" << std::endl;
            // Fix the size to match the actual data
            entry.size = entry.data.size();
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
            metadataUndo.push_back({name, previousSize, previousHash});
            std::cout << "Fixed issues with file: '" << name << "'" << std::endl;
        }
    }
    
    // Remove problematic files
    for (const auto& key : keysToRemove) {
        auto invalidEntry = m_files.extract(key);
        if (!invalidEntry.empty()) {
            removedEntries.push_back(std::move(invalidEntry));
        }
        std::cout << "Removed invalid file entry with key: '" << key << "'" << std::endl;
        issuesFixed++;
    }
    
    // Save the repaired archive
    if (issuesFixed > 0) {
        std::cout << "Fixed " << issuesFixed << " issues. Saving repaired archive..." << std::endl;
        if (SaveArchive()) {
            for (auto& removed : removedEntries) {
                SecureMemory::Cleanse(removed.mapped().data);
            }
            std::cout << "Archive successfully repaired and saved!" << std::endl;
            std::cout << "---------------------------------\n" << std::endl;
            return true;
        } else {
            std::cout << "Failed to save repaired archive!" << std::endl;
            for (const auto& undo : metadataUndo) {
                auto entry = m_files.find(undo.key);
                if (entry != m_files.end()) {
                    entry->second.size = undo.size;
                    entry->second.hash = undo.hash;
                }
            }
            for (auto& removed : removedEntries) {
                m_files.insert(std::move(removed));
            }
            std::cout << "Repair rollback restored the previous in-memory state" << std::endl;
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
    if (!PathSecurity::ValidateUsername(username)) {
        return archives;
    }
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
        if (entry.is_regular_file() && !entry.is_symlink()) {
            std::string filename = entry.path().filename().string();
            std::cout << " - " << filename;
            
            // Check if the file starts with the username prefix
            if (filename.size() > userPrefix.size() + 4 &&
                filename.compare(0, userPrefix.size(), userPrefix) == 0 &&
                entry.path().extension() == ".enc") {
                // Extract archive name from filename (remove username_ prefix and .enc extension)
                std::string archiveName = filename.substr(userPrefix.length());
                std::cout << " (matches user prefix)";
                
                archiveName.resize(archiveName.size() - 4);
                std::cout << ", extracted name: " << archiveName;

                const bool collision = std::any_of(
                    archives.begin(), archives.end(), [&](const std::string& existing) {
                        return PathSecurity::NamesCollide(existing, archiveName);
                    });
                if (PathSecurity::ValidateArchiveName(archiveName) && !collision) {
                    archives.push_back(archiveName);
                    std::cout << ", added to list" << std::endl;
                } else {
                    std::cout << ", rejected unsafe or colliding name" << std::endl;
                }
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
    if (!PathSecurity::ValidateArchiveName(archiveName)) {
        return;
    }
    const std::string previousName = m_archiveName;
    const std::string previousRevision = m_diskRevision;
    const bool previousHasRevision = m_hasDiskRevision;
    m_archiveName = archiveName;
    m_archivePath = GetArchiveFilePath();
    m_identityValid = !m_archivePath.empty();
    if (!m_identityValid) {
        m_archiveName = previousName;
        m_archivePath = GetArchiveFilePath();
        m_identityValid = !m_archivePath.empty();
        m_diskRevision = previousRevision;
        m_hasDiskRevision = previousHasRevision;
    } else {
        m_diskRevision.clear();
        m_hasDiskRevision = false;
    }
}

std::string CryptoArchive::GetArchiveName() const {
    return m_archiveName;
}

bool CryptoArchive::CreateNewArchive(const std::string& username, const std::string& password, const std::string& archiveName) {
    if (!PathSecurity::ValidateUsername(username) ||
        !PathSecurity::ValidateArchiveName(archiveName)) {
        return false;
    }
    for (const auto& existing : FindUserArchives(username)) {
        if (PathSecurity::NamesCollide(existing, archiveName)) {
            return false;
        }
    }
    // Verifică dacă există deja o arhivă cu acest nume
    CryptoArchive archive(username, archiveName);
    
    // Dacă arhiva există, returnăm false
    if (archive.ArchiveExists()) {
        return false;
    }
    
    // Inițializăm și salvăm noua arhivă
    return archive.InitializeArchive(password);
}

bool CryptoArchive::RenameArchive(const std::string& username,
                                  const std::string& currentName,
                                  const std::string& newName,
                                  std::string* error) {
    const auto fail = [error](const std::string& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    std::string validationError;
    if (!PathSecurity::ValidateUsername(username, &validationError) ||
        !PathSecurity::ValidateArchiveName(currentName, &validationError) ||
        !PathSecurity::ValidateArchiveName(newName, &validationError)) {
        return fail(validationError);
    }
    if (currentName == newName) {
        return fail("Enter a different archive name.");
    }

    for (const auto& existing : FindUserArchives(username)) {
        if (PathSecurity::NamesCollide(existing, newName)) {
            return fail("An archive with this name already exists.");
        }
    }

    std::filesystem::path sourcePath;
    std::filesystem::path destinationPath;
    if (!PathSecurity::ArchiveFilePath(username, currentName, sourcePath, &validationError) ||
        !PathSecurity::ArchiveFilePath(username, newName, destinationPath, &validationError)) {
        return fail(validationError.empty() ? "Could not resolve the archive path."
                                            : validationError);
    }

    std::error_code fileError;
    if (!std::filesystem::is_regular_file(sourcePath, fileError) || fileError) {
        return fail("The source archive does not exist or is not a regular file.");
    }
    if (!AtomicFile::RenameNoReplace(sourcePath, destinationPath)) {
        return fail("The archive could not be renamed without replacing another file.");
    }

    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool CryptoArchive::ChangePassword(const std::string& oldPassword, const std::string& newPassword) {
    std::cout << "\n---------- CHANGE PASSWORD ----------" << std::endl;
    
    if (!m_isLoaded) {
        std::cout << "Cannot change password - archive not loaded!" << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return false;
    }

    if (newPassword.empty()) {
        std::cout << "New password cannot be empty" << std::endl;
        return false;
    }
    
    // First verify the old password by attempting to decrypt the archive
    std::vector<uint8_t> decryptedData = DecryptArchiveData(oldPassword);
    SecureMemory::ScopedCleanse decryptedDataGuard(decryptedData);
    if (decryptedData.empty()) {
        std::cout << "Invalid old password!" << std::endl;
        std::cout << "---------------------------------\n" << std::endl;
        return false;
    }
    
    // Old password is correct, update the stored password.
    // Keep the current value until the new encrypted file is written successfully.
    SecureMemory::SecureString previousPassword(m_password.get());
    if (!m_password.assign(newPassword)) {
        return false;
    }
    const bool saveResult = SaveArchive();
    if (!saveResult) {
        m_password.assign(previousPassword.get());
        std::cout << "Password change failed; previous password remains active" << std::endl;
    } else {
        std::cout << "Password changed successfully" << std::endl;
    }
    
    std::cout << "---------------------------------\n" << std::endl;
    return saveResult;
}

bool CryptoArchive::PreparePasswordChange(const std::string& oldPassword,
                                          const std::string& newPassword,
                                          std::vector<uint8_t>& replacement) const {
    if (!m_isLoaded || oldPassword.empty() || newPassword.empty() ||
        !m_password.equals(oldPassword)) {
        return false;
    }

    std::vector<uint8_t> authenticatedPlaintext = DecryptArchiveData(oldPassword);
    SecureMemory::ScopedCleanse plaintextGuard(authenticatedPlaintext);
    if (authenticatedPlaintext.empty() || !BuildEncryptedArchive(newPassword, replacement)) {
        return false;
    }

    std::vector<uint8_t> verifiedPlaintext;
    SecureMemory::ScopedCleanse verifiedGuard(verifiedPlaintext);
    return DecryptSecureArchiveBytes(replacement, newPassword, verifiedPlaintext) &&
           verifiedPlaintext == authenticatedPlaintext;
}

bool CryptoArchive::ReloadArchive() {
    if (m_password.empty()) {
        return false;
    }
    return LoadArchive(m_password.get());
}

void CryptoArchive::ClearDecryptedData() noexcept {
    for (auto& [name, entry] : m_files) {
        (void)name;
        SecureMemory::Cleanse(entry.data);
    }
    m_files.clear();
}
