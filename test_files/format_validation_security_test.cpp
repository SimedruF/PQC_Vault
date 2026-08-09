#include "FormatValidation.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
    }
    return condition;
}

template <typename Integer>
void AppendNative(std::vector<std::uint8_t>& output, Integer value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    output.insert(output.end(), bytes, bytes + sizeof(value));
}

void AppendBe32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        output.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void AppendBe64(std::vector<std::uint8_t>& output, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

std::vector<std::uint8_t> MakeUserV5() {
    std::vector<std::uint8_t> result{'P', 'Q', 'C', 'U', 'S', 'R', '0', '5'};
    AppendBe32(result, 5);
    AppendBe64(result, 24 + 9 * 9);
    AppendBe32(result, 9);
    for (std::uint8_t value = 1; value <= 9; ++value) {
        AppendBe64(result, 1);
        result.push_back(value);
    }
    return result;
}

std::vector<std::uint8_t> MakeNativeUser(std::uint32_t version,
                                         std::size_t componentCount,
                                         bool sizeTLengths) {
    std::vector<std::uint8_t> result;
    if (version != 1) {
        AppendNative(result, version);
    }
    for (std::size_t i = 0; i < componentCount; ++i) {
        if (sizeTLengths) {
            AppendNative(result, std::size_t{1});
        } else {
            AppendNative(result, std::uint64_t{1});
        }
        result.push_back(static_cast<std::uint8_t>(i + 1));
    }
    return result;
}

std::vector<std::uint8_t> MakeAuthenticatedContainer(const char (&magic)[9]) {
    std::vector<std::uint8_t> result(magic, magic + 8);
    AppendBe32(result, 2);
    AppendBe32(result, 1);
    AppendBe64(result, 32768);
    AppendBe32(result, 8);
    AppendBe32(result, 1);
    AppendBe32(result, 32);
    AppendBe32(result, 12);
    AppendBe32(result, 16);
    AppendBe64(result, 1);
    result.insert(result.end(), 32 + 12 + 1 + 16, 0x5a);
    return result;
}

bool ValidateUserAs(const std::vector<std::uint8_t>& input,
                    FormatValidation::UserFormat expected) {
    FormatValidation::UserFormat actual = FormatValidation::UserFormat::Invalid;
    return FormatValidation::ValidateUserFile(input.data(), input.size(), &actual) &&
           actual == expected;
}

} // namespace

int main() {
    bool success = true;

    const auto userV1 = MakeNativeUser(1, 4, true);
    const auto userV2 = MakeNativeUser(2, 7, true);
    const auto userV3 = MakeNativeUser(3, 9, false);
    const auto userV4 = MakeNativeUser(4, 9, false);
    const auto userV5 = MakeUserV5();
    success &= Expect(ValidateUserAs(userV1, FormatValidation::UserFormat::V1),
                      "accept structurally valid V1 user format");
    success &= Expect(ValidateUserAs(userV2, FormatValidation::UserFormat::V2),
                      "accept structurally valid V2 user format");
    success &= Expect(ValidateUserAs(userV3, FormatValidation::UserFormat::V3),
                      "accept structurally valid V3 user format");
    success &= Expect(ValidateUserAs(userV4, FormatValidation::UserFormat::V4),
                      "accept structurally valid V4 user format");
    success &= Expect(ValidateUserAs(userV5, FormatValidation::UserFormat::V5),
                      "accept structurally valid portable V5 user format");

    std::vector<std::vector<std::uint8_t>> userFormats =
        {userV1, userV2, userV3, userV4, userV5};
    for (auto format : userFormats) {
        format.pop_back();
        success &= Expect(!FormatValidation::ValidateUserFile(format.data(), format.size()),
                          "reject every truncated user format");
    }

    auto trailingUser = userV5;
    trailingUser.push_back(0);
    success &= Expect(!FormatValidation::ValidateUserFile(trailingUser.data(), trailingUser.size()),
                      "reject trailing V5 bytes");
    auto maliciousUser = userV5;
    std::fill(maliciousUser.begin() + 24, maliciousUser.begin() + 32, 0xff);
    success &= Expect(!FormatValidation::ValidateUserFile(maliciousUser.data(), maliciousUser.size()),
                      "reject malicious V5 component length");

    auto archiveV1 = std::vector<std::uint8_t>{'P', 'Q', 'C', 'E', 'N', 'C', '0', '1'};
    AppendNative(archiveV1, std::uint64_t{1});
    archiveV1.push_back(0x42);
    success &= Expect(FormatValidation::ValidateArchiveFile(archiveV1.data(), archiveV1.size()),
                      "accept PQCENC01 structure");

    const auto archiveV2 = MakeAuthenticatedContainer("PQCENC02");
    const auto databaseV2 = MakeAuthenticatedContainer("PQCDB002");
    success &= Expect(FormatValidation::ValidateArchiveFile(archiveV2.data(), archiveV2.size()),
                      "accept PQCENC02 structure");
    success &= Expect(FormatValidation::ValidateDatabaseV2(databaseV2.data(), databaseV2.size()),
                      "accept PQCDB002 structure");

    auto truncatedArchive = archiveV2;
    truncatedArchive.pop_back();
    success &= Expect(!FormatValidation::ValidateArchiveFile(truncatedArchive.data(),
                                                              truncatedArchive.size()),
                      "reject truncated PQCENC02");
    auto oversizedArchive = archiveV2;
    std::fill(oversizedArchive.begin() + 44, oversizedArchive.begin() + 52, 0xff);
    success &= Expect(!FormatValidation::ValidateArchiveFile(oversizedArchive.data(),
                                                              oversizedArchive.size()),
                      "reject oversized encrypted payload declaration");
    auto trailingDatabase = databaseV2;
    trailingDatabase.push_back(0);
    success &= Expect(!FormatValidation::ValidateDatabaseV2(trailingDatabase.data(),
                                                             trailingDatabase.size()),
                      "reject trailing PQCDB002 data");

    success &= Expect(!FormatValidation::ValidateUserFile(nullptr, 0),
                      "reject empty user input");
    success &= Expect(!FormatValidation::ValidateArchiveFile(nullptr, 0),
                      "reject empty archive input");
    success &= Expect(!FormatValidation::ValidateDatabaseV2(nullptr, 0),
                      "reject empty database input");
    return success ? 0 : 1;
}
