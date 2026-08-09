#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace TransactionalFileBatch {

struct Entry {
    std::filesystem::path destination;
    std::vector<uint8_t> replacement;
};

// Publishes all replacements as one recoverable logical transaction. If the
// process stops during publication, RecoverPendingTransactions restores every
// original file before the application opens encrypted state again.
bool Commit(const std::vector<Entry>& entries);

// Safe to call repeatedly. Committed journals are cleaned; incomplete journals
// are rolled back to their original files.
bool RecoverPendingTransactions();

namespace Testing {

// The next Commit fails immediately before publishing the selected target and
// performs its normal rollback.
void FailBeforeTargetWrite(size_t targetIndex);

// The next Commit returns after publishing the selected target and intentionally
// leaves its journal behind, simulating a process crash for recovery tests.
void SimulateCrashAfterTargetWrite(size_t targetIndex);

} // namespace Testing
} // namespace TransactionalFileBatch
