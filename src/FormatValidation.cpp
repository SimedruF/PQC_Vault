#include "FormatValidation.h"

#include <array>
#include <cstring>
#include <limits>

namespace FormatValidation {
namespace {

constexpr std::size_t MAX_COMPONENT_SIZE = 16U * 1024U * 1024U;
constexpr std::size_t MAX_USER_FILE_SIZE = 64U * 1024U * 1024U;
constexpr std::size_t MAX_CONTAINER_SIZE = 1024U * 1024U * 1024U;
constexpr std::array<std::uint8_t, 8> USER_V5_MAGIC =
    {'P', 'Q', 'C', 'U', 'S', 'R', '0', '5'};
constexpr std::array<std::uint8_t, 8> ARCHIVE_V2_MAGIC =
    {'P', 'Q', 'C', 'E', 'N', 'C', '0', '2'};
constexpr std::array<std::uint8_t, 8> ARCHIVE_V1_MAGIC =
    {'P', 'Q', 'C', 'E', 'N', 'C', '0', '1'};
constexpr std::array<std::uint8_t, 8> DATABASE_V2_MAGIC =
    {'P', 'Q', 'C', 'D', 'B', '0', '0', '2'};

bool StartsWith(const std::uint8_t* data, std::size_t size,
                const std::array<std::uint8_t, 8>& magic) noexcept {
    return data != nullptr && size >= magic.size() &&
           std::memcmp(data, magic.data(), magic.size()) == 0;
}

bool ReadBe32(const std::uint8_t* data, std::size_t size,
              std::size_t& offset, std::uint32_t& value) noexcept {
    if (data == nullptr || offset > size || size - offset < 4) {
        return false;
    }
    value = 0;
    for (int i = 0; i < 4; ++i) {
        value = (value << 8U) | data[offset++];
    }
    return true;
}

bool ReadBe64(const std::uint8_t* data, std::size_t size,
              std::size_t& offset, std::uint64_t& value) noexcept {
    if (data == nullptr || offset > size || size - offset < 8) {
        return false;
    }
    value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8U) | data[offset++];
    }
    return true;
}

template <typename Integer>
bool ReadNative(const std::uint8_t* data, std::size_t size,
                std::size_t& offset, Integer& value) noexcept {
    if (data == nullptr || offset > size || size - offset < sizeof(Integer)) {
        return false;
    }
    std::memcpy(&value, data + offset, sizeof(Integer));
    offset += sizeof(Integer);
    return true;
}

bool SkipComponent(std::size_t inputSize, std::size_t& offset,
                   std::uint64_t componentSize) noexcept {
    if (offset > inputSize || componentSize == 0 ||
        componentSize > MAX_COMPONENT_SIZE ||
        componentSize > inputSize - offset) {
        return false;
    }
    offset += static_cast<std::size_t>(componentSize);
    return true;
}

bool ValidateNativeComponents(const std::uint8_t* data, std::size_t size,
                              std::size_t offset, std::size_t count,
                              bool lengthIsSizeT) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t componentSize = 0;
        if (lengthIsSizeT) {
            std::size_t nativeSize = 0;
            if (!ReadNative(data, size, offset, nativeSize)) {
                return false;
            }
            componentSize = nativeSize;
        } else if (!ReadNative(data, size, offset, componentSize)) {
            return false;
        }
        if (offset > size || !SkipComponent(size, offset, componentSize)) {
            return false;
        }
    }
    return offset == size;
}

