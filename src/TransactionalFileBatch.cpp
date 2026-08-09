#include "TransactionalFileBatch.h"

#include "AtomicFile.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <set>
#include <string>

namespace TransactionalFileBatch {
namespace {

constexpr std::array<uint8_t, 8> JOURNAL_MAGIC = {'P', 'Q', 'C', 'T', 'X', 'N', '0', '1'};
constexpr uint32_t JOURNAL_PREPARED = 1;
constexpr uint32_t JOURNAL_COMMITTED = 2;
constexpr uint32_t MAX_TRANSACTION_FILES = 1024;
constexpr uint32_t MAX_PATH_SIZE = 16 * 1024;
constexpr size_t NO_FAILURE = std::numeric_limits<size_t>::max();

struct Journal {
    uint32_t state = JOURNAL_PREPARED;
    uint32_t committedCount = 0;
    std::vector<std::filesystem::path> destinations;
};

std::atomic<unsigned long long> g_transactionCounter{0};
std::atomic<size_t> g_failBeforeTarget{NO_FAILURE};
std::atomic<size_t> g_crashAfterTarget{NO_FAILURE};

std::filesystem::path TransactionRoot() {
    return std::filesystem::path(".pqcwallet_transactions");
}

void AppendUint32(std::vector<uint8_t>& output, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
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

std::filesystem::path BackupPath(const std::filesystem::path& transaction, size_t index) {
    return transaction / ("original." + std::to_string(index));
}

std::filesystem::path ReplacementPath(const std::filesystem::path& transaction, size_t index) {
    return transaction / ("replacement." + std::to_string(index));
}

std::filesystem::path ManifestPath(const std::filesystem::path& transaction) {
    return transaction / "manifest.bin";
}

bool ReadFile(const std::filesystem::path& path, std::vector<uint8_t>& output) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        return false;
    }
    const uintmax_t rawSize = std::filesystem::file_size(path, error);
    if (error || rawSize > static_cast<uintmax_t>(std::numeric_limits<size_t>::max()) ||
        rawSize > static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    output.resize(static_cast<size_t>(rawSize));
    if (!output.empty()) {
        file.read(reinterpret_cast<char*>(output.data()),
                  static_cast<std::streamsize>(output.size()));
    }
    return file.good() || (file.eof() && file.gcount() == static_cast<std::streamsize>(output.size()));
}

std::vector<uint8_t> EncodeJournal(const Journal& journal) {
    std::vector<uint8_t> output;
    output.insert(output.end(), JOURNAL_MAGIC.begin(), JOURNAL_MAGIC.end());
    AppendUint32(output, journal.state);
    AppendUint32(output, journal.committedCount);
    AppendUint32(output, static_cast<uint32_t>(journal.destinations.size()));
    for (const auto& destination : journal.destinations) {
        const std::string path = destination.generic_string();
        AppendUint32(output, static_cast<uint32_t>(path.size()));
        output.insert(output.end(), path.begin(), path.end());
    }
    return output;
}

bool DecodeJournal(const std::vector<uint8_t>& input, Journal& journal) {
    if (input.size() < JOURNAL_MAGIC.size() + 3 * sizeof(uint32_t) ||
        !std::equal(JOURNAL_MAGIC.begin(), JOURNAL_MAGIC.end(), input.begin())) {
        return false;
    }

    size_t offset = JOURNAL_MAGIC.size();
    uint32_t count = 0;
    if (!ReadUint32(input, offset, journal.state) ||
        !ReadUint32(input, offset, journal.committedCount) ||
        !ReadUint32(input, offset, count) || count == 0 || count > MAX_TRANSACTION_FILES ||
        (journal.state != JOURNAL_PREPARED && journal.state != JOURNAL_COMMITTED) ||
        journal.committedCount > count) {
        return false;
    }

    journal.destinations.clear();
    journal.destinations.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t length = 0;
        if (!ReadUint32(input, offset, length) || length == 0 || length > MAX_PATH_SIZE ||
            offset > input.size() || input.size() - offset < length) {
            return false;
        }
        const std::string path(input.begin() + static_cast<std::ptrdiff_t>(offset),
                               input.begin() + static_cast<std::ptrdiff_t>(offset + length));
        offset += length;
        journal.destinations.emplace_back(path);
    }
    return offset == input.size();
}

bool WriteJournal(const std::filesystem::path& transaction, const Journal& journal) {
    const std::vector<uint8_t> encoded = EncodeJournal(journal);
    return AtomicFile::Write(ManifestPath(transaction), encoded);
}

bool ReadJournal(const std::filesystem::path& transaction, Journal& journal) {
    std::vector<uint8_t> encoded;
    return ReadFile(ManifestPath(transaction), encoded) && DecodeJournal(encoded, journal);
}

bool CleanTransactionDirectory(const std::filesystem::path& transaction) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(transaction, error);
    if (error || status.type() != std::filesystem::file_type::directory) {
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(transaction, error)) {
        if (error) {
            return false;
        }
        const auto entryStatus = entry.symlink_status(error);
        if (error || entryStatus.type() != std::filesystem::file_type::regular) {
            return false;
        }
    }
    for (const auto& entry : std::filesystem::directory_iterator(transaction, error)) {
        if (error || !std::filesystem::remove(entry.path(), error) || error) {
            return false;
        }
    }
    return std::filesystem::remove(transaction, error) && !error;
}

