#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <openssl/crypto.h>

namespace SecureMemory {

inline void Cleanse(void* data, std::size_t size) noexcept {
    if (data != nullptr && size != 0) {
        OPENSSL_cleanse(data, size);
    }
}

template <std::size_t N>
inline void Cleanse(char (&buffer)[N]) noexcept {
    Cleanse(buffer, N);
}

template <typename T>
inline void Cleanse(std::vector<T>& value) noexcept {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Secure cleansing requires trivially copyable elements");
    if (!value.empty()) {
        Cleanse(value.data(), value.size() * sizeof(T));
    }
}

inline void Cleanse(std::string& value) noexcept {
    if (!value.empty()) {
        Cleanse(value.data(), value.size());
    }
}

// Non-copyable owner for passwords that must survive beyond a single call.
// The occupied bytes are overwritten before replacement and destruction.
class SecureString {
public:
    SecureString() = default;

    explicit SecureString(std::string_view value) {
        assign(value);
    }

    ~SecureString() {
        clear();
    }

    SecureString(const SecureString&) = delete;
    SecureString& operator=(const SecureString&) = delete;
    SecureString(SecureString&&) = delete;
    SecureString& operator=(SecureString&&) = delete;

    bool assign(std::string_view value) {
        if (value.data() == value_.data() && value.size() == value_.size()) {
            return true;
        }
        clear();
        if (value.empty()) {
            return true;
        }
        try {
            value_.assign(value.data(), value.size());
            return true;
        } catch (...) {
            clear();
            return false;
        }
    }

    void clear() noexcept {
        Cleanse(value_);
        value_.clear();
    }

    [[nodiscard]] bool empty() const noexcept { return value_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return value_.size(); }
    [[nodiscard]] const std::string& get() const noexcept { return value_; }

    [[nodiscard]] bool equals(std::string_view other) const noexcept {
        return value_.size() == other.size() &&
               (value_.empty() ||
                CRYPTO_memcmp(value_.data(), other.data(), value_.size()) == 0);
    }

private:
    std::string value_;
};

template <typename Container>
class ScopedCleanse final {
public:
    explicit ScopedCleanse(Container& value) noexcept : value_(value) {}
    ~ScopedCleanse() { Cleanse(value_); }

    ScopedCleanse(const ScopedCleanse&) = delete;
    ScopedCleanse& operator=(const ScopedCleanse&) = delete;

private:
    Container& value_;
};

} // namespace SecureMemory