bool ValidateAuthenticatedContainer(const std::uint8_t* data, std::size_t size,
                                    const std::array<std::uint8_t, 8>& magic,
                                    std::uint32_t expectedVersion) noexcept {
    if (data == nullptr || size > MAX_CONTAINER_SIZE || size < 112 ||
        !StartsWith(data, size, magic)) {
        return false;
    }
    std::size_t offset = magic.size();
    std::uint32_t version = 0;
    std::uint32_t kdf = 0;
    std::uint64_t n = 0;
    std::uint32_t r = 0;
    std::uint32_t p = 0;
    std::uint32_t saltSize = 0;
    std::uint32_t nonceSize = 0;
    std::uint32_t tagSize = 0;
    std::uint64_t ciphertextSize = 0;
    if (!ReadBe32(data, size, offset, version) ||
        !ReadBe32(data, size, offset, kdf) ||
        !ReadBe64(data, size, offset, n) ||
        !ReadBe32(data, size, offset, r) ||
        !ReadBe32(data, size, offset, p) ||
        !ReadBe32(data, size, offset, saltSize) ||
        !ReadBe32(data, size, offset, nonceSize) ||
        !ReadBe32(data, size, offset, tagSize) ||
        !ReadBe64(data, size, offset, ciphertextSize) ||
        version != expectedVersion || kdf != 1 || n != 32768 || r != 8 || p != 1 ||
        saltSize != 32 || nonceSize != 12 || tagSize != 16 || ciphertextSize == 0 ||
        ciphertextSize > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    const std::uint64_t remaining = static_cast<std::uint64_t>(saltSize) + nonceSize +
                                    ciphertextSize + tagSize;
    return offset <= size && remaining == size - offset;
}

} // namespace

bool ValidateUserFile(const std::uint8_t* data, std::size_t size,
                      UserFormat* format) noexcept {
    if (format != nullptr) {
        *format = UserFormat::Invalid;
    }
    if (data == nullptr || size == 0 || size > MAX_USER_FILE_SIZE) {
        return false;
    }

    if (StartsWith(data, size, USER_V5_MAGIC)) {
        std::size_t offset = USER_V5_MAGIC.size();
        std::uint32_t version = 0;
        std::uint64_t totalSize = 0;
        std::uint32_t componentCount = 0;
        if (!ReadBe32(data, size, offset, version) ||
            !ReadBe64(data, size, offset, totalSize) ||
            !ReadBe32(data, size, offset, componentCount) ||
            version != 5 || totalSize != size || componentCount != 9) {
            return false;
        }
        for (std::uint32_t i = 0; i < componentCount; ++i) {
            std::uint64_t componentSize = 0;
            if (!ReadBe64(data, size, offset, componentSize) || offset > size ||
                !SkipComponent(size, offset, componentSize)) {
                return false;
            }
        }
        if (offset != size) {
            return false;
        }
        if (format != nullptr) {
            *format = UserFormat::V5;
        }
        return true;
    }

    std::size_t offset = 0;
    std::uint32_t nativeVersion = 0;
    if (!ReadNative(data, size, offset, nativeVersion)) {
        return false;
    }
    if (nativeVersion == 4 || nativeVersion == 3) {
        const bool valid = ValidateNativeComponents(data, size, offset, 9, false);
        if (valid && format != nullptr) {
            *format = nativeVersion == 4 ? UserFormat::V4 : UserFormat::V3;
        }
        return valid;
    }
    if (nativeVersion == 2) {
        const bool valid = ValidateNativeComponents(data, size, offset, 7, true);
        if (valid && format != nullptr) {
            *format = UserFormat::V2;
        }
        return valid;
    }

    const bool valid = ValidateNativeComponents(data, size, 0, 4, true);
    if (valid && format != nullptr) {
        *format = UserFormat::V1;
    }
    return valid;
}

bool ValidateArchiveFile(const std::uint8_t* data, std::size_t size) noexcept {
    if (ValidateAuthenticatedContainer(data, size, ARCHIVE_V2_MAGIC, 2)) {
        return true;
    }
    if (!StartsWith(data, size, ARCHIVE_V1_MAGIC) || size < 17 ||
        size > MAX_CONTAINER_SIZE) {
        return false;
    }
    std::size_t offset = ARCHIVE_V1_MAGIC.size();
    std::uint64_t payloadSize = 0;
    return ReadNative(data, size, offset, payloadSize) && payloadSize != 0 &&
           payloadSize == size - offset;
}

bool ValidateDatabaseV2(const std::uint8_t* data, std::size_t size) noexcept {
    return ValidateAuthenticatedContainer(data, size, DATABASE_V2_MAGIC, 2);
}

} // namespace FormatValidation