bool Rollback(const std::filesystem::path& transaction, const Journal& journal) {
    bool success = true;
    for (size_t i = 0; i < journal.destinations.size(); ++i) {
        std::vector<uint8_t> original;
        if (!ReadFile(BackupPath(transaction, i), original) ||
            !AtomicFile::Write(journal.destinations[i], original)) {
            success = false;
        }
    }
    return success;
}

std::filesystem::path CreateTransactionDirectory() {
    const std::filesystem::path root = TransactionRoot();
    std::error_code error;
    std::filesystem::create_directories(root, error);
    if (error) {
        return {};
    }
    const auto rootStatus = std::filesystem::symlink_status(root, error);
    if (error || rootStatus.type() != std::filesystem::file_type::directory) {
        return {};
    }
    std::filesystem::permissions(root, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, error);
    if (error) {
        return {};
    }

    for (int attempt = 0; attempt < 32; ++attempt) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto counter = g_transactionCounter.fetch_add(1, std::memory_order_relaxed);
        const auto candidate = root / ("master-password-" + std::to_string(now) + "-" +
                                       std::to_string(counter));
        if (std::filesystem::create_directory(candidate, error)) {
            std::filesystem::permissions(candidate, std::filesystem::perms::owner_all,
                                         std::filesystem::perm_options::replace, error);
            return error ? std::filesystem::path{} : candidate;
        }
        if (error && error != std::errc::file_exists) {
            return {};
        }
        error.clear();
    }
    return {};
}

} // namespace

namespace Testing {

void FailBeforeTargetWrite(size_t targetIndex) {
    g_failBeforeTarget.store(targetIndex, std::memory_order_release);
}

void SimulateCrashAfterTargetWrite(size_t targetIndex) {
    g_crashAfterTarget.store(targetIndex, std::memory_order_release);
}

} // namespace Testing

bool RecoverPendingTransactions() {
    const std::filesystem::path root = TransactionRoot();
    std::error_code error;
    if (!std::filesystem::exists(root, error)) {
        return !error;
    }
    if (error || std::filesystem::symlink_status(root, error).type() !=
                     std::filesystem::file_type::directory) {
        return false;
    }

    bool success = true;
    for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
        if (error) {
            return false;
        }
        const auto status = entry.symlink_status(error);
        if (error || status.type() != std::filesystem::file_type::directory) {
            success = false;
            continue;
        }

        Journal journal;
        if (!ReadJournal(entry.path(), journal)) {
            // No valid manifest means publication never started.
            success = CleanTransactionDirectory(entry.path()) && success;
            continue;
        }

        if (journal.state == JOURNAL_PREPARED && !Rollback(entry.path(), journal)) {
            success = false;
            continue;
        }
        success = CleanTransactionDirectory(entry.path()) && success;
    }

    if (success && std::filesystem::is_empty(root, error) && !error) {
        std::filesystem::remove(root, error);
    }
    return success && !error;
}

