#include "AtomicFile.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <iostream>
#include <limits>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#endif

namespace AtomicFile {
namespace {

std::atomic<unsigned long long> g_temporaryCounter{0};
std::atomic<bool> g_failBeforeReplace{false};
std::atomic<bool> g_failBeforeTemporaryCreate{false};

std::filesystem::path ParentDirectory(const std::filesystem::path& destination) {
    const auto parent = destination.parent_path();
    return parent.empty() ? std::filesystem::path(".") : parent;
}

std::filesystem::path TemporaryPath(const std::filesystem::path& destination) {
#ifdef _WIN32
    const auto processId = static_cast<unsigned long long>(GetCurrentProcessId());
#else
    const auto processId = static_cast<unsigned long long>(getpid());
#endif
    const auto counter = g_temporaryCounter.fetch_add(1, std::memory_order_relaxed);
    return ParentDirectory(destination) /
           (destination.filename().string() + ".tmp." + std::to_string(processId) + "." +
            std::to_string(counter));
}

void RemoveTemporaryFile(const std::filesystem::path& path) noexcept {
    std::error_code error;
    std::filesystem::remove(path, error);
}

#ifndef _WIN32
bool SynchronizeDirectory(const std::filesystem::path& directory) {
    int flags = O_RDONLY;
#ifdef O_DIRECTORY
    flags |= O_DIRECTORY;
#endif
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
    const int descriptor = open(directory.c_str(), flags);
    if (descriptor < 0) {
        return false;
    }

    const bool synchronized = fsync(descriptor) == 0;
    const int savedError = errno;
    close(descriptor);
    errno = savedError;
    return synchronized;
}
#endif

} // namespace

namespace Testing {

void FailNextWriteBeforeReplace() {
    g_failBeforeReplace.store(true, std::memory_order_release);
}

void FailNextWriteBeforeTemporaryCreate() {
    g_failBeforeTemporaryCreate.store(true, std::memory_order_release);
}

} // namespace Testing

bool Write(const std::filesystem::path& destination, const uint8_t* data, size_t size) {
    if (destination.empty() || destination.filename().empty() || (size != 0 && data == nullptr)) {
        return false;
    }

    try {
        const std::filesystem::path parent = ParentDirectory(destination);
        std::error_code directoryError;
        std::filesystem::create_directories(parent, directoryError);
        if (directoryError || !std::filesystem::is_directory(parent)) {
            std::cerr << "Cannot prepare destination directory: " << parent << std::endl;
            return false;
        }
        if (g_failBeforeTemporaryCreate.exchange(false, std::memory_order_acq_rel)) {
            return false;
        }

        std::filesystem::path temporary;

#ifdef _WIN32
        HANDLE handle = INVALID_HANDLE_VALUE;
        for (int attempt = 0; attempt < 32 && handle == INVALID_HANDLE_VALUE; ++attempt) {
            temporary = TemporaryPath(destination);
            handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                 FILE_ATTRIBUTE_TEMPORARY, nullptr);
            if (handle == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS) {
                break;
            }
        }
        if (handle == INVALID_HANDLE_VALUE) {
            std::cerr << "Cannot create temporary file for: " << destination << std::endl;
            return false;
        }

        bool success = true;
        size_t offset = 0;
        while (offset < size) {
            const size_t remaining = size - offset;
            const DWORD chunk = static_cast<DWORD>(std::min<size_t>(
                remaining, static_cast<size_t>(std::numeric_limits<DWORD>::max())));
            DWORD written = 0;
            if (!WriteFile(handle, data + offset, chunk, &written, nullptr) || written != chunk) {
                success = false;
                break;
            }
            offset += written;
        }

        if (success && !FlushFileBuffers(handle)) {
            success = false;
        }
        if (!CloseHandle(handle)) {
            success = false;
        }

        if (success && g_failBeforeReplace.exchange(false, std::memory_order_acq_rel)) {
            success = false;
        }
        if (!success) {
            RemoveTemporaryFile(temporary);
            return false;
        }

        if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            RemoveTemporaryFile(temporary);
            return false;
        }
#else
        int descriptor = -1;
        for (int attempt = 0; attempt < 32 && descriptor < 0; ++attempt) {
            temporary = TemporaryPath(destination);
            int flags = O_WRONLY | O_CREAT | O_EXCL;
#ifdef O_CLOEXEC
            flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
            flags |= O_NOFOLLOW;
#endif
            descriptor = open(temporary.c_str(), flags, S_IRUSR | S_IWUSR);
            if (descriptor < 0 && errno != EEXIST) {
                break;
            }
        }
        if (descriptor < 0) {
            std::cerr << "Cannot create temporary file for: " << destination << std::endl;
            return false;
        }

        bool success = fchmod(descriptor, S_IRUSR | S_IWUSR) == 0;
        size_t offset = 0;
        while (success && offset < size) {
            const ssize_t written = write(descriptor, data + offset, size - offset);
            if (written > 0) {
                offset += static_cast<size_t>(written);
            } else if (written < 0 && errno == EINTR) {
                continue;
            } else {
                success = false;
            }
        }

        if (success && fsync(descriptor) != 0) {
            success = false;
        }
        if (close(descriptor) != 0) {
            success = false;
        }

        if (success && g_failBeforeReplace.exchange(false, std::memory_order_acq_rel)) {
            success = false;
        }
        if (!success) {
            RemoveTemporaryFile(temporary);
            return false;
        }

        if (rename(temporary.c_str(), destination.c_str()) != 0) {
            RemoveTemporaryFile(temporary);
            return false;
        }

        // The new file is already atomically committed. A directory fsync failure
        // affects crash durability, not the integrity of the visible destination.
        if (!SynchronizeDirectory(parent)) {
            std::cerr << "Warning: could not synchronize directory after replacing: "
                      << destination << std::endl;
        }
#endif
        return true;
    } catch (const std::exception& error) {
        std::cerr << "Atomic write failed for " << destination << ": " << error.what()
                  << std::endl;
        return false;
    }
}

bool RenameNoReplace(const std::filesystem::path& source,
                     const std::filesystem::path& destination) {
    if (source.empty() || destination.empty() || source.filename().empty() ||
        destination.filename().empty() || source == destination ||
        ParentDirectory(source).lexically_normal() !=
            ParentDirectory(destination).lexically_normal()) {
        return false;
    }

    std::error_code statusError;
    const auto sourceStatus = std::filesystem::symlink_status(source, statusError);
    if (statusError || sourceStatus.type() != std::filesystem::file_type::regular) {
        return false;
    }

#ifdef _WIN32
    return MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
#else
#if defined(__linux__) && defined(SYS_renameat2)
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1U << 0)
#endif
    if (syscall(SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD,
                destination.c_str(), RENAME_NOREPLACE) == 0) {
        if (!SynchronizeDirectory(ParentDirectory(destination))) {
            std::cerr << "Warning: could not synchronize directory after renaming: "
                      << destination << std::endl;
        }
        return true;
    }
    if (errno != ENOSYS && errno != EINVAL) {
        return false;
    }
#endif

    // Portable POSIX fallback. link() never replaces an existing destination.
    if (link(source.c_str(), destination.c_str()) != 0) {
        return false;
    }
    if (unlink(source.c_str()) != 0) {
        const int savedError = errno;
        unlink(destination.c_str());
        errno = savedError;
        return false;
    }
    if (!SynchronizeDirectory(ParentDirectory(destination))) {
        std::cerr << "Warning: could not synchronize directory after renaming: "
                  << destination << std::endl;
    }
    return true;
#endif
}

} // namespace AtomicFile
