#include "SecureMemory.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

namespace {

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
    }
    return condition;
}

} // namespace

int main() {
    static_assert(!std::is_copy_constructible_v<SecureMemory::SecureString>);
    static_assert(!std::is_copy_assignable_v<SecureMemory::SecureString>);
    static_assert(!std::is_move_constructible_v<SecureMemory::SecureString>);

    bool success = true;

    SecureMemory::SecureString secret("first secret");
    success &= Expect(secret.equals("first secret"), "retain a secret without implicit copies");
    success &= Expect(!secret.equals("wrong secret"), "constant-time value comparison");
    success &= Expect(secret.assign("replacement"), "replace a retained secret");
    success &= Expect(secret.equals("replacement"), "read replacement secret");
    secret.clear();
    success &= Expect(secret.empty(), "clear retained secret");

    char passwordBuffer[32] = "temporary password";
    SecureMemory::Cleanse(passwordBuffer);
    success &= Expect(std::all_of(std::begin(passwordBuffer), std::end(passwordBuffer),
                                 [](char value) { return value == 0; }),
                      "cleanse fixed password buffer");

    std::vector<unsigned char> key(32, 0xa5U);
    SecureMemory::Cleanse(key);
    success &= Expect(std::all_of(key.begin(), key.end(),
                                 [](unsigned char value) { return value == 0; }),
                      "cleanse key vector");

    std::string plaintext = "decrypted payload";
    SecureMemory::Cleanse(plaintext);
    success &= Expect(std::all_of(plaintext.begin(), plaintext.end(),
                                 [](char value) { return value == 0; }),
                      "cleanse plaintext string");

    return success ? 0 : 1;
}
