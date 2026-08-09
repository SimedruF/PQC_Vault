#pragma once

#include <cstddef>
#include <cstdint>

namespace FormatValidation {

enum class UserFormat : std::uint8_t {
    Invalid = 0,
    V1 = 1,
    V2 = 2,
    V3 = 3,
    V4 = 4,
    V5 = 5
};

bool ValidateUserFile(const std::uint8_t* data, std::size_t size,
                      UserFormat* format = nullptr) noexcept;
bool ValidateArchiveFile(const std::uint8_t* data, std::size_t size) noexcept;
bool ValidateDatabaseV2(const std::uint8_t* data, std::size_t size) noexcept;

} // namespace FormatValidation
