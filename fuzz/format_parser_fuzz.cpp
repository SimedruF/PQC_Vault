#include "FormatValidation.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    FormatValidation::UserFormat format = FormatValidation::UserFormat::Invalid;
    (void)FormatValidation::ValidateUserFile(data, size, &format);
    (void)FormatValidation::ValidateArchiveFile(data, size);
    (void)FormatValidation::ValidateDatabaseV2(data, size);
    return 0;
}