bool Commit(const std::vector<Entry>& entries) {
    if (entries.empty() || entries.size() > MAX_TRANSACTION_FILES ||
        !RecoverPendingTransactions()) {
        return false;
    }

    Journal journal;
    journal.destinations.reserve(entries.size());
    std::set<std::filesystem::path> uniqueDestinations;
    for (const auto& entry : entries) {
        if (entry.destination.empty() || entry.replacement.empty()) {
            return false;
        }
        std::error_code error;
        const auto absolute = std::filesystem::absolute(entry.destination, error).lexically_normal();
        const std::string serializedPath = absolute.generic_string();
        if (error || !std::filesystem::is_regular_file(absolute, error) || error ||
            serializedPath.empty() || serializedPath.size() > MAX_PATH_SIZE ||
            !uniqueDestinations.insert(absolute).second) {
            return false;
        }
        journal.destinations.push_back(absolute);
    }

    const std::filesystem::path transaction = CreateTransactionDirectory();
    if (transaction.empty()) {
        return false;
    }

    for (size_t i = 0; i < entries.size(); ++i) {
        std::vector<uint8_t> original;
        std::vector<uint8_t> stagedReplacement;
        if (!ReadFile(journal.destinations[i], original) ||
            !AtomicFile::Write(BackupPath(transaction, i), original) ||
            !AtomicFile::Write(ReplacementPath(transaction, i), entries[i].replacement) ||
            !ReadFile(ReplacementPath(transaction, i), stagedReplacement) ||
            stagedReplacement != entries[i].replacement) {
            CleanTransactionDirectory(transaction);
            return false;
        }
    }

    if (!WriteJournal(transaction, journal)) {
        CleanTransactionDirectory(transaction);
        return false;
    }

    const size_t failIndex = g_failBeforeTarget.exchange(NO_FAILURE, std::memory_order_acq_rel);
    const size_t crashIndex = g_crashAfterTarget.exchange(NO_FAILURE, std::memory_order_acq_rel);
    for (size_t i = 0; i < entries.size(); ++i) {
        if (failIndex == i) {
            const bool rolledBack = Rollback(transaction, journal);
            if (rolledBack) {
                CleanTransactionDirectory(transaction);
            }
            return false;
        }

        std::vector<uint8_t> replacement;
        if (!ReadFile(ReplacementPath(transaction, i), replacement) ||
            !AtomicFile::Write(journal.destinations[i], replacement)) {
            const bool rolledBack = Rollback(transaction, journal);
            if (rolledBack) {
                CleanTransactionDirectory(transaction);
            }
            return false;
        }

        if (crashIndex == i) {
            return false;
        }

        journal.committedCount = static_cast<uint32_t>(i + 1);
        if (!WriteJournal(transaction, journal)) {
            const bool rolledBack = Rollback(transaction, journal);
            if (rolledBack) {
                CleanTransactionDirectory(transaction);
            }
            return false;
        }
    }

    journal.state = JOURNAL_COMMITTED;
    if (!WriteJournal(transaction, journal)) {
        const bool rolledBack = Rollback(transaction, journal);
        if (rolledBack) {
            CleanTransactionDirectory(transaction);
        }
        return false;
    }

    if (!CleanTransactionDirectory(transaction)) {
        std::cerr << "Warning: committed master-password journal could not be removed" << std::endl;
    }
    return true;
}

} // namespace TransactionalFileBatch
